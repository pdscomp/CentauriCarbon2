/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-05 15:17:30
 * @LastEditors  : Jack
 * @LastEditTime : 2025-04-02 16:46:13
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <atomic>
#include <iostream>
#include <string>
#include <vector>
namespace znp {

struct AICameraStatus {
  std::atomic<bool> isTimelapsePhotographyActive_{
      false};  // 延时摄影功能状态	   true:启动     false:未启动
  std::atomic<bool> isVideoStreamActive_{
      false};  // RTSP服务标志    true:工作,  false:未工作

  std::string version_;                   // 固件版本
  std::string soft_version_{"1.1.1"};     // 软件版本
};
}
