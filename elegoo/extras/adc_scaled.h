/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-05 15:13:12
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-22 15:12:08
 * @Description  : Support for scaling ADC values based on measured VREF and
 *VSSA
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include "configfile.h"
#include "mcu.h"
#include "printer.h"

namespace elegoo {
namespace extras {


class PrinterADCScaled;
class MCU_scaled_adc;

class MCU_scaled_adc : public MCU_pins{
 public:
  MCU_scaled_adc(PrinterADCScaled* main, std::shared_ptr<PinParams> pin_params);
  void setup_adc_callback(float report_time,
                          std::function<void(float, float)> callback);
  std::pair<float, float> get_last_value() const;

 private:
  void handle_callback(float read_time, float read_value);

 private:
  PrinterADCScaled* _main;
  std::shared_ptr<MCU_adc> _mcu_adc;
  std::function<void(float, float)> _callback;
  std::pair<float, float> _last_state;
};

class PrinterADCScaled : public ChipBase,public std::enable_shared_from_this<PrinterADCScaled>{
 public:
  PrinterADCScaled(std::shared_ptr<ConfigWrapper>& config);
  std::shared_ptr<MCU_pins> setup_pin(const std::string& pin_type,
                                            std::shared_ptr<PinParams> pin_params);
  void vref_callback(float read_time, float read_value);
  void vssa_callback(float read_time, float read_value);

 public:
  const std::string& get_name() const;
  std::shared_ptr<Printer> get_printer() const;
  std::shared_ptr<MCU> get_mcu() const;
  const std::pair<float, float> & get_last_vref() const;
  const std::pair<float, float> & get_last_vssa() const;

 private:
  std::shared_ptr<MCU_adc> _config_pin(
      std::shared_ptr<ConfigWrapper>& config, const std::string& name,
      std::function<void(float, float)> callback);
  std::pair<float, float> calc_smooth(float read_time, float read_value,
                                      const std::pair<float, float>& last);

 private:
  float inv_smooth_time;
  std::shared_ptr<MCU_adc> mcu_vref;
  std::shared_ptr<MCU_adc> mcu_vssa;
  std::string name;  
  std::shared_ptr<Printer> printer;
  std::shared_ptr<MCU> mcu;
  std::pair<float, float> last_vref;
  std::pair<float, float> last_vssa;
};


std::shared_ptr<PrinterADCScaled> adc_scaled_load_config_prefix(
    std::shared_ptr<ConfigWrapper> config);

}  
}