/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-10-31 21:52:39
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-04 17:53:54
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "control.h"
#include "execption/camera_execption.h"
#include "spdlog/spdlog.h"
#include "comm_protocol.h"
namespace znp {

Control::Control(std::string socket_path) {
  socket_path_ = socket_path;
  timer_loop_ = std::make_shared<hv::EventLoop>();

  server_socket_ = std::make_shared<ServerSocket>(socket_path,
      std::bind(&Control::OnPairMessageReceived, this, std::placeholders::_1));

  timer_loop_->setInterval(500,
          std::bind(&Control::CameraStatusMonitor, this));    // 每隔500ms 检测设备文件是否存在
  SPDLOG_INFO("Control create ok");
}

Control::~Control() {
  if (timer_loop_) {
    this->timer_loop_->stop();
  }
  this->Stop();
}

void Control::Start() {
  isCameraDevice = true;

  ai_camera_ = std::make_shared<znp::AICamera<znp::UvcCamera>>(timer_loop_.get());

  RegisterCmdHandler(
    [this](json & j_request, json & j_respond) {
      if (this->ai_camera_) {
        return this->ai_camera_->CmdHandle(j_request, j_respond);
      }
    });

  this->ai_camera_->RegisterRespondFun(
    [this](const std::string& message) {
      if (this->server_socket_) {
        return this->server_socket_->sendMsg(message);
      }
    }
  );

  if (ai_camera_) ai_camera_->Start();
}

void Control::Stop() {
  if (ai_camera_) {
    isCameraDevice = false;
    ai_camera_.reset();
    this->camera_handler_ = nullptr;
    SPDLOG_INFO("Control Stop ai_camera" );
  }
}

void Control::Loop() {
  if (timer_loop_) {
    this->timer_loop_->run();
    SPDLOG_INFO("{}::{} run ", __FUNCTION__, __LINE__);
  }
}

void Control::CameraStatusMonitor() {
  bool status = CommonTools::isFileExists("/dev/video0");
  if (status!= isCameraDevice) {
    if (status) {
      Start();
      SPDLOG_WARN("Camera device insertion!");
      json jsonResponse;
      MessageCameraInsert(jsonResponse);
      std::string jsonString = MessageParseString(jsonResponse);
      this->server_socket_->sendMsg(jsonString);
    }
    else {
      Stop();
      SPDLOG_WARN("Camera device pull out!");
      json jsonResponse;
      MessageCameraPullOut(jsonResponse);
      std::string jsonString = MessageParseString(jsonResponse);
      this->server_socket_->sendMsg(jsonString);
    }
  }
}

void Control::MessageCmdParse(json &jsonRequest, json &jsonResponse) {
  if (!ai_camera_) {
    SPDLOG_ERROR("No AiCamera!");
    MessageCameraNotFound(jsonResponse);
    return;
  }

  jsonResponse["id"] = jsonRequest["id"];
  jsonResponse["method"] = jsonRequest["method"];
  if (this->camera_handler_) {
    this->camera_handler_(jsonRequest, jsonResponse);
  }
}

void Control::RegisterCmdHandler(Handler handler)
{
  if (handler != nullptr) {
    this->camera_handler_ = handler;
  }
}

void Control::OnPairMessageReceived(const std::string &message) {
  if (message.empty()) {
    return;
  }
  try {
    json jsonRequest = nlohmann::json::parse(message);
    json jsonResponse;
    MessageCmdParse(jsonRequest, jsonResponse);
    std::string jsonString = MessageParseString(jsonResponse);
    this->server_socket_->sendMsg(jsonString);
  }
  catch (const std::exception &e) {
    SPDLOG_ERROR("Parse json error: {}", e.what());
    json jsonResponse;
    MessageCmdNotFound(jsonResponse);
    std::string jsonString = MessageParseString(jsonResponse);
    this->server_socket_->sendMsg(jsonString);
  }

}

}  // namespace znp
