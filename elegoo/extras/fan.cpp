/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-07 16:07:17
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-27 16:14:45
 * @Description  : Printer cooling fan
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "fan.h"
#include "utilities.h"
#include "print_stats.h"
#include "fan_generic.h"
namespace elegoo
{
    namespace extras
    {
        Fan::Fan(std::shared_ptr<ConfigWrapper> config, double default_shutdown_speed)
        {
            SPDLOG_DEBUG("Fan init !");
            printer = config->get_printer();
            auto pos = config->get_name().find_last_of(' ');
            if (pos != std::string::npos) 
            {
                fan_name = config->get_name().substr(config->get_name().find_last_of(' ') + 1);
            }
            else
            {
                fan_name = config->get_name();
            }

            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            last_fan_value = 0.0;
            last_req_value = 0.0;
            cur_error = 0;
            max_error = 6;
            is_report = false;

            max_power = config->getdouble("max_power", 1, DOUBLE_NONE, 1, 0);
            kick_start_time = config->getdouble("kick_start_time", 0.1, 0);
            off_below = config->getdouble("off_below", 0, 0, 1);
            cycle_time = config->getdouble("cycle_time", 0.01, DOUBLE_NONE, DOUBLE_NONE, 0);
            hardware_pwm = config->getboolean("hardware_pwm", BoolValue::BOOL_FALSE);
            shutdown_speed = config->getdouble("shutdown_speed", default_shutdown_speed, 0, 1);

            auto ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
            std::string str_pin = config->get("pin");
            auto mcu_fan_ptr = ppins->setup_pin("pwm", str_pin);
            SPDLOG_INFO("__func__:{} #1 str_pin:{}",__func__,str_pin);

            mcu_fan = std::static_pointer_cast<MCU_pwm>(mcu_fan_ptr);
            mcu_fan->setup_max_duration(0.0);
            mcu_fan->setup_cycle_time(cycle_time, hardware_pwm);

            auto shutdown_power =
                ((max_power > shutdown_speed) ? shutdown_speed : max_power) > 0.0
                    ? ((max_power > shutdown_speed) ? shutdown_speed : max_power)
                    : 0.0;
            mcu_fan->setup_start_value(0.0, shutdown_power);

            std::string enable_pin_str = config->get("enable_pin", "");
            if (enable_pin_str != "")
            {
                auto enable_pin_ptr = ppins->setup_pin("digital_out", enable_pin_str);
                enable_pin = std::static_pointer_cast<MCU_digital_out>(enable_pin_ptr);
                enable_pin->setup_max_duration(0.0);
            }

            gcrq = std::make_shared<GCodeRequestQueue>(
                config, this->mcu_fan->get_mcu(),
                std::bind(&Fan::_apply_speed, this, std::placeholders::_1,
                          std::placeholders::_2));

            tachometer = std::make_shared<FanTachometer>(config);

            reactor = this->printer->get_reactor();
            check_timer = reactor->register_timer(
                [this](double eventtime){
                    return this->check_fan(eventtime);
                }
            );
            
            elegoo::common::SignalManager::get_instance().register_signal(
                "gcode:request_restart",
                std::function<void(double)>([this](double print_time)
                                            { set_speed(0.0, print_time); }));
            SPDLOG_DEBUG("Fan init success!");
        }

        std::pair<std::string, double> Fan::_apply_speed(double print_time,
                                                         double value)
        {
            if (value < off_below)
            {
                value = 0.0;
            }

            value = ((max_power < value * max_power) ? max_power : value * max_power) > 0.0
                    ? ((max_power < value * max_power) ? max_power : value * max_power) : 0.0;

            if (value==last_fan_value)
            {
                return {"discard", 0.0};
            }

            if (enable_pin)
            {
                if (value > 0 && (0 == last_fan_value))
                {
                    enable_pin->set_digital(print_time, 1);
                }
                else if ((0 == value) && last_fan_value > 0)
                {
                    enable_pin->set_digital(print_time, 0);
                }
            }

            if (value && kick_start_time && (!last_fan_value || value - last_fan_value > 0.5))
            {
                last_req_value = value;
                last_fan_value = max_power;
                mcu_fan->set_pwm(print_time, max_power); 
                return {"delay", kick_start_time};
            }

            last_fan_value = last_req_value = value;
            mcu_fan->set_pwm(print_time, value);
            return {"", 0.0};
        }

        void Fan::set_speed(double value, double print_time)
        {
            gcrq->send_async_request(value, print_time);
            if(value > 0) 
            {
                reactor->update_timer(check_timer, _NOW);
                cur_error = 0;
                is_report = false;
            }
            else
            {
                reactor->update_timer(check_timer, _NEVER);
            }
        }

        void Fan::set_speed_from_command(double value)
        {
            gcrq->queue_gcode_request(value);
            if(value > 0) 
            {
                reactor->update_timer(check_timer, _NOW);
                cur_error = 0;
                is_report = false;
            }
            else
            {
                reactor->update_timer(check_timer, _NEVER);
            }
        }

        double Fan::check_fan(double eventtime)
        {
            if (printer->is_shutdown())
            {
                return _NEVER;
            }    

            double rpm = tachometer->get_status(eventtime)["rpm"];
            if((last_req_value > 0 && rpm > 0) || (last_req_value == 0 && rpm == 0)) 
            {
                cur_error = 0;
                is_report = false;
            }
            else
            {
                cur_error++;
            }

            if(cur_error > max_error)
            {
                if(!is_report)
                {
                    SPDLOG_INFO("last_req_value {}  rpm {}  name {} ",last_req_value, rpm, fan_name);
                    if(fan_name == "fan")
                    {
                        gcode->respond_ecode("", elegoo::common::ErrorCode::FAN_MODEL, 
                            elegoo::common::ErrorLevel::WARNING);
                    }
                    else if(fan_name == "heatbreak_cooling_fan")
                    {
                        gcode->respond_ecode("", elegoo::common::ErrorCode::FAN_THROAT,
                            elegoo::common::ErrorLevel::WARNING);
                        
                        std::shared_ptr<PrintStats> print_stats = 
                            any_cast<std::shared_ptr<PrintStats>>(printer->lookup_object("print_stats", std::shared_ptr<PrintStats>()));
              
                        if(print_stats && print_stats->get_status(get_monotonic())["state"] == "printing") 
                        {
                            std::shared_ptr<GCodeDispatch> gcode = 
                                any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
                            SPDLOG_INFO("gcode->run_script PAUSE {} {}",fan_name, is_report);
                            gcode->run_script("PAUSE\n");
                        }
                    }
                    else if(fan_name == "board_cooling_fan")
                    {
                        gcode->respond_ecode("", elegoo::common::ErrorCode::FAN_BOARD,
                            elegoo::common::ErrorLevel::WARNING);
                    }
                    else if(fan_name == "cavity_fan")
                    {
                        gcode->respond_ecode("", elegoo::common::ErrorCode::FAN_BOX_FAN,
                            elegoo::common::ErrorLevel::WARNING);
                    }
                    else if(fan_name == "fan1")
                    {
                        gcode->respond_ecode("", elegoo::common::ErrorCode::FAN_FAN1,
                            elegoo::common::ErrorLevel::WARNING);
                    }
                    is_report = true;
                }

                cur_error = max_error;
            }

            return eventtime + 5.0;
        }

        void Fan::_handle_request_restart(double print_time)
        {
            set_speed(0.0, print_time);
        }

        std::shared_ptr<MCU> Fan::get_mcu() const { return this->mcu_fan->get_mcu(); }

        json Fan::get_status(double eventtime)
        {
            json status;
            status["speed"] = last_req_value;
            status["rpm"] = tachometer->get_status(eventtime)["rpm"];
            return status;
        }

        FanTachometer::FanTachometer(std::shared_ptr<ConfigWrapper> config)
            : freq_counter(nullptr)
        {
            SPDLOG_DEBUG("FanTachometer init!");
            std::string pin = config->get("tachometer_pin", "");
            if (pin != "")
            {
                ppr = config->getint("tachometer_ppr", 2, 1);

                double sample_time = 1.0;
                double poll_time = config->getdouble("tachometer_poll_interval", 0.0015, DOUBLE_NONE, DOUBLE_NONE, 0);
                freq_counter = std::make_shared<FrequencyCounter>(config->get_printer(), pin, sample_time, poll_time);
            }
            SPDLOG_DEBUG("FanTachometer init success!!");
        }

        json FanTachometer::get_status(double eventtime)
        {
            json status;
            if (freq_counter)
            {
                double rpm = freq_counter->get_frequency() * 30.0 / ppr;
                status["rpm"] = rpm;
            }
            else
            {
                status["rpm"] = 0.0;
            }
            return status;
        }

        PrinterFan::PrinterFan(std::shared_ptr<ConfigWrapper> config)
            : fan(std::make_shared<Fan>(config))
        {
            SPDLOG_DEBUG("PrinterFan init!");
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(config->get_printer()->lookup_object("gcode"));
            gcode->register_command("M106",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_M106(gcmd);
                                    });

            gcode->register_command("M107",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_M107(gcmd);
                                    });
            SPDLOG_DEBUG("PrinterFan init ok!!");
        }

        json PrinterFan::get_status(double eventtime)
        {
            return fan->get_status(eventtime);
        }

        void PrinterFan::cmd_M106(std::shared_ptr<GCodeCommand> gcmd)
        {
            //P0 散热风扇
            //P2 辅助散热风扇
            //P3 空气循环
            double value = gcmd->get_double("S", 255, 0) / 255.0;
            int fan_index = gcmd->get_double("P", 0);
            SPDLOG_DEBUG("__func__:{} #1 value:{},fan_index:{}",__func__,value,fan_index);
            if(0 == fan_index)
                fan->set_speed(value);
            else if (2 == fan_index)
            {
                gcode->run_script_from_command("SET_FAN_SPEED FAN=fan1 SPEED=" + std::to_string(value));
            }
            // else if (3 == fan_index)
            // {  
            //     gcode->run_script_from_command("SET_CAVITY_FAN SPEED=" + std::to_string(value));   
            // }
        }

        void PrinterFan::cmd_M107(std::shared_ptr<GCodeCommand> gcmd)
        {
            fan->set_speed_from_command(0.0);
        }

        std::shared_ptr<PrinterFan> fan_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterFan>(config);
        }

    }
}