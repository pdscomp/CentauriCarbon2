/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2024-12-16 20:51:41
 * @Description  : Handle pwm output pins with variable frequency
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory.h>
#include "json.h"
#include "chipbase.h"

class Printer;
class MCU;
class ConfigWrapper;
class CommandWrapper;
class GCodeCommand;
namespace elegoo {
namespace extras {
class MCU_pwm_cycle
{
public:
    MCU_pwm_cycle(
        const std::shared_ptr<PinParams> & pin_params,
        double cycle_time, double start_value, 
        double shutdown_value);
    ~MCU_pwm_cycle();

    void set_pwm_cycle(double print_time, double value, double cycle_time);
private:
    void build_config();

private:
    std::shared_ptr<MCU> mcu;
    std::shared_ptr<CommandWrapper> set_cmd;
    std::shared_ptr<CommandWrapper> set_cycle_ticks;
    double cycle_time;
    uint64_t cycle_ticks;
    std::string pin;
    int invert;
    uint64_t last_clock;
    double shutdown_value;
    double start_value;
    uint32_t oid;
};

class PrinterOutputPWMCycle
{
public:
    PrinterOutputPWMCycle(std::shared_ptr<ConfigWrapper> config);
    ~PrinterOutputPWMCycle();

    json get_status(double eventtime);
    void cmd_SET_PIN(std::shared_ptr<GCodeCommand> gcmd);
private:
    void set_pin(double print_time, double value, double cycle_time);
    std::vector<std::string> split(
        const std::string& str, char delimiter);
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<MCU_pwm_cycle> mcu_pin;
    double last_print_time;
    double last_cycle_time;
    double default_cycle_time;
    double scale;
    double last_value;
    double shutdown_value;
};



std::shared_ptr<PrinterOutputPWMCycle> pwm_cycle_time_load_config_prefix(
    std::shared_ptr<ConfigWrapper> config);
 

}
}