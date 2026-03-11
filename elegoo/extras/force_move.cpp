/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-20 15:16:18
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-30 12:00:08
 * @Description  : Utility for manually moving a stepper for diagnostic purposes
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "force_move.h"
#include "kinematics_factory.h"
#include "logger.h"

namespace elegoo
{
    namespace extras
    {
        const double BUZZ_DISTANCE = 1.0;
        const double BUZZ_VELOCITY = BUZZ_DISTANCE / 0.250;
        const double BUZZ_RADIANS_DISTANCE = M_PI / 180.0;
        const double BUZZ_RADIANS_VELOCITY = BUZZ_RADIANS_DISTANCE / 0.250;
        const double STALL_TIME = 0.100;

        const std::string cmd_STEPPER_BUZZ_help =
            "Oscillate a given stepper to help id it";
        const std::string cmd_FORCE_MOVE_help =
            "Manually move a stepper; invalidates kinematics";
        const std::string cmd_SET_KINEMATIC_POSITION_help =
            "Force a low-level kinematic position";

        static std::tuple<double, double, double, double> calc_move_time(double dist, double speed, double accel)
        {
            double axis_r = 1.0;

            if (dist < 0.0)
            {
                axis_r = -1.0;
                dist = -dist;
            }
            if (accel == 0.0 || dist == 0.0)
            {
                return {axis_r, 0., dist / speed, speed};
            }
            double max_cruise_v2 = dist * accel;
            if (max_cruise_v2 < speed * speed)
                speed = std::sqrt(max_cruise_v2);

            double accel_t = speed / accel;
            double accel_decel_d = accel_t * speed;
            double cruise_t = (dist - accel_decel_d) / speed;
            return {axis_r, accel_t, cruise_t, speed};
        }

        ForceMove::ForceMove(std::shared_ptr<ConfigWrapper> config)
        {
            this->printer = config->get_printer();
            this->steppers.clear();
            _trapq = trapq_alloc();
            _trapq_append = trapq_append;
            _trapq_finalize_move = trapq_finalize_moves;
            _stepper_kinematics = std::shared_ptr<stepper_kinematics>(
                cartesian_stepper_alloc('x'),
                free);

            // 仅注册SET_KINEMATIC_POSITION命令,其他命令没有必要支持
            auto gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            // gcode->register_command("STEPPER_BUZZ", [this](std::shared_ptr<GCodeCommand> gcmd)
            //                         { cmd_STEPPER_BUZZ(gcmd); }, true, cmd_STEPPER_BUZZ_help);

            // if (config->getboolean("enable_force_move", BoolValue::BOOL_FALSE))
            {
                gcode->register_command("FORCE_MOVE", [this](std::shared_ptr<GCodeCommand> gcmd)
                                        { cmd_FORCE_MOVE(gcmd); }, true, cmd_FORCE_MOVE_help);

                gcode->register_command("SET_KINEMATIC_POSITION", [this](std::shared_ptr<GCodeCommand> gcmd)
                                        { cmd_SET_KINEMATIC_POSITION(gcmd); }, true, cmd_SET_KINEMATIC_POSITION_help);

                default_position = config->getdoublelist("default_position", {128, 128, 128});
                if (default_position.size() != 3)
                    throw elegoo::common::ConfigParserError("default_position must likes '128,128,128'");
            }

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this]()
                                      {
                                        auto gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
                                          // 设置默认机器位置,但是不要设置归零
                                          std::map<std::string, std::string> new_params;
                                          new_params["X"] = std::to_string(default_position[0]);
                                          new_params["Y"] = std::to_string(default_position[1]);
                                          new_params["Z"] = std::to_string(default_position[2]);
                                          new_params["CLEAR"] = "XYZ";
                                          std::shared_ptr<GCodeCommand> gcmd = gcode->create_gcode_command("SET_KINEMATIC_POSITION", "SET_KINEMATIC_POSITION", new_params);
                                          cmd_SET_KINEMATIC_POSITION(gcmd); }));
        }

        ForceMove::~ForceMove() {}

        void ForceMove::register_stepper(std::shared_ptr<ConfigWrapper> config,
                                         std::shared_ptr<MCU_stepper> mcu_stepper)
        {
            this->steppers[mcu_stepper->get_name()] = mcu_stepper;
        }

        std::shared_ptr<MCU_stepper> ForceMove::lookup_stepper(
            const std::string &name)
        {
            if (steppers.find(name) == steppers.end())
                throw elegoo::common::CommandError("Unknown stepper " + (name));
            return this->steppers[name];
        }

        bool ForceMove::_force_enable(std::shared_ptr<MCU_stepper> stepper)
        {
            auto toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            double print_time = toolhead->get_last_move_time();
            auto stepper_enable = any_cast<std::shared_ptr<PrinterStepperEnable>>(printer->lookup_object("stepper_enable"));
            auto enable = stepper_enable->lookup_enable(stepper->get_name());
            bool was_enable = enable->is_motor_enabled();
            if (!was_enable)
            {
                enable->motor_enable(print_time);
                toolhead->dwell(STALL_TIME);
            }
            return was_enable;
        }

        void ForceMove::_restore_enable(std::shared_ptr<MCU_stepper> stepper,
                                        bool was_enable)
        {
            if (!was_enable)
            {
                auto toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
                toolhead->dwell(STALL_TIME);
                double print_time = toolhead->get_last_move_time();
                auto stepper_enable = any_cast<std::shared_ptr<PrinterStepperEnable>>(printer->lookup_object("stepper_enable"));
                auto enable = stepper_enable->lookup_enable(stepper->get_name());
                enable->motor_disable(print_time);
                toolhead->dwell(STALL_TIME);
            }
        }

        void ForceMove::manual_move(std::shared_ptr<MCU_stepper> stepper, double dist,
                                    double speed, double accel)
        {
            auto toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            toolhead->flush_step_generation();
            std::shared_ptr<stepper_kinematics> prev_sk = stepper->set_stepper_kinematics(_stepper_kinematics);
            trapq *prev_trapq = stepper->set_trapq(this->_trapq);
            stepper->set_position({0.0, 0.0, 0.0});
            double axis_r, accel_t, cruise_t, cruise_v;
            std::tie(axis_r, accel_t, cruise_t, cruise_v) = calc_move_time(dist, speed, accel);
            double print_time = toolhead->get_last_move_time();
            this->_trapq_append(this->_trapq, print_time, accel_t, cruise_t, accel_t,
                                0., 0., 0., axis_r, 0., 0., 0., cruise_v, accel);
            print_time = print_time + accel_t + cruise_t + accel_t;
            stepper->generate_steps(print_time);
            this->_trapq_finalize_move(this->_trapq, print_time + 99999.9,
                                       print_time + 99999.9);
            stepper->set_trapq(prev_trapq);
            stepper->set_stepper_kinematics(prev_sk);
            toolhead->note_mcu_movequeue_activity(print_time);
            toolhead->dwell(accel_t + cruise_t + accel_t);
            toolhead->flush_step_generation();
        }

        std::shared_ptr<MCU_stepper> ForceMove::_lookup_stepper(
            std::shared_ptr<GCodeCommand> gcmd)
        {
            auto name = gcmd->get("STEPPER");
            if (steppers.find(name) == steppers.end())
                throw elegoo::common::CommandError("Unknown stepper " + (name));
            return this->steppers[name];
        }

        void ForceMove::cmd_STEPPER_BUZZ(std::shared_ptr<GCodeCommand> gcmd)
        {
            std::shared_ptr<MCU_stepper> stepper = this->_lookup_stepper(gcmd);
            SPDLOG_INFO("Stepper buzz {}", stepper->get_name());
            bool was_enable = this->_force_enable(stepper);
            auto toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            double dist = BUZZ_DISTANCE;
            double speed = BUZZ_VELOCITY;
            if (stepper->units_in_radians())
            {
                dist = BUZZ_DISTANCE;
                speed = BUZZ_VELOCITY;
            }

            for (int i = 0; i < 10; i++)
            {
                this->manual_move(stepper, dist, speed);
                toolhead->dwell(0.050);
                this->manual_move(stepper, -dist, speed);
                toolhead->dwell(0.450);
            }
            this->_restore_enable(stepper, was_enable);
        }

        void ForceMove::cmd_FORCE_MOVE(std::shared_ptr<GCodeCommand> gcmd)
        {
            std::shared_ptr<MCU_stepper> stepper = this->_lookup_stepper(gcmd);
            float distance = gcmd->get_double("DISTANCE");
            float speed = gcmd->get_double("VELOCITY", DOUBLE_INVALID, DOUBLE_NONE, DOUBLE_NONE, 0);
            float accel = gcmd->get_double("ACCEL", 0, 0);
            SPDLOG_INFO("FORCE_MOVE {} distance={} velocity={} accel={}", stepper->get_name(), distance, speed, accel);
            this->_force_enable(stepper);
            this->manual_move(stepper, distance, speed, accel);
        }

        void ForceMove::cmd_SET_KINEMATIC_POSITION(std::shared_ptr<GCodeCommand> gcmd)
        {
            auto toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            toolhead->get_last_move_time();
            auto curpos = toolhead->get_position();
            auto x = gcmd->get_double("X", curpos[0]);
            auto y = gcmd->get_double("Y", curpos[1]);
            auto z = gcmd->get_double("Z", curpos[2]);
            auto clear = gcmd->get("CLEAR", "");
            std::vector<int> clear_axes;
            for (auto ch : clear)
            {
                if (ch == 'x' || ch == 'X')
                    clear_axes.push_back(0);
                else if (ch == 'y' || ch == 'Y')
                    clear_axes.push_back(1);
                else if (ch == 'z' || ch == 'Z')
                    clear_axes.push_back(2);
            }
            SPDLOG_INFO("SET_KINEMATIC_POSITION pos={}, {}, {} clear={}", x, y, z, clear);
            toolhead->set_position({x, y, z, curpos[3]}, {0, 1, 2});
            toolhead->get_kinematic()->clear_homing_state(clear_axes);
        }

        std::shared_ptr<ForceMove> force_move_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<ForceMove>(config);
        }

    }
}