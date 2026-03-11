/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:25:44
 * @Description  : Code for handling the kinematics of hybrid-corexz robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "hybrid_corexz.h"


HybridCoreXZKinematics::HybridCoreXZKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{

}

HybridCoreXZKinematics::~HybridCoreXZKinematics()
{

}


std::vector<std::shared_ptr<MCU_stepper>> HybridCoreXZKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> HybridCoreXZKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void HybridCoreXZKinematics::update_limits()
{

}

void HybridCoreXZKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{

}

void HybridCoreXZKinematics::home_axis()
{

}

void HybridCoreXZKinematics::home(std::shared_ptr<Homing> homing_state)
{

}

void HybridCoreXZKinematics::check_move( Move* move)
{

}

json HybridCoreXZKinematics::get_status(double eventtime)
{
    return json::object();
}

void HybridCoreXZKinematics::motor_off()
{

}

void HybridCoreXZKinematics::check_endstops()
{

}
