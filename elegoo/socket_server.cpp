/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2025-02-17 21:17:05
 * @LastEditors  : Ben
 * @LastEditTime : 2025-02-17 21:40:53
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
 
 #define SOCKET_PATH "/tmp/elegoo_uds"
 
 int main() {
     // 创建一个 UNIX 套接字
     unlink(SOCKET_PATH);
     int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
     if (server_fd == -1) {
         perror("socket failed");
         return 1;
     }
 
     // 设置服务器地址
     sockaddr_un addr;
     memset(&addr, 0, sizeof(addr));
     addr.sun_family = AF_UNIX;
     strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
 
     // 绑定套接字
     if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
         perror("bind failed");
         close(server_fd);
         return 1;
     }
 
     // 监听连接
     if (listen(server_fd, 1) == -1) {
         perror("listen failed");
         close(server_fd);
         return 1;
     }
 
     std::cout << "Server is listening on " << SOCKET_PATH << "..." << std::endl;
 
     while (true) {
        // 接受客户端连接
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd == -1) {
            perror("accept failed");
            continue; // 继续等待新的连接
        }

        std::cout << "Client connected." << std::endl;

        // 持续接收客户端消息
        char buffer[4096];
        while (true) {
            ssize_t num_bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (num_bytes > 0) {
                std::string data(buffer, (num_bytes > 0) ? num_bytes : 0);
                std::cout << data << std::endl;
            } else if (num_bytes == 0) {
                std::cout << "Client disconnected." << std::endl;
                break; // 客户端断开连接
            } else {
                perror("recv failed");
                break;
            }

            // 创建响应消息
            json response_data;
            response_data["id"] = 123;
            response_data["result"] = json::object();

            // 发送响应
            std::string response_message = response_data.dump();
            send(client_fd, response_message.data(), response_message.size(), 0);
        }

        close(client_fd); // 关闭当前客户端连接，等待下一个连接
    }

    // 关闭服务器套接字
    close(server_fd);
    unlink(SOCKET_PATH); // 删除 UNIX 套接字文件

    return 0;
}
 