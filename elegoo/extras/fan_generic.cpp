/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-07 16:07:17
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:20:13
 * @Description  : Support fans that are controlled by gcode
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "fan_generic.h"
#include "logger.h"
#include "utilities.h"

namespace elegoo
{
    namespace extras
    {
        PrinterFanGeneric::PrinterFanGeneric(std::shared_ptr<ConfigWrapper> config)
        {
            fan = std::make_shared<Fan>(config, 0.0);
            fan_name = config->get_name().substr(config->get_name().find_last_of(' ') + 1);
            // template_eval = lookup_template_eval(config);
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(config->get_printer()->lookup_object("gcode"));
            gcode->register_mux_command("SET_FAN_SPEED", "FAN", fan_name,
                std::bind(&PrinterFanGeneric::cmd_SET_FAN_SPEED,
                this, std::placeholders::_1));
        }

        json PrinterFanGeneric::get_status(double eventtime)
        {
            return fan->get_status(eventtime);
        }

        void PrinterFanGeneric::cmd_SET_FAN_SPEED(std::shared_ptr<GCodeCommand> gcmd)
        {
            double speed = gcmd->get_double("SPEED", 0);
            fan->set_speed_from_command(speed);
        }

        std::shared_ptr<PrinterFanGeneric> fan_generic_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterFanGeneric>(config);
        }

    }
}