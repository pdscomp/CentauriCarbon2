/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-05 15:13:12
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-27 17:05:11
 * @Description  : Utility for querying the current state of adc pins
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include "configfile.h"
#include "gcode_move.h"
#include "json.h"
#include "mcu.h"
#include "mcu.h"
#include "printer.h"

namespace elegoo {
namespace extras {
class QueryADC {
 public:
  QueryADC(std::shared_ptr<ConfigWrapper> config);
  ~QueryADC();
  void register_adc(const std::string &name, std::shared_ptr<MCU_adc> mcu_adc);
  void cmd_QUERY_ADC(std::shared_ptr<GCodeCommand> gcmd);

 private:
  std::shared_ptr<Printer> printer;
  std::map<std::string, std::shared_ptr<MCU_adc>> adc;
};

std::shared_ptr<QueryADC> query_adc_load_config(
    std::shared_ptr<ConfigWrapper> config);


}
}