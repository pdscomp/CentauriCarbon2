/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : huangpeiyun
 * @LastEditTime : 2025-07-29 14:22:59
 * @Description  : PWM and digital output pin handling
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <memory>
#include <functional>
#include <vector>
#include "json.h"
#include "mcu.h"
#include "utilities.h"

class ConfigWrapper;
class MCU;
class Printer;
class ToolHead;
class GCodeCommand;
class GCodeDispatch;
namespace elegoo
{
    namespace extras
    {

        class GCodeRequestQueue
        {
        public:
            GCodeRequestQueue(std::shared_ptr<ConfigWrapper> config,
                              std::shared_ptr<MCU> mcu,
                              std::function<std::pair<std::string, double>(double, double)> callback);
            ~GCodeRequestQueue();

            void queue_gcode_request(double value);
            void send_async_request(double value, double print_time = DOUBLE_NONE);

        private:
            void handle_connect();
            void flush_notification(double print_time, double clock);
            void queue_request(double print_time, double value);

        private:
            std::function<std::pair<std::string, double>(double, double)> callback;
            std::shared_ptr<Printer> printer;
            std::shared_ptr<MCU> mcu;
            std::shared_ptr<ToolHead> toolhead;
            std::vector<std::pair<double, double>> rqueue;
            double next_min_flush_time;
        };

        class PrinterOutputPin
        {
        public:
            PrinterOutputPin(std::shared_ptr<ConfigWrapper> config);
            ~PrinterOutputPin();
            json get_status(double eventtime);
            void cmd_SET_PIN(std::shared_ptr<GCodeCommand> gcmd);

        private:
            std::pair<std::string, double> set_pin(double print_time, double value);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<ConfigWrapper> config;
            std::shared_ptr<MCU_pins> mcu_pin;
            double last_value;
            int led_value = 0;
            bool is_pwm;
            double scale;
            double shutdown_value;
            std::shared_ptr<GCodeRequestQueue> gcrq;
        };

        class PrinterTemplateEvaluator
        {
        private:
            /* data */
        public:
            PrinterTemplateEvaluator(std::shared_ptr<ConfigWrapper> config);
            ~PrinterTemplateEvaluator();
            void _activate_timer();
            void set_template(std::shared_ptr<GCodeCommand> gcmd, std::function<void(const std::string &)> callback, std::function<void(void)> flush_callback = nullptr);
        };

        std::shared_ptr<PrinterTemplateEvaluator> lookup_template_eval(std::shared_ptr<ConfigWrapper> config);

        std::shared_ptr<PrinterOutputPin> output_pin_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config);

    }
}