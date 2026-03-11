/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-14 18:03:08
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:46:44
 * @Description  : Common helper code for TMC stepper drivers
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "configfile.h"
#include "force_move.h"
#include "gcode_move.h"
#include "heaters.h"
#include "homing.h"
#include "json.h"
#include "mcu.h"
#include "printer.h"
#include "stepper.h"
#include "stepper_enable.h"
#include "tmc_uart.h"
#include "tmc2130.h"

namespace elegoo {
namespace extras {

class MCU_TMC_Base;
class MCU_TMC_uart;
class TMCCurrentHelper;
class TMCCommandHelper;

class FieldHelper;
// ######################################################################
// # Field helpers
// ######################################################################
class FieldHelper {
 public:
  FieldHelper(std::map<std::string, std::map<std::string, uint32_t>> all_fields,
              std::vector<std::string> signed_fields,
              std::map<std::string, std::function<std::string(int)>>
                  field_formatters,
              std::vector<std::pair<std::string, int64_t>> registers = {});
  ~FieldHelper();
  std::string lookup_register(std::string field_name,
                              std::string default_value = "");
  int64_t get_field(std::string field_name, uint32_t reg_value = 0,
                std::string reg_name = "");
  int64_t set_field(std::string field_name, int64_t field_value, uint32_t reg_value = 0,
                std::string reg_name = "");
  int64_t set_config_field(std::shared_ptr<ConfigWrapper> config,
                       std::string field_name, int default_val);
  std::string pretty_format(const std::string& reg_name, int reg_value);
  std::map<std::string, int64_t> get_reg_fields(const std::string& reg_name,
                                            int reg_value);

  std::map<std::string, std::map<std::string, uint32_t>> all_fields;
  std::vector<std::pair<std::string, int64_t>> registers;

 private:
  std::map<std::string, uint32_t> signed_fields;
  std::map<std::string, std::function<std::string(int)>>
      field_formatters;
  std::map<std::string, std::string> field_to_register;
};

// ######################################################################
// # Periodic error checking
// ######################################################################

class TMCErrorCheck {
 public:
  TMCErrorCheck(std::shared_ptr<ConfigWrapper> config,
                std::shared_ptr<MCU_TMC_Base> mcu_tmc,
                std::function<void(double)> fun);
  ~TMCErrorCheck();

  uint32_t _query_register(
      const std::pair<std::string, std::vector<uint32_t>>& reg_info,
      bool try_clear = false);
  void _query_temperature();
  double _do_periodic_check(double eventtime);
  void stop_checks();
  bool start_checks();
  void get_status(std::map<std::string, std::map<std::string, int>>& drv_status,
                  std::map<std::string, double>& temperature);

 private:
  std::shared_ptr<Printer> printer;
  std::function<void(double)> init_register;
  std::string stepper_name;
  std::shared_ptr<MCU_TMC_Base> mcu_tmc;
  std::shared_ptr<FieldHelper> fields;
  std::shared_ptr<ReactorTimer> check_timer;
  bool clear_gstat;
  int last_drv_status;
  std::map<std::string, int> last_drv_fields;
  std::string irun_field;
  std::string reg_name;
  std::pair<std::string, std::vector<uint32_t>> gstat_reg_info;
  std::pair<std::string, std::vector<uint32_t>> drv_status_reg_info;
  uint32_t adc_temp;
  std::string adc_temp_reg;
};

class TMCCurrentHelper {
public:
    TMCCurrentHelper(std::shared_ptr<ConfigWrapper> config,
                   std::shared_ptr<MCU_TMC_Base> mcu_tmc);
    ~TMCCurrentHelper();
     std::tuple<double, double, double, double> get_current();
    void set_current(double run_current, double hold_current, double print_time);
private:
    int calc_current_bits(double current, bool vsense);
    double calc_current_from_bits(int cs, bool vsense);
    std::tuple<bool, int, int> calc_current(double run_current, double hold_current);

private:
    std::shared_ptr<Printer> printer;
    std::string name;
    std::shared_ptr<MCU_TMC_Base> mcu_tmc;
    std::shared_ptr<FieldHelper> fields;
    double req_hold_current;
    double sense_resistor;
};

// ######################################################################
// # G-Code command helpers
// ######################################################################

class TMCCommandHelper {
 public:
  TMCCommandHelper(std::shared_ptr<ConfigWrapper> config,
                   std::shared_ptr<MCU_TMC_Base> mcu_tmc,
                   std::shared_ptr<TMCCurrentHelper> current_helper);
  ~TMCCommandHelper();

  void _init_registers(double print_time = DOUBLE_NONE);

  void cmd_SET_TMC_FIELD(std::shared_ptr<GCodeCommand> gcmd);
  void cmd_INIT_TMC(std::shared_ptr<GCodeCommand> gcmd);
  void cmd_SET_TMC_CURRENT(std::shared_ptr<GCodeCommand> gcmd);
  void cmd_DUMP_TMC(std::shared_ptr<GCodeCommand> gcmd);

  int64_t _get_phases();
  std::pair<int64_t, int64_t> get_phase_offset();
  int64_t _query_phase();
  void _handle_sync_mcu_pos(std::shared_ptr<MCU_stepper> stepper);
  //void _handle_sync_mcu_pos();

  json _do_enable(double print_time);
  json _do_disable(double print_time);
  void _handle_stepper_enable(double print_time, bool is_enable);
  void _handle_mcu_identify();
  void _handle_connect();
  json get_status(double eventtime = DOUBLE_NONE);
  void setup_register_dump(
      std::vector<std::string> read_registers,
      std::function<std::pair<std::string, int64_t>(std::string, int64_t)>
          read_translate = {});

 private:
  std::shared_ptr<Printer> printer;
  std::string stepper_name;
  std::string name;
  std::shared_ptr<MCU_TMC_Base> mcu_tmc;
  std::shared_ptr<TMCCurrentHelper> current_helper;  // 类型不确定,准备设置基类
  std::shared_ptr<TMCErrorCheck> echeck_helper;
  std::shared_ptr<FieldHelper> fields;
  std::vector<std::string> read_registers;
  std::function<std::pair<std::string, int64_t>(std::string, int64_t)> read_translate;
  int64_t toff;
  int64_t mcu_phase_offset;
  std::shared_ptr<MCU_stepper> stepper;
  std::shared_ptr<PrinterStepperEnable> stepper_enable;
};

// ######################################################################
// # TMC virtual pins
// ######################################################################

class TMCVirtualPinHelper :public std::enable_shared_from_this<TMCVirtualPinHelper>, public ChipBase {
 public:
  TMCVirtualPinHelper(std::shared_ptr<ConfigWrapper> config,
                      std::shared_ptr<MCU_TMC_Base> mcu_tmc);
  ~TMCVirtualPinHelper();

  void init();
  std::shared_ptr<MCU_pins> setup_pin(const std::string& pin_type, std::shared_ptr<PinParams> pin_params);
  void handle_homing_move_begin(std::shared_ptr<HomingMove> hmove);
  void handle_homing_move_end(std::shared_ptr<HomingMove> hmove);

 private:
  std::shared_ptr<Printer> printer;
  std::shared_ptr<ConfigWrapper> config;
  std::shared_ptr<MCU_TMC_Base> mcu_tmc;
  std::shared_ptr<FieldHelper> fields;
  int pwmthrs, thigh, coolthrs;
  bool en_pwm;
  std::string diag_pin;
  std::string diag_pin_field;
  std::shared_ptr<MCU_endstop> mcu_endstop;
};

// ######################################################################
// # Config reading helpers
// ######################################################################
namespace tmc {
void TMCWaveTableHelper(std::shared_ptr<ConfigWrapper> config,
                        std::shared_ptr<MCU_TMC_Base> mcu_tmc);
void TMCMicrostepHelper(std::shared_ptr<ConfigWrapper> config,
                        std::shared_ptr<MCU_TMC_Base> mcu_tmc);
int TMCtstepHelper(std::shared_ptr<MCU_TMC_Base> mcu_tmc, double velocity,
                   std::shared_ptr<MCU_stepper> pstepper = nullptr,
                   std::shared_ptr<ConfigWrapper> config = nullptr);
void TMCStealthchopHelper(std::shared_ptr<ConfigWrapper> config,
                          std::shared_ptr<MCU_TMC_Base> mcu_tmc);
void TMCVcoolthrsHelper(std::shared_ptr<ConfigWrapper> config,
                        std::shared_ptr<MCU_TMC_Base> mcu_tmc);
void TMCVhighHelper(std::shared_ptr<ConfigWrapper> config,
                    std::shared_ptr<MCU_TMC_Base> mcu_tmc);
}

}
}
