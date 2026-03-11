/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-22 12:04:19
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 15:01:11
 * @Description  : Automatic calibration of input shapers
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <complex>
#include "fftw3.h"
#include "configfile.h"
#include "configfile.h"
#include "gcode.h"
#include "input_shaper.h"
#include "mcu.h"
#include "printer.h"
#include "shaper_defs.h"
#include "adxl345.h"

class Printer;
namespace elegoo {
namespace extras {
class AccelQueryHelper;

struct CalibrationResult {
    std::string name;
    double freq;
    std::vector<double> vals;
    double vibrs;
    double smoothing;
    double score;
    double max_accel;
};

class CalibrationData {
public:
    CalibrationData(std::vector<double> freq_bins, 
        std::vector<double> psd_sum,
        std::vector<double> psd_x, 
        std::vector<double> psd_y,
        std::vector<double> psd_z);

    ~CalibrationData();

    void add_data(std::shared_ptr<CalibrationData> other);

    void normalize_to_frequencies();

    const std::vector<double>& get_psd(const std::string& axis) const;

    std::vector<double> freq_bins;
    std::vector<double> psd_x;
    std::vector<double> psd_y;
    std::vector<double> psd_z;
    std::vector<double> psd_sum;

private:
    std::vector<std::vector<double>*> _psd_list;
    std::map<std::string, std::vector<double>> _psd_map;
    size_t data_sets;
};

class ShaperCalibrate {
public:
    ShaperCalibrate(std::shared_ptr<Printer> printer);
    ~ShaperCalibrate();

    std::shared_ptr<CalibrationData> background_process_exec(
        std::function<std::shared_ptr<CalibrationData>(
            std::shared_ptr<AccelQueryHelper>)> method, 
        std::shared_ptr<AccelQueryHelper> data);

    CalibrationResult background_process_exec(
        std::function<CalibrationResult(
        InputShaperCfg shaper_cfg, 
        std::shared_ptr<CalibrationData> calibration_data,
        std::vector<double> shaper_freqs, 
        double damping_ratio, 
        double scv,
        double max_smoothing, 
        std::vector<double> test_damping_ratios,
        double max_freq)> method, 
        InputShaperCfg shaper_cfg, 
        std::shared_ptr<CalibrationData> calibration_data,
        std::vector<double> shaper_freqs, 
        double damping_ratio, 
        double scv,
        double max_smoothing, 
        std::vector<double> test_damping_ratios,
        double max_freq);

    std::shared_ptr<CalibrationData> calc_freq_response(
        std::shared_ptr<AccelQueryHelper> raw_values);

    std::shared_ptr<CalibrationData> process_accelerometer_data(
        std::shared_ptr<AccelQueryHelper> data);

    double find_shaper_max_accel(
        const std::pair<std::vector<double>, std::vector<double>>& shaper,
        double scv);

    std::pair<CalibrationResult, std::vector<CalibrationResult>>
        find_best_shaper(std::shared_ptr<CalibrationData> calibration_data,
            std::vector<std::string> shapers = {},
            double damping_ratio = DOUBLE_NONE, double scv = DOUBLE_NONE,
            std::vector<double> shaper_freqs = {},
            double max_smoothing = DOUBLE_NONE,
            std::vector<double> test_damping_ratios = {},
            double max_freq = DOUBLE_NONE);

    void save_params(std::shared_ptr<PrinterConfig> configfile, std::string axis,
                    const std::string& shaper_name,
                    const std::string& shaper_freq);

    void apply_params(std::shared_ptr<InputShaper> input_shaper, std::string axis,
                        const std::string& shaper_name,
                        const std::string& shaper_freq);

    CalibrationResult fit_shaper(
        InputShaperCfg shaper_cfg, 
        std::shared_ptr<CalibrationData> calibration_data,
        std::vector<double> shaper_freqs, 
        double damping_ratio, 
        double scv,
        double max_smoothing, 
        std::vector<double> test_damping_ratios,
        double max_freq);

    void save_calibration_data(
        const std::string& output,
        std::shared_ptr<CalibrationData> calibration_data,
        std::vector<CalibrationResult> shapers = {},
        double max_freq = DOUBLE_NONE);

private:
    std::vector<std::vector<double>> _split_into_windows(
        const std::vector<double>& x, int window_size, int overlap);

    std::pair<std::vector<double>, std::vector<double>> _psd(
        const std::vector<double>& x, double fs, int nfft);

    std::vector<double> _estimate_shaper(
        const std::pair<std::vector<double>, std::vector<double>>& shaper,
        double test_damping_ratio, const std::vector<double>& test_freqs);

    std::pair<double, std::vector<double>> _estimate_remaining_vibrations(
        const std::pair<std::vector<double>, std::vector<double>>& shaper,
        double test_damping_ratio, const std::vector<double>& freq_bins,
        const std::vector<double>& psd);

    double _get_shaper_smoothing(
        const std::pair<std::vector<double>, 
        std::vector<double>>& shaper,
        double accel = 5000, double scv = 5.0);

    double _bisect(std::function<bool(double)> func);

private:
    std::shared_ptr<Printer> printer;
};

}
}