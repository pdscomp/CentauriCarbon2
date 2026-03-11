/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-05 12:30:51
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-05 14:24:38
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <iostream>
#include <vector>
#include "crc16.h"

namespace znp {
enum UvcCmd : uint8_t {
  kHeartBeat = 0x00,
  kCatchPicture = 0x01,
  kCatchPictureClean = 0x02,
  kWireDrawingCheck = 0x03,
  kVersion = 0x04,
  kForeignBodyCheck = 0x05,
  kLed = 0x6,
  kAi = 0x07,
};

enum UVCCatchPicture : uint8_t {
  kCatchPictureOn = 0x00,
  kCatchPictureOff = 0x01
};

enum UVCAICheck : uint8_t { kStart = 0x00, kOnce = 0x01, kStop = 0x02 };

enum UvcLed : uint8_t { kGetState = 0x00, kOff = 0x01, kOn = 0x02 };

enum UvcAi : uint8_t { kGetStatus = 0x00, kClose = 0x01, kOpen = 0x02 };

enum UvcResult : uint8_t {
  kState_0 =
      0x00,         // AI状态正常、延时摄影捕捉帧成功、非炒面、无异物、AI异常(0x07)
  kState_1 = 0x01,  // AI未开启、延时摄影捕捉帧失败、炒面、有异物、AI关闭(0x07)
  kState_2 = 0x02,  // 摄像头异常、AI开启(0x07)
  kState_3 = 0x03   // AI未开启、
};

enum class CameraCmdCode {                   // AI识别编码
  CAMERA_GET_AI_STATUS = 0,                  // AI识别状态查询
  CAMERA_HEART_BEAT = CAMERA_GET_AI_STATUS,  // 心跳检测
  CAMERA_GET_FRAME_OFF,                      // 关闭延时抓拍
  CAMERA_GET_FRAME_ON,                       // 开启延时抓拍
  CAMERA_GET_FRAME_CLEAN,                    // 清除延时摄状态
  CAMERA_WIRE_DRAW_CHECK_BEGIN,              // 启动炒面检测
  CAMERA_WIRE_DRAW_CHECK_ONCE,               // 正常炒面检测
  CAMERA_WIRE_DRAW_CHECK_LAST,               // 最后一次炒面检测
  CAMERA_GET_VERSION,                        // 摄像头版本号获取
  CAMERA_FOREIGN_BODY_CHECK_BEGIN,           // 启动AI异物识别
  CAMERA_FOREIGN_BODY_CHECK_ONCE,            // 正常AI异物识别
  CAMERA_FOREIGN_BODY_CHECK_LAST,            // 最后一次AI异物识别
  CAMERA_LED_GET_STATUS,                     // 获取LED灯状态
  CAMERA_LED_OFF,                            // 关闭LED
  CAMERA_LED_ON,                             // 开启LED
  CAMERA_AI_STATUS_CHECK,                    // AI功能状态查询
  CAMERA_AI_STATUS_OFF,                      // AI功能状态关闭
  CAMERA_AI_STATUS_ON,                       // AI功能状态打开
  CAMERA_END_CODE
};
}
