/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Jack
 * @LastEditTime : 2025-02-24 14:50:20
 * @Description  : Heater handling on homing moves
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "homing_heaters.h"
#include "printer.h"
#include "heaters.h"
#include "kinematics_factory.h"

namespace elegoo
{
    namespace extras
    {
        HomingHeaters::HomingHeaters(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("HomingHeaters init!");
            printer = config->get_printer();

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:connect",
                std::function<void()>([this]()
                                      {
            SPDLOG_DEBUG("HomingHeaters connect~~~~~~~~~~~~~~~~~");
            handle_connect();
            SPDLOG_DEBUG("HomingHeaters connect~~~~~~~~~~~~~~~~~ success!"); }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:homing_move_begin",
                std::function<void(std::shared_ptr<HomingMove>)>(
                    [this](std::shared_ptr<HomingMove> hmove)
                    {
                        handle_homing_move_begin(hmove);
                    }));
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:homing_move_end",
                std::function<void(std::shared_ptr<HomingMove>)>(
                    [this](std::shared_ptr<HomingMove> hmove)
                    {
                        handle_homing_move_end(hmove);
                    }));

    disable_heaters = config->getlist("heaters");
    flaky_steppers = config->getlist("steppers");
    pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(config->get_printer()->load_object(config, "heaters"));

            SPDLOG_DEBUG("HomingHeaters init success!!");
        }

        HomingHeaters::~HomingHeaters()
        {
        }

        void HomingHeaters::handle_connect()
        {
            std::vector<std::string> all_heaters = pheaters->get_all_heaters();
            if (disable_heaters.empty())
            {
                disable_heaters = all_heaters;
            }
            else
            {
                bool allValid = std::all_of(disable_heaters.begin(), disable_heaters.end(),
                                            [&](const std::string &heater)
                                            {
                                                return std::find(all_heaters.begin(), all_heaters.end(), heater) != all_heaters.end();
                                            });
                if (!allValid)
                {
                    // throw printer->configError(
                    //     "One or more of these heaters are unknown: " + vectorToString(disableHeaters));
                }
            }

            // 获取所有步进电机
            std::shared_ptr<Kinematics>
                kin = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"))->get_kinematic();
            std::vector<std::string> all_steppers;
            // for (std::shared_ptr<MCU_stepper> stepper : kin->get_steppers())     // kin is nullptr!
            // {
            //     all_steppers.push_back(stepper->get_name());
            // }

            if (flaky_steppers.empty())
            {
                return;
            }

            bool steppersValid = std::all_of(flaky_steppers.begin(), flaky_steppers.end(),
                                             [&](const std::string &stepper)
                                             {
                                                 return std::find(all_heaters.begin(), all_heaters.end(), stepper) != all_heaters.end();
                                             });

            if (!steppersValid)
            {
                // throw printer->configError(
                //     "One or more of these steppers are unknown: " + vectorToString(flakySteppers));
            }
        }

        bool HomingHeaters::check_eligible(const std::vector<std::shared_ptr<MCU_pins>> &endstops)
        {
            if (flaky_steppers.empty())
            {
                return true;
            }

            std::vector<std::string> steppers_being_homed;
            for (const auto &es : endstops)
            {
                for (const auto &stepper : es->get_steppers())
                {
                    steppers_being_homed.push_back(stepper->get_name());
                }
            }

            return std::any_of(steppers_being_homed.begin(),
                               steppers_being_homed.end(),
                               [&](const std::string &stepper)
                               {
                                   return std::find(flaky_steppers.begin(),
                                                    flaky_steppers.end(), stepper) != flaky_steppers.end();
                               });
        }

        void HomingHeaters::handle_homing_move_begin(std::shared_ptr<HomingMove> hmove)
        {
            if (!check_eligible(hmove->get_mcu_endstops()))
            {
                return;
            }
            for (const std::string &heater_name : disable_heaters)
            {
                std::shared_ptr<Heater> heater =
                    pheaters->lookup_heater(heater_name);
                target_save[heater_name] = heater->get_temp(0).second;
                heater->set_temp(0.0);
            }
            SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
        }

        void HomingHeaters::handle_homing_move_end(std::shared_ptr<HomingMove> hmove)
        {
            if (!check_eligible(hmove->get_mcu_endstops()))
            {
                return;
            }

            for (const std::string &heater_name : disable_heaters)
            {
                std::shared_ptr<Heater> heater =
                    pheaters->lookup_heater(heater_name);
                heater->set_temp(target_save[heater_name]);
            }
        }

        std::shared_ptr<HomingHeaters> homing_heaters_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<HomingHeaters>(config);
        }

    }
}
