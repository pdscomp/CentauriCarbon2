#pragma once

#include <iostream>
#include <vector>

namespace znp {

/* 摄像头模组反馈结果 */
enum CameraCmd : uint8_t {
  cCamera = 0,            // 摄像头功能
  cAI = 1,                // 摄像头AI功能模块
  cWireDraw = 2,          // 炒面检测
  cForeignBodyCheck = 3,  // 异物检测
  cLed = 4,               // 摄像头LED灯
  cVersion = 5,           // 摄像头固件版本
  cABSide = 6,            // AB面检测
  cEnd                    // 结束标志
};

enum CameraState : uint8_t {
  sOn = 0,  // 摄像头开启 / AI开启 / 炒面现象 / 有异物 / LED 亮 / 获取版本号/A面
  sOff = 1,  // 摄像头关闭 / AI关闭 / 无炒面现象 / 无异物 / LED 灭 / B面
  sAbnormal = 2,  // 摄像头异常 / AI异常 / 未识别
  sOther = 3,     // 其他情况（LED关闭二次确认）
};

enum StatusID : int {
  STATUS_UPDATE = 1000,     // 普通更新
  STATUS_EMERGENCY = 1001,  // 紧急更新

  EMERGENCY_CAMERA_INSERT = 1002,    // 摄像头插入
  EMERGENCY_CAMERA_REMOVED = 1003,   // 摄像头拔出
  EMERGENCY_CAMERA_ONLINE = 1004,    // 摄像头上线
  EMERGENCY_CAMERA_OFFLINE = 1005,   // 摄像头离线
  EMERGENCY_CAMERA_ABNORMAL = 1006,  // 摄像头异常

  EMERGENCY_LED_ABNORMAL = 1008,      // LED异常
  EMERGENCY_AI_ABNORMAL = 1009,       // AI异常

  EMERGENCY_AI_DETECTION_TIME_OUT = 2000,  //识别超时
};

/* AI摄像头操作命令 */
enum AiCameraCmdCode : uint8_t {
  AI_CAMERA_GET_STATE = 0,                 // 获取基本状态
  AI_CAMERA_GET_VERSION,                   // 获取摄像头版本号
  AI_CAMERA_SET_OTA_RUNNING,               // 启动OTA功能
  AI_CAMERA_TIME_LAPSE_START,              // 非断电续打启动延时摄影
  AI_CAMERA_TIME_LAPSE_CONTINUE,           // 断电续打继续延时摄影
  AI_CAMERA_TIME_LAPSE_CATCH_PICTURE,      // 抓拍照片
  AI_CAMERA_TIME_LAPSE_STOP,               // 停止延时摄影
  AI_CAMERA_LIGHT_FOREIGN_BODY_CHECK,      // 异物检测
  AI_CAMERA_LIGHT_PLATE_CHECK,             // 光板检测
  AI_CAMERA_WIRE_DRAWING_NORMAL_START,     // 常规拉丝(炒面)检测启动
  AI_CAMERA_WIRE_DRAWING_MAJOR_START,      // 专业拉丝(炒面)检测启动
  AI_CAMERA_WIRE_DRAWING_SET_NORMAL_MODE,  // 设置拉丝检测为常规模式
  AI_CAMERA_WIRE_DRAWING_SET_MAJOR_MODE,   // 设置拉丝检测为专业模式
  AI_CAMERA_WIRE_DRAWING_STOP,             // 停止拉丝检测
  AI_CAMERA_LED_GET_STATUS,                // 获取LED灯状态
  AI_CAMERA_LED_TURN_ON,                   // 点亮LED灯
  AI_CAMERA_LED_TURN_OFF,                  // 熄灭LED灯
  AI_CAMERA_AI_FUNCTION_GET_STATUS,        // 获取AI功能状态
  AI_CAMERA_AI_FUNCTION_TURN_ON,           // 打开AI功能
  AI_CAMERA_AI_FUNCTION_TURN_OFF,          // 关闭AI功能
  AI_CAMERA_END_CODE                       // 结尾标志
};

/* 通信模块ID */
enum RequestID : int {
  CAMERA_GET_STATUS = 0,
  VERSION_GET_INFO = 10,
  VERSION_OTA_RUNNING = 11,
  AI_FUNCTION_GET_STATUS = 20,
  AI_FUNCTION_SET_STATUS = 21,
  DELAY_PHOTO_GET_STATUS = 30,
  DELAY_PHOTO_START = 31,
  DELAY_PHOTO_CAPTURE = 32,
  DELAY_PHOTO_STOP = 33,
  WIRE_DRAWING_GET_STATUS = 40,
  WIRE_DRAWING_START = 41,
  WIRE_DRAWING_SET_MODE = 42,
  WIRE_DRAWING_STOP = 43,
  FOREIGN_OBJECT_DETECTION_CHECK = 50,
  AB_SIDE_CHECK = 51,
  LED_GET_STATUS = 60,
  LED_CONTROL = 61,
  RTSP_SERVICE_GET_STATUS = 70,
  RTSP_SERVICE_GET_INFO = 71,
};
}
