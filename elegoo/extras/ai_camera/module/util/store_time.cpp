/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-12-05 12:28:58
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-05 14:16:53
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "store_time.h"

#include <chrono>

namespace store {
/**
 * @brief 获取现在 steady clock（开机到当前时刻）的毫秒数
 * @return uint64_t ms 开机到当前时刻，累计相对时间毫秒数
 */
uint64_t GetSteadyClockMs(void) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

/**
 * @brief 获取现在 steady clock（开机到当前时刻）的秒数
 * @return uint64_t s 开机到当前时刻，累计相对时间秒数
 */
uint64_t GetSteadyClockS(void) {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

/**
 * @brief 获取现在 system clock (系统时间)d的毫秒数
 * @return uint64_t ms 系统时间的毫秒数，会随着校时，手动调整时间而变化
 */
uint64_t GetSystemClockMs(void) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

uint64_t GetSteadyClockNs(void) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
}  // namespace store
