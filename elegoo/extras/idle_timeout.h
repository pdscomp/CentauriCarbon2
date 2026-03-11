/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-05-23 20:03:28
 * @Description  : idle_time is a configuration parameter in the Elegoo 
 * firmware used to control the idle time between certain operations, 
 * such as moves or extrusions. This parameter is primarily used to optimize 
 * print quality and performance, especially when dealing with fine 
 * details or high-precision printing tasks.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <memory>
#include <string>
#include "json.h"
class ConfigWrapper;
class Printer;
class SelectReactor;
class GCodeDispatch;
class ToolHead;
class PrinterGCodeMacro;
class GCodeCommand;
class ReactorTimer;

namespace elegoo {
namespace extras {

class TemplateWrapper;
class IdleTimeout
{
public:
    IdleTimeout(std::shared_ptr<ConfigWrapper> config);
    ~IdleTimeout();

    json get_status(double eventtime);
    double transition_idle_state(double eventtime);
    double check_idle_timeout(double eventtime);
    double timeout_handler(double eventtime);
    void handle_sync_print_time(double curtime, double print_time, double est_print_time);
    void cmd_SET_EXTRUDER_IDLE_TIMEOUT(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SET_HEATER_BED_IDLE_TIMEOUT(std::shared_ptr<GCodeCommand> gcmd);
    double heater_bed_idle_timeout_handler(double eventtime);
    double heater_bed_active_timeout_handler(double eventtime);
private:
    void handle_ready();
    void handle_print_state(std::string print_state);

private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<GCodeDispatch> gcode;
    std::shared_ptr<ToolHead> toolhead;
    std::shared_ptr<ReactorTimer> timeout_timer;
    std::shared_ptr<ReactorTimer> heater_bed_idle_timer;
    std::shared_ptr<ReactorTimer> heater_bed_active_timer;
    std::shared_ptr<TemplateWrapper> idle_gcode;
    double extruder_idle_timeout;
    double last_print_start_systime;
    double heater_bed_active_timeout;
    double heater_bed_idle_timeout;
    double heater_bed_idle_timeout_min;
    double heater_bed_idle_timeout_max;
    std::string state;
    std::string print_state;
};


std::shared_ptr<IdleTimeout> idle_timeout_load_config(std::shared_ptr<ConfigWrapper> config);

}
}