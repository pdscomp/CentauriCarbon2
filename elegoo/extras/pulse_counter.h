/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-08 10:07:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-28 20:31:00
 * @Description  : Support for GPIO input edge counters
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include <memory>
#include <string>
#include <set>
#include <functional>
#include <algorithm>
#include "configfile.h"
#include "mcu.h"
#include "printer.h"

class MCU_counter
{
public:
    MCU_counter(std::shared_ptr<Printer> printer, const std::string &pin, double sample_time, double poll_time);
    ~MCU_counter();
    void setup_callback(std::function<void(double time, uint64_t count, double count_time)> cb);
    void build_config();
    void _handle_counter_state(const json& params);
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<MCU> _mcu;
    uint32_t _oid;
    std::string _pin;
    int _pullup;
    double _poll_time;
    uint64_t _poll_ticks;
    double _sample_time;
    uint64_t _last_count;
    std::function<void(double time, uint64_t count, double count_time)> _callback;
};


class FrequencyCounter
{
public:
    FrequencyCounter(std::shared_ptr<Printer> printer, const std::string &pin, double sample_time, double poll_time);
    ~FrequencyCounter();
    double get_frequency() const;
private:
    void _counter_callback(double time, uint64_t count, double count_time);
    std::shared_ptr<Printer> printer;
    std::function<void(double, double)> _callback;
    std::shared_ptr<MCU_counter> _counter;
    double _last_time;
    uint64_t _last_count;
    double _freq;
};

