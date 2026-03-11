/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-15 11:45:02
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:55:48
 * @Description  : Perform Z Homing at specific XY coordinates
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
#include "cartesian.h"
#include "configfile.h"
#include "gcode.h"
#include "mcu.h"
#include "printer.h"
#include "toolhead.h"

namespace elegoo {
namespace extras {
class SafeZHoming {
 public:
  SafeZHoming(std::shared_ptr<ConfigWrapper> config);
  void cmd_G28(std::shared_ptr<GCodeCommand> gcmd);

 private:
  std::shared_ptr<ConfigWrapper> config;
  std::shared_ptr<Printer> printer;
  double home_x_pos, home_y_pos;
  double z_hop, z_hop_speed, max_z, speed;
  bool move_to_previous;
  std::shared_ptr<GCodeDispatch> gcode;
  std::function<void(std::shared_ptr<GCodeCommand>)> prev_G28;
};

std::shared_ptr<SafeZHoming> safe_z_home_load_config(std::shared_ptr<ConfigWrapper> config);


}
}