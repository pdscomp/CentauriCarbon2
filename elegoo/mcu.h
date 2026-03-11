/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-07 14:24:31
 * @LastEditors  : loping
 * @LastEditTime : 2025-04-25 15:58:42
 * @Description  : Interface to Elegoo micro-controller code
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <memory>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include "msgproto.h"
#include "c_helper.h"
#include "json.h"
#include "pins.h"
#include "host_warpper.h"
#include "any.h"

class SerialReader;
class SelectReactor;
class ReactorCompletion;
class ConfigWrapper;
class ClockSync;
class Printer;
class MCU;
class SerialRetryCommand;
class MCU_stepper;
class CommandWrapper;

class mcu_error
{
};

class RetryAsyncCommand
{
public:
    RetryAsyncCommand(std::shared_ptr<SerialReader> serial,
                      const std::string &name, uint32_t oid);
    ~RetryAsyncCommand();

    void handle_callback(json params);
    json get_response(const std::vector<uint8_t> &cmds,
                      std::shared_ptr<command_queue> cmd_queue,
                      uint64_t minclock,
                      uint64_t reqclock);

private:
    std::shared_ptr<SerialReader> serial;
    std::shared_ptr<ReactorCompletion> completion;
    std::shared_ptr<SelectReactor> reactor;
    bool need_response;
    std::string name;
    double min_query_time;
    uint32_t oid;
};

class CommandQueryWrapper
{
public:
    CommandQueryWrapper(std::shared_ptr<SerialReader> serial,
                        const std::string &msgformat,
                        const std::string &respformat,
                        uint32_t oid = 0, std::shared_ptr<command_queue> cmd_queue = nullptr, bool is_async = false);
    ~CommandQueryWrapper();

    json send(const std::vector<Any> &data = {}, uint64_t minclock = 0, uint64_t reqclock = 0);
    json send_with_preface(std::shared_ptr<CommandWrapper> preface_cmd,
                           const std::vector<Any> preface_data = {},
                           const std::vector<Any> data = {},
                           uint64_t minclock = 0, uint64_t reqclock = 0);

private:
    json do_send(const std::vector<uint8_t> &cmds, uint64_t minclock, uint64_t reqclock);

private:
    std::shared_ptr<SerialReader> serial;
    std::shared_ptr<Format> cmd;
    std::shared_ptr<command_queue> cmd_queue;
    std::string response;
    bool is_async;
    uint32_t oid;
};

class CommandWrapper
{
public:
    CommandWrapper(std::shared_ptr<SerialReader> serial,
                   const std::string &msgformat,
                   std::shared_ptr<command_queue> cmd_queue = nullptr);
    ~CommandWrapper();

    void send(const std::vector<Any> &data = {}, uint32_t minclock = 0, uint32_t reqclock = 0);
    void send_wait_ack(const std::vector<Any> &data = {}, uint32_t minclock = 0, uint32_t reqclock = 0);
    uint32_t get_command_tag();
    std::shared_ptr<Format> cmd;

private:
    std::shared_ptr<SerialReader> serial;
    std::shared_ptr<command_queue> cmd_queue;
    uint32_t msgtag;
};

class MCU_trsync
{
public:
    MCU_trsync(std::shared_ptr<MCU> mcu, std::shared_ptr<trdispatch> td);
    ~MCU_trsync();

    std::shared_ptr<MCU> get_mcu();
    uint32_t get_oid();
    std::shared_ptr<command_queue> get_command_queue();
    void add_stepper(std::shared_ptr<MCU_stepper> stepper);
    std::vector<std::shared_ptr<MCU_stepper>> get_steppers();
    void start(double print_time,
               double report_offset,
               std::shared_ptr<ReactorCompletion> trigger_completion,
               double expire_timeout);
    void set_home_end_time(double home_end_time);
    int stop();

public:
    static const uint32_t REASON_ENDSTOP_HIT = 1;
    static const uint32_t REASON_HOST_REQUEST = 2;
    static const uint32_t REASON_PAST_END_TIME = 3;
    static const uint32_t REASON_COMMS_TIMEOUT = 4;
    // double start_time;
private:
    void build_config();
    void shutdown();
    void handle_trsync_state(const json &params);

private:
    std::shared_ptr<MCU> mcu;
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<trdispatch> td;
    std::shared_ptr<command_queue> cmd_queue;
    std::vector<std::shared_ptr<MCU_stepper>> steppers;
    std::shared_ptr<CommandQueryWrapper> trsync_query_cmd;
    std::shared_ptr<CommandWrapper> trsync_start_cmd;
    std::shared_ptr<CommandWrapper> trsync_set_timeout_cmd;
    std::shared_ptr<CommandWrapper> trsync_trigger_cmd;
    std::shared_ptr<CommandWrapper> stepper_stop_cmd;
    std::vector<std::shared_ptr<MCU_trsync>> trsyncs;
    std::shared_ptr<trdispatch_mcu> td_mcu;
    std::shared_ptr<ReactorCompletion> trigger_completion;
    uint32_t oid;
    uint64_t home_end_clock;
};

class TriggerDispatch
{
public:
    TriggerDispatch(std::shared_ptr<MCU> mcu);
    ~TriggerDispatch();

    uint32_t get_oid();
    std::shared_ptr<command_queue> get_command_queue();
    void add_stepper(std::shared_ptr<MCU_stepper> stepper);
    std::vector<std::shared_ptr<MCU_stepper>> get_steppers();
    std::shared_ptr<ReactorCompletion> start(double print_time);
    void wait_end(double end_time);
    int stop();

private:
    std::shared_ptr<MCU> mcu;
    std::vector<std::shared_ptr<MCU_trsync>> trsyncs;
    std::shared_ptr<trdispatch> td;
    std::shared_ptr<ReactorCompletion> trigger_completion;
};

class MCU_endstop : public MCU_pins
{
public:
    MCU_endstop(std::shared_ptr<MCU> mcu, std::shared_ptr<PinParams> pin_params);
    ~MCU_endstop();

    std::shared_ptr<MCU> get_mcu() override;
    void add_stepper(std::shared_ptr<MCU_stepper> stepper) override;
    std::vector<std::shared_ptr<MCU_stepper>> get_steppers() override;
    std::shared_ptr<ReactorCompletion> home_start(double print_time, double sample_time, int sample_count, double rest_time, bool triggered, double accel_time = 0.) override;
    double home_wait(double home_end_time) override;
    bool query_endstop(double print_time) override;

private:
    void build_config();

private:
    std::shared_ptr<MCU> mcu;
    std::string pin;
    int pullup;
    bool invert;
    uint32_t oid;
    std::shared_ptr<CommandWrapper> home_cmd;
    std::shared_ptr<CommandQueryWrapper> query_cmd;
    uint64_t rest_ticks;
    std::shared_ptr<TriggerDispatch> dispatch;
};

class MCU_digital_out : public MCU_pins
{
public:
    MCU_digital_out(std::shared_ptr<MCU> mcu,
                    std::shared_ptr<PinParams> pin_params);
    ~MCU_digital_out();
    std::shared_ptr<MCU> get_mcu() override;
    void setup_max_duration(double max_duration) override;
    void setup_start_value(double start_value, double shutdown_value) override;
    void set_digital(double print_time, double value);

private:
    void build_config();

private:
    std::shared_ptr<MCU> mcu;
    std::string pin;
    bool invert;
    uint32_t start_value;
    uint32_t shutdown_value;
    double max_duration;
    uint64_t last_clock;
    double pwm_max;
    uint32_t oid;
    std::shared_ptr<CommandWrapper> set_cmd;
};

class MCU_pwm : public MCU_pins
{
public:
    MCU_pwm(std::shared_ptr<MCU> mcu,
            std::shared_ptr<PinParams> pin_params);
    ~MCU_pwm();

    std::shared_ptr<MCU> get_mcu() override;
    void setup_max_duration(double max_duration) override;
    void setup_cycle_time(double cycle_time, bool hardware_pwm = false) override;
    void setup_start_value(double start_value, double shutdown_value) override;
    void set_pwm(double print_time, double value);

private:
    void build_config();

private:
    std::shared_ptr<MCU> mcu;
    bool hardware_pwm;
    double cycle_time;
    double max_duration;
    std::string pin;
    bool invert;
    double start_value;
    double shutdown_value;
    uint64_t last_clock;
    double pwm_max;
    uint32_t oid;
    std::shared_ptr<CommandWrapper> set_cmd;
};

class MCU_adc : public MCU_pins
{
public:
    MCU_adc(std::shared_ptr<MCU> mcu,
            std::shared_ptr<PinParams> pin_params);
    ~MCU_adc();

    std::shared_ptr<MCU> get_mcu() override;
    void setup_adc_sample(double sample_time, int sample_count,
                          double minval = 0.0, double maxval = 1.0, int range_check_count = 0);
    void setup_adc_callback(double report_time,
                            std::function<void(double, double)> callback);
    std::pair<double, double> get_last_value();

private:
    void build_config();
    void handle_analog_in_state(const json &params);

private:
    std::shared_ptr<MCU> mcu;
    std::string pin;
    double min_sample;
    double max_sample;
    double sample_time;
    double report_time;
    int sample_count;
    int range_check_count;
    uint64_t report_clock;
    std::pair<double, double> last_state;
    double inv_max_adc;
    uint32_t oid;
    std::function<void(double, double)> callback;
};

class MCU : public std::enable_shared_from_this<MCU>, public ChipBase
{
public:
    MCU(std::shared_ptr<ConfigWrapper> config,
        std::shared_ptr<ClockSync> clocksync);
    ~MCU();
    std::shared_ptr<MCU_pins> setup_pin(const std::string &pin_type, std::shared_ptr<PinParams> pin_params);
    uint32_t create_oid();
    void register_config_callback(std::function<void()> cb);
    void add_config_cmd(const std::string &cmd, bool is_init = false, bool on_restart = false);
    int get_query_slot(uint32_t oid);
    uint64_t seconds_to_clock(double time);
    float get_max_stepper_error();
    std::shared_ptr<Printer> get_printer();
    std::string get_name();
    void register_response(std::function<void(const json &params)> cb, const std::string &msg, uint32_t oid = 0);
    std::shared_ptr<command_queue> alloc_command_queue();
    std::shared_ptr<CommandWrapper> lookup_command(const std::string &msgformat, std::shared_ptr<command_queue> cq = nullptr);
    std::shared_ptr<CommandQueryWrapper> lookup_query_command(
        const std::string &msgformat,
        const std::string &respformat,
        uint32_t oid = 0,
        std::shared_ptr<command_queue> cq = nullptr,
        bool is_async = false);
    std::shared_ptr<CommandWrapper> try_lookup_command(const std::string &msgformat);
    std::map<std::string, std::map<std::string, int>> get_enumerations();
    std::map<std::string, std::string> get_constants();
    float get_constant_float(const std::string &name);
    uint64_t print_time_to_clock(double print_time);
    double clock_to_print_time(double clock);
    double estimated_print_time(double eventtime);
    uint64_t clock32_to_clock64(uint32_t clock32);
    void register_stepqueue(std::shared_ptr<stepcompress> stepqueue);
    void request_move_queue_slot();
    void register_flush_callback(std::function<void(double, double)> callback);
    void flush_moves(double print_time, double clear_history_time);
    void check_active(double print_time, double eventtime);
    bool is_fileoutput();
    bool get_shutdown();
    uint64_t get_shutdown_clock();
    json get_status();
    std::pair<bool, std::string> stats(double eventtime);

    std::shared_ptr<SerialReader> serial;

private:
    void handle_mcu_stats(json params);
    void handle_shutdown(json params);
    void handle_starting(json params);
    void check_restart(const std::string &reason);
    void connect_file(bool pace = false);
    void send_config(uint32_t prev_crc);
    json send_get_config();
    std::string log_info();
    void connect();
    void mcu_identify();
    void ready();
    void disconnect();
    void shutdown(bool force = false);
    void restart_via_command();
    void restart_via_remoterpc();
    void firmware_restart(bool force = false);
    void firmware_restart_bridge();
    std::vector<std::string> split(const std::string &s, char delimiter);

private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<ClockSync> clocksync;
    std::shared_ptr<SelectReactor> reactor;
    std::string name;
    int baud;
    std::string canbus_iface;
    std::string serialport;
    std::string restart_method;
    std::shared_ptr<CommandWrapper> reset_cmd;
    std::shared_ptr<CommandWrapper> config_reset_cmd;
    bool is_mcu_bridge;
    bool is_rpmsg;
    std::shared_ptr<CommandWrapper> emergency_stop_cmd;
    bool is_shutdown;
    bool is_timeout;
    uint64_t shutdown_clock;
    std::string shutdown_msg;
    static uint32_t oid_count;
    std::vector<std::function<void()>> config_callbacks;
    std::vector<std::string> config_cmds;
    std::vector<std::string> restart_cmds;
    std::vector<std::string> init_cmds;
    int mcu_freq;
    double max_stepper_error;
    int reserved_move_slots;
    std::vector<std::string> stepqueues;
    json get_status_info;
    double stats_sumsq_base;
    double mcu_tick_avg;
    double mcu_tick_stddev;
    double mcu_tick_awake;
    // std::function<double(double)> estimated_print_time;
    std::vector<std::function<void(double, double)>> flush_callbacks;
    std::shared_ptr<steppersync> stepper_sync;
    std::vector<std::shared_ptr<stepcompress>> sc;
    stepcompress **sc_list;
};
