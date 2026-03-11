/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-22 11:09:31
 * @LastEditors  : Ben
 * @LastEditTime : 2025-03-27 15:53:48
 * @Description  : logging configuration module.
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace elegoo
{
namespace common
{

ElegooLog* ElegooLog::log = nullptr;

ElegooLog::ElegooLog()
{

}

ElegooLog::~ElegooLog()
{
    spdlog::shutdown();
}

ElegooLog& ElegooLog::get_instance()
{
    if (log == nullptr)
    {
        log = new ElegooLog;
    }
    return *log;
}

void ElegooLog::init_log(std::string logger_name,
    std::string log_file_path,
    int log_level, bool async_mode)
{
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    // std::stringstream ss;
    // ss << std::put_time(std::localtime(&time), "%Y-%m-%d-%H-%M-%S");
    // std::string log_file_name = ss.str() + ".log";
    std::string log_file_name = "elegoo.log";
    std::string logFile = log_file_path + log_file_name;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    // auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);
    //最大保留10M的日记数据
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logFile, 10*1024*1024, 1);
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%t] %v");
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%t] %v");

    std::shared_ptr<spdlog::logger> logger;
    if (async_mode)
    {
        spdlog::init_thread_pool(8192, 1); // 队列大小8192，1个后台线程
        logger = std::make_shared<spdlog::async_logger>(logger_name,
            spdlog::sinks_init_list{console_sink, file_sink},
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    }
    else
    {
        logger = std::make_shared<spdlog::logger>(logger_name,
            spdlog::sinks_init_list{console_sink, file_sink}
            );
    }

    logger->set_level(static_cast<spdlog::level::level_enum>(log_level));
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::info);

    SPDLOG_INFO("elegoo log init finish!");
}

void ElegooLog::set_level(int level)
{
    spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
}

} // namespace common
} // namespace elegoo