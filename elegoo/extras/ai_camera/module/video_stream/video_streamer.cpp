/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2025-05-29 20:55:06
 * @LastEditors  : Jack
 * @LastEditTime : 2025-09-16 20:37:25
 * @Description  :
 *
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "video_streamer.h"
#include "ai_camera_pthread.h"
#include "camera_execption.h"

namespace znp {

VideoStreamer::VideoStreamer(const GetFrameCallback& callback)
    : port_(VSTREAM_PORT), get_frame_callback_(callback), running_(false),
        // server_thread_(nullptr), capture_thread_(nullptr),
        MAX_CLIENTS(VSTREAM_MAX_CLIENT), FRAME_BUFFER_SIZE(VSTREAM_FRAME_MAX_SIZE), FPS(VSTREAM_FPS) {
    frame_buffer_ = new unsigned char[FRAME_BUFFER_SIZE];
}

VideoStreamer::~VideoStreamer() {
    stop();
    delete[] frame_buffer_;
}

void VideoStreamer::start() {
    running_ = true;

    CreateNewThread([this]() { run_server();},  1*1024*1024, "VideoStreamServerLoop");
    CreateNewThread([this]() { capture_loop();},  1*1024*1024, "VideoStreamCaptureLoop");
}

void VideoStreamer::stop() {
    remove_all_client();
    if (running_) {
        running_ = false;
    }
    usleep(100*1000);
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
}

void VideoStreamer::restart() {
    stop();
    usleep(1*1000*1000);
    start();
}

bool VideoStreamer::isAlive() {
    return running_;
}

int VideoStreamer::getClientNums() {
    return active_clients_.size();
}

void VideoStreamer::run_server() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        SPDLOG_ERROR("Failed to create socket\n");
        server_fd = -1;
        return;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_REUSEPORT) failed");
    }

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed\n");
        close(server_fd);
        server_fd = -1;
        stop();
        return;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        SPDLOG_ERROR("Listen failed\n");
        close(server_fd);
        server_fd = -1;
        stop();
        return;
    }

    SPDLOG_INFO("Server running on port {} ", port_ );

    while (running_) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (!running_) break;
            SPDLOG_ERROR("Accept failed\n");
            continue;
        }
        SPDLOG_INFO("New client connectting!!! active_clients_.size(): {} fd: {} ", active_clients_.size(), client_fd );
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            if (active_clients_.size() >= MAX_CLIENTS) {
                SPDLOG_ERROR("Max clients reached. Rejecting connection");
                close(client_fd);
                continue;
            }
            active_clients_.push_back(client_fd);
        }
        SPDLOG_INFO("active_clients_ nums:  {} ", active_clients_.size() );
        std::thread(&VideoStreamer::handle_client, this, client_fd).detach();
    }
    SPDLOG_INFO("close(server_fd) ");
}

void VideoStreamer::handle_client(int client_fd) {
    static int retry_times = 0;
    char buffer[4096];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::string request(buffer);

        SPDLOG_INFO("request :  {} ", request );
        if (request.find("OPTIONS") != std::string::npos) {
            const char* preflight_response =
                "HTTP/1.1 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                "Access-Control-Allow-Headers: Content-Type\r\n"
                "Content-Length: 0\r\n"
                "\r\n";
            send(client_fd, preflight_response, strlen(preflight_response), 0);
            close(client_fd);
            remove_client(client_fd);
            return;
        }
        // Send HTTP response header
        const char* response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=--frame_boundary\r\n"
            "Connection: keep-alive\r\n"
            "\r\n";
        if (send(client_fd, response, strlen(response), 0) < 0) {
            SPDLOG_ERROR("Failed to send header\n");
            close(client_fd);
            remove_client(client_fd);
            return;
        }
    }
    else {
        close(client_fd);
        remove_client(client_fd);
        return;
    }
    const std::string boundary = "\r\n--frame_boundary\r\nContent-Type: image/jpeg\r\nContent-Length: ";
    std::vector<unsigned char> local_buffer(FRAME_BUFFER_SIZE);

    while (running_) {
        try {
            std::unique_lock<std::mutex> lock(frame_mutex_);
            frame_cv_.wait(lock, [this] { return !running_ || has_new_frame_; });
            if (!running_) break;

            // Copy frame data to local buffer
            int size = frame_size_;
            if (size > 0 && size <= FRAME_BUFFER_SIZE) {
                memcpy(local_buffer.data(), frame_buffer_, size);
                has_new_frame_ = false;
                lock.unlock();
            } else {
                lock.unlock();
                SPDLOG_ERROR("Invalid frame size: {} ", size);
                continue;
            }

            // Build and send frame header
            char header[256];
            snprintf(header, sizeof(header), "%s%d\r\n\r\n", boundary.c_str(), size);
            if (send(client_fd, header, strlen(header), 0) < 0) {
                int error_code = errno;
                SPDLOG_ERROR("Failed to send header: {} (Error code: {})", strerror(error_code), error_code);
                retry_times++;
                if (retry_times > 3) {  // 3次发送失败,退出
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            // Send frame data with loop for reliability
            ssize_t total_sent = 0;
            while (total_sent < size) {
                ssize_t sent = send(client_fd, local_buffer.data() + total_sent, size - total_sent, 0);
                if (sent < 0) {
                    int error_code = errno;
                    SPDLOG_ERROR("Failed to send header: {} (Error code: {})", strerror(error_code), error_code);
                    break;
                }
                total_sent += sent;
            }

            if (total_sent != size) {
                int error_code = errno;
                SPDLOG_ERROR("Failed to send header: {} (Error code: {})", strerror(error_code), error_code);
                break;
            }
        }catch (const CameraException &e) {
          SPDLOG_ERROR(e.what());
        }
    }

    close(client_fd);
    remove_client(client_fd);
}

void VideoStreamer::capture_loop() {
    const std::chrono::milliseconds frame_interval(1000 / FPS);  // ~66.67ms
    while (running_) {
        int size = 0;
        if (get_frame_callback_(frame_buffer_, &size)) {
            if (size > 0 && size <= FRAME_BUFFER_SIZE) {
                {
                    std::lock_guard<std::mutex> lock(frame_mutex_);
                    frame_size_ = size;
                    has_new_frame_ = true;
                }
                frame_cv_.notify_all();
            } else {
                SPDLOG_ERROR("Invalid frame size:{} ", size);
            }
        } else {
            // SPDLOG_ERROR("Failed to get frame from callback\n");
        }

        std::this_thread::sleep_for(frame_interval);
    }
}

void VideoStreamer::remove_client(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto it = active_clients_.begin(); it != active_clients_.end(); ++it) {
        if (*it == client_fd) {
            active_clients_.erase(it);
            break;
        }
    }
    SPDLOG_INFO( "active_clients_ nums: {}", active_clients_.size());
}

void VideoStreamer::remove_all_client() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto it = active_clients_.begin(); it != active_clients_.end(); ++it) {
        SPDLOG_INFO( "close fd: {}", *it);
        close(*it);
    }
    active_clients_.clear();
}

}
