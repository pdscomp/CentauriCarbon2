/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:10
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-27 13:44:04
 * @Description  : Micro-controller clock synchronization
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "clocksync.h"
#include "msgproto.h"
#include <iostream>
#include <cmath>
#include <iomanip>
#include <sstream>
#include "logger.h"
ClockSync::ClockSync(std::shared_ptr<SelectReactor> reactor, const std::string &name) : reactor(reactor), queries_pending(0), mcu_freq(1),
                                                               last_clock(0), clock_est(std::make_tuple(0, 0, 0)),
                                                               min_half_rtt(999999999.9), min_rtt_time(0), time_avg(0),
                                                               time_variance(0), clock_avg(0), clock_covariance(0),
                                                               prediction_variance(0), last_prediction_time(0), name(name)
{
    get_clock_timer = reactor->register_timer(
        [this](double eventtime)
        { return this->get_clock_event(eventtime); }, _NEVER, std::string("clocksync ") + name);
}

ClockSync::~ClockSync()
{
}

void ClockSync::connect(std::shared_ptr<SerialReader> serial)
{
    this->serial = serial;
    mcu_freq = serial->get_msgparser()->get_constant_float("CLOCK_FREQ");

    // 加载初始时钟和频率
    json params = serial->send_with_response("get_uptime", "uptime");
    last_clock = (((uint64_t)std::stoul(params["high"].get<std::string>())) << 32) | std::stoul(params["clock"].get<std::string>());
    // SPDLOG_DEBUG("__func__:{},std::stoul(params['high'].get<std::string>()):{},last_clock:{}",__func__,std::stoul(params["high"].get<std::string>()),last_clock);
    clock_avg = last_clock;
    time_avg = params["#sent_time"].get<double>();
    clock_est = {time_avg, clock_avg, mcu_freq};
    prediction_variance = std::pow(0.001 * mcu_freq, 2);

    // SPDLOG_DEBUG("__func__:{},prediction_variance:{},time_avg:{},clock_avg:{},mcu_freq:{}",__func__,prediction_variance,time_avg,clock_avg,mcu_freq);
    // 启动定时获取时钟的计时器
    for (int i = 0; i < 8; ++i)
    {
        reactor->pause(get_monotonic() + 0.050);
        last_prediction_time = -9999.0;
        params = serial->send_with_response("get_clock", "clock");
        handle_clock(params);
    }

    // 设置命令和命令队列
    get_clock_cmd = serial->get_msgparser()->create_command("get_clock");
    cmd_queue = serial->alloc_command_queue();

    // 注册时钟处理回调
    serial->register_response([this](const json params)
                              { handle_clock(params); }, "clock");

    // 更新定时器
    reactor->update_timer(get_clock_timer, _NOW);
}

void ClockSync::connect_file(std::shared_ptr<SerialReader> serial, bool pace)
{
    this->serial = serial;
}

uint64_t ClockSync::print_time_to_clock(double print_time)
{
    return static_cast<uint64_t>(print_time * mcu_freq);
}

double ClockSync::clock_to_print_time(double clock)
{
    return clock / mcu_freq;
}

uint64_t ClockSync::get_clock(double eventtime)
{
    std::unique_lock<std::mutex> lock(mutex);
    double sample_time = std::get<0>(clock_est);
    double clock = std::get<1>(clock_est);
    double freq = std::get<2>(clock_est);

    // SPDLOG_DEBUG("eventtime:{},sample_time:{},clock:{},freq:{}",eventtime,sample_time,clock,freq);
    return static_cast<uint64_t>(clock + (eventtime - sample_time) * freq);
}

double ClockSync::estimate_clock_systime(double reqclock)
{
    std::unique_lock<std::mutex> lock(mutex);
    double sample_time = std::get<0>(clock_est);
    double clock = std::get<1>(clock_est);
    double freq = std::get<2>(clock_est);

    return (reqclock - clock) / freq + sample_time;
}

double ClockSync::estimated_print_time(double eventtime)
{
    return clock_to_print_time(get_clock(eventtime));
}

uint64_t ClockSync::clock32_to_clock64(uint32_t clock32)
{
    uint64_t last_clock = this->last_clock;
    uint64_t clock_diff = (clock32 - last_clock) & 0xffffffff;
    clock_diff -= (clock_diff & 0x80000000) << 1;
    return last_clock + clock_diff;
}

bool ClockSync::is_active()
{
    return queries_pending <= 4;
}

std::string ClockSync::dump_debug()
{
    double sample_time = std::get<0>(clock_est);
    double clock = std::get<1>(clock_est);
    double freq = std::get<2>(clock_est);

    std::ostringstream oss;
    oss << "clocksync state: mcu_freq=" << static_cast<int>(mcu_freq) << " last_clock=" << static_cast<uint64_t>(last_clock)
        << " clock_est=("  << sample_time << " " << static_cast<uint64_t>(clock) << " " << freq << ")"
        << " min_half_rtt="  << min_half_rtt
        << " min_rtt_time=" << min_rtt_time
        << " time_avg=" << time_avg << "(" << time_variance << ")"
        << " clock_avg=" << clock_avg << "(" << clock_covariance << ")"
        << " pred_variance=" << prediction_variance;

    return oss.str();
}

std::string ClockSync::stats(double eventtime)
{
    double sample_time = std::get<0>(clock_est);
    double clock = std::get<1>(clock_est);
    double freq = std::get<2>(clock_est);
    return "freq=" + std::to_string(freq);
}

std::pair<double, double> ClockSync::calibrate_clock(double print_time, double eventtime)
{
    return std::make_pair(0.0, mcu_freq);
}

std::tuple<double, double, double> ClockSync::get_clock_est()
{
    return clock_est;
}

double ClockSync::get_mcu_freq()
{
    return mcu_freq;
}

std::mutex& ClockSync::get_mutex() {
    return mutex;
}

double ClockSync::get_clock_event(double eventtime)
{
    serial->raw_send(get_clock_cmd, 0, 0, cmd_queue);
    queries_pending++;
    if(queries_pending > 2)
    {
        SPDLOG_WARN("queries_pending {}", queries_pending);
    }
    return eventtime + 0.9839;
}

void ClockSync::handle_clock(json params)
{
    queries_pending = 0;
    uint64_t last_clock = this->last_clock;
    uint64_t clock_delta = (std::stoul(params["clock"].get<std::string>()) - last_clock) & 0xFFFFFFFF;
    uint64_t clock;
    this->last_clock = clock = last_clock + clock_delta;

    double sent_time = params["#sent_time"].get<double>();
    if (sent_time == 0.0)
        return;
    double receive_time = params["#receive_time"].get<double>();
    double half_rtt = 0.5 * (receive_time - sent_time);
    double aged_rtt = (sent_time - min_rtt_time) * RTT_AGE;

    if (half_rtt < min_half_rtt + aged_rtt)
    {
        min_half_rtt = half_rtt;
        min_rtt_time = sent_time;
        SPDLOG_INFO("{}New minimum RTT: sent_time={},half_rtt={},freq={}", serial->get_warn_prefix(),
                    sent_time, half_rtt, std::get<2>(clock_est));
    }

    // 过滤极端异常的样本
    double exp_clock = ((sent_time - time_avg) * std::get<2>(clock_est) + clock_avg);
    double clock_diff2 = std::pow(clock - exp_clock, 2);

    if (clock_diff2 > 25.0 * prediction_variance &&
        clock_diff2 > std::pow(0.000500 * mcu_freq, 2))
    {
        if (clock > exp_clock && sent_time < last_prediction_time + 10.0)
        {
            SPDLOG_INFO("{}Ignoring clock sample: sent_time={},time_avg={},freq={},diff={}({}),stddev={}", serial->get_warn_prefix(),
                        sent_time, time_avg, std::get<2>(clock_est), clock - exp_clock, (clock - exp_clock) / mcu_freq, std::sqrt(prediction_variance));
            return;
        }
        SPDLOG_INFO("{}Resetting prediction variance: sent_time={},time_avg={},freq={},diff={}({}),stddev={}", serial->get_warn_prefix(),
                    sent_time, time_avg, std::get<2>(clock_est), clock - exp_clock, (clock - exp_clock) / mcu_freq, std::sqrt(prediction_variance));
        prediction_variance = std::pow(0.001 * mcu_freq, 2);
    }
    else
    {
        last_prediction_time = sent_time;
        prediction_variance = (1.0 - DECAY) * (prediction_variance + clock_diff2 * DECAY);
    }

    double diff_sent_time = sent_time - time_avg;
    time_avg += DECAY * diff_sent_time;
    time_variance = (1.0 - DECAY) * (time_variance + diff_sent_time * diff_sent_time * DECAY);
    double diff_clock = clock - clock_avg;
    clock_avg += DECAY * diff_clock;
    clock_covariance = (1.0 - DECAY) * (clock_covariance + diff_sent_time * diff_clock * DECAY);
    // 从线性回归更新预测
    double new_freq = clock_covariance / time_variance;
    double pred_stddev = std::sqrt(prediction_variance);
    // 更新串口的时钟估计
    serial->set_clock_est(new_freq, time_avg + TRANSMIT_EXTRA,
                          static_cast<uint64_t>(this->clock_avg - 3.0 * pred_stddev), clock);
    
    std::unique_lock<std::mutex> lock(mutex);
    this->clock_est = std::make_tuple(time_avg + min_half_rtt, clock_avg, new_freq);
    // SPDLOG_INFO("clock_est name = {} clock_est_0 = {} clock_est_1 = {} clock_est_2 = {} clock_delta = {} clock_str = {} clock_str_uint = {} sent_time = {} receive_time = {} prediction_variance = {} min_rtt_time={} last_prediction_time = {} diff_sent_time={} clock_covariance={} time_variance={} clock={}",
    //     name, std::get<0>(clock_est),std::get<1>(clock_est), std::get<2>(clock_est),clock_delta,params["clock"].get<std::string>(),std::stoul(params["clock"].get<std::string>()), sent_time,receive_time,prediction_variance,min_rtt_time,last_prediction_time,diff_sent_time,clock_covariance,time_variance,clock);
}

SecondarySync::SecondarySync(std::shared_ptr<SelectReactor> reactor,
                             std::shared_ptr<ClockSync> main_sync, const std::string& name)
    : ClockSync(reactor, name), main_sync(main_sync),
      clock_adj(std::make_pair(0.0, 1.0)),
      last_sync_time(0.0)
{
    std::cout << "create SecondarySync success!" << std::endl;
}

SecondarySync::~SecondarySync()
{
}

void SecondarySync::connect(std::shared_ptr<SerialReader> serial)
{
    ClockSync::connect(serial);

    clock_adj = std::make_pair(0.0, mcu_freq);

    double curtime = get_monotonic();
    double main_print_time = main_sync->estimated_print_time(curtime);
    double local_print_time = estimated_print_time(curtime);

    clock_adj = std::make_pair(main_print_time - local_print_time, mcu_freq);
    calibrate_clock(0.0, curtime);
}

void SecondarySync::connect_file(std::shared_ptr<SerialReader> serial, bool pace)
{
    ClockSync::connect_file(serial, pace);
    clock_adj = std::make_pair(0.0, mcu_freq);
}

uint64_t SecondarySync::print_time_to_clock(double print_time)
{
    double adjusted_offset = clock_adj.first;
    double adjusted_freq = clock_adj.second;

    return static_cast<uint64_t>((print_time - adjusted_offset) * adjusted_freq);
}

double SecondarySync::clock_to_print_time(double clock)
{
    double adjusted_offset = clock_adj.first;
    double adjusted_freq = clock_adj.second;

    return clock / adjusted_freq + adjusted_offset;
}

std::string SecondarySync::dump_debug()
{
    std::stringstream ss;
    ss << ClockSync::dump_debug() << " clock_adj=("
       << std::fixed << std::setprecision(3) << clock_adj.first << " "
       << std::fixed << std::setprecision(3) << clock_adj.second << ")";
    return ss.str();
}

std::string SecondarySync::stats(double eventtime)
{
    std::stringstream ss;
    ss << ClockSync::stats(eventtime) << " adj=" << static_cast<int>(clock_adj.second);
    return ss.str();
}

std::pair<double, double> SecondarySync::calibrate_clock(double print_time, double eventtime)
{
    std::unique_lock<std::mutex> lock(main_sync->get_mutex());
    double ser_time = std::get<0>(main_sync->get_clock_est());
    double ser_clock = std::get<1>(main_sync->get_clock_est());
    double ser_freq = std::get<2>(main_sync->get_clock_est());
    lock.unlock();

    double main_mcu_freq = main_sync->get_mcu_freq(); // mcu_freq

    double est_main_clock = (eventtime - ser_time) * ser_freq + ser_clock;
    double est_print_time = est_main_clock / main_mcu_freq;

    double sync1_print_time = std::max(print_time, est_print_time);
    double sync2_print_time = std::max(sync1_print_time + 4.0,
                                       std::max(last_sync_time, print_time + 2.5 * (print_time - est_print_time)));
    double sync2_main_clock = sync2_print_time * main_mcu_freq;
    double sync2_sys_time = ser_time + (sync2_main_clock - ser_clock) / ser_freq;

    // TODO:int64_t ?
    uint64_t sync1_clock = print_time_to_clock(sync1_print_time);
    uint64_t sync2_clock = get_clock(sync2_sys_time);
    double adjusted_freq = (sync2_clock - sync1_clock) / (sync2_print_time - sync1_print_time);
    double adjusted_offset = sync1_print_time - sync1_clock / adjusted_freq;

    clock_adj = std::make_pair(adjusted_offset,adjusted_freq);
    // SPDLOG_INFO("calibrate_clock name = {} clock_adj.first = {} clock_adj.second = {} sync2_clock = {} sync1_clock = {} sync2_print_time= {} sync1_print_time={} sync2_sys_time={} ser_time={} ser_clock={} ser_freq={} main_mcu_freq={} print_time={} eventtime={} last_sync_time={}",
    //     name, clock_adj.first,clock_adj.second,sync2_clock, sync1_clock, sync2_print_time,sync1_print_time,sync2_sys_time,ser_time,ser_clock,ser_freq,main_mcu_freq,print_time,eventtime,last_sync_time);
    last_sync_time = sync2_print_time;
    return clock_adj;
}