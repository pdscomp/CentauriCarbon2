/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : coconut
 * @LastEditTime : 2025-04-01 19:25:40
 * @Description  : Code for handling the kinematics of hybrid-corexz robots
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include "kinematics.h"

class HybridCoreXZKinematics : public Kinematics
{
public:
    HybridCoreXZKinematics(std::shared_ptr<ToolHead> toolhead,
        std::shared_ptr<ConfigWrapper> config);
    ~HybridCoreXZKinematics();

    std::vector<std::shared_ptr<MCU_stepper>> get_steppers();
    std::vector<double> calc_position(
        const std::map<std::string, double>& stepper_positions);
    void update_limits();
    void set_position(std::vector<double> newpos, 
        const std::vector<int>& homing_axes);
    void home_axis();
    void home(std::shared_ptr<Homing> homing_state);
    void check_move( Move* move);
    json get_status(double eventtime);

private:
    void motor_off();
    void check_endstops();

};