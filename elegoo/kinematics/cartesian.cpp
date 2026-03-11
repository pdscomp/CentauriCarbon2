/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:26:18
 * @Description  : Code for handling the kinematics of cartesian robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "cartesian.h"   
   
CartKinematics::CartKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{

}

CartKinematics::~CartKinematics()
{

}


std::vector<std::shared_ptr<MCU_stepper>> CartKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> CartKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void CartKinematics::update_limits()
{

}

void CartKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{

}

void CartKinematics::home_axis()
{

}

void CartKinematics::home(std::shared_ptr<Homing> homing_state)
{

}

void CartKinematics::check_move( Move* move)
{

}

json CartKinematics::get_status(double eventtime)
{
    return json::object();
}

void CartKinematics::motor_off()
{

}

void CartKinematics::check_endstops()
{

}
