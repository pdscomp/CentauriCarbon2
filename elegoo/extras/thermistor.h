/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-23 12:22:02
 * @LastEditors  : Ben
 * @LastEditTime : 2024-12-16 20:56:49
 * @Description  : Temperature measurements with thermistors
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <map>
#include <memory>
#include <string>
#include "liner_base.h"
#include "configfile.h"
#include "json.h"
#include "printer.h"
#include "heaters.h"

namespace elegoo {
namespace extras {
class Thermistor : public LinearBase {
 public:
  Thermistor(const float pullup, const float inline_resistor);
  ~Thermistor();
  void setup_coefficients(double t1, double r1, double t2, double r2, double t3,
                          double r3, const std::string& name = "");
  void setup_coefficients_beta(double t1, double r1, double beta);

  double calc_temp(double adc) const override;
  double calc_adc(double temp) const override;

 private:
  double pullup;
  double inline_resistor;
  double c1, c2, c3;
};

std::shared_ptr<Thermistor> PrinterThermistor(
    std::shared_ptr<ConfigWrapper> config,
    std::map<std::string, double> params);

class CustomThermistor {
 public:
  CustomThermistor(std::shared_ptr<ConfigWrapper> config);
  std::shared_ptr<PrinterADCtoTemperature> create(
      std::shared_ptr<ConfigWrapper> config);
  const std::string & get_name();
 private:
  std::string extract_name(const std::string& full_name) const {
    std::istringstream iss(full_name);
    std::string prefix, name;

    // iss >> prefix >> name;// 读取前缀
    if (iss >> prefix) {
        // 忽略前缀后的所有空格
        iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
        // 使用 getline 读取剩余部分，包括空格
        std::getline(iss, name, '\n');
    }
    SPDLOG_DEBUG("__func__:{},full_name:{},prefix:{},name:{}",__func__,full_name,prefix,name);
    return name;
  }

  std::string name;
  double t1, r1, t2, r2, t3, r3, beta;
public:
  std::map<std::string, double> params;
};


std::shared_ptr<void> thermistor_load_config_prefix(std::shared_ptr<ConfigWrapper> config);


}  
}