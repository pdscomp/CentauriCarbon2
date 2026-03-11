// rs485.h
#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <future>
#include <chrono>
#include <string>
#include "serial.h" // 假设 Serial 类在 serial.h 中

struct SerialRequest
{
    std::vector<uint8_t> tx_data;
    std::promise<std::vector<uint8_t>> result_promise;
    std::chrono::milliseconds timeout;
};

class Serial
{
public:
    Serial(const std::string &port, const int &baudrate);
    ~Serial();
    
    bool connect();
    void disconnect();
    ssize_t read(void *buf, size_t size);
    ssize_t write(const void *buf, size_t size);

private:
    std::string port;
    int baudrate;
    std::shared_ptr<SerialPort> serial_port;    
};

class RS485
{
public:
    RS485(std::string port, int baudrate);
    ~RS485();

    std::vector<uint8_t> send_and_wait(const std::vector<uint8_t>& data, std::chrono::milliseconds timeout);

    void start();
    void stop();

private:
    void serial_thread_loop();

    Serial serial;
    bool running = false;
    std::thread serial_thread;
    std::deque<SerialRequest> request_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
};
