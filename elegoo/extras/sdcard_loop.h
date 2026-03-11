/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-11 15:44:36
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:59:21
 * @Description  : Sdcard file looping support
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>
#include "configfile.h"
#include "mcu.h"
#include "printer.h"
#include "virtual_sdcard.h"

namespace elegoo {
namespace extras {
class SDCardLoop {
 public:
  SDCardLoop(std::shared_ptr<ConfigWrapper> config);
  ~SDCardLoop();

  void cmd_SDCARD_LOOP_BEGIN(std::shared_ptr<GCodeCommand> gcmd);
  void cmd_SDCARD_LOOP_END(std::shared_ptr<GCodeCommand> gcmd);
  void cmd_SDCARD_LOOP_DESIST(std::shared_ptr<GCodeCommand> gcmd);
  bool loop_begin(int count);
  bool loop_end();
  bool loop_desist();

 private:
  std::shared_ptr<VirtualSD> sdcard;
  std::shared_ptr<GCodeDispatch> gcode;
  std::vector<std::pair<int, int>> loop_stack;
};

std::shared_ptr<SDCardLoop> sdcard_loop_load_config(std::shared_ptr<ConfigWrapper> config);

}
}
