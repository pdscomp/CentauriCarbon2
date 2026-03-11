/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:25:49
 * @Description  : Code for handling the kinematics of hybrid-corexy robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "hybrid_corexy.h"


HybridCoreXYKinematics::HybridCoreXYKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{

}

HybridCoreXYKinematics::~HybridCoreXYKinematics()
{

}


std::vector<std::shared_ptr<MCU_stepper>> HybridCoreXYKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> HybridCoreXYKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void HybridCoreXYKinematics::update_limits()
{

}

void HybridCoreXYKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{

}

void HybridCoreXYKinematics::home_axis()
{

}

void HybridCoreXYKinematics::home(std::shared_ptr<Homing> homing_state)
{

}

void HybridCoreXYKinematics::check_move( Move* move)
{

}

json HybridCoreXYKinematics::get_status(double eventtime)
{
    return json::object();
}

void HybridCoreXYKinematics::motor_off()
{

}

void HybridCoreXYKinematics::check_endstops()
{

}
