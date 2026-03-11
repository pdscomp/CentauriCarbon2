/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-05 11:48:29
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-30 12:02:00
 * @Description  : More verbose information on micro-controller errors
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "error_mcu.h"
#include "logger.h"
#include "utilities.h"
namespace elegoo {
namespace extras {

const std::string message_shutdown = R"(Once the underlying issue is corrected,
use the "FIRMWARE_RESTART" command to reset the firmware, 
reload the config, and restart the host software. 
Printer is shutdown )";

const std::string message_protocol_error1 = R"(
This is frequently caused by running an older version of the
firmware on the MCU(s). Fix by recompiling and flashing the
firmware.
)";

const std::string message_protocol_error2 = R"(
Once the underlying issue is corrected, use the "RESTART"
command to reload the config and restart the host software.
)";

const std::string message_mcu_connect_error = R"(
Once the underlying issue is corrected, use the
"FIRMWARE_RESTART" command to reset the firmware, reload the
config, and restart the host software.
Error configuring printer
)";

const std::map<std::tuple<std::string>, std::string> Common_MCU_errors = {
    {std::make_tuple("Timer too close"), R"(
This often indicates the host computer is overloaded. Check
for other processes consuming excessive CPU time, high swap
usage, disk errors, overheating, unstable voltage, or
similar system problems on the host computer.)"},
    {std::make_tuple("Missed scheduling of next "), R"(
This is generally indicative of an intermittent
communication failure between micro-controller and host.)"},
    {std::make_tuple("ADC out of range"), R"(
This generally occurs when a heater temperature exceeds
its configured min_temp or max_temp.)"},
    {std::make_tuple("Rescheduled timer in the past"), R"(
This generally occurs when the micro-controller has been
requested to step at a rate higher than it is capable of
obtaining.)"},
    {std::make_tuple("Command request"), R"(
This generally occurs in response to an M112 G-Code command
or in response to an internal error in the host software.)"}};

std::string error_hint(const std::string& msg) {
  for (const auto& entry : Common_MCU_errors) {
    const auto& prefix = std::get<0>(entry.first);
    const auto& help_msg = entry.second;
    if (msg.find(prefix) == 0) {
      return help_msg;
    }
  }
  return "";
}

std::string join(const std::vector<std::string>& vec,
                 const std::string& delimiter) {
  std::ostringstream oss;
  for (size_t i = 0; i < vec.size(); ++i) {
    if (i != 0) {
      oss << delimiter;
    }
    oss << vec[i];
  }
  return oss.str();
}

PrinterMCUError::PrinterMCUError(std::shared_ptr<ConfigWrapper> config) {
  printer = config->get_printer();

  elegoo::common::SignalManager::get_instance().register_signal(
      "elegoo:notify_mcu_shutdown",
      std::function<void(std::string, std::map<std::string, std::string>)>(
          [this](std::string msg, std::map<std::string, std::string> details) {
            SPDLOG_DEBUG("elegoo:notify_mcu_shutdown !");
            _handle_notify_mcu_shutdown(msg, details);
            SPDLOG_DEBUG("elegoo:notify_mcu_shutdown !");
          }));
  elegoo::common::SignalManager::get_instance().register_signal(
      "elegoo:notify_mcu_error",
      std::function<void(std::string, std::map<std::string, std::string>)>(
          [this](std::string msg, std::map<std::string, std::string> details) {
            _handle_notify_mcu_error(msg, details);
          }));

  SPDLOG_INFO("create PrinterMCUError success!");
}

void PrinterMCUError::add_clarify(
    const std::string& msg,
    std::function<std::string(const std::string&,
                              const std::map<std::string, std::string>&)>
        callback) {
  clarify_callbacks[msg].push_back(callback);
}

void PrinterMCUError::_check_mcu_shutdown(
    const std::string& msg, const std::map<std::string, std::string>& details) {
  std::string mcu_name;
  if (details.find("mcu") != details.end()) {
    mcu_name = details.at("mcu");
  }

  std::string mcu_msg;
  if (details.find("reason") != details.end()) {
    mcu_msg = details.at("reason");
  }

  std::string event_type;
  if (details.find("event_type") != details.end()) {
    event_type = details.at("event_type");
  }
  std::string prefix = "MCU " + mcu_name + " shutdown: ";

  if (event_type == "is_shutdown") {
    prefix = "Previous MCU " + mcu_name + " shutdown: ";
  }

  auto hint = error_hint(mcu_msg);
  std::vector<std::string> clarify;
  auto it = clarify_callbacks.find(mcu_msg);
  if (it != clarify_callbacks.end()) {
    for (const auto& cb : it->second) {
      std::string cm = cb(msg, details);
      if (!cm.empty()) {
        clarify.push_back(cm);
      }
    }
  }

  std::string clarify_msg = "";
  if (!clarify.empty()) {
    clarify_msg = "\n\n" + join(clarify, "\n") + "\n";
  }

  std::string newmsg = prefix + mcu_msg + clarify_msg + hint + message_shutdown;
  SPDLOG_INFO("__func__:{},msg:{},prefix:{},newmsg:{}",__func__,msg,prefix,newmsg);
  printer->update_error_msg(msg, newmsg);
}

void PrinterMCUError::_handle_notify_mcu_shutdown(
    const std::string& msg, const std::map<std::string, std::string>& details) {
  if (msg == "MCU shutdown") {
    _check_mcu_shutdown(msg, details);
  } else {
    printer->update_error_msg(msg, (msg + message_shutdown));
  }
SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
}

void PrinterMCUError::_check_protocol_error(
    const std::string& msg, const std::map<std::string, std::string>& details) {
  std::string host_version;
  auto start_args = printer->get_start_args();
  if (start_args.find("software_version") != start_args.end()) {
    host_version = printer->get_start_args().at("software_version");
  }
  std::vector<std::string> msg_update;
  std::vector<std::string> msg_updated;
  std::string mcu_version;
  for (const auto& f : printer->lookup_objects("mcu")) {
    try {
      mcu_version = (any_cast<std::shared_ptr<MCU>>(f.second))
                        ->get_status()["mcu_version"];
    } catch (const std::exception& e) {
      SPDLOG_ERROR("Unable to retrieve mcu_version from mcu");
      continue;
    }

    if (mcu_version != host_version) {
      msg_update.push_back(common::split(f.first, " ").back() + ": Current version " +
                           mcu_version);
    } else {
      msg_updated.push_back(common::split(f.first, " ").back() + ": Current version " +
                            mcu_version);
    }
  }
  if (msg_update.empty()) {
    msg_update.push_back("<none>");
  }
  if (msg_updated.empty()) {
    msg_updated.push_back("<none>");
  }

  std::vector<std::string> newmsg = {"MCU Protocol error",
                                     message_protocol_error1,
                                     "Your Elegoo version is: " + host_version,
                                     "MCU(s) which should be updated:"};

  for (const auto& _msg : msg_update) {
    newmsg.push_back(_msg);
  }
  newmsg.push_back("Up-to-date MCU(s):");
  for (const auto& _msg : msg_updated) {
    newmsg.push_back(_msg);
  }
  newmsg.push_back(message_protocol_error2);
  std::string error_msg;
  if (details.find("error") != details.end()) {
    error_msg = details.at("error");
  }
  newmsg.push_back(error_msg);

  printer->update_error_msg(msg, join(newmsg, "\n"));
SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
}

void PrinterMCUError::_check_mcu_connect_error(
    const std::string& msg, const std::map<std::string, std::string>& details) {
  std::string error_msg;
  if (details.find("error") != details.end()) {
    error_msg = details.at("error");
  }
  std::string newmsg = error_msg + message_mcu_connect_error;
  printer->update_error_msg(msg, newmsg);
  SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
}

void PrinterMCUError::_handle_notify_mcu_error(
    const std::string& msg, const std::map<std::string, std::string>& details) {
  if (msg == "Protocol error") {
    _check_protocol_error(msg, details);
  } else if (msg == "MCU error during connect") {
    _check_mcu_connect_error(msg, details);
  }
SPDLOG_INFO("{} : {}___ ok", __FUNCTION__, __LINE__);
}


std::shared_ptr<PrinterMCUError> error_mcu_load_config(
    std::shared_ptr<ConfigWrapper> config) {
  return std::make_shared<PrinterMCUError>(config);


}
}
}