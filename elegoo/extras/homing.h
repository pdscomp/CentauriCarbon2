/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-20 14:20:28
 * @Description  : Helper code for implementing homing operations
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <vector>
#include <memory>
#include <string>
#include <map>

class PrinterRail;
class MCU_stepper;
class ToolHead;
class Printer;
class MCU_endstop;
class ConfigWrapper;
class GCodeCommand;
class MCU_pins;
class GCodeDispatch;
namespace elegoo
{
    namespace extras
    {

        class StepperPosition
        {
        public:
            StepperPosition(std::shared_ptr<MCU_stepper> stepper,
                            const std::string &endstop_name);
            ~StepperPosition();
            void note_home_end(double trigger_time);
            void verify_no_probe_skew(void);
            std::shared_ptr<MCU_stepper> stepper;
            std::string endstop_name;
            std::string stepper_name;
            int64_t start_pos;
            double start_cmd_pos;
            int64_t halt_pos;
            int64_t trig_pos;

        private:
        };

        class HomingMove : public std::enable_shared_from_this<HomingMove>
        {
        public:
            HomingMove(std::shared_ptr<Printer> printer,
                       const std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> &endstops,
                       std::shared_ptr<ToolHead> toolhead = nullptr);
            ~HomingMove();

            std::vector<std::shared_ptr<MCU_pins>> get_mcu_endstops();
            std::vector<double> calc_toolhead_pos(
                std::map<std::string, double> kin_spos,
                const std::map<std::string, double> &offsets);
            std::vector<double> homing_move(std::vector<double> movepos,
                                            double speed, bool probe_pos = false, bool triggered = true,
                                            bool check_triggered = true);
            std::string check_no_movement();
            std::vector<std::shared_ptr<StepperPosition>> stepper_positions;

        private:
            double _calc_endstop_rate(std::shared_ptr<MCU_pins> mcu_endstop,
                                      const std::vector<double> &movepos, double speed);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<ToolHead> toolhead;
            std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> endstops;
        };

        class Homing : public std::enable_shared_from_this<Homing>
        {
        public:
            Homing(std::shared_ptr<Printer> printer);
            ~Homing();

            void set_axes(const std::vector<int> &axes);
            std::vector<int> get_axes();
            int get_trigger_position(const std::string &stepper_name);
            void set_stepper_adjustment(
                const std::string &stepper_name, double adjustment);
            void set_homed_position(const std::vector<double> &pos);
            void home_rails(std::vector<std::shared_ptr<PrinterRail>> rails,
                            std::vector<double> forcepos, std::vector<double> movepos);
            int samples;
            double samples_tolerance;

        private:
            std::vector<double> _fill_coord(const std::vector<double> &coord);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<ToolHead> toolhead;
            std::vector<int> changed_axes;
            std::map<std::string, int> trigger_mcu_pos;
            std::map<std::string, double> adjust_pos;
        };

        class PrinterHoming
        {
        public:
            PrinterHoming(std::shared_ptr<ConfigWrapper> config);
            ~PrinterHoming();

            void manual_home(std::shared_ptr<ToolHead> toolhead,
                             const std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> &endstops,
                             const std::vector<double> &pos, double speed, bool triggered, bool check_triggered);
            std::vector<double> probing_move(std::shared_ptr<MCU_pins> mcu_probe,
                                             const std::vector<double> &pos, double speed);
            void cmd_G28(std::shared_ptr<GCodeCommand> gcmd);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<GCodeDispatch> gcode;
        };

        std::shared_ptr<PrinterHoming> homing_load_config(std::shared_ptr<ConfigWrapper> config);

    }
}