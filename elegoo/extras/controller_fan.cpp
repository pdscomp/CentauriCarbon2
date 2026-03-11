/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-12-21 16:35:48
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-28 13:44:07
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-20 15:16:18
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 11:10:04
 * @Description  : Support a fan for cooling the MCU whenever a stepper or
 *heater is on
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "controller_fan.h"
#include <cmath>

namespace elegoo
{
    namespace extras
    {
        const float PIN_MIN_TIME = 0.100;

        ControllerFan::ControllerFan(std::shared_ptr<ConfigWrapper> config)
        {
            printer = config->get_printer();
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this]()
                                      { handle_ready(); }));

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:connect",
                std::function<void()>([this]()
                                      { handle_connect(); }));

            std::istringstream iss(config->get_name());
            std::string word;
            while (iss >> word) 
            {
                name = word;
            }

            stepper_names = config->getlist("stepper");
            stepper_enable = any_cast<std::shared_ptr<PrinterStepperEnable>>(printer->load_object(config, "stepper_enable"));
            fan = std::make_shared<Fan>(config);
            fan_speed = config->getdouble("fan_speed", 1.0, 0.0, 1.0);
            idle_speed = config->getdouble("idle_speed", fan_speed, 0.0, 1.0);
            idle_timeout = config->getint("idle_timeout", 30, 0);
            heater_names = config->getlist("heater", {
                                                         "extruder",
                                                     });
            last_on = idle_timeout;
            last_speed = 0.0;

            // for (const auto &name : heater_names)
            //     SPDLOG_INFO("heater_names {}", name);
            // for (const auto &name : stepper_names)
            //     SPDLOG_INFO("stepper_names {}", name);
        }

        json ControllerFan::get_status(double eventtime)
        {
            return fan->get_status(eventtime);
        }

        void ControllerFan::handle_connect()
        {
            // SPDLOG_INFO("ControllerFan::handle_connect #0");
            // 查找加热器
            auto pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));
            for (const auto &name : heater_names)
                heaters.push_back(pheaters->lookup_heater(name));
            // 查找所有可用的电机
            auto all_steppers = this->stepper_enable->get_steppers();

            if (stepper_names.empty())
            {
                stepper_names = all_steppers;
                // SPDLOG_INFO("ControllerFan::handle_connect #1");
                return;
            }
            // 检查关联的电机是否都存在
            for (const auto &name : stepper_names)
            {
                if (std::find(all_steppers.begin(), all_steppers.end(), name) == all_steppers.end())
                {
                    throw elegoo::common::ConfigParserError("steppers are unknown: " + name);
                }
            }
            // SPDLOG_INFO("ControllerFan::handle_connect #2");
        }

        void ControllerFan::handle_ready()
        {
            // SPDLOG_INFO("ControllerFan::handle_ready #0");
            std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
            fan_timer = reactor->register_timer(
                [this](double eventtime)
                { return this->callback(eventtime); }, get_monotonic() + PIN_MIN_TIME, std::string("controller_fan ") + name);
            // SPDLOG_INFO("ControllerFan::handle_ready #1");
        }

        double ControllerFan::callback(double eventtime)
        {
            // SPDLOG_INFO("ControllerFan::callback #0");

            double speed = 0.0;
            bool active = false;
            for (const auto &name : stepper_names)
            {
                active |= stepper_enable->lookup_enable(name)->is_motor_enabled();
            }
            for (const auto &heater : heaters)
            {
                float current_temp, target_temp;
                auto temp = heater->get_temp(eventtime);
                target_temp = temp.second;
                if (target_temp > 0.0)
                {
                    active = true;
                }
            }
            // SPDLOG_DEBUG("ControllerFan::callback #1 active {}", active);

            if (active)
            {
                last_on = 0;
                speed = fan_speed;
            }
            else if (last_on < idle_timeout)
            {
                speed = idle_speed;
                last_on++;
            }

            if (speed != last_speed)
            {
                last_speed = speed;
                SPDLOG_DEBUG("ControllerFan call set_speed {}", speed);
                fan->set_speed(speed);
            }
            return eventtime + 1.0;
        }

        std::shared_ptr<ControllerFan> controller_fan_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<ControllerFan>(config);
        }

    }
}
