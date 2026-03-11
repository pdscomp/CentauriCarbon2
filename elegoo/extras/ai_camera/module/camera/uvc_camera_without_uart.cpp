/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-05 11:50:31
 * @LastEditors  : Jack
 * @LastEditTime : 2025-09-02 20:57:10
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/


 #include "uvc_camera_without_uart.h"
 #include "camera_uc03.h"
 #include "camera_execption.h"
 #include "spdlog/spdlog.h"
 namespace znp {
 
UvcCamera::UvcCamera() {
    StartMjpgStream();
 }
 
 UvcCamera::~UvcCamera() {
    StopMjpgStream();
    SPDLOG_INFO("UvcCamera Close\n");
 }
 
 void UvcCamera::Start() {
   SPDLOG_INFO("UvcCamera Start\n");
   try {
      // StartMjpgStream();
      // StartH264Stream();
   } catch (const CameraException& e) {
     throw;
   }
 }
 
 void UvcCamera::Stop() {
    SPDLOG_INFO("UvcCamera Stop\n");
    StopMjpgStream();
    // StopH264Stream();
 }
 
void UvcCamera::StartMjpgStream(void)
{
   try {
      SPDLOG_INFO("UvcCamera StartMjpgStream\n");
      if (IsMjpgStreamWorking()) {
        // StopMjpgStream();
        return;
      }

      VideoStreamMjpgInit();
      if (v4l2_MJPG_) v4l2_MJPG_->Start();
   } catch (const CameraException& e) {
     throw;
   }
}
void UvcCamera::StopMjpgStream(void)
{
  SPDLOG_INFO("UvcCamera StopMjpgStream\n");
  if (v4l2_MJPG_) v4l2_MJPG_->Stop();
  VideoStreamMjpgDeinit();
}

void UvcCamera::StartH264Stream(void)
{
   try {
      SPDLOG_INFO("UvcCamera StartH264Stream\n");
      if (IsH264StreamWorking()) {
        // StopH264Stream();
        SPDLOG_INFO("UvcCamera H264Stream is alive!!!");
        return;
      }

      VideoStreamH264Init();
      if (v4l2_H264_) v4l2_H264_->Start();
   } catch (const CameraException& e) {
     throw;
   }
}
void UvcCamera::StopH264Stream(void)
{
  SPDLOG_INFO("UvcCamera StopH264Stream\n");
  if (v4l2_H264_) v4l2_H264_->Stop();
  VideoStreamH264Deinit();
}

 void UvcCamera::V4l2H264ReceivedCallBack(uint8_t *data, int len) 
 {  // 接收UVC数据,视频帧存入队列缓存区
  if ((!data) || (len < 0)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  if(this->videoListH264_) {
    this->videoListH264_->AppendWriteNode(data, len, FRAME_END);
  }
  else {
    SPDLOG_ERROR("videoListH264_ not init!");
    return;    
  }
}

 void UvcCamera::V4l2MjpgReceivedCallBack(uint8_t *data, int len) 
 {  // 接收UVC数据,视频帧存入队列缓存区
  if ((!data) || (len < 0) || (!this->videoListMjpg_)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  if(this->videoListMjpg_) {
    this->videoListMjpg_->AppendWriteNode(data, len, FRAME_END);
  }
  else {
    SPDLOG_ERROR("videoListMjpg_ not init!");
    return;    
  }
}

  
bool UvcCamera::GetFrameOfMjpeg(unsigned char* pBuf, int* pLen) {
  if ((!pBuf) || (!pLen) || (!this->videoListMjpg_)) {
    SPDLOG_ERROR("error param!");
    return false;
  }
  if (this->videoListMjpg_->ReadOneFrame(pBuf, pLen)) {
      // SPDLOG_ERROR("ReadOneFrame Mjpeg error!");
      return false;
  }

  return true;
}

bool UvcCamera::GetFrameOfH264(unsigned char* pBuf, int* pLen) {
  if ((!pBuf) || (!pLen) || (!this->videoListH264_)) {
    SPDLOG_ERROR("error param!");
    return false;
  }
  if (this->videoListH264_->ReadOneFrame(pBuf, pLen)) {
      SPDLOG_ERROR("ReadOneFrame H264 error!");
      return false;
  }

  return true;
}

bool UvcCamera::VideoStreamMjpgInit(void) {
    if(videoListMjpg_.get() == nullptr) {
      videoListMjpg_ = std::make_shared<VideoList>();
    }
    if (videoListMjpg_->Init()) {
      throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                            "videoList_ Init failed!");
    }   

    if(v4l2_MJPG_.get() == nullptr) {
        v4l2_MJPG_ = std::make_shared<znp::V4L2>(
          uvc_camera_info_mjpg.camera_device_name_.c_str(),
          uvc_camera_info_mjpg.video_queue_frame_width_,
          uvc_camera_info_mjpg.video_queue_frame_hight_,
          uvc_camera_info_mjpg.video_format,
          uvc_camera_info_mjpg.video_fps);
      }
    if (v4l2_MJPG_) {
        v4l2_MJPG_->RegisterVideoReceivedCallBack(
            std::bind(&UvcCamera::V4l2MjpgReceivedCallBack, this, std::placeholders::_1,
                      std::placeholders::_2));
    }
    return true;
}

bool UvcCamera::VideoStreamMjpgDeinit(void) {
    if (v4l2_MJPG_) v4l2_MJPG_.reset(); 
    if (videoListMjpg_) videoListMjpg_.reset();

  return true;
}

bool UvcCamera::VideoStreamH264Init(void) {
    if (!IsDeviceSupportH264()) {
      SPDLOG_ERROR("videoListMjpg_ not init!");
      return false;
    }

    if(!videoListH264_) {
      videoListH264_ = std::make_shared<VideoList>();
    }
    if (videoListH264_->Init()) {
      throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                            "videoList_ Init failed!");
    }   

    if(!v4l2_H264_) {
        v4l2_H264_ = std::make_shared<znp::V4L2>(
          uvc_camera_info_h264.camera_device_name_.c_str(),
          uvc_camera_info_h264.video_queue_frame_width_,
          uvc_camera_info_h264.video_queue_frame_hight_,
          uvc_camera_info_h264.video_format,
          uvc_camera_info_h264.video_fps);
      }
    if (v4l2_H264_) {
        v4l2_H264_->RegisterVideoReceivedCallBack(
            std::bind(&UvcCamera::V4l2H264ReceivedCallBack, this, std::placeholders::_1,
                      std::placeholders::_2));
    }
    return true;
}

bool UvcCamera::VideoStreamH264Deinit(void) {
    if (v4l2_H264_) v4l2_H264_.reset(); 
    if (videoListH264_) videoListH264_.reset();

  return true;
}

bool UvcCamera::IsDeviceSupportH264(void)
{
  struct stat buffer;
  return (stat(uvc_camera_info_h264.camera_device_name_.c_str(), &buffer) == 0);
}

bool UvcCamera::IsMjpgStreamWorking(void)
{
  return v4l2_MJPG_.get() ? true: false;
}

bool UvcCamera::IsH264StreamWorking(void)
{
  return v4l2_H264_.get() ? true: false;
}

}