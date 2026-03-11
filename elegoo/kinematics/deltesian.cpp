/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:25:54
 * @Description  : Code for handling the kinematics of deltesian robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "deltesian.h"



DeltesianKinematics::DeltesianKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{

}

DeltesianKinematics::~DeltesianKinematics()
{

}


std::vector<std::shared_ptr<MCU_stepper>> DeltesianKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> DeltesianKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void DeltesianKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{

}

void DeltesianKinematics::home(std::shared_ptr<Homing> homing_state)
{

}

void DeltesianKinematics::check_move( Move* move)
{
    
}

json DeltesianKinematics::get_status(double eventtime)
{
    return json::object();
}

void DeltesianKinematics::actuator_to_cartesian()
{

}

void DeltesianKinematics::pillars_z_max()
{

}

void DeltesianKinematics::motor_off()
{

}
