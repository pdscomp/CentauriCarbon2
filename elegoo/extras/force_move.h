/*****************************************************************************
 * @Author       : Jack
 * @Date         : 2024-11-11 15:38:41
 * @LastEditors  : Jack
 * @LastEditTime : 2024-11-26 14:43:19
 * @Description  : Utility for manually moving a stepper for diagnostic purposes
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>
#include "configfile.h"
#include "mcu.h"
#include "printer.h"
#include "stepper_enable.h"
#include "c_helper.h"

namespace elegoo
{
    namespace extras
    {
        class ForceMove
        {
        public:
            ForceMove(std::shared_ptr<ConfigWrapper> config);
            ~ForceMove();
            void register_stepper(std::shared_ptr<ConfigWrapper> config,
                                  std::shared_ptr<MCU_stepper> mcu_stepper);
            std::shared_ptr<MCU_stepper> lookup_stepper(const std::string &name);
            bool _force_enable(std::shared_ptr<MCU_stepper> mcu_stepper);
            void _restore_enable(std::shared_ptr<MCU_stepper> stepper, bool was_enable);
            void manual_move(std::shared_ptr<MCU_stepper> stepper, double dist,
                             double speed, double accel = 0.0);
            std::shared_ptr<MCU_stepper> _lookup_stepper(
                std::shared_ptr<GCodeCommand> gcmd);
            void cmd_STEPPER_BUZZ(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_FORCE_MOVE(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_SET_KINEMATIC_POSITION(std::shared_ptr<GCodeCommand> gcmd);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<GCodeCommand> gcmd;
            std::map<std::string, std::shared_ptr<MCU_stepper>> steppers;
            std::shared_ptr<stepper_kinematics> _stepper_kinematics;
            trapq *_trapq;
            std::function<void(struct trapq *, double, double, double, double,
                               double, double, double,
                               double, double, double,
                               double, double, double)>
                _trapq_append;
            std::function<void(struct trapq *, double, double)> _trapq_finalize_move;
            std::vector<double> default_position;
        };

        std::shared_ptr<ForceMove> force_move_load_config(std::shared_ptr<ConfigWrapper> config);

    }
}