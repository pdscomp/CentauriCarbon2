/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-05 11:48:29
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-30 17:50:26
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <functional>
#include <iostream>
#include <map>

#include "camera_base.h"
// #include "communicator.h"
#include "server_socket.h"
#include "video_streamer.h"
#include "rtc_streamer.h"
#include "uvc_camera_without_uart.h"
#include "aiCamera_cmd.h"
#include "aiCamera_status.h"
#include "time_lapse_photo.h"
#include "video_format_convert.h"
#include "time_lapse_control.h"
#include "json.hpp"
#include "common_tools.h"

#include "hv/Event.h"
#include "hv/EventLoop.h"
#include "hv/hv.h"

#define REAL_TIME_SAVE_VIDEO 0
namespace znp {
using json = nlohmann::json;
template <class CameraT>
class AICamera : public CameraBase {
 public:
  AICamera(hv::EventLoop *loop);
  ~AICamera();

  void Start() override;
  void Stop() override;

  void RegisterRespondFun(RespondMsg respond_msg) override;
  void CmdHandle(json &j_requset, json &j_respond) override;

  void TimeLapseControl(json &j_requset, json &j_respond);  // 延时摄影控制
  void TimeLapseComposite(json &params, json &j_respond);   // 延时摄影合成
  void AiDetection(json &params, json &j_respond);          // Ai检测
  void RtcMonitor(json &params, json &j_respond);           // rtc推流
 private:
  void TimerHandle(hv::TimerID id);

  void TimeLapseStart(json &j_respond, std::string module_name, int t_frame, bool is_continue);
  void TimeLapseSavePicture(json &j_respond, int index);
  void TimeLapseStop(json &j_respond);

  void RtcMonitorStart(json &j_respond, 
                      const std::string& appid, 
                      const std::string& license, 
                      const std::string& token, 
                      const std::string& channel, 
                      int uid);
  void RtcMonitorRefresh(json &j_respond, const std::string &new_token);
  void RtcMonitorStop(json &j_respond);
  void RtcMonitorPause(json &j_respond);
  void RtcMonitorResume(json &j_respond);

 private:
  int timer_delay_ms_{5*1000};

  hv::TimerID time_id;
  std::string video_name_;
  std::string video_stream_ip_;
  unsigned char * pic_buf_{nullptr};
  std::shared_ptr<CameraT> camera_{nullptr};
  std::shared_ptr<VideoStreamer> video_streamer_{nullptr};
  std::shared_ptr<RtcStreamer> rtc_streamer_{nullptr};
#if REAL_TIME_SAVE_VIDEO 
  std::shared_ptr<VideoFormatConvert> video_converter_{nullptr};   // 一边拍摄一边合成延时摄影视频时使用
#else
  std::shared_ptr<TimeLapseVideo> time_lapse_video_{nullptr};   // 先抓拍照片,最后合成延时摄影视频时使用
#endif
  RespondMsg respond_msg_;
  hv::EventLoop *loop_{nullptr};
  
  AICameraStatus aiCameraInfo_;
  std::mutex jpeg_write_mutex_;  // 保存JPEG图片互斥锁
};

}  // namespace znp
