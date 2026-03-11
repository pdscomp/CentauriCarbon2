/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-09 16:06:34
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-09 16:35:16
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

#include <functional>
#include "configfile.h"

namespace elegoo {
namespace extras {

class LinearBase {
 public:
  LinearBase(std::shared_ptr<ConfigWrapper> config,
             const std::vector<std::pair<double, double>>& params){};
  LinearBase(const float pullup, const float inline_resistor){};
  virtual double calc_temp(double adc) const {};
  virtual double calc_adc(double temp) const {};

 private:
  // std::shared_ptr<LinearInterpolate> li;
};

}
}