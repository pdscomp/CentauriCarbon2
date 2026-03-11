/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-01-16 12:20:23
 * @LastEditors  : Jack
 * @LastEditTime : 2025-01-16 16:15:58
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <sys/stat.h> // For stat()

struct VideoInfo_ {
    std::string filename;
    int duration; // in seconds
    std::string last_write_time; // POSIX time
};

std::vector<VideoInfo_> getVideoInfo(const std::string& path);
