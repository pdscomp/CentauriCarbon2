/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-02-21 12:14:07
 * @LastEditors  : Coconut
 * @LastEditTime : 2025-02-21 12:14:07
 * @Description  : Load cell probe support
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
#include "probe.h"
#include "load_cell.h"
#include "motion_report.h"
#include "tap_analysis.h"

class ConfigWrapper;
class Printer;

namespace elegoo
{
    namespace extras
    {

        class PrinterProbe;
        class LoadCell;
        class LoadCellProbeSessionHelper;
        class ProbeSessionContext;
        class LoadCellEndstop;
        class DumpTrapQ;

        class LoadCellProbeSessionHelper : public std::enable_shared_from_this<LoadCellProbeSessionHelper>,
                                           public ProbeSessionHelperInterface
        {
        public:
            LoadCellProbeSessionHelper() = default;
            ~LoadCellProbeSessionHelper() = default;
            void init(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<LoadCellEndstop> mcu_probe);
            std::shared_ptr<ProbeSessionHelperInterface> start_probe_session(std::shared_ptr<GCodeCommand> gcmd) override;
            void end_probe_session() override;
            json get_probe_params(std::shared_ptr<GCodeCommand> gcmd = nullptr) override;
            void run_probe(std::shared_ptr<GCodeCommand> gcmd) override;
            std::vector<std::vector<double>> pull_probed_results() override;
            std::pair<std::vector<double>, bool> single_tap(json params);

        private:
            void _handle_command_error();
            void _probe_state_error();
            void _handle_connect();

            std::shared_ptr<Printer> printer;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<ToolHead> toolhead;
            std::shared_ptr<LoadCellEndstop> mcu_probe;
            std::shared_ptr<GCodeCommand> dummy_gcode_cmd;
            double z_position;
            std::shared_ptr<HomingViaProbeHelper> homing_helper;
            double speed;
            double calibrate_accel;
            int sample_count;
            double sample_retract_dist;
            double lift_speed;
            std::string samples_result;
            double samples_tolerance;
            bool multi_probe_pending;
            std::vector<std::vector<double>> results;
            int session_state;

            int samples_retries;
            int samples_bad_times;
            double samples_bad_dist;
            double samples_bad_speed;

            double pullback_speed;
            double pullback_distance;
            double pullback_speed2;
            double pullback_distance2;

            int shake_count;
            int shake_times;
            std::shared_ptr<TemplateWrapper> shake_gcode;
        };
        class ProbeSessionContext
        {
        public:
            ProbeSessionContext(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<LoadCell> load_cell_inst);
            ~ProbeSessionContext() = default;
            double pullback_move(json params, double probing_z);
            void notify_probe_start(double print_time);
            std::pair<std::vector<double>, bool> tapping_move(std::shared_ptr<LoadCellEndstop> mcu_probe, const std::vector<double> &pos, json params);
            void set_subpath(const std::string &path) { this->subpath = path; }

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<ToolHead> toolhead;
            std::shared_ptr<LoadCell> load_cell;
            std::shared_ptr<LoadCellSampleCollector> collector;
            std::vector<double> tap_notches;
            double notch_quality;
            std::shared_ptr<DigitalFilter> tap_filter;
            std::string path;
            std::string subpath = "";

            void _handle_connect();
            std::vector<TrapezoidalMove> _extract_trapq(std::shared_ptr<DumpTrapQ> trapq, double start, double end);
        };
        class LoadCellEndstop : public std::enable_shared_from_this<LoadCellEndstop>, public MCU_pins
        {
            enum
            {
                LOAD_CELL_ERROR_NONE = 0,
                LOAD_CELL_ERROR_SAFETY_LIMIT,
                LOAD_CELL_ERROR_DRIFT_LIMIT,
                LOAD_CELL_ERROR_WATCHDOG,
            };

        public:
            const uint32_t WATCHDOG_MAX = 3;
            const uint32_t REASON_SENSOR_ERROR = MCU_trsync::REASON_COMMS_TIMEOUT + 1;

            LoadCellEndstop(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<LoadCell> load_cell_inst, std::shared_ptr<ProbeSessionContext> helper);
            ~LoadCellEndstop() = default;

            // MCU_endstop
            std::shared_ptr<MCU> get_mcu() override;
            void add_stepper(std::shared_ptr<MCU_stepper> stepper) override;
            std::vector<std::shared_ptr<MCU_stepper>> get_steppers() override;
            std::shared_ptr<ReactorCompletion> home_start(double print_time, double sample_time, int sample_count, double rest_time, bool triggered, double accel_time = 0.) override;
            double home_wait(double home_end_time) override;
            bool query_endstop(double print_time) override;

            // ProbeEndstopWrapperstd::shared_ptr<ConfigWrapper> config, std::shared_ptr<LoadCell>
            void multi_probe_begin() override;
            void multi_probe_end() override;
            std::vector<double> probing_move(const std::vector<double> &pos, double speed) override;
            void probe_prepare(std::shared_ptr<HomingMove> hmove) override;
            void probe_finish(std::shared_ptr<HomingMove> hmove) override;

            std::pair<std::vector<double>, bool> tapping_move(const std::vector<double> &pos, json params);
            json get_status(double eventtime);
            void _set_endstop_range(int32_t tare_counts);
            int32_t get_tare_counts() { return tare_counts; }

            void pause_and_tare();
            int get_oid();
            void set_tare_time(double time)
            {
                tare_time = time;
            }

            void cmd_LOAD_CELL_SET(std::shared_ptr<GCodeCommand> gcmd);
            void set_subpath(const std::string &path) { _helper->set_subpath(path); }

            double trigger_start_time;
            double trigger_time;
            double trigger_emit_time;
            int32_t trigger_sample;
            int32_t trigger_emit_sample;

        private:
            void _config_commands();
            void _build_config();
            void _handle_ready();
            void _raise_probe();
            void _lower_probe();
            void _handle_mcu_identify();
            // void _handle_load_cell_tare(std::shared_ptr<LoadCell> lc);
            std::pair<double, int> _clear_home();
            double _get_position_endstop();

            std::shared_ptr<Printer> printer;
            std::shared_ptr<ToolHead> toolhead;
            std::shared_ptr<ConfigWrapper> _config;
            std::string _config_name;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<LoadCell> _load_cell;
            std::shared_ptr<ProbeSessionContext> _helper;
            std::shared_ptr<HX71xBase> _sensor;
            std::shared_ptr<MCU> _mcu;

            std::string output_pin;
            std::shared_ptr<MCU_endstop> mcu_endstop;
            bool use_gpio;

            int _oid;
            int _sos_filter_oid;
            std::shared_ptr<TriggerDispatch> _dispatch;
            double _rest_time;
            // 触发参数
            double trigger_force_grams;
            double safety_limit_grams;
            int trigger_count;
            double static_std;
            int static_retry;

            // 去皮滤波参数
            double tare_samples;
            // 下位机滤波器配置
            std::shared_ptr<DigitalFilter> _continuous_tare_filter;

            double position_endstop;
            std::string multi;
            std::shared_ptr<CommandWrapper> _home_cmd;
            std::shared_ptr<CommandQueryWrapper> _query_cmd;
            std::shared_ptr<CommandWrapper> _set_range_cmd;

            double last_trigger_time;

            // 动态去皮
            int32_t tare_counts;
            double tare_time = 0.;
        };
        class LoadCellPrinterProbe : public std::enable_shared_from_this<LoadCellPrinterProbe>, public PrinterProbeInterface
        {
        public:
            LoadCellPrinterProbe() = default;
            ~LoadCellPrinterProbe() = default;
            void init(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<LoadCell> load_cell_inst, std::shared_ptr<LoadCellEndstop> load_cell_endstop);
            json get_probe_params(std::shared_ptr<GCodeCommand> gcmd = nullptr) override;
            std::vector<double> get_offsets() override;
            json get_status(double eventtime = std::numeric_limits<double>::quiet_NaN()) override;
            std::shared_ptr<ProbeSessionHelperInterface> start_probe_session(std::shared_ptr<GCodeCommand> gcmd) override;
            std::shared_ptr<ProbeCommandHelper> get_helper() { return cmd_helper; }
            std::shared_ptr<MCU_pins> get_mcu_probe() { return mcu_probe; }
            void set_subpath(const std::string &path) override
            {
                mcu_probe->set_subpath(path);
            }

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<LoadCellEndstop> mcu_probe;
            std::shared_ptr<LoadCell> _load_cell;
            std::shared_ptr<ProbeCommandHelper> cmd_helper;
            std::shared_ptr<ProbeOffsetsHelper> probe_offsets;
            std::shared_ptr<LoadCellProbeSessionHelper> probe_session;
            std::function<bool(double)> _query_endstop;
        };

        std::shared_ptr<PrinterProbeInterface> load_cell_probe_load_config(std::shared_ptr<ConfigWrapper> config);
    }
}