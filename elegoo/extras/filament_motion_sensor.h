/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-07-24 16:44:27
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-16 15:15:14
 * @Description  : 
 * @Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <string>
#include <functional>
#include <memory>
#include "printer.h"
#include "configfile.h"
#include "pause_resume.h"
#include "gcode_macro.h"
#include "buttons.h"
#include "common/utilities.h"


namespace elegoo
{
    namespace extras
    {
        class RunoutHelper;
        class Canvas;
        
        class EncoderSensor
        {
        public:
            EncoderSensor(std::shared_ptr<ConfigWrapper> config);
            ~EncoderSensor();

            std::function<json(double)> get_status;
        private:
            void _handle_ready();
            void _handle_printing(double print_time);
            void _handle_not_printing(double print_time);
            double _get_extruder_pos(double eventtime = DOUBLE_NONE);
            void _update_filament_runout_pos(double eventtime = DOUBLE_NONE);
            double encoder_event(double eventtime, bool state);
            double switch_event(double eventtime, bool state);
            double _extruder_pos_update_event(double eventtime);
            void CMD_encoder_sensor_switch(std::shared_ptr<GCodeCommand> gcmd);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<SelectReactor> reactor;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<RunoutHelper> runout_helper;
            std::shared_ptr<PrinterExtruder> extruder;
            std::shared_ptr<Canvas> canvas;
            std::shared_ptr<ReactorTimer> _extruder_pos_update_timer;
            std::function<json(double)> estimated_print_time;
            std::string extruder_name;
            double detection_length;
            double filament_runout_pos;
            bool enable;
            bool sensor_enable;
        };
        
        std::shared_ptr<EncoderSensor> encoder_sensor_load_config_prefix(std::shared_ptr<ConfigWrapper> config);
    }
}
