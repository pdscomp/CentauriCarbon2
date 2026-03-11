/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:26:05
 * @Description  : Code for handling the kinematics of corexz robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "corexz.h"


CoreXZKinematics::CoreXZKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{


}

CoreXZKinematics::~CoreXZKinematics()
{


}

std::vector<std::shared_ptr<MCU_stepper>> CoreXZKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> CoreXZKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void CoreXZKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{


}

void CoreXZKinematics::home(std::shared_ptr<Homing> homing_state)
{


}

void CoreXZKinematics::check_move( Move* move)
{


}

json CoreXZKinematics::get_status(double eventtime)
{
    return json::object();
}

void CoreXZKinematics::motor_off()
{


}

void CoreXZKinematics::check_endstops()
{


}
