/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:25:25
 * @Description  : Code for handling the kinematics of polar robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "polar.h"


PolarKinematics::PolarKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{

}

PolarKinematics::~PolarKinematics()
{

}


std::vector<std::shared_ptr<MCU_stepper>> PolarKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> PolarKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void PolarKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{

}


void PolarKinematics::home(std::shared_ptr<Homing> homing_state)
{

}

void PolarKinematics::check_move(Move* move)
{

}

json PolarKinematics::get_status(double eventtime)
{
    return json::object();
}

void PolarKinematics::home_axis()
{

}

void PolarKinematics::motor_off()
{

}
