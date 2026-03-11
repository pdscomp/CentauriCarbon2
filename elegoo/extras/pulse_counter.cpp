/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-08 10:07:20
 * @LastEditors  : Ben
 * @LastEditTime : 2025-05-23 14:14:23
 * @Description  : Support for GPIO input edge counters
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "pulse_counter.h"

MCU_counter::MCU_counter(std::shared_ptr<Printer> printer, const std::string &pin, double sample_time, double poll_time)
{
    std::shared_ptr<PrinterPins> ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
    std::shared_ptr<PinParams> pin_params = ppins->lookup_pin(pin , false, true);
    this->_mcu = std::static_pointer_cast<MCU>(pin_params->chip);
    this->_oid = this->_mcu->create_oid();
    this->_pin = *pin_params->pin;
    this->_pullup = pin_params->pullup;
    this->_poll_time = poll_time;
    this->_poll_ticks = 0;
    this->_sample_time = sample_time;
    this->_callback = nullptr;
    this->_last_count = 0;
    this->_mcu->register_config_callback(
                    [this]()
                    {
                        build_config();
                    });
    SPDLOG_DEBUG("__func__:{} #1 pin:{} sample_time:{} poll_time:{},oid:{}",__func__,pin,sample_time,poll_time,this->_oid);
}

MCU_counter::~MCU_counter()
{
    SPDLOG_DEBUG("~MCU_counter");
}

void MCU_counter::build_config()
{
    this->_mcu->add_config_cmd(
                "config_counter oid=" + std::to_string(this->_oid) + 
                " pin=" + this->_pin + 
                " pull_up=" + std::to_string(this->_pullup)
                );
    int clock = this->_mcu->get_query_slot(this->_oid);
    this->_poll_ticks = this->_mcu->seconds_to_clock(this->_poll_time);
    uint64_t sample_ticks = this->_mcu->seconds_to_clock(this->_sample_time);
    this->_mcu->add_config_cmd(
                "query_counter oid=" + std::to_string(this->_oid) + 
                " clock=" + std::to_string(clock) + 
                " poll_ticks=" + std::to_string(this->_poll_ticks) + 
                " sample_ticks=" + std::to_string(sample_ticks)
                , true);
    this->_mcu->register_response(
                [this](const json& params)
                {
                    _handle_counter_state(params);
                }
                , "counter_state"
                , this->_oid
                );
    SPDLOG_DEBUG("__func__:{} #1 _mcu->get_name:{} oid:{} {} {} {} {} {} {}",__func__,this->_mcu->get_name(),this->_oid,clock,this->_poll_ticks,this->_poll_time,sample_ticks,this->_pin,this->_pullup);
}

void MCU_counter::setup_callback(std::function<void(double time, uint64_t count, double count_time)> cb)
{
    SPDLOG_DEBUG("__func__:{} #1 ",__func__);
    this->_callback = cb;
    
}

void MCU_counter::_handle_counter_state(const json& params)
{
    // SPDLOG_DEBUG("__func__:{} #1 {} {}",__func__,params.is_object(),params.is_array());
    // SPDLOG_DEBUG("__func__:{} #1 params['oid']:{},_mcu->get_name:{}",__func__,std::stol(params["oid"].get<std::string>()),_mcu->get_name());
    // SPDLOG_DEBUG("__func__:{} #1 {}",__func__,std::stol(params["next_clock"].get<std::string>()));
    // SPDLOG_DEBUG("__func__:{} #1 {}",__func__,std::stol(params["count_clock"].get<std::string>()));
    // SPDLOG_DEBUG("__func__:{} #1 {}",__func__,std::stol(params["count"].get<std::string>()));
    uint64_t next_clock = this->_mcu->clock32_to_clock64(std::stoul(params["next_clock"].get<std::string>()));
    double time = this->_mcu->clock_to_print_time(next_clock - this->_poll_ticks);

    uint64_t count_clock = this->_mcu->clock32_to_clock64(std::stoul(params["count_clock"].get<std::string>()));
    double count_time = this->_mcu->clock_to_print_time(count_clock);

    // handle 32-bit counter overflow
    uint64_t last_count = this->_last_count;
    uint32_t delta_count = (std::stoul(params["count"].get<std::string>()) - last_count) & 0xffffffff;
    uint64_t count = last_count + delta_count;
    this->_last_count = count;

    // SPDLOG_DEBUG("__func__:{} #1 {} {} {} {} {} {} {} {} {}",__func__,next_clock,time,count_clock,count_time,last_count,delta_count,count,this->_poll_ticks,next_clock - this->_poll_ticks);
    if (nullptr !=  this->_callback)
    {
        // SPDLOG_DEBUG("__func__:{},time:{},count:{},count_time:{}",__func__,time,count,count_time);
        this->_callback(time, count, count_time);
    }
}

FrequencyCounter::FrequencyCounter(std::shared_ptr<Printer> printer, const std::string &pin, double sample_time, double poll_time)
{
    SPDLOG_DEBUG("__func__:{} #1 pin:{} sample_time:{} poll_time:{}",__func__,pin,sample_time,poll_time);
    this->_callback = nullptr;
    this->_last_time = DOUBLE_NONE;
    this->_last_count = 0;
    this->_freq = 0.;
    this->_counter = std::make_shared<MCU_counter>(printer,pin,sample_time,poll_time);
    this->_counter->setup_callback(
                    [this](double time, uint64_t count, double count_time)
                    {
                        _counter_callback(time,count,count_time);
                    });
}

FrequencyCounter::~FrequencyCounter()
{
    SPDLOG_DEBUG("~FrequencyCounter");
}

void FrequencyCounter::_counter_callback(double time, uint64_t count, double count_time)
{
    // SPDLOG_DEBUG("__func__:{} #1 {} {} {} {}",__func__,this->_last_time,time,count,count_time);
    if(std::isnan(this->_last_time)) // First sample
    {
        this->_last_time = time;
    }
    else
    {
        double delta_time = count_time - this->_last_time;
        if(delta_time > 0.)
        {
            this->_last_time = count_time;
            uint64_t delta_count = count - this->_last_count;
            this->_freq = delta_count / delta_time;
            // SPDLOG_DEBUG("__func__:{} #1 _freq:{} {} {} {} {} {} {}",__func__,this->_freq,this->_last_time,this->_last_count,delta_time,count_time,count,delta_count);
        }
        else // No counts since last sample
        {
            this->_last_time = time;
            this->_freq = 0.;
            // SPDLOG_DEBUG("__func__:{} #1 _freq:{} {} {} {} {} {}",__func__,this->_freq,this->_last_time,this->_last_count,delta_time,count_time,count);
        }

        if(nullptr != this->_callback)
        {
            this->_callback(time,this->_freq);
        }
    }
    this->_last_count = count;
}

double FrequencyCounter::get_frequency() const 
{
    SPDLOG_DEBUG("__func__:{} #1 _freq:{}",__func__,this->_freq);
    return this->_freq;
}
