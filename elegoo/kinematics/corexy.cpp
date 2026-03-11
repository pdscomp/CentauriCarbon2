/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-19 20:19:44
 * @Description  : Code for handling the kinematics of corexy robots
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "corexy.h"

CoreXYKinematics::CoreXYKinematics(std::shared_ptr<ToolHead> toolhead,
                                   std::shared_ptr<ConfigWrapper> config)
{
    // 创建轴
    std::vector<std::string> axis_names = {"x", "y", "z"};
    for (const std::string &n : axis_names)
    {
        std::shared_ptr<ConfigWrapper> section = config->getsection("stepper_" + n);
        rails.push_back(LookupMultiRail(section));
    }

    for (std::shared_ptr<MCU_stepper> s : rails[1]->get_steppers())
    {
        rails[0]->get_endstops()[0].first->add_stepper(s);
    }
    for (std::shared_ptr<MCU_stepper> s : rails[0]->get_steppers())
    {
        rails[1]->get_endstops()[0].first->add_stepper(s);
    }

    rails[0]->setup_itersolve(corexy_stepper_alloc, '+');
    rails[1]->setup_itersolve(corexy_stepper_alloc, '-');
    rails[2]->setup_itersolve(cartesian_stepper_alloc, 'z');

    for (std::shared_ptr<MCU_stepper> s : get_steppers())
    {
        s->set_trapq(toolhead->get_trapq().get());
        toolhead->register_step_generator([s](double flush_time)
                                          { s->generate_steps(flush_time); });
    }

    double max_velocity, max_accel;
    std::tie(max_velocity, max_accel) = toolhead->get_max_velocity();
    max_z_velocity = config->getdouble("max_z_velocity",
                                       max_velocity, DOUBLE_NONE, max_velocity, 0.);
    max_z_accel = config->getdouble("max_z_accel",
                                    max_accel, DOUBLE_NONE, max_accel, 0.);
    // 获取手动移动的行程范围
    x_range = config->getdoublelist("x_range", {0., 256.});
    y_range = config->getdoublelist("y_range", {0., 256.});
    z_range = config->getdoublelist("z_range", {0., 256.});

    limits = {{1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, -1.0f}};

    std::vector<std::pair<double, double>> ranges;
    for (std::shared_ptr<PrinterRail> rail : rails)
    {
        ranges.push_back(rail->get_range());
    }
    axes_min = {ranges[0].first, ranges[1].first, ranges[2].first, 0.0f};
    axes_max = {ranges[0].second, ranges[1].second, ranges[2].second, 0.0f};
}

CoreXYKinematics::~CoreXYKinematics()
{
}

std::vector<std::shared_ptr<MCU_stepper>> CoreXYKinematics::get_steppers()
{
    std::vector<std::shared_ptr<MCU_stepper>> all_steppers;
    for (std::shared_ptr<PrinterRail> rail : rails)
    {
        std::vector<std::shared_ptr<MCU_stepper>> rail_steppers = rail->get_steppers();
        all_steppers.insert(all_steppers.end(),
                            rail_steppers.begin(), rail_steppers.end());
    }
    return all_steppers;
}

std::vector<double> CoreXYKinematics::calc_position(
    const std::map<std::string, double> &stepper_positions)
{
    std::vector<double> pos;
    for (std::shared_ptr<PrinterRail> rail : rails)
    {
        pos.push_back(stepper_positions.at(rail->get_name(false)));
    }
    // <x,y,z>
    return {0.5f * (pos[0] + pos[1]), 0.5f * (pos[0] - pos[1]), pos[2]};
}

void CoreXYKinematics::set_position(std::vector<double> newpos,
                                    const std::vector<int> &homing_axes)
{
    for (int i = 0; i < rails.size(); ++i)
    {
        rails[i]->set_position(newpos);
        if (std::find(homing_axes.begin(), homing_axes.end(), i) != homing_axes.end())
        {
            limits[i] = rails[i]->get_range();
        }
    }
}

void CoreXYKinematics::home(std::shared_ptr<Homing> homing_state)
{
    // 逐轴归零
    for (int axis : homing_state->get_axes())
    {
        std::shared_ptr<PrinterRail> rail = rails[axis];
        double position_min, position_max;
        std::tie(position_min, position_max) = rail->get_range();
        HomingInfo hi = rail->get_homing_info();

        // 计算归零目标位置，假设当前处于远离限位的位置forcepos，向限位位置homepos移动
        std::vector<double> homepos = {DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE};
        homepos[axis] = hi.position_endstop;
        std::vector<double> forcepos = homepos;
        if (hi.positive_dir)
            forcepos[axis] -= 1.5f * (hi.position_endstop - position_min);
        else
            forcepos[axis] += 1.5f * (position_max - hi.position_endstop);
        homing_state->home_rails({rail}, forcepos, homepos);
    }
}

void CoreXYKinematics::check_move(Move *move)
{
    double xpos = move->end_pos[0];
    double ypos = move->end_pos[1];
    // 默认取消软限位
    // if (xpos < limits[0].first || xpos > limits[0].second ||
    //     ypos < limits[1].first || ypos > limits[1].second)
    // {
    //     check_endstops(move);
    // }
    // 如果Z轴没有移动，则返回
    if (move->axes_d[2] == 0.0f)
        return;
    // 如果Z轴有移动，计算速度和加速度比例，并更新限制
    // check_endstops(move);
    double z_ratio = move->move_d / std::abs(move->axes_d[2]);
    move->limit_speed(max_z_velocity * z_ratio, max_z_accel * z_ratio);
}

std::vector<std::shared_ptr<PrinterRail>> CoreXYKinematics::get_rails()
{
    return rails;
}

void CoreXYKinematics::clear_homing_state(const std::vector<int> &clear_axes)
{
    for (auto axis : clear_axes)
    {
        if (axis >= 0 && axis < 3)
        {
            limits[axis].first = 1.;
            limits[axis].second = -1.;
        }
    }
}

json CoreXYKinematics::get_status(double eventtime)
{
    // 增加真归零状态
    std::string homed_axes;
    const std::string axis_names = "xyz";
    for (size_t i = 0; i < limits.size(); ++i)
    {
        double min_limit = limits[i].first;
        double max_limit = limits[i].second;
        if (min_limit <= max_limit)
            homed_axes += axis_names[i];
    }

    json status;
    status["homed_axes"] = homed_axes;
    json axis_min = json::array();
    json axis_max = json::array();
    status["axis_minimum"] = axes_min;
    status["axis_maximum"] = axes_max;
    return status;
}

void CoreXYKinematics::check_endstops(Move *move)
{
    const auto &end_pos = move->end_pos; // 获取移动的终点位置
    for (int i = 0; i < 3; ++i)
    {
        if (move->axes_d[i] &&
            (end_pos[i] < limits[i].first || end_pos[i] > limits[i].second))
        {
            if (limits[i].first > limits[i].second)
                throw move->move_error("Must home axis first");
            throw move->move_error();
        }
    }
}