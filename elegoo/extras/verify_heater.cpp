/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-04-23 11:33:34
 * @LastEditors  : loping
 * @LastEditTime : 2025-04-27 11:00:32
 * @Description  : Heater/sensor verification code
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "verify_heater.h"
#include "heaters.h"


namespace elegoo
{
    namespace extras
    {
        // #undef SPDLOG_DEBUG
        // #define SPDLOG_DEBUG SPDLOG_INFO
        
        static const std::string HINT_THERMAL = "\
        See the 'verify_heater' section in docs/Config_Reference.md\
        for the parameters that control this check.\
        ";
        HeaterCheck::HeaterCheck(std::shared_ptr<ConfigWrapper> config)
        {
            this->printer = config->get_printer();
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:connect",
                std::function<void()>([this](){
                    SPDLOG_DEBUG("HeaterCheck connect~~~~~~~~~~~~~~~~~");
                    handle_connect();
                    SPDLOG_DEBUG("HeaterCheck connect~~~~~~~~~~~~~~~~~ success!");
                })
            );
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:shutdown",
                std::function<void()>([this](){
                    SPDLOG_DEBUG("HeaterCheck shutdown~~~~~~~~~~~~~~~~~");
                    handle_shutdown();
                    SPDLOG_DEBUG("HeaterCheck shutdown~~~~~~~~~~~~~~~~~ success!");
                })
            );
            this->heater_name = config->get_name().substr(config->get_name().find_last_of(' ') + 1);
            this->heater = nullptr;
            this->hysteresis = config->getdouble("hysteresis",5.,0.);
            this->max_error = config->getdouble("max_error",120.,0.);
            this->heating_gain = config->getdouble("heating_gain",2.,DOUBLE_NONE,DOUBLE_NONE,0.);
            double default_gain_time = 20.;
            if(this->heater_name == "heater_bed")
            {
                default_gain_time = 60.;
            }
            this->check_gain_time = config->getdouble("check_gain_time",default_gain_time,1.);
            this->approaching_target = this->starting_approach = false;
            this->last_target = this->goal_temp = this->error = 0.;
            this->goal_systime = this->printer->get_reactor()->NEVER;
            this->check_timer = nullptr;
            this->is_verify_heater_fault = false;
            SPDLOG_DEBUG("__func__:{} #1",__func__);
        }

        HeaterCheck::~HeaterCheck()
        {
            SPDLOG_INFO("~HeaterCheck");
        }

        void HeaterCheck::handle_connect()
        {
            if(this->printer->get_start_args().find("debugoutput") != this->printer->get_start_args().end())
            {
                //Disable verify_heater if outputting to a debug file
                SPDLOG_INFO("Disable verify_heater if outputting to a debug file");
                return;
            }
            auto pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(this->printer->lookup_object("heaters",std::shared_ptr<PrinterHeaters>()));
            this->heater = pheaters->lookup_heater(this->heater_name);
            SPDLOG_INFO("Starting heater checks for {}", this->heater_name);
            auto reactor = this->printer->get_reactor();
            this->check_timer = reactor->register_timer(
                [this](double eventtime){
                    return this->check_event(eventtime);
                },reactor->NOW);
        }

        void HeaterCheck::handle_shutdown()
        {
            if(!this->check_timer)
            {
                auto reactor = this->printer->get_reactor();
                reactor->update_timer(this->check_timer,reactor->NEVER);
            }
        }

        double HeaterCheck::check_event(double eventtime)
        {
            std::pair<double, double> temp_pair = this->heater->get_temp(eventtime);
            double temp = temp_pair.first;
            double target = temp_pair.second;
            SPDLOG_DEBUG("heater_name:{},temp:{},target:{},last_target:{},hysteresis:{}",heater_name,temp,target,last_target,hysteresis);
            if(temp >= target - this->hysteresis || target <= 0.)
            {
                SPDLOG_DEBUG("heater_name:{},temp:{},target:{},last_target:{},hysteresis:{}",heater_name,temp,target,last_target,hysteresis);
                //Temperature near target - reset checks
                if(this->approaching_target && target)
                {
                    SPDLOG_INFO("Heater {} within range of {}",this->heater_name, target);
                }
                this->approaching_target = this->starting_approach = false;
                if(this->is_verify_heater_fault)
                {
                    this->is_verify_heater_fault = false;
                    SPDLOG_INFO("emit verify_heater:normal");
                    elegoo::common::SignalManager::get_instance().emit_signal("verify_heater:normal",this->heater_name);
                }
                if(temp <= target + this->hysteresis)
                {
                    this->error = 0.;
                }
                this->last_target = target;
                return eventtime + 1.;
            }
            this->error += (target - this->hysteresis) - temp;
            SPDLOG_DEBUG("heater_name:{},this->starting_approach:{},approaching_target:{},target:{},last_target:{},temp:{},goal_temp:{},error:{}",heater_name,this->starting_approach,approaching_target,target,last_target,temp,goal_temp,error);
            if(!this->approaching_target)
            {
                SPDLOG_DEBUG("heater_name:{},this->starting_approach:{},approaching_target:{},target:{},last_target:{},temp:{},goal_temp:{},error:{},max_error:{},is_verify_heater_fault:{}",heater_name,this->starting_approach,approaching_target,target,last_target,temp,goal_temp,error,this->max_error,is_verify_heater_fault);
                if(target != this->last_target)
                {
                    //Target changed - reset checks
                    SPDLOG_INFO("Heater {} approaching new target of {}",this->heater_name, target);
                    this->approaching_target = this->starting_approach = true;
                    this->goal_temp = temp + this->heating_gain;
                    this->goal_systime = eventtime + this->check_gain_time;
                }
                else if (this->error >= this->max_error)
                {
                    if(!this->is_verify_heater_fault)
                    {
                        SPDLOG_DEBUG("heater_name:{},approaching_target:{},is_verify_heater_fault:{},max_error:{},error:{}",heater_name,approaching_target,is_verify_heater_fault,max_error,error);
                        //Failure due to inability to maintain target temperature
                        return eventtime + this->heater_fault();
                    }
                    else
                    {
                        this->error = this->max_error;
                    }
                }
            }
            else if (temp >= this->goal_temp)
            {
                SPDLOG_DEBUG("heater_name:{},this->starting_approach:{},approaching_target:{},target:{},last_target:{},temp:{},heating_gain:{},goal_temp:{},error:{},eventtime:{},check_gain_time:{},goal_systime:{}",heater_name,this->starting_approach,approaching_target,target,last_target,temp,heating_gain,goal_temp,error,eventtime,check_gain_time,goal_systime);
                this->starting_approach = false;
                this->error = 0.;
                this->goal_temp = temp + this->heating_gain;
                this->goal_systime = eventtime + this->check_gain_time;
            }
            else if (eventtime >= this->goal_systime)
            {
                //Temperature approaching target - reset checks
                this->approaching_target = false;
                if(this->is_verify_heater_fault)
                {
                    this->is_verify_heater_fault = false;
                    SPDLOG_INFO("emit verify_heater:normal");
                    elegoo::common::SignalManager::get_instance().emit_signal("verify_heater:normal",this->heater_name);
                }
                SPDLOG_INFO("Heater {} no longer approaching target {}",this->heater_name, target);
            }
            else if (this->starting_approach)
            {
                this->goal_temp = std::min(this->goal_temp,temp + this->heating_gain);
            }
            this->last_target = target;
            return eventtime + 1.;
        }

        double HeaterCheck::heater_fault()
        {
            std::string msg = "Heater " + this->heater_name + " not heating at expected rate";
            SPDLOG_ERROR(msg);
            // this->printer->invoke_shutdown(msg + HINT_THERMAL);
            // return this->printer->get_reactor()->NEVER;
            this->is_verify_heater_fault = true;
            SPDLOG_INFO("emit verify_heater:fault");
            SPDLOG_INFO("heater_name:{}  is_verify_heater_fault:{}", this->heater_name, this->is_verify_heater_fault);
            elegoo::common::SignalManager::get_instance().emit_signal("verify_heater:fault",this->heater_name);
            return 2.;
        }

        std::shared_ptr<HeaterCheck> verify_heater_load_config_prefix(std::shared_ptr<ConfigWrapper> config)
        {
            SPDLOG_DEBUG("__func__:{} #1",__func__);
            return std::make_shared<HeaterCheck>(config);
        }
    }
}