/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:32
 * @LastEditors  : Ben
 * @LastEditTime : 2024-11-28 21:21:19
 * @Description  : The console module in Elegoo is responsible for handling 
 * user interactions through the command-line interface (CLI). It provides 
 * a way for users to send commands, view status information, and manage 
 * the printer directly from the terminal. 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <string>
#include <map>
#include <vector>
#include "reactor.h"
#include "serialhdl.h"
#include "c_helper.h"
#include "clocksync.h"
class KeyboardReader
{
public:
    KeyboardReader(std::shared_ptr<SelectReactor> reactor,
        std::string serialport, int baud, 
        std::string canbus_iface,
        int canbus_nodeid);
    ~KeyboardReader();

    double connect(double eventtime);
    void output(const std::string& msg);
    void handle_default(const json& params);
    void handle_output(const json& params);
    void handle_suppress(const json& params);
    void update_evals(double eventtime);
    void command_SET(std::vector<std::string>& parts);
    void command_DUMP(std::vector<std::string>& parts, const std::string& filename="");
    void command_FILEDUMP(std::vector<std::string>& parts);
    void command_DELAY(std::vector<std::string>& parts);
    void command_FLOOD(std::vector<std::string>& parts);
    void command_SUPPRESS(std::vector<std::string>& parts);
    void command_STATS(std::vector<std::string>& parts);
    void command_LIST(std::vector<std::string>& parts);
    void command_HELP(std::vector<std::string>& parts);
    std::string translate(const std::string& line, double eventtime);
    void process_kbd(double eventtime);
    std::string trim(const std::string& str, 
        bool remove_front=true, bool remove_back=true);
    std::vector<std::string> split(const std::string& str);
    double evaluate_expression(const std::string& expr, 
        const std::map<std::string, double>& variables);
private:
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<SerialReader> ser;
    std::shared_ptr<ClockSync> clocksync;
    std::string serialport;
    int baud;
    std::string canbus_iface;
    int canbus_nodeid;
    double start_time;
    int fd;
    int mcu_freq;
    std::string data;
    std::map<std::string, double> eval_globals;
    std::map<std::string, std::function<void(std::vector<std::string>&)>> local_commands;
};