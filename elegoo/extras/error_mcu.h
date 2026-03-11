/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-05 11:48:29
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-27 18:33:37
 * @Description  : More verbose information on micro-controller errors
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "configfile.h"
#include "printer.h"

namespace elegoo {
namespace extras {
class PrinterMCUError {
 public:
  PrinterMCUError(std::shared_ptr<ConfigWrapper> config);
  void add_clarify(
      const std::string &msg,
      std::function<std::string(const std::string &,
                                const std::map<std::string, std::string> &)>
          callback);
  void _check_mcu_shutdown(const std::string &msg,
                           const std::map<std::string, std::string> &details);
  void _handle_notify_mcu_shutdown(
      const std::string &msg,
      const std::map<std::string, std::string> &details);
  void _check_protocol_error(const std::string &msg,
                             const std::map<std::string, std::string> &details);
  void _check_mcu_connect_error(
      const std::string &msg,
      const std::map<std::string, std::string> &details);
  void _handle_notify_mcu_error(
      const std::string &msg,
      const std::map<std::string, std::string> &details);

 private:
  std::shared_ptr<Printer> printer;
  std::map<std::string,
      std::vector<std::function<std::string(
          const std::string &, const std::map<std::string, std::string> &)>>>
      clarify_callbacks;
};


std::shared_ptr<PrinterMCUError> error_mcu_load_config(
    std::shared_ptr<ConfigWrapper> config);

}
}