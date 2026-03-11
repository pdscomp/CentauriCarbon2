/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-27 12:04:25
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-18 13:37:12
 * @Description  : Exclude moves toward and inside objects
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
#include "extruder.h"
#include "gcode_move.h"
#include "json.h"
#include "mcu.h"
#include "printer.h"
#include "toolhead.h"
#include "tuning_tower.h"
#include "gcode.h"
#include "gcode_move.h"
#include "gcode_move_transform.h"


namespace elegoo {
namespace extras {

class TuningTower;
class GCodeMove;
class ExcludeObject {
 public:
  ExcludeObject(std::shared_ptr<ConfigWrapper> config);
  ~ExcludeObject();

  void _register_transform();
  void _handle_connect();
  void _unregister_transform();
  void _reset_state();
  void _reset_file();
  std::vector<double> _get_extrusion_offsets();
  std::vector<double> get_position();
  void _normal_move(std::vector<double> newpos, double speed);
  void _ignore_move(std::vector<double> newpos, double speed);
  void _move_into_excluded_region(std::vector<double> newpos, double speed);
  void _move_from_excluded_region(std::vector<double> newpos, double speed);
  bool _test_in_excluded_region();
  json get_status(double eventtime = 0.0);
  void move(std::vector<double> newpos, double speed);
  void cmd_EXCLUDE_OBJECT_START(std::shared_ptr<GCodeCommand> gcmd);
  void cmd_EXCLUDE_OBJECT_END(std::shared_ptr<GCodeCommand> gcmd);
  void cmd_EXCLUDE_OBJECT(std::shared_ptr<GCodeCommand> gcmd);
  void cmd_EXCLUDE_OBJECT_DEFINE(std::shared_ptr<GCodeCommand> gcmd);
  void _add_object_definition(std::map<std::string, std::string> definition);
  void _exclude_object(const std::string &name);
  void _unexclude_object(const std::string &name);
  void _list_objects(std::shared_ptr<GCodeCommand> gcmd);
  void _list_excluded_objects(std::shared_ptr<GCodeCommand> gcmd);

 public:
  std::shared_ptr<ToolHead> get_toolhead();
  std::shared_ptr<GCodeMoveTransform> move_transform;

 private:
  std::shared_ptr<ToolHead> toolhead;
  std::shared_ptr<Printer> printer;
  std::shared_ptr<GCodeDispatch> gcode;
  std::shared_ptr<GCodeMove> gcode_move;
  std::shared_ptr<GCodeMoveTransform> next_transform;
  std::vector<double> last_position_extruded{0.0, 0.0, 0.0, 0.0};
  std::vector<double> last_position_excluded{0.0, 0.0, 0.0, 0.0};
  std::vector<double> last_position{0.0, 0.0, 0.0, 0.0};
  std::map<std::string, std::vector<double>> extrusion_offsets;
  double max_position_extruded;
  double max_position_excluded;
  double extruder_adj;

  int initial_extrusion_moves;
  bool in_excluded_region;
  bool was_excluded_at_start;
  std::string current_object;
  std::vector<std::map<std::string, std::string>> objects;
  std::vector<std::string> excluded_objects;

  double last_speed;
};


std::shared_ptr<ExcludeObject> exclude_object_load_config(
  std::shared_ptr<ConfigWrapper> config);


}
}