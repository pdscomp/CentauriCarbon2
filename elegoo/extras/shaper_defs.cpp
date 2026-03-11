/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-23 12:20:06
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-12 17:41:45
 * @Description  : Definitions of the supported input shapers
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "shaper_defs.h"
#include "common/logger.h"

namespace elegoo {
namespace extras {

static std::vector<InputShaperCfg> INPUT_SHAPERS = {
    InputShaperCfg("zv", get_zv_shaper, 21.0),
    InputShaperCfg("mzv", get_mzv_shaper, 23.0),
    InputShaperCfg("zvd", get_zvd_shaper, 29.0),
    InputShaperCfg("ei", get_ei_shaper, 29.0),
    InputShaperCfg("2hump_ei", get_2hump_ei_shaper, 39.0),
    InputShaperCfg("3hump_ei", get_3hump_ei_shaper, 48.0),
};

std::vector<InputShaperCfg> get_input_shapers()
{
  SPDLOG_DEBUG("__func__:{} #1",__func__);
  return INPUT_SHAPERS;
}

std::pair<std::vector<double>, std::vector<double>> get_none_shaper() {
  return {{}, {}};
}

std::pair<std::vector<double>, std::vector<double>> get_zv_shaper(
    double shaper_freq, double damping_ratio) {
  double df = std::sqrt(1.0 - damping_ratio * damping_ratio);
  double K = std::exp(-damping_ratio * M_PI / df);
  double t_d = 1.0 / (shaper_freq * df);
  std::vector<double> A = {1.0, K};
  std::vector<double> T = {0.0, 0.5 * t_d};
  return {A, T};
}

std::pair<std::vector<double>, std::vector<double>> get_zvd_shaper(
    double shaper_freq, double damping_ratio) {
  double df = std::sqrt(1.0 - damping_ratio * damping_ratio);
  double K = std::exp(-damping_ratio * M_PI / df);
  double t_d = 1.0 / (shaper_freq * df);
  std::vector<double> A = {1.0, 2.0 * K, K * K};
  std::vector<double> T = {0.0, 0.5 * t_d, t_d};
  return {A, T};
}

std::pair<std::vector<double>, std::vector<double>> get_mzv_shaper(
    double shaper_freq, double damping_ratio) {
  double df = std::sqrt(1.0 - damping_ratio * damping_ratio);
  double K = std::exp(-0.75 * damping_ratio * M_PI / df);
  double t_d = 1.0 / (shaper_freq * df);

  double a1 = 1.0 - 1.0 / std::sqrt(2.0);
  double a2 = (std::sqrt(2.0) - 1.0) * K;
  double a3 = a1 * K * K;

  std::vector<double> A = {a1, a2, a3};
  std::vector<double> T = {0.0, 0.375 * t_d, 0.75 * t_d};
  return {A, T};
}

std::pair<std::vector<double>, std::vector<double>> get_ei_shaper(
    double shaper_freq, double damping_ratio) {
  double v_tol = 1.0 / SHAPER_VIBRATION_REDUCTION;
  double df = std::sqrt(1.0 - damping_ratio * damping_ratio);
  double K = std::exp(-damping_ratio * M_PI / df);
  double t_d = 1.0 / (shaper_freq * df);

  double a1 = 0.25 * (1.0 + v_tol);
  double a2 = 0.5 * (1.0 - v_tol) * K;
  double a3 = a1 * K * K;

  std::vector<double> A = {a1, a2, a3};
  std::vector<double> T = {0.0, 0.5 * t_d, t_d};
  return {A, T};
}

std::pair<std::vector<double>, std::vector<double>> get_2hump_ei_shaper(
    double shaper_freq, double damping_ratio) {
  double v_tol = 1.0 / SHAPER_VIBRATION_REDUCTION;
  double df = std::sqrt(1.0 - damping_ratio * damping_ratio);
  double K = std::exp(-damping_ratio * M_PI / df);
  double t_d = 1.0 / (shaper_freq * df);

  double V2 = v_tol * v_tol;
  double X = std::pow(V2 * (std::sqrt(1.0 - V2) + 1.0), 1.0 / 3.0);
  double a1 = (3.0 * X * X + 2.0 * X + 3.0 * V2) / (16.0 * X);
  double a2 = (0.5 - a1) * K;
  double a3 = a2 * K;
  double a4 = a1 * K * K * K;

  std::vector<double> A = {a1, a2, a3, a4};
  std::vector<double> T = {0.0, 0.5 * t_d, t_d, 1.5 * t_d};
  return {A, T};
}

std::pair<std::vector<double>, std::vector<double>> get_3hump_ei_shaper(
    double shaper_freq, double damping_ratio) {
  double v_tol = 1.0 / SHAPER_VIBRATION_REDUCTION;
  double df = std::sqrt(1.0 - damping_ratio * damping_ratio);
  double K = std::exp(-damping_ratio * M_PI / df);
  double t_d = 1.0 / (shaper_freq * df);

  double K2 = K * K;
  double a1 = 0.0625 * (1.0 + 3.0 * v_tol +
                        2.0 * std::sqrt(2.0 * (v_tol + 1.0) * v_tol));
  double a2 = 0.25 * (1.0 - v_tol) * K;
  double a3 = (0.5 * (1.0 + v_tol) - 2.0 * a1) * K2;
  double a4 = a2 * K2;
  double a5 = a1 * K2 * K2;

  std::vector<double> A = {a1, a2, a3, a4, a5};
  std::vector<double> T = {0.0, 0.5 * t_d, t_d, 1.5 * t_d, 2.0 * t_d};
  return {A, T};
}

}
}
