/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-03-23 16:56:10
 * @Description  : Kinematic input shaper to minimize motion vibrations in XY plane
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#ifndef __INPUT_SHAPER_H__
#define __INPUT_SHAPER_H__

#pragma once

#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <functional>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "printer.h"
#include "configfile.h"
#include "toolhead.h"

namespace elegoo 
{
    namespace extras 
    {
        class InputShaperParams
        {
        public:
            InputShaperParams(std::string axis, std::shared_ptr<ConfigWrapper> config);
            ~InputShaperParams();
            void update(std::shared_ptr<GCodeCommand> gcmd);
            std::tuple<int32_t, std::vector<double>, std::vector<double>> get_shaper();
            std::map<std::string,std::string> get_status();
        private:
            std::string axis;
            std::map<std::string,std::function<
                    std::pair<std::vector<double>, std::vector<double>>(double,double)>> shapers;
            std::string shaper_type;
            double shaper_freq;
            double damping_ratio;
        };

        class AxisInputShaper
        {
        public:
            AxisInputShaper(std::string axis, std::shared_ptr<ConfigWrapper> config);
            ~AxisInputShaper();
            std::string get_name();
            std::tuple<int32_t, std::vector<double>, std::vector<double>> get_shaper();
            void update(std::shared_ptr<GCodeCommand> gcmd);
            bool set_shaper_kinematics(std::shared_ptr<stepper_kinematics> sk);
            void disable_shaping();
            void enable_shaping();
            void report(std::shared_ptr<GCodeCommand>);
            std::map<std::string,std::string> get_shaping_params();
        private:
            std::string axis;
            std::shared_ptr<InputShaperParams> params;
            std::tuple<int32_t, std::vector<double>, std::vector<double>> saved;
            int32_t n;
            std::vector<double> A;
            std::vector<double> T;

        };

        class InputShaper
        {
        public:
            InputShaper(std::shared_ptr<ConfigWrapper> config);
            ~InputShaper();
            std::vector<std::shared_ptr<AxisInputShaper>> get_shapers();
            void connect();
            void disable_shaping();
            void enable_shaping();
            void cmd_SET_INPUT_SHAPER(std::shared_ptr<GCodeCommand> gcmd);

        private:
            std::shared_ptr<stepper_kinematics> _get_input_shaper_stepper_kinematics(std::shared_ptr<MCU_stepper> stepper);
            void _update_input_shaping();
        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<ToolHead> toolhead;
            std::vector<std::shared_ptr<AxisInputShaper>> shapers;
            std::vector<std::shared_ptr<stepper_kinematics>> input_shaper_stepper_kinematics;
            std::vector<std::shared_ptr<stepper_kinematics>> orig_stepper_kinematics;
        };

        std::shared_ptr<InputShaper> input_shaper_load_config(std::shared_ptr<ConfigWrapper> config);
    }
}

#endif