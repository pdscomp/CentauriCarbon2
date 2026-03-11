/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-16 16:44:55
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:51:55
 * @Description  : TMC2209 configuration
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "configfile.h"
#include "gcode.h"
#include "mcu.h"
#include "printer.h"
#include "tmc.h"
#include "tmc_uart.h"
#include "tmc2130.h"


namespace elegoo {
namespace extras {

class TMC2209 {
 public:
  TMC2209(std::shared_ptr<ConfigWrapper> config);
  json get_status(double eventtime = DOUBLE_NONE );
  std::pair<int, int> get_phase_offset();
 private:
  std::shared_ptr<FieldHelper> fields;
  std::shared_ptr<TMCVirtualPinHelper> pin_helper;
  std::shared_ptr<MCU_TMC_uart> mcu_tmc;
  std::shared_ptr<TMCCommandHelper> cmdhelper;
};

std::shared_ptr<TMC2209> tmc2209_load_config_prefix(
      std::shared_ptr<ConfigWrapper> config);



}
}
