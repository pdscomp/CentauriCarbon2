/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-06-11 15:19:17
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-28 19:42:08
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

extern "C" {
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <dirent.h>
}
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "spdlog/spdlog.h"

namespace znp
{
    
class TimeLapseVideo {
public:
    TimeLapseVideo(const std::string &picture_dir, const std::string &video_dir);
    ~TimeLapseVideo();
    bool Start(const std::string& file_name, int width, int height, int fps, int total_frame);
    bool Continue(const std::string& file_name, int width, int height, int fps, int total_frame);
    bool Capture(int index, unsigned char* data,int data_size);
    bool Close(void);
    void ClearPicture(const std::string& pic_dir);
    bool Composite(const std::string& video_name, int width, int height, int fps);
    const std::string & GetVideoName(void);
    const std::string & GetPicName(void);
    const std::string & GetPicPath(void);
    const std::string & GetVideoPath(void);
    const uint32_t GetVideoSize(void);
    const uint32_t GetVideoDuration(void);

private:
    int width_;
    int height_;
    int fps_;
    int total_frames_;
    int video_duration_;
    int frame_count_;
    uint32_t video_size_;
    std::string filename_;              //当前延时摄影文件名  xx.gcode2025xxxx -> (打印文件名+开始打印时间)
    std::string pic_dirname_;           //延时图片路径  /opt/usr/picture/xx.gcode2025xxxx
    std::string pic_dirname_temp_;      //临时延时图片路径  /opt/usr/picture/xx.gcode2025xxxx.temp
    std::string picture_dir_;           //延时摄影图片路径前缀 /opt/usr/picture/
    std::string video_dir_;             //延时摄影视频路径前缀 /opt/usr/video/
    std::string video_name_;            //延时摄影视频文件名 xx.gcode2025xxxx.mp4
    std::string video_path_;            //延时摄影视频路径 /opt/usr/video/xx.gcode2025xxxx.mp4
    bool is_working_;

    void SavePicture(const std::string& file_path, unsigned char* data,int data_size);
    void DeleteFilesInDirectory(const std::string& dir_path);
    bool DeleteJpegInDirectory(const std::string& dir_path);
    void CombineImagesToVideo(const std::string& image_dir,const std::string& output_video, int width, int height, int fps);
    void CallFFmpeg(const std::string& command);
    void Mv(const std::string& src, const std::string& dest);
    std::string exec(const char* cmd);
    std::string addTimestampToFilename(const std::string& filename);
    bool findTempDirectory(const std::string& str, const std::string& directory);
};




}
