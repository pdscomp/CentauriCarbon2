/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-02 14:59:47
 * @Description  : idle_time is a configuration parameter in the Elegoo 
 * firmware used to control the idle time between certain operations, 
 * such as moves or extrusions. This parameter is primarily used to optimize 
 * print quality and performance, especially when dealing with fine 
 * details or high-precision printing tasks.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "idle_timeout.h"
#include <iostream>
#include "printer.h"
#include "gcode_macro.h"


const double PIN_MIN_TIME = 0.100;
const double READY_TIMEOUT = .500;

namespace elegoo {
namespace extras {
IdleTimeout::IdleTimeout(std::shared_ptr<ConfigWrapper> config)
{
    SPDLOG_INFO("init IdleTimeout ...");
    printer = config->get_printer();
    reactor = printer->get_reactor();
    gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:ready",
        std::function<void()>([this](){
            SPDLOG_DEBUG("elegoo:ready !");
            handle_ready();
            SPDLOG_DEBUG("elegoo:ready !");
        })
    );

    extruder_idle_timeout = config->getdouble("extruder_idle_timeout", 600, DOUBLE_NONE, DOUBLE_NONE, 0);
    heater_bed_idle_timeout_min = config->getdouble("heater_bed_idle_timeout_min", 60);
    heater_bed_idle_timeout_max = config->getdouble("heater_bed_idle_timeout_max", 259200);
    heater_bed_idle_timeout = config->getdouble("heater_bed_idle_timeout", 600, heater_bed_idle_timeout_min, heater_bed_idle_timeout_max);

    std::shared_ptr<PrinterGCodeMacro> gcode_macro = 
        any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->load_object(config, "gcode_macro"));
    idle_gcode = gcode_macro->load_template(config, "extruder_timeout_gcode", "M104 S0");

    gcode->register_command(
        "SET_IDLE_TIMEOUT",
        [this](std::shared_ptr<GCodeCommand> gcmd){ 
            cmd_SET_EXTRUDER_IDLE_TIMEOUT(gcmd); 
        },
        false,
        "Set the idle timeout in seconds"
    );

    gcode->register_command(
        "SET_HEATER_BED_IDLE_TIMEOUT",
        [this](std::shared_ptr<GCodeCommand> gcmd){ 
            cmd_SET_HEATER_BED_IDLE_TIMEOUT(gcmd); 
        },
        false,
        "Set the idle timeout in seconds"
    );

    state = "Idle";
    print_state = "standby";
    last_print_start_systime = 0;
    heater_bed_active_timeout = 259200; //3days
    SPDLOG_INFO("init IdleTimeout success !!!");
}

IdleTimeout::~IdleTimeout()
{

}


json IdleTimeout::get_status(double eventtime)
{
    double printing_time = 0.0;
    if (state == "Printing") 
    {
        printing_time = eventtime - last_print_start_systime;
    }
    json status;
    status["state"] = state;
    status["printing_time"] = printing_time;
    return status;
}

void IdleTimeout::handle_ready()
{
    toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
    timeout_timer = reactor->register_timer(
        [this](double eventtime) 
        { 
            return timeout_handler(eventtime);
        }
    );

    heater_bed_idle_timer = reactor->register_timer(
        [this](double eventtime) 
        { 
            return heater_bed_idle_timeout_handler(eventtime);
        },
        get_monotonic() + heater_bed_idle_timeout
    );

    heater_bed_active_timer = reactor->register_timer(
        [this](double eventtime) 
        { 
            return heater_bed_active_timeout_handler(eventtime);
        }
    );

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:print_stats",
        std::function<void(std::string)>([this](std::string print_state) {
            handle_print_state(print_state);
        })
    );

    elegoo::common::SignalManager::get_instance().register_signal(
        "toolhead:sync_print_time",
        std::function<void(double, double, double)>(
            [this](double curtime, double print_time, double est_print_time){
            handle_sync_print_time(curtime, print_time, est_print_time);
        })
    );
}

void IdleTimeout::handle_print_state(std::string print_state)
{
    SPDLOG_INFO("{} print_state:{}",__func__,print_state);
    this->print_state = print_state;
    if(print_state == "standby" || print_state == "error" || 
        print_state == "complete" || print_state == "cancelled") {
        if(heater_bed_idle_timer) {
            reactor->update_timer(heater_bed_idle_timer, get_monotonic() + heater_bed_idle_timeout);
        }
        if(heater_bed_active_timer) {
            reactor->update_timer(heater_bed_active_timer, _NEVER);
        }
    } else {
        if(heater_bed_idle_timer) {
            reactor->update_timer(heater_bed_idle_timer, _NEVER);
        }
        if(heater_bed_active_timer) {
            reactor->update_timer(heater_bed_active_timer, get_monotonic() + heater_bed_active_timeout);
        }
    }
}

double IdleTimeout::transition_idle_state(double eventtime)
{
    state = "Printing";
    try {
        idle_gcode->run_gcode_from_command();
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Exception during idle timeout GCode execution: {}",e.what());
        state = "Ready";
        return eventtime + 1.0;
    }

    double print_time = toolhead->get_last_move_time();
    state = "Idle";
    elegoo::common::SignalManager::get_instance().emit_signal<double>(
        "idle_timeout:idle", print_time);
    return _NEVER;
}

double IdleTimeout::check_idle_timeout(double eventtime)
{
    Busy busy = toolhead->check_busy(eventtime);
    double idle_time = busy.est_print_time - busy.print_time;

    if (!busy.lookahead_empty || idle_time < 1.0) 
    {
        return eventtime + extruder_idle_timeout;
    }
    if (idle_time < extruder_idle_timeout) 
    {
        return eventtime + extruder_idle_timeout - idle_time;
    }
    if (gcode->get_mutex()->test()) 
    {
        return eventtime + 1.0;
    }

    return transition_idle_state(eventtime);
}

double IdleTimeout::timeout_handler(double eventtime)
{
    if (printer->is_shutdown()) 
    {
        return _NEVER;
    }

    if (state == "Ready") 
    {
        return check_idle_timeout(eventtime);
    }

    Busy busy = toolhead->check_busy(eventtime);
    double buffer_time = std::min(2.0, busy.print_time - busy.est_print_time);

    if (!busy.lookahead_empty) 
    {
        return eventtime + READY_TIMEOUT + std::max(0.0, buffer_time);
    }

    if (buffer_time > -READY_TIMEOUT) 
    {
        return eventtime + READY_TIMEOUT + buffer_time;
    }

    if (gcode->get_mutex()->test()) 
    {
        return eventtime + READY_TIMEOUT;
    }

    state = "Ready";
    elegoo::common::SignalManager::get_instance().emit_signal<double>(
        "idle_timeout:ready", busy.est_print_time + PIN_MIN_TIME);
    return eventtime + extruder_idle_timeout;
}

void IdleTimeout::handle_sync_print_time(double curtime, double print_time, double est_print_time)
{
    if (state == "Printing") {
        return;
    }

    state = "Printing";
    last_print_start_systime = curtime;
    double check_time = READY_TIMEOUT + print_time - est_print_time;

    if(timeout_timer) {
        reactor->update_timer(timeout_timer, curtime + check_time);
    }

    elegoo::common::SignalManager::get_instance().emit_signal<double>(
        "idle_timeout:printing", est_print_time + PIN_MIN_TIME);
}

void IdleTimeout::cmd_SET_EXTRUDER_IDLE_TIMEOUT(std::shared_ptr<GCodeCommand> gcmd)
{
    double timeout = gcmd->get_double("TIMEOUT", extruder_idle_timeout, DOUBLE_NONE, DOUBLE_NONE, 0);
    extruder_idle_timeout = timeout;

    std::ostringstream oss;
    oss << "idle_timeout: Timeout set to " << timeout << " s";
    gcmd->respond_info(oss.str(), true);

    if (state == "Ready") 
    {
        double checktime = get_monotonic() + timeout;
        reactor->update_timer(timeout_timer, checktime);
    }

    SPDLOG_INFO("set extruder idle timeout = {}",timeout);
}

void IdleTimeout::cmd_SET_HEATER_BED_IDLE_TIMEOUT(std::shared_ptr<GCodeCommand> gcmd)
{
    double timeout = gcmd->get_double("TIMEOUT", heater_bed_idle_timeout, DOUBLE_NONE, DOUBLE_NONE, 0);
    heater_bed_idle_timeout = timeout;
    if(print_state == "standby" || print_state == "error" || 
        print_state == "complete" || print_state == "cancelled") 
    {
        reactor->update_timer(heater_bed_idle_timer, get_monotonic() + timeout);
    }  
    SPDLOG_INFO("set heater bed idle timeout = {}",timeout);  
}

double IdleTimeout::heater_bed_idle_timeout_handler(double eventtime) 
{
    Busy busy = toolhead->check_busy(eventtime);
    double idle_time = busy.est_print_time - busy.print_time;

    if (idle_time < heater_bed_idle_timeout) 
    {
        return eventtime + heater_bed_idle_timeout - idle_time;
    }

    gcode->run_script_from_command("M140 S0");
    SPDLOG_INFO("Device idle, activating heated bed protection.");
    return _NEVER;
}

double IdleTimeout::heater_bed_active_timeout_handler(double eventtime) 
{
    Busy busy = toolhead->check_busy(eventtime);
    double idle_time = busy.est_print_time - busy.print_time;

    if (idle_time < heater_bed_active_timeout) 
    {
        return eventtime + heater_bed_active_timeout - idle_time;
    }

    gcode->run_script_from_command("M140 S0");
    SPDLOG_INFO("Device pause too long, activating heated bed protection.");

    return _NEVER;
}


std::shared_ptr<IdleTimeout> idle_timeout_load_config(std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<IdleTimeout>(config);
}

}
}