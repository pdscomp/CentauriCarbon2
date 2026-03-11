/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:54:03
 * @LastEditors  : loping
 * @LastEditTime : 2025-05-14 21:42:47
 * @Description  : Main code for host side printer firmware
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include "common/logger.h"
#include "clocksync.h"
#include "configfile.h"
#include "gcode.h"
#include "mathutil.h"
#include "mcu.h"
#include "reactor.h"
#include "pins.h"
#include "host_warpper.h"
// #include "logger.h"
#include "reactor.h"
#include "serialhdl.h"
#include "stepper.h"
#include "toolhead.h"
#include "utilities.h"
#include "webhooks.h"
#include "msgproto.h"
#include <unordered_map>
#include "json.h"
#include "exception_handler.h"
#include "event_handler.h"
#include "any.h"
#include <dlfcn.h>

#define OPT_USR "/opt/usr"
#define OPT_INST "/opt/inst"
class Printer : public std::enable_shared_from_this<Printer>{
public:
    Printer(std::shared_ptr<SelectReactor> main_reactor,
        const std::unordered_map<std::string, std::string>& start_args);
    ~Printer();

    std::unordered_map<std::string, std::string> get_start_args();
    std::shared_ptr<SelectReactor> get_reactor();
    std::pair<std::string, std::string> get_state_message();
    bool is_shutdown();
    void update_error_msg(const std::string& oldmsg, const std::string& newmsg);

    void add_object(const std::string& name,
        Any default_object);

    Any lookup_object(const std::string& name, Any default_object = Any());
    std::map<std::string, Any> lookup_objects(const std::string& module = "");

    Any load_object(std::shared_ptr<ConfigWrapper> config, const std::string& section);
    std::string run();
    void set_rollover_info(std::string, std::string, bool log = true);
    double invoke_shutdown(const std::string& msg,
        const std::map<std::string, std::string>& details=std::map<std::string, std::string>());
    void invoke_async_shutdown(const std::string& msg,
        const std::map<std::string, std::string>& details={});
    void request_exit(std::string result);

private:
    void set_state(const std::string& msg);
    void read_config();
    json connect(double eventtime);

private:
    std::unordered_map<std::string, std::string> start_args;
    std::shared_ptr<SelectReactor> reactor;
    std::string message_startup;
    std::string message_ready;
    std::string message_restart;
    std::string state_message;
    bool in_shutdown_state;
    std::string run_result;
    std::map<std::string, Any> objects;
    std::map<std::string,  std::map<std::string, std::function<void()>>> event_handlers;
    std::shared_ptr<ConfigWrapper> config;
};
