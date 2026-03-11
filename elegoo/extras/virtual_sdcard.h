/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-14 17:51:15
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-16 17:38:13
 * @Description  : Virtual sdcard support (print files directly from a host
 * g-code file)
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>

#include "configfile.h"
#include "gcode_macro.h"
#include "gcode_move.h"
#include "json.h"
#include "mcu.h"
#include "print_stats.h"
#include "printer.h"

#define OP_LIBC 0

#pragma once

class PrintStats;

namespace elegoo
{
    namespace extras
    {

        class TemplateWrapper;
        class VirtualSD
        {
        public:
            VirtualSD(std::shared_ptr<ConfigWrapper> config);
            ~VirtualSD();

            void handle_ready();
            void handle_shutdown();
            void handle_power_outage();
            void canvas_power_outage_resume();
            std::pair<bool, std::string> stats(double eventtime);
            std::vector<std::pair<std::string, uint64_t>> get_file_list(
                bool check_subdirs = false);
            json get_status();
            std::string file_path();
            double progress();
            double work_handler(double eventtime);
            bool is_cmd_from_sd();
            bool is_active();
            int get_file_position();
            void do_pause();
            void do_resume();
            void do_cancel();
            void cmd_error(std::shared_ptr<GCodeCommand> gcmd);
            void _reset_file();
            void cmd_SDCARD_RESET_FILE(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_RESET_PRINT_TIME(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_SDCARD_PRINT_FILE(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_POWER_OUTAGE_RESUME(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_SET_POWER_OUTAGE_ENABLE(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_M20(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_M21(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_M23(std::shared_ptr<GCodeCommand> gcmd);
            void _load_file(std::shared_ptr<GCodeCommand> gcmd,
                            const std::string &filename, bool check_subdirs = false);
            void cmd_M24(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_M25(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_M26(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_M27(std::shared_ptr<GCodeCommand> gcmd);
            void set_file_position(uint64_t pos);
            bool file_exists(const std::string &filename);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<ConfigWrapper> config;
            std::shared_ptr<PrintStats> print_stats;
            std::shared_ptr<SelectReactor> reactor;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<ReactorTimer> work_timer;
            json power_outage_context;
            std::string sdcard_dirname;
            int power_outage_enable;
#if OP_LIBC
            FILE *current_file = NULL;
#else
            std::ifstream current_file;
#endif
            std::string current_file_name;
            std::string power_outage_file_name;
            uint64_t file_position, file_size;
            uint64_t next_file_position;
            bool must_pause_work, cmd_from_sd;
            bool is_over_work;
            std::shared_ptr<elegoo::extras::TemplateWrapper> on_error_gcode;
            std::shared_ptr<elegoo::extras::TemplateWrapper> print_start_gcode;
            std::shared_ptr<elegoo::extras::TemplateWrapper> move_to_waste_box;
            std::shared_ptr<elegoo::extras::TemplateWrapper> extrude_filament;
        };

        std::shared_ptr<VirtualSD> virtual_sdcard_load_config(std::shared_ptr<ConfigWrapper> config);

    }
}