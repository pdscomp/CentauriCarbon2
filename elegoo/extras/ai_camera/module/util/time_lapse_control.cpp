/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-06-11 15:18:52
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-28 19:48:10
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "time_lapse_control.h"
#include "common_tools.h"
#include "video_encoder.h"

namespace znp
{
TimeLapseVideo::TimeLapseVideo(const std::string &picture_dir, const std::string &video_dir) {
    this->picture_dir_ = picture_dir;
    this->video_dir_ = video_dir;
    this->is_working_ = false;
    this->DeleteJpegInDirectory(picture_dir_);
}

TimeLapseVideo::~TimeLapseVideo() {
    this->is_working_ = false;
}

bool TimeLapseVideo::Start(const std::string& file_name, int width, int height, int fps, int total_frame) {
    ClearPicture(picture_dir_);
    this->filename_ = addTimestampToFilename(file_name);;
    this->width_ = width;
    this->height_ = height;
    this->fps_ = fps;
    this->total_frames_ = total_frame;
    this->video_duration_ = this->total_frames_ / this->fps_;
    this->frame_count_ = 0; 
    this->is_working_ = true;
    this->pic_dirname_ = this->picture_dir_ + this->filename_;
    this->pic_dirname_temp_ = pic_dirname_+ ".temp";

    if (!CommonTools::mkdirDirectory(this->pic_dirname_temp_)) {
        SPDLOG_ERROR("mkdirDirectory {} failed!!", this->pic_dirname_temp_);
        return false;
    }
    return true;
}

bool TimeLapseVideo::Continue(const std::string& file_name, int width, int height, int fps, int total_frame) {
    if (!findTempDirectory(file_name, this->picture_dir_)) {
        return false;
    }
    this->width_ = width;
    this->height_ = height;
    this->fps_ = fps;
    this->total_frames_ = total_frame;
    this->video_duration_ = this->total_frames_ / this->fps_;
    this->frame_count_ = 0; 
    this->is_working_ = true;
    this->pic_dirname_ = this->picture_dir_ + this->filename_;
    this->pic_dirname_temp_ = pic_dirname_+ ".temp";

    return true;
}

bool TimeLapseVideo::Capture(int index, unsigned char* data,int data_size) {
    if (this->is_working_ == false) {
        SPDLOG_ERROR("no timelapse task!!!");
        return false;
    }
    
    if ((!data) || (data_size <= 0) || index <= 0) {
        SPDLOG_ERROR("error param");
        return false;
    }

    char buff[5];
    std::snprintf(buff, sizeof(buff), "%04d", index);
    std::string pic_path = pic_dirname_temp_ + "/" + std::string(buff) + ".jpeg";
    SPDLOG_INFO(pic_path);

    this->frame_count_ = index;
    SavePicture(pic_path, data, data_size);
    return true;
}

void TimeLapseVideo::ClearPicture(const std::string& pic_dir) {
    std::string dir_path = this->picture_dir_ + "*.temp";
    std::string cmd = "rm -rf " + dir_path;
    SPDLOG_INFO("cmd {}", cmd);
    std::system(cmd.c_str());
}

bool TimeLapseVideo::Composite(const std::string& video_name, int width, int height, int fps)
{
    std::string pic_dir = this->picture_dir_ + video_name;
    std::string pic_dir_link = "/tmp/pic_link";
    std::string cmd = "ln -s \'" + pic_dir + "\'" + " " + pic_dir_link;
    SPDLOG_INFO("cmd {}", cmd);
    std::system(cmd.c_str());

    this->video_name_ =  video_name + ".mp4";
    this->video_path_ = this->video_dir_ + this->video_name_;

    if (!CommonTools::directoryExists(pic_dir)) {
        SPDLOG_ERROR("no such pid_dir: {} ", pic_dir);
        return false;
    }

    // CombineImagesToVideo(pic_dir_link, this->video_path_, width, height, fps);
    std::shared_ptr<ImageToVideoEncoder> encoder = std::make_shared<ImageToVideoEncoder>();
    if (encoder->encode_from_directory(pic_dir_link, this->video_path_, width, height, fps)) {
        SPDLOG_INFO("✅ Video saved to {}", video_path_);
    } else {
        SPDLOG_INFO("❌ Encoding failed.");
        return false;
    }

    encoder.reset();
    cmd = "rm -rf " + pic_dir_link;
    SPDLOG_INFO("cmd {}", cmd);
    std::system(cmd.c_str());

    cmd = "rm -rf \'" + pic_dir + "\'";
    SPDLOG_INFO("cmd {}", cmd);
    std::system(cmd.c_str());

    return true;
}

const std::string & TimeLapseVideo::GetVideoName(void)
{
    return this->video_name_;
}

const std::string & TimeLapseVideo::GetPicName(void)
{
    return this->filename_;
}

const std::string & TimeLapseVideo::GetPicPath(void)
{
    return this->pic_dirname_;
}

const std::string & TimeLapseVideo::GetVideoPath(void)
{
    return this->video_path_;
}

const uint32_t TimeLapseVideo::GetVideoSize(void)
{
    return this->video_size_;
}

const uint32_t TimeLapseVideo::GetVideoDuration(void)
{
    return this->video_duration_;
}


std::string TimeLapseVideo::addTimestampToFilename(const std::string& filename) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;

#if defined(__unix__)
    ::localtime_r(&now_time, &tm); // Unix/Linux
#elif defined(_MSC_VER)
    ::localtime_s(&tm, &now_time); // Windows
#else
    tm = *std::localtime(&now_time); // 其他平台（非线程安全）
#endif

    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", &tm);
#if 0
    size_t dot_pos = filename.find_last_of(".");

    if (dot_pos != std::string::npos) {
        return filename.substr(0, dot_pos) + std::string(buffer) + filename.substr(dot_pos);
    } else {
        return filename + std::string(buffer);
    }
#endif
    return filename + std::string(buffer);
}


bool TimeLapseVideo::Close(void) {
    if (this->is_working_ == false) {
       SPDLOG_ERROR("no timelapse task!!!");
       return false; 
    }
    this->is_working_ = false;
    if (this->frame_count_ >= this->total_frames_) {
        Mv(pic_dirname_temp_, pic_dirname_);
        return true;
    }
    else {
        DeleteFilesInDirectory(pic_dirname_temp_);
        return false;
    }
}

void TimeLapseVideo::SavePicture(const std::string& file_path, unsigned char* data,int data_size) {
  if ((!data) || (data_size <= 0)) {
    SPDLOG_ERROR("error param");
    return;
  }

  FILE *fp = fopen(file_path.c_str(), "w+");
  if (fp == NULL) {
    SPDLOG_ERROR("fopen failed:{}", file_path.c_str());
    return ;
  } 
  int ret = 0;
  int writed_size = 0;
  while (writed_size < data_size)
  {
    ret = fwrite(data+writed_size, 1024, 1, fp);
    writed_size += 1024;
  }
  fclose(fp);
}

void TimeLapseVideo::DeleteFilesInDirectory(const std::string& dir_path) {
    std::string cmd = "rm -rf \'" + dir_path + "\'" ;
    SPDLOG_INFO("cmd {}", cmd);
    system(cmd.c_str());
}

bool TimeLapseVideo::DeleteJpegInDirectory(const std::string& dir_path) {
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        SPDLOG_ERROR("Failed to open directory: {}", dir_path);
        return false;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;

        if (filename == "." || filename == "..") {
            continue;
        }

        std::string full_path = dir_path + "/" + filename;

        struct stat statbuf;
        if (lstat(full_path.c_str(), &statbuf) == -1) {
            SPDLOG_WARN("Failed to get file status: {}", full_path);
            continue;
        }

        if (S_ISREG(statbuf.st_mode)) {
            if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".jpeg") {
                if (unlink(full_path.c_str()) == -1) {
                    SPDLOG_ERROR("Failed to delete file: {}", full_path);
                    closedir(dir);
                    return false;
                } else {
                    SPDLOG_INFO("Deleted .jpeg file: {}", full_path);
                }
            }
        }
    }

    closedir(dir);
    return true;
}

void TimeLapseVideo::CombineImagesToVideo(const std::string& image_dir,const std::string& output_video, int width, int height, int fps) {
    std::string quoted_image_dir = "\'" + image_dir + "/*.jpeg\'"; 
    std::string quoted_output_video = output_video + ".temp";
    std::string quoted_output_video_temp = "\'" + quoted_output_video + "\'";
    std::string s_fps = std::to_string(fps);
    std::string s_width = std::to_string(width);
    std::string s_height = std::to_string(height);

#if 1
    std::string ffmpeg_cmd = "ffmpeg -framerate " + s_fps + " -pattern_type glob -i " +
                             quoted_image_dir + " -vf \"scale=" + s_width + ":" + s_height +
                             ",format=yuv420p\" -c:v libx264 -f mp4 -preset ultrafast -thread_type slice -threads 1 " +
                             quoted_output_video_temp;

    std::string command = "cpulimit -l 80 ";
    command += ffmpeg_cmd;
#else
    std::string command = "nice -n 10 ffmpeg -framerate " + s_fps + " -pattern_type glob -i " +
                      quoted_image_dir + " -vf \"scale="+ s_width + ":" + s_height + ",format=yuv420p\" -c:v libx264 -preset ultrafast -thread_type slice -threads 1 " +
                      quoted_output_video + " ";
    
#endif
    SPDLOG_INFO(ffmpeg_cmd);

    // sleep(2);
    CallFFmpeg(ffmpeg_cmd);

    Mv(quoted_output_video, output_video);
    this->video_size_ = CommonTools::getFileSize(output_video);
}

void TimeLapseVideo::CallFFmpeg(const std::string& command)
{
    try {
        std::string output = exec(command.c_str());
        if(output.empty()) {
        //   throw std::runtime_error(std::string("exec command failed: "));
        }
        std::cout << "FFmpeg output:\n" << output << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to call FFmpeg: ") + e.what());
    }
}

void TimeLapseVideo::Mv(const std::string& src, const std::string& dest)
{
    if (rename(src.c_str(), dest.c_str()) != 0) {
        std::cerr << "Error moving file: " << strerror(errno) << std::endl;
        return;
    }
    std::cout << "Moved '" << src << "' to '" << dest << "' successfully." << std::endl;
}

std::string TimeLapseVideo::exec(const char* cmd)
{
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
        usleep(1);
    }

    return result;
}

bool TimeLapseVideo::findTempDirectory(const std::string& str, const std::string& directory) {
    if (str.empty()) {
        return false;
    }
    
    DIR* dir = opendir(directory.c_str());
    if (dir == nullptr) {
        std::cerr << "无法打开目录: " << directory << std::endl;
        return false;
    }
    
    struct dirent* entry;
    const size_t strLength = str.length();
    
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        std::string dirName = entry->d_name;
        size_t nameLength = dirName.length();
        
        if (nameLength > 5 && dirName.substr(nameLength - 5) == ".temp") {
            struct stat pathStat;
            std::string fullPath = directory + "/" + dirName;
            
            if (stat(fullPath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
                if (nameLength >= strLength && dirName.compare(0, strLength, str) == 0) {
                    std::cout << "-------------dirName: " << dirName << std::endl;
                    this->filename_ = dirName.substr(0, nameLength - 5);
                    closedir(dir);
                    return true;
                }
            }
        }
    }
    
    closedir(dir);
    return false;
}

}

