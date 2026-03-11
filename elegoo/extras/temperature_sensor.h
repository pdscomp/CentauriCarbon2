/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-04-21 21:59:38
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-21 12:11:20
 * @Description  : Support generic temperature sensors
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef __TEMPERATURE_SENSOR_H__
#define __TEMPERATURE_SENSOR_H__

#include <string>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "printer.h"
#include "configfile.h"

namespace elegoo
{
    namespace extras
    {
        class Heater;

        class PrinterSensorGeneric : public Heater
        {
        public:
            PrinterSensorGeneric(std::shared_ptr<ConfigWrapper> config, std::shared_ptr<HeaterBase> sensor);

            ~PrinterSensorGeneric();
            void handle_connect();
            double temperature_callback(double read_time, double temp);
            std::pair<double, double> get_temp(double eventtime);
            std::pair<bool, std::string> stats(double eventtime);
            json get_status(double eventtime);

        private:
            std::shared_ptr<ConfigWrapper> config;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<PrinterHeaters> pheaters;
            std::shared_ptr<HeaterBase> sensor;
            std::string name;
            bool is_ot_box_report;
            double ot_box;
            double min_temp;
            double max_temp;
            double last_temp;
            double measured_min;
            double measured_max;
        };

        std::shared_ptr<PrinterSensorGeneric> ztemperature_sensor_load_config_prefix(std::shared_ptr<ConfigWrapper> config);
    }
}

#endif