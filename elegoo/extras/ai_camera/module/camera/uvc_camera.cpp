/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-11-05 11:50:31
 * @LastEditors  : Ben
 * @LastEditTime : 2025-04-10 12:23:46
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/


#include "uvc_camera.h"
#include "camera_uc03.h"
#include "camera_execption.h"
#include "spdlog/spdlog.h"
namespace znp {

UVCCamera::UVCCamera() {
  videoList_ = std::make_shared<VideoList>();
  if (videoList_->Init()) {
    throw CameraException(CameraException::ErrorCode::INITIALIZATION_ERROR,
                          "videoList_ Init failed!");
  }
  uart_ = std::make_shared<znp::Uart>(uvc_camera_info.uart_device_name_,
                                      uvc_camera_info.uart_device_baudrate_);
  if (uart_) {
    uart_->RegisterReceivedCallBack(std::bind(&UVCCamera::UartReceivedCallBack,
                                              this, std::placeholders::_1,
                                              std::placeholders::_2));
  }
  v4l2_h264_ = std::make_shared<znp::V4L2>(
      uvc_camera_info.camera_device_h264_name_.c_str(),
      uvc_camera_info.video_queue_frame_width_,
      uvc_camera_info.video_queue_frame_hight_,
      uvc_camera_info.video_format_h264,
      uvc_camera_info.video_fps_h264);
  if (v4l2_h264_) {
    v4l2_h264_->RegisterVideoReceivedCallBack(
        std::bind(&UVCCamera::V4l2ReceivedCallBack, this, std::placeholders::_1,
                  std::placeholders::_2));
  }
  v4l2_jpeg_ = std::make_shared<znp::V4L2>(
      uvc_camera_info.camera_device_jpeg_name_.c_str(),
      uvc_camera_info.video_queue_frame_width_,
      uvc_camera_info.video_queue_frame_hight_,
      uvc_camera_info.video_format_jpeg,
      uvc_camera_info.video_fps_jpeg);

  this->handler_map_ = {
      {UvcCmd::kHeartBeat,
       [this](uint8_t* data, uint16_t length) {
         HeartBeatHandle(data, length);
       }},
      {UvcCmd::kCatchPicture,
       [this](uint8_t* data, uint16_t length) {
         CatchPictureHandle(data, length);
       }},
      {UvcCmd::kCatchPictureClean,
       [this](uint8_t* data, uint16_t length) {
         CatchPictureCleanHandle(data, length);
       }},
      {UvcCmd::kWireDrawingCheck,
       [this](uint8_t* data, uint16_t length) {
         WireDrawingCheckHandle(data, length);
       }},
      {UvcCmd::kVersion,
       [this](uint8_t* data, uint16_t length) { VersionHandle(data, length); }},
      {UvcCmd::kForeignBodyCheck,
       [this](uint8_t* data, uint16_t length) {
         ForeignBodyCheckHandle(data, length);
       }},
      {UvcCmd::kLed,
       [this](uint8_t* data, uint16_t length) { LedHandle(data, length); }},
      {UvcCmd::kAi,
       [this](uint8_t* data, uint16_t length) { AiHandle(data, length); }}};
}

UVCCamera::~UVCCamera() { 
  SPDLOG_INFO("UVCCamera Close\n");
  if (uart_) uart_.reset();
  if (v4l2_h264_) v4l2_h264_.reset();
  if (v4l2_jpeg_) v4l2_jpeg_.reset(); 
  if (videoList_) videoList_.reset(); 
}

void UVCCamera::Start() {
  SPDLOG_INFO("UVCCamera Start\n");
  try {
    if (uart_) uart_->Start();
    if (v4l2_h264_) v4l2_h264_->Start();
    isUartBusy = false;
  } catch (const CameraException& e) {
    throw;
  }
}

void UVCCamera::RegisterUartReceivedCallBack(UartCallBack callback) {
  this->uart_received_callback_ = callback;
}

void UVCCamera::Stop() {
  SPDLOG_INFO("UVCCamera Stop\n");
  if (uart_) uart_->Stop();
  if (v4l2_h264_) v4l2_h264_->Stop();
  if (v4l2_jpeg_) v4l2_jpeg_->Stop();
}

bool UVCCamera::IsVideoAlive() const {
  return (this->v4l2_h264_ && this->v4l2_h264_->IsOpen());
}

bool UVCCamera::IsUARTAlive() const {
  return (this->uart_ && this->uart_->IsOpen());
}

bool UVCCamera::StartStreamOfMjpeg() {
  if (this->v4l2_jpeg_) {
    if (!this->v4l2_jpeg_->IsOpen()) {
      this->v4l2_jpeg_->Open();
    }
    else {
      SPDLOG_WARN("Mjpeg Stream Is Open!");
    }
    return true;
  }
  else {
    SPDLOG_ERROR("v4l2_jpeg_ Is Not Exist!");
    return false;
  }
}

bool UVCCamera::StopStreamOfMjpeg() {
  if (this->v4l2_jpeg_) {
    if (this->v4l2_jpeg_->IsOpen()) {
      this->v4l2_jpeg_->Close();
    }
    else {
      SPDLOG_WARN("Mjpeg Stream Is Not Open");
    }
    return true;
  }
  else {
    SPDLOG_ERROR("v4l2_jpeg_ Is Not Exist!");
    return false;
  }
}

bool UVCCamera::GetFrameOfMjpeg(unsigned char* pBuf, int* pLen) {
  if ((!pBuf) || (!pLen)) {
    SPDLOG_ERROR("error param!");
    return false;
  }
  bool retval = false;
  if (this->v4l2_jpeg_) {
    retval = this->v4l2_jpeg_->GetOneFrame(pBuf, pLen);
  }
  return retval;
}

bool UVCCamera::GetFrameOfH264(unsigned char* pBuf, int* pLen) {
  if ((!pBuf) || (!pLen)) {
    SPDLOG_ERROR("param error!");
    return false;
  }
  std::lock_guard<std::mutex> lock(this->get_h264_mutex_);  
  if (this->videoList_->ReadOneFrame(pBuf, pLen)) {
    // SPDLOG_ERROR("ReadOneFrame error!");
    return false;
  }
  return true;
}

bool UVCCamera::SendCmdToCamera(CameraCmdCode cmd) {
  return this->AnalysisCmd(cmd);
}

bool UVCCamera::ControlDevice(const uint8_t cmd, const uint8_t param) {
  if (this->isUartBusy) {
    SPDLOG_WARN("UartBusy!");
    return false;
  } else {
    std::lock_guard<std::mutex> lock(this->uart_write_mutex_);
    this->protocol_.Packet(cmd, param);
    if (this->uart_) {
      this->isUartBusy = true;
      if (this->uart_->Write(this->protocol_.Getframe().data(),
                             this->protocol_.Getframe().size())) {
        uart_time_stamp_ = store::GetSteadyClockMs();   // 最后一次发送时间戳
        uint64_t now_stamp_ = uart_time_stamp_;
        while(isUartBusy || now_stamp_ - uart_time_stamp_ > 3*1000) {
          now_stamp_ = store::GetSteadyClockMs();
          std::this_thread::sleep_for(std::chrono::milliseconds(20));  // 20ms检测一次
        }
        if (now_stamp_ - uart_time_stamp_ < 3*1000) {   // isUartBusy被另一条线程设置为false,获取到结果
          return true;
        }
        else {
          this->isUartBusy = false;
          return false;   // 获取超时
        }
        
      }
    }
  }
}

bool UVCCamera::LedOpen() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kLed, UvcLed::kOn);
}

bool UVCCamera::LedClose() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kLed, UvcLed::kOff);
}

bool UVCCamera::LedGetStatus() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kLed, UvcLed::kGetState);
}

bool UVCCamera::AIGetStatus() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kAi, UvcAi::kGetStatus);
}

bool UVCCamera::AIOpen() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kAi, UvcAi::kOpen);
}

bool UVCCamera::AIClose() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kAi, UvcAi::kClose);
}

bool UVCCamera::CatchPictureOn() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kCatchPicture,
                             UVCCatchPicture::kCatchPictureOn);
}

bool UVCCamera::CatchPictureOFF() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kCatchPicture,
                             UVCCatchPicture::kCatchPictureOff);
}

bool UVCCamera::CatchPictureClean() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kCatchPictureClean,
                             UVCCatchPicture::kCatchPictureOn);
}

bool UVCCamera::VersionInfo() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kVersion,
                             UVCCatchPicture::kCatchPictureOn);
}

bool UVCCamera::WireDrawingCheckStart() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kWireDrawingCheck, UVCAICheck::kStart);
}

bool UVCCamera::WireDrawingCheckOnce() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kWireDrawingCheck, UVCAICheck::kOnce);
}

bool UVCCamera::WireDrawingCheckStop() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kWireDrawingCheck, UVCAICheck::kStop);
}

bool UVCCamera::ForeignBodyCheckStart() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kForeignBodyCheck, UVCAICheck::kStart);
}

bool UVCCamera::ForeignBodyCheckOnce() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kForeignBodyCheck, UVCAICheck::kOnce);
}

bool UVCCamera::ForeignBodyCheckStop() {
  SPDLOG_INFO("{}", __FUNCTION__);
  return this->ControlDevice(UvcCmd::kForeignBodyCheck, UVCAICheck::kStop);
}

/*
 * 功  能：
 * 心跳检测(周期性发送串口心跳指令,检查串口接收是否超时,5秒超时；10秒离线)
 * 参  数：
 * 返回值：无
 */
void UVCCamera::HeartBeat() {
  if (!IsUARTAlive()) {
    SPDLOG_ERROR("Uart is not exist!");
    return;
  }

  this->ControlDevice(UvcCmd::kHeartBeat,
                      UVCCatchPicture::kCatchPictureOn);  // 心跳查询指令
  if (!uart_received_callback_) {
    SPDLOG_ERROR("uart_received_callback_ not regiester!");
  }
  auto now = store::GetSteadyClockMs();
  if (uart_time_stamp_ == 0) {
    uart_time_stamp_ = now;
  } else {
    if (now - uart_time_stamp_ >= uvc_camera_info.uart_time_off_line_ms_) {
      SPDLOG_ERROR("Uart offline!");
      uart_received_callback_(CameraCmd::cCamera, CameraState::sOff, nullptr);
    } else if (now - uart_time_stamp_ >= uvc_camera_info.uart_time_out_ms_) {
      SPDLOG_WARN("Uart Time out!");
      this->isUartBusy = false;
      uart_received_callback_(CameraCmd::cCamera, CameraState::sOff, nullptr);
    }
  }
}

bool UVCCamera::AnalysisCmd(CameraCmdCode cmd) {  // 解析上层命令执行相应功能
  if (cmd >= znp::CameraCmdCode::CAMERA_END_CODE ||
      cmd < znp::CameraCmdCode::CAMERA_GET_AI_STATUS) {
    SPDLOG_ERROR("error prama!");
    return false;
  }

  switch (cmd) {
    case znp::CameraCmdCode::CAMERA_GET_AI_STATUS:  // AI识别状态查询/心跳检测
      return this->AIGetStatus();
      break;
    case znp::CameraCmdCode::CAMERA_GET_FRAME_OFF:  // 关闭延时抓拍
      return this->CatchPictureOFF();
      break;
    case znp::CameraCmdCode::CAMERA_GET_FRAME_ON:  // 开启延时抓拍
      return this->CatchPictureOn();
      break;
    case znp::CameraCmdCode::CAMERA_GET_FRAME_CLEAN:  // 清除延时摄状态
      return this->CatchPictureClean();
      break;
    case znp::CameraCmdCode::CAMERA_WIRE_DRAW_CHECK_BEGIN:  // 启动炒面检测
      return this->WireDrawingCheckStart();
      break;
    case znp::CameraCmdCode::CAMERA_WIRE_DRAW_CHECK_ONCE:  // 正常炒面检测
      return this->WireDrawingCheckOnce();
      break;
    case znp::CameraCmdCode::CAMERA_WIRE_DRAW_CHECK_LAST:  // 最后一次炒面检测
      return this->WireDrawingCheckStop();
      break;
    case znp::CameraCmdCode::CAMERA_GET_VERSION:  // 摄像头版本号获取
      return this->VersionInfo();
      break;
    case znp::CameraCmdCode::CAMERA_FOREIGN_BODY_CHECK_BEGIN:  // 启动AI异物识别
      return this->ForeignBodyCheckStart();
      break;
    case znp::CameraCmdCode::CAMERA_FOREIGN_BODY_CHECK_ONCE:  // 正常AI异物识别
      return this->ForeignBodyCheckOnce();
      break;
    case znp::CameraCmdCode::
        CAMERA_FOREIGN_BODY_CHECK_LAST:  // 最后一次AI异物识别
      return this->ForeignBodyCheckStop();
      break;
    case znp::CameraCmdCode::CAMERA_LED_GET_STATUS:  // 获取LED灯状态
      return this->LedGetStatus();
      break;
    case znp::CameraCmdCode::CAMERA_LED_OFF:  // 关闭LED
      return this->LedClose();
      break;
    case znp::CameraCmdCode::CAMERA_LED_ON:  // 开启LED
      return this->LedOpen();
      break;
    case znp::CameraCmdCode::CAMERA_AI_STATUS_CHECK:  // AI功能状态查询
      return this->AIGetStatus();
      break;
    case znp::CameraCmdCode::CAMERA_AI_STATUS_OFF:  // AI功能状态关闭
      return this->AIClose();
      break;
    case znp::CameraCmdCode::CAMERA_AI_STATUS_ON:  // AI功能状态打开
      return this->AIOpen();
      break;
    default:
      SPDLOG_WARN("param error!");
      break;
  }
  return false;
}

void UVCCamera::UartReceivedCallBack(uint8_t* data,
                                     int len) {  // 接收串口数据 解析数据帧等
  if ((!data) || (len < 0)) {
    SPDLOG_ERROR("error prama!");
    return;
  }

  for (int i = 0; i < len; i++) {
    if (protocol_.Parse(this->message_, data[i])) {
      // 进入此处说明得到完整一帧
      this->message_.received = false;  // 重置接收一帧标志
      FrameFormat* p = (FrameFormat*)message_.frame;
      auto head = p->head;
      auto crc16 = p->crc16;
      auto len = p->length;
      auto cmd = p->cmd;
      auto param = p->payload[0];

      // SPDLOG_INFO("head:0x{:04x}, crc16:0x{:04x} length:0x{:04x} cmd:0x{:02x} param:0x{:02x}",
      //             head, crc16, len, cmd, param);

      uint8_t val[p->length] = {0};
      memcpy(val, p->payload, p->length - 1);
      auto iter = handler_map_.find(p->cmd);
      if (iter != handler_map_.end()) {
        uart_time_stamp_ = store::GetSteadyClockMs();   // 更新时间戳
        iter->second(val, p->length - 1); // 反馈数据到上层
        this->isUartBusy = false;
      }
    }
  }
}

void UVCCamera::V4l2ReceivedCallBack(
    uint8_t* data, int len) {  // 接收UVC数据,视频帧存入队列缓存区
  if ((!data) || (len < 0)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  this->videoList_->AppendWriteNode(data, len, FRAME_END);
}

void UVCCamera::HeartBeatHandle(uint8_t* param, uint16_t len) {
  if ((!param) || (len != 1)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  uart_received_callback_(CameraCmd::cCamera, CameraState::sOn, nullptr);
  // 摄像头在线
  switch (param[len-1])
  {
      case 0x00:  // AI开启
          break;
      case 0x01:  // AI未开启
          break;
      default:
          break;
  }
}

void UVCCamera::CatchPictureHandle(uint8_t* param, uint16_t len) {
  if ((!param) || (len != 1)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  SPDLOG_INFO("{}: param[len-1]:{}", __FUNCTION__, param[len - 1]);
  switch (param[len - 1]) {
    case 0x00:  // 抓拍图片成功
      break;
    case 0x01:  // 抓拍图片失败
      break;
    default:
      break;
  }
}

void UVCCamera::CatchPictureCleanHandle(uint8_t* param, uint16_t len) {
  if ((!param) || (len != 0)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  // 清空抓拍标志位
}

void UVCCamera::WireDrawingCheckHandle(uint8_t* param, uint16_t len) {
  if ((!param) || (len != 1)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  if (!uart_received_callback_) {
    SPDLOG_ERROR("no uart_received_callback_!");
    return;
  }
  SPDLOG_INFO("{}: param[len-1]:{}", __FUNCTION__, param[len - 1]);
  switch (param[len - 1]) {
    case 0x00:  // 非炒面
      uart_received_callback_(CameraCmd::cWireDraw, CameraState::sOff, nullptr);
      break;
    case 0x01:  // 炒面
      uart_received_callback_(CameraCmd::cWireDraw, CameraState::sOn, nullptr);
      break;
    case 0x02:  // 摄像头异常
      uart_received_callback_(CameraCmd::cWireDraw, CameraState::sAbnormal,
                              nullptr);
      break;
    case 0x03:  // AI未开启
      uart_received_callback_(CameraCmd::cWireDraw, CameraState::sOther,
                              nullptr);
      break;
    default:
      SPDLOG_ERROR("param not found!");
      break;
  }
}

void UVCCamera::VersionHandle(uint8_t* param, uint16_t len) {
  if ((!param) || (len != 3)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  uint8_t length = len & 0xFF;  // 取低8位 (字符串长度)
  if (this->uart_received_callback_) {
    this->uart_received_callback_(CameraCmd::cVersion, length, param);
  }
}

void UVCCamera::ForeignBodyCheckHandle(uint8_t* param, uint16_t len) {
  if ((!param) || (len != 1)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  SPDLOG_INFO("{}: param[len-1]:{}", __FUNCTION__, param[len - 1]);
  switch (param[len - 1]) {
    case 0x00:  // 无异物
      uart_received_callback_(CameraCmd::cForeignBodyCheck, CameraState::sOff,
                              nullptr);
      break;
    case 0x01:  // 有异物
      uart_received_callback_(CameraCmd::cForeignBodyCheck, CameraState::sOn,
                              nullptr);
      break;
    case 0x02:  // 摄像头异常
      uart_received_callback_(CameraCmd::cForeignBodyCheck,
                              CameraState::sAbnormal, nullptr);
      break;
    case 0x03:  // AI未开启
      uart_received_callback_(CameraCmd::cForeignBodyCheck, CameraState::sOther,
                              nullptr);
      break;
    default:
      SPDLOG_ERROR("param not found!");
      break;
  }
}

void UVCCamera::LedHandle(uint8_t* param, uint16_t len) {
  if ((!param) || (len != 1)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  if (!uart_received_callback_) {
    SPDLOG_ERROR("no uart_received_callback_!");
    return;
  }
  SPDLOG_INFO("{}: param[len-1]:{}", __FUNCTION__, param[len - 1]);
  switch (param[len - 1]) {
    case 0x00:  // LED异常
      uart_received_callback_(CameraCmd::cLed, CameraState::sAbnormal, nullptr);
      break;
    case 0x01:  // 关闭状态
      uart_received_callback_(CameraCmd::cLed, CameraState::sOff, nullptr);
      break;
    case 0x02:  // 开启状态
      uart_received_callback_(CameraCmd::cLed, CameraState::sOn, nullptr);
      break;
    case 0x03:  // 关闭二次确认
      uart_received_callback_(CameraCmd::cLed, CameraState::sOther, nullptr);
      break;
    default:
      SPDLOG_ERROR("param not found!");
      break;
  }
}

void UVCCamera::AiHandle(uint8_t* param, uint16_t len) {
  if ((!param) || (len != 1)) {
    SPDLOG_ERROR("error prama!");
    return;
  }
  SPDLOG_INFO("{}: param[len-1]:{}", __FUNCTION__, param[len - 1]);
  switch (param[len - 1]) {
    case 0x00:  // AI功能异常
      uart_received_callback_(CameraCmd::cAI, CameraState::sAbnormal, nullptr);
      break;
    case 0x01:  // AI关闭状态
      uart_received_callback_(CameraCmd::cAI, CameraState::sOff, nullptr);
      break;
    case 0x02:  // AI开启状态
      uart_received_callback_(CameraCmd::cAI, CameraState::sOn, nullptr);
      break;
    default:
      SPDLOG_ERROR("param not found!");
      break;
  }
}

}  // namespace znp
