/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:25:59
 * @Description  : Code for handling the kinematics of linear delta robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "delta.h"



DeltaKinematics::DeltaKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{

}

DeltaKinematics::~DeltaKinematics()
{

}


std::vector<std::shared_ptr<MCU_stepper>> DeltaKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> DeltaKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void DeltaKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{

}

void DeltaKinematics::home(std::shared_ptr<Homing> homing_state)
{

}

void DeltaKinematics::check_move( Move* move)
{

}

json DeltaKinematics::get_status(double eventtime)
{
    return json::object();
}

void DeltaKinematics::get_calibration()
{

}

void DeltaKinematics::actuator_to_cartesian()
{

}

void DeltaKinematics::motor_off()
{

}
