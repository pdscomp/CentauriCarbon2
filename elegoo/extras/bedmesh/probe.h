/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : Ben
 * @LastEditTime : 2024-11-28 21:56:54
 * @Description  : Z-Probe support
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <cmath>
#include <algorithm>
#include <array>
#include <memory>
#include <array>
#include <vector>
#include <string>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include "json.h"
#include "chipbase.h"

class ConfigWrapper;
class Printer;
class ReactorCompletion;
class GCodeCommand;
class GCodeDispatch;
class MCU_pins;
class MCU_stepper;
class PrinterRail;
class PinParams;

namespace elegoo
{
    namespace extras
    {
        class ProbeCommandHelper;
        class HomingViaProbeHelper;
        class ProbeSessionHelperInterface;
        class ProbeSessionHelper;
        class ProbeOffsetsHelper;
        class ProbePointsHelper;
        class ProbeEndstopWrapper;
        class PrinterProbeInterface;
        class PrinterProbe;
        class Homing;
        class HomingMove;
        class GCodeMove;
        class PrinterGCodeMacro;
        class TemplateWrapper;
        const std::string HINT_TIMEOUT = "If the probe did not move far enough to trigger, then consider reducing the Z axis minimum position so the probe can travel further (the Z minimum position can be negative).";
        std::vector<double> calc_probe_z_average(const std::vector<std::vector<double>> &positions, const std::string &method);
        std::vector<double> run_single_probe(std::shared_ptr<PrinterProbeInterface> probe, std::shared_ptr<GCodeCommand> gcmd);

        class ProbeCommandHelper
        {
        public:
            ProbeCommandHelper(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<PrinterProbeInterface> probe, std::function<bool(double)> query_endstop = nullptr);
            ~ProbeCommandHelper() = default;

            void cmd_QUERY_PROBE(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_PROBE(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_PROBE_CALIBRATE(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_PROBE_ACCURACY(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_PROBE_FIVE_POINTS(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_Z_OFFSET_APPLY_PROBE(std::shared_ptr<GCodeCommand> gcmd);
            void probe_calibrate_finalize(const std::vector<double> &kin_pos = {});
            json get_status(double eventtime);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<PrinterProbeInterface> probe;
            std::string name;
            bool last_state;
            double last_z_result;
            double probe_calibrate_z;
            std::function<bool(double)> query_endstop;
            void _move(const std::vector<double> &coord, double speed);
        };

        class HomingViaProbeHelper : public std::enable_shared_from_this<HomingViaProbeHelper>, public ChipBase
        {
        public:
            HomingViaProbeHelper() = default;
            ~HomingViaProbeHelper() = default;
            void init(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<MCU_pins> mcu_probe);
            std::shared_ptr<MCU_pins> setup_pin(const std::string &pin_type, std::shared_ptr<PinParams> pin_params) override;

        private:
            void _handle_mcu_identify(void);
            void _handle_homing_move_begin(std::shared_ptr<HomingMove> hmove);
            void _handle_homing_move_end(std::shared_ptr<HomingMove> hmove);
            void _handle_home_rails_begin(std::shared_ptr<Homing> homing_state, std::vector<std::shared_ptr<PrinterRail>> rails);
            void _handle_home_rails_end(std::shared_ptr<Homing> homing_state, std::vector<std::shared_ptr<PrinterRail>> rails);
            void _handle_command_error(void);

            std::shared_ptr<Printer> printer;
            std::shared_ptr<MCU_pins> mcu_probe;
            bool multi_probe_pending;
        };

        class ProbeSessionHelperInterface
        {
        public:
            virtual ~ProbeSessionHelperInterface() {}
            virtual std::shared_ptr<ProbeSessionHelperInterface> start_probe_session(std::shared_ptr<GCodeCommand> gcmd) = 0;
            virtual void end_probe_session() = 0;
            virtual json get_probe_params(std::shared_ptr<GCodeCommand> gcmd = nullptr) = 0;
            virtual void run_probe(std::shared_ptr<GCodeCommand> gcmd) = 0;
            virtual std::vector<std::vector<double>> pull_probed_results() = 0;
        };

        class ProbeSessionHelper : public std::enable_shared_from_this<ProbeSessionHelper>, public ProbeSessionHelperInterface
        {
        public:
            ProbeSessionHelper() = default;
            ~ProbeSessionHelper() = default;
            void init(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<ProbeEndstopWrapper> mcu_probe);
            std::shared_ptr<ProbeSessionHelperInterface> start_probe_session(std::shared_ptr<GCodeCommand> gcmd) override;
            void end_probe_session() override;
            json get_probe_params(std::shared_ptr<GCodeCommand> gcmd = nullptr) override;
            void run_probe(std::shared_ptr<GCodeCommand> gcmd) override;
            std::vector<std::vector<double>> pull_probed_results() override;

        private:
            void _handle_command_error();
            void _probe_state_error();
            std::vector<double> _probe(double speed);

            std::shared_ptr<Printer> printer;
            std::shared_ptr<ProbeEndstopWrapper> mcu_probe;
            std::shared_ptr<GCodeCommand> dummy_gcode_cmd;
            double z_position;
            std::shared_ptr<HomingViaProbeHelper> homing_helper;
            double speed;
            double lift_speed;
            int sample_count;
            double sample_retract_dist;
            std::string samples_result;
            double samples_tolerance;
            int samples_retries;
            bool multi_probe_pending;
            std::vector<std::vector<double>> results;
        };

        class ProbeOffsetsHelper
        {
        public:
            ProbeOffsetsHelper(std::shared_ptr<ConfigWrapper> config);
            ~ProbeOffsetsHelper() = default;
            std::vector<double> get_offsets();

        private:
            double x_offset, y_offset, z_offset;
        };

        class ProbePointsHelper
        {
        public:
            ProbePointsHelper(std::shared_ptr<ConfigWrapper> config,
                              std::function<std::string(const std::vector<double> &, std::vector<std::vector<double>> &)> finalize_callback,
                              std::vector<std::vector<double>> default_points = {});
            ~ProbePointsHelper() = default;
            void minimum_points(int n);
            void update_probe_points(const std::vector<std::vector<double>> &points, int min_points);
            void use_xy_offsets(bool use_offsets);
            double get_lift_speed();
            void start_probe(std::shared_ptr<GCodeCommand> gcmd);
            json get_web_feedback();
            void notify_failed();

        private:
            void _move(const std::vector<double> &coord, double speed);
            void _raise_tool(bool is_first = false);
            bool _invoke_callback(std::vector<std::vector<double>> &results);
            void _move_next(int probe_num);
            void _manual_probe_start();
            void _manual_probe_finalize(const std::vector<double> &kin_pos);
            void bed_mesh_calibrate_ready(std::shared_ptr<GCodeCommand> gcmd);
            void bed_mesh_calibrate_done(std::shared_ptr<GCodeCommand> gcmd);

            std::shared_ptr<Printer> printer;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<PrinterGCodeMacro> gcode_macro;
            std::function<std::string(const std::vector<double> &, std::vector<std::vector<double>> &)> finalize_callback;
            std::string name;
            std::vector<std::vector<double>> probe_points;
            double default_horizontal_move_z;
            double horizontal_move_z;
            double speed;
            bool use_offsets;
            double lift_speed;
            std::vector<double> probe_offsets;
            std::vector<std::vector<double>> manual_results;
            json web_feedback;
            double calibrate_accel;
            double orign_accel;
        };

        class ProbeEndstopWrapper : public std::enable_shared_from_this<ProbeEndstopWrapper>, public MCU_pins
        {
        public:
            ProbeEndstopWrapper(std::shared_ptr<ConfigWrapper> config);
            ~ProbeEndstopWrapper() = default;

            // MCU_endstop
            std::shared_ptr<MCU> get_mcu() override;
            void add_stepper(std::shared_ptr<MCU_stepper> stepper) override;
            std::vector<std::shared_ptr<MCU_stepper>> get_steppers() override;
            std::shared_ptr<ReactorCompletion> home_start(double print_time, double sample_time, int sample_count, double rest_time, bool triggered, double accel_time = 0.) override;
            double home_wait(double home_end_time) override;
            bool query_endstop(double print_time) override;

            // ProbeEndstopWrapper
            void multi_probe_begin() override;
            void multi_probe_end() override;
            std::vector<double> probing_move(const std::vector<double> &pos, double speed) override;
            void probe_prepare(std::shared_ptr<HomingMove> hmove) override;
            void probe_finish(std::shared_ptr<HomingMove> hmove) override;

        private:
            void _raise_probe();
            void _lower_probe();
            double _get_position_endstop();
            std::shared_ptr<Printer> printer;
            double position_endstop;
            bool stow_on_each_sample;
            std::shared_ptr<MCU_pins> mcu_endstop;
            std::string multi;
        };

        class PrinterProbeInterface
        {
        public:
            virtual ~PrinterProbeInterface() {}
            virtual json get_probe_params(std::shared_ptr<GCodeCommand> gcmd = nullptr) = 0;
            virtual std::vector<double> get_offsets() = 0;
            virtual json get_status(double eventtime = std::numeric_limits<double>::quiet_NaN()) = 0;
            virtual std::shared_ptr<ProbeSessionHelperInterface> start_probe_session(std::shared_ptr<GCodeCommand> gcmd) = 0;
            virtual std::shared_ptr<ProbeCommandHelper> get_helper() = 0;
            virtual std::shared_ptr<MCU_pins> get_mcu_probe() = 0;
            virtual void set_subpath(const std::string &path) {}
        };

        class PrinterProbe : public std::enable_shared_from_this<PrinterProbe>, public PrinterProbeInterface
        {
        public:
            PrinterProbe() = default;
            ~PrinterProbe() = default;
            void init(std::shared_ptr<ConfigWrapper> config);
            json get_probe_params(std::shared_ptr<GCodeCommand> gcmd = nullptr) override;
            std::vector<double> get_offsets() override;
            json get_status(double eventtime = std::numeric_limits<double>::quiet_NaN()) override;
            std::shared_ptr<ProbeSessionHelperInterface> start_probe_session(std::shared_ptr<GCodeCommand> gcmd) override;
            std::shared_ptr<ProbeCommandHelper> get_helper() { return cmd_helper; }
            std::shared_ptr<MCU_pins> get_mcu_probe() { return mcu_probe; }

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<ProbeEndstopWrapper> mcu_probe;
            std::shared_ptr<ProbeCommandHelper> cmd_helper;
            std::shared_ptr<ProbeOffsetsHelper> probe_offsets;
            std::shared_ptr<ProbeSessionHelper> probe_session;
            std::function<bool(double)> _query_endstop;
        };

        std::shared_ptr<PrinterProbeInterface> probe_load_config(std::shared_ptr<ConfigWrapper> config);

    }
}