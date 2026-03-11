/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-23 12:21:07
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-17 15:40:48
 * @Description  : Support for enable pins on stepper motor drivers
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "configfile.h"
#include "json.h"
#include "mcu.h"
#include "pins.h"
#include "printer.h"
#include "toolhead.h"

namespace elegoo {
namespace extras {
  
class PrinterStepperEnable;
class StepperEnablePin;
class EnableTracking;
class PrinterStepperEnable {
 public:
  PrinterStepperEnable(std::shared_ptr<ConfigWrapper> config);
  ~PrinterStepperEnable();

  std::vector<std::string> get_steppers();
  void register_stepper(std::shared_ptr<ConfigWrapper> config,
                        std::shared_ptr<MCU_stepper> mcu_stepper);
  void motor_off();
  void motor_debug_enable(const std::string &stepper, int enable);
  json get_status() const;
  void _handle_request_restart();
  void cmd_M18();
  void cmd_SET_STEPPER_ENABLE(std::shared_ptr<GCodeCommand> gcmd);
  std::shared_ptr<EnableTracking> lookup_enable(const std::string &name);

 private:
  std::shared_ptr<Printer> printer;
  std::map<std::string, std::shared_ptr<EnableTracking>> enable_lines;

};

class StepperEnablePin {
 public:
  StepperEnablePin(std::shared_ptr<MCU_digital_out> mcu_enable,
                   int enable_count);
  void set_enable(double print_time);
  void set_disable(double print_time);
  bool IsDedicated() const {return is_dedicated;};
  void set_dedicated(bool bcmd) {is_dedicated = bcmd;};

 private:
  std::shared_ptr<MCU_digital_out> mcu_enable;
  int enable_count;
  bool is_dedicated;
};

std::shared_ptr<StepperEnablePin> setup_enable_pin(
    std::shared_ptr<Printer> printer, const std::string &pin);

class EnableTracking {
 public:
  EnableTracking(std::shared_ptr<MCU_stepper> stepper,
                 std::shared_ptr<StepperEnablePin> enable);
  ~EnableTracking();
  void register_state_callback(std::function<void(double, bool)> callback);
  void motor_enable(double print_time);
  void motor_disable(double print_time);
  bool is_motor_enabled() const;
  bool has_dedicated_enable() const;

 private:
  std::shared_ptr<MCU_stepper> stepper;
  std::shared_ptr<StepperEnablePin> enable;
  std::vector<std::function<void(double, bool)>> callbacks;
  bool is_enabled;

};

std::shared_ptr<PrinterStepperEnable> stepper_enable_load_config(
    std::shared_ptr<ConfigWrapper> config);

}
}
