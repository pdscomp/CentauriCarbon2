/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:25:13
 * @Description  : Code for handling the kinematics of rotary delta robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include "kinematics.h"


class RotaryDeltaKinematics : public Kinematics
{
public:
    RotaryDeltaKinematics(std::shared_ptr<ToolHead> toolhead,
        std::shared_ptr<ConfigWrapper> config);
    ~RotaryDeltaKinematics();

    std::vector<std::shared_ptr<MCU_stepper>> get_steppers();
    std::vector<double> calc_position(
        const std::map<std::string, double>& stepper_positions);
    void set_position(std::vector<double> newpos, 
        const std::vector<int>& homing_axes);
    void home(std::shared_ptr<Homing> homing_state);
    void check_move(Move* move);
    json get_status(double eventtime);
    void get_calibration();

private:
    void motor_off();


};


class RotaryDeltaCalibration
{
public:
    RotaryDeltaCalibration();
    ~RotaryDeltaCalibration();

    void coordinate_descent_params();
    void new_calibration();
    void elbow_coord();
    void actuator_to_cartesian();
    void get_position_from_stable();
    void calc_stable_position();
    void save_state();
private:

};