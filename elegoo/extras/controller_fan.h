/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-20 15:16:18
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 11:10:07
 * @Description  : Support a fan for cooling the MCU whenever a stepper or
 *heater is on
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <memory>
#include <string>
#include "configfile.h"
#include "fan.h"
#include "heaters.h"
#include "mcu.h"
#include "printer.h"
#include "stepper_enable.h"

class Heater;
class ReactorTimer;

namespace elegoo
{
    namespace extras
    {

        class ControllerFan
        {
        public:
            ControllerFan(std::shared_ptr<ConfigWrapper> config);
            json get_status(double eventtime);
            void handle_connect();
            void handle_ready();
            double callback(double eventtime);

        private:
            std::shared_ptr<Printer> printer;
            std::vector<std::string> stepper_names;
            std::vector<std::string> heater_names;
            std::shared_ptr<PrinterStepperEnable> stepper_enable;
            std::vector<std::shared_ptr<Heater>> heaters;
            std::shared_ptr<Fan> fan;
            std::shared_ptr<ReactorTimer> fan_timer;
            float fan_speed;
            float idle_speed;
            int idle_timeout;
            int last_on;
            float last_speed;
            std::string name;
        };

        std::shared_ptr<ControllerFan> controller_fan_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config);

    }
}