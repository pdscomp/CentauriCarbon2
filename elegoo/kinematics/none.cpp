/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:25:33
 * @Description  : Dummy "none" kinematics support (for developer testing)
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "none.h"


NoneKinematics::NoneKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{

}

NoneKinematics::~NoneKinematics()
{

}


std::vector<std::shared_ptr<MCU_stepper>> NoneKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> NoneKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void NoneKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{

}

void NoneKinematics::home(std::shared_ptr<Homing> homing_state)
{

}

void NoneKinematics::check_move(Move* move)
{

}

json NoneKinematics::get_status(double eventtime)
{
    return json::object();
}
