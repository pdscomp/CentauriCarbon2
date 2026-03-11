/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-16 10:37:07
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-12 14:20:48
 * @Description  : resonance_tester is a functional module in the Elegoo
 * firmware used to detect and analyze resonance phenomena in the printer at
 * different frequencies. Resonance is a phenomenon where mechanical systems
 * experience amplified vibrations at specific frequencies, which can lead to
 * decreased print quality, surface ripples, or ringing issues in 3D printers.
 * By using resonance_tester, users can identify these resonance frequencies
 * and take measures (such as adjusting input shaper parameters) to mitigate
 * or eliminate these problems.
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "lis2dw.h"
#include "adxl345.h"

class ConfigWrapper;
class Printer;
class InputShaper;
class GCodeDispatch;
class GCodeCommand;
class TemplateWrapper;


namespace elegoo {
namespace extras {
class ShaperCalibrate;
class CalibrationData;
class CalibrationResult;
class AccelChip;
class AccelQueryHelper;
class TestAxis
{
public:
    TestAxis(const std::string& axis = "",
        const std::vector<double>& vd = {});
    ~TestAxis();

    bool matches(const std::string& chip_axis);
    std::string get_name();
    std::pair<double, double> get_point(double l);

private:
    std::string name;
    std::vector<double> vib_dir;
};

std::shared_ptr<TestAxis> parse_axis(
    std::shared_ptr<GCodeCommand> gcmd,
    std::string raw_axis);

class VibrationPulseTest
{
public:
    VibrationPulseTest(std::shared_ptr<ConfigWrapper> config);
    ~VibrationPulseTest();

    std::vector<std::vector<double>> get_start_test_points();
    void prepare_test(std::shared_ptr<GCodeCommand> gcmd);
    void run_test(std::shared_ptr<TestAxis> axis,
        std::shared_ptr<GCodeCommand> gcmd);
    double get_max_freq();

private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<GCodeDispatch> gcode;
    double min_freq;
    double max_freq;
    double accel_per_hz;
    double hz_per_sec;
    double freq_start;
    double freq_end;
    std::vector<std::vector<double>> probe_points;


};

class ResonanceTester
{
public:
    ResonanceTester(std::shared_ptr<ConfigWrapper> config);
    ~ResonanceTester();

    void connect();
    void cmd_TEST_RESONANCES(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SHAPER_CALIBRATE(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_MEASURE_AXES_NOISE(std::shared_ptr<GCodeCommand> gcmd);
    json get_status(double eventtime);
    bool is_valid_name_suffix(const std::string& name_suffix);
    std::string get_filename(const std::string& base, const std::string& name_suffix,
        std::shared_ptr<TestAxis> axis = nullptr, const std::vector<double>& point={},
        std::string chip_name = "");
    std::string save_calibration_data(const std::string& base_name, const std::string& name_suffix,
        std::shared_ptr<ShaperCalibrate> shaper_calibrate,
        std::shared_ptr<TestAxis> axis,
        std::shared_ptr<CalibrationData> calibration_data,
        std::vector<CalibrationResult> all_shapers = {},
        const std::vector<double>& point = {},
        double max_freq=0);

private:
    std::map<std::shared_ptr<TestAxis>, std::shared_ptr<CalibrationData>>
        run_test(std::shared_ptr<GCodeCommand> gcmd,
            std::vector<std::shared_ptr<TestAxis>> axes,
            std::shared_ptr<ShaperCalibrate> helper,
            const std::string& raw_name_suffix="",
            std::vector<std::shared_ptr<AccelChip>> accel_chips={},
            const std::vector<double>& test_point={});
    std::vector<std::shared_ptr<AccelChip>> parse_chips(const std::string& accel_chips);
    double get_max_calibration_freq();
    std::string get_current_time_string();
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<VibrationPulseTest> test;
    std::vector<std::pair<std::string, std::string>> accel_chip_names;
    std::shared_ptr<GCodeDispatch> gcode;
    std::vector<std::pair<std::string, std::shared_ptr<AccelChip>>> accel_chips;
    double move_speed;
    double max_smoothing;
    json status;
    std::shared_ptr<TemplateWrapper> shaper_calibrate_gcode;
};

std::shared_ptr<ResonanceTester> resonance_tester_load_config(
    std::shared_ptr<ConfigWrapper> config);

}
}