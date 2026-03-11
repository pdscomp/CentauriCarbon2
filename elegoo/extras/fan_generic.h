/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2025-01-18 15:57:12
 * @LastEditors  : error: git config user.name & please set dead value or install git
 * @LastEditTime : 2025-02-27 14:47:48
 * @Description  :
 *
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#pragma once

#include <memory>
#include <string>
#include "configfile.h"
#include "fan.h"
#include "mcu.h"
#include "output_pin.h"
#include "printer.h"
namespace elegoo
{
    namespace extras
    {
        class PrinterFanGeneric
        {
        public:
            PrinterFanGeneric(std::shared_ptr<ConfigWrapper> config);
            json get_status(double eventtime);
            void cmd_SET_FAN_SPEED(std::shared_ptr<GCodeCommand> gcmd);

        private:
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<Fan> fan;
            std::string fan_name;
            // std::shared_ptr<PrinterTemplateEvaluator> template_eval;
            // void _template_update(const std::string &text);
        };

        std::shared_ptr<PrinterFanGeneric> fan_generic_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config);

    }
}