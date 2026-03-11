/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-08-23 15:29:30
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-26 12:11:01
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "cavity_temperature.h"
#include "print_stats.h"
#include "temperature_sensor.h"

namespace elegoo
{
    namespace extras
    {
        #undef SPDLOG_DEBUG
        #define SPDLOG_DEBUG SPDLOG_INFO


        CavityTemperature::CavityTemperature(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("{} init __start",__func__);
            this->config = config;
            this->printer = config->get_printer();
            this->reactor = printer->get_reactor();
            this->name = elegoo::common::split(config->get_name()).back();
            this->mode = 0;
            this->last_fan_cmd = CONTROL_0;
            this->A = config->getdouble("A",1);
            this->B = config->getdouble("B",0.7);
            this->C = config->getdouble("C",0.3);
            this->D = config->getdouble("D",1);
            this->E = config->getdouble("E",0.3);
            this->T0 = config->getdouble("T0",30);
            this->T1 = config->getdouble("T1",38);
            this->T2 = config->getdouble("T2",37);
            
            this->gcode = any_cast<std::shared_ptr<GCodeDispatch>>(this->printer->lookup_object("gcode"));
            this->gcode->register_command(
                    "SET_CAVITY_TEMPERATURE_MODE"
                    ,[this](std::shared_ptr<GCodeCommand> gcmd){
                        CMD_set_cavity_temperature_mode(gcmd);
                    }
                    ,false
                    ,"SET CAVITY TEMPERATURE MODE");
            this->cavity_temperature_timer = reactor->register_timer(
                [this](double eventtime){ 
                    return cavity_temperature_callback(eventtime); 
                }
                , _NEVER, "cavity temperature timer"
            );
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this](){
                    handle_ready();
                })
            );
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:print_stats",
                std::function<void(std::string)>([this](std::string print_state) {
                    handle_print_state(print_state);
                })
            );
            SPDLOG_INFO("{} init __over",__func__);
        }
        
        CavityTemperature::~CavityTemperature()
        {
            SPDLOG_INFO("~CavityTemperature");
        }
        
        int32_t CavityTemperature::get_cavity_temperature_mode()
        {
            return this->mode;
        }

        void CavityTemperature::handle_ready()
        {
            box = any_cast<std::shared_ptr<PrinterSensorGeneric>>(printer->lookup_object("ztemperature_sensor box",std::shared_ptr<PrinterSensorGeneric>()));
            reactor->update_timer(cavity_temperature_timer,get_monotonic() + 5.);
            SPDLOG_INFO("{} __OVER",__func__);
        }

        void CavityTemperature::handle_print_state(std::string value)
        {
            print_state = value;
        }

        double CavityTemperature::cavity_temperature_callback(double eventtime)
        {
            if(print_state == "printing")
            {
                if(!mode)
                {
                    double cur_box_temp = box->get_temp(eventtime).first;
                    if(cur_box_temp >= T2)
                    {
                        reactor->register_callback([this](double eventtime) { 
                            if(last_fan_cmd != CONTROL_1)
                            {
                                gcode->run_script_from_command("M106 P3 S" + std::to_string(255. * D) + " W0");
                            }
                            
                            last_fan_cmd = CONTROL_1;
                            return json::object(); }
                        );
                    }
                    else
                    {
                        reactor->register_callback([this](double eventtime) { 
                            if(last_fan_cmd != CONTROL_2)
                            {
                                gcode->run_script_from_command("M106 P3 S" + std::to_string(255. * E) + " W0");
                            }
                            last_fan_cmd = CONTROL_2;
                            return json::object(); }
                        );
                    }
                }
                else
                {
                    reactor->register_callback([this](double eventtime) { 
                        if(last_fan_cmd != CONTROL_3)
                        {
                            gcode->run_script_from_command("M106 P3 S" + std::to_string(255. * E) + " W0");
                        }
                        last_fan_cmd = CONTROL_3;
                        return json::object(); }
                    );
                }
            }
            return eventtime + 10.;
        }

        void CavityTemperature::CMD_set_cavity_temperature_mode(std::shared_ptr<GCodeCommand> gcmd)
        {
            if(print_state == "printing")
            {
                SPDLOG_WARN("cur is printing,can not set cavity temperature mode!");
                return;
            }

            this->mode = gcmd->get_int("MODE",0,0,1);
            SPDLOG_INFO("{} mode:{}",__func__,mode);
            
            if(!mode)
            {
                gcode->run_script_from_command("EXHAUST_GRILLE_OPEN");
            }
            else
            {
                gcode->run_script_from_command("EXHAUST_GRILLE_CLOSE");
            }

            // gcode->run_script_from_command("M106 P3 S0");
        }

        void CavityTemperature::state_feedback(std::string command,std::string result)
        {
            json res;
            res["command"] = command;  
            res["result"] = result;  
            gcode->respond_feedback(res);
        }

        json CavityTemperature::get_status(double eventtime)
        {
            json status = {};
            status["A"] = this->A;
            status["B"] = this->B;
            status["C"] = this->C;
            status["T0"] = this->T0;
            status["T1"] = this->T1;
            return status;
        }

        std::shared_ptr<CavityTemperature> cavity_temperature_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_INFO("{}()",__func__);
            return std::make_shared<CavityTemperature>(config);
        }
    }
}