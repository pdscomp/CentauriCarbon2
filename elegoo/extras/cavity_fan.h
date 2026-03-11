/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-09-19 20:19:13
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-29 15:19:58
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory>
#include <vector>
#include "json.h"
class ConfigWrapper;
class Printer;
class ReactorTimer;
class SelectReactor;
class GCodeDispatch;
class GCodeCommand;

namespace elegoo {
namespace extras {

class PrinterSensorGeneric;
class Fan;
class PrinterFanCavity
{
public:
    PrinterFanCavity(std::shared_ptr<ConfigWrapper> config);
    ~PrinterFanCavity();

    void handle_ready();
    void handle_print_state(std::string value);
    void handle_cavity_mode(bool value);
    json get_status(double eventtime);
    double callback(double eventtime);
    void cmd_SET_CAVITY_FAN(std::shared_ptr<GCodeCommand> gcmd);
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<PrinterSensorGeneric> box;
    std::shared_ptr<Fan> fan;
    std::shared_ptr<ReactorTimer> fan_timer;
    std::string name;
    double heater_temp;
    double fan_speed;
    double last_speed;
    double A;
    double B;
    double C;
    double D;
    double E;
    double F;
    double G;
    double T0;
    double T1;
    double T2;
    double T3;
    double T4;
    bool mode;
    double cur_box_temp;
    double last_box_temp;
};

std::shared_ptr<PrinterFanCavity> cavity_fan_load_config(
    std::shared_ptr<ConfigWrapper> config);

}
}