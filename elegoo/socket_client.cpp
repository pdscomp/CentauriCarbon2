/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-12-26 14:49:58
 * @LastEditors  : Ben
 * @LastEditTime : 2024-12-26 15:00:44
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include "json.h"

#define SOCKET_PATH "/tmp/elegoo_uds"

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
    send_data["method"] = "gcode/script";
    send_data["params"]["script"] = "G91\\n G1 x1 F7800\\n G90";

    std::string message = send_data.dump();
    message += "\x03";

    send(client_fd, message.data(), message.size(), 0);

    // 接收服务器响应
    char buffer[256];
    ssize_t num_bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (num_bytes > 0) {
        buffer[num_bytes] = '\0';
        std::cout << "Received from server: " << buffer << std::endl;
    }

    // 关闭套接字
    close(client_fd);

    return 0;
}
