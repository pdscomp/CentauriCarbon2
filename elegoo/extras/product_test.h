/*****************************************************************************
 * @Author       : loping
 * @Date         : 2025-06-10 10:34:06
 * @LastEditors  : loping
 * @LastEditTime : 2025-06-11 10:34:50
 * @Description  :
 * @Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef __PRODUCT_TEST_H__
#define __PRODUCT_TEST_H__

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
#include "mmu.h"
#include "servo.h"
#include "filament_motion_sensor.h"

namespace elegoo
{
    namespace extras
    {
        class ControllerFan;
        class PrinterHeaterFan;
        class BedMesh;
        class ResonanceTester;
        class PrinterHeaters;
        class Heater;
        class PrinterFan;
        class PrinterOutputPin;
        class AccelChip;
        class Lis2dw;
        class PrinterButtons;
        class SwitchSensor;
        class LoadCell;

        typedef enum
        {
            PRODUCT_TEST_EXTRUDER_TEMP = 0,
            PRODUCT_TEST_HEAD_BED_TEMP,
            PRODUCT_TEST_HEATER_FAN_RPM,
            PRODUCT_TEST_CONTROL_FAN_RPM,
            PRODUCT_TEST_RESONANCE_TESTER,
            PRODUCT_TEST_BED_MESH,
            PRODUCT_TEST_NONE,
        }PRODUCT_TEST_E;

        class ProductTest
        {
        public:
            ProductTest(std::shared_ptr<ConfigWrapper> config);
            ~ProductTest();
            void cmd_TURN_ON_OUTPUT_PIN(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_TURN_OFF_OUTPUT_PIN(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_TURN_ON_MOTOR(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_TURN_ON_LOAD_CELL(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_FILA_IN(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_FILA_OUT(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_BEEP(std::shared_ptr<GCodeCommand> gcmd);
            double callback(double eventtime);
            void product_test_feedback(std::string command,std::string result);
            double motor_reset_button_handler(double eventtime, bool state) ;
            double filament_runout_button_handler(double eventtime, bool state) ;
            void handle_shutdown();

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<SelectReactor> reactor;
            std::shared_ptr<PrinterHeaters> pheaters;
            std::map<std::string, std::shared_ptr<Heater>> heaters;
            std::shared_ptr<ControllerFan> controller_fan;
            std::shared_ptr<PrinterHeaterFan> heater_fan;
            std::shared_ptr<PrinterFan> fan;
            std::shared_ptr<PrinterButtons> buttons;
            bool motor_reset_pin_value;
            bool filament_runout_pin_value;
            std::shared_ptr<SwitchSensor> switch_sensor;
            std::shared_ptr<Lis2dw> lis2dw;
            std::shared_ptr<LoadCell> loadcell;
            std::shared_ptr<BedMesh> bed_mesh;
            std::shared_ptr<ResonanceTester> resonance_tester;
            std::shared_ptr<GCodeDispatch> gcode;
            std::shared_ptr<Canvas> canvas;
            std::shared_ptr<PrinterServo> servo;
            std::shared_ptr<EncoderSensor> encoder_sensor;

            std::string name;
            double start_detect_time;
            PRODUCT_TEST_E next_state;
            bool state_result[PRODUCT_TEST_NONE];
            int8_t rpm_retry_cnt[PRODUCT_TEST_NONE];
            bool product_test_ready;
            bool product_test_start;
            bool shutdown_status;
            std::shared_ptr<ReactorTimer> test_timer;
        };

        std::shared_ptr<ProductTest> zproduct_test_load_config(std::shared_ptr<ConfigWrapper> config);

    }
}

#endif