/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-08-04 14:39:06
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-29 15:22:15
 * @Description  : 
 * @Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "servo.h"
#include "buttons.h"


namespace elegoo
{
    namespace extras
    {
        // #undef SPDLOG_DEBUG
        // #define SPDLOG_DEBUG SPDLOG_INFO


        const double SERVO_SIGNAL_PERIOD = 0.020;
        const std::string cmd_SET_SERVO_help = "Set servo angle";

        PrinterServo::PrinterServo(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("{} init __start",__func__);
            this->printer = config->get_printer();
            this->reactor = printer->get_reactor();
            this->M2801_open_angle = config->getdouble("M2801_open_angle",45,0,90);
            this->M2801_open_time = config->getdouble("M2801_open_time",10);
            this->M2801_close_angle = config->getdouble("M2801_close_angle",45,0,90);
            this->M2801_close_time = config->getdouble("M2801_close_time",300);
            this->servo_dir = config->getdouble("servo_dir",1,-1,1);
            this->min_width = config->getdouble("minimum_pulse_width",0.001,0.001,0.002,0.,SERVO_SIGNAL_PERIOD);
            this->max_width = config->getdouble("maximum_pulse_width",0.002,0.001,0.002,this->min_width,SERVO_SIGNAL_PERIOD);
            this->max_angle = config->getdouble("maximum_servo_angle",180.);
            this->angle_to_width = (this->max_width - this->min_width) / this->max_angle;
            this->width_to_value = 1. / SERVO_SIGNAL_PERIOD;
            this->last_value = 0.;
            this->speed = 0;
            this->last_speed = 0;
            servo_det_state = true;
            double initial_pwm = 0.;
            double iangle = config->getdouble("initial_angle",DOUBLE_NONE,0.,360.);
            if(!std::isnan(iangle))
            {
                initial_pwm = this->_get_pwm_from_angle(iangle);
            }
            else
            {
                double iwidth = config->getdouble("initial_pulse_width",0.,0.,this->max_width);
                initial_pwm = this->_get_pwm_from_pulse_width(iwidth);
            }
            start_percent = config->getdouble("start_percent",100,0,100);
            fan_enable = config->getint("fan_enable",1,0,1);
            // Setup mcu_servo pin
            std::shared_ptr<PrinterPins> ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
            std::string servo_pin = config->get("servo_pin","");
            this->mcu_servo = std::static_pointer_cast<MCU_pwm>(ppins->setup_pin("pwm",servo_pin));
            mcu_servo->setup_max_duration(0.);
            mcu_servo->setup_cycle_time(SERVO_SIGNAL_PERIOD);
            mcu_servo->setup_start_value(initial_pwm,0.);
            // Create gcode request queue
            this->gcrq = std::make_shared<GCodeRequestQueue>(
                config, this->mcu_servo->get_mcu(),
                std::bind(&PrinterServo::_set_pwm, this, std::placeholders::_1,
                          std::placeholders::_2));
            // Register commands
            this->servo_name = elegoo::common::split(config->get_name()).back();
            this->gcode = any_cast<std::shared_ptr<GCodeDispatch>>(this->printer->lookup_object("gcode"));
            gcode->register_mux_command("SET_SERVO","SERVO",servo_name,
                    [this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_set_servo(gcmd);
                    },cmd_SET_SERVO_help);
            gcode->register_command("EXHAUST_GRILLE_OPEN",
                    [this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_exhaust_grille_open(gcmd);
                    },false,"open exhauxt grille");
            gcode->register_command("EXHAUST_GRILLE_CLOSE",
                    [this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_exhaust_grille_close(gcmd);
                    },false,"close exhauxt grille");
            gcode->register_command("SET_CAVITY_TEMPERATURE_MODE"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_set_cavity_temperature_mode(gcmd);
                    }
                    ,false,"SET CAVITY TEMPERATURE MODE");
            //
            std::string servo_det_pin = config->get("servo_det_pin","");
            buttons = any_cast<std::shared_ptr<PrinterButtons>>(printer->load_object(config, "buttons"));
            buttons->register_buttons({servo_det_pin}
                , [this](double eventtime, bool state) {
                    return this->servo_det_pin_handler(eventtime, state);
                }
            );

            webhooks = any_cast<std::shared_ptr<WebHooks>>(printer->lookup_object("webhooks"));

            this->grille_timer = reactor->register_timer(
                [this](double eventtime){ 
                    return grille_callback(eventtime); 
                }
                , _NEVER, "grille timer"
            );
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this](){
                    servo_handle_ready();
                })
            );

            SPDLOG_INFO("{} init __over",__func__);
        }
        
        PrinterServo::~PrinterServo()
        {
            SPDLOG_INFO("~PrinterServo");
        }
        
        void PrinterServo::servo_handle_ready()
        {
            cur_cycle_count = 0;
            cycle_count = 5;
            autodet_wait_time = 5;
            failed_wait_time = 60;
            
            grill_state = GrilleState::OPEN;
            reactor->update_timer(grille_timer,get_monotonic() + 2.);
        }
        
        double PrinterServo::grille_callback(double eventtime)
        {
            if(grill_state == GrilleState::OPEN)
            {
                if(!servo_det_state)
                { 
                    if(cur_cycle_count > cycle_count)
                    {
                        cur_cycle_count = 0;
                        webhooks->report_error(elegoo::common::ErrorCode::GRILLE_OPEN_FAILED, elegoo::common::ErrorLevel::WARNING, "");
                        SPDLOG_INFO("grille open failed!");
                        speed = _get_pwm_from_angle(90);
                        return get_monotonic() + failed_wait_time;
                    }
    
                    speed = _get_pwm_from_angle(90. + servo_dir * M2801_open_angle);
                    cur_cycle_count++;
                    SPDLOG_INFO("servo go home!");
                }
                else
                {
                    speed = _get_pwm_from_angle(90);
                    cur_cycle_count = 0;
                }
            }
            else if(grill_state == GrilleState::CLOSE)
            {
                if(servo_det_state)
                { 
                    if(cur_cycle_count > cycle_count)
                    {
                        cur_cycle_count = 0;
                        webhooks->report_error(elegoo::common::ErrorCode::GRILLE_CLOSE_FAILED, elegoo::common::ErrorLevel::WARNING, "");
                        SPDLOG_INFO("grille close failed!");
                        speed = _get_pwm_from_angle(90);
                        return get_monotonic() + failed_wait_time;
                    }
                                    
                    speed = _get_pwm_from_angle(90. - servo_dir * M2801_close_angle);
                    cur_cycle_count++;
                    SPDLOG_INFO("servo away from home!");
                }
                else
                {
                    speed = _get_pwm_from_angle(90);
                    cur_cycle_count = 0;
                }
            }

            if(speed != last_speed)
            {          
                last_speed = speed;          
                gcrq->send_async_request(speed);
            }

            return get_monotonic() + autodet_wait_time;
        }


        double PrinterServo::servo_det_pin_handler(double eventtime, bool state)
        {
            this->servo_det_state = !state;
            return 0.0;
        }

        void PrinterServo::CMD_exhaust_grille_open(std::shared_ptr<GCodeCommand> gcmd)
        {
            cycle_count = gcmd->get_int("A", 5);
            autodet_wait_time = gcmd->get_double("B", 5.);
            failed_wait_time = gcmd->get_int("C", 60.);

            if(fan_enable)
            {
                gcode->run_script_from_command("M106 P2 S" +std::to_string(255 * start_percent / 100));
            }

            elegoo::common::SignalManager::get_instance().emit_signal<bool>(
                "elegoo:cavity_mode", false);

            grill_state = GrilleState::OPEN;
            SPDLOG_INFO("start grille open check.");
        }

        void PrinterServo::CMD_exhaust_grille_close(std::shared_ptr<GCodeCommand> gcmd)
        {
            cycle_count = gcmd->get_int("A", 5);
            autodet_wait_time = gcmd->get_double("B", 5.);
            failed_wait_time = gcmd->get_int("C", 60.);

            elegoo::common::SignalManager::get_instance().emit_signal<bool>(
                "elegoo:cavity_mode", true);
                
            grill_state = GrilleState::CLOSE;
            SPDLOG_INFO("start grille close check.");
        }
        
        void PrinterServo::CMD_set_cavity_temperature_mode(std::shared_ptr<GCodeCommand> gcmd)
        {
            int mode = gcmd->get_int("MODE",0,0,1);
            if(!mode)
            {
                gcode->run_script_from_command("EXHAUST_GRILLE_OPEN");
            }
            else
            {
                gcode->run_script_from_command("EXHAUST_GRILLE_CLOSE");
            }
        }

        // void PrinterServo::CMD_M2801(std::shared_ptr<GCodeCommand> gcmd)
        // {
        //     angle = gcmd->get_double("S", 0, 0., 90.);
        //     timeout = gcmd->get_double("T", 500.);
        //     cmd_mode = gcmd->get_int("A", 0, 0, 1);
        //     SPDLOG_INFO("{} angle:{},timeout:{},cmd_mode:{},servo_det_state:{}",__func__,angle,timeout,cmd_mode,servo_det_state);

        //     if(cmd_mode)
        //     {
        //         if(servo_det_state)
        //         {
        //             SPDLOG_INFO("servo is already go home!");
        //             return;
        //         }

        //         gcode->run_script_from_command("SET_SERVO SERVO=printer_servo ANGLE=" + std::to_string(90. + servo_dir * angle));
        //         double cur_time = get_monotonic();
        //         double end_time = cur_time + 3;
            
        //         while(cur_time < end_time)
        //         {
        //             cur_time = get_monotonic() + 0.5;
        //             reactor->pause(cur_time);

        //             if(servo_det_state)
        //             {
        //                 reactor->pause(get_monotonic() + timeout/1000.0);
        //                 break;
        //             }
        //         }

        //         gcode->run_script_from_command("SET_SERVO SERVO=printer_servo ANGLE=90");

        //         if(cur_time >=end_time)
        //         {
        //             SPDLOG_WARN("servo go home timeout!");
        //         }
        //         else
        //         {
        //             SPDLOG_INFO("servo go home success!");
        //         }
        //     }
        //     else
        //     {
        //         if(!servo_det_state)
        //         {
        //             SPDLOG_INFO("servo is already away from home!");
        //             return;
        //         }

        //         gcode->run_script_from_command("SET_SERVO SERVO=printer_servo ANGLE=" + std::to_string(90. - servo_dir * angle));

        //         double cur_time = get_monotonic();
        //         double end_time = cur_time + 3;
            
        //         while(cur_time < end_time)
        //         {
        //             cur_time = get_monotonic() + 0.5;
        //             reactor->pause(cur_time);

        //             if(!servo_det_state)
        //             {
        //                 reactor->pause(get_monotonic() + timeout/1000.0);
        //                 break;
        //             }
        //         }

        //         gcode->run_script_from_command("SET_SERVO SERVO=printer_servo ANGLE=90");

        //         if(cur_time >=end_time)
        //         {
        //             SPDLOG_WARN("servo away from home timeout!");
        //         }
        //         else
        //         {
        //             SPDLOG_INFO("servo away from home success!");
        //         }
        //     }

        // }

        void PrinterServo::CMD_set_servo(std::shared_ptr<GCodeCommand> gcmd)
        {
            double width = gcmd->get_double("WIDTH",DOUBLE_NONE);
            double value = DOUBLE_NONE;
            if(!std::isnan(width))
            {
                value = this->_get_pwm_from_pulse_width(width);
            }
            else
            {
                double angle = gcmd->get_double("ANGLE");
                value = this->_get_pwm_from_angle(angle);
            }
            this->gcrq->queue_gcode_request(value);
        }
        
        json PrinterServo::get_status(double eventtime)
        {
            json servo_status = {};
            servo_status["value"] = this->last_value;
            servo_status["mode"] = static_cast<int>(grill_state);
            servo_status["fan_enable"] = this->fan_enable;
            servo_status["start_percent"] = this->start_percent;
            return servo_status;
        }

        std::pair<std::string, double> PrinterServo::_set_pwm(double print_time, double value)
        {
            if(value == this->last_value)
            {
                return {"discard", 0.};
            }
            this->last_value = value;
            this->mcu_servo->set_pwm(print_time,value);
            return {"normal", 0.0};
        }

        double PrinterServo::_get_pwm_from_angle(double angle)
        {
            double angle_tmp = std::max(0.,std::min(this->max_angle,angle));
            double width = this->min_width + angle * this->angle_to_width;
            return width * this->width_to_value;
        }
        
        double PrinterServo::_get_pwm_from_pulse_width(double width)
        {
            if(width)
            {
                width = std::max(this->min_width,std::min(this->max_width,width));
            }
            return width * this->width_to_value;
        }

        std::shared_ptr<PrinterServo> servo_load_config_prefix(std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterServo>(config);
        }
    }

}
