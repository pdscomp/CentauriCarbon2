/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:53:47
 * @LastEditors  : huangpeiyun
 * @LastEditTime : 2025-09-17 15:17:11
 * @Description  : Interface to Elegoo micro-controller code
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "mcu.h"
#include "serialhdl.h"
#include "reactor.h"
#include "clocksync.h"
#include "configfile.h"
#include "printer.h"
#include "extras/canbus_ids.h"
#include "pins.h"
#include "c_helper.h"
#include "extras/error_mcu.h"
#include "stepper.h"
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <unistd.h>
#include "zlib.h"
#include "serial_bootloader.h"
#include "dsp_helper.h"
RetryAsyncCommand::RetryAsyncCommand(std::shared_ptr<SerialReader> serial,
                                     const std::string &name, uint32_t oid) : serial(serial), oid(oid),
                                                                              name(name), need_response(true)
{
    reactor = serial->get_reactor();
    completion = reactor->completion();
    min_query_time = get_monotonic();
    serial->register_response(
        [this](const json &params)
        {
            handle_callback(params);
        },
        name,
        oid);
}

RetryAsyncCommand::~RetryAsyncCommand()
{
    // SPDLOG_DEBUG("~RetryAsyncCommand()");
}

void RetryAsyncCommand::handle_callback(json params)
{
    if (need_response && params["#sent_time"].get<double>() >= min_query_time)
    {
        need_response = false;
        reactor->async_complete(completion, params);
    }
}

json RetryAsyncCommand::get_response(
    const std::vector<uint8_t> &cmds,
    std::shared_ptr<command_queue> cmd_queue,
    uint64_t minclock,
    uint64_t reqclock)
{
    serial->raw_send_wait_ack(cmds, minclock, reqclock, cmd_queue);
    min_query_time = 0.0;
    double first_query_time = get_monotonic();
    double query_time = first_query_time;

    while (true)
    {
        json params = completion->wait(query_time + 0.500);
        if (!params.empty())
        {
            serial->register_response(nullptr, name, oid);
            return params;
        }

        query_time = get_monotonic();
        if (query_time > first_query_time + 5.0)
        {
            serial->register_response(nullptr, name, oid);
            SPDLOG_DEBUG("Timeout on wait for {} response", this->name);
            throw std::runtime_error("Timeout on wait for '" + this->name + "' response");
        }
        serial->raw_send(cmds, minclock, minclock, cmd_queue);
    }
}

CommandQueryWrapper::CommandQueryWrapper(std::shared_ptr<SerialReader> serial,
                                         const std::string &msgformat,
                                         const std::string &respformat,
                                         uint32_t oid, std::shared_ptr<command_queue> cmd_queue, bool is_async)
    : serial(serial), cmd_queue(cmd_queue), is_async(is_async), oid(oid)
{
    SPDLOG_DEBUG("CommandQueryWrapper init!");
    cmd = serial->get_msgparser()->lookup_command(msgformat);
    serial->get_msgparser()->lookup_command(respformat);
    std::istringstream iss(respformat);
    iss >> response;
    if (cmd_queue == nullptr)
    {
        this->cmd_queue = serial->get_default_command_queue();
    }
    SPDLOG_DEBUG("CommandQueryWrapper init success!!");
}

CommandQueryWrapper::~CommandQueryWrapper()
{
}

json CommandQueryWrapper::send(const std::vector<Any> &data, uint64_t minclock, uint64_t reqclock)
{
    // SPDLOG_DEBUG("__func__:{} #1",__func__);
    return do_send(cmd->encode(data), minclock, reqclock);
    // SPDLOG_DEBUG("__func__:{} #1",__func__);
}

json CommandQueryWrapper::send_with_preface(
    std::shared_ptr<CommandWrapper> preface_cmd,
    const std::vector<Any> preface_data,
    const std::vector<Any> data,
    uint64_t minclock, uint64_t reqclock)
{
    std::vector<uint8_t> cmds = preface_cmd->cmd->encode(preface_data);
    std::vector<uint8_t> data_cmds = cmd->encode(data);
    cmds.insert(cmds.end(), data_cmds.begin(), data_cmds.end());
    return do_send(cmds, minclock, reqclock);
}

json CommandQueryWrapper::do_send(const std::vector<uint8_t> &cmds, uint64_t minclock, uint64_t reqclock)
{
    reqclock = std::max(minclock, reqclock);
    if (is_async)
    {
        std::shared_ptr<RetryAsyncCommand> xmit_async_helper = std::make_shared<RetryAsyncCommand>(serial, response, oid);
        try
        {
            // std::vector<std::vector<uint8_t>> cmdsResp = {};
            // cmdsResp.push_back(cmds);
            return xmit_async_helper->get_response(cmds, cmd_queue, minclock, reqclock);
        }
        catch (const std::exception &e)
        {
            // throw MCUError(e.what());
        }
    }
    else
    {
        std::shared_ptr<SerialRetryCommand> xmit_retry_helper = std::make_shared<SerialRetryCommand>(serial, response, oid);
        try
        {
            std::vector<std::vector<uint8_t>> cmdsResp = {};
            cmdsResp.push_back(cmds);
            return xmit_retry_helper->get_response(cmdsResp, cmd_queue, minclock, reqclock);
        }
        catch (const std::exception &e)
        {
            // throw MCUError(e.what());
        }
    }

    return {};
}

CommandWrapper::CommandWrapper(std::shared_ptr<SerialReader> serial,
                               const std::string &msgformat,
                               std::shared_ptr<command_queue> cmd_queue)
    : serial(serial), cmd_queue(cmd_queue)
{
    std::shared_ptr<MessageParser> msgparser = serial->get_msgparser();
    cmd = msgparser->lookup_command(msgformat);
    if (cmd_queue == nullptr)
    {
        this->cmd_queue = serial->get_default_command_queue();
    }
    msgtag = static_cast<uint32_t>(msgparser->lookup_msgid(msgformat)) & 0xFFFFFFFF;
}

CommandWrapper::~CommandWrapper()
{
}

void CommandWrapper::send(const std::vector<Any> &data, uint32_t minclock, uint32_t reqclock)
{
    std::vector<uint8_t> result = cmd->encode(data);
    serial->raw_send(result, minclock, reqclock, cmd_queue);
}

void CommandWrapper::send_wait_ack(const std::vector<Any> &data, uint32_t minclock, uint32_t reqclock)
{
    SPDLOG_DEBUG("__func__:{} #1", __func__);
    std::vector<uint8_t> result = cmd->encode(data);
    serial->raw_send_wait_ack(result, minclock, reqclock, cmd_queue);
}

uint32_t CommandWrapper::get_command_tag()
{
    return msgtag;
}

MCU_trsync::MCU_trsync(std::shared_ptr<MCU> mcu, std::shared_ptr<trdispatch> td)
    : mcu(mcu), td(td)
{
    reactor = mcu->get_printer()->get_reactor();
    oid = mcu->create_oid();
    cmd_queue = mcu->alloc_command_queue();
    mcu->register_config_callback([this]()
                                  { build_config(); });

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:shutdown",
        std::function<void()>([this]()
                              {
            SPDLOG_DEBUG("elegoo:shutdown !");
            shutdown();
            SPDLOG_DEBUG("elegoo:shutdown !"); }));
}

MCU_trsync::~MCU_trsync()
{
}

std::shared_ptr<MCU> MCU_trsync::get_mcu()
{
    return mcu;
}

uint32_t MCU_trsync::get_oid()
{
    return oid;
}

std::shared_ptr<command_queue> MCU_trsync::get_command_queue()
{
    return cmd_queue;
}

void MCU_trsync::add_stepper(std::shared_ptr<MCU_stepper> stepper)
{
    if (std::find(steppers.begin(), steppers.end(), stepper) != steppers.end())
    {
        return;
    }
    steppers.push_back(stepper);
}

std::vector<std::shared_ptr<MCU_stepper>> MCU_trsync::get_steppers()
{
    return steppers;
}

void MCU_trsync::start(double print_time,
                       double report_offset,
                       std::shared_ptr<ReactorCompletion> trigger_completion,
                       double expire_timeout)
{
    // start_time = get_monotonic();
    this->trigger_completion = trigger_completion;
    home_end_clock = 0;
    uint64_t clock = mcu->print_time_to_clock(print_time);
    uint64_t expire_ticks = mcu->seconds_to_clock(expire_timeout);
    uint64_t expire_clock = clock + expire_ticks;

    uint64_t report_ticks = mcu->seconds_to_clock(expire_timeout * 0.3);
    uint64_t report_clock = clock + static_cast<uint64_t>(report_ticks * report_offset + 0.5);
    uint64_t min_extend_ticks = static_cast<uint64_t>(report_ticks * 0.8 + 0.5);

    trdispatch_mcu_setup(td_mcu.get(), clock, expire_clock, expire_ticks, min_extend_ticks);
    mcu->register_response(
        [this](const json &params)
        {
            handle_trsync_state(params);
        },
        "trsync_state", oid);
    std::vector<Any> data;
    data.push_back(std::to_string(oid));
    data.push_back(std::to_string(report_clock));
    data.push_back(std::to_string(report_ticks));
    data.push_back(std::to_string(REASON_COMMS_TIMEOUT));

    SPDLOG_DEBUG("trsync_start_cmd on_restart=true");
    trsync_start_cmd->send(data, 0, report_clock);

    for (auto stepper : this->steppers)
    {
        SPDLOG_DEBUG("stepper->get_name:{}", stepper->get_name());
        this->stepper_stop_cmd->send({std::to_string(stepper->get_oid()),
                                      std::to_string(this->get_oid())});
    }
    this->trsync_set_timeout_cmd->send({std::to_string(this->get_oid()), std::to_string(expire_clock)}, 0, expire_clock);
}

void MCU_trsync::set_home_end_time(double home_end_time)
{
    home_end_clock = mcu->print_time_to_clock(home_end_time);
    SPDLOG_DEBUG("MCU_trsync::set_home_end_time:mcu name {} home_end_time {} clock {}",
                 mcu->get_name(), home_end_time, home_end_clock);
}

int MCU_trsync::stop()
{
    mcu->register_response(
        std::function<void(const json &params)>(),
        "trsync_state", oid);

    trigger_completion = nullptr;
    if (mcu->is_fileoutput())
    {
        return REASON_ENDSTOP_HIT;
    }
    std::vector<Any> t_data;
    t_data.push_back(std::to_string(oid));
    t_data.push_back(std::to_string(REASON_HOST_REQUEST));
    json params = trsync_query_cmd->send(t_data);

    for (auto stepper : this->steppers)
    {
        SPDLOG_DEBUG("stepper->get_name:{}", stepper->get_name());
        stepper->note_homing_end();
    }
    return std::stoi(params["trigger_reason"].get<std::string>());
}

void MCU_trsync::build_config()
{
    mcu->add_config_cmd("config_trsync oid=" + std::to_string(oid));
    mcu->add_config_cmd(
        "trsync_start oid=" + std::to_string(oid) +
            " report_clock=0 report_ticks=0 expire_reason=0",
        false, true);

    trsync_start_cmd = mcu->lookup_command(
        "trsync_start oid=%c report_clock=%u report_ticks=%u expire_reason=%c",
        cmd_queue);
    trsync_set_timeout_cmd = mcu->lookup_command(
        "trsync_set_timeout oid=%c clock=%u", cmd_queue);
    trsync_trigger_cmd = mcu->lookup_command(
        "trsync_trigger oid=%c reason=%c", cmd_queue);
    trsync_query_cmd = mcu->lookup_query_command(
        "trsync_trigger oid=%c reason=%c",
        "trsync_state oid=%c can_trigger=%c trigger_reason=%c clock=%u",
        oid, cmd_queue);
    stepper_stop_cmd = mcu->lookup_command(
        "stepper_stop_on_trigger oid=%c trsync_oid=%c", cmd_queue);

    // Create trdispatch_mcu object
    uint32_t set_timeout_tag = mcu->lookup_command(
                                      "trsync_set_timeout oid=%c clock=%u")
                                   ->get_command_tag();

    std::shared_ptr<CommandWrapper> trigger_cmd = mcu->lookup_command(
        "trsync_trigger oid=%c reason=%c");
    uint32_t trigger_tag = trigger_cmd->get_command_tag();

    std::shared_ptr<CommandWrapper> state_cmd = mcu->lookup_command(
        "trsync_state oid=%c can_trigger=%c trigger_reason=%c clock=%u");
    uint32_t state_tag = state_cmd->get_command_tag();

    td_mcu = std::shared_ptr<trdispatch_mcu>(
        trdispatch_mcu_alloc(td.get(),
                             mcu->serial->get_serialqueue().get(), cmd_queue.get(), oid, set_timeout_tag, trigger_tag, state_tag),
        free);
}

void MCU_trsync::shutdown()
{
    std::shared_ptr<ReactorCompletion> tc = trigger_completion;
    if (tc != nullptr)
    {
        trigger_completion = nullptr;
        json value;
        tc->complete(value);
    }
}

void MCU_trsync::handle_trsync_state(const json &params)
{
    // 下位机限位触发后将can_trigger设置为0
    if (!std::stoi(params["can_trigger"].get<std::string>()))
    {
        // 触发
        std::shared_ptr<ReactorCompletion> tc = trigger_completion;
        if (tc != nullptr)
        {
            // SPDLOG_INFO("handle_trsync_state {}", get_monotonic() - start_time);
            trigger_completion = nullptr;
            int reason = std::stoi(params["trigger_reason"].get<std::string>());
            bool is_failure = (reason >= REASON_COMMS_TIMEOUT);
            SPDLOG_INFO("handle_trsync_state #1 reason:{} is_failure:{}", reason, is_failure);
            json value;
            value["is_failure"] = is_failure;
            reactor->async_complete(tc, value);
        }
    }
    else if (home_end_clock)
    {
        uint64_t clock = mcu->clock32_to_clock64(std::stoul(params["clock"].get<std::string>()));
        if (clock >= home_end_clock)
        {
            SPDLOG_DEBUG("handle_trsync_state #2 {} {}", clock, home_end_clock);
            home_end_clock = 0;
            std::vector<Any> data;
            data.push_back(std::to_string(oid));
            data.push_back(std::to_string(REASON_PAST_END_TIME));
            trsync_trigger_cmd->send(data);
        }
    }
}

TriggerDispatch::TriggerDispatch(std::shared_ptr<MCU> mcu)
    : mcu(mcu), td(trdispatch_alloc(), free)
{
    // td = std::shared_ptr<trdispatch>(trdispatch_alloc(), free);
    trsyncs.push_back(std::make_shared<MCU_trsync>(mcu, td));
}

TriggerDispatch::~TriggerDispatch()
{
}

uint32_t TriggerDispatch::get_oid()
{
    return trsyncs[0]->get_oid();
}

std::shared_ptr<command_queue> TriggerDispatch::get_command_queue()
{
    return trsyncs[0]->get_command_queue();
}

void TriggerDispatch::add_stepper(std::shared_ptr<MCU_stepper> stepper)
{
    std::map<std::shared_ptr<MCU>, std::shared_ptr<MCU_trsync>> t_trsyncs;
    for (const std::shared_ptr<MCU_trsync> &trsync : trsyncs)
    {
        t_trsyncs[trsync->get_mcu()] = trsync;
    }

    std::shared_ptr<MCU_trsync> t_trsync;
    auto it = t_trsyncs.find(stepper->get_mcu());
    if (it != t_trsyncs.end())
    {
        t_trsync = it->second;
    }
    else
    {
        t_trsync = std::make_shared<MCU_trsync>(stepper->get_mcu(), td);
        trsyncs.push_back(t_trsync);
    }

    t_trsync->add_stepper(stepper);

    std::string sname = stepper->get_name();
    if (sname.rfind("stepper_", 0) == 0)
    {
        for (const std::shared_ptr<MCU_trsync> &ot : trsyncs)
        {
            for (const std::shared_ptr<MCU_stepper> &s : ot->get_steppers())
            {
                if (ot != t_trsync && s->get_name().rfind(sname.substr(0, 9), 0) == 0)
                {
                    throw std::runtime_error("Multi-mcu homing not supported on multi-mcu shared axis");
                }
            }
        }
    }
}

std::vector<std::shared_ptr<MCU_stepper>> TriggerDispatch::get_steppers()
{
    std::vector<std::shared_ptr<MCU_stepper>> all_steppers;

    for (const std::shared_ptr<MCU_trsync> &trsync : trsyncs)
    {
        const std::vector<std::shared_ptr<MCU_stepper>> &steppers =
            trsync->get_steppers();
        all_steppers.insert(all_steppers.end(), steppers.begin(), steppers.end());
    }

    return all_steppers;
}

std::shared_ptr<ReactorCompletion> TriggerDispatch::start(double print_time)
{
    std::shared_ptr<SelectReactor> reactor = mcu->get_printer()->get_reactor();
    trigger_completion = reactor->completion();
    double expire_timeout = 0.025;
    if (trsyncs.size() == 1)
    {
        expire_timeout = 0.25;
    }

    // 启动同步器
    for (size_t i = 0; i < trsyncs.size(); ++i)
    {
        double report_offset = static_cast<double>(i) / trsyncs.size();
        trsyncs[i]->start(print_time, report_offset, trigger_completion, expire_timeout);
    }

    // 启动快速分发,当收到下位机上报的触发消息会在底层广播到其他MCU
    std::shared_ptr<MCU_trsync> etrsync = trsyncs[0];
    trdispatch_start(td.get(), MCU_trsync::REASON_HOST_REQUEST);

    return trigger_completion;
}

void TriggerDispatch::wait_end(double end_time)
{
    std::shared_ptr<MCU_trsync> etrsync = trsyncs[0];
    // 设置超时时间为归零运动结束时刻
    etrsync->set_home_end_time(end_time);
    if (mcu->is_fileoutput())
    {
        json value;
        trigger_completion->complete(value);
    }
    trigger_completion->wait();
}

int TriggerDispatch::stop()
{
    trdispatch_stop(td.get());

    std::vector<int> res;
    for (const std::shared_ptr<MCU_trsync> &trsync : trsyncs)
    {
        res.push_back(trsync->stop());
    }

    auto it = std::find_if(res.begin(), res.end(), [](int r)
                           { return r >= MCU_trsync::REASON_COMMS_TIMEOUT; });

    if (it != res.end())
    {
        return *it;
    }

    return res.empty() ? 0 : res[0];
}

MCU_endstop::MCU_endstop(std::shared_ptr<MCU> mcu, std::shared_ptr<PinParams> pin_params)
    : mcu(mcu)
{
    pin = *pin_params->pin;
    pullup = pin_params->pullup;
    invert = pin_params->invert;
    oid = mcu->create_oid();
    mcu->register_config_callback([this]()
                                  { build_config(); });
    rest_ticks = 0;
    dispatch = std::make_shared<TriggerDispatch>(mcu);
}

MCU_endstop::~MCU_endstop()
{
}

std::shared_ptr<MCU> MCU_endstop::get_mcu()
{
    return mcu;
}

void MCU_endstop::add_stepper(std::shared_ptr<MCU_stepper> stepper)
{
    dispatch->add_stepper(stepper);
}

std::vector<std::shared_ptr<MCU_stepper>> MCU_endstop::get_steppers()
{
    return dispatch->get_steppers();
}

std::shared_ptr<ReactorCompletion> MCU_endstop::home_start(double print_time,
                                                           double sample_time,
                                                           int sample_count,
                                                           double rest_time,
                                                           bool triggered, double accel_time)
{
    uint64_t clock = mcu->print_time_to_clock(print_time);
    rest_ticks = mcu->print_time_to_clock(print_time + rest_time) - clock;
    // 启动
    std::shared_ptr<ReactorCompletion> trigger_completion = dispatch->start(print_time);
    home_cmd->send({std::to_string(oid),
                    std::to_string(clock),
                    std::to_string(mcu->seconds_to_clock(sample_time)),
                    std::to_string(static_cast<uint64_t>(sample_count)),
                    std::to_string(rest_ticks),
                    std::to_string(static_cast<uint8_t>(triggered ^ invert)),
                    std::to_string(dispatch->get_oid()),
                    std::to_string(MCU_trsync::REASON_ENDSTOP_HIT)},
                   0, clock);

    // std::vector<Any> data;
    // data.push_back(std::to_string(oid));
    // json params = query_cmd->send(data);
    // uint64_t next_clock = mcu->clock32_to_clock64(std::stoul(params["next_clock"].get<std::string>())); // 待确认
    // uint64_t pin_value = std::stoul(params["pin_value"].get<std::string>()); // 待确认
    // SPDLOG_INFO("{} pin_value:{} next_clock:{}",__func__,pin_value,next_clock);

    return trigger_completion;
}

double MCU_endstop::home_wait(double home_end_time)
{
    // 等待限位触发
    dispatch->wait_end(home_end_time);
    // ...
    home_cmd->send({std::to_string(oid), std::to_string(0),
                    std::to_string(0), std::to_string(0),
                    std::to_string(0), std::to_string(0),
                    std::to_string(0), std::to_string(0)});
    int res = dispatch->stop();

    if (res >= MCU_trsync::REASON_COMMS_TIMEOUT)
    {
        throw elegoo::common::CommandError("Communication timeout during homing");
    }
    if (res != MCU_trsync::REASON_ENDSTOP_HIT)
    {
        return 0.0;
    }

    if (mcu->is_fileoutput())
    {
        return home_end_time;
    }

    std::vector<Any> data;
    data.push_back(std::to_string(oid));
    json params = query_cmd->send(data);
    uint64_t next_clock = mcu->clock32_to_clock64(std::stoul(params["next_clock"].get<std::string>())); // 待确认
    uint64_t pin_value = std::stoul(params["pin_value"].get<std::string>());                            // 待确认
    SPDLOG_INFO("{} pin_value:{} next_clock:{}", __func__, pin_value, next_clock);

    return mcu->clock_to_print_time(next_clock - rest_ticks);
}

bool MCU_endstop::query_endstop(double print_time)
{
    uint64_t clock = mcu->print_time_to_clock(print_time);
    if (mcu->is_fileoutput())
        return false;
    std::vector<Any> data;
    data.push_back(std::to_string(oid));
    auto params = query_cmd->send(data, clock);
    return std::stoi(params["pin_value"].get<std::string>()) ^ invert;
}

void MCU_endstop::build_config()
{
    mcu->add_config_cmd("config_endstop oid=" + std::to_string(oid) +
                        " pin=" + pin + " pull_up=" + std::to_string(pullup));

    mcu->add_config_cmd(
        "endstop_home oid=" + std::to_string(oid) +
            " clock=0 sample_ticks=0 sample_count=0 rest_ticks=0 pin_value=0 trsync_oid=0 trigger_reason=0",
        false, true);

    std::shared_ptr<command_queue> cmd_queue = dispatch->get_command_queue();

    home_cmd = mcu->lookup_command(
        "endstop_home oid=%c clock=%u sample_ticks=%u sample_count=%c rest_ticks=%u pin_value=%c trsync_oid=%c trigger_reason=%c",
        cmd_queue);

    query_cmd = mcu->lookup_query_command(
        "endstop_query_state oid=%c",
        "endstop_state oid=%c homing=%c next_clock=%u pin_value=%c",
        oid, cmd_queue);
}

MCU_digital_out::MCU_digital_out(std::shared_ptr<MCU> mcu,
                                 std::shared_ptr<PinParams> pin_params)
    : mcu(mcu)
{
    mcu->register_config_callback(
        [this]()
        {
            build_config();
        });
    pin = *pin_params->pin;
    invert = pin_params->invert;
    start_value = shutdown_value = invert;
    max_duration = 2.;
    last_clock = 0;
}

MCU_digital_out::~MCU_digital_out()
{
}

std::shared_ptr<MCU> MCU_digital_out::get_mcu()
{
    return mcu;
}

void MCU_digital_out::setup_max_duration(double max_duration)
{
    this->max_duration = max_duration;
}

void MCU_digital_out::setup_start_value(double start_value, double shutdown_value)
{
    this->start_value = (!!start_value) ^ invert;
    this->shutdown_value = (!!shutdown_value) ^ invert;
}

void MCU_digital_out::set_digital(double print_time, double value)
{
    uint64_t clock = mcu->print_time_to_clock(print_time);
    bool adjusted_value = (static_cast<bool>(value) ^ invert);
    SPDLOG_INFO("set_digital pin {}  {}  {}", pin, print_time, get_monotonic());
    set_cmd->send({std::to_string(oid), std::to_string(clock), std::to_string(adjusted_value)}, last_clock, clock);

    last_clock = clock;
}

void MCU_digital_out::build_config()
{
    if (max_duration && start_value != shutdown_value)
    {
        throw std::runtime_error("Pin with max duration must have start value equal to shutdown value");
    }

    uint32_t mdur_ticks = mcu->seconds_to_clock(max_duration);
    if (mdur_ticks >= (1U << 31))
    {
        throw std::runtime_error("Digital pin max duration too large");
    }

    mcu->request_move_queue_slot();
    oid = mcu->create_oid();
    SPDLOG_INFO("create_oid={}, pin={} {}", oid, pin, mcu->get_name());
    std::string config_cmd = "config_digital_out oid=" + std::to_string(oid) + " pin=" + pin +
                             " value=" + std::to_string(start_value) +
                             " default_value=" + std::to_string(shutdown_value) +
                             " max_duration=" + std::to_string(mdur_ticks);
    mcu->add_config_cmd(config_cmd);

    std::string update_cmd = "update_digital_out oid=" + std::to_string(oid) +
                             " value=" + std::to_string(start_value);
    mcu->add_config_cmd(update_cmd, false, true); // 设置 on_restart = true
    std::shared_ptr<command_queue> cmd_queue = mcu->alloc_command_queue();
    set_cmd = mcu->lookup_command("queue_digital_out oid=%c clock=%u on_ticks=%u", cmd_queue);
}

MCU_pwm::MCU_pwm(std::shared_ptr<MCU> mcu,
                 std::shared_ptr<PinParams> pin_params)
    : mcu(mcu), hardware_pwm(false)
{
    cycle_time = 0.100;
    max_duration = 2.;
    mcu->register_config_callback(
        [this]()
        {
            build_config();
        });
    pin = *pin_params->pin;
    invert = pin_params->invert;
    start_value = shutdown_value = invert;
    last_clock = 0;
    pwm_max = 0.;
}

MCU_pwm::~MCU_pwm()
{
}

std::shared_ptr<MCU> MCU_pwm::get_mcu()
{
    return mcu;
}

void MCU_pwm::setup_max_duration(double max_duration)
{
    this->max_duration = max_duration;
}

void MCU_pwm::setup_cycle_time(double cycle_time, bool hardware_pwm)
{
    this->cycle_time = cycle_time;
    this->hardware_pwm = hardware_pwm;
}

void MCU_pwm::setup_start_value(double start_value, double shutdown_value)
{
    if (invert)
    {
        start_value = 1.0 - start_value;
        shutdown_value = 1.0 - shutdown_value;
    }

    this->start_value = std::max(0.0, std::min(1.0, start_value));
    this->shutdown_value = std::max(0.0, std::min(1.0, shutdown_value));
}

void MCU_pwm::set_pwm(double print_time, double value)
{
    if (invert)
    {
        value = 1.0 - value;
    }

    uint64_t v = static_cast<uint64_t>(std::max(0.0, std::min(1.0, value)) * pwm_max + 0.5);
    uint64_t clock = mcu->print_time_to_clock(print_time);

    // SPDLOG_INFO("set_pwm pin: {}", pin);
    set_cmd->send({std::to_string(oid), std::to_string(clock), std::to_string(v)}, last_clock, clock);
    last_clock = clock;
}

void MCU_pwm::build_config()
{
    if (max_duration > 0 && start_value != shutdown_value)
    {
        throw std::runtime_error("Pin with max duration must have start value equal to shutdown value");
    }

    std::shared_ptr<command_queue> cmd_queue = mcu->alloc_command_queue();
    double curtime = get_monotonic();
    double printtime = mcu->estimated_print_time(curtime);

    last_clock = mcu->print_time_to_clock(printtime + 0.200);
    uint32_t cycle_ticks = mcu->seconds_to_clock(cycle_time);
    uint32_t mdur_ticks = mcu->seconds_to_clock(max_duration);

    if (mdur_ticks >= (1u << 31))
    {
        throw std::runtime_error("PWM pin max duration too large");
    }

    if (hardware_pwm)
    {
        pwm_max = 255; // mcu->get_constant_float("PWM_MAX");
        mcu->request_move_queue_slot();
        oid = mcu->create_oid();

        // 添加配置命令
        mcu->add_config_cmd(
            "config_pwm_out oid=" + std::to_string(oid) + " pin=" + pin +
            " cycle_ticks=" + std::to_string(cycle_ticks) +
            " value=" + std::to_string(int(start_value * pwm_max)) +
            " default_value=" + std::to_string(int(shutdown_value * pwm_max)) +
            " max_duration=" + std::to_string(mdur_ticks));

        int svalue = int(start_value * pwm_max + 0.5);

        mcu->add_config_cmd(
            "queue_pwm_out oid=" + std::to_string(oid) +
                " clock=" + std::to_string(last_clock) +
                " value=" + std::to_string(svalue),
            false, true);

        SPDLOG_DEBUG("set_cmd on_restart=true");
        set_cmd = mcu->lookup_command(
            "queue_pwm_out oid=%c clock=%u value=%hu", cmd_queue);

        return;
    }

    if (shutdown_value != 0.0 && shutdown_value != 1.0)
    {
        throw std::runtime_error("shutdown value must be 0.0 or 1.0 on soft pwm");
    }

    if (cycle_ticks >= (1u << 31))
    {
        throw std::runtime_error("PWM pin cycle time too large");
    }

    mcu->request_move_queue_slot();
    oid = mcu->create_oid();
    mcu->add_config_cmd(
        "config_digital_out oid=" + std::to_string(oid) +
        " pin=" + pin +
        " value=" + std::to_string(start_value >= 1.0) +
        " default_value=" + std::to_string(shutdown_value >= 0.5) +
        " max_duration=" + std::to_string(mdur_ticks));

    mcu->add_config_cmd(
        "set_digital_out_pwm_cycle oid=" + std::to_string(oid) +
        " cycle_ticks=" + std::to_string(cycle_ticks));

    pwm_max = static_cast<float>(cycle_ticks);

    int svalue = static_cast<int>(start_value * cycle_ticks + 0.5);
    mcu->add_config_cmd(
        "queue_digital_out oid=" + std::to_string(oid) +
            " clock=" + std::to_string(last_clock) +
            " on_ticks=" + std::to_string(svalue),
        true);

    SPDLOG_INFO("create oid = {} {}", oid, pin);
    set_cmd = mcu->lookup_command(
        "queue_digital_out oid=%c clock=%u on_ticks=%u", cmd_queue);
}

MCU_adc::MCU_adc(std::shared_ptr<MCU> mcu,
                 std::shared_ptr<PinParams> pin_params)
    : mcu(mcu)
{
    pin = *pin_params->pin;
    min_sample = 0;
    max_sample = 0;
    sample_time = 0;
    report_time = 0;
    sample_count = 0;
    range_check_count = 0;
    report_clock = 0;
    last_state = std::make_pair(0.0, 0.0);
    mcu->register_config_callback(
        [this]()
        {
            build_config();
        });
    inv_max_adc = 0;
}

MCU_adc::~MCU_adc()
{
}

std::shared_ptr<MCU> MCU_adc::get_mcu()
{
    return mcu;
}

void MCU_adc::setup_adc_sample(double sample_time, int sample_count,
                               double minval, double maxval, int range_check_count)
{
    this->sample_time = sample_time;
    this->sample_count = sample_count;
    this->min_sample = minval;
    this->max_sample = maxval;
    this->range_check_count = range_check_count;
}

void MCU_adc::setup_adc_callback(
    double report_time,
    std::function<void(double, double)> callback)
{
    this->report_time = report_time;
    this->callback = callback;
}

std::pair<double, double> MCU_adc::get_last_value()
{
    return last_state;
}

void MCU_adc::build_config()
{
    if (sample_count == 0)
    {
        return;
    }

    oid = mcu->create_oid();
    mcu->add_config_cmd("config_analog_in oid=" + std::to_string(oid) + " pin=" + pin);

    int clock = mcu->get_query_slot(oid);
    int sample_ticks = mcu->seconds_to_clock(sample_time);
    float mcu_adc_max = mcu->get_constant_float("ADC_MAX"); // 4095
    float max_adc = sample_count * mcu_adc_max;
    inv_max_adc = 1.0f / max_adc;
    report_clock = mcu->seconds_to_clock(report_time);

    int min_sample = std::max(0, std::min(0xffff, static_cast<int>(this->min_sample * max_adc)));
    int max_sample = std::max(0, std::min(0xffff, static_cast<int>(std::ceil(this->max_sample * max_adc))));

    mcu->add_config_cmd("query_analog_in oid=" + std::to_string(oid) +
                            " clock=" + std::to_string(clock) +
                            " sample_ticks=" + std::to_string(sample_ticks) +
                            " sample_count=" + std::to_string(sample_count) +
                            " rest_ticks=" + std::to_string(report_clock) +
                            " min_value=" + std::to_string(min_sample) +
                            " max_value=" + std::to_string(max_sample) +
                            " range_check_count=" + std::to_string(range_check_count),
                        true);

    mcu->register_response(
        [this](const json &params)
        {
            handle_analog_in_state(params);
        },
        "analog_in_state", oid);
}

void MCU_adc::handle_analog_in_state(const json &params)
{
    double params_value = std::stoul(params["value"].get<std::string>());
    double last_value = params_value * inv_max_adc;
    uint64_t next_clock = mcu->clock32_to_clock64(std::stoul(params["next_clock"].get<std::string>()));
    uint64_t last_read_clock = next_clock - report_clock;
    double last_read_time = mcu->clock_to_print_time(last_read_clock);
    last_state = std::make_pair(last_value, last_read_time);

    // SPDLOG_INFO("__func__:{},params['value'].get<std::string>():{},inv_max_adc:{},last_value:{},next_clock:{},last_read_time:{},report_clock:{}",__func__,params["value"].get<std::string>(),inv_max_adc,last_value,next_clock,last_read_time,report_clock);
    // 如果回调函数不为空，调用回调函数
    if (callback != nullptr)
    {
        callback(last_read_time, last_value);
    }
}

MCU::MCU(std::shared_ptr<ConfigWrapper> config,
         std::shared_ptr<ClockSync> clocksync) : clocksync(clocksync)
{
    printer = config->get_printer();
    reactor = printer->get_reactor();
    name = config->get_name();
    if (name.rfind("mcu ", 0) == 0)
    {
        name = name.substr(4);
    }
    std::string wp = "mcu '" + name + "': ";
    serial = std::make_shared<SerialReader>(reactor, wp, name, config->get("serial", ""));
    baud = 0;
    // 只支持串口
#if 0
    std::string canbus_uuid = config->get("canbus_uuid", "");
    if (canbus_uuid != "")
    {
        serialport = canbus_uuid;
        canbus_iface = config->get("canbus_interface", "can0");
        std::shared_ptr<PrinterCANBus> cbid =
            any_cast<std::shared_ptr<PrinterCANBus>>(printer->load_object(config, "canbus_ids"));
        cbid->add_uuid(config, canbus_uuid, canbus_iface);
    }
    else
#endif
    {
        serialport = config->get("serial");
        is_rpmsg = serialport.rfind("/dev/rpmsg_", 0) == 0;
        if (!(serialport.rfind("/dev/rpmsg_", 0) == 0 ||
              serialport.rfind("/tmp/elegoo_host_", 0) == 0))
        {
            baud = config->getint("baud", 250000, 2400);
        }
    }

    restart_method = config->get("restart_method", "command");

    // 对于有配置电源控制脚，尝试获取电源控制脚并上电
    std::string power_pin = config->get("power_pin", "");
    if (!power_pin.empty())
    {
        std::shared_ptr<PrinterPins> ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
        std::shared_ptr<PinParams> power_pin_params = ppins->lookup_pin(config->get("power_pin"), true, false);
        std::shared_ptr<MCU_host_digital_pin> power_pin = std::static_pointer_cast<MCU_host_digital_pin>(power_pin_params->chip->setup_pin("host_digital_pin", power_pin_params));
        // 确保MCU上电
        SPDLOG_INFO("MCU '{}' restart power", name, config_cmds.size());
        power_pin->set_direction(0);
        power_pin->set_digital(power_pin_params->invert);
        reactor->pause(get_monotonic() + 0.2);
        power_pin->set_digital(!power_pin_params->invert);
        // reactor->pause(get_monotonic() + 1.0);
    }
    else if (is_rpmsg)
    {
        restart_via_remoterpc();
    }

    // 串口模式下，尝试让设备退出bootloader
    if (baud > 0)
    {
        // 开机后会进入bootloader停留50ms,这里目的是让其快速进入APP
        int test = 5;
        SerialBootloader sb(serialport, 250000);
        sb.connect();
        do
        {
            if (sb.ping())
            {
                SPDLOG_INFO("bootloader mode, try jump to app!");
                sb.jump_to_app();
                break;
            }
        } while (test-- > 0);
        sb.disconnect();
    }

    is_mcu_bridge = false;
    is_shutdown = false;
    is_timeout = false;
    shutdown_clock = 0;
    oid_count = 0;
    mcu_freq = 0;
    max_stepper_error = config->getdouble("max_stepper_error", 0.000025, 0.);
    reserved_move_slots = 0;
    stats_sumsq_base = 0.;
    mcu_tick_avg = 0.;
    mcu_tick_stddev = 0.;
    mcu_tick_awake = 0.;
    get_status_info = json::object();

    // 注册事件
    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:firmware_restart",
        std::function<void()>([this]()
                              {
            SPDLOG_DEBUG("MCU firmware_restart {}", this->name);
            firmware_restart();
            SPDLOG_DEBUG("MCU firmware_restart {} success", this->name); }));

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:mcu_identify",
        std::function<void()>([this]()
                              {
            SPDLOG_DEBUG("MCU mcu_identify {}", this->name);
            mcu_identify();
            SPDLOG_DEBUG("MCU mcu_identify {} success", this->name); }));

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:connect",
        std::function<void()>([this]()
                              {
            SPDLOG_DEBUG("MCU connect {}", this->name);
            connect();
            SPDLOG_DEBUG("MCU connect {} success", this->name); }));

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:shutdown",
        std::function<void()>([this]()
                              {
            SPDLOG_DEBUG("elegoo:shutdown !");
            shutdown();
            SPDLOG_DEBUG("elegoo:shutdown !"); }));

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:disconnect",
        std::function<void()>([this]()
                              { disconnect(); }));

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:ready",
        std::function<void()>([this]()
                              {
            SPDLOG_DEBUG("elegoo:ready !");
            ready();
            SPDLOG_DEBUG("elegoo:ready !"); }));
    SPDLOG_DEBUG("create MCU success!");
}

MCU::~MCU()
{
    delete[] sc_list;
}

std::shared_ptr<MCU_pins> MCU::setup_pin(const std::string &pin_type, std::shared_ptr<PinParams> pin_params)
{
    std::map<std::string, int> type = {
        {"endstop", 1},
        {"digital_out", 2},
        {"pwm", 3},
        {"adc", 4},
    };

    switch (type.at(pin_type))
    {
    case 1:
        return std::dynamic_pointer_cast<MCU_pins>(std::make_shared<MCU_endstop>(shared_from_this(), pin_params));
    case 2:
        return std::dynamic_pointer_cast<MCU_pins>(std::make_shared<MCU_digital_out>(shared_from_this(), pin_params));
    case 3:
        return std::dynamic_pointer_cast<MCU_pins>(std::make_shared<MCU_pwm>(shared_from_this(), pin_params));
    case 4:
        return std::dynamic_pointer_cast<MCU_pins>(std::make_shared<MCU_adc>(shared_from_this(), pin_params));
    default:
        throw std::runtime_error("Unsupported pin type: " + pin_type);
    }
}

uint32_t MCU::oid_count = 0;
uint32_t MCU::create_oid()
{
    oid_count++;
    return oid_count - 1;
}

void MCU::register_config_callback(std::function<void()> cb)
{
    config_callbacks.push_back(cb);
}

void MCU::add_config_cmd(const std::string &cmd, bool is_init, bool on_restart)
{
    if (is_init)
    {
        SPDLOG_DEBUG("init_cmds.push_back cmd:{}", cmd);
        init_cmds.push_back(cmd);
    }
    else if (on_restart)
    {
        SPDLOG_DEBUG("restart_cmds.push_back cmd:{}", cmd);
        restart_cmds.push_back(cmd);
    }
    else
    {
        config_cmds.push_back(cmd);
    }
}

int MCU::get_query_slot(uint32_t oid)
{
    uint64_t slot = seconds_to_clock(oid * 0.01);
    int t = static_cast<int>(estimated_print_time(get_monotonic()) + 1.5);
    return print_time_to_clock(t) + slot;
}

uint64_t MCU::seconds_to_clock(double time)
{
    return static_cast<uint64_t>(time * mcu_freq);
}

float MCU::get_max_stepper_error()
{
    return max_stepper_error;
}

std::shared_ptr<Printer> MCU::get_printer()
{
    return printer;
}

std::string MCU::get_name()
{
    return name;
}

void MCU::register_response(std::function<void(const json &params)> cb, const std::string &msg, uint32_t oid)
{
    serial->register_response(cb, msg, oid);
}

std::shared_ptr<command_queue> MCU::alloc_command_queue()
{
    return serial->alloc_command_queue();
}

std::shared_ptr<CommandWrapper> MCU::lookup_command(
    const std::string &msgformat,
    std::shared_ptr<command_queue> cq)
{
    return std::make_shared<CommandWrapper>(serial, msgformat, cq);
}

std::shared_ptr<CommandQueryWrapper> MCU::lookup_query_command(const std::string &msgformat,
                                                               const std::string &respformat, uint32_t oid,
                                                               std::shared_ptr<command_queue> cq, bool is_async)
{
    return std::make_shared<CommandQueryWrapper>(serial, msgformat, respformat, oid, cq, is_async);
}

std::shared_ptr<CommandWrapper> MCU::try_lookup_command(const std::string &msgformat)
{
    try
    {
        return lookup_command(msgformat);
    }
    catch (const std::exception &e)
    {
        return nullptr;
    }
}

std::map<std::string, std::map<std::string, int>> MCU::get_enumerations()
{
    return serial->get_msgparser()->get_enumerations();
}

std::map<std::string, std::string> MCU::get_constants()
{
    return serial->get_msgparser()->get_constants();
}

float MCU::get_constant_float(const std::string &name)
{
    return serial->get_msgparser()->get_constant_float(name);
}

uint64_t MCU::print_time_to_clock(double print_time)
{
    return clocksync->print_time_to_clock(print_time);
}

double MCU::clock_to_print_time(double clock)
{
    return clocksync->clock_to_print_time(clock);
}

double MCU::estimated_print_time(double eventtime)
{
    return clocksync->estimated_print_time(eventtime);
}

uint64_t MCU::clock32_to_clock64(uint32_t clock32)
{
    return clocksync->clock32_to_clock64(clock32);
}

void MCU::register_stepqueue(std::shared_ptr<stepcompress> stepqueue)
{
    sc.push_back(stepqueue);
}

void MCU::request_move_queue_slot()
{
    reserved_move_slots++;
}

void MCU::register_flush_callback(std::function<void(double, double)> callback)
{
    flush_callbacks.push_back(callback);
}

void MCU::flush_moves(double print_time, double clear_history_time)
{
    if (stepper_sync == nullptr)
    {
        SPDLOG_ERROR("__func__:{} stepper_sync == nullptr", __func__);
        return;
    }

    uint64_t clock = print_time_to_clock(print_time);
    // SPDLOG_DEBUG("__func__:{},print_time:{},clock:{}",__func__,print_time,clock);
    // TODO?
    if (clock < 0)
    {
        return;
    }

    for (const auto &cb : flush_callbacks)
    {
        cb(print_time, clock);
    }

    uint64_t clear_history_clock = std::max<uint64_t>(0, print_time_to_clock(clear_history_time));
    // SPDLOG_DEBUG("__func__:{},clear_history_time:{},clear_history_clock:{}",__func__,clear_history_time,clear_history_clock);

    int ret = steppersync_flush(stepper_sync.get(), clock, clear_history_clock);
    if (ret != 0)
    {
        throw std::runtime_error("Internal error in MCU '" + name + "' stepcompress");
    }
    // SPDLOG_DEBUG("__func__:{},print_time:{},clock:{}",__func__,print_time,clock);
}

void MCU::check_active(double print_time, double eventtime)
{
    if (stepper_sync == nullptr)
    {
        return;
    }

    std::pair<double, double> result = clocksync->calibrate_clock(print_time, eventtime);
    steppersync_set_time(stepper_sync.get(), result.first, result.second);

    if (clocksync->is_active() || is_fileoutput() || is_timeout)
    {
        return;
    }

    is_timeout = true;
    std::cout << "Timeout with MCU '" << name << "' (eventtime=" << eventtime << ")\n";
    SPDLOG_WARN("Timeout with MCU {} eventtime: {}", name, eventtime);

    printer->invoke_shutdown("Lost communication with MCU '" + name + "'");
}

bool MCU::is_fileoutput()
{
    auto start_args = printer->get_start_args();
    return start_args.find("debugoutput") != start_args.end();
}

bool MCU::get_shutdown()
{
    return is_shutdown;
}

uint64_t MCU::get_shutdown_clock()
{
    return shutdown_clock;
}

json MCU::get_status()
{
    return get_status_info;
}

std::pair<bool, std::string> MCU::stats(double eventtime)
{
    std::ostringstream loadStream;
    loadStream << "mcu_awake=" << std::fixed << std::setprecision(3) << mcu_tick_awake
               << " mcu_task_avg=" << std::fixed << std::setprecision(6) << mcu_tick_avg
               << " mcu_task_stddev=" << std::fixed << std::setprecision(6) << mcu_tick_stddev;
    std::string load = loadStream.str();

    std::string stats = load + " " + serial->stats(eventtime) + " " + clocksync->stats(eventtime);

    std::map<std::string, double> last_stats;
    std::vector<std::string> parts = split(stats, ' ');
    // bug here
    for (const std::string &part : parts)
    {
        auto pos = part.find('=');
        if (pos != std::string::npos)
        {
            std::string key = part.substr(0, pos);
            std::string value = part.substr(pos + 1);
            last_stats[key] = (value.find('.') != std::string::npos) ? std::stod(value) : std::stoul(value);
        }
    }
    get_status_info["last_stats"] = last_stats;

    std::ostringstream resultStream;
    resultStream << name << ": " << stats;
    return {false, resultStream.str()};
}

void MCU::handle_mcu_stats(json params)
{
    uint32_t count = std::stoul(params["count"].get<std::string>());
    uint32_t tick_sum = std::stoul(params["sum"].get<std::string>());

    double c = 1.0 / (count * mcu_freq);
    mcu_tick_avg = tick_sum * c;

    double tick_sumsq = std::stoul(params["sumsq"].get<std::string>()) * stats_sumsq_base;
    double diff = count * tick_sumsq - std::pow(tick_sum, 2);
    mcu_tick_stddev = c * std::sqrt(std::max(0.0, diff));
    mcu_tick_awake = tick_sum / mcu_freq;
}

void MCU::handle_shutdown(json params)
{
    uint32_t clock = 0;
    if (is_shutdown)
    {
        return;
    }
    is_shutdown = true;

    // 查看由于tiemr too close导致的时间误差
    uint32_t timer_too_close_clock_32[2];
    uint64_t timer_too_close_clock_64[2];
    if (serial->check_timer_too_close(timer_too_close_clock_32))
    {
        timer_too_close_clock_64[0] = clock32_to_clock64(timer_too_close_clock_32[0]);
        timer_too_close_clock_64[1] = clock32_to_clock64(timer_too_close_clock_32[1]);
        SPDLOG_INFO("check_timer_too_close except {} {} actual {} {} delay_ms {}", timer_too_close_clock_32[0], timer_too_close_clock_64[0], timer_too_close_clock_32[1], timer_too_close_clock_64[1],
                    (timer_too_close_clock_32[1] - timer_too_close_clock_32[0]) * 1000. / mcu_freq);
    }

    if (params.contains("clock") && !params["clock"].is_null())
    {
        clock = std::stoul(params["clock"].get<std::string>());
        shutdown_clock = clock32_to_clock64(clock);
    }

    shutdown_msg = params["static_string_id"].get<std::string>();
    int shutdown_msg_code = atoi(shutdown_msg.c_str());
    std::map<std::string, std::map<std::string, int>> enumerations = get_enumerations();
    if (enumerations.find("static_string_id") != enumerations.end())
    {
        std::map<std::string, int> static_string_id = enumerations.at("static_string_id");
        for (auto it = static_string_id.begin(); it != static_string_id.end(); ++it)
        {
            if (it->second == shutdown_msg_code)
            {
                shutdown_msg = it->first;
                break;
            }
        }
    }
    SPDLOG_ERROR("mcu {} clock32 {} clock64 {} shutdown id {} msg {}", name, clock, shutdown_clock, shutdown_msg_code, shutdown_msg);

    if (shutdown_msg != "shutdown_pins")
    {
        std::string event_type = params["#name"].get<std::string>();
        printer->invoke_async_shutdown("MCU shutdown", {{"reason", shutdown_msg},
                                                        {"mcu", name},
                                                        {"event_type", event_type}});
        SPDLOG_ERROR("MCU '" + name + "' " + event_type + ": " + shutdown_msg + "\n" + clocksync->dump_debug() + "\n" + serial->dump_debug());

        // 关闭超级电容控制引脚
        elegoo::common::SignalManager::get_instance().emit_signal("mcu:mcu_shutdown");
    }
}

void MCU::handle_starting(json params)
{
    if (!is_shutdown)
    {
        std::string message = "MCU '" + name + "' spontaneous restart";
        printer->invoke_async_shutdown(message);
    }
}

void MCU::check_restart(const std::string &reason)
{
    auto start_args = printer->get_start_args();
    std::string start_reason = start_args["start_reason"];

    if (start_reason == "firmware_restart")
    {
        return;
    }
    SPDLOG_WARN("Attempting automated MCU {} restart: {}", name, reason);
    printer->request_exit("firmware_restart");
    reactor->pause(get_monotonic() + 2.000);
    throw std::runtime_error("Attempt MCU '" + name + "' restart failed");
}

void MCU::connect_file(bool pace)
{
    auto start_args = printer->get_start_args();
    std::string out_fname, dict_fname;

    if (name == "mcu")
    {
        out_fname = start_args["debugoutput"];
        dict_fname = start_args["dictionary"];
    }
    else
    {
        out_fname = start_args["debugoutput"] + "-" + name;
        dict_fname = start_args["dictionary_" + name];
    }

    int outfile = open(out_fname.c_str(), O_WRONLY | O_NOCTTY);
    if (outfile == -1)
    {
        std::cerr << "Failed to open output file: " << out_fname << ", Error: " << strerror(errno) << std::endl;
        return;
    }

    // 打开字典文件，使用 O_RDONLY
    int dfile = open(dict_fname.c_str(), O_RDONLY | O_NOCTTY);
    if (dfile == -1)
    {
        std::cerr << "Failed to open dictionary file: " << dict_fname << ", Error: " << strerror(errno) << std::endl;
        close(outfile);
        return;
    }

    std::vector<uint8_t> dict_data;
    uint8_t buffer[4096];
    ssize_t bytesRead;
    while ((bytesRead = read(dfile, buffer, sizeof(buffer))) > 0)
    {
        dict_data.insert(dict_data.end(), buffer, buffer + bytesRead);
    }

    // 关闭字典文件
    close(dfile);

    serial->connect_file(outfile, dict_data);
    clocksync->connect_file(serial, pace);

    if (!pace)
    {
        // estimated_print_time = [](double) { return 0.0; };
    }
}

void MCU::send_config(uint32_t prev_crc)
{
    SPDLOG_INFO("config_callbacks.size:{}", config_callbacks.size());
    for (const auto &cb : config_callbacks)
    {
        cb();
    }

    config_cmds.insert(config_cmds.begin(),
                       "allocate_oids count=" + std::to_string(oid_count));

    SPDLOG_INFO("prev_crc:{},name:{}", prev_crc, name);
    // 解析 pin 名称
    std::shared_ptr<PrinterPins> ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
    std::shared_ptr<PinResolver> pin_resolver = ppins->get_pin_resolver(name);
    // SPDLOG_INFO("__func__:{},config_cmds.size:{},restart_cmds.size:{},init_cmds.size:{}",__func__,config_cmds.size(),restart_cmds.size(),init_cmds.size());
    for (auto cmdlist : {&config_cmds, &restart_cmds, &init_cmds})
    {
        for (auto it = 0; it < cmdlist->size(); ++it)
        {
            // SPDLOG_INFO("__func__:{},cmdlist.size():{},cmdlist[it]:{}",__func__,cmdlist->size(),(*cmdlist)[it]);
            (*cmdlist)[it] = pin_resolver->update_command((*cmdlist)[it]);
            // SPDLOG_INFO("__func__:{},cmdlist.size():{},cmdlist[it]:{}",__func__,cmdlist->size(),(*cmdlist)[it]);
        }
    }
    // SPDLOG_INFO("__func__:{},config_cmds[0]:{}",__func__,config_cmds[0]);
    std::string encoded_config = elegoo::common::join(config_cmds, "\n");
    SPDLOG_DEBUG("config_cmds.size:{},encoded_config.size:{},encoded_config:\n{}", config_cmds.size(), encoded_config.size(), encoded_config);

    uint32_t config_crc = crc32(0L, reinterpret_cast<const unsigned char *>(encoded_config.c_str()), encoded_config.size()) & 0xffffffff;
    SPDLOG_INFO("config_crc {} prev_crc {}", config_crc, prev_crc);
    add_config_cmd("finalize_config crc=" + std::to_string(config_crc));

    // 如果之前的 CRC 不匹配，进行重启检查
    if (prev_crc != 0 && config_crc != prev_crc)
    {
        check_restart("CRC mismatch");
        // throw std::runtime_error("MCU '" + name + "' CRC does not match config");
        SPDLOG_ERROR("MCU '" + name + "' CRC does not match config");
        return;
    }

    // 传输配置消息（如有需要）
    register_response([this](const json &params)
                      { handle_starting(params); }, "starting");
    try
    {
        if (prev_crc == 0)
        {
            // std::cout << "Sending MCU '" << name << "' printer configuration..." << std::endl;
            SPDLOG_INFO("Sending MCU '{}' printer configuration... config_cmds.size:{}", name, config_cmds.size());
            for (const auto &cmd : config_cmds)
            {
                SPDLOG_INFO("cmd:{}", cmd);
                serial->send(cmd);
            }
        }
        else
        {
            SPDLOG_INFO("Sending MCU '{}' printer configuration... restart_cmds.size:{}", name, restart_cmds.size());
            for (const auto &cmd : restart_cmds)
            {
                serial->send(cmd);
            }
        }

        // 传输初始化消息
        for (const auto &cmd : init_cmds)
        {
            serial->send(cmd);
        }
    }
    catch (const std::exception &e)
    {
        std::string enum_name = "pin";          // 模拟的异常枚举
        std::string enum_value = "unknown_pin"; // 模拟的异常值
        if (enum_name == "pin")
        {
            throw std::runtime_error(
                "Pin '" + enum_value + "' is not a valid pin name on mcu '" + name + "'");
        }
        throw;
    }
    SPDLOG_DEBUG("__func__:{},__LINE__:{}", __func__, __LINE__);
}

json MCU::send_get_config()
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    std::shared_ptr<CommandQueryWrapper> get_config_cmd =
        lookup_query_command(
            "get_config",
            "config is_config=%c crc=%u is_shutdown=%c move_count=%hu");

    if (is_fileoutput())
    {
        json value;
        value["is_config"] = std::to_string(true);
        value["move_count"] = std::to_string(500);
        value["crc"] = std::to_string(0);
        return value;
    }

    json config_params = get_config_cmd->send();

    SPDLOG_DEBUG("__func__:{},config_params['is_shutdown']:{}", __func__, config_params["is_shutdown"].get<std::string>());
    SPDLOG_DEBUG("__func__:{},config_params['is_config']:{}", __func__, config_params["is_config"].get<std::string>());
    // SPDLOG_DEBUG("__func__:{},config_params['move_count']:{}",__func__,config_params["move_count"].get<std::string>());
    // SPDLOG_DEBUG("__func__:{},config_params['crc']:{}",__func__,config_params["crc"].get<std::string>());
    if (is_shutdown)
    {
        // throw std::runtime_error("MCU '" + name + "' error during config: " + shutdown_msg);
        SPDLOG_ERROR("MCU '" + name + "' error during config: " + shutdown_msg);
    }

    if (std::stoi(config_params["is_shutdown"].get<std::string>()))
    {
        // throw std::runtime_error("无法更新MCU '" + name + "' 的配置，因为它已关闭");
        SPDLOG_ERROR("Can not update MCU '" + name + "' config as it is shutdown");
        check_restart("full reset before config");
    }
    return config_params;
}

std::string MCU::log_info()
{
    std::shared_ptr<MessageParser> msgparser = serial->get_msgparser();
    int message_count = msgparser->get_messages().size();
    std::pair<std::string, std::string> info = msgparser->get_version_info();

    std::stringstream log_info;
    log_info << "Loaded MCU '" << name << "' " << message_count << " commands ("
             << info.first << " / " << info.second << ")\n";

    log_info << "MCU '" << name << "' config: ";
    // const auto& constants = get_constants();
    // for (auto it = constants.begin(); it != constants.end(); ++it) {
    //     log_info << it->first << "=" << it->second;
    //     if (std::next(it) != constants.end()) {
    //         log_info << " ";
    //     }
    // }

    return log_info.str();
}

void MCU::connect()
{
    json config_params = send_get_config();

    if (!std::stoi(config_params["is_config"].get<std::string>()))
    {
        send_config(0);
        config_params = send_get_config();
        if (!std::stoi(config_params["is_config"].get<std::string>()) && !is_fileoutput())
        {
            throw std::runtime_error("Unable to configure MCU '" + name + "'");
        }
    }
    else
    {
        std::unordered_map<std::string, std::string> start_args = printer->get_start_args();
        auto it = start_args.find("start_reason");
        if (it != start_args.end() && it->second == "firmware_restart")
        {
            throw std::runtime_error("Failed automated reset of MCU '" + name + "'");
        }

        send_config(std::stoul(config_params["crc"].get<std::string>()));
    }
    int move_count = std::stoul(config_params["move_count"].get<std::string>());
    if (move_count < reserved_move_slots)
    {
        throw std::runtime_error("Too few moves available on MCU '" + name + "'");
    }

    sc_list = new stepcompress *[sc.size()];
    for (size_t i = 0; i < sc.size(); ++i)
    {
        sc_list[i] = sc[i].get();
    }

    // SPDLOG_INFO("__func__:{},move_count:{},reserved_move_slots:{}",__func__,move_count,reserved_move_slots);
    stepper_sync = std::shared_ptr<steppersync>(
        steppersync_alloc(serial->get_serialqueue().get(),
                          sc_list, sc.size(), move_count - reserved_move_slots),
        steppersync_free);
    steppersync_set_time(stepper_sync.get(), 0., mcu_freq);
}

void MCU::mcu_identify()
{
    if (is_fileoutput())
    {
        connect_file();
    }
    else
    {
        std::string resmeth = restart_method;
        try
        {
            if (!canbus_iface.empty())
            {
                std::shared_ptr<PrinterCANBus> cbid =
                    any_cast<std::shared_ptr<PrinterCANBus>>(printer->lookup_object("canbus_ids"));
                int nodeid = cbid->get_nodeid(serialport);
                serial->connect_canbus(serialport, nodeid, canbus_iface);
            }
            else if (is_rpmsg)
            {
                serial->connect_rpmsg(serialport);
            }
            else if (baud > 0)
            {
                bool rts = (resmeth != "cheetah");
                serial->connect_uart(serialport, baud, rts);
            }
            else
            {
                serial->connect_pipe(serialport);
            }
            clocksync->connect(serial);
        }
        catch (const std::exception &e)
        {
            std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));

            if (name == "toolhead")
            {
                gcode->respond_ecode("", elegoo::common::ErrorCode::EXTRUDER_CONNECT_FAIL,
                                     elegoo::common::ErrorLevel::WARNING);
            }
            else if (name == "bed_sensor")
            {
                gcode->respond_ecode("", elegoo::common::ErrorCode::BED_MESH_CONNECT_FAIL,
                                     elegoo::common::ErrorLevel::WARNING);
            }

            throw std::runtime_error(e.what());
        }
    }

    std::shared_ptr<PrinterPins> ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
    std::shared_ptr<PinResolver> pin_resolver = ppins->get_pin_resolver(name);
    for (const auto &pair : get_constants())
    {
        const std::string &cname = pair.first;
        const std::string &value = pair.second;
        const std::string prefix = "RESERVE_PINS_";
        if (!cname.compare(0, prefix.length(), prefix))
        {
            std::istringstream ss(value);
            std::string pin;
            std::string substr = cname.substr(13);
            SPDLOG_DEBUG("__func__:{},cname:{},value:{}", __func__, cname, value);
            while (std::getline(ss, pin, ','))
            {
                SPDLOG_DEBUG("__func__:{},value:{},pin:{},substr:{}", __func__, value, pin, substr);
                pin_resolver->reserve_pin(pin, substr);
            }
        }
    }

    mcu_freq = get_constant_float("CLOCK_FREQ");               // 84000000;//
    stats_sumsq_base = get_constant_float("STATS_SUMSQ_BASE"); // 256;//
    emergency_stop_cmd = lookup_command("emergency_stop");
    reset_cmd = try_lookup_command("reset");
    config_reset_cmd = try_lookup_command("config_reset");

    bool ext_only = (reset_cmd == nullptr && config_reset_cmd == nullptr);
    std::shared_ptr<MessageParser> msgparser = serial->get_msgparser();
    int default_baud = 1000000;
    int mbaud = msgparser->get_constant_int("SERIAL_BAUD", &default_baud); // 250000;//

#if 0
    if (msgparser->get_constant_int("CANBUS_BRIDGE", 0) != 0)
    {
        is_mcu_bridge = true;

        elegoo::common::SignalManager::get_instance().register_signal(
            "elegoo:firmware_restart",
            std::function<void()>([this]()
                                  { firmware_restart_bridge(); }));
    }
#endif

    std::pair<std::string, std::string> info = msgparser->get_version_info();
    get_status_info["mcu_version"] = info.first;
    get_status_info["mcu_build_versions"] = info.second;
    get_status_info["mcu_constants"] = msgparser->get_constants();

    register_response([this](const json &params)
                      { handle_shutdown(params); }, "shutdown");
    register_response([this](const json &params)
                      { handle_shutdown(params); }, "is_shutdown");
    register_response([this](const json &params)
                      { handle_mcu_stats(params); }, "stats");
}

void MCU::ready()
{
    if (is_fileoutput())
    {
        return;
    }

    double mcu_freq = this->mcu_freq;
    double systime = get_monotonic();
    int calc_freq = clocksync->get_clock(systime + 1) - clocksync->get_clock(systime);
    int mcu_freq_mhz = static_cast<int>(mcu_freq / 1000000.0 + 0.5);
    int calc_freq_mhz = static_cast<int>(calc_freq / 1000000.0 + 0.5);
    SPDLOG_INFO("mcu:{},mcu_freq_mhz:{},calc_freq_mhz:{},calc_freq:{}", name, mcu_freq_mhz, calc_freq_mhz, calc_freq);

    if (mcu_freq_mhz != calc_freq_mhz)
    {
        std::shared_ptr<PrinterConfig> pconfig =
            any_cast<std::shared_ptr<PrinterConfig>>(printer->lookup_object("configfile"));
        std::string msg = "MCU '" + name + "' configured for " +
                          std::to_string(mcu_freq_mhz) + "Mhz but running at " +
                          std::to_string(calc_freq_mhz) + "Mhz!";
        pconfig->runtime_warning(msg);
    }
}

void MCU::disconnect()
{
    serial->disconnect();
    stepper_sync = nullptr;
}

void MCU::shutdown(bool force)
{
    // SPDLOG_INFO("__func__:{},is_shutdown:{},force:{}",__func__,is_shutdown,force);
    if (emergency_stop_cmd == nullptr || (is_shutdown && !force))
    {
        return;
    }
    emergency_stop_cmd->send();
}

void MCU::restart_via_command()
{
    if ((reset_cmd == nullptr && config_reset_cmd == nullptr) ||
        !clocksync->is_active())
    {
        SPDLOG_INFO("Unable to issue reset command on MCU {}", name);
        return;
    }

    if (reset_cmd == nullptr)
    {
        SPDLOG_INFO("Attempting MCU {} config_reset command", name);
        is_shutdown = true;
        shutdown(true); // Force shutdown
        reactor->pause(get_monotonic() + 0.015);
        config_reset_cmd->send();
    }
    else
    {
        SPDLOG_INFO("Attempting MCU {} reset command", name);
        reset_cmd->send();
    }

    SPDLOG_INFO("MCU {} reset command #1", name);
    reactor->pause(get_monotonic() + 0.015);
    SPDLOG_INFO("MCU {} reset command #2", name);
    disconnect();
    SPDLOG_INFO("MCU {} reset command #3", name);
}

void MCU::restart_via_remoterpc()
{
    SPDLOG_INFO("restart_via_remoterpc");
    // 需要先关闭设备，释放资源后再重启DSP
    disconnect();
    if (remoterpc_ctrl(0) != 0)
        SPDLOG_ERROR("remoterpc_ctrl failed");
    reactor->pause(get_monotonic() + 1.0);
    if (remoterpc_ctrl(1) != 0)
        SPDLOG_ERROR("remoterpc_ctrl failed");
    reactor->pause(get_monotonic() + 1.0);
}

void MCU::firmware_restart(bool force)
{
    if (is_mcu_bridge && !force)
    {
        return;
    }
    if (restart_method == "command")
    {
        restart_via_command();
    }
    if (restart_method == "remoterpc")
    {
        restart_via_remoterpc();
    }
}

void MCU::firmware_restart_bridge()
{
    firmware_restart(true);
}

std::vector<std::string> MCU::split(const std::string &s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}
