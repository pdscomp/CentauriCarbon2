// rs485.cpp
#include "rs485.h"
#include <utility>  // for std::move
#include <iostream>
#include <iomanip>

static constexpr uint8_t CRC8_INIT = 0x66;
static constexpr uint16_t CRC16_INIT = 0x913D;

// 预生成的查找表
static const uint8_t crc8_table[256] = {
    0x00, 0x39, 0x72, 0x4B, 0xE4, 0xDD, 0x96, 0xAF, 0xF1, 0xC8, 0x83, 0xBA, 0x15, 0x2C, 0x67, 0x5E,
    0xDB, 0xE2, 0xA9, 0x90, 0x3F, 0x06, 0x4D, 0x74, 0x2A, 0x13, 0x58, 0x61, 0xCE, 0xF7, 0xBC, 0x85,
    0x8F, 0xB6, 0xFD, 0xC4, 0x6B, 0x52, 0x19, 0x20, 0x7E, 0x47, 0x0C, 0x35, 0x9A, 0xA3, 0xE8, 0xD1,
    0x54, 0x6D, 0x26, 0x1F, 0xB0, 0x89, 0xC2, 0xFB, 0xA5, 0x9C, 0xD7, 0xEE, 0x41, 0x78, 0x33, 0x0A,
    0x27, 0x1E, 0x55, 0x6C, 0xC3, 0xFA, 0xB1, 0x88, 0xD6, 0xEF, 0xA4, 0x9D, 0x32, 0x0B, 0x40, 0x79,
    0xFC, 0xC5, 0x8E, 0xB7, 0x18, 0x21, 0x6A, 0x53, 0x0D, 0x34, 0x7F, 0x46, 0xE9, 0xD0, 0x9B, 0xA2,
    0xA8, 0x91, 0xDA, 0xE3, 0x4C, 0x75, 0x3E, 0x07, 0x59, 0x60, 0x2B, 0x12, 0xBD, 0x84, 0xCF, 0xF6,
    0x73, 0x4A, 0x01, 0x38, 0x97, 0xAE, 0xE5, 0xDC, 0x82, 0xBB, 0xF0, 0xC9, 0x66, 0x5F, 0x14, 0x2D,
    0x4E, 0x77, 0x3C, 0x05, 0xAA, 0x93, 0xD8, 0xE1, 0xBF, 0x86, 0xCD, 0xF4, 0x5B, 0x62, 0x29, 0x10,
    0x95, 0xAC, 0xE7, 0xDE, 0x71, 0x48, 0x03, 0x3A, 0x64, 0x5D, 0x16, 0x2F, 0x80, 0xB9, 0xF2, 0xCB,
    0xC1, 0xF8, 0xB3, 0x8A, 0x25, 0x1C, 0x57, 0x6E, 0x30, 0x09, 0x42, 0x7B, 0xD4, 0xED, 0xA6, 0x9F,
    0x1A, 0x23, 0x68, 0x51, 0xFE, 0xC7, 0x8C, 0xB5, 0xEB, 0xD2, 0x99, 0xA0, 0x0F, 0x36, 0x7D, 0x44,
    0x69, 0x50, 0x1B, 0x22, 0x8D, 0xB4, 0xFF, 0xC6, 0x98, 0xA1, 0xEA, 0xD3, 0x7C, 0x45, 0x0E, 0x37,
    0xB2, 0x8B, 0xC0, 0xF9, 0x56, 0x6F, 0x24, 0x1D, 0x43, 0x7A, 0x31, 0x08, 0xA7, 0x9E, 0xD5, 0xEC,
    0xE6, 0xDF, 0x94, 0xAD, 0x02, 0x3B, 0x70, 0x49, 0x17, 0x2E, 0x65, 0x5C, 0xF3, 0xCA, 0x81, 0xB8,
    0x3D, 0x04, 0x4F, 0x76, 0xD9, 0xE0, 0xAB, 0x92, 0xCC, 0xF5, 0xBE, 0x87, 0x28, 0x11, 0x5A, 0x63
};

// CRC-16 查找表（按 MSB-first 左移规则生成）
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

// 查表法 CRC-8 计算函数 时间复杂度 O(n)
static uint8_t crc8_lookup(const std::vector<uint8_t>& data)
{
    uint8_t crc = CRC8_INIT;
    for (const auto& byte : data) {
        crc = crc8_table[crc ^ byte];
    }
    return crc;
}

static uint16_t crc16_lookup(const std::vector<uint8_t>& data)
{
    uint16_t crc = CRC16_INIT; // 初始值
    for (const auto& byte : data) {
        uint8_t tbl_idx = (crc >> 8) ^ byte;   // 高8位异或数据
        crc = (crc << 8) ^ crc16_table[tbl_idx];
    }
    return crc;
}

// print vector<uint8_t> for debugging
static void print_vector_uint8(const std::vector<uint8_t>& vec)
{
    for (const auto& byte : vec) {
        std::cout << "0x" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte) << " ";
    }
    std::cout << std::dec << std::endl;
}

// Check if the frame is a valid frame head
static bool check_short_frame_head(const std::vector<uint8_t>& frame)
{
    if (frame.size() < 4) return false; // Minimum size for a valid frame
    if (frame[0] != 0x3D) return false; // Check frame head
    if (frame[1] != 0x80 && frame[1] != 0x00) return false; // Check frame type
    uint8_t crc8 = crc8_lookup(std::vector<uint8_t>(frame.begin(), frame.begin() + 3));

    if (frame[3] != crc8) return false; // Check CRC-8

    return true;
}

static bool check_short_frame(const std::vector<uint8_t>& frame)
{
    if (!check_short_frame_head(frame)) return false;
    uint8_t frame_length = frame[2];
    if (frame.size() < frame_length) return false;
    uint16_t crc16 = (frame[frame_length - 2] << 8) | frame[frame_length - 1];
    uint16_t crc16_calc = crc16_lookup(std::vector<uint8_t>(frame.begin(), frame.begin() + frame_length - 2));
    if (crc16 != crc16_calc) return false; // Check CRC-16

    return true;
}

Serial::Serial(const std::string &port, const int &baudrate) : port(port), baudrate(baudrate)
{
    serial_port = std::make_shared<SerialPort>(port, baudrate);
}

Serial::~Serial()
{
    serial_port->close();
}

bool Serial::connect()
{
    if (!serial_port->open()) {
        return false;
    }

    return true;
}

void Serial::disconnect()
{
    serial_port->close();
}

ssize_t Serial::read(void *buf, size_t size)
{
    return serial_port->read(buf, size);
}

ssize_t Serial::write(const void *buf, size_t size)
{
    return serial_port->write(buf, size);
}

RS485::RS485(std::string port, int baudrate)
    : serial(std::move(port), baudrate)
{
}

RS485::~RS485()
{
    serial.disconnect();
}

std::vector<uint8_t> RS485::send_and_wait(const std::vector<uint8_t>& data, std::chrono::milliseconds timeout)
{
    SerialRequest request;
    request.tx_data = data;
    request.timeout = timeout;
    std::future<std::vector<uint8_t>> future = request.result_promise.get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        request_queue.push_back(std::move(request));
    }
    cv.notify_one();

    return future.get(); // 阻塞等待结果或异常
}

void RS485::start()
{
    if (!serial.connect()) {
        std::cerr << "Failed to open serial port" << std::endl;
        return;
    }

    running = true;
    serial_thread = std::thread(&RS485::serial_thread_loop, this);
}

void RS485::stop()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        running = false;
    }
    cv.notify_all();
    if (serial_thread.joinable()) {
        serial_thread.join();
    }
}

void RS485::serial_thread_loop()
{
    while (true) {
        SerialRequest request;

        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [this] { return !request_queue.empty() || !running; });

            if (!running && request_queue.empty()) {
                serial.disconnect();
                break;
            }

            request = std::move(request_queue.front());
            request_queue.pop_front();
        }

        std::vector<uint8_t> tx_data = std::move(request.tx_data);

        if (tx_data.empty()) {
            // std::cout << "No data to send." << std::endl;
            continue;
        }

        serial.write(tx_data.data(), tx_data.size());
        // std::cout << "serial_thread_loop() Sent data: ";
        // print_vector_uint8(tx_data);

        bool is_valid_frame_head = false;
        bool jump_over_promise = false;
        bool is_frame_valid = false;
        size_t frame_length = 0;
        std::deque<uint8_t> rx_data;

        std::chrono::steady_clock::time_point deadline;

        if (request.timeout.count() <= 0) {
            jump_over_promise = true;
            deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
            try {
                request.result_promise.set_value(std::vector<uint8_t>());
            } catch (const std::exception& e) {
                std::cerr << "Failed to set result_promise: " << e.what() << std::endl;
            }
        } else {
            deadline = std::chrono::steady_clock::now() + request.timeout;
        }

        do {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            uint8_t buffer[256];
            ssize_t bytes_read = serial.read(buffer, sizeof(buffer));

            if (bytes_read < 0) {
                std::cerr << "Error reading from serial port" << std::endl;
                break;
            } else if (bytes_read > 0) {
                rx_data.insert(rx_data.end(), buffer, buffer + bytes_read);
            } else {
                continue;
            }

            // std::cout << "serial_thread_loop() Received data: ";
            // print_vector_uint8(std::vector<uint8_t>(rx_data.begin(), rx_data.end()));

            while (rx_data.size() >= 4) {
                if (is_valid_frame_head) {
                    if (rx_data.size() >= frame_length) {
                        if (check_short_frame(std::vector<uint8_t>(rx_data.begin(), rx_data.begin() + frame_length))) {
                            is_frame_valid = true;
                        } else {
                            rx_data.erase(rx_data.begin(), rx_data.begin() + frame_length);
                            std::cerr << "Invalid rx_data received." << std::endl;
                        }
                    }
                    break;
                } else {
                    if (check_short_frame_head(std::vector<uint8_t>(rx_data.begin(), rx_data.end()))) {
                        is_valid_frame_head = true;
                        frame_length = static_cast<size_t>(rx_data[2]);
                    } else {
                        rx_data.pop_front();
                        // std::cout << "Frame head not found, removing first byte." << std::endl;
                    }
                }
            }
        } while (std::chrono::steady_clock::now() < deadline);

        if (!jump_over_promise) {
            try {
                if (is_frame_valid) {
                    request.result_promise.set_value(std::vector<uint8_t>(rx_data.begin(), rx_data.begin() + frame_length));
                    rx_data.erase(rx_data.begin(), rx_data.begin() + frame_length);
                } else {
                    request.result_promise.set_value(std::vector<uint8_t>());
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to set result_promise: " << e.what() << std::endl;
            }
        }
    }
}
