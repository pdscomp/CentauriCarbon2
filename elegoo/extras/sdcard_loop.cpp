/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-11 15:44:36
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-23 12:11:55
 * @Description  : Sdcard file looping support
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "sdcard_loop.h"
#include "logger.h"

namespace elegoo {
namespace extras {
const std::string cmd_SDCARD_LOOP_BEGIN_help =
    "Begins a looped section in the SD file.";
const std::string cmd_SDCARD_LOOP_END_help =
    "Ends a looped section in the SD file.";
const std::string cmd_SDCARD_LOOP_DESIST_help =
    "Stops iterating the current loop stack.";

SDCardLoop::SDCardLoop(std::shared_ptr<ConfigWrapper> config) {
SPDLOG_INFO("SDCardLoop init!");
  auto printer = config->get_printer();
  this->sdcard = any_cast<std::shared_ptr<VirtualSD>>(printer->load_object(config, "virtual_sdcard"));

  this->gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));

  this->gcode->register_command("SDCARD_LOOP_BEGIN",
                                [this](std::shared_ptr<GCodeCommand> gcmd) {
                                  cmd_SDCARD_LOOP_BEGIN(gcmd);
                                },
                                false, cmd_SDCARD_LOOP_BEGIN_help);

  this->gcode->register_command(
      "SDCARD_LOOP_END",
      [this](std::shared_ptr<GCodeCommand> gcmd) { cmd_SDCARD_LOOP_END(gcmd); },
      false, cmd_SDCARD_LOOP_END_help);

  this->gcode->register_command("SDCARD_LOOP_DESIST",
                                [this](std::shared_ptr<GCodeCommand> gcmd) {
                                  cmd_SDCARD_LOOP_DESIST(gcmd);
                                },
                                false, cmd_SDCARD_LOOP_DESIST_help);
  this->loop_stack.clear();
SPDLOG_INFO("SDCardLoop init success!!");
}

SDCardLoop::~SDCardLoop() {}

void SDCardLoop::cmd_SDCARD_LOOP_BEGIN(std::shared_ptr<GCodeCommand> gcmd) {
  int count = gcmd->get_int("COUNT", INT_NONE, 0);
  if (!loop_begin(count)) {
    SPDLOG_INFO("Only permitted in SD file.");
    return;
    throw elegoo::common::CommandError("Only permitted in SD file.");
  }
}

void SDCardLoop::cmd_SDCARD_LOOP_END(std::shared_ptr<GCodeCommand> gcmd) {
  if (!loop_end()) {
    SPDLOG_INFO("Only permitted in SD file.");
    return;
    throw elegoo::common::CommandError("Only permitted in SD file.");
  }
}

void SDCardLoop::cmd_SDCARD_LOOP_DESIST(std::shared_ptr<GCodeCommand> gcmd) {
  if (!loop_desist()) {
    throw elegoo::common::CommandError("Only permitted outside of a SD file.");
  }
}

bool SDCardLoop::loop_begin(int count) {
  if (!this->sdcard->is_cmd_from_sd()) {
    return false;
  }
  this->loop_stack.push_back({count, sdcard->get_file_position()});
  return true;
}

bool SDCardLoop::loop_end() {
  if (!this->sdcard->is_cmd_from_sd()) {
    return false;
  }
  if (loop_stack.size() == 0) {
    return true;
  }
  std::pair<int, int> data = this->loop_stack.back();
  int count = data.first;
  int position = data.second;
  if (count == 0) {
    this->sdcard->set_file_position(position);
    this->loop_stack.push_back({0, position});
  } else if (count == 1) {
  } else {
    this->sdcard->set_file_position(position);
    this->loop_stack.push_back({count - 1, position});
  }
  return true;
}

bool SDCardLoop::loop_desist() {
  if (this->sdcard->is_cmd_from_sd()) {
    return false;
  }
  SPDLOG_INFO("Desisting existing SD loops");
  loop_stack.clear();
  return true;
}

std::shared_ptr<SDCardLoop> sdcard_loop_load_config(std::shared_ptr<ConfigWrapper> config) {
  return std::make_shared<SDCardLoop>(config);
}


}
}