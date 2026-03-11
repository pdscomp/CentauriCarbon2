/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-07 16:07:17
 * @LastEditors  : huangpeiyun
 * @LastEditTime : 2025-05-30 15:19:18
 * @Description  : Printer cooling fan
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include "configfile.h"
#include "mcu.h"
#include "output_pin.h"
#include "printer.h"
#include "pulse_counter.h"

class GCodeDispatch;
namespace elegoo
{
    namespace extras
    {
        class Fan;
        class FanTachometer;
        class PrinterFan;

        class Fan
        {
        public:
            Fan(std::shared_ptr<ConfigWrapper> config,
                double default_shutdown_speed = 0.0);
            void set_speed(double value, double print_time = DOUBLE_NONE);
            void set_speed_from_command(double value);
            std::shared_ptr<MCU> get_mcu() const;
            json get_status(double eventtime);

        private:
            std::pair<std::string, double> _apply_speed(double print_time, double value);
            void _handle_request_restart(double print_time);
            double check_fan(double eventtime);

            std::shared_ptr<Printer> printer;
            double last_fan_value;
            double last_req_value;
            double max_power;
            double kick_start_time;
            double off_below;
            double cycle_time;
            bool hardware_pwm;
            double shutdown_speed;
            int max_error;
            int cur_error;
            std::string fan_name;
            std::shared_ptr<GCodeDispatch> gcode;
            // std::shared_ptr<WebHooks> webhooks;
            std::shared_ptr<MCU_pwm> mcu_fan;
            std::shared_ptr<MCU_digital_out> enable_pin;
            std::shared_ptr<GCodeRequestQueue> gcrq;
            std::shared_ptr<FanTachometer> tachometer;
            std::shared_ptr<ReactorTimer> check_timer;
            std::shared_ptr<SelectReactor> reactor;
            bool is_report;
        };

        class FanTachometer
        {
        public:
            FanTachometer(std::shared_ptr<ConfigWrapper> config);
            json get_status(double eventtime);

        private:
            std::shared_ptr<FrequencyCounter> freq_counter;
            int ppr;
        };

        class PrinterFan
        {
        public:
            PrinterFan(std::shared_ptr<ConfigWrapper> config);
            json get_status(double eventtime);
            void cmd_M106(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_M107(std::shared_ptr<GCodeCommand> gcmd);

        private:
            std::shared_ptr<Fan> fan;
            std::shared_ptr<GCodeDispatch> gcode;
        };

        std::shared_ptr<PrinterFan> fan_load_config(std::shared_ptr<ConfigWrapper> config);

    }
}