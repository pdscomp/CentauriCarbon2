/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-03 20:53:11
 * @Description  : Handle pwm output pins with variable frequency
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "pwm_cycle_time.h"
#include "printer.h"


const double PIN_MIN_TIME = 0.100;
const double MAX_SCHEDULE_TIME = 5.0;
namespace elegoo {
namespace extras {

MCU_pwm_cycle::MCU_pwm_cycle(
    const std::shared_ptr<PinParams> & pin_params,
    double cycle_time, double start_value, 
    double shutdown_value) : cycle_time(cycle_time) 
{
    mcu = std::static_pointer_cast<MCU>(pin_params->chip);
    mcu->register_config_callback([this](){
        build_config();
    });
    pin = std::static_pointer_cast<std::string>(pin_params->pin)->c_str();
    invert = pin_params->invert;

    if (invert) 
    {
        start_value = 1.0 - start_value;
        shutdown_value = 1.0 - shutdown_value;
    }

    start_value = std::max(0.0, std::min(1.0, start_value));
    shutdown_value = std::max(0.0, std::min(1.0, shutdown_value));

    last_clock = 0.0;
    cycle_ticks = 0.0;
}

MCU_pwm_cycle::~MCU_pwm_cycle()
{

}

void MCU_pwm_cycle::set_pwm_cycle(double print_time, double value, double cycle_time)
{
    uint64_t clock = mcu->print_time_to_clock(print_time);
    double minclock = last_clock;

    uint64_t cycle_ticks = mcu->seconds_to_clock(cycle_time);


    if (cycle_ticks != cycle_ticks) 
    {
        if (cycle_ticks >= (1U << 31)) 
        {
            throw elegoo::common::CommandError("PWM cycle time too large");
        }
        set_cycle_ticks->send({oid, cycle_ticks}, minclock, clock);
        cycle_ticks = cycle_ticks;
    }

    // Process the PWM value (invert if necessary)
    if (invert) {
        value = 1.0 - value;
    }
    
    // Clamp the value between 0 and 1, then scale it to cycle_ticks
    int pwm_value = static_cast<int>(std::max(0.0, std::min(1.0, value)) * cycle_ticks + 0.5);

    // Send the PWM update
    SPDLOG_INFO("set_pwm_cycle  pin: {}",pin);
    set_cmd->send({ oid, clock, (uint64_t)pwm_value}, last_clock, clock);
    last_clock = clock;
}

void MCU_pwm_cycle::build_config()
{ 
    std::shared_ptr<command_queue> cmd_queue = 
        mcu->alloc_command_queue();

    double curtime = get_monotonic();
    double printtime = mcu->estimated_print_time(curtime);
    last_clock = mcu->print_time_to_clock(printtime + 0.200);
    uint64_t cycle_ticks = mcu->seconds_to_clock(cycle_time);

    if (shutdown_value != 0.0 && shutdown_value != 1.0) 
    {
        // mcu->get_printer()->config_error("shutdown value must be 0.0 or 1.0 on soft pwm");
    }

    if (cycle_ticks >= (1 << 31))
    {
        // mcu->get_printer()->config_error("PWM pin cycle time too large");
    }

    mcu->request_move_queue_slot();
    oid = mcu->create_oid();
    mcu->add_config_cmd(
        "config_digital_out oid=" + std::to_string(oid) + " pin=" + 
        pin + " value=" + std::to_string(start_value >= 1.0) +
        " default_value=" + std::to_string(shutdown_value >= 0.5) + 
        " max_duration=0");

    mcu->add_config_cmd(
        "set_digital_out_pwm_cycle oid=" + std::to_string(oid) + 
        " cycle_ticks=" + std::to_string(static_cast<int>(cycle_ticks)));
    this->cycle_ticks = cycle_ticks;
    int svalue = static_cast<int>(start_value * cycle_ticks + 0.5);
    mcu->add_config_cmd(
        "queue_digital_out oid=" + std::to_string(oid) + 
        " clock=" + std::to_string(static_cast<int>(last_clock)) + 
        " on_ticks=" + std::to_string(svalue));

    set_cmd = mcu->lookup_command(
        "queue_digital_out oid=%c clock=%u on_ticks=%u", cmd_queue);
    set_cycle_ticks = mcu->lookup_command(
        "set_digital_out_pwm_cycle oid=%c cycle_ticks=%u", cmd_queue);
}


PrinterOutputPWMCycle::PrinterOutputPWMCycle(
    std::shared_ptr<ConfigWrapper> config)
{
SPDLOG_INFO("PrinterOutputPWMCycle init!");
    printer = config->get_printer();
    last_print_time = 0;
    double cycle_time = config->getdouble("cycle_time", 0.1, 
        DOUBLE_NONE, MAX_SCHEDULE_TIME, 0);
    last_cycle_time = default_cycle_time = cycle_time;
    scale = config->getdouble("scale", 1, DOUBLE_NONE,
        DOUBLE_NONE, 0);
    last_value = config->getdouble(
        "value", 0, 0, scale) / scale;
    shutdown_value = config->getdouble(
        "shutdown_value", 0, 0, scale) / scale;

    std::shared_ptr<PrinterPins> ppins = 
        any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));

    std::shared_ptr<PinParams> pin_params = 
        ppins->lookup_pin(config->get("pin"), true);
    mcu_pin = std::make_shared<MCU_pwm_cycle>(pin_params,
        cycle_time, last_value, shutdown_value);

    std::vector<std::string> parts = split(config->get_name(), ' ');
    std::string pin_name = (parts.size() > 1) ? parts[1] : "";

    std::shared_ptr<GCodeDispatch> gcode = 
        any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));  

    gcode->register_mux_command("SET_PIN", "PIN", pin_name,
        [this](std::shared_ptr<GCodeCommand> gcmd){
            cmd_SET_PIN(gcmd);
        },
        "Set the value of an output pin"
        );  
SPDLOG_INFO("PrinterOutputPWMCycle init success!!");
}

PrinterOutputPWMCycle::~PrinterOutputPWMCycle()
{

}


json PrinterOutputPWMCycle::get_status(double eventtime)
{
    json val;
    val["value"] = last_value;

    return val;
}

void PrinterOutputPWMCycle::cmd_SET_PIN(std::shared_ptr<GCodeCommand> gcmd)
{
    double value = gcmd->get_double("VALUE", DOUBLE_INVALID, 0, scale);
    value /= scale; 

    double cycle_time = gcmd->get_double("CYCLE_TIME", 
        default_cycle_time, DOUBLE_NONE, MAX_SCHEDULE_TIME, 0);

    std::shared_ptr<ToolHead> toolhead = 
        any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
    toolhead->register_lookahead_callback(
        [this, value, cycle_time](double print_time) {
            set_pin(print_time, value, cycle_time);
        });
}

void PrinterOutputPWMCycle::set_pin(double print_time, double value, double cycle_time)
{
    if (value == last_value && cycle_time == last_cycle_time) 
    {
        return;
    }

    print_time = std::max(print_time, last_print_time + PIN_MIN_TIME);
    mcu_pin->set_pwm_cycle(print_time, value, cycle_time);

    last_value = value;
    last_cycle_time = cycle_time;
    last_print_time = print_time;
}


std::vector<std::string> PrinterOutputPWMCycle::split(
    const std::string& str, char delimiter) 
{
    std::vector<std::string> tokens;
    std::string token;
    std::stringstream ss(str);

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}


std::shared_ptr<PrinterOutputPWMCycle> pwm_cycle_time_load_config_prefix(
    std::shared_ptr<ConfigWrapper> config) {
return std::make_shared<PrinterOutputPWMCycle>(config);
}

}
}