/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:54:22
 * @LastEditors  : error: git config user.name & please set dead value or install git
 * @LastEditTime : 2025-03-01 17:07:08
 * @Description  : The toolhead module in Elegoo is responsible for managing
 * the printer's extruder (or toolhead) and its related functions.
 * It coordinates the movement of the extruder, handles temperature control,
 * and provides status queries, ensuring that the printer can perform printing
 * tasks efficiently and accurately.
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "toolhead.h"
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>
#include <dlfcn.h> // For dynamic loading of modules
#include "chelper/trapq.h"
#include <cmath>
#include "pyhelper.h"
#include <dlfcn.h>
#include "kinematics_factory.h"
#include "extruder.h"
#include "idle_timeout.h"
#include "statistics.h"
#include "printer.h"
#include "logger.h"

const double BUFFER_TIME_LOW = 1.0;
const double BUFFER_TIME_HIGH = 2.0;
const double BUFFER_TIME_START = 0.250;
const double BGFLUSH_LOW_TIME = 0.200;
const double BGFLUSH_BATCH_TIME = 0.200;
const double BGFLUSH_EXTRA_TIME = 0.250;
const double MIN_KIN_TIME = 0.100;
const double MOVE_BATCH_TIME = 0.500;
const double STEPCOMPRESS_FLUSH_TIME = 0.050;
const double SDS_CHECK_TIME = 0.001;
const double MOVE_HISTORY_EXPIRE = 30.0;

const double DRIP_SEGMENT_TIME = 0.050;
const double DRIP_TIME = 0.100;

// 启动定时器最小唤醒时间
const double CHECK_PAUSE_TIME = 0.1;

// 前瞻队列刷新阈值
const double LOOKAHEAD_FLUSH_TIME = 0.250;

// template <typename T>
// inline T min(T first)
// {
//     return first;
// }

// template <typename T, typename... Args>
// inline T min(T first, Args... args)
// {
//     return std::min(first, min(args...));
// }

// template <typename T>
// inline T max(T first)
// {
//     return first;
// }

// template <typename T, typename... Args>
// inline T max(T first, Args... args)
// {
//     return std::max(first, max(args...));
// }

Move::Move(std::shared_ptr<ToolHead> th, std::vector<double> s_pos, std::vector<double> e_pos, double spd)
{
    axes_d.resize(4, 0.0);
    axes_r.resize(4, 0.0);
    toolhead = th;
    start_pos = s_pos;
    end_pos = e_pos;
    accel = toolhead->max_accel;
    junction_deviation = toolhead->junction_deviation;
    double velocity = std::min(spd, toolhead->max_velocity);
    is_kinematic_move = true;
    std::vector<double> ax_d(4, 0.0);
    for (int i = 0; i < 4; i++)
    {
        axes_d[i] = ax_d[i] = e_pos[i] - s_pos[i];
    }

    double mv_d;
    move_d = mv_d = sqrt(axes_d[0] * axes_d[0] + axes_d[1] * axes_d[1] + axes_d[2] * axes_d[2]);
    // SPDLOG_INFO("move_d {}", move_d);
    double inv_move_d;
    if (mv_d < 0.000000001)
    {
        for (int i = 0; i < 3; i++)
        {
            end_pos[i] = s_pos[i];
        }
        ax_d[0] = ax_d[1] = ax_d[2] = 0.0;
        move_d = mv_d = fabs(ax_d[3]);
        inv_move_d = 0.0;
        if (mv_d)
        {
            inv_move_d = 1.0 / mv_d;
        }
        accel = 99999999.9;
        velocity = spd;
        is_kinematic_move = false;
    }
    else
    {
        inv_move_d = 1.0 / mv_d;
    }
    for (int i = 0; i < 4; i++)
    {
        axes_r[i] = inv_move_d * ax_d[i];
    }
    min_move_t = mv_d / velocity;
    max_start_v2 = 0;
    max_cruise_v2 = velocity * velocity;
    delta_v2 = 2.0 * mv_d * accel;
    max_smoothed_v2 = 0.0;
    smooth_delta_v2 = 2.0 * mv_d * toolhead->max_accel_to_decel;
    next_junction_v2 = 999999999.9;
}

Move::~Move()
{
}

void Move::limit_speed(double speed, double acc)
{
    double speed2 = speed * speed;
    if (speed2 < max_cruise_v2)
    {
        max_cruise_v2 = speed2;
        min_move_t = move_d / speed;
    }
    accel = std::min(accel, acc);
    delta_v2 = 2.0 * move_d * accel;
    smooth_delta_v2 = std::min(smooth_delta_v2, delta_v2);
}

void Move::limit_next_junction_speed(double speed)
{
    next_junction_v2 = std::min(next_junction_v2, speed * speed);
}

elegoo::common::CommandError Move::move_error(const std::string &msg)
{
    std::vector<double> ep = end_pos;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s: %.3f %.3f %.3f [%.3f]", msg.c_str(), ep[0], ep[1], ep[2], ep[3]);
    return elegoo::common::CommandError(buf);
}

void Move::calc_junction(Move *prev_move)
{
    if (!is_kinematic_move || !prev_move->is_kinematic_move)
    {
        return;
    }
    double extruder_v2 = toolhead->extruder->calc_junction(prev_move, this);
    double max_start_v2 = std::min({extruder_v2, max_cruise_v2,
                               prev_move->max_cruise_v2, prev_move->next_junction_v2,
                               prev_move->max_start_v2 + prev_move->delta_v2});
    double junction_cos_theta = -(axes_r[0] * prev_move->axes_r[0] + axes_r[1] * prev_move->axes_r[1] + axes_r[2] * prev_move->axes_r[2]);
    double sin_theta_d2 = sqrt(std::max(0.5 * (1.0 - junction_cos_theta), 0.));
    double cos_theta_d2 = sqrt(std::max(0.5 * (1.0 + junction_cos_theta), 0.));
    double one_minus_sin_theta_d2 = 1. - sin_theta_d2;
    if (one_minus_sin_theta_d2 > 0. && cos_theta_d2 > 0.)
    {
        double R_jd = sin_theta_d2 / one_minus_sin_theta_d2;
        double move_jd_v2 = R_jd * junction_deviation * accel;
        double pmove_jd_v2 = R_jd * prev_move->junction_deviation * prev_move->accel;
        double quarter_tan_theta_d2 = .25 * sin_theta_d2 / cos_theta_d2;
        double move_centripetal_v2 = delta_v2 * quarter_tan_theta_d2;
        double pmove_centripetal_v2 = prev_move->delta_v2 * quarter_tan_theta_d2;
        max_start_v2 = std::min({max_start_v2, move_jd_v2, pmove_jd_v2,
                            move_centripetal_v2, pmove_centripetal_v2});
    }

    this->max_start_v2 = max_start_v2;
    this->max_smoothed_v2 = std::min(
        max_start_v2, prev_move->max_smoothed_v2 + prev_move->smooth_delta_v2);
}

void Move::set_junction(double start_v2, double cruise_v2, double end_v2)
{
    // t_start_v2 = start_v2;
    // t_cruise_v2 = cruise_v2;
    // t_end_v2 = end_v2;

    double half_inv_accel = .5 / accel;
    double accel_d = (cruise_v2 - start_v2) * half_inv_accel;
    double decel_d = (cruise_v2 - end_v2) * half_inv_accel;
    double cruise_d = move_d - accel_d - decel_d;

    start_v = sqrt(start_v2);
    cruise_v = sqrt(cruise_v2);
    end_v = sqrt(end_v2);
    accel_t = accel_d / ((start_v + cruise_v) * 0.5);
    cruise_t = cruise_d / cruise_v;
    decel_t = decel_d / ((end_v + cruise_v) * 0.5);

    if (std::abs(accel_t) < 1e-9)
        accel_t = 0;
    if (std::abs(cruise_t) < 1e-9)
        cruise_t = 0;
    if (std::abs(decel_t) < 1e-9)
        decel_t = 0;
}

void Move::printf_data()
{
    SPDLOG_INFO("print_data print_time={}, start_v2={}, cruise_v2={}, end_v2={}, accel={}, move_d={}, accel_t={}, cruise_t={}, decel_t={}, pos[0]={}, pos[1]={}, pos[2]={}",
                next_time, t_start_v2, t_cruise_v2, t_end_v2, accel, move_d, accel_t, cruise_t, decel_t, start_pos[0], start_pos[1], start_pos[2]);
}

ToolHead::ToolHead(std::shared_ptr<ConfigWrapper> config)
    : commanded_pos{0, 0, 0, 0}, config(config)
{
    move_transform = std::make_shared<GCodeMoveTransform>();
    move_transform->move_with_transform = std::bind(&ToolHead::move, this, std::placeholders::_1, std::placeholders::_2);
    move_transform->position_with_transform = std::bind(&ToolHead::get_position, this);
}

ToolHead::~ToolHead()
{
}

void ToolHead::init()
{
    printer = config->get_printer();
    reactor = printer->get_reactor();

    std::map<std::string, Any> mcu_map = printer->lookup_objects("mcu");
    for (auto &pair : mcu_map)
    {
        all_mcus.push_back(any_cast<std::shared_ptr<MCU>>(pair.second));
    }
    mcu = all_mcus[0];

    lookahead = std::make_shared<LookAheadQueue>(shared_from_this());
    lookahead->set_flush_time(BUFFER_TIME_HIGH);

    max_velocity = config->getdouble("max_velocity", DOUBLE_INVALID, DOUBLE_NONE, DOUBLE_NONE, 0.);
    max_accel = config->getdouble("max_accel", DOUBLE_INVALID, DOUBLE_NONE, DOUBLE_NONE, 0.);

    min_cruise_ratio = 0.5;
    if (std::isnan(config->getdouble("minimum_cruise_ratio", DOUBLE_NONE)))
    {
        req_accel_to_decel = config->getdouble("max_accel_to_decel", DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, 0.);
        if (!std::isnan(req_accel_to_decel))
        {
            config->deprecate("max_accel_to_decel");
            min_cruise_ratio = 1.0 - std::min(1.0, (req_accel_to_decel / max_accel));
        }
    }

    min_cruise_ratio = config->getdouble("minimum_cruise_ratio", min_cruise_ratio, 0., DOUBLE_NONE, DOUBLE_NONE, 1.);
    square_corner_velocity = config->getdouble("square_corner_velocity", 5., 0.);
    junction_deviation = max_accel_to_decel = 0.;
    _calc_junction_deviation();
    check_stall_time = 0.;
    print_stall = 0;
    can_pause = true;
    if (mcu->is_fileoutput())
    {
        can_pause = false;
    }

    need_check_pause = -1.0;
    print_time = 0.0;
    special_queuing_state = "NeedPrime";
    flush_timer = reactor->register_timer([this](double eventtime)
                                          { return _flush_handler(eventtime); }, _NEVER, "flush_timer");
    do_kick_flush_timer = true;
    last_flush_time = min_restart_time = 0.0;
    need_flush_time = step_gen_time = clear_history_time = 0.0;
    kin_flush_delay = SDS_CHECK_TIME;
    kin_flush_times.clear();
    step_generators.clear();
    tra = std::shared_ptr<trapq>(
        trapq_alloc(),
        trapq_free);
    gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
    extruder = std::make_shared<DummyExtruder>(printer); // kinermatics is import module

    try
    {
        kin_name = config->get("kinematics");
        kin = KinematicsFactory::create_kinematics(kin_name, shared_from_this(), config);
    }
    catch (const elegoo::common::ConfigParserError &e)
    {
        throw;
    }
    catch (const elegoo::common::PinsError &e)
    {
        throw;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading kinematics '" << kin_name << "': " << e.what() << std::endl;
        throw elegoo::common::ConfigParserError("Error loading kinematics '" + kin_name + "'");
    }

    gcode->register_command(
        "G4",
        [this](std::shared_ptr<GCodeCommand> cmd)
        { this->cmd_G4(cmd); });
    gcode->register_command(
        "M400",
        [this](std::shared_ptr<GCodeCommand> cmd)
        { this->cmd_M400(cmd); });

    gcode->register_command(
        "SET_VELOCITY_LIMIT",
        [this](std::shared_ptr<GCodeCommand> cmd)
        { this->cmd_SET_VELOCITY_LIMIT(cmd); },
        false,
        "Set printer velocity limits");

    gcode->register_command(
        "M204",
        [this](std::shared_ptr<GCodeCommand> cmd)
        { this->cmd_M204(cmd); });
    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:shutdown",
        std::function<void()>([this]()
                              { _handle_shutdown(); }));

    modules = {
        "gcode_move",
        "gcode_macro",
        "homing",
        "idle_timeout",
        "statistics",
        // "manual_probe",
        // "tuning_tower"
    };
    for (auto &module_name : modules)
    {
        printer->load_object(config, module_name);
    }
}

void ToolHead::_advance_flush_time(double flush_time)
{
    flush_time = std::max(flush_time, this->last_flush_time);
    double sg_flush_want = std::min(flush_time + STEPCOMPRESS_FLUSH_TIME, this->print_time - this->kin_flush_delay);
    double sg_flush_time = std::max(sg_flush_want, flush_time);
    for (auto sg : step_generators)
    {
        sg(sg_flush_time);
    }

    // 设置最小重启时间
    this->min_restart_time = std::max(this->min_restart_time, sg_flush_time);

    double clear_history_time = this->clear_history_time;
    if (!this->can_pause)
    {
        clear_history_time = flush_time - MOVE_HISTORY_EXPIRE;
    }
    double free_time = sg_flush_time - this->kin_flush_delay;
    trapq_finalize_moves(this->tra.get(), free_time, clear_history_time);
    this->extruder->update_move_time(free_time, clear_history_time);

    for (auto m : all_mcus)
    {
        m->flush_moves(flush_time, clear_history_time);
    }
    this->last_flush_time = flush_time;
}

void ToolHead::_advance_move_time(double next_print_time)
{
    double pt_delay = this->kin_flush_delay + STEPCOMPRESS_FLUSH_TIME;
    double flush_time = std::max(this->last_flush_time, this->print_time - pt_delay);
    // 更新打印时间
    this->print_time = std::max(this->print_time, next_print_time);

    double want_flush_time = std::max(flush_time, this->print_time - pt_delay);
    while (1)
    {
        flush_time = std::min(flush_time + MOVE_BATCH_TIME, want_flush_time);
        _advance_flush_time(flush_time);
        if (flush_time >= want_flush_time)
        {
            break;
        }
    }
}

void ToolHead::_calc_print_time(void)
{
    double curtime = get_monotonic();
    double est_print_time = mcu->estimated_print_time(curtime);
    double kin_time = std::max(est_print_time + MIN_KIN_TIME, min_restart_time);
    kin_time += kin_flush_delay;
    double min_print_time = std::max(est_print_time + BUFFER_TIME_START, kin_time);

    // SPDLOG_INFO("_calc_print_time est_print_time {} print_time {} min_print_time {} kin_time {} ", est_print_time, print_time, min_print_time, kin_time);
    if (min_print_time > print_time)
    {
        // SPDLOG_INFO("_calc_print_time print_time {} min_print_time {}", print_time, min_print_time);
        print_time = min_print_time;
        elegoo::common::SignalManager::get_instance().emit_signal<double, double, double>(
            "toolhead:sync_print_time", curtime, est_print_time, print_time);
    }
}

void ToolHead::_process_moves(std::vector<Move *> moves)
{
    if (!special_queuing_state.empty())
    {
        if (special_queuing_state != "Drip")
        {
            special_queuing_state = "";
            need_check_pause = -1.;
        }
        _calc_print_time();
    }

    double next_move_time = print_time;
    for (auto move : moves)
    {
        // move->next_time = next_move_time;
        if (move->is_kinematic_move)
        {
            // printf("move: %lf accel_t: %lf cruise_t: %lf decel_t: %lf"
            //        " start_pos: %lf, %lf, %lf"
            //        " axes_r: %lf, %lf, %lf"
            //        " start_v: %lf, cruise_v: %lf, accel: %lf\n",
            //        next_move_time, move->accel_t, move->cruise_t, move->decel_t,
            //        move->start_pos[0], move->start_pos[1], move->start_pos[2],
            //        move->axes_r[0], move->axes_r[1], move->axes_r[2],
            //        move->start_v, move->cruise_v, move->accel);
            trapq_append(
                tra.get(), next_move_time,
                move->accel_t, move->cruise_t, move->decel_t,
                move->start_pos[0], move->start_pos[1], move->start_pos[2],
                move->axes_r[0], move->axes_r[1], move->axes_r[2],
                move->start_v, move->cruise_v, move->accel);
        }
        if (move->axes_d[3])
        {
            extruder->move(next_move_time, move);
        }
        next_move_time = (next_move_time + move->accel_t + move->cruise_t + move->decel_t);
        for (auto cb : move->timing_callbacks)
        {
            cb(next_move_time);
        }
    }
    if (!special_queuing_state.empty())
    {
        _update_drip_move_time(next_move_time);
    }
    note_mcu_movequeue_activity(next_move_time + kin_flush_delay, true);
    _advance_move_time(next_move_time);
}

void ToolHead::_flush_lookahead(void)
{
    lookahead->flush();
    special_queuing_state = "NeedPrime";
    need_check_pause = -1.;
    lookahead->set_flush_time(BUFFER_TIME_HIGH);
    check_stall_time = 0.0;
}

void ToolHead::flush_step_generation(void)
{
    _flush_lookahead();
    _advance_flush_time(step_gen_time);
    min_restart_time = std::max(min_restart_time, print_time);
}

double ToolHead::get_last_move_time(void)
{
    if (!special_queuing_state.empty())
    {
        _flush_lookahead();
        _calc_print_time();
    }
    else
    {
        lookahead->flush();
    }
    return print_time;
}

void ToolHead::_check_pause()
{

    double eventtime = get_monotonic();
    double est_print_time = mcu->estimated_print_time(eventtime);
    double buffer_time = print_time - est_print_time;
    // SPDLOG_INFO("_check_pause special_queuing_state {} est_print_time {} print_time {} buffer_time {}", special_queuing_state, est_print_time, print_time, buffer_time);

    // 非主状态切换到"Priming"启动定时器
    if (!special_queuing_state.empty())
    {
        if (check_stall_time)
        {
            if (est_print_time < check_stall_time)
            {
                print_stall += 1;
            }
            check_stall_time = 0.0;
        }

        special_queuing_state = "Priming";
        need_check_pause = -1;
        if (!priming_timer)
        {
            priming_timer = reactor->register_timer([this](const double ettime) -> double
                                                    { return this->_priming_handler(ettime); }, _NEVER, "priming_timer");
        }
        double wtime = eventtime + std::max(CHECK_PAUSE_TIME, buffer_time - BUFFER_TIME_LOW);
        reactor->update_timer(priming_timer, wtime);
    }

    while (1)
    {
        double pause_time = buffer_time - BUFFER_TIME_HIGH;
        if (pause_time <= 0.0)
            break;

        if (!can_pause)
        {
            need_check_pause = reactor->NEVER;
            return;
        }
        // 1.0 -> 0.5,提高检查频率,防止调度波动导致超时
        eventtime = reactor->pause(eventtime + std::min(1.0, pause_time));
        est_print_time = mcu->estimated_print_time(eventtime);
        buffer_time = print_time - est_print_time;
    }

    // 主状态下推迟检查
    if (special_queuing_state.empty())
    {
        need_check_pause = est_print_time + BUFFER_TIME_HIGH + 0.100;
    }
}

double ToolHead::_priming_handler(double ettime)
{
    if (priming_timer)
    {
        reactor->unregister_timer(priming_timer);
        priming_timer = nullptr;
    }

    try
    {
        if (special_queuing_state == "Priming")
        {
            _flush_lookahead();
            check_stall_time = print_time;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in priming_handler: " << e.what() << std::endl;
        printer->invoke_shutdown("Exception in priming_handler");
    }
    return reactor->NEVER;
}

double ToolHead::_flush_handler(double eventtime)
{
    try
    {
        double est_print_time = mcu->estimated_print_time(eventtime);
        if (special_queuing_state.empty())
        {
            double pt_time = print_time;
            double buffer_time = pt_time - est_print_time;
            if (buffer_time > BUFFER_TIME_LOW)
            {
                return eventtime + buffer_time - BUFFER_TIME_LOW;
            }
            _flush_lookahead();
            if (pt_time != print_time)
            {
                check_stall_time = print_time;
            }
        }
        while (1)
        {
            double end_flush = need_flush_time + BGFLUSH_EXTRA_TIME;
            if (last_flush_time >= end_flush)
            {
                do_kick_flush_timer = true;
                return reactor->NEVER;
            }
            double buffer_time = last_flush_time - est_print_time;
            if (buffer_time > BGFLUSH_LOW_TIME)
            {
                return eventtime + buffer_time - BGFLUSH_LOW_TIME;
            }
            double ftime = est_print_time + BGFLUSH_LOW_TIME + BGFLUSH_BATCH_TIME;
            _advance_flush_time(std::min(end_flush, ftime));
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        printer->invoke_shutdown("Exception in flush_handler");
    }
    return reactor->NEVER;
}

std::vector<double> ToolHead::get_position()
{
    return commanded_pos;
}

void ToolHead::set_position(const std::vector<double> &newpos, const std::vector<int> &homing_axes)
{
    if (newpos.size() != 4)
        throw std::runtime_error("set_position newpos must size 4");
    flush_step_generation();
    trapq_set_position(tra.get(), print_time, newpos[0], newpos[1], newpos[2]);
    commanded_pos = newpos;
    kin->set_position(newpos, homing_axes);
    elegoo::common::SignalManager::get_instance().emit_signal("toolhead:set_position");
}

void ToolHead::limit_next_junction_speed(double speed)
{
    Move *last_move = lookahead->get_last(); // Get the last move
    if (last_move != nullptr)
    {
        last_move->limit_next_junction_speed(speed);
    }
}

void ToolHead::move(const std::vector<double> &newpos, double speed)
{
    Move *move = new Move{shared_from_this(), commanded_pos, newpos, speed};
    if (!move->move_d)
    {
        delete move;
        return;
    }
    if (move->is_kinematic_move)
    {
        kin->check_move(move);
    }
    if (move->axes_d[3])
    {
        try
        {
            extruder->check_move(move);
        }
        catch (...)
        {
            delete move;
            return;
            // throw;
        }
    }
    commanded_pos = move->end_pos;
    lookahead->add_move(move);
    if (print_time > need_check_pause)
    {
        _check_pause();
    }
}

void ToolHead::manual_move(const std::vector<double> &coord, const double &speed)
{
    std::vector<double> curpos = commanded_pos;
    for (int i = 0; i < std::min(coord.size(), curpos.size()); i++)
    {
        if (!std::isnan(coord[i]))
        {
            curpos[i] = coord[i];
        }
    }
    move(curpos, speed);
    elegoo::common::SignalManager::get_instance().emit_signal("toolhead:manual_move");
}

void ToolHead::dwell(double delay)
{
    double next_print_time = get_last_move_time() + std::max(0.0, delay);
    _advance_move_time(next_print_time);
    _check_pause();
}

void ToolHead::wait_moves(void)
{
    // SPDLOG_INFO("wait_moves #1");
    _flush_lookahead();
    // SPDLOG_INFO("wait_moves #2");
    double eventtime = get_monotonic();
    while (special_queuing_state.empty() || print_time >= mcu->estimated_print_time(eventtime))
    {
        // SPDLOG_INFO("wait_moves #3");
        if (!can_pause)
        {
            // SPDLOG_INFO("wait_moves #4");
            break;
        }
        eventtime = reactor->pause(eventtime + 0.100);
        // SPDLOG_INFO("wait_moves #5");
    }
    // SPDLOG_INFO("wait_moves #6");
}

void ToolHead::set_extruder(std::shared_ptr<DummyExtruder> extruder, double extruder_pos)
{
    this->extruder = extruder;
    commanded_pos[3] = extruder_pos;
}

std::shared_ptr<DummyExtruder> ToolHead::get_extruder()
{
    return extruder;
}

void ToolHead::_update_drip_move_time(double next_print_time)
{
    double flush_delay = DRIP_TIME + STEPCOMPRESS_FLUSH_TIME + kin_flush_delay;
    while (print_time < next_print_time)
    {
        if (drip_completion->test())
        {
            throw elegoo::common::DripModeEndSignalError("DripModeEndSignalError");
        }
        double curtime = get_monotonic();
        double est_print_time = mcu->estimated_print_time(curtime);
        double wait_time = print_time - est_print_time - flush_delay;
        if (wait_time > 0.0 && can_pause)
        {
            drip_completion->wait(curtime + wait_time);
            continue;
        }
        double npt = std::min(print_time + DRIP_SEGMENT_TIME, next_print_time);
        note_mcu_movequeue_activity(npt + kin_flush_delay, true); // to do confirm parameter
        _advance_move_time(npt);
    }
}

void ToolHead::drip_move(std::vector<double> &newpos, double speed, std::shared_ptr<ReactorCompletion> drip) // to do comfirm parameter
{
    dwell(kin_flush_delay);
    lookahead->flush();
    special_queuing_state = "Drip";
    need_check_pause = reactor->NEVER;
    reactor->update_timer(flush_timer, reactor->NEVER);
    do_kick_flush_timer = false;
    lookahead->set_flush_time(BUFFER_TIME_HIGH);
    check_stall_time = 0.0;
    drip_completion = drip;

    try
    {
        move(newpos, speed);
    }
    catch (const elegoo::common::CommandError &e)
    {
        reactor->update_timer(flush_timer, reactor->NOW);
        flush_step_generation();
        throw;
    }
    // Transmit move in "drip" mode
    try
    {
        lookahead->flush(false);
    }
    catch (const elegoo::common::DripModeEndSignalError &e)
    {
        // Assuming DripModeEndSignal is a custom exception
        lookahead->reset();
        trapq_finalize_moves(tra.get(), reactor->NEVER, 0);
    }

    // Exit "Drip" state
    reactor->update_timer(flush_timer, reactor->NOW);
    flush_step_generation();
}

std::pair<bool, std::string> ToolHead::stats(double eventtime)
{
    double max_queue_time = std::max(print_time, last_flush_time);
    for (auto m : all_mcus)
    {
        m->check_active(max_queue_time, eventtime);
    }
    double est_print_time = mcu->estimated_print_time(eventtime);
    clear_history_time = est_print_time - MOVE_HISTORY_EXPIRE;
    // std::cout << "clear_history_time: " << clear_history_time << std::endl;
    double buffer_time = print_time - est_print_time;
    double is_active = (buffer_time > -60.0) || (special_queuing_state.empty());
    if (special_queuing_state == "Drip")
    {
        buffer_time = 0.0;
    }

    std::ostringstream oss;
    oss << "print_time=" << print_time
        << " buffer_time=" << std::max(buffer_time, 0.0)
        << " print_stall=" << print_stall;

    return std::make_pair(is_active, oss.str());
}

Busy ToolHead::check_busy(double eventtime)
{
    double est_print_time = mcu->estimated_print_time(eventtime);
    bool lookahead_empty = lookahead->queue.empty();
    Busy busy;
    busy.est_print_time = est_print_time;
    busy.print_time = print_time;
    busy.lookahead_empty = lookahead_empty;
    return busy;
}

json ToolHead::get_status(double eventtime)
{
    double print_time = this->print_time;
    double estimated_print_time = mcu->estimated_print_time(eventtime);
    json res = kin->get_status(eventtime);
    res["print_time"] = print_time;
    res["stalls"] = print_stall;
    res["estimated_print_time"] = estimated_print_time;
    res["extruder"] = extruder->get_name();
    res["position"] = commanded_pos;
    res["max_velocity"] = max_velocity;
    res["max_accel"] = max_accel;
    res["minimum_cruise_ratio"] = min_cruise_ratio;
    res["square_corner_velocity"] = square_corner_velocity;
    return res;
}

void ToolHead::register_step_generator(std::function<void(double)> handler)
{
    step_generators.emplace_back(handler);
}

std::string ToolHead::get_extruder_name()
{
    return extruder->get_name();
}

void ToolHead::_handle_shutdown()
{
    can_pause = false;
    lookahead.reset();
}

std::shared_ptr<Kinematics> ToolHead::get_kinematic()
{
    return kin;
}

std::shared_ptr<trapq> ToolHead::get_trapq()
{
    return tra;
}

void ToolHead::note_step_generation_scan_time(double delay, double old_delay)
{
    flush_step_generation();
    // Remove old_delay if it's present
    if (old_delay != 0.0)
    {
        auto it = std::find(kin_flush_times.begin(), kin_flush_times.end(), old_delay);
        if (it != kin_flush_times.end())
        {
            kin_flush_times.erase(it);
        }
    }
    // Add new delay if it's non-zero
    if (delay != 0.0)
    {
        kin_flush_times.push_back(delay);
    }
    // Calculate the new delay
    double new_delay = *std::max_element(kin_flush_times.begin(), kin_flush_times.end());
    new_delay = std::max(new_delay, SDS_CHECK_TIME);
    kin_flush_delay = new_delay;
}

void ToolHead::register_lookahead_callback(std::function<void(double)> callback)
{
    Move *last_move = lookahead->get_last(); // Get the last move
    if (last_move == nullptr)
    {
        callback(get_last_move_time());
        return;
    }
    last_move->timing_callbacks.push_back(callback); // Append the callback
}

void ToolHead::note_mcu_movequeue_activity(double mq_time, bool set_step_gen_time)
{
    need_flush_time = std::max(need_flush_time, mq_time);

    if (set_step_gen_time)
    {
        step_gen_time = std::max(step_gen_time, mq_time);
    }

    if (do_kick_flush_timer)
    {
        do_kick_flush_timer = false;
        reactor->update_timer(flush_timer, reactor->NOW); // Assuming NOW is a static member of Reactor
    }
}

std::pair<double, double> ToolHead::get_max_velocity()
{
    return {max_velocity, max_accel};
}

void ToolHead::_calc_junction_deviation()
{
    double scv2 = square_corner_velocity * square_corner_velocity;
    junction_deviation = scv2 * (std::sqrt(2.0) - 1.0) / max_accel;
    max_accel_to_decel = max_accel * (1.0 - min_cruise_ratio);
}

void ToolHead::cmd_G4(std::shared_ptr<GCodeCommand> gcmd)
{
    // Dwell
    double delay = gcmd->get_double("P", 0., 0.) / 1000.0;
    dwell(delay);
}

void ToolHead::cmd_M400(std::shared_ptr<GCodeCommand> gcmd)
{
    // Wait for current moves to finish
    // SPDLOG_INFO("cmd_M400 #1");
    wait_moves();
    // SPDLOG_INFO("cmd_M400 #2");
}

#define INVALID_DOUBLE 9999.99
void ToolHead::cmd_SET_VELOCITY_LIMIT(std::shared_ptr<GCodeCommand> gcmd)
{
    // TODO...
    double max_velocity_new = gcmd->get_double("VELOCITY", DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, 0);
    double max_accel_new = gcmd->get_double("ACCEL", DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, 0);
    double square_corner_velocity_new = gcmd->get_double("SQUARE_CORNER_VELOCITY", DOUBLE_NONE, 0);
    double min_cruise_ratio_new = gcmd->get_double("MINIMUM_CRUISE_RATIO", DOUBLE_NONE, 0, DOUBLE_NONE, DOUBLE_NONE, 1);

    if (std::isnan(min_cruise_ratio_new))
    {
        double req_accel_to_decel = gcmd->get_double("ACCEL_TO_DECEL", DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, 0);
        if (!std::isnan(req_accel_to_decel) && !std::isnan(max_accel_new))
        {
            min_cruise_ratio_new = 1.0 - std::min(1.0, req_accel_to_decel / max_accel_new);
        }
        else if (!std::isnan(req_accel_to_decel) && std::isnan(max_accel_new))
        {
            min_cruise_ratio_new = 1.0 - std::min(1.0, (req_accel_to_decel / max_accel));
        }
    }

    if (!std::isnan(max_velocity_new))
    {
        max_velocity = max_velocity_new;
    }
    if (!std::isnan(max_accel_new))
    {
        max_accel = max_accel_new;
    }
    if (!std::isnan(square_corner_velocity_new))
    {
        square_corner_velocity = square_corner_velocity_new;
    }
    if (!std::isnan(min_cruise_ratio_new))
    {
        min_cruise_ratio = min_cruise_ratio_new;
    }

    _calc_junction_deviation();

    std::string msg = "max_velocity: " + std::to_string(max_velocity) + "\n" +
                      "max_accel: " + std::to_string(max_accel) + "\n" +
                      "minimum_cruise_ratio: " + std::to_string(min_cruise_ratio) + "\n" +
                      "square_corner_velocity: " + std::to_string(square_corner_velocity);

    printer->set_rollover_info("toolhead", "toolhead: " + msg);

    if (!max_velocity_new && !max_accel_new && !square_corner_velocity_new && !min_cruise_ratio_new)
    {
        gcmd->respond_info(msg, false);
    }
}

void ToolHead::cmd_M204(std::shared_ptr<GCodeCommand> gcmd)
{
    // Use S for accel
    double accel = gcmd->get_double("S", DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, 0);

    if (std::isnan(accel))
    {
        // Use minimum of P and T for accel
        double p = gcmd->get_double("P", DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, 0);
        double t = gcmd->get_double("T", DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, 0);

        if (std::isnan(p) || std::isnan(t))
        {
            gcmd->respond_info("Invalid M204 command \"" + gcmd->get_commandline() + "\"", false);
            return;
        }
        accel = std::min(p, t);
    }

    max_accel = accel;
    _calc_junction_deviation();
}

LookAheadQueue::LookAheadQueue(std::shared_ptr<ToolHead> toolhead)
    : toolhead(toolhead)
{
    junction_flush = LOOKAHEAD_FLUSH_TIME;
}

LookAheadQueue::~LookAheadQueue()
{
}
void LookAheadQueue::reset()
{
    for (auto it = queue.begin(); it != queue.end(); ++it)
        delete *it;
    queue.clear();
    junction_flush = LOOKAHEAD_FLUSH_TIME;
}

void LookAheadQueue::set_flush_time(double flush_time)
{
    junction_flush = flush_time;
}

Move *LookAheadQueue::get_last()
{
    if (!queue.empty())
    {
        return queue.back();
    }
    return nullptr;
}

void LookAheadQueue::flush(bool lazy)
{
    junction_flush = LOOKAHEAD_FLUSH_TIME;
    bool update_flush_count = lazy;
    size_t flush_count = queue.size();

    std::vector<std::tuple<Move *, double, double>> delayed;
    double next_end_v2 = 0.0, next_smoothed_v2 = 0.0, peak_cruise_v2 = 0.0;

    for (int i = flush_count - 1; i >= 0; --i)
    {
        Move *move = queue[i];
        double reachable_start_v2 = next_end_v2 + move->delta_v2;
        double start_v2 = std::min(move->max_start_v2, reachable_start_v2);
        double reachable_smoothed_v2 = next_smoothed_v2 + move->smooth_delta_v2;
        double smoothed_v2 = std::min(move->max_smoothed_v2, reachable_smoothed_v2);

        if (smoothed_v2 < reachable_smoothed_v2)
        {
            if (smoothed_v2 + move->smooth_delta_v2 > next_smoothed_v2 || !delayed.empty())
            {
                if (update_flush_count && peak_cruise_v2)
                {
                    flush_count = i;
                    update_flush_count = false;
                }
                peak_cruise_v2 = std::min(move->max_cruise_v2, (smoothed_v2 + reachable_smoothed_v2) * 0.5);
                if (!delayed.empty())
                {
                    if (!update_flush_count && i < flush_count)
                    {
                        double mc_v2 = peak_cruise_v2;
                        for (auto it = delayed.rbegin(); it != delayed.rend(); ++it)
                        {
                            mc_v2 = std::min(mc_v2, std::get<1>(*it));
                            std::get<0>(*it)->set_junction(std::min(std::get<1>(*it), mc_v2), mc_v2, std::min(std::get<2>(*it), mc_v2));
                        }
                    }
                    delayed.clear();
                }
            }
            if (!update_flush_count && i < flush_count)
            {
                double cruise_v2 = std::min({(start_v2 + reachable_start_v2) * 0.5, move->max_cruise_v2, peak_cruise_v2});
                move->set_junction(std::min(start_v2, cruise_v2), cruise_v2, std::min(next_end_v2, cruise_v2));
            }
        }
        else
        {
            delayed.emplace_back(move, start_v2, next_end_v2);
        }
        next_end_v2 = start_v2;
        next_smoothed_v2 = smoothed_v2;
    }

    if (update_flush_count || flush_count == 0)
    {
        return;
    }

    // SPDLOG_INFO("flush flush_count {}", flush_count);
    // try
    // {
    //    toolhead->_process_moves(std::vector<Move *>(queue.begin(), queue.begin() + flush_count));
    // }
    // catch(const elegoo::common::CommandError &e)
    // {
    //     for (auto it = queue.begin(); it != queue.begin() + flush_count; ++it){
    //         (*it)->printf_data();
    //     }
    //     SPDLOG_INFO("last_queue ===================================");
    //     for (auto it : last_queue) {
    //         it->printf_data();
    //     }
    //     throw;
    // }

    // for (Move* ptr : last_queue) {
    //     delete ptr;
    // }
    // last_queue.clear();

    // for (auto it = queue.begin(); it != queue.begin() + flush_count; ++it) {
    //     last_queue.push_back(new Move(*(*it)));  // 调用 Move 的拷贝构造函数
    // }

    toolhead->_process_moves(std::vector<Move *>(queue.begin(), queue.begin() + flush_count));
    for (auto it = queue.begin(); it != queue.begin() + flush_count; ++it)
        delete *it;
    queue.erase(queue.begin(), queue.begin() + flush_count);
}

void LookAheadQueue::add_move(Move *move)
{
    queue.push_back(move);
    if (queue.size() == 1)
        return;
    move->calc_junction(queue[queue.size() - 2]);
    junction_flush -= move->min_move_t;

    if (junction_flush <= 0.0)
    {
        // double monotime = get_monotonic();
        // SPDLOG_INFO("__func__:{} queue.size(): {} junction_flush:{}", __func__, queue.size(), junction_flush);
        flush(true);
        // SPDLOG_INFO("__func__:{},__LINE__:{} monotime:{} get_monotonic - monotime:{}\n",__func__,__LINE__,monotime,get_monotonic() - monotime);
    }
}
