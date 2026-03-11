/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-17 14:40:02
 * @LastEditors  : Jack
 * @LastEditTime : 2025-02-24 14:55:51
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-18 16:08:16
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:57:53
 * @Description  : Save arbitrary variables so that values can be kept across
 *restarts
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "save_variables.h"
#include "logger.h"
namespace elegoo {
namespace extras {
const std::string cmd_SAVE_VARIABLE_help = "Save arbitrary variables to disk";

std::string expandUser(const std::string& path) {
  if (path.substr(0, 1) == "~") {
    const char* home = getenv("HOME");
    if (home) {
      return std::string(home) + path.substr(1);
    }
  }
  return path;
}

SaveVariables::SaveVariables(std::shared_ptr<ConfigWrapper> config) {
SPDLOG_INFO("SaveVariables init!");
  printer = config->get_printer();
  filename = expandUser(config->get("filename"));
  allVariables.clear();
  ;
  try {
    struct stat buffer;
    if (stat(filename.c_str(), &buffer) != 0) {
      std::ofstream file(filename);
      file.close();
    }
    loadVariables();
  } catch (const std::runtime_error& e) {
    elegoo::common::CommandError(std::string(e.what()));
  }
  std::shared_ptr<GCodeDispatch> gcode =
          any_cast<std::shared_ptr<GCodeDispatch>>(
              printer->lookup_object("gcode"));
  gcode->register_command(
      "SAVE_VARIABLE",
      [this](std::shared_ptr<GCodeCommand> gcmd) { cmd_SAVE_VARIABLE(gcmd); },
      false, cmd_SAVE_VARIABLE_help);
SPDLOG_INFO("SaveVariables init success!!");
}

void SaveVariables::loadVariables() {
  std::ifstream file(filename);
  if (!file) {
    throw std::runtime_error("Unable to open file");
  }

  std::map<std::string, std::string> allvars;
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string key, value;
    if (std::getline(iss, key, ':') && std::getline(iss, value)) {
      allvars[key] = value;
    }
  }

  allVariables = allvars;
}

void SaveVariables::cmd_SAVE_VARIABLE(std::shared_ptr<GCodeCommand> gcmd) {
  std::string varname = gcmd->get("VARIABLE");
  std::string value_str = gcmd->get("VALUE");

  std::map<std::string, std::string> newvars = allVariables;
  newvars[varname] = value_str;

  try {
    std::ofstream file(filename);
    if (!file) {
      throw std::runtime_error("Unable to open file for writing");
    }

    for (const auto& item : newvars) {
      file << item.first << ": " << item.second << std::endl;
    }
  } catch (const std::runtime_error& e) {
    std::string msg = "Unable to save variable";
    SPDLOG_ERROR(msg);
    throw elegoo::common::CommandError(msg);
  }

  loadVariables();
}

json SaveVariables::get_status(double eventtime) {
  return {{"variables", allVariables}};
}


std::shared_ptr<SaveVariables> save_variables_load_config(
    std::shared_ptr<ConfigWrapper> config) {
  return std::make_shared<SaveVariables>(config);
}

}
}
