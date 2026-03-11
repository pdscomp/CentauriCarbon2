/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-25 10:09:42
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-30 10:48:29
 * @Description  : Helper script to adjust parameters based on Z level
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "tuning_tower.h"
#include <cmath>
#include "utilities.h"

const double CANCEL_Z_DELTA = 2.0;

namespace elegoo {
namespace extras {

TuningTower::TuningTower(std::shared_ptr<ConfigWrapper> config){
  SPDLOG_INFO("TuningTower init!");
  move_transform = std::make_shared<GCodeMoveTransform>();
  move_transform->move_with_transform = std::bind(&TuningTower::move, this, std::placeholders::_1, std::placeholders::_2);
  move_transform->position_with_transform = std::bind(&TuningTower::get_position, this);




  this->printer = config->get_printer();
  this->normal_transform = nullptr;
  this->last_z = -99999999.9;
  this->last_command_value = 0.0;
  auto gcode_move = any_cast<std::shared_ptr<GCodeMove>>(
      printer->load_object(config, "gcode_move"));

  gcode =
      any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
  this->gcode->register_command(
      "TUNING_TOWER",
      [this](std::shared_ptr<GCodeCommand> gcmd) { cmd_TUNING_TOWER(gcmd); },
      false, "Tool to adjust a parameter at each Z height");
SPDLOG_INFO("TuningTower init success!!");
}

void TuningTower::cmd_TUNING_TOWER(std::shared_ptr<GCodeCommand> gcmd) {
  if (normal_transform != nullptr) {
    end_test();
  }

  auto command = gcmd->get("COMMAND");
  auto parameter = gcmd->get("PARAMETER");

  auto start = gcmd->get_double("START", 0);
  auto factor = gcmd->get_double("FACTOR", 0);
  auto band = gcmd->get_double("BAND", 0,0);
  auto step_delta = gcmd->get_double("STEP_DELTA", 0);
  auto step_height = gcmd->get_double("STEP_HEIGHT", 0,0);
  auto skip = gcmd->get_double("SKIP", 0,0);

  using namespace elegoo::common;
  if (!elegoo::common::are_equal(factor, 0.0) &&
      (!elegoo::common::are_equal(step_height, 0.0) ||
       !elegoo::common::are_equal(step_delta, 0.0))) {
    throw CommandError("Cannot specify both FACTOR and STEP_DELTA/STEP_HEIGHT");
    return;
  }
  if ((!elegoo::common::are_equal(step_delta, 0.0)) !=
      (!elegoo::common::are_equal(step_height, 0.0))) {
    throw CommandError("Must specify both STEP_DELTA and STEP_HEIGHT");
    return;
  }

  if (gcode->is_traditional_gcode(command)) {
    command_fmt = command + " " + parameter + "%.9f";
  } else {
    command_fmt = command + " " + parameter + "=%.9f";
  }
  normal_transform = gcode_move->set_move_transform(move_transform, true);

  last_z = -99999999.9;
  last_command_value = 0.0;
  get_position();
  std::vector<std::string> message_parts;
  message_parts.push_back("start=" + std::to_string(start));
  if (factor) {
    message_parts.push_back("factor=" + std::to_string(factor));
    if (band) {
      message_parts.push_back("band=" + std::to_string(band));
    }
  } else {
    message_parts.push_back("step_delta=" + std::to_string(step_delta));
    message_parts.push_back("step_height=" + std::to_string(step_height));
  }
  if (skip) {
    message_parts.push_back("skip=" + std::to_string(skip));
  }
  gcmd->respond_info(
      "Starting tuning test (" + elegoo::common::join(message_parts, " ") + ")",
      true);
}

std::vector<double> TuningTower::get_position() {
  auto pos = normal_transform->position_with_transform();
  last_position = pos;
  return pos;
}

double TuningTower::calc_value(double z) {
  using namespace elegoo::common;

  if (!elegoo::common::are_equal(skip, 0.0)) {
    z = std::max(0.0, z - skip);
  }
  if (!elegoo::common::are_equal(step_height, 0.0)) {
    return start + step_delta * std::floor(z / step_height);
  }
  if (!elegoo::common::are_equal(band, 0.0)) {
    z = (std::floor(z / band) + 0.5) * band;
  }
  return start + z * factor;
}

void TuningTower::move(std::vector<double>& newpos, double speed) {
  if (newpos[3] > last_position[3] && newpos[2] != last_z &&
      newpos != last_position) {
    double z = newpos[2];
    if (z < last_z - CANCEL_Z_DELTA) {
      end_test();
    } else {
      double gcode_z = 0.0, newval = 0.0;
      gcode_z = gcode_move->get_status()["gcode_position"][2];
      newval = calc_value(gcode_z);
      last_z = z;
      if (newval != last_command_value) {
        last_command_value = newval;
        gcode->run_script_from_command(command_fmt + std::to_string(newval));
      }
    }
  }
  last_position = newpos;
  normal_transform->move_with_transform(newpos, speed);
}

void TuningTower::end_test() 
{
  gcode->respond_info("Ending tuning test mode");
  gcode_move->set_move_transform(normal_transform, true);
  normal_transform.reset();
}

bool TuningTower::is_active() const { return normal_transform != nullptr; }


std::shared_ptr<TuningTower> tuning_tower_load_config(std::shared_ptr<ConfigWrapper> config) 
{
  return std::make_shared<TuningTower>(config);
}

}
}
