/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-26 20:24:10
 * @Description  : Run user defined actions in place of a normal G28 homing command
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "homing_override.h"
#include "printer.h"
#include "homing.h"
#include "tmc2209.h"
#include "toolhead.h"
#include "kinematics.h"
#include "probe.h"
namespace elegoo
{
    namespace extras
    {
        HomingOverride::HomingOverride(std::shared_ptr<ConfigWrapper> config)
            : in_script(false)
        {
            printer = config->get_printer();
            safe_home_temp = config->getdouble("safe_home_temp", 140.);
            start_pos = {config->getdouble("set_position_x", DOUBLE_NONE),
                         config->getdouble("set_position_y", DOUBLE_NONE),
                         config->getdouble("set_position_z", DOUBLE_NONE)};

            axes = to_upper(config->get("axes", "XYZ"));
            any_cast<std::shared_ptr<PrinterHoming>>(printer->load_object(config, "homing"));
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            prev_G28 = gcode->register_command("G28", nullptr);
            gcode->register_command("G28",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_G28(gcmd);
                                    });

            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:ready",
                std::function<void()>([this]()
                                      { handle_ready(); }));
            // 获取XYZ参数
            for (int i = 0; i < 3; i++)
            {
                std::string prefix(1, "xyz"[i]);
                prefix += "_";
                hops[i]["ready_start_pos"] = config->getdouble(prefix + "ready_start_pos", hops[i]["ready_retract_pos"]);
                hops[i]["ready_retract_pos"] = config->getdouble(prefix + "ready_retract_pos", 25.0);
                hops[i]["ready_end_pos"] = config->getdouble(prefix + "ready_end_pos", DOUBLE_NONE);
                hops[i]["ready_speed"] = config->getdouble(prefix + "ready_speed", 10.0);
                // if (i < 2)
                {
                    hops[i]["homing_speed"] = config->getdouble(prefix + "homing_speed", 10.0);
                    hops[i]["second_homing_speed"] = config->getdouble(prefix + "second_homing_speed", 10.0);
                    hops[i]["retract_dist"] = config->getdouble(prefix + "retract_dist", 10.0);
                    hops[i]["retract_speed"] = config->getdouble(prefix + "retract_speed", 10.0);
                    hops[i]["second_retract_dist"] = config->getdouble(prefix + "second_retract_dist", 10.0);
                    hops[i]["second_retract_speed"] = config->getdouble(prefix + "second_retract_speed", 10.0);
                }
            }
            
            home_x_pos_probe = config->getdouble("home_x_pos");
            home_y_pos_probe = config->getdouble("home_y_pos");
            home_xy_speed_probe = config->getdouble("home_xy_speed", 10.0);
            home_x_pos2_probe = config->getdouble("home_x_pos2", DOUBLE_NONE);
            home_y_pos2_probe = config->getdouble("home_y_pos2", DOUBLE_NONE);
            home_xy_speed2_probe = config->getdouble("home_xy_speed2", DOUBLE_NONE);

            home_x_pos_endstop = config->getdouble("home_x_pos_endstop");
            home_y_pos_endstop = config->getdouble("home_y_pos_endstop");
            home_xy_speed_endstop = config->getdouble("home_xy_speed_endstop", 10.0);
            home_x_pos2_endstop = config->getdouble("home_x_pos2_endstop", DOUBLE_NONE);
            home_y_pos2_endstop = config->getdouble("home_y_pos2_endstop", DOUBLE_NONE);
            home_xy_speed2_endstop = config->getdouble("home_xy_speed2_endstop", DOUBLE_NONE);

            homing_accel = config->getdouble("homing_accel", 5000.0);
            homing_current_decay = config->getdouble("homing_current_decay", 1.0);
            z_retract_pos = config->getdouble("z_retract_pos", 10.0);
            z_retract_speed = config->getdouble("z_retract_speed", 10.0);

            z_samples = config->getint("z_samples", 5);
            z_samples_tolerance = config->getdouble("z_samples_tolerance", 0.01);
        }

        HomingOverride::~HomingOverride()
        {
        }

        void HomingOverride::handle_ready()
        {
            toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
        }

        void HomingOverride::single_home(int axis, double homing_speed)
        {
            // 设置归零速度
            std::vector<std::shared_ptr<PrinterRail>> rails = toolhead->get_kinematic()->get_rails();
            double prev_speed = rails[axis]->set_homing_speed(homing_speed);
            // 执行归零
            std::map<std::string, std::string> new_params;
            new_params[std::string(1, "XYZ"[axis])] = "0";
            std::shared_ptr<GCodeCommand> g28_gcmd = gcode->create_gcode_command("G28", "G28", new_params);
            try
            {
                this->prev_G28(g28_gcmd);
            }
            catch (...)
            {
                rails[axis]->set_homing_speed(prev_speed);
                throw;
            }
            rails[axis]->set_homing_speed(prev_speed);
        }

        void HomingOverride::single_retract(int axis, double retract_dist, double retract_speed, bool wait)
        {
            // 移开并等待
            auto pos = toolhead->get_position();
            pos[axis] += retract_dist;
            toolhead->manual_move(pos, retract_speed);
            if (wait)
                toolhead->wait_moves();
        }

        int HomingOverride::is_probe()
        {
            std::vector<std::shared_ptr<PrinterRail>> rails = toolhead->get_kinematic()->get_rails();
            std::shared_ptr<PrinterRail> rail = rails[2];
            return rail->is_probe();
        }

        void HomingOverride::homing_ready(int axis, bool homed)
        {
            auto pos = toolhead->get_position();
            bool endstop_state = 0;

            // 20250927，新增逻辑Z轴如果限位触发则下降，否则直接抬升
            if (axis == 2)
            {
                SPDLOG_INFO("homing_ready axis z");
                std::vector<std::shared_ptr<PrinterRail>> rails = toolhead->get_kinematic()->get_rails();
                std::shared_ptr<PrinterRail> rail = rails[2];
                std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> endstops;
                std::shared_ptr<MCU_pins> endstop;
                std::string name;
                endstops = rail->get_endstops();
                std::tie(endstop, name) = endstops[0];
                endstop_state = endstop->query_endstop(toolhead->get_last_move_time());
                SPDLOG_INFO("homing_ready #2: endstop name {} state {}", name, endstop_state);
            }

            if ((axis < 2 && pos[axis] < hops[axis]["ready_start_pos"]) || (axis == 2 && endstop_state))
            {
                std::vector<double> coord = {DOUBLE_NONE, DOUBLE_NONE, DOUBLE_NONE};
                coord[axis] = hops[axis]["ready_retract_pos"];
                toolhead->manual_move(coord, hops[axis]["ready_speed"]);
                if (!std::isnan(hops[axis]["ready_end_pos"]))
                {
                    coord[axis] = hops[axis]["ready_end_pos"];
                    toolhead->manual_move(coord, hops[axis]["ready_speed"]);
                }
            }
            toolhead->wait_moves();
        }

        void HomingOverride::homing_set_run_current(int axis, double current)
        {
            // 设置电流
            char cmdline[256];
            snprintf(cmdline, sizeof(cmdline), "SET_TMC_CURRENT STEPPER=stepper_%c CURRENT=%f", "xyz"[axis], current);
            gcode->run_script_from_command(cmdline);
        }

        double HomingOverride::homing_get_run_current(int axis)
        {
            std::string prefix(1, "xyz"[axis]);
            // 获取驱动对象
            std::string drv_name = "tmc2209 stepper_" + prefix;
            std::shared_ptr<TMC2209> drv = any_cast<std::shared_ptr<TMC2209>>(printer->lookup_object(drv_name));
            if (drv == nullptr)
                return 0.0;
            json drv_status = drv->get_status();
            // 获取运行电流
            return drv_status["run_current"].get<double>();
        }

        void HomingOverride::homing_set_accel(uint32_t accel)
        {
            // 设置加速度
            char cmdline[256];
            snprintf(cmdline, sizeof(cmdline), "M204 S%u", accel);
            gcode->run_script_from_command(cmdline);
        }

        uint32_t HomingOverride::homing_get_accel()
        {
            json status = toolhead->get_status(get_monotonic());
            double accel = status["max_accel"].get<double>();
            return accel;
        }

        void HomingOverride::cmd_G28(std::shared_ptr<GCodeCommand> gcmd)
        {
            // 1. 获取需要归零的轴与归零状态
            bool need[3] = {gcmd->get("X", "None") != "None",
                            gcmd->get("Y", "None") != "None",
                            gcmd->get("Z", "None") != "None"};
            json kin_status = toolhead->get_kinematic()->get_status(get_monotonic());
            bool homed[3] = {!(kin_status["homed_axes"].get<std::string>().find('x') == std::string::npos),
                             !(kin_status["homed_axes"].get<std::string>().find('y') == std::string::npos),
                             !(kin_status["homed_axes"].get<std::string>().find('z') == std::string::npos)};
            SPDLOG_INFO("need {} {} {} homed {} {} {}", need[0], need[1], need[2], homed[0], homed[1], homed[2]);
            // 2. 判断是否为G28 X Y TRY, 如果是的话若X与Y都归零则返回,否则X与Y都进行归零
            bool home_try = gcmd->get("TRY", "None") != "None";
            if (home_try && need[0] && need[1])
                home_try = true;
            else
                home_try = false;
            if (home_try)
            {
                if (!homed[0] || !homed[1])
                    need[0] = need[1] = true;
                else
                    return;
            }

            json res;
            res["command"] = "M2202";
            res["result"] = "2801";
            gcmd->respond_feedback(res);

            // 安全归零温度
            std::shared_ptr<PrinterHeaters> pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));
            double cur_temp = pheaters->get_heaters()["extruder"]->get_temp(0).first;
            if (cur_temp < safe_home_temp)
            {
                SPDLOG_INFO("{} cur_temp:{},safe_home_temp:{}", __func__, cur_temp, safe_home_temp);
                gcode->run_script_from_command("M109 S" + std::to_string(safe_home_temp));
            }

            // 3. 如果是G28, 说明都是要归零
            if (!need[0] && !need[1] && !need[2])
                need[0] = need[1] = need[2] = true;

            // 4. 如果要进行Z，但是X或Y没有归零，需要先归零X或Y
            if (need[2])
            {
                if (!homed[0] || !homed[1])
                    need[0] = need[1] = true;
            }

            // 5. 执行Z轴准备动作
            homing_ready(2, homed[2]);

            // 6. 执行X/Y轴归零
            if (need[0] || need[1])
            {
                // 获取电机电流
                double run_current[2] = {
                    homing_get_run_current(0),
                    homing_get_run_current(1)};
                // 执行准备动作以及设置电流
                for (int axis = 0; axis < 2; axis++)
                    homing_set_run_current(axis, run_current[axis] * homing_current_decay);

                // 获取加速度
                uint32_t prev_accel = homing_get_accel();
                // 设置加速度
                if (need[0] || need[1])
                    homing_set_accel(homing_accel);

                for (int axis = 0; axis < 2; axis++)
                    homing_ready(axis, homed[axis]);

                // 执行XY归零
                int times = 0;
                try
                {
                    while (times < 2)
                    {
                        for (int axis = 0; axis < 2; axis++)
                        {
                            if (need[axis])
                            {
                                gcode->run_script_from_command("M114");
                                // 归零
                                single_home(axis, times == 0 ? hops[axis]["homing_speed"] : hops[axis]["second_homing_speed"]);
                                if (need[0] ^ need[1])
                                {
                                    single_retract(axis,
                                                   times == 0 ? hops[axis]["retract_dist"] : hops[axis]["second_retract_dist"],
                                                   times == 0 ? hops[axis]["retract_speed"] : hops[axis]["second_retract_speed"],
                                                   times == 0 ? true : false);
                                }
                                else
                                {
                                    single_retract(axis,
                                                   times == 0 ? hops[axis]["retract_dist"] : hops[axis]["second_retract_dist"],
                                                   times == 0 ? hops[axis]["retract_speed"] : hops[axis]["second_retract_speed"],
                                                   times == 0 ? true : (axis == 0 ? true : false));
                                }
                                gcode->run_script_from_command("M114");
                            }
                        }
                        times++;
                    }
                }
                catch (...)
                {
                    // 恢复电流
                    for (int axis = 0; axis < 2; axis++)
                    {
                        homing_set_run_current(axis, run_current[axis]);
                    }
                    // 恢复加速度
                    if (need[0] || need[1])
                        homing_set_accel(prev_accel);

                    res["command"] = "M2202";
                    res["result"] = "2802";
                    gcmd->respond_feedback(res);
                    throw;
                }

                // 恢复电流
                for (int axis = 0; axis < 2; axis++)
                {
                    homing_set_run_current(axis, run_current[axis]);
                }

                // 恢复加速度
                if (need[0] || need[1])
                    homing_set_accel(prev_accel);
            }

            // 7. 归零Z
            try
            {
                if (need[2])
                {

                    if (is_probe())
                    {
                        home_x_pos = home_x_pos_probe;
                        home_y_pos = home_y_pos_probe;
                        home_xy_speed = home_xy_speed_probe;
                        home_x_pos2 = home_x_pos2_probe;
                        home_y_pos2 = home_y_pos2_probe;
                        home_xy_speed2 = home_xy_speed2_probe;
                    }
                    else
                    {
                        home_x_pos = home_x_pos_endstop;
                        home_y_pos = home_y_pos_endstop;
                        home_xy_speed = home_xy_speed_endstop;
                        home_x_pos2 = home_x_pos2_endstop;
                        home_y_pos2 = home_y_pos2_endstop;
                        home_xy_speed2 = home_xy_speed2_endstop;
                    }

                    // 移动到Z轴归零点 home_x_pos，home_y_pos
                    // 判断是否存在I与J位置用于设置Z轴归零位置
                    double home_x = gcmd->get_double("I", home_x_pos);
                    double home_y = gcmd->get_double("J", home_y_pos);
                    bool i_j = false;
                    if (gcmd->get("I", "None") != "None" || gcmd->get("J", "None") != "None")
                        i_j = true;
                    std::vector<double> coord = {home_x, home_y, DOUBLE_NONE};
                    SPDLOG_INFO("manual_move z start");
                    toolhead->manual_move(coord, home_xy_speed);
                    SPDLOG_INFO("manual_move z over");

                    // 第二段
                    if (!std::isnan(home_x_pos2) && !std::isnan(home_y_pos2) && !std::isnan(home_xy_speed2))
                    {
                        coord = {home_x_pos2, home_y_pos2, DOUBLE_NONE};
                        toolhead->manual_move(coord, home_xy_speed2);
                    }

                    // 进行Z轴归零
                    std::map<std::string, std::string> new_params = {{"Z", "0"}};
                    if (gcmd->get_int("L", INT_MAX) != INT_MAX)
                        new_params["L"] = std::to_string(gcmd->get_int("L"));
                    else
                        new_params["L"] = std::to_string(z_samples);
                    if (!std::isnan(gcmd->get_double("K", DOUBLE_NONE)))
                        new_params["K"] = std::to_string(gcmd->get_double("K"));
                    else
                        new_params["K"] = std::to_string(z_samples_tolerance);

                    std::shared_ptr<GCodeCommand> g28_gcmd = gcode->create_gcode_command("G28", "G28", new_params);
                    SPDLOG_INFO("prev_G28 z start");
                    this->prev_G28(g28_gcmd);
                    SPDLOG_INFO("prev_G28 z over");
                    auto pos = toolhead->get_position();
                    SPDLOG_INFO("manual_move z start");
                    if (!i_j)
                    {
                        // 移动Z轴到归零结束位置
                        pos[2] = z_retract_pos;
                        toolhead->manual_move(pos, z_retract_speed);
                    }
                    else
                    {
                        pos[2] = 0.;
                        toolhead->manual_move(pos, z_retract_speed);
                    }
                    SPDLOG_INFO("manual_move z over");
                }
            }
            catch (...)
            {
                res["command"] = "M2202";
                res["result"] = "2802";
                gcmd->respond_feedback(res);
                throw;
            }

            res["command"] = "M2202";
            res["result"] = "2802";
            gcmd->respond_feedback(res);
        }

        std::string HomingOverride::to_upper(const std::string &input)
        {
            std::string result = input;
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c)
                           {
                               return std::toupper(c);
                           });
            return result;
        }

        std::shared_ptr<HomingOverride> homing_override_load_config(
            std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<HomingOverride>(config);
        }

    }
}