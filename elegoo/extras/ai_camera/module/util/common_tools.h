/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-10-25 17:52:25
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-28 19:42:39
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#ifndef COMMON_TOOLS_H
#define COMMON_TOOLS_H

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
namespace CommonTools {

bool isWLAN0Connect();
// 获取当前设备WLAN0的IP地址
std::string getWLAN0IPAddress();

// 判断当前摄像头文件是否存在
bool isFileExists(const std::string& Path);

void SavePicture(const std::string& file_path, unsigned char* data,
                 int data_size);

void CombineImagesToVideo(const std::string& image_dir,
                          const std::string& output_video);

void DeleteFilesInDirectory(const std::string& dir_path);

void RenameFile(const std::string& old_path, const std::string& new_path);

// 检测摄像头拔出和插入事件（假设是通过文件系统监测）
void monitorCameraEvents(const std::string& cameraPath, void (*callback)(bool));

int countJpegFilesPosix(std::string& directoryPath);
bool directoryExists(const std::string& path);
bool mkdirDirectory(const std::string& path);
uint32_t getFileSize(const std::string& file_path);

}

#endif