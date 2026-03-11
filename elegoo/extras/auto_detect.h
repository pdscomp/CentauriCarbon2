/*****************************************************************************
 * @Author       : Loping
 * @Date         : 2025-03-10 11:03:36
 * @LastEditors  : loping
 * @LastEditTime : 2025-04-23 15:10:49
 * @Description  : auto detect
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef __AUTO_DETECT_H__
#define __AUTO_DETECT_H__

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
        class ControllerFan;
        class PrinterHeaterFan;
        class BedMesh;
        class ResonanceTester;

        typedef enum
        {
            EXTRUDER_TEMP = 0,
            HEAD_BED_TEMP,
            HEATER_FAN_RPM,
            CONTROL_FAN_RPM,
            RESONANCE_TESTER,
            BED_MESH,
            AUTO_DETECT_NONE,
        }AUTO_DETECT_E;

        class AutoDetect
        {
        public:
            AutoDetect(std::shared_ptr<ConfigWrapper> config);
            ~AutoDetect();
            void cmd_AUTO_DETECT(std::shared_ptr<GCodeCommand> gcmd);
            double callback(double eventtime);
            void extruder_temp_feedback(double detect_time);
            void head_bed_temp_feedback(double detect_time);
            void heater_fan_rpm_feedback(double detect_time);
            void control_fan_rpm_feedback(double detect_time);
            void resonance_tester_feedback(double detect_time);
            void bed_mesh_feedback(double detect_time);
            void start_resonance();
            void start_bed_mesh();

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<SelectReactor> reactor;
            std::shared_ptr<PrinterHeaters> pheaters;
            std::map<std::string, std::shared_ptr<Heater>> heaters;
            std::shared_ptr<ControllerFan> controller_fan;
            std::shared_ptr<PrinterHeaterFan> heater_fan;
            std::shared_ptr<BedMesh> bed_mesh;
            std::shared_ptr<ResonanceTester> resonance_tester;
            std::shared_ptr<GCodeDispatch> gcode;
            std::string name;
            double start_detect_time;
            AUTO_DETECT_E next_state;
            bool state_result[AUTO_DETECT_NONE];
            int8_t rpm_retry_cnt[AUTO_DETECT_NONE];
            bool is_auto_detect;
        };

        std::shared_ptr<AutoDetect> rfauto_detect_load_config(std::shared_ptr<ConfigWrapper> config);
    }
}

#endif