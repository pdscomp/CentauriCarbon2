/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-13 17:21:10
 * @Description  : Run user defined actions in place of a normal G28 homing command
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <functional>

class ConfigWrapper;
class Printer;
class GCodeCommand;
class GCodeDispatch;
class ToolHead;
namespace elegoo
{
    namespace extras
    {
        class TemplateWrapper;
        class HomingOverride
        {
        public:
            HomingOverride(std::shared_ptr<ConfigWrapper> config);
            ~HomingOverride();
            void handle_ready();
            void cmd_G28(std::shared_ptr<GCodeCommand> gcmd);
            std::string to_upper(const std::string &input);
            std::map<std::string, double> hops[3];


            double home_x_pos;
            double home_y_pos;
            double home_xy_speed;
            double home_x_pos2;
            double home_y_pos2;
            double home_xy_speed2;

            double home_x_pos_probe;
            double home_y_pos_probe;
            double home_xy_speed_probe;
            double home_x_pos2_probe;
            double home_y_pos2_probe;
            double home_xy_speed2_probe;

            double home_x_pos_endstop;
            double home_y_pos_endstop;
            double home_xy_speed_endstop;
            double home_x_pos2_endstop;
            double home_y_pos2_endstop;
            double home_xy_speed2_endstop;

            double homing_accel;
            double homing_current_decay;
            double z_retract_pos;
            double z_retract_speed;
            double z_samples;
            double z_samples_tolerance;

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<ToolHead> toolhead;
            std::vector<double> start_pos;
            std::shared_ptr<TemplateWrapper> template_wrapper;
            std::shared_ptr<GCodeDispatch> gcode;
            std::function<void(std::shared_ptr<GCodeCommand>)> prev_G28;
            std::string axes;
            bool in_script;
            double safe_home_temp;

            void single_home(int axis, double homing_speed);
            void single_retract(int axis, double retract_dist, double retract_speed, bool wait);
            void homing_ready(int axis, bool homed);
            void homing_set_run_current(int axis, double current);
            double homing_get_run_current(int axis);
            void homing_set_accel(uint32_t accel);
            uint32_t homing_get_accel();
            int is_probe();
        };

        std::shared_ptr<HomingOverride> homing_override_load_config(
            std::shared_ptr<ConfigWrapper> config);
    }
}