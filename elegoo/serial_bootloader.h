/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-03-14 09:51:04
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-14 09:51:04
 * @Description  : 串口升级通讯协议
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#pragma once
#include <string>
#include <memory>
#include <vector>

class SerialPort;

class SerialBootloader
{
public:
    SerialBootloader(const std::string &port, int baudrate);
    ~SerialBootloader();
    bool connect();
    void disconnect();
    bool send_and_wait_ack(
        uint8_t *req_buf, uint32_t req_len,
        uint8_t cmd,
        uint8_t *payload, uint32_t payload_len,
        uint8_t *ack_buf, uint32_t ack_len,
        double timeout);
    bool ping();
    bool erase_flash(uint32_t size);
    uint16_t program_flash(uint8_t *buf, uint32_t offset, uint16_t length, uint32_t size);
    bool jump_to_app();
    bool jump_to_bootloader();
    bool update(const char *firmware);
private:
    std::string port;
    int baudrate;
    std::shared_ptr<SerialPort> serial;
};
