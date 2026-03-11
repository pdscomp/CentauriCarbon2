/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-22 12:04:19
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 15:01:38
 * @Description  : Automatic calibration of input shapers
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "shaper_calibrate.h"

namespace elegoo {
namespace extras {
const double MIN_FREQ = 5.0;
const double MAX_FREQ = 200.0;
const double WINDOW_T_SEC = 0.5;
const double MAX_SHAPER_FREQ = 150.0;
const std::vector<double> TEST_DAMPING_RATIOS = {0.075, 0.1, 0.15};
const std::vector<std::string> AUTOTUNE_SHAPERS = 
    {"zv", "mzv", "ei", "2hump_ei", "3hump_ei"};
static std::vector<InputShaperCfg> INPUT_SHAPERS = {
    InputShaperCfg("zv", get_zv_shaper, 21.0),
    InputShaperCfg("mzv", get_mzv_shaper, 23.0),
    InputShaperCfg("zvd", get_zvd_shaper, 29.0),
    InputShaperCfg("ei", get_ei_shaper, 29.0),
    InputShaperCfg("2hump_ei", get_2hump_ei_shaper, 39.0),
    InputShaperCfg("3hump_ei", get_3hump_ei_shaper, 48.0),
};

template <typename T>
std::vector<T> interp(const std::vector<T>& x, 
    const std::vector<T>& xp,
    const std::vector<T>& fp) {

    if (xp.size() != fp.size()) {
        throw std::invalid_argument("xp and fp must have the same size");
    }

    std::vector<T> result(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        if (x[i] <= xp.front()) {
            result[i] = fp.front();
        } else if (x[i] >= xp.back()) {
            result[i] = fp.back();
        } else {
            auto it = std::lower_bound(xp.begin(), xp.end(), x[i]);
            size_t idx = std::distance(xp.begin(), it) - 1;
            T x1 = xp[idx];
            T x2 = xp[idx + 1];
            T y1 = fp[idx];
            T y2 = fp[idx + 1];
            result[i] = y1 + (y2 - y1) * (x[i] - x1) / (x2 - x1);
        }
    }

    return result;
}

CalibrationData::CalibrationData(std::vector<double> freq_bins,
    std::vector<double> psd_sum,
    std::vector<double> psd_x,
    std::vector<double> psd_y,
    std::vector<double> psd_z) {

    this->freq_bins = freq_bins;
    this->psd_sum = psd_sum;
    this->psd_x = psd_x;
    this->psd_y = psd_y;
    this->psd_z = psd_z;
    this->_psd_list = {&this->psd_sum, &this->psd_x, &this->psd_y, &this->psd_z};
    this->_psd_map["x"] = this->psd_x;
    this->_psd_map["y"] = this->psd_y;
    this->_psd_map["z"] = this->psd_z;
    this->_psd_map["all"] = this->psd_sum;
    this->data_sets = 1;
}


CalibrationData::~CalibrationData() {}

void CalibrationData::add_data(std::shared_ptr<CalibrationData> other) {
    auto joined_data_sets = this->data_sets + other->data_sets;
    std::vector<double> other_normalized;

    int min_size = (this->_psd_list.size() > other->_psd_list.size()) ? 
        other->_psd_list.size() : this->_psd_list.size();

    for (int i = 0; i < min_size; i++) {
        other_normalized = interp<double>(freq_bins, other->freq_bins, (*other->_psd_list[i]));

        std::transform((*_psd_list[i]).begin(), (*_psd_list[i]).end(), (*_psd_list[i]).begin(),
            [this](double val) { return val * data_sets; });
            
        for (size_t j = 0; i <  (*_psd_list[i]).size(); ++j) {
            (*_psd_list[i])[j] = ( (*_psd_list[i])[j] + other_normalized[j]) * (1.0 / joined_data_sets);
        }
    }

    data_sets = joined_data_sets;
}

void CalibrationData::normalize_to_frequencies() {
    for (auto psd : _psd_list) {
        for (size_t i = 0; i < (*psd).size(); ++i) {
            (*psd)[i] /= (freq_bins[i] + 0.1);

            if (freq_bins[i] < MIN_FREQ) {
                (*psd)[i] = 0.0;
            }
        }
    }
}

const std::vector<double>& CalibrationData::get_psd(
    const std::string& axis) const {

    auto it = _psd_map.find(axis);
    if (_psd_map.find(axis) == _psd_map.end()) {
        throw std::out_of_range("Invalid axis");
    }

    return it->second;
}

ShaperCalibrate::ShaperCalibrate(std::shared_ptr<Printer> printer) {
    this->printer = printer;
}

ShaperCalibrate::~ShaperCalibrate() {
    SPDLOG_INFO("~ShaperCalibrate()");
}

std::shared_ptr<CalibrationData> ShaperCalibrate::background_process_exec(
    std::function<std::shared_ptr<CalibrationData>(std::shared_ptr<AccelQueryHelper>)> method, 
    std::shared_ptr<AccelQueryHelper> data) {

    if(printer == nullptr) {
        return method(data);
    }

    std::promise<std::pair<bool, std::string>> promise;
    std::future<std::pair<bool, std::string>> future = promise.get_future();

    std::shared_ptr<CalibrationData> res;
    auto wrapper = [&]() {
        try {
            res = method(data);
            promise.set_value({false, "success"});
        } catch (const std::exception &e) {
            promise.set_value({true, e.what()});
        } catch (...) {
            promise.set_value({true, "Unknown exception"});
        }
    };

    std::thread calc_thread(wrapper);
    calc_thread.detach();

    auto reactor = printer->get_reactor();
    auto gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    double eventtime = get_monotonic();
    double last_report_time = eventtime;

    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        if (eventtime > last_report_time + 5.0) {
            last_report_time = eventtime;
            gcode->respond_info("Wait for calculations..", false);
        }
        eventtime = reactor->pause(eventtime + 0.1);
    }

    auto result = future.get();
    if (result.first) {
        throw std::runtime_error("Error in remote calculation: " + result.second);
    }
    return res;
}

CalibrationResult ShaperCalibrate::background_process_exec(
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
    double max_freq) {

    if(printer == nullptr) {
        return method(shaper_cfg,calibration_data,shaper_freqs,
        damping_ratio,scv,max_smoothing,test_damping_ratios,max_freq);
    }

    std::promise<std::pair<bool, std::string>> promise;
    std::future<std::pair<bool, std::string>> future = promise.get_future();

    CalibrationResult res;
    auto wrapper = [&]() {
        try {
            res = method(shaper_cfg,calibration_data,shaper_freqs,
            damping_ratio,scv,max_smoothing,test_damping_ratios,max_freq);;
            promise.set_value({false, "success"});
        } catch (const std::exception &e) {
            promise.set_value({true, e.what()});
        } catch (...) {
            promise.set_value({true, "Unknown exception"});
        }
    };

    std::thread calc_thread(wrapper);
    calc_thread.detach();

    auto reactor = printer->get_reactor();
    auto gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    double eventtime = get_monotonic();
    double last_report_time = eventtime;

    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        if (eventtime > last_report_time + 5.0) {
            last_report_time = eventtime;
            gcode->respond_info("Wait for calculations..", false);
        }
        eventtime = reactor->pause(eventtime + 0.1);
    }

    auto result = future.get();
    if (result.first) {
        throw std::runtime_error("Error in remote calculation: " + result.second);
    }
    return res;
}

std::vector<std::vector<double>> ShaperCalibrate::_split_into_windows(
    const std::vector<double>& x, int window_size, int overlap) {

    if (x.size() < window_size || overlap >= window_size) {
        throw std::invalid_argument("Invalid window_size or overlap.");
    }

    size_t step_between_windows = window_size - overlap;
    size_t n_windows = (x.size() - overlap) / step_between_windows;  
    
    std::vector<std::vector<double>> windows(window_size, std::vector<double>(n_windows));

    for (int i = 0; i < n_windows; ++i) {
        int start = i * step_between_windows;
        for (int j = 0; j < window_size; ++j) {
            windows[j][i] = x[start + j]; // 按列存储
        }
    }

    return windows;
}

double i0(double x) {
    const double max_iter = 20;
    const double tol = 1e-15;
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < max_iter; ++k) {
        term *= x * x / (4.0 * k * k);
        sum += term;
        if (term < tol) break;
    }
    return sum;
}

std::vector<double> kaiser(int n, double beta) {
    std::vector<double> window(n);
    double alpha = (n - 1) / 2.0;
    for (int i = 0; i < n; ++i) {
        double x = (i - alpha) / alpha;
        window[i] = i0(beta * std::sqrt(1 - x * x)) / i0(beta);
    }
    return window;
}

std::vector<std::complex<double>> compute_rfft(const std::vector<double>& input, int nfft) {
    int output_size = nfft / 2 + 1;
    std::vector<std::complex<double>> output(output_size);

    // 分配输入和输出数组
    double* in = fftw_alloc_real(nfft);
    fftw_complex* out = fftw_alloc_complex(output_size);

    // 复制输入数据
    std::fill(in, in + nfft, 0.0);
    std::copy(input.begin(), input.begin() + std::min((int)input.size(), nfft), in);

    // 创建 FFT 计划
    fftw_plan plan = fftw_plan_dft_r2c_1d(nfft, in, out, FFTW_ESTIMATE);
    fftw_execute(plan);

    // 复制结果
    for (int i = 0; i < output_size; ++i) {
        output[i] = std::complex<double>(out[i][0], out[i][1]);
    }

    // 释放资源
    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);

    return output;
}

std::pair<std::vector<double>, std::vector<double>> ShaperCalibrate::_psd(
    const std::vector<double>& x, double fs, int nfft) {

    std::vector<double> window = kaiser(nfft, 6.0);

    double sum_of_squares = 0.0;
    for (size_t i = 0; i < window.size(); ++i) {
        sum_of_squares += window[i] * window[i];
    }
    double scale = 1.0 / sum_of_squares;

    int overlap = nfft / 2;
    std::vector<std::vector<double>> x_windows = _split_into_windows(x, nfft, overlap);

    size_t rows = x_windows.size();
    size_t cols = x_windows[0].size();
    std::vector<double> means(cols, 0.0);

    for (size_t j = 0; j < cols; ++j) {
        for (size_t i = 0; i < rows; ++i) {
            means[j] += x_windows[i][j];
        }
        means[j] /= rows;
    }
    
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            x_windows[i][j] = window[i] * (x_windows[i][j] - means[j]);
        }
    }
    int output_size = nfft / 2 + 1;
    std::vector<std::vector<std::complex<double>>> result(output_size, std::vector<std::complex<double>>(cols));

    for (int col = 0; col < cols; ++col) {
        std::vector<double> column(rows);
        for (int row = 0; row < rows; ++row) {
            column[row] = x_windows[row][col];
        }

        std::vector<std::complex<double>> fft_result = compute_rfft(column, nfft);

        for (int i = 0; i < output_size; ++i) {
            result[i][col] = fft_result[i];
        }
    }

    for(int col = 0; col < cols; ++col) {
        for (int i = 0; i < output_size; ++i) {
            result[i][col] = std::norm(result[i][col]) * (scale / fs);
            if (i > 0 && i < output_size - 1) {
                result[i][col] *= 2.0;
            }
        }
    }

    std::vector<double> psd(output_size, 0);
    for (int i = 0; i < output_size; ++i) {
        double sum = 0.0;
        for(int col = 0; col < cols; ++col) {
            sum += result[i][col].real();
        }
        psd[i] = sum / cols;
    }


    std::vector<double> freqs(nfft / 2 + 1);
    for (size_t i = 0; i < output_size; ++i) {
        freqs[i] = i * fs / nfft;
    }

    return std::make_pair(freqs, psd);
}

unsigned int nextPowerOf2(double value) {
    if (value <= 1) return 1; // 2^0 = 1
    
    unsigned int intValue = static_cast<unsigned int>(value - 1);
    unsigned int power = 1;
    
    while (power <= intValue) {
        power <<= 1;
    }
    return power;
}

std::shared_ptr<CalibrationData> ShaperCalibrate::calc_freq_response(
    std::shared_ptr<AccelQueryHelper> raw_values) {
    if (!raw_values) {
      return nullptr;
    }

    std::vector<AccelMeasurement> samples = raw_values->get_samples();
    if (samples.empty()) {
        return nullptr;
    }
    //调试使用
    // std::ifstream file("./1.txt"); // 替换为你的文件路径
    // if (!file.is_open()) {
    //     std::cerr << "无法打开文件！" << std::endl;
    //     return nullptr;
    // }

    // std::vector<AccelMeasurement> samples; // 存储所有数据
    // std::string line;

    // // 读取文件行
    // while (std::getline(file, line)) {
    //     std::stringstream ss(line);
    //     AccelMeasurement row;
    //     std::string value;

    //     try {
    //         // 解析四个数据字段
    //         std::getline(ss, value, '\t'); row.time = std::stod(value);
    //         std::getline(ss, value, '\t'); row.accel_x = std::stod(value);
    //         std::getline(ss, value, '\t'); row.accel_y = std::stod(value);
    //         std::getline(ss, value, '\t'); row.accel_z = std::stod(value);

    //         // 存入 vector
    //         samples.push_back(row);
    //     } catch (const std::exception& e) {
    //         std::cerr << "数据解析错误：" << e.what() << "，行内容：" << line << std::endl;
    //     }
    // }

    // file.close();
    
    int N = samples.size();
    double T = samples.back().time - samples.front().time;
    double SAMPLING_FREQ = N / T;
    int M = nextPowerOf2(SAMPLING_FREQ * WINDOW_T_SEC - 1);
    if (N <= M) {
        SPDLOG_ERROR("N <= M");
        return nullptr;
    }

    std::vector<double> accel_x;
    std::vector<double> accel_y;
    std::vector<double> accel_z;
    for (size_t i = 0; i < samples.size(); i++) {
        accel_x.push_back(samples.at(i).accel_x);
        accel_y.push_back(samples.at(i).accel_y);
        accel_z.push_back(samples.at(i).accel_z);
    }

    auto result_x = _psd(accel_x, SAMPLING_FREQ, M);
    auto result_y = _psd(accel_y, SAMPLING_FREQ, M);
    auto result_z = _psd(accel_z, SAMPLING_FREQ, M);

    std::vector<double> total_psd(result_x.second.size());
    for (size_t i = 0; i < result_x.second.size(); ++i) {
        total_psd[i] = result_x.second[i] + result_y.second[i] + result_z.second[i];
    } 

    return std::make_shared<CalibrationData>(result_x.first, total_psd,
                result_x.second, result_y.second,
                result_z.second);
}

std::shared_ptr<CalibrationData> ShaperCalibrate::process_accelerometer_data(
      std::shared_ptr<AccelQueryHelper> data)
{
    std::shared_ptr<CalibrationData> calibration_data = background_process_exec(
        [this](std::shared_ptr<AccelQueryHelper> data) { return calc_freq_response(data); }, data);

    if (!calibration_data) {
        throw elegoo::common::CommandError("Internal error processing accelerometer data ");
    }

    return calibration_data;
}

std::vector<double> ShaperCalibrate::_estimate_shaper(
    const std::pair<std::vector<double>, std::vector<double>>& shaper,
    double test_damping_ratio, const std::vector<double>& test_freqs) {

    const std::vector<double>& A = shaper.first;
    const std::vector<double>& T = shaper.second;

    double sum_A = std::accumulate(A.begin(), A.end(), 0.0);
    double inv_D = 1.0 / sum_A;

    std::vector<double> omega(test_freqs.size());
    std::vector<double> damping(test_freqs.size());
    std::vector<double> omega_d(test_freqs.size());

    for (size_t i = 0; i < test_freqs.size(); ++i) {
        omega[i] = 2.0 * M_PI * test_freqs[i];  // 角频率
        damping[i] = test_damping_ratio * omega[i];  // 阻尼项
        omega_d[i] = omega[i] * sqrt(1.0 - test_damping_ratio * test_damping_ratio);  // 阻尼后角频率
    }

    size_t n = damping.size();
    size_t m = T.size();
    
    std::vector<std::vector<double>> W(n, std::vector<double>(m));
    std::vector<std::vector<double>> S(n, std::vector<double>(m));
    std::vector<std::vector<double>> C(n, std::vector<double>(m));
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < m; ++j) {
            W[i][j] = A[j] * exp(-damping[i] * (T.back() - T[j]));
            double omega_t = omega_d[i] * T[j];
            S[i][j] = W[i][j] * sin(omega_t);
            C[i][j] = W[i][j] * cos(omega_t);     
        }
    }

    std::vector<double> R(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        double S_sum = accumulate(S[i].begin(), S[i].end(), 0.0);
        double C_sum = accumulate(C[i].begin(), C[i].end(), 0.0);
        R[i] = sqrt(S_sum * S_sum + C_sum * C_sum) * inv_D;
    }

    return R;
}

std::pair<double, std::vector<double>> ShaperCalibrate::_estimate_remaining_vibrations(
    const std::pair<std::vector<double>, std::vector<double>>& shaper,
    double test_damping_ratio, const std::vector<double>& freq_bins,
    const std::vector<double>& psd) {
    
    std::vector<double> vals = _estimate_shaper(shaper, test_damping_ratio, freq_bins);
    double vibr_threshold = *max_element(psd.begin(), psd.end()) / 20;

    double remaining_vibrations = 0.0;
    double all_vibrations = 0.0;
    for (size_t i = 0; i < vals.size(); ++i) {
        remaining_vibrations += std::max(vals[i] * psd[i] - vibr_threshold, 0.0);
        all_vibrations += std::max(psd[i] - vibr_threshold, 0.0);
    }
    
    return {remaining_vibrations / all_vibrations, vals};
}

double ShaperCalibrate::_get_shaper_smoothing(
    const std::pair<std::vector<double>, std::vector<double>>& shaper, 
    double accel, double scv) {

    double half_accel = accel * 0.5;
    const std::vector<double>& A = shaper.first;
    const std::vector<double>& T = shaper.second;
    double sum_A = std::accumulate(A.begin(), A.end(), 0.0);
    double inv_D = 1.0 / sum_A;
    int n = T.size();

    double ts = 0.0;
    for (int i = 0; i < n; ++i) {
        ts += A[i] * T[i];
    }
    ts *= inv_D;

    double offset_90 = 0.0, offset_180 = 0.0;
    for (int i = 0; i < n; ++i) {
        if (T[i] >= ts) {
            offset_90 += A[i] * (scv + half_accel * (T[i] - ts)) * (T[i] - ts);
        }
        offset_180 += A[i] * half_accel * pow(T[i] - ts, 2);
    }
    offset_90 *= inv_D * sqrt(2.0);
    offset_180 *= inv_D;

    return std::max(offset_90, offset_180);
}

CalibrationResult ShaperCalibrate::fit_shaper(
    InputShaperCfg shaper_cfg, 
    std::shared_ptr<CalibrationData> calibration_data,
    std::vector<double> shaper_freqs, 
    double damping_ratio, 
    double scv,
    double max_smoothing, 
    std::vector<double> test_damping_ratios,
    double max_freq) {

    if(std::isnan(damping_ratio) || damping_ratio == 0) {
        damping_ratio = 0.1;
    }

    if(test_damping_ratios.empty()) {
        test_damping_ratios = {0.075, 0.1, 0.15};
    }

    std::vector<double> test_freqs;
    if (shaper_freqs.empty()) {
        shaper_freqs = {DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE};
    } 
  
    if (shaper_freqs.size() == 3) {
        double freq_end = (std::isnan(shaper_freqs[1]) || shaper_freqs[1]==0) ? MAX_SHAPER_FREQ : shaper_freqs[1];
        double freq_start = std::min((std::isnan(shaper_freqs[0]) || shaper_freqs[0]==0) ? 
        shaper_cfg.min_freq : shaper_freqs[0], shaper_freqs[1] - 1e-7);
        double freq_step = (std::isnan(shaper_freqs[2]) || shaper_freqs[2]==0) ? 0.2 : shaper_freqs[2];
        for (double f = freq_start; f < freq_end; f += freq_step) {
            test_freqs.push_back(f);
        }
    } else {
        test_freqs = shaper_freqs;
    }

    max_freq = std::max((std::isnan(max_freq) || max_freq==0) ? MAX_FREQ : max_freq, 
        *max_element(test_freqs.begin(), test_freqs.end()));
  
    std::vector<double> freq_bins = calibration_data->freq_bins;
    std::vector<double> freq_bins_filter;
    std::vector<double> psd;
    for (size_t i = 0; i < freq_bins.size(); ++i) {
        if (freq_bins[i] <= max_freq) {
            psd.push_back(calibration_data->psd_sum[i]);
            freq_bins_filter.push_back(freq_bins[i]);
        }
    }

    CalibrationResult best_res;
    bool is_vaild = false;
    std::vector<CalibrationResult> results;

    for (auto it = test_freqs.rbegin(); it != test_freqs.rend(); ++it) {
        double shaper_vibrations = 0;
        std::vector<double> shaper_vals(freq_bins_filter.size(), 0.0);
        auto shaper = shaper_cfg.init_func(*it, damping_ratio);
        double shaper_smoothing = _get_shaper_smoothing(shaper, 5000, scv);
        if ((!std::isnan(max_smoothing) && max_smoothing!=0) && 
            shaper_smoothing > max_smoothing && is_vaild) {
            return best_res;
        }

        for (double dr : test_damping_ratios) {
            auto retval = _estimate_remaining_vibrations(
                shaper, dr, freq_bins_filter, psd);
            for (size_t i = 0; i < shaper_vals.size(); i++) {
                shaper_vals[i] = std::max(shaper_vals[i], retval.second[i]);
            }  
            
            if (retval.first > shaper_vibrations) {
                shaper_vibrations = retval.first;
            }
        }

        double max_accel = find_shaper_max_accel(shaper, scv);
        double shaper_score = shaper_smoothing * (pow(shaper_vibrations, 1.5) +
                                                shaper_vibrations * 0.2 + 0.01);

        CalibrationResult result;
        result.name = shaper_cfg.name;
        result.freq = *it;
        result.vals = shaper_vals;
        result.vibrs = shaper_vibrations;
        result.smoothing = shaper_smoothing;
        result.score = shaper_score;
        result.max_accel = max_accel;
        results.push_back(result);

        if (!is_vaild || best_res.vibrs > result.vibrs) {
            is_vaild = true;
            best_res = result;
        }
    }

    CalibrationResult selected = best_res;
    for (auto it = results.rbegin(); it != results.rend(); ++it) {
        if ((*it).vibrs < best_res.vibrs * 1.1 && (*it).score < selected.score) {
            selected = (*it);
        }
    }

    return selected;
}

double ShaperCalibrate::_bisect(std::function<bool(double)> func) {
    double left = 1.0, right = 1.0;
    if (!func(1e-9)) {
        return 0.0;
    }

    while (!func(left)) {
        right = left;
        left *= 0.5;
    }

    if (right == left) {
        while (func(right)) {
            right *= 2.0;
        }
    }

    while (right - left > 1e-8) {
        double middle = (left + right) * 0.5;
        if (func(middle)) {
            left = middle;
        } else {
            right = middle;
        }
    }

    return left;
}

double ShaperCalibrate::find_shaper_max_accel(
    const std::pair<std::vector<double>, 
    std::vector<double>>& shaper, double scv) {

    return _bisect([this, shaper, scv](double test_accel) {
        return _get_shaper_smoothing(shaper, test_accel, scv) < 0.12;
    });
}

std::pair<CalibrationResult, std::vector<CalibrationResult>>
    ShaperCalibrate::find_best_shaper(
        std::shared_ptr<CalibrationData> calibration_data,
        std::vector<std::string> shapers, double damping_ratio, double scv,
        std::vector<double> shaper_freqs, double max_smoothing,
        std::vector<double> test_damping_ratios, double max_freq) 
{
    CalibrationResult best_shaper;
    bool is_vaild = false;
    std::vector<CalibrationResult> all_shapers;
    if (shapers.empty()) {
        shapers = AUTOTUNE_SHAPERS;
    }

    for (const auto& shaper_cfg : INPUT_SHAPERS) {
        auto it = std::find(shapers.begin(), shapers.end(), shaper_cfg.name);
        if (it == shapers.end()) {
            continue;
        }
        CalibrationResult shaper = background_process_exec(
            [this](InputShaperCfg shaper_cfg, 
            std::shared_ptr<CalibrationData> calibration_data,
            std::vector<double> shaper_freqs, 
            double damping_ratio, 
            double scv,
            double max_smoothing, 
            std::vector<double> test_damping_ratios,
            double max_freq) {
                return fit_shaper(shaper_cfg, calibration_data, shaper_freqs,
                damping_ratio, scv, max_smoothing,
                test_damping_ratios, max_freq);
            },
            shaper_cfg, calibration_data, shaper_freqs,
            damping_ratio, scv, max_smoothing,
            test_damping_ratios, max_freq
        );

        SPDLOG_INFO("Fitted shaper: {} frequency = {} Hz (vibrations = {}%, smoothing ~= {})",
            shaper.name, shaper.freq, shaper.vibrs * 100., shaper.smoothing);
        SPDLOG_INFO("To avoid too much smoothing with '{}', suggested max_accel <= {} mm/sec^2",
            shaper.name, round(shaper.max_accel / 100.) * 100.);

        all_shapers.push_back(shaper);
        if (!is_vaild || shaper.score * 1.2 < best_shaper.score ||
            (shaper.score * 1.05 < best_shaper.score &&
            shaper.smoothing * 1.1 < best_shaper.smoothing)) {
            best_shaper = shaper;
            is_vaild = true;
        }
    }

    return {best_shaper, all_shapers};
}

void ShaperCalibrate::save_params(std::shared_ptr<PrinterConfig> configfile,
                                  std::string axis,
                                  const std::string& shaper_name,
                                  const std::string& shaper_freq) {
    if (axis == "xy") {
        save_params(configfile, "x", shaper_name, shaper_freq);
        save_params(configfile, "y", shaper_name, shaper_freq);
    } else {
        configfile->set("input_shaper", "shaper_type_" + axis, shaper_name);
        configfile->set("input_shaper", "shaper_freq_" + axis, shaper_freq);
    }
}

void ShaperCalibrate::apply_params(std::shared_ptr<InputShaper> input_shaper,
                                   std::string axis,
                                   const std::string& shaper_name,
                                   const std::string& shaper_freq) {
    if (axis == "xy") {
        apply_params(input_shaper, "x", shaper_name, shaper_freq);
        apply_params(input_shaper, "y", shaper_name, shaper_freq);
        return;
    }

    auto gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    std::transform(axis.begin(), axis.end(), axis.begin(), ::toupper);
    input_shaper->cmd_SET_INPUT_SHAPER(
        gcode->create_gcode_command(
            "SET_INPUT_SHAPER",
            "SET_INPUT_SHAPER",
            {{"SHAPER_TYPE_" + axis,shaper_name},
             {"SHAPER_FREQ_" + axis,shaper_freq}}
        )
    );
}

void ShaperCalibrate::save_calibration_data(const std::string& output,
    std::shared_ptr<CalibrationData> calibration_data,
    std::vector<CalibrationResult> shapers, double max_freq) {

    if (std::isnan(max_freq)) {
        max_freq = MAX_FREQ;
    }

    std::ofstream csvfile(output.c_str());
    if (!csvfile.is_open()) {
        std::cerr << "Failed to open file: " << output << std::endl;
        return;
    }
    csvfile << "freq,psd_x,psd_y,psd_z,psd_xyz";

    if (!shapers.empty()) {
        for (const auto& shaper : shapers) {
            csvfile << "," << shaper.name << "(" << std::fixed << std::setprecision(1)
                << shaper.freq << ")";
        }
    }
    csvfile << "\n";

    if (calibration_data) {
        size_t num_freqs = calibration_data->freq_bins.size();
        for (size_t i = 0; i < num_freqs; ++i) {
            if (calibration_data->freq_bins[i] >= max_freq) {
                break;
            }
            csvfile << std::fixed << std::setprecision(1)
                    << calibration_data->freq_bins[i] << "," << std::setprecision(3)
                    << calibration_data->psd_x[i] << "," << calibration_data->psd_y[i]
                    << "," << calibration_data->psd_z[i] << ","
                    << calibration_data->psd_sum[i];

            if (!shapers.empty()) {
                for (const auto& shaper : shapers) {
                    csvfile << "," << std::setprecision(3) << shaper.vals[i];
                }
            }
            csvfile << "\n";
        }
    }

    csvfile.close();
}
}
}
