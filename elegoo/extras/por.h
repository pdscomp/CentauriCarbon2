/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2024-11-07 14:24:31
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-20 16:37:57
 * @Description  : Power off recover
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <functional>
#include "msgproto.h"
#include "c_helper.h"
#include "json.h"
#include "pins.h"
#include "any.h"
#include "chipbase.h"

#define JITTER  0

class SelectReactor;
class ReactorTimer;
class ReactorCompletion;
class ConfigWrapper;
class Printer;
class MCU;
class CommandWrapper;
class MCU_host_digital_pin;

namespace elegoo
{
    namespace extras
    {
        class POR
        {
        public:
            POR(std::shared_ptr<ConfigWrapper> config);
            ~POR();
            void trig_shutdown();
            void cmd_POR_TRIG_SHUTDOWN(std::shared_ptr<GCodeCommand> gcmd);
            void handle_mcu_shutdown();
        private:
            void build_config(std::shared_ptr<MCU> mcu);

        private:
            // sensor
            std::shared_ptr<MCU_host_digital_pin> host_sensor_pin;
            std::shared_ptr<MCU_host_digital_pin> host_ctrl_pin;
            std::shared_ptr<MCU_host_digital_pin> host_charge_pin;
            double next_charge_time;
            int charge_done;

            std::string sensor_pin;
            int sensor_pullup;
            int sensor_invert;
            std::shared_ptr<ReactorTimer> query_timer;
            std::shared_ptr<SelectReactor> reactor;
            // check time
            double rest_time;
            double sample_time;
            int sample_count;
            bool shutdown;
#if JITTER
            int jitter_times;
            int jitter_counter;
#endif  
            // shutdown
            std::vector<std::pair<std::shared_ptr<MCU_host_digital_pin>, int>> host_shutdown_pins;
            std::vector<std::tuple<std::shared_ptr<MCU>, std::string, int>> shutdown_pins;
            std::set<std::shared_ptr<MCU>> mcus;
            std::map<std::shared_ptr<MCU>, uint32_t> oids;
            std::map<std::shared_ptr<MCU>, std::shared_ptr<CommandWrapper>> set_cmds;
        };

        std::shared_ptr<POR> por_load_config(std::shared_ptr<ConfigWrapper> config);
    }
}