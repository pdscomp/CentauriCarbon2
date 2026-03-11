/***************************************************************************** 
 * @Author       : Jack
 * @Date         : 2025-01-10 11:29:45
 * @LastEditors  : Jack
 * @LastEditTime : 2025-07-21 12:12:00
 * @Description  : 
 * 
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "server_socket.h"
#include "ai_camera_pthread.h"
#include "spdlog/spdlog.h"

namespace znp {

bool ClientConnection::send(const std::string& message) {
    SPDLOG_INFO("send: {}", message);
    ssize_t bytes_sent = ::send(m_sockfd, message.c_str(), message.size(), 0);
    return bytes_sent == static_cast<ssize_t>(message.size());
}

bool ClientConnection::receive(std::string& message) {
    char buffer[MAX_RECEIVE];
    ssize_t bytes_received = recv(m_sockfd, buffer, MAX_RECEIVE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received - 1] = '\0';
        message.assign(buffer, bytes_received);
        return true;
    }
    return false;
}


ServerSocket::ServerSocket(const std::string& socket_path, Callback callback) : m_sockfd(-1) {
    this->callback_ = callback;
    m_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_sockfd < 0) {
        perror("Socket creation failed");
        throw std::runtime_error("Socket creation failed");
    }

    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    strncpy(m_addr.sun_path, socket_path.c_str(), sizeof(m_addr.sun_path) - 1);

    unlink(socket_path.c_str());

    if (bind(m_sockfd, (struct sockaddr*)&m_addr, sizeof(m_addr)) < 0) {
        perror("Bind failed");
        throw std::runtime_error("Bind failed");
    }

    if (listen(m_sockfd, 1) < 0) { // backlog of 10 connections
        perror("Listen failed");
        throw std::runtime_error("Listen failed");
    }

    CreateNewThread([this]() { serverLoop0();},  1*1024*1024, "ServerSocket0");
    CreateNewThread([this]() { serverLoop1();},  1*1024*1024, "ServerSocket1");
}

ServerSocket::~ServerSocket() {
    if (m_sockfd != -1) {
        close(m_sockfd);
        m_sockfd = -1;
    }

    unlink(m_addr.sun_path);
}

void ServerSocket::acceptConnection(std::shared_ptr<ClientConnection> &m_client_connection) {
    sockaddr_un addr;
    socklen_t addr_size = sizeof(addr);
    sleep(3);
    SPDLOG_DEBUG("Accepting connection ·······················");
    int new_fd = accept(m_sockfd, (struct sockaddr*)&addr, &addr_size);
    if (new_fd < 0) {
        perror("Accept failed");
        throw std::runtime_error("Accept failed");
    }

    m_client_connection = std::make_shared<ClientConnection>(new_fd);
}

bool ServerSocket::sendMsg(const std::string& message) {
    if (!m0_client_connection && !m1_client_connection) {
        SPDLOG_ERROR("No client_connection!");
        return false;
    }
    
    if (m0_client_connection) {
        m0_client_connection->send(message);
    }
    if (m1_client_connection) {
        m1_client_connection->send(message);
    }
    return true;
}

void ServerSocket::serverLoop0() {
    while(true) {
        acceptConnection(m0_client_connection);
        SPDLOG_INFO("client0 connection!");
        if (m0_client_connection) {
            while (true) {
                std::string message;
                if (m0_client_connection->receive(message)) {
                    SPDLOG_INFO("received client0 message: {}", message);
                    std::lock_guard<std::mutex> lock(this->comm_mutex_);
                    callback_(message);
                } else {
                    break;
                }
            }
            m0_client_connection.reset();
        }
    }
}

void ServerSocket::serverLoop1() {
    while(true) {
        acceptConnection(m1_client_connection);
        SPDLOG_INFO("client1 connection!");
        if (m1_client_connection) {
            while (true) {
                std::string message;
                if (m1_client_connection->receive(message)) {
                    SPDLOG_INFO("received client1 message: {}", message);
                    std::lock_guard<std::mutex> lock(this->comm_mutex_);
                    callback_(message);
                } else {
                    break;
                }
            }
            m1_client_connection.reset();
        }
    }
}

}