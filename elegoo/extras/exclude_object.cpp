/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-16 15:54:02
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:13:19
 * @Description  : Exclude moves toward and inside objects
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "exclude_object.h"
#include "logger.h"
#include "utilities.h"
namespace elegoo {
namespace extras {
const std::string cmd_EXCLUDE_OBJECT_START_help =
    "Marks the beginning the current object as labeled";
const std::string cmd_EXCLUDE_OBJECT_END_help =
    "Marks the end the current object";
const std::string cmd_EXCLUDE_OBJECT_help =
    "Cancel moves inside a specified objects";
const std::string cmd_EXCLUDE_OBJECT_DEFINE_help =
    "Provides a summary of an object";

ExcludeObject::ExcludeObject(std::shared_ptr<ConfigWrapper> config) {
  SPDLOG_INFO("ExcludeObject init!");
  move_transform = std::make_shared<GCodeMoveTransform>();
  move_transform->move_with_transform = std::bind(&ExcludeObject::move, this, std::placeholders::_1, std::placeholders::_2);
  move_transform->position_with_transform = std::bind(&ExcludeObject::get_position, this);
  this->printer = config->get_printer();
  this->gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
  
  this->gcode_move = any_cast<std::shared_ptr<GCodeMove>>(printer->load_object(config, "gcode_move"));

  elegoo::common::SignalManager::get_instance().register_signal(
    "elegoo:connect",
      std::function<void()>([this](){
          SPDLOG_DEBUG("ExcludeObject connect~~~~~~~~~~~~~~~~~");
          _handle_connect();
          SPDLOG_DEBUG("ExcludeObject connect~~~~~~~~~~~~~~~~~ success!");
      })
  );
  elegoo::common::SignalManager::get_instance().register_signal(
    "virtual_sdcard:reset_file",
      std::function<void()>([this](){
          _reset_file();
      })
  );

  this->next_transform = nullptr;

  this->_reset_state();
  this->gcode->register_command("EXCLUDE_OBJECT_START",
                                [this](std::shared_ptr<GCodeCommand> gcmd) {
                                  cmd_EXCLUDE_OBJECT_START(gcmd);
                                },
                                false, cmd_EXCLUDE_OBJECT_START_help);

  this->gcode->register_command("EXCLUDE_OBJECT_END",
                                [this](std::shared_ptr<GCodeCommand> gcmd) {
                                  cmd_EXCLUDE_OBJECT_END(gcmd);
                                },
                                false, cmd_EXCLUDE_OBJECT_END_help);

  this->gcode->register_command("EXCLUDE_OBJECT",
                                [this](std::shared_ptr<GCodeCommand> gcmd) {
                                  cmd_EXCLUDE_OBJECT(gcmd);
                                },
                                false, cmd_EXCLUDE_OBJECT_help);

  this->gcode->register_command("EXCLUDE_OBJECT_DEFINE",
                                [this](std::shared_ptr<GCodeCommand> gcmd) {
                                  cmd_EXCLUDE_OBJECT_DEFINE(gcmd);
                                },
                                false, cmd_EXCLUDE_OBJECT_DEFINE_help);
SPDLOG_INFO("ExcludeObject init success!");
}

ExcludeObject::~ExcludeObject() {}

void ExcludeObject::_register_transform() {
  if (next_transform == nullptr) {
    std::shared_ptr<TuningTower> tuning_tower =
        any_cast<std::shared_ptr<TuningTower>>(
            printer->lookup_object("tuning_tower"));
    if (tuning_tower->is_active()) {
      SPDLOG_INFO(
          "The ExcludeObject move transform is not being loaded due to Tuning "
          "tower being Active");
    }
    return;
  }

  next_transform = gcode_move->set_move_transform(move_transform, true);
  extrusion_offsets.clear();
  max_position_extruded = 0.0;
  max_position_excluded = 0.0;
  extruder_adj = 0.0;
  initial_extrusion_moves = 5;

  std::fill_n(last_position.begin(), 4, 0.0f);
  last_position_extruded = last_position;
  last_position_excluded = last_position;
}

void ExcludeObject::_handle_connect() {
  this->toolhead =
      any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
}

void ExcludeObject::_unregister_transform() {
  if (next_transform != nullptr) {
    auto tuning_tower = any_cast<std::shared_ptr<TuningTower>>(
        printer->lookup_object("tuning_tower"));
    if (tuning_tower->is_active()) {
      SPDLOG_ERROR(
          "The Exclude Object move transform was not unregistered because it "
          "is not at the head of the transform chain.");
      return;
    }
    gcode_move->set_move_transform(next_transform, true);
    next_transform.reset();
    gcode_move->reset_last_position();  
  }
}

void ExcludeObject::_reset_state() {
  objects.clear();
  excluded_objects.clear();
  current_object.clear();
  in_excluded_region = false;
}

void ExcludeObject::_reset_file() {
  this->_reset_state();
  this->_unregister_transform();
}

std::vector<double> ExcludeObject::_get_extrusion_offsets() {
  auto offset =
      this->extrusion_offsets.find(toolhead->get_extruder()->get_name());
  if (offset == extrusion_offsets.end()) {
    std::vector<double> offset_temp{0.0, 0.0, 0.0, 0.0};
    extrusion_offsets[toolhead->get_extruder()->get_name()] = offset_temp;
    return offset_temp;
  } else {
    return offset->second;
  }
}

std::vector<double> ExcludeObject::get_position() {
  auto offset = this->_get_extrusion_offsets();
  auto pos = this->next_transform->position_with_transform();

  for (int i = 0; i < 4; i++) {
    last_position[i] = pos[i] + offset[i];
  }
  return this->last_position;
}

void ExcludeObject::_normal_move(std::vector<double> newpos, double speed) {
  using namespace elegoo::common;
  auto offset = this->_get_extrusion_offsets();
  if ((this->initial_extrusion_moves > 0) && (last_position[3] != newpos[3])) {
    initial_extrusion_moves -= 1;
  }
  last_position = newpos;
  last_position_extruded = last_position;
  max_position_extruded =
      (max_position_extruded > newpos[3]) ? max_position_extruded : newpos[3];

  if ((offset[0] != 0 || offset[1] != 0) &&
          (!elegoo::common::are_equal(newpos[0], last_position_excluded[0])) ||
      (!elegoo::common::are_equal(newpos[1], last_position_excluded[1]))) {
    offset[0] = 0.0;
    offset[1] = 0.0;
    offset[2] = 0.0;
    offset[3] += this->extruder_adj;
    this->extruder_adj = 0.0;
  }

  if ((!elegoo::common::are_equal(offset[2], 0)) &&
      (!elegoo::common::are_equal(newpos[2], this->last_position_excluded[2]))) {
    offset[2] = 0;
  }

  if ((!elegoo::common::are_equal(extruder_adj, 0)) &&
      (!elegoo::common::are_equal(newpos[3], this->last_position_excluded[3]))) {
    offset[3] += extruder_adj;
    extruder_adj = 0;
  }

  auto tx_pos = newpos;
  for (int i = 0; i < 4; i++) {
    tx_pos[i] = newpos[i] - offset[i];
  }
  next_transform->move_with_transform(tx_pos, speed);
}

void ExcludeObject::_ignore_move(std::vector<double> newpos, double speed) {
  auto offset = this->_get_extrusion_offsets();
  for (int i = 0; i < 3; i++) {
    offset[i] = newpos[i] - last_position_extruded[i];
  }

  offset[3] = offset[3] + newpos[3] - last_position[3];
  last_position = newpos;
  last_position_excluded = last_position;
  max_position_excluded =
      (max_position_excluded > newpos[3]) ? max_position_excluded : newpos[3];
}

void ExcludeObject::_move_into_excluded_region(std::vector<double> newpos,
                                               double speed) {
  in_excluded_region = true;
  _ignore_move(newpos, speed);
}

void ExcludeObject::_move_from_excluded_region(std::vector<double> newpos,
                                               double speed) {
  in_excluded_region = false;
  this->extruder_adj =
      this->max_position_excluded - this->last_position_excluded[3] -
      (this->max_position_extruded - this->last_position_extruded[3]);
  this->_normal_move(newpos, speed);
}

bool ExcludeObject::_test_in_excluded_region() {
  return ((std::find(excluded_objects.begin(), excluded_objects.end(),
                     current_object) != excluded_objects.end()) &&
          (initial_extrusion_moves == 0));
}

json ExcludeObject::get_status(double eventtime) {
  json status;
  json objects_json;
  json object_json;
  for (const auto& object : objects) {
    for (const auto& obj : object) {
      object_json[obj.first] = obj.second;
    }
    objects_json[object.at("name")] = object_json;
  }
  status["objects"] = objects_json;
  for (const auto& obj : excluded_objects) {
    status["excluded_objects"].push_back(obj);
  }
  status["current_object"] = current_object;

  return status;
}

void ExcludeObject::move(std::vector<double> newpos, double speed) {
  bool move_in_excluded_region = _test_in_excluded_region();
  last_speed = speed;

  if (move_in_excluded_region) {
    if (in_excluded_region) {
      _ignore_move(newpos, speed);
    } else {
      _move_into_excluded_region(newpos, speed);
    }
  } else {
    if (in_excluded_region) {
      _move_from_excluded_region(newpos, speed);
    } else {
      _normal_move(newpos, speed);
    }
  }
}

void ExcludeObject::cmd_EXCLUDE_OBJECT_START(
    std::shared_ptr<GCodeCommand> gcmd) {
SPDLOG_INFO("{} : {}___", __FUNCTION__, __LINE__);
  std::string name = gcmd->get("NAME");
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return std::toupper(c); });
  bool found =
      std::any_of(objects.begin(), objects.end(),
                  [&name](const std::map<std::string, std::string>& obj) {
                    return obj.at("name") == name;
                  });

  if (!found) {
    std::map<std::string, std::string> definition;
    definition["name"] = name;
    _add_object_definition(definition);
  }
  current_object = name;
  was_excluded_at_start = _test_in_excluded_region();
SPDLOG_INFO("{} : {}___ ok!", __FUNCTION__, __LINE__);
}

void ExcludeObject::cmd_EXCLUDE_OBJECT_END(std::shared_ptr<GCodeCommand> gcmd) {
SPDLOG_INFO("{} : {}___", __FUNCTION__, __LINE__);
  if (current_object.empty() && next_transform) {
    gcmd->respond_info(
        "EXCLUDE_OBJECT_END called, but no object is  currently active", true);
    return;
  }
  auto name = gcmd->get("NAME", "");
  std::string name_copy = name;
  std::transform(name_copy.begin(), name_copy.end(), name_copy.begin(),
                 ::toupper);
  if ((!name.empty()) && (name_copy != current_object)) {
    gcmd->respond_info("EXCLUDE_OBJECT_END NAME=" + name_copy +
                           " does not match the current object NAME=" +
                           current_object,
                       true);
  }
  current_object.clear();
SPDLOG_INFO("{} : {}___ ok!", __FUNCTION__, __LINE__);
}

void ExcludeObject::cmd_EXCLUDE_OBJECT(std::shared_ptr<GCodeCommand> gcmd) {
SPDLOG_INFO("{} : {}___", __FUNCTION__, __LINE__);
  std::string reset = gcmd->get("RESET", "");
  std::string current = gcmd->get("CURRENT", "");
  std::string name = gcmd->get("NAME", "");
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  if (!reset.empty()) {
    if (!name.empty()) {
      _unexclude_object(name);
    } else {
      excluded_objects.clear();
    }
  } else if (!name.empty()) {
    if (std::find(excluded_objects.begin(), excluded_objects.end(), name) ==
        excluded_objects.end()) {
      _exclude_object(name);
    }
  } else if (!current.empty()) {
    if (current_object.empty()) {
      throw elegoo::common::CommandError("There is no current object to cancel");
    } else {
      _exclude_object(current_object);
    }
  } else {
    _list_excluded_objects(gcmd);
  }
SPDLOG_INFO("{} : {}___ ok!", __FUNCTION__, __LINE__);
}

void ExcludeObject::cmd_EXCLUDE_OBJECT_DEFINE(
    std::shared_ptr<GCodeCommand> gcmd) {
    SPDLOG_INFO("{} : {}___", __FUNCTION__, __LINE__);
  std::string reset = gcmd->get("RESET", "");
  std::string name = gcmd->get("NAME", "");
  std::transform(name.begin(), name.end(), name.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  if (!reset.empty()) {
    _reset_file();
  } else if (!name.empty()) {
    std::map<std::string, std::string> parameters =
        gcmd->get_command_parameters();
    parameters.erase("NAME");
    auto center_it = parameters.find("CENTER");
    auto polygon_it = parameters.find("POLYGON");

    std::map<std::string, std::string> obj;
    obj["name"] = name;
    for (const auto& param : parameters) {
      if (param.first != "CENTER" && param.second != "POLYGON") {
        obj[param.first] = param.second;
      }
    }

    if (center_it != parameters.end()) {
      std::string center_str = center_it->second;
      obj["center"] = "[" + center_str + "]";
    }

    if (polygon_it != parameters.end()) {
      std::string polygon_str = polygon_it->second;
      obj["polygon"] = polygon_str;
    }

    _add_object_definition(obj);
  } else {
    _list_objects(gcmd);
  }
SPDLOG_INFO("{} : {}___ ok!", __FUNCTION__, __LINE__);
}

void ExcludeObject::_add_object_definition(
    std::map<std::string, std::string> definition) {
  objects.push_back(definition);

  std::sort(objects.begin(), objects.end(),
            [](const std::map<std::string, std::string>& a,
               const std::map<std::string, std::string>& b) {
              return a.at("name") < b.at("name");
            });
}

void ExcludeObject::_exclude_object(const std::string& name) {
  this->_register_transform();
  this->gcode->respond_info("Excluding object " + name);

  if (std::find(excluded_objects.begin(), excluded_objects.end(), name) ==
      excluded_objects.end()) {
    excluded_objects.push_back(name);
    std::sort(excluded_objects.begin(), excluded_objects.end());
  }
}

void ExcludeObject::_unexclude_object(const std::string& name) {
  this->gcode->respond_info("Unexcluding object " + name);
  auto it = std::find(excluded_objects.begin(), excluded_objects.end(), name);
  if (it != excluded_objects.end()) {
    excluded_objects.erase(it);
    std::sort(excluded_objects.begin(), excluded_objects.end());
  }
}

void ExcludeObject::_list_objects(std::shared_ptr<GCodeCommand> gcmd) {
  std::string object_list;
  if (!gcmd->get("JSON", "").empty()) {
    object_list = json(objects).dump();
  } else {
    for (const auto& obj : objects) {
      object_list += obj.at("name");
      object_list += " ";
    }
  }
  gcmd->respond_info("Known objects: " + object_list, true);
}

void ExcludeObject::_list_excluded_objects(std::shared_ptr<GCodeCommand> gcmd) {
  std::string object_list;
  for (size_t i = 0; i < excluded_objects.size(); ++i) {
    if (i > 0) {
      object_list += " ";
    }
    object_list += excluded_objects[i];
  }
  gcmd->respond_info("Excluded objects: " + object_list, true);
}

std::shared_ptr<ToolHead> ExcludeObject::get_toolhead() { return toolhead; }

std::shared_ptr<ExcludeObject> exclude_object_load_config(
    std::shared_ptr<ConfigWrapper> config) {
  return std::make_shared<ExcludeObject>(config);

}
}
}