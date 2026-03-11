/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-08-23 15:29:24
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-26 17:22:05
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/


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
        class PrintStats;
        class PrinterSensorGeneric;
        
        enum FanControlType{
            CONTROL_0,
            CONTROL_1,
            CONTROL_2,
            CONTROL_3,
        };

        class CavityTemperature
        {
        public:
            CavityTemperature(std::shared_ptr<ConfigWrapper> config);
            ~CavityTemperature();
            json get_status(double eventtime);
            int32_t get_cavity_temperature_mode();
        private:
            void handle_ready();
            void handle_print_state(std::string value);
            double cavity_temperature_callback(double eventtime);
            void CMD_set_cavity_temperature_mode(std::shared_ptr<GCodeCommand> gcmd);
            void state_feedback(std::string command,std::string result);
            //
            std::shared_ptr<ConfigWrapper> config;
            std::shared_ptr<Printer> printer;
            std::shared_ptr<SelectReactor> reactor;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<PrintStats> print_stats;
            std::shared_ptr<PrinterSensorGeneric> box;
            std::shared_ptr<ReactorTimer> cavity_temperature_timer;
            std::string name;
            std::string print_state;
            FanControlType last_fan_cmd;
            bool mode;    // 0 冷却模式 1 腔温保持模式
            double A;
            double B;
            double C;
            double D;
            double E;
            double T0;
            double T1;
            double T2;
        };
        

        std::shared_ptr<CavityTemperature> cavity_temperature_load_config(std::shared_ptr<ConfigWrapper> config);
    }
}
