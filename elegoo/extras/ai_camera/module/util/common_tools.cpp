/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2024-10-31 21:52:39
 * @LastEditors  : Jack
 * @LastEditTime : 2025-08-28 19:43:44
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "common_tools.h"
#include "spdlog/spdlog.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>

#include <sys/wait.h>  // For waitpid
#include <vector>
#include <memory>
namespace CommonTools {


// Check if wlan0 is enabled (up and running)
static bool isWLAN0Enabled() {
    // Method 2: Using ioctl and SIOCGIFFLAGS for more reliable interface state checking
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return false;
    }

    struct ifreq ifr;
    strncpy(ifr.ifr_name, "wlan0", IFNAMSIZ - 1);

    bool isEnabled = false;
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) == 0) {
        isEnabled = (ifr.ifr_flags & IFF_UP) && (ifr.ifr_flags & IFF_RUNNING);
    } else {
        perror("ioctl SIOCGIFFLAGS");
    }
    close(sockfd);
    return isEnabled;
}

// Check if wlan0 is connected to a hotspot by reading /proc/net/wireless
// Read operstate to check if wlan0 is in an operational state
static bool isWLAN0Operational() {
    std::ifstream operstate("/sys/class/net/wlan0/operstate");
    if (!operstate.is_open()) {
        std::cerr << "Failed to open /sys/class/net/wlan0/operstate" << std::endl;
        return false;
    }

    std::string state;
    operstate >> state;

    // Possible states: "up", "down", "unknown"
    return state == "up";
}

// Optionally check carrier status if available
static bool isWLAN0CarrierDetected() {
    std::ifstream carrier("/sys/class/net/wlan0/carrier");
    if (!carrier.is_open()) {
        // Carrier file might not exist for all wireless devices.
        // In this case, we can't make a strong assertion about the connection status.
        return true; // Assume it's connected as a fallback.
    }

    int status;
    if (carrier >> status) {
        return status == 1; // 1 means carrier detected (connected).
    }
    return false;
}

// Check if wlan0 has been receiving or transmitting data recently
static bool isWLAN0Active() {
    std::ifstream rxBytesFile("/sys/class/net/wlan0/statistics/rx_bytes");
    std::ifstream txBytesFile("/sys/class/net/wlan0/statistics/tx_bytes");

    if (!rxBytesFile.is_open() || !txBytesFile.is_open()) {
        std::cerr << "Failed to open statistics files" << std::endl;
        return false;
    }

    unsigned long long rxBytes, txBytes;
    if (!(rxBytesFile >> rxBytes) || !(txBytesFile >> txBytes)) {
        std::cerr << "Failed to read statistics files" << std::endl;
        return false;
    }

    // If there are any bytes received or transmitted, assume the interface is active.
    return rxBytes > 0 || txBytes > 0;
}

// Main function to check if wlan0 is enabled and connected to a hotspot
bool isWLAN0Connect() {
    if (!isWLAN0Enabled()) {
        std::cout << "wlan0 is not enabled." << std::endl;
        return false;
    }


    if (!isWLAN0Operational()) {
        std::cout << "wlan0 is enabled but not operational." << std::endl;
        return false;
    }

    if (!isWLAN0CarrierDetected()) {
        std::cout << "wlan0 is enabled but no carrier detected." << std::endl;
        return false;
    }

    if (!isWLAN0Active()) {
        std::cout << "wlan0 is enabled but seems inactive." << std::endl;
        return false;
    }
    // std::cout << "wlan0 is enabled and connected to a hotspot." << std::endl;
    return true;
}

// 获取当前设备WLAN0的IP地址
std::string getWLAN0IPAddress() {
  struct ifaddrs *ifaddr = nullptr, *ifa = nullptr;
  char ip[INET_ADDRSTRLEN] = {0};

  if (getifaddrs(&ifaddr) == -1) {
    perror("getifaddrs");
    return "";
  }

  for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr && ifa->ifa_name == std::string("wlan0") &&
        ifa->ifa_addr->sa_family == AF_INET) {  // 检查IPv4
      inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, ip,
                sizeof(ip));
      freeifaddrs(ifaddr);
      // SPDLOG_INFO("wlan0 ip:{}", ip);
      return std::string(ip);
    }
  }

  freeifaddrs(ifaddr);
  return "";
}

// 判断当前摄像头文件是否存在
bool isFileExists(const std::string& Path) {
  struct stat buffer;
  return (stat(Path.c_str(), &buffer) == 0);
}

// 检测摄像头拔出和插入事件（假设是通过文件系统监测）
void monitorCameraEvents(const std::string& cameraPath,
                         void (*callback)(bool)) {
  static bool wasInserted = false;

  while (true) {
    bool exists = isFileExists(cameraPath);
    if (exists && !wasInserted) {
      wasInserted = true;
      callback(true);  // 摄像头插入
    } else if (!exists && wasInserted) {
      wasInserted = false;
      callback(false);  // 摄像头拔出
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));  // 每秒检查一次
  }
}

// 定义 SavePicture 函数
void SavePicture(const std::string& file_path, unsigned char* data,
                 int data_size) {
  if ((!data) || (data_size <= 0)) {
    throw std::runtime_error("error param");
  }

  #if 0
  std::ofstream file(file_path, std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + file_path);
  }

  file.write(reinterpret_cast<const char*>(data), data_size);

  if (!file) {
    throw std::runtime_error("Failed to write to file: " + file_path);
  }
  SPDLOG_INFO("save picture: {}", file_path);
  file.close();
  #else
  FILE *fp = fopen(file_path.c_str(), "w+");
  if (fp == NULL) {
    SPDLOG_ERROR("fopen failed");
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

  #endif
}

// 删除指定路径下的所有文件
void DeleteFilesInDirectory(const std::string& dir_path) {
  DIR* dir;
  struct dirent* entry;
  struct stat file_stat;

  // 打开目录
  dir = opendir(dir_path.c_str());
  if (dir == nullptr) {
    throw std::runtime_error("Failed to open directory: " + dir_path);
  }

  // 遍历目录
  while ((entry = readdir(dir)) != nullptr) {
    // 跳过 "." 和 ".."
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // 构建完整路径
    std::string full_path = dir_path + entry->d_name;

    // 获取文件状态
    if (lstat(full_path.c_str(), &file_stat) == -1) {
      closedir(dir);
      throw std::runtime_error("Failed to get file status: " + full_path);
    }

    // 检查是否是文件
    if (S_ISREG(file_stat.st_mode)) {
      // 删除文件
      if (unlink(full_path.c_str()) == -1) {
        closedir(dir);
        throw std::runtime_error("Failed to delete file: " + full_path);
      }
      std::cout << "Deleted file: " << full_path << std::endl;
    }
  }

  // 关闭目录
  if (closedir(dir) == -1) {
    throw std::runtime_error("Failed to close directory: " + dir_path);
  }
}


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
        // std::cout << buffer.data() << std::endl;
    }

    // FILE* raw = pipe.release();
    // int status = pclose(raw);
    // int exit_code = WEXITSTATUS(status); //判断执行状态

    return result;
}

// 调用 FFmpeg 命令行工具
void CallFFmpeg(const std::string& command) {
    try {
        std::string output = exec(command.c_str());
        if(output.empty()) {
          throw std::runtime_error(std::string("exec command failed: "));
        }
        std::cout << "FFmpeg output:\n" << output << std::endl;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to call FFmpeg: ") + e.what());
    }
}

// 合成视频
void CombineImagesToVideo(const std::string& image_dir,
                          const std::string& output_video) {
    // Ensure the paths are properly quoted to handle spaces and special characters
    std::string quoted_image_dir = "\"" + image_dir + "%d.jpeg\""; 
    std::string quoted_output_video = "\"" + output_video + "\"";
  
    #if 0
    std::string command = "ffmpeg -framerate 20 -pix_fmt yuvj422p -start_number 1 -i " +
                          quoted_image_dir + " -c:v mjpeg -vf \"scale=640:360\" " +
                          quoted_output_video + " &";
    #else
    std::string command = "ffmpeg -framerate 20 -start_number 1 -i " +
                      quoted_image_dir + " -vf \"scale=640:360,format=yuv420p\" -c:v libx264 -preset ultrafast " +
                      quoted_output_video + " &";           
    #endif

    CallFFmpeg(command);
}

// 修改文件名
void RenameFile(const std::string& old_path, const std::string& new_path) {
  // 调用 rename 函数
  if (rename(old_path.c_str(), new_path.c_str()) != 0) {
    throw std::runtime_error("Failed to rename file from " + old_path + " to " +
                             new_path);
  }
  std::cout << "File renamed from " << old_path << " to " << new_path
            << std::endl;
}


int countJpegFilesPosix(std::string& directoryPath) {
    int count = 0;
    DIR* dir = opendir(directoryPath.c_str());
    
    if (dir == nullptr) {
        std::cerr << "无法打开目录: " << directoryPath << std::endl;
        return -1;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        
        // 跳过.和..
        if (filename == "." || filename == "..") {
            continue;
        }
        
        // 获取文件扩展名
        size_t extPos = filename.rfind('.');
        if (extPos != std::string::npos) {
            std::string extension = filename.substr(extPos);
            std::transform(extension.begin(), extension.end(), extension.begin(), 
                           [](unsigned char c) { return std::tolower(c); });
            
            if (extension == ".jpg" || extension == ".jpeg") {
                count++;
            }
        }
    }
    
    closedir(dir);
    return count;
}


bool directoryExists(const std::string& path) {
    struct stat info;

    if (stat(path.c_str(), &info) != 0) {
        return false;
    } else if (info.st_mode & S_IFDIR) {
        return true;
    } else {
        return false;
    }
}

bool mkdirDirectory(const std::string& path) {
    struct stat info;

    if (stat(path.c_str(), &info) == 0) {
        if (S_ISDIR(info.st_mode)) {
            return true;
        }
        return false;
    }

    mode_t mode = 0755;
    if (mkdir(path.c_str(), mode) == 0) {
        return true;
    }
    return false;
}


uint32_t getFileSize(const std::string& file_path) {
    struct stat file_stat;
    if (stat(file_path.c_str(), &file_stat) != 0) {
        return 0;
    }

    if (!S_ISREG(file_stat.st_mode)) {
        return 0;
    }

    if (file_stat.st_size < 0 || file_stat.st_size > 0xFFFFFFFFULL) {
        return 0;
    }

    return static_cast<uint32_t>(file_stat.st_size);
}



}  // namespace
