/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-04-21 21:59:38
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-21 14:30:23
 * @Description  : Support generic temperature sensors
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "temperature_sensor.h"

static double KELVIN_TO_CELSIUS = -273.15;

// #undef SPDLOG_DEBUG
// #define SPDLOG_DEBUG SPDLOG_INFO


namespace elegoo
{
    namespace extras
    {
        PrinterSensorGeneric::PrinterSensorGeneric(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<HeaterBase> sensor)
            : Heater(config, sensor), config(config), is_ot_box_report(false)
        {
            SPDLOG_INFO("__func__:{} #1",__func__);
            
            this->gcode = any_cast<std::shared_ptr<GCodeDispatch>>(config->get_printer()->lookup_object("gcode",std::shared_ptr<GCodeDispatch>()));
            this->name = config->get_name().substr(config->get_name().find_last_of(' ') + 1);
            this->ot_box = config->getdouble("ot_box",100.);
            this->min_temp = config->getdouble("min_temp",KELVIN_TO_CELSIUS,KELVIN_TO_CELSIUS);
            this->max_temp = config->getdouble("max_temp",99999999.9,DOUBLE_NONE,DOUBLE_NONE,this->min_temp);
            this->pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(config->get_printer()->lookup_object("heaters",std::shared_ptr<PrinterHeaters>()));
            this->sensor = sensor;
            this->sensor->setup_minmax(this->min_temp,this->max_temp);
            this->sensor->setup_callback(
                    [this](double read_time,double temp)
                    {
                        temperature_callback(read_time,temp);
                    });

            this->last_temp = 0.;
            this->measured_min = 99999999.;
            this->measured_max = 0.;

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:connect",
                std::function<void()>([this](){
                    SPDLOG_DEBUG("PrinterSensorGeneric connect~~~~~~~~~~~~~~~~~");
                    handle_connect();
                    SPDLOG_DEBUG("PrinterSensorGeneric connect~~~~~~~~~~~~~~~~~ success!");
                })
            );
        }

        PrinterSensorGeneric::~PrinterSensorGeneric()
        {
            SPDLOG_INFO("~PrinterSensorGeneric");
        }

        void PrinterSensorGeneric::handle_connect()
        {
            SPDLOG_INFO("__func__:{} #1",__func__);
            pheaters->register_sensor(config, shared_from_this());
        }

        double PrinterSensorGeneric::temperature_callback(double read_time,double temp)
        {
            SPDLOG_DEBUG("__func__:{} temp:{},last_temp:{}",__func__,temp,last_temp);
            this->last_temp = temp;
            if(temp)
            {
                this->measured_min = std::min(this->measured_min,temp);
                this->measured_max = std::max(this->measured_max,temp);
                if(temp > this->ot_box)
                {
                    if(false == is_ot_box_report)
                    {
                        is_ot_box_report = true;
                        SPDLOG_INFO("{} name:{} temp:{} ot_box:{} is_ot_box_report:{}",__func__,name,temp,ot_box,is_ot_box_report);
                        gcode->respond_ecode("OT_BOX", elegoo::common::ErrorCode::OT_BOX, 
                            elegoo::common::ErrorLevel::WARNING);
                    }
                }
                else
                {
                    is_ot_box_report = false;
                }
            }

            return 0;
        }

        std::pair<double, double> PrinterSensorGeneric::get_temp(double eventtime)
        {
            return std::make_pair(this->last_temp, 0.);
        }

        std::pair<bool, std::string> PrinterSensorGeneric::stats(double eventtime)
        {
            return {false, this->name + ": temp=" + std::to_string(this->last_temp)};
        }

        json PrinterSensorGeneric::get_status(double eventtime)
        {
            json status;
            status["temperature"] = std::round(this->last_temp*100)/100.;
            status["measured_min_temp"] = std::round(this->measured_min*100)/100.;
            status["measured_max_temp"] = std::round(this->measured_max*100)/100.;
            return status;
        }

        std::shared_ptr<PrinterSensorGeneric> ztemperature_sensor_load_config_prefix(std::shared_ptr<ConfigWrapper> config)
        {
            auto pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(config->get_printer()->lookup_object("heaters",std::shared_ptr<PrinterHeaters>()));
            auto sensor = pheaters->setup_sensor(config);
            SPDLOG_INFO("__func__:{} #1",__func__);
            return std::make_shared<PrinterSensorGeneric>(config,sensor);
        }
    }
}