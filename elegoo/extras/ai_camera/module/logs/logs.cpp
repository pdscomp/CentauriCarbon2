/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2025-09-13 15:42:51
 * @LastEditors  : Jack
 * @LastEditTime : 2025-09-13 15:42:52
 * @Description  :
 *
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#include "logs.h"
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

void init_logs() {
    try {
        // 创建控制台 sink（带颜色）
        auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
        console_sink->set_level(spdlog::level::info);

        // 创建文件 sink（按大小滚动）
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "/opt/usr/logs/ai_camera.log",  // 文件路径
            1024 * 1024,                    // 最大 1MB
            2                               // 最多保留 2 个文件
        );
        file_sink->set_level(spdlog::level::debug);

        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

        auto logger = std::make_shared<spdlog::logger>("ai_camera_logger", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);

        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%t] %v");
        file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] [%t] %v");

        spdlog::set_default_logger(logger);
    } catch (const spdlog::spdlog_ex& ex) {
        fprintf(stderr, "Failed to initialize logger: %s\n", ex.what());
        exit(EXIT_FAILURE);
    }
}