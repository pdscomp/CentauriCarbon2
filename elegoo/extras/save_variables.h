/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-18 16:08:16
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:57:24
 * @Description  : Save arbitrary variables so that values can be kept across
 *restarts
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "configfile.h"
#include "gcode.h"
#include "mcu.h"
#include "printer.h"

namespace elegoo {
namespace extras {
class SaveVariables {
 public:
  SaveVariables(std::shared_ptr<ConfigWrapper> config);
  void loadVariables();
  void cmd_SAVE_VARIABLE(std::shared_ptr<GCodeCommand> gcmd);
  json get_status(double eventtime);

 private:
  std::shared_ptr<Printer> printer;
  std::string filename;
  std::map<std::string, std::string> allVariables;
};


std::shared_ptr<SaveVariables> save_variables_load_config(
    std::shared_ptr<ConfigWrapper> config);


}
}