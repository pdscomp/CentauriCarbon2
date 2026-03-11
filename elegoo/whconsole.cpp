#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include "json.h"
#include <cerrno>
#include <cstring>
#include <ctime>
#include <memory>
#include <thread>
#include <chrono>

using json = nlohmann::json;

// 设置文件描述符为非阻塞模式
void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 创建 Webhook 套接字连接
int webhook_socket_create(const std::string& uds_filename) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Unable to create socket: " << strerror(errno) << std::endl;
        exit(-1);
    }
    set_nonblock(sock);

    std::cerr << "Waiting for connect to " << uds_filename << std::endl;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, uds_filename.c_str(), sizeof(addr.sun_path) - 1);

    while (true) {
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            if (errno == ECONNREFUSED) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            std::cerr << "Unable to connect socket " << uds_filename
                      << " [" << errno << ", " << strerror(errno) << "]" << std::endl;
            exit(-1);
        }
        break;
    }

    std::cerr << "Connection established." << std::endl;
    return sock;
}

// 读取键盘输入和处理 Webhook 套接字数据
class KeyboardReader {
public:
    KeyboardReader(const std::string& uds_filename) {
        kbd_fd = STDIN_FILENO;
        set_nonblock(kbd_fd);
        webhook_socket = webhook_socket_create(uds_filename);
        FD_ZERO(&fdset);
        FD_SET(kbd_fd, &fdset);
        FD_SET(webhook_socket, &fdset);
        kbd_data.clear();
        socket_data.clear();
    }

    // 处理 Webhook 套接字数据
    void process_socket() {
        char buffer[4096];
        ssize_t bytes_received = recv(webhook_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            // TODO
            std::cerr << "Socket closed or error: " << strerror(errno) << std::endl;
            exit(0);
        }

        std::string data(buffer, bytes_received);
        size_t pos = 0;
        while ((pos = data.find('\x03')) != std::string::npos) {
            std::string part = data.substr(0, pos);
            data = data.substr(pos + 1);
            std::cout << "GOT: " << part << std::endl;
        }
        socket_data = data;
    }

    // 处理键盘输入并发送数据到 Webhook 套接字
    void process_kbd() {
        char buffer[4096];
        ssize_t bytes_read = read(kbd_fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            std::cerr << "Error reading from stdin: " << strerror(errno) << std::endl;
            return;
        }

        std::string data(buffer, bytes_read);
        size_t pos = 0;
        while ((pos = data.find('\n')) != std::string::npos) {
            std::string line = data.substr(0, pos);
            data = data.substr(pos + 1);
            line = kbd_data + line;
            kbd_data.clear();

            if (line.empty() || line[0] == '#') continue;

            try {
                json j = json::parse(line);
                std::string cm = j.dump(); // 获取 JSON 字符串

                std::cout << "SEND: " << cm << std::endl;

                std::string message = cm + "\x03"; // 添加结束符
                ssize_t bytes_sent = send(webhook_socket, message.c_str(), message.size(), 0);
                if (bytes_sent == -1) {
                    std::cerr << "Error sending data: " << strerror(errno) << std::endl;
                }
            } catch (json::parse_error& e) {
                std::cerr << "ERROR: Unable to parse line" << std::endl;
                continue;
            }
        }
        kbd_data = data;
    }

    // 运行主循环，监听键盘和套接字数据
    void run() {
        while (true) {
            fd_set readfds = fdset;
            struct timeval timeout = {1, 0};  // 1秒超时
            int activity = select(FD_SETSIZE, &readfds, nullptr, nullptr, &timeout);
            if (activity == -1) {
                std::cerr << "Select error: " << strerror(errno) << std::endl;
                exit(-1);
            }

            if (FD_ISSET(kbd_fd, &readfds)) {
                process_kbd();
            }

            if (FD_ISSET(webhook_socket, &readfds)) {
                process_socket();
            }
        }
    }

private:
    int kbd_fd;
    int webhook_socket;
    fd_set fdset;
    std::string kbd_data;
    std::string socket_data;
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <socket filename>" << std::endl;
        return -1;
    }

    KeyboardReader reader(argv[1]);
    reader.run();

    return 0;
}

