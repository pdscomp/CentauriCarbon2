/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:54:14
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-14 09:46:05
 * @Description  : Serial port management for firmware communication
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <termios.h>
#include <mutex>
#include <thread>
#include <list>
#include "c_helper.h"
#include "json.h"
#include "serial.h"

class SelectReactor;
class MessageParser;
class SerialPort;
class ReactorCompletion;
class SerialReader : public std::enable_shared_from_this<SerialReader>
{
public:
    SerialReader(std::shared_ptr<SelectReactor> reactor,
                 const std::string warn_prefix = "", const std::string name = "", const std::string serial = "");
    ~SerialReader();

    void connect_canbus(const std::string &canbus_uuid,
                        int canbus_nodeid, const std::string &canbus_iface = "can0");
    void connect_pipe(const std::string &filename);
    void connect_uart(const std::string &serialport,
                      int baud, bool rts = true);
    void connect_rpmsg(const std::string &serialport);
    void connect_file(int outfile,
                      const std::vector<uint8_t> &dictionary, bool pace = false);
    void set_clock_est(double freq, double conv_time,
                       uint64_t conv_clock, uint64_t last_clock);
    void disconnect();
    std::string stats(double eventtime);
    std::shared_ptr<SelectReactor> get_reactor();
    std::shared_ptr<MessageParser> get_msgparser();
    std::shared_ptr<serialqueue> get_serialqueue();
    std::shared_ptr<command_queue> get_default_command_queue();
    void register_response(std::function<void(const json &params)> callback,
                           const std::string &name, uint32_t oid = 0);
    void raw_send(const std::vector<uint8_t> &cmd,
                  uint64_t minclock,
                  uint64_t reqclock,
                  std::shared_ptr<command_queue> cmd_queue);
    json raw_send_wait_ack(
        const std::vector<uint8_t> &cmd,
        uint64_t minclock,
        uint64_t reqclock,
        std::shared_ptr<command_queue> cmd_queue);
    void send(const std::string &msg, int minclock = 0, int reqclock = 0);
    json send_with_response(const std::string &msg, const std::string &response);
    std::shared_ptr<command_queue> alloc_command_queue();
    std::string dump_debug();
    void handle_unknown(const json &params);
    void handle_output(const json &params);
    void handle_default(const json &params);
    std::string get_warn_prefix() { return warn_prefix; }

    bool check_timer_too_close(uint32_t *clock)
    {
        if (timer_too_close)
        {
            clock[0] = timer_too_close_clock[0];
            clock[1] = timer_too_close_clock[1];
            return true;
        }
        return false;
    }

private:
    void bg_thread();
    void error(const std::string &msg);
    json get_identify_data(double eventtime);
    bool start_session(int fd, uint8_t serial_fd_type = 'u', int client_id = 0);
    void handle_unknown_init(const json &params);
    std::string join(const std::vector<std::string> &elements, const std::string &delimiter);

private:
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<MessageParser> msgparser;
    std::shared_ptr<serialqueue> sq = nullptr;
    std::shared_ptr<command_queue> default_cmd_queue;
    std::map<int, std::shared_ptr<ReactorCompletion>> pending_notifications;
    std::string warn_prefix;
    std::string name;
    std::string serial;
    bool _is_rpmsg = false;
    int _ept_id;
    int last_notify_id;
    std::mutex mutex;
    std::mutex pending_notifications_mutex;

    char *stats_buf;
    std::shared_ptr<std::thread> background_thread;
    int fd = -1;
    std::map<std::pair<std::string, int>,
             std::function<void(const json &params)>>
        handlers;

    uint32_t timer_too_close_clock[2];
    bool timer_too_close = false;
};

class SerialRetryCommand
{
public:
    SerialRetryCommand(std::shared_ptr<SerialReader> serial,
                       const std::string &name, uint32_t oid = 0);
    ~SerialRetryCommand();

    void handle_callback(const json &params);
    json get_response(const std::vector<std::vector<uint8_t>> &cmds,
                      std::shared_ptr<command_queue> cmd_queue,
                      uint64_t minclock = 0, uint64_t reqclock = 0);

private:
    std::shared_ptr<SerialReader> serial;
    std::string name;
    uint32_t oid;
    json last_params;
};
