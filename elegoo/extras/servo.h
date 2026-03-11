/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-08-04 14:39:00
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-29 15:20:09
 * @Description  : 
 * @Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef __SERVO_H__
#define __SERVO_H__

#include <string>
#include <functional>
#include <vector>
#include <iostream>
#include "printer.h"
#include "configfile.h"
#include "pins.h"
#include "output_pin.h"

namespace elegoo
{
    namespace extras
    {
        class PrinterButtons;


        enum GrilleState{
            OPEN,
            CLOSE
        };

        class PrinterServo
        {
        public:
            PrinterServo(std::shared_ptr<ConfigWrapper> config);
            ~PrinterServo();
            json get_status(double eventtime);

        private:
            std::pair<std::string, double> _set_pwm(double print_time, double value);
            double _get_pwm_from_angle(double angle);
            double _get_pwm_from_pulse_width(double width);
            void CMD_set_servo(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_exhaust_grille_open(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_exhaust_grille_close(std::shared_ptr<GCodeCommand> gcmd);
            void CMD_set_cavity_temperature_mode(std::shared_ptr<GCodeCommand> gcmd);
            double servo_det_pin_handler(double eventtime, bool state);
            void servo_handle_ready();
            double servo_callback(double eventtime);
            double grille_callback(double eventtime);
        private:
            std::shared_ptr<ConfigWrapper> config;
            std::shared_ptr<Printer> printer;
            std::shared_ptr<SelectReactor> reactor;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<WebHooks> webhooks;
            std::shared_ptr<ReactorTimer> servo_timer;
            std::shared_ptr<ReactorTimer> grille_timer;
            std::shared_ptr<MCU_pwm> mcu_servo;
            std::shared_ptr<GCodeRequestQueue> gcrq;
            std::shared_ptr<PrinterButtons> buttons;
            int32_t servo_dir;
            double M2801_open_angle;
            double M2801_open_time;
            double M2801_close_angle;
            double M2801_close_time;
            double min_width;
            double max_width;
            double max_angle;
            double angle_to_width;
            double width_to_value;
            double last_value;
            std::string servo_name;
            bool servo_det_state;           // 归零 1 远离 0
            double start_percent;
            bool fan_enable;

            int32_t cur_cycle_count;        // L=0
            int32_t cycle_count;            // L
            double autodet_wait_time;       // J
            double failed_wait_time;        // K

            GrilleState grill_state;
            double speed;
            double last_speed;
        };
        
        std::shared_ptr<PrinterServo> servo_load_config_prefix(std::shared_ptr<ConfigWrapper> config);
    }
}

#endif