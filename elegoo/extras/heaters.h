/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : huangpeiyun
 * @LastEditTime : 2025-07-17 12:16:00
 * @Description  : Tracking of PWM controlled heaters and their temperature control
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "adc_temperature.h"
#include "json.h"
#include "liner_base.h"
#include "heater_base.h"

class Printer;
namespace elegoo
{
    namespace extras
    {

        class ControlBase;
        class ControlBangBang;
        class ControlPID;
        class Heater;
        class SensorFactory
        {
        public:
            SensorFactory(std::string name, std::shared_ptr<LinearBase> linear = nullptr);
            ~SensorFactory();

            std::shared_ptr<HeaterBase> create_sensor(std::shared_ptr<ConfigWrapper> config);

        private:
            std::string sensor_name;
            std::shared_ptr<LinearBase> linear_{nullptr};
        };

        class Heater : public std::enable_shared_from_this<Heater>
        {
        public:
            Heater(std::shared_ptr<ConfigWrapper> config,
                   std::shared_ptr<HeaterBase> sensor);
            ~Heater();
            void set_pwm(double read_time, double value);
            void temperature_callback(double read_time, double temp);
            void setup_callback(std::function<void(double, double)> callback);
            std::string get_name();
            double get_pwm_delay();
            double get_max_power();
            double get_smooth_time();
            void set_temp(double degrees);
            std::pair<double, double> get_temp(double eventtime);
            bool check_busy(double eventtime);

            std::shared_ptr<ControlBase> set_control(
                std::shared_ptr<ControlBase> control);
            void alter_target(double target_temp);
            std::pair<bool, std::string> stats(double eventtime);
            json get_status(double eventtime);
            void cmd_SET_HEATER_TEMPERATURE(std::shared_ptr<GCodeCommand> gcmd);
            bool can_extrude;
            bool is_adc_temp_fault;
            bool is_verify_heater_fault;

        private:
            void handle_shutdown();
            void handle_print_stop();

        private:
            std::shared_ptr<Printer> printer;
            // std::shared_ptr<WebHooks> webhooks;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<void> sensor;
            std::string name;
            std::string short_name;
            bool is_ot_bed_report;
            double ot_bed;
            double min_temp;
            double max_temp;
            double pwm_delay;
            double min_extrude_temp;
            double max_power;
            double smooth_time;
            double inv_smooth_time;
            double last_temp;
            double smoothed_temp;
            double target_temp;
            double last_temp_time;
            double next_pwm_time;
            double last_pwm_value;
            double last_read_time;
            bool is_shutdown;
            std::mutex lock;
            // MCU_pwm* mcu_pwm;
            std::shared_ptr<MCU_pwm> mcu_pwm;
            std::vector<std::function<void(double, double)>> callbacks;
            // 停止打印不等待升降温
            bool print_stop;

        public:
            std::shared_ptr<ControlBase> control;
        };

        class ControlBase
        {
        public:
            virtual ~ControlBase() = default;

            virtual void temperature_update(
                double read_time, double temp, double target_temp) = 0;
            virtual bool check_busy(double eventtime,
                                    double smoothed_temp, double target_temp) = 0;
            virtual void write_file(const std::string &filename) {};
        };

        class ControlBangBang : public ControlBase
        {
        public:
            ControlBangBang(std::shared_ptr<Heater> heater,
                            std::shared_ptr<ConfigWrapper> config);
            ~ControlBangBang();

            void temperature_update(
                double read_time, double temp, double target_temp);
            bool check_busy(double eventtime,
                            double smoothed_temp, double target_temp);

        private:
            std::shared_ptr<Heater> heater;
            double heater_max_power;
            double max_delta;
            bool heating;
        };

        class ControlPID : public ControlBase
        {
        public:
            ControlPID(std::shared_ptr<Heater> heater,
                       std::shared_ptr<ConfigWrapper> config);
            ~ControlPID();

            void temperature_update(
                double read_time, double temp, double target_temp);
            bool check_busy(double eventtime,
                            double smoothed_temp, double target_temp);

        private:
            std::shared_ptr<Heater> heater;
            double heater_max_power;
            double Kp;
            double Ki;
            double Kd;
            double min_deriv_time;
            double temp_integ_max;
            double prev_temp;
            double prev_temp_time;
            double prev_temp_deriv;
            double prev_temp_integ;
        };

        class ControlAutoTune : public ControlBase
        {
        public:
            ControlAutoTune(std::shared_ptr<Heater> heater, double target);
            ~ControlAutoTune();
            void set_pwm(double read_time, double value);
            void temperature_update(double read_time, double temp, double target_temp);
            bool check_busy(double eventtime,
                            double smoothed_temp, double target_temp);
            void check_peaks();
            std::tuple<double, double, double> calc_pid(int pos);
            std::tuple<double, double, double> calc_final_pid();
            void write_file(const std::string &filename);

        private:
            std::shared_ptr<Heater> heater;
            std::vector<std::pair<double, double>> pwm_samples;
            std::vector<std::pair<double, double>> temp_samples;
            std::vector<std::pair<double, double>> peaks;
            double heater_max_power;
            double calibrate_temp;
            bool heating;
            double peak;
            double peak_time;
            double last_pwm;
        };

        class PrinterHeaters
        {
        public:
            PrinterHeaters(std::shared_ptr<ConfigWrapper> config);
            ~PrinterHeaters();

            void load_config(std::shared_ptr<ConfigWrapper> config);
            void add_sensor_factory(const std::string &sensor_type,
                                    std::shared_ptr<SensorFactory> sensor_factory);
            std::shared_ptr<Heater> setup_heater(
                std::shared_ptr<ConfigWrapper> config,
                const std::string &gcode_id);
            void setup_heater();
            std::vector<std::string> get_all_heaters();
            std::map<std::string, std::shared_ptr<Heater>> get_heaters();
            std::shared_ptr<Heater> lookup_heater(const std::string &heater_name);
            std::shared_ptr<HeaterBase> setup_sensor(std::shared_ptr<ConfigWrapper> config);
            void register_sensor(std::shared_ptr<ConfigWrapper> config,
                                 std::shared_ptr<Heater> psensor,
                                 const std::string &gcode_id = "");
            void register_monitor(std::shared_ptr<ConfigWrapper> config);
            json get_status(double eventtime);
            void turn_off_all_heaters(double print_time = 0.0);
            void cmd_TURN_OFF_HEATERS(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_M105(std::shared_ptr<GCodeCommand> gcmd);
            void set_temperature(std::shared_ptr<Heater> heater, double temp, bool wait = false);
            void cmd_TEMPERATURE_WAIT(std::shared_ptr<GCodeCommand> gcmd);

        private:
            void handle_ready();
            std::string get_temp(double eventtime);
            void wait_for_temperature(std::shared_ptr<Heater> heater);
            bool print_stop;
        private:
            std::shared_ptr<Printer> printer;
            bool has_started;
            bool have_load_sensors;
            std::map<std::string, std::shared_ptr<SensorFactory>> sensor_factories;
            std::map<std::string, std::shared_ptr<Heater>> heaters;
            std::map<std::string, std::shared_ptr<Heater>> gcode_id_to_sensor;
            std::vector<std::string> available_heaters;
            std::vector<std::string> available_monitors;
            std::vector<std::string> available_sensors;
        };

        std::shared_ptr<PrinterHeaters> heaters_load_config(std::shared_ptr<ConfigWrapper> config);

    }
}