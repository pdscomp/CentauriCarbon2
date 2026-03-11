/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-25 10:09:42
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-18 13:45:40
 * @Description  : Helper script to adjust parameters based on Z level
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
#include "gcode.h"
#include "gcode_move.h"
#include "json.h"
#include "mcu.h"
#include "printer.h"
#include "gcode_move_transform.h"

namespace elegoo {
namespace extras {

class GCodeMove;
class TuningTower {
 public:
  TuningTower(std::shared_ptr<ConfigWrapper> config);
  void cmd_TUNING_TOWER(std::shared_ptr<GCodeCommand> gcmd);
  std::vector<double> get_position();
  double calc_value(double z);
  void move(std::vector<double>& newpos, double speed);
  void end_test();
  bool is_active() const;
  std::shared_ptr<GCodeMoveTransform> move_transform;

 private:
  std::shared_ptr<Printer> printer;
  std::shared_ptr<GCodeMove> gcode_move;
  std::shared_ptr<GCodeDispatch> gcode;
  std::shared_ptr<GCodeMoveTransform> normal_transform;
  std::vector<double> last_position = {0., 0., 0., 0.};
  double last_z;
  double start, factor, band, step_delta, step_height, skip;
  double last_command_value;
  std::string command, parameter, command_fmt;
};


std::shared_ptr<TuningTower> tuning_tower_load_config(std::shared_ptr<ConfigWrapper> config);


}
}
