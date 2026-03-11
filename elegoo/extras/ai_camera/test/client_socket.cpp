/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-01-16 17:15:00
 * @LastEditors  : Jack
 * @LastEditTime : 2025-01-21 14:16:12
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "json.h"

#define SOCKET_PATH "/tmp/aicamera_uds"

int main() {
    // 创建一个 UNIX 套接字
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("socket failed");
        return 1;
    }

    // 设置服务器地址
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // 连接到服务器
    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect failed");
        close(client_fd);
        return 1;
    }

    // 发送消息
    json send_data;

    send_data["id"] = 123;
    // send_data["method"] = "info";   //  获取摄像头系统信息
    // send_data["method"] = "ota_run";   //  OTA升级

    // send_data["method"] = "ai_enable";      //  AI功能开关
    // send_data["params"]["value"] = "on";    // on: 开启     off：关闭

    send_data["method"] = "time_lapse";      //  延时摄影
    // send_data["params"]["status"] = "start";    // start: 开始     capture：抓拍    stop：结束
    // send_data["params"]["mode"] = "begin";  // begin: 非断电续打    continue：断电续打

    send_data["params"]["status"] = "capture";    // start: 开始     capture：抓拍    stop：结束

    send_data["params"]["index"] = 1;  // index： 照片序号

    // send_data["params"]["status"] = "stop";    // start: 开始     capture：抓拍    stop：结束
    // send_data["params"]["filename"] = "test_video";  // filename： 保存的视频文件名

    // send_data["method"] = "ai_detection";      //  AI识别
    // send_data["params"]["operate"] = "wire";    // wire：炒面  ietms: 异物  ab_side：AB面识别


    // send_data["method"] = "led";      //  led

    // send_data["params"]["operate"] = "set";    // get：获取led状态  set：设置led状态
    // send_data["params"]["value"] = "off";  // on：点亮  off：熄灭

    // send_data["method"] = "video_list";   //  获取延时摄影文件列表

    std::string message = send_data.dump();
    message += "\x03";

    send(client_fd, message.data(), message.size(), 0);

    // 接收服务器响应
    char buffer[4096];
    ssize_t num_bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (num_bytes > 0) {
        buffer[num_bytes] = '\0';
        std::cout << "Received from server: " << buffer << std::endl;
    }



    // 关闭套接字
    close(client_fd);

    return 0;
}
