/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-13 15:45:27
 * @Description  : Code for handling the kinematics of corexy robots
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include "kinematics.h"

class CoreXYKinematics : public Kinematics
{

public:
    CoreXYKinematics(std::shared_ptr<ToolHead> toolhead,
                     std::shared_ptr<ConfigWrapper> config);
    ~CoreXYKinematics();

    std::vector<std::shared_ptr<MCU_stepper>> get_steppers();
    std::vector<double> calc_position(
        const std::map<std::string, double> &stepper_positions);
    void set_position(std::vector<double> newpos, const std::vector<int> &homing_axes);
    void home(std::shared_ptr<Homing> homing_state);
    void check_move(Move *move);
    json get_status(double eventtime);
    std::vector<std::shared_ptr<PrinterRail>> get_rails() override;
    void clear_homing_state(const std::vector<int> &homing_axes) override;
    std::vector<std::vector<double>> get_rails_range() override
    {
        return {x_range, y_range, z_range};
    }
    double get_max_z_velocity() override { return max_z_velocity; }
    double get_max_z_accel() override { return max_z_accel; }

private:
    void check_endstops(Move *move);

private:
    std::vector<std::shared_ptr<PrinterRail>> rails;
    double max_z_velocity;
    double max_z_accel;
    std::vector<double> x_range;
    std::vector<double> y_range;
    std::vector<double> z_range;
    std::vector<std::pair<double, double>> limits;
    std::vector<double> axes_min;
    std::vector<double> axes_max;
};
