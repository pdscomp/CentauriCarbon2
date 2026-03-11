/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-20 13:51:35
 * @Description  : Heater handling on homing moves
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <memory>
#include <vector>
#include <string>
#include <map>

class ConfigWrapper;
class Printer;
class MCU_endstop;
class MCU_pins;
namespace elegoo
{
    namespace extras
    {

        class PrinterHeaters;
        class HomingMove;

        class HomingHeaters
        {
        public:
            HomingHeaters(std::shared_ptr<ConfigWrapper> config);
            ~HomingHeaters();

            void handle_connect();
            bool check_eligible(const std::vector<std::shared_ptr<MCU_pins>> &endstops);
            void handle_homing_move_begin(std::shared_ptr<HomingMove> hmove);
            void handle_homing_move_end(std::shared_ptr<HomingMove> hmove);

        private:
            std::shared_ptr<Printer> printer;
            std::vector<std::string> disable_heaters;
            std::vector<std::string> flaky_steppers;
            std::shared_ptr<PrinterHeaters> pheaters;
            std::map<std::string, double> target_save;
        };

        std::shared_ptr<HomingHeaters> homing_heaters_load_config_prefix(
            std::shared_ptr<ConfigWrapper> config);
    }
}
