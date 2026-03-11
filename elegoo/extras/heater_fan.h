/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-12 18:16:19
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-07 13:26:07
 * @Description  : Support fans that are enabled when a heater is on
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory>
#include <vector>
#include "json.h"
class ConfigWrapper;
class Printer;
class ReactorTimer;


namespace elegoo {
namespace extras {

class Heater;

class Fan;
class PrinterHeaterFan
{
public:
    PrinterHeaterFan(std::shared_ptr<ConfigWrapper> config);
    ~PrinterHeaterFan();

    void handle_ready();
    json get_status(double eventtime);
    double callback(double eventtime);

private:
    std::shared_ptr<Printer> printer;
    std::vector<std::string> heater_names;
    std::vector<std::shared_ptr<Heater>> heaters;
    std::shared_ptr<Fan> fan;
    std::shared_ptr<ReactorTimer> fan_timer;
    std::string name;
    double heater_temp;
    double fan_speed;
    double last_speed;
};

std::shared_ptr<PrinterHeaterFan> heater_fan_load_config_prefix(
    std::shared_ptr<ConfigWrapper> config);

}
}
