/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-04-23 11:33:25
 * @LastEditors  : loping
 * @LastEditTime : 2025-04-25 20:35:53
 * @Description  : Heater/sensor verification code
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef __VERIFY_HEATER_H__
#define __VERIFY_HEATER_H__

#include <string>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "printer.h"
#include "configfile.h"

namespace elegoo
{
    namespace extras
    {
        class Heater;

        class HeaterCheck
        {
        public:
            HeaterCheck(std::shared_ptr<ConfigWrapper> config);
            ~HeaterCheck();
            void handle_connect();
            void handle_shutdown();
            double check_event(double eventtime);
            double heater_fault();
        private:
            std::shared_ptr<Printer> printer;
            std::string heater_name;
            std::shared_ptr<Heater> heater;
            std::shared_ptr<ReactorTimer> check_timer;
            double hysteresis;
            double max_error;
            double heating_gain;
            double check_gain_time;
            double last_target;
            double goal_temp;
            double error;
            double goal_systime;
            bool approaching_target;
            bool starting_approach;
            bool is_verify_heater_fault;
        };

        std::shared_ptr<HeaterCheck> verify_heater_load_config_prefix(std::shared_ptr<ConfigWrapper> config);
    }
}

#endif