#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include "spdlog/spdlog.h"

#define VSTREAM_FRAME_MAX_SIZE (300*1024)
#define VSTREAM_PORT 8080
#define VSTREAM_FPS 15
#define VSTREAM_MAX_CLIENT 4

namespace znp {

class VideoStreamer {
public:
    using GetFrameCallback = std::function<bool(unsigned char*, int*)>;

    VideoStreamer(const GetFrameCallback& callback);
    ~VideoStreamer();

    void start();
    void stop();
    void restart();
    bool isAlive();
    int getClientNums();

private:
    void run_server();
    void handle_client(int client_fd);
    void capture_loop();
    void remove_client(int client_fd);
    void remove_all_client();

    int port_;
    int server_fd;
    GetFrameCallback get_frame_callback_;
    std::atomic<bool> running_;
    // std::shared_ptr<std::thread> server_thread_;
    // std::shared_ptr<std::thread> capture_thread_;
    std::mutex clients_mutex_;
    std::vector<int> active_clients_;
    const int MAX_CLIENTS;
    const int FRAME_BUFFER_SIZE;
    const int FPS;

    // Frame buffer and synchronization
    unsigned char* frame_buffer_;
    int frame_size_;
    std::mutex frame_mutex_;
    std::condition_variable frame_cv_;
    bool has_new_frame_;
};


}
