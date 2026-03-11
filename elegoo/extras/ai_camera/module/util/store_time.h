#pragma once
#include <stdint.h>

namespace store {
/**
 * @brief 获取现在 steady clock（开机到当前时刻）的毫秒数
 * @return uint64_t ms 开机到当前时刻，累计相对时间毫秒数
 */
uint64_t GetSteadyClockMs(void);
/**
 * @brief 获取现在 system clock (系统时间)d的毫秒数
 * @return uint64_t ms 系统时间的毫秒数，会随着校时，手动调整时间而变化
 */
uint64_t GetSystemClockMs(void);

uint64_t GetSteadyClockNs(void);

uint64_t GetSteadyClockS(void);
}  // namespace store