/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-08 10:33:38
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:17:06
 * @Description  : Code for supporting multiple steppers in single filament
 *extruder
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <string>
#include "configfile.h"
#include "json.h"
#include "extruder.h" // ExtruderStepper类
#include "printer.h"
namespace elegoo
{
    namespace extras
    {
        class PrinterExtruderStepper
        {
        public:
            PrinterExtruderStepper(std::shared_ptr<ConfigWrapper> config);
            void handle_connect();
            json get_status(double eventtime);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<ExtruderStepper> extruder_stepper;
            std::string extruder_name;
        };
        std::shared_ptr<PrinterExtruderStepper> extruder_stepper_load_config_prefix(std::shared_ptr<ConfigWrapper> config);
    }
}