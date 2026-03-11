/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2024-11-07 14:24:31
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-20 16:37:57
 * @Description  : Z backlash and temperature compensation
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <functional>

class SelectReactor;
class ReactorTimer;
class ReactorCompletion;
class ConfigWrapper;
class Printer;
class GCodeDispatch;
class MCU;
class CommandWrapper;
class ToolHead;
class GCodeMoveTransform;
class PrinterRail;
class GCodeCommand;

namespace elegoo
{
    namespace extras
    {
        class Heater;
        class Homing;
        class TemplateWrapper;
        class ZCompensationHeater
        {
        public:
            ZCompensationHeater(std::shared_ptr<Printer> printer, const std::string &name, double start, double diff, std::vector<double> datas, bool use_fix_ref_temperature, double fix_ref_temperature);
            ~ZCompensationHeater() = default;
            void handle_home_rails_end();
            double calc_adjust();
            double calc_abs_offset(double temp);

        private:
            std::shared_ptr<Heater> heater;
            double coeff;
            double smoothed_temp;
            double ref_temperature;
            std::string name;
            double start;
            double diff;
            std::vector<double> temps;
            std::vector<double> datas;
            double slope, intercept;
            bool use_fix_ref_temperature;
            double fix_ref_temperature;
        };

        class ZCompensation
        {
        public:
            ZCompensation(std::shared_ptr<ConfigWrapper> config);
            ~ZCompensation() = default;

        private:
            void handle_connect();
            void handle_home_rails_end(std::shared_ptr<Homing> homing_state, std::vector<std::shared_ptr<PrinterRail>> rails);
            std::vector<double> get_position();
            void move(const std::vector<double> &newpos, double speed);
            std::vector<double> calc_adjust(std::vector<double> pos);
            std::vector<double> calc_unadjust(std::vector<double> pos);
            void cmd_NOZZLE_TEMP_CALIBRATE(std::shared_ptr<GCodeCommand> gcmd);

            std::shared_ptr<Printer> printer;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<ToolHead> toolhead;
            std::shared_ptr<GCodeMoveTransform> move_transform;
            std::shared_ptr<GCodeMoveTransform> next_transform;
            double z_step_dist;
            bool z_homed;
            // 当前的调整值
            double last_z_adjust_mm;
            double z_adjust_mm;
            std::vector<double> last_position;

            std::vector<std::shared_ptr<ZCompensationHeater>> heaters;

            // 校准参数
            double nozzle_temp_start;
            double nozzle_temp_end;
            double nozzle_temp_diff;
            // 使用参考温度
            bool use_fix_ref_temperature;
            double fix_ref_temperature;

            std::shared_ptr<TemplateWrapper> clean_nozzle_gcode;
            double nozzle_calibrate_start;
            double nozzle_calibrate_diff;
            std::vector<double> nozzle_calibrate_datas;
        };

        std::shared_ptr<ZCompensation> z_compensation_load_config(std::shared_ptr<ConfigWrapper> config);
    }
}