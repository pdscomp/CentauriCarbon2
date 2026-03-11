/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-05 11:48:29
 * @LastEditors  : Jack
 * @LastEditTime : 2025-01-18 14:43:12
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "rtsp_streamer.h"
#include "camera_execption.h"
#include "ai_camera_pthread.h"
#include "spdlog/spdlog.h"
namespace znp {

RtspStreamer::RtspStreamer(std::string ip, int port)
    : rtsp_ip_(ip),
      port_(port){
  this->rtsp_url_ =
      "rtsp://" + rtsp_ip_ + ":" + std::to_string(port_) + "/" + "live";
  this->frame_buf_ = new unsigned char[RTSP_FRAME_MAX_SIZE];
}
RtspStreamer::~RtspStreamer() { 
  this->Stop();
  if (rtsp_server_) {
    delete rtsp_server_;
    rtsp_server_ = nullptr;
  }
  if (frame_buf_) {
    delete [] frame_buf_;
    frame_buf_ = nullptr;
  }
  SPDLOG_WARN("RtspStreamer over!");
}

void RtspStreamer::Start() {
  if (eventLoop_ == nullptr) {
    SPDLOG_WARN("eventLoop_ init!");
    eventLoop_ = std::make_shared<xop::EventLoop>();     
  }
  if (!this->is_push_frame_alive_) {
    this->is_rtsp_server_alive_ = true;
    CreateNewThread([this]() { RtspServerThread();},  1*1024*1024, "RtspServerThread");
  } else {
    SPDLOG_WARN("RtspStreamer is running!");
  }
}
void RtspStreamer::Stop() {
  if (this->is_push_frame_alive_ && eventLoop_) {
    SPDLOG_WARN("RtspStreamer Exitting!");
    this->is_push_frame_alive_ = false;
    this->eventLoop_->quit();
    SPDLOG_WARN("RtspStreamer Exitting OK!");
  } else {
    SPDLOG_WARN("RtspStreamer not running!");
  }
}
void RtspStreamer::SetServerIP(const std::string ip) {
  rtsp_ip_ = ip;
  rtsp_url_ = "rtsp://" + rtsp_ip_ + ":" + std::to_string(port_) + "/" + "live";
  SPDLOG_INFO("new rtsp_url_:", rtsp_url_);
}
const std::string &RtspStreamer::GetServerIP() { return this->rtsp_ip_; }
void RtspStreamer::GetServerInfo(std::string &ip, std::string &url) {
  ip = this->rtsp_ip_;
  url = this->rtsp_url_;
}

void RtspStreamer::RegisterGetFarmeCallBack(GetFarmeCallBack callback) {
  if (!this->get_frame_callback_) {
    this->get_frame_callback_ = callback;
  }
}

void RtspStreamer::RtspServerThread() {
  try {
    rtsp_server_ = new xop::RtspServer(eventLoop_.get(), rtsp_ip_.c_str(), RTSP_PORT);
    xop::MediaSession *session = xop::MediaSession::createNew("live");
    session->addMediaSource(xop::channel_0, xop::H264Source::createNew());
    sessionId_ = rtsp_server_->addMeidaSession(session);

    this->is_push_frame_alive_ = true;
    SPDLOG_INFO("sessionId:{}", sessionId_);
    CreateNewThread([this]() { VideoStreamerThread();},  1*1024*1024, "RTSPPushFrameThread");
    SPDLOG_INFO("start RtspServer!");
  } catch (const CameraException &e) {
    SPDLOG_ERROR("RtspServer init failed!");
    throw;
  }
  if (this->eventLoop_) {
    this->eventLoop_->loop();
  }
  SPDLOG_ERROR("RtspServer exit!!!");
}

void RtspStreamer::VideoStreamerThread() {
  if (!this->get_frame_callback_ || !rtsp_server_) {
    SPDLOG_ERROR("get_frame_callback_ no Register");
    return;
  }
  SPDLOG_INFO("Started rtsp streaming! rtspurl:{}", rtsp_url_);
  xop::AVFrame videoFrame = {0};
  SPDLOG_INFO("sessionId_ : {} videoFrame init: size {}, timestamp {}", sessionId_, frame_len_, videoFrame.timestamp);
  while (this->is_push_frame_alive_ && this->is_rtsp_server_alive_) {
    if (!this->get_frame_callback_(frame_buf_, &frame_len_)) {
      xop::Timer::sleep(20);
      continue;
    }
    videoFrame.type = 0;
    videoFrame.size = frame_len_;
    videoFrame.timestamp = xop::H264Source::getTimeStamp();
    videoFrame.buffer.reset(new uint8_t[videoFrame.size]);
    memcpy(videoFrame.buffer.get(), frame_buf_,
           videoFrame.size);
    if (!rtsp_server_->pushFrame(sessionId_, xop::channel_0, videoFrame) ) {
    }
    // SPDLOG_INFO("frame_len_: {} timestamp: {}", frame_len_, videoFrame.timestamp);
    xop::Timer::sleep(1000/fps_);
  }
  SPDLOG_INFO("rtsp push streaming! over!");
}
}
