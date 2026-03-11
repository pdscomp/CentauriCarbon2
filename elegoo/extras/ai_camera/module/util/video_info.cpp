/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-01-16 12:20:34
 * @LastEditors  : Jack
 * @LastEditTime : 2025-01-16 16:09:08
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "video_info.h"
#include <dirent.h> // For directory operations
#include <iostream>
#include <vector>

#include <string>
#include <cstdio> // For popen and pclose
#include <memory> // For unique_ptr
#include <stdexcept> // For runtime_error
#include <algorithm>
// Helper function to execute a command and return its output as a string
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

double getVideoDurationInSeconds(const std::string& filePath) {
    std::string command = "ffprobe -v quiet -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"" + filePath + "\"";

    try {
        // Execute the command and capture the output
        std::string output = exec(command.c_str());

        // Trim any trailing whitespace characters from the output
        output.erase(std::find_if(output.rbegin(), output.rend(), [](int ch) {
            return !std::isspace(ch);
        }).base(), output.end());

        // Convert the output string to a double representing the duration in seconds
        return std::stod(output);
    } catch (const std::exception& e) {
        std::cerr << "Error executing ffprobe: " << e.what() << std::endl;
        return -1.0; // Return an invalid duration on error
    }
}


std::string convertTimeTToStringThreadSafe(time_t rawtime) {
    char buffer[20];
    struct tm timeinfo;

    // Convert time_t to tm as local time in a thread-safe manner
    if (localtime_r(&rawtime, &timeinfo) != nullptr) {
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        return "Invalid time_t value";
    }

    return std::string(buffer);
}

std::vector<VideoInfo_> getVideoInfo(const std::string& path) {
    std::vector<VideoInfo_> videoInfos;

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        std::cerr << "Failed to open directory: " << path << std::endl;
        return videoInfos;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string entryName(entry->d_name);
        if (entryName == "." || entryName == "..") continue;

        std::string fullPath = (path.back() == '/') ? path + entryName : path + "/" + entryName;
        struct stat fileStat;
        if (stat(fullPath.c_str(), &fileStat) == -1) {
            continue; // Skip files that cannot be accessed
        }

        if (S_ISREG(fileStat.st_mode) && entryName.size() > 4 && entryName.substr(entryName.size() - 4) == ".mp4") {
            VideoInfo_ info;
            info.filename = fullPath;
            info.last_write_time = convertTimeTToStringThreadSafe(fileStat.st_mtime);

            info.duration = (int)getVideoDurationInSeconds(fullPath);

            videoInfos.push_back(info);
        }
    }

    closedir(dir);
    return videoInfos;
}


