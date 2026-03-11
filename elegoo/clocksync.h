/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:13
 * @LastEditors  : Ben
 * @LastEditTime : 2025-07-16 10:21:12
 * @Description  : Micro-controller clock synchronization
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory>
#include "json.h"
#include "reactor.h"
#include "serialhdl.h"
#define RTT_AGE (0.000010 / (60.0 * 60.0))
#define DECAY (1.0 / 30.0)
#define TRANSMIT_EXTRA 0.001

class ClockSync
{
public:
    ClockSync(std::shared_ptr<SelectReactor> reactor, const std::string& name = "");
    ~ClockSync();
    
    virtual void connect(std::shared_ptr<SerialReader> serial);
    virtual void connect_file(std::shared_ptr<SerialReader> serial, bool pace = false);
    virtual uint64_t print_time_to_clock(double print_time);
    virtual double clock_to_print_time(double clock);
    virtual uint64_t get_clock(double eventtime);
    virtual double estimate_clock_systime(double reqclock);
    virtual double estimated_print_time(double eventtime);
    virtual uint64_t clock32_to_clock64(uint32_t clock32);
    virtual bool is_active();
    virtual std::string dump_debug();
    virtual std::string stats(double eventtime);
    virtual std::pair<double, double> calibrate_clock(double print_time, double eventtime);
    
    std::tuple<double, double, double> get_clock_est();
    double get_mcu_freq();
    std::mutex& get_mutex();
protected:
    double get_clock_event(double eventtime);
    void handle_clock(json params);

protected:
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<SerialReader> serial;
    std::shared_ptr<ReactorTimer> get_clock_timer;
    std::shared_ptr<command_queue> cmd_queue;
    std::mutex mutex;
    // get_clock_cmd
    // cmd_queue
    int queries_pending;
    double mcu_freq;
    uint64_t last_clock;
    std::tuple<double, double, double> clock_est;
    std::vector<uint8_t> get_clock_cmd;
    double min_half_rtt;
    double min_rtt_time;
    double time_avg;
    double time_variance;
    double clock_avg;
    double clock_covariance;
    double prediction_variance;
    double last_prediction_time;
    std::string name;
};

class SecondarySync : public ClockSync
{
public:
    SecondarySync(std::shared_ptr<SelectReactor> reactor, 
        std::shared_ptr<ClockSync> main_sync, const std::string& name = "");
    ~SecondarySync();

    void connect(std::shared_ptr<SerialReader> serial);
    void connect_file(std::shared_ptr<SerialReader> serial, bool pace = false);
    uint64_t print_time_to_clock(double print_time);
    double clock_to_print_time(double clock);
    std::string dump_debug();
    std::string stats(double eventtime);
    std::pair<double, double> calibrate_clock(double print_time, double eventtime);

private:
    std::shared_ptr<ClockSync> main_sync;
    std::pair<double, double> clock_adj;
    double last_sync_time;
};