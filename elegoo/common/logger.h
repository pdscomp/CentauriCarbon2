/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-22 11:09:23
 * @LastEditors  : huangpeiyun
 * @LastEditTime : 2025-06-28 12:27:15
 * @Description  : logging configuration module.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>  
#include <spdlog/sinks/basic_file_sink.h>      
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/async.h>                      
#include <memory>

namespace elegoo
{
namespace common
{
    
class ElegooLog
{
public:
    static ElegooLog& get_instance();
    void init_log(std::string logger_name = "elegoo", 
        std::string log_file_path = "logs",
        int log_level = spdlog::level::trace, bool async_mode=false);

    void set_level(int level=spdlog::level::trace);

private:
    ElegooLog();
    ~ElegooLog();
    static ElegooLog* log;
};

} // namespace common
} // namespace elegoo