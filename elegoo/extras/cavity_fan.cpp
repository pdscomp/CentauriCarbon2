/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-09-19 20:19:05
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-29 15:20:43
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "cavity_fan.h"
#include "fan.h"
#include "printer.h"
#include "temperature_sensor.h"


namespace elegoo
{
    namespace extras
    {
        const double PIN_MIN_TIME = 0.100;

        PrinterFanCavity::PrinterFanCavity(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("PrinterFanCavity init!");
            printer = config->get_printer();
            reactor = printer->get_reactor();
            mode = false;

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this]()
                { 
                    handle_ready(); 
                })
            );

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:print_stats",
                std::function<void(std::string)>([this](std::string print_state) 
                {
                    handle_print_state(print_state);
                })
            );

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:cavity_mode",
                std::function<void(bool)>([this](bool value) 
                {
                    handle_cavity_mode(value);
                })
            );

                    
            A = config->getdouble("A",1);
            B = config->getdouble("B",0.7);
            C = config->getdouble("C",0.3);
            D = config->getdouble("D",1);
            E = config->getdouble("E",0.3);
            F = config->getdouble("F",0.4);
            G = config->getdouble("G",0.5);
            T0 = config->getdouble("T0",30);
            T1 = config->getdouble("T1",38);
            T2 = config->getdouble("T2",37);
            T3 = config->getdouble("T3",40);
            T4 = config->getdouble("T4",45);

            fan = std::make_shared<Fan>(config, 0);
            last_speed = 0;
            SPDLOG_INFO("PrinterFanCavity init success!!");
        }

        PrinterFanCavity::~PrinterFanCavity() {}

        void PrinterFanCavity::handle_ready()
        {

            std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            gcode->register_command("SET_CAVITY_FAN",
                [this](std::shared_ptr<GCodeCommand> gcmd)
                {
                    cmd_SET_CAVITY_FAN(gcmd);
                }
            );

            fan_timer = reactor->register_timer(
                [this](double eventtime)
                { 
                    return callback(eventtime); 
                },
                _NEVER, "cavity_fan"
            );

            box = any_cast<std::shared_ptr<PrinterSensorGeneric>>(
                printer->lookup_object("ztemperature_sensor box",std::shared_ptr<PrinterSensorGeneric>()));
            last_box_temp = cur_box_temp = box->get_temp(0).first;
        }

        void PrinterFanCavity:: handle_print_state(std::string value)
        {
            if(value == "printing")
            {
                reactor->update_timer(fan_timer, _NOW);
            }
            else
            {
                reactor->update_timer(fan_timer, _NEVER);
            }
        }

        void PrinterFanCavity::handle_cavity_mode(bool value)
        {
            mode = value;
        }

        json PrinterFanCavity::get_status(double eventtime)
        {
            json status = {};
            status = fan->get_status(eventtime);
            status["A"] = this->A;
            status["B"] = this->B;
            status["C"] = this->C;
            status["T0"] = this->T0;
            status["T1"] = this->T1;
            return status;
        }

        double PrinterFanCavity::callback(double eventtime)
        {
            double speed = 0.0;

            if(!mode)
            {
                double diff_temp = 0;
                cur_box_temp = box->get_temp(eventtime).first;
                if(cur_box_temp < last_box_temp)
                {
                    diff_temp = 2;
                }

                if(cur_box_temp < T2 - diff_temp)
                {
                    speed = E;
                }
                else if(cur_box_temp >= T2 && cur_box_temp < T3 - diff_temp)
                {
                    speed = D;
                }
                else if(cur_box_temp >= T3 && cur_box_temp < T4 - diff_temp)
                {
                    speed = F;
                }
                else if(cur_box_temp >= T4)
                {
                    speed = G;
                }

                last_box_temp = cur_box_temp;
            }
            else
            {
                speed = E;
            }


            if (speed != last_speed)
            {
                last_speed = speed;
                fan->set_speed(speed);
            }

            return eventtime + 10;
        }

        void PrinterFanCavity::cmd_SET_CAVITY_FAN(std::shared_ptr<GCodeCommand> gcmd)
        {
            double speed = gcmd->get_double("SPEED", 0, 0) / 255.0;
            fan->set_speed(speed);
            SPDLOG_INFO("set cavity fan: {}", speed);
        }

        std::shared_ptr<PrinterFanCavity> cavity_fan_load_config(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterFanCavity>(config);
        }

    }
}
