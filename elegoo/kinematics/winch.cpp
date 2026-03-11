/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:19:37
 * @Description  : Code for handling the kinematics of cable winch robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "winch.h"

WinchKinematics::WinchKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{

}

WinchKinematics::~WinchKinematics()
{

}


std::vector<std::shared_ptr<MCU_stepper>> WinchKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> WinchKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void WinchKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{

}

void WinchKinematics::home(std::shared_ptr<Homing> homing_state)
{

}

void WinchKinematics::check_move(Move* move)
{

}

json WinchKinematics::get_status(double eventtime)
{
    return json::object();
}
