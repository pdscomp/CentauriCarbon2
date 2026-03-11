/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2024-11-07 14:24:31
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-20 16:37:57
 * @Description  : 封装Linux GPIO控制
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include <memory>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <functional>
#include "msgproto.h"
#include "c_helper.h"
#include "json.h"
#include "pins.h"
#include "any.h"
#include "chipbase.h"

class SelectReactor;
class ReactorTimer;
class ReactorCompletion;
class ConfigWrapper;
class Printer;
class MCU;
class CommandWrapper;

class MCU_host_digital_pin : public MCU_pins
{
public:
    MCU_host_digital_pin(std::shared_ptr<PinParams> pin_params);
    ~MCU_host_digital_pin();
    void set_digital(uint8_t value);
    void set_direction(uint8_t in);
    uint8_t get_digital();

private:
    std::string pin_name;
    uint8_t port;
    uint8_t pin_idx;
    uint32_t pin;
    int fd_direction = -1;
    int fd_value = -1;
};

class HostWarpper : public std::enable_shared_from_this<HostWarpper>, public ChipBase
{
public:
    HostWarpper() = default;
    ~HostWarpper() = default;
    void init(std::shared_ptr<ConfigWrapper> config);
    std::shared_ptr<MCU_pins> setup_pin(const std::string &pin_type, std::shared_ptr<PinParams> pin_params);

private:
    std::shared_ptr<Printer> printer;
};