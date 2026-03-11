/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:25:18
 * @Description  : Code for handling the kinematics of rotary delta robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "rotary_delta.h"

RotaryDeltaKinematics::RotaryDeltaKinematics(std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config)
{

}

RotaryDeltaKinematics::~RotaryDeltaKinematics()
{

}


std::vector<std::shared_ptr<MCU_stepper>> RotaryDeltaKinematics::get_steppers()
{
    return std::vector<std::shared_ptr<MCU_stepper>>();
}

std::vector<double> RotaryDeltaKinematics::calc_position(
    const std::map<std::string, double>& stepper_positions)
{
    return std::vector<double>();
}

void RotaryDeltaKinematics::set_position(std::vector<double> newpos, 
    const std::vector<int>& homing_axes)
{

}

void RotaryDeltaKinematics::home(std::shared_ptr<Homing> homing_state)
{

}

void RotaryDeltaKinematics::check_move(Move* move)
{

}

json RotaryDeltaKinematics::get_status(double eventtime)
{
    return json::object();
}

void RotaryDeltaKinematics::get_calibration()
{

}

void RotaryDeltaKinematics::motor_off()
{

}


RotaryDeltaCalibration::RotaryDeltaCalibration()
{

}

RotaryDeltaCalibration::~RotaryDeltaCalibration()
{

}

void RotaryDeltaCalibration::coordinate_descent_params()
{
    
}

void RotaryDeltaCalibration::new_calibration()
{
    
}

void RotaryDeltaCalibration::elbow_coord()
{
    
}

void RotaryDeltaCalibration::actuator_to_cartesian()
{
    
}

void RotaryDeltaCalibration::get_position_from_stable()
{
    
}

void RotaryDeltaCalibration::calc_stable_position()
{
    
}

void RotaryDeltaCalibration::save_state()
{
    
}