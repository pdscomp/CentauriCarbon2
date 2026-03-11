/*****************************************************************************
 * @Author       : loping
 * @Date         : 2024-11-26 10:38:23
 * @LastEditors  : coconut
 * @LastEditTime : 2025-02-19 18:35:09
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef __CHIPBASE_H__
#define __CHIPBASE_H__

#pragma once

#include <string>

#include "homing.h"
#include "json.h"

class ConfigWrapper;
class Printer;
class MCU;
class MCU_pins;
class PinParams;
class MCU_stepper;
class ReactorCompletion;

class ChipBase
{
public:
    ChipBase() {}
    ChipBase(std::shared_ptr<ConfigWrapper> config);
    virtual ~ChipBase();
    virtual std::shared_ptr<MCU_pins> setup_pin(const std::string &pin_type, std::shared_ptr<PinParams> pin_params);

public:
    int chip_type = 0;
    std::shared_ptr<Printer> printer;
};

class MCU_pins
{
public:
    MCU_pins() {}
    MCU_pins(std::shared_ptr<ChipBase> mcu, std::shared_ptr<PinParams> pin_params);
    virtual ~MCU_pins();
    virtual void setup_cycle_time(double cycle_time, bool hardware_pwm) {}
    virtual void setup_max_duration(double max_duration) {}
    virtual void setup_start_value(double start_value, double shutdown_value) {}
    virtual void set_pwm(double print_time, double value) {}
    virtual void set_digital(double print_time, double value) {}

    // Interface for MCU_endstop
    virtual std::shared_ptr<MCU> get_mcu() { return nullptr; }
    virtual void add_stepper(std::shared_ptr<MCU_stepper> stepper) {}
    virtual std::vector<std::shared_ptr<MCU_stepper>> get_steppers() { return {}; }
    virtual std::shared_ptr<ReactorCompletion> home_start(double print_time, double sample_time, int sample_count, double rest_time, bool triggered, double accel_time = 0.) { return nullptr; }
    virtual double home_wait(double home_end_time) { return 0.; }
    virtual bool query_endstop(double print_time) { return false; }

    // ProbeEndstopWrapper
    virtual void multi_probe_begin() {}
    virtual void multi_probe_end() {}
    virtual std::vector<double> probing_move(const std::vector<double> &pos, double speed) { return {}; }
    virtual void probe_prepare(std::shared_ptr<elegoo::extras::HomingMove> hmove) {}
    virtual void probe_finish(std::shared_ptr<elegoo::extras::HomingMove> hmove) {}

    std::shared_ptr<ChipBase> mcu;
    std::string pin;
    std::function<double(void)> get_position_endstop;
};

class PinParams
{
public:
    PinParams() {}
    ~PinParams() {}

    std::shared_ptr<ChipBase> chip;
    std::shared_ptr<std::string> chip_name;
    std::shared_ptr<std::string> pin;
    std::shared_ptr<std::string> share_type;
    int invert;
    int pullup;
    std::shared_ptr<void> pin_class;
};

#endif