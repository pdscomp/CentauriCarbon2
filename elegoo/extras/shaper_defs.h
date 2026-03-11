/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-23 12:19:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-03-05 14:21:25
 * @Description  : Definitions of the supported input shapers
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <mutex>

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace elegoo {
namespace extras {

#define SHAPER_VIBRATION_REDUCTION 20.0
#define DEFAULT_DAMPING_RATIO 0.1

struct InputShaperCfg {
  std::string name;
  std::function<std::pair<std::vector<double>, 
    std::vector<double>>(double, double)>init_func;
  double min_freq;

  InputShaperCfg(
      std::string name,
      std::function<std::pair<std::vector<double>, 
        std::vector<double>>(double, double)> init_func,
      double min_freq) : name(name), init_func(init_func), min_freq(min_freq) {}
};

std::vector<InputShaperCfg> get_input_shapers();

std::pair<std::vector<double>, std::vector<double>> get_none_shaper();
std::pair<std::vector<double>, std::vector<double>> get_zv_shaper(
    double shaper_freq, double damping_ratio);
std::pair<std::vector<double>, std::vector<double>> get_zvd_shaper(
    double shaper_freq, double damping_ratio);
std::pair<std::vector<double>, std::vector<double>> get_mzv_shaper(
    double shaper_freq, double damping_ratio);
std::pair<std::vector<double>, std::vector<double>> get_ei_shaper(
    double shaper_freq, double damping_ratio);
std::pair<std::vector<double>, std::vector<double>> get_2hump_ei_shaper(
    double shaper_freq, double damping_ratio);
std::pair<std::vector<double>, std::vector<double>> get_3hump_ei_shaper(
    double shaper_freq, double damping_ratio);

}
}