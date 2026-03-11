/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-08 10:33:38
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:17:46
 * @Description  : Code for supporting multiple steppers in single filament
 *extruder
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "extruder_stepper.h"
namespace elegoo
{
    namespace extras
    {
        PrinterExtruderStepper::PrinterExtruderStepper(
            std::shared_ptr<ConfigWrapper> config)
            : printer(config->get_printer()), extruder_name(config->get("extruder"))
        {
            extruder_stepper = std::make_shared<ExtruderStepper>(config);
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:connect",
                std::function<void()>([this]()
                                      { handle_connect(); }));
            SPDLOG_INFO("PrinterExtruderStepper init success! {}", extruder_name);
        }

        void PrinterExtruderStepper::handle_connect()
        {
            extruder_stepper->sync_to_extruder(extruder_name);
        }

        json PrinterExtruderStepper::get_status(double eventtime)
        {
            return extruder_stepper->get_status(eventtime);
        }

        std::shared_ptr<PrinterExtruderStepper> extruder_stepper_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterExtruderStepper>(config);
        }

    }
}