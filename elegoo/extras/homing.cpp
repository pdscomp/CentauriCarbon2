/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-20 14:24:16
 * @Description  :  Helper code for implementing homing operations
 *
 * Copyright (c) 2025 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#include <numeric>
#include "homing.h"
#include "printer.h"
#include "kinematics_factory.h"
#include "stepper_enable.h"
#include "homing_override.h"
#include "probe.h"
#include "print_stats.h"

const double HOMING_START_DELAY = 0.001;
const double ENDSTOP_SAMPLE_TIME = .000015;
const int ENDSTOP_SAMPLE_COUNT = 4;

namespace elegoo
{
    namespace extras
    {

        std::shared_ptr<ReactorCompletion> multi_complete(std::shared_ptr<Printer> &printer,
                                                          std::vector<std::shared_ptr<ReactorCompletion>> &completions)
        {
            if (completions.size() == 1)
            {
                return completions[0];
            }

            std::shared_ptr<SelectReactor> reactor = printer->get_reactor();
            std::shared_ptr<ReactorCompletion> cp = reactor->register_callback(
                [&completions](double e)
                {
                    for (std::shared_ptr<ReactorCompletion> c : completions)
                    {
                        return c->wait();
                    }
                    return json::object();
                });

            for (std::shared_ptr<ReactorCompletion> c : completions)
            {
                reactor->register_callback([&cp, &c](double e)
                                           {
            if (c->wait())
            {
                cp->complete(json::object());
            }
            return json::object(); });
            }

            return cp;
        }

        StepperPosition::StepperPosition(std::shared_ptr<MCU_stepper> stepper,
                                         const std::string &endstop_name) : stepper(stepper), endstop_name(endstop_name)
        {
            stepper_name = stepper->get_name();
            start_pos = stepper->get_mcu_position();
            start_cmd_pos = stepper->mcu_to_commanded_position(start_pos);
            halt_pos = trig_pos = 0;
        }

        StepperPosition::~StepperPosition()
        {
        }

        void StepperPosition::note_home_end(double trigger_time)
        {
            halt_pos = stepper->get_mcu_position();
            trig_pos = stepper->get_past_mcu_position(trigger_time);
        }

        void StepperPosition::verify_no_probe_skew(void)
        {
            int64_t new_start_pos = stepper->get_mcu_position(start_cmd_pos);
            if (new_start_pos != start_pos)
            {
                SPDLOG_WARN("Stepper {} position skew after probe: pos {} now {}",
                            stepper->get_name(), start_pos, new_start_pos);
            }
        }

        HomingMove::HomingMove(std::shared_ptr<Printer> printer,
                               const std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> &endstops,
                               std::shared_ptr<ToolHead> toolhead)
            : printer(printer), endstops(endstops)
        {
            if (toolhead == nullptr)
                toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            this->toolhead = toolhead;
        }

        HomingMove::~HomingMove()
        {
        }

        std::vector<std::shared_ptr<MCU_pins>> HomingMove::get_mcu_endstops()
        {
            std::vector<std::shared_ptr<MCU_pins>> mcu_endstops;
            for (const auto &endstop : endstops)
            {
                mcu_endstops.push_back(endstop.first);
            }
            return mcu_endstops;
        }

        std::vector<double> HomingMove::calc_toolhead_pos(
            std::map<std::string, double> kin_spos,
            const std::map<std::string, double> &offsets)
        {
            std::shared_ptr<Kinematics> kin = toolhead->get_kinematic();
            if (!kin)
                throw std::runtime_error("Kinematics object not found");
            for (std::shared_ptr<MCU_stepper> stepper : kin->get_steppers())
            {
                std::string sname = stepper->get_name();
                kin_spos[sname] += offsets.count(sname) ? offsets.at(sname) * stepper->get_step_dist() : 0.0;
            }
            std::vector<double> thpos = toolhead->get_position();
            std::vector<double> new_pos = kin->calc_position(kin_spos);
            new_pos.insert(new_pos.end(), thpos.begin() + 3, thpos.end());
            return new_pos;
        }

        std::vector<double> HomingMove::homing_move(std::vector<double> movepos,
                                                    double speed, bool probe_pos, bool triggered, bool check_triggered)
        {
            elegoo::common::SignalManager::get_instance().emit_signal("homing:homing_move_begin", shared_from_this());

            // double __print_time = toolhead->print_time;
            // SPDLOG_INFO("homing_move #0 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;

            toolhead->flush_step_generation();

            // SPDLOG_INFO("homing_move #1 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;

            // 从迭代求解器中读取坐标位置
            std::shared_ptr<Kinematics> kin = toolhead->get_kinematic();
            std::map<std::string, double> kin_spos;
            for (std::shared_ptr<MCU_stepper> s : kin->get_steppers())
            {
                kin_spos[s->get_name()] = s->get_commanded_position();
                SPDLOG_INFO("kin_spos name {} command_position {}", s->get_name(), s->get_commanded_position());
            }

            // 跟踪限位关联的电机
            stepper_positions.clear();
            for (const auto &es_pair : endstops)
            {
                std::shared_ptr<MCU_pins> es = es_pair.first;
                const std::string &name = es_pair.second;
                for (auto stepper : es->get_steppers())
                    stepper_positions.push_back(std::make_shared<StepperPosition>(stepper, name));
            }

            // 调用MCU_endstop的home_start函数
            // 这里会同步一次时间,导致print_time增加
            // 前面的flush_step_generation会进入"NeedPrime"状态,这里会导致进入_calc_print_time增加时间
            double print_time = toolhead->get_last_move_time();

            // SPDLOG_INFO("homing_move #2 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;

            std::vector<std::shared_ptr<ReactorCompletion>> endstop_triggers;
            for (auto &endstop_pair : endstops)
            {
                double rest_time = _calc_endstop_rate(endstop_pair.first, movepos, speed);

                // 计算加速时间
                std::vector<double> startpos = toolhead->get_position();
                std::vector<double> axes_d(3);
                for (size_t i = 0; i < 3; ++i)
                    axes_d[i] = movepos[i] - startpos[i];
                double move_d = std::sqrt(axes_d[0] * axes_d[0] + axes_d[1] * axes_d[1] + axes_d[2] * axes_d[2]);
                double accel = toolhead->max_accel;
                double velocity = speed;
                if (axes_d[2])
                {
                    double z_ratio = move_d / std::abs(axes_d[2]);
                    accel = std::min(accel, toolhead->kin->get_max_z_accel() * z_ratio);
                    velocity = std::min(velocity, toolhead->kin->get_max_z_velocity() * z_ratio);
                }
                // v = sqrt(2as)
                velocity = std::min(std::sqrt(2 * accel * move_d), velocity);
                // t = v / a
                double accel_t = velocity / accel + 0.02;

                // 调用home_start函数
                std::shared_ptr<ReactorCompletion>
                    wait = endstop_pair.first->home_start(print_time, ENDSTOP_SAMPLE_TIME,
                                                          ENDSTOP_SAMPLE_COUNT, rest_time, triggered, accel_t);
                endstop_triggers.push_back(wait);
            }

            // SPDLOG_INFO("homing_move #3 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;

            std::shared_ptr<ReactorCompletion> all_endstop_trigger = multi_complete(printer, endstop_triggers);
            toolhead->dwell(HOMING_START_DELAY);

            // SPDLOG_INFO("homing_move #4 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;

            // 开始移动
            std::string error;
            try
            {
                toolhead->drip_move(movepos, speed, all_endstop_trigger);
            }
            catch (const elegoo::common::CommandError &e)
            {
                error = "Error during homing move: " + std::string(e.what());
            }

            // SPDLOG_INFO("homing_move #5 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;

            // 等待完成触发
            std::map<std::string, double> trigger_times = {};
            double move_end_print_time = toolhead->get_last_move_time();

            // SPDLOG_INFO("homing_move #6 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;

            // 开始和结束的时间
            double trigger_time = 0.;
            for (auto &val : endstops)
            {
                try
                {
                    trigger_time = val.first->home_wait(move_end_print_time);
                }
                catch (const elegoo::common::CommandError &e)
                {
                    if (error.empty())
                        error = "Error during homing " + val.second + ": " + e.what();
                    continue;
                }
                // 正常触发，保存触发时间
                if (trigger_time > 0.0)
                    trigger_times[val.second] = trigger_time;
                // 完成整段归零运动仍然没有触发
                else if (check_triggered && error.empty())
                    error = "No trigger on " + val.second + " after full movement";
            }

            // SPDLOG_INFO("homing_move #7 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;

            // 计算触发时刻电机位置
            toolhead->flush_step_generation();
            // SPDLOG_INFO("homing_move #8 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;
            for (std::shared_ptr<StepperPosition> sp : stepper_positions)
            {
                double tt = trigger_times.count(sp->endstop_name) ? trigger_times[sp->endstop_name] : move_end_print_time;
                SPDLOG_INFO("endstop_name {} tt {} trigger_time {} move_end_print_time {}", sp->endstop_name, tt, trigger_time, move_end_print_time);
                sp->note_home_end(tt);
            }

            std::vector<double> haltpos;
            std::vector<double> trigpos;
            // 探针分支
            if (probe_pos)
            {
                std::map<std::string, double> halt_steps, trig_steps;
                for (std::shared_ptr<StepperPosition> sp : stepper_positions)
                {
                    halt_steps[sp->stepper_name] = sp->halt_pos - sp->start_pos;
                    trig_steps[sp->stepper_name] = sp->trig_pos - sp->start_pos;
                    SPDLOG_INFO("stepper_name {} halt_steps {} trig_steps {} start_pos {} trig_pos {} halt_pos {}",
                                sp->stepper_name, halt_steps[sp->stepper_name], trig_steps[sp->stepper_name], sp->start_pos, sp->trig_pos, sp->halt_pos);
                }
                haltpos = calc_toolhead_pos(kin_spos, trig_steps);
                trigpos = haltpos;
                if (trig_steps != halt_steps)
                    haltpos = calc_toolhead_pos(kin_spos, halt_steps);

                printf("movepos %f %f %f haltpos %f %f %f trigpos %f %f %f\n",
                       movepos[0], movepos[1], movepos[2],
                       haltpos[0], haltpos[1], haltpos[2],
                       trigpos[0], trigpos[1], trigpos[2]);

                toolhead->set_position(haltpos);
                // SPDLOG_INFO("homing_move #9 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
                // __print_time = toolhead->print_time;
                for (std::shared_ptr<StepperPosition> sp : stepper_positions)
                    sp->verify_no_probe_skew();
            }
            // 归零分支
            else
            {
                // 假设触发点与暂停点都在目标位置
                haltpos = movepos;
                trigpos = movepos;
                std::map<std::string, double> over_steps;
                for (auto sp : stepper_positions)
                {
                    over_steps[sp->stepper_name] = sp->halt_pos - sp->trig_pos;
                    SPDLOG_INFO("stepper_name {} over_steps {} halt_pos {} start_pos {} trig_pos {}", sp->stepper_name, over_steps[sp->stepper_name], sp->halt_pos, sp->start_pos, sp->trig_pos);
                }
                if (std::any_of(over_steps.begin(), over_steps.end(),
                                [](const std::pair<std::string, double> &step)
                                { return step.second != 0.0; }))
                {
                    toolhead->set_position(movepos);
                    // SPDLOG_INFO("homing_move #9 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
                    // __print_time = toolhead->print_time;
                    std::map<std::string, double> halt_kin_spos;
                    for (std::shared_ptr<MCU_stepper> s : kin->get_steppers())
                        halt_kin_spos[s->get_name()] = s->get_commanded_position();
                    haltpos = calc_toolhead_pos(halt_kin_spos, over_steps);
                }
                toolhead->set_position(haltpos);
                // SPDLOG_INFO("homing_move #10 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
                // __print_time = toolhead->print_time;
            }

            // 通知归零或探测已经完成
            try
            {
                elegoo::common::SignalManager::get_instance().emit_signal("homing:homing_move_end", shared_from_this());
            }
            catch (const elegoo::common::CommandError &e)
            {
                if (error.empty())
                    error = e.what();
            }

            // SPDLOG_INFO("homing_move #11 print_time {} {}", toolhead->print_time, toolhead->print_time - __print_time);
            // __print_time = toolhead->print_time;

            // 结束后抛出异常
            if (!error.empty())
            {
                SPDLOG_ERROR("{}", error);
                throw elegoo::common::CommandError(error);
            }
            return trigpos;
        }

        std::string HomingMove::check_no_movement()
        {
            std::unordered_map<std::string, std::string> start_args =
                printer->get_start_args();
            if (start_args.find("debuginput") != start_args.end())
                return "";
            for (std::shared_ptr<StepperPosition> sp : stepper_positions)
            {
                if (sp->start_pos == sp->trig_pos)
                    return sp->endstop_name;
            }
            return "";
        }

        double HomingMove::_calc_endstop_rate(
            std::shared_ptr<MCU_pins> mcu_endstop,
            const std::vector<double> &movepos, double speed)
        {
            std::vector<double> startpos = toolhead->get_position();
            std::vector<double> axes_d(3);
            for (size_t i = 0; i < 3; ++i)
                axes_d[i] = movepos[i] - startpos[i];
            double move_d = std::sqrt(axes_d[0] * axes_d[0] + axes_d[1] * axes_d[1] + axes_d[2] * axes_d[2]);
            // 最短时间
            double move_t = move_d / speed;
            double max_steps = 0.0;

            // 计算关联该限位的电机在归零过程中走一步最快的速率作为限位查询周期
            for (std::shared_ptr<MCU_stepper> stepper : mcu_endstop->get_steppers())
            {
                double start_coord = stepper->calc_position_from_coord(startpos);
                double end_coord = stepper->calc_position_from_coord(movepos);
                double steps = std::abs(start_coord - end_coord) / stepper->get_step_dist();
                max_steps = std::max(max_steps, steps);
            }

            // 返回计算的速率
            if (max_steps <= 0.0)
                return 0.001;
            return move_t / max_steps;
        }

        Homing::Homing(std::shared_ptr<Printer> printer)
            : printer(printer)
        {
            toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
        }

        Homing::~Homing()
        {
        }

        void Homing::set_axes(const std::vector<int> &axes)
        {
            changed_axes = axes;
        }

        std::vector<int> Homing::get_axes()
        {
            return changed_axes;
        }

        int Homing::get_trigger_position(const std::string &stepper_name)
        {
            return trigger_mcu_pos[stepper_name];
        }

        void Homing::set_stepper_adjustment(
            const std::string &stepper_name, double adjustment)
        {
            adjust_pos[stepper_name] = adjustment;
        }

        void Homing::set_homed_position(const std::vector<double> &pos)
        {
            toolhead->set_position(_fill_coord(pos));
        }

        void Homing::home_rails(std::vector<std::shared_ptr<PrinterRail>> rails,
                                std::vector<double> forcepos, std::vector<double> movepos)
        {
            // forcepos与movepos都是<x,y,z,e>，并且使用nan标识none
            elegoo::common::SignalManager::get_instance().emit_signal("homing:home_rails_begin", shared_from_this(), rails);

            // 根据forcepos判断哪些轴需要归零
            std::vector<int> homing_axes;
            for (int axis = 0; axis < 3; ++axis)
            {
                if (!std::isnan(forcepos[axis]))
                {
                    homing_axes.push_back(axis);
                }
            }

            std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> endstops;
            for (const std::shared_ptr<PrinterRail> &rail : rails)
            {
                auto rail_endstops = rail->get_endstops();
                endstops.insert(endstops.end(), rail_endstops.begin(), rail_endstops.end());
            }

            HomingInfo hi = rails[0]->get_homing_info();
            std::shared_ptr<PrinterProbeInterface> prb = nullptr;
            try
            {
                if (!printer->lookup_object("probe").empty())
                    prb = any_cast<std::shared_ptr<PrinterProbeInterface>>(printer->lookup_object("probe"));
            }
            catch (const elegoo::common::ConfigParserError &e)
            {
            }

            if (prb && endstops[0].first == prb->get_mcu_probe())
            {
                // 修改当前位置为行程最大值
                double position_min, position_max;
                std::tie(position_min, position_max) = rails[0]->get_range();
                forcepos[2] = position_max;
                std::vector<double> startpos = _fill_coord(forcepos);
                toolhead->set_position(startpos, homing_axes);
                std::map<std::string, std::string> params;
                std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
                SPDLOG_INFO("samples {} samples_tolerance {}", samples, samples_tolerance);

                // 第一次归零
                {
                    params["PROBE_SPEED"] = std::to_string(hi.speed);
                    params["SAMPLES"] = std::to_string(1);
                    params["SAMPLES_TOLERANCE"] = std::to_string(samples_tolerance);
                    params["NAME"] = "HOMING_1";
                    std::shared_ptr<GCodeCommand> dummy_gcode_cmd = gcode->create_gcode_command("", "", params);
                    std::vector<double> pos = run_single_probe(prb, dummy_gcode_cmd);
                    std::vector<double> curpos = toolhead->get_position();
                    std::vector<double> newpos = curpos;
                    // 修正零点为传感器触发位置
                    newpos[2] = curpos[2] - pos[2];
                    toolhead->set_position(newpos);
                    SPDLOG_INFO("homing_rail #0 {} {} {}", curpos[2], pos[2], newpos[2]);
                }

                // 第二次归零
                if (samples > 1)
                {
                    params["PROBE_SPEED"] = std::to_string(hi.second_homing_speed);
                    params["SAMPLES"] = std::to_string(1);
                    params["SAMPLES_TOLERANCE"] = std::to_string(samples_tolerance);
                    params["NAME"] = "HOMING_2";
                    std::shared_ptr<GCodeCommand> dummy_gcode_cmd = gcode->create_gcode_command("", "", params);
                    std::vector<double> pos = run_single_probe(prb, dummy_gcode_cmd);
                    std::vector<double> curpos = toolhead->get_position();
                    std::vector<double> newpos = curpos;
                    newpos[2] = curpos[2] - pos[2];
                    toolhead->set_position(newpos);
                    SPDLOG_INFO("homing_rail #1 {} {} {}", curpos[2], pos[2], newpos[2]);
                }

                // 第三次归零
                if (samples > 2)
                {
                    params["PROBE_SPEED"] = std::to_string(hi.second_homing_speed);
                    params["SAMPLES"] = std::to_string(samples - 2);
                    params["SAMPLES_TOLERANCE"] = std::to_string(samples_tolerance);
                    params["PULLBACK_SPEED"] = std::to_string(1.);
                    params["NAME"] = "HOMING_3";
                    std::shared_ptr<GCodeCommand> dummy_gcode_cmd = gcode->create_gcode_command("", "", params);
                    std::vector<double> pos = run_single_probe(prb, dummy_gcode_cmd);
                    std::vector<double> curpos = toolhead->get_position();
                    std::vector<double> newpos = curpos;
                    newpos[2] = curpos[2] - pos[2];
                    toolhead->set_position(newpos);
                    SPDLOG_INFO("homing_rail #2 {} {} {}", curpos[2], pos[2], newpos[2]);
                }
            }
            else
            {
                // 强制设置当前位置以及归零状态
                std::vector<double> startpos = _fill_coord(forcepos);
                std::vector<double> homepos = _fill_coord(movepos);
                toolhead->set_position(startpos, homing_axes);
                std::shared_ptr<HomingMove> hmove = std::make_shared<HomingMove>(printer, endstops);
                // 开始第一阶段归零
                hmove->homing_move(homepos, hi.speed);
                if (hi.retract_dist != 0.0)
                {
                    // 计算第二段归零参数
                    startpos = _fill_coord(forcepos);
                    homepos = _fill_coord(movepos);

                    std::vector<double> axes_d(homepos.size());
                    for (size_t i = 0; i < axes_d.size(); ++i)
                    {
                        axes_d[i] = homepos[i] - startpos[i];
                    }
                    double move_d = std::sqrt(axes_d[0] * axes_d[0] + axes_d[1] * axes_d[1] + axes_d[2] * axes_d[2]);

                    double retract_r = std::min(1.0, hi.retract_dist / move_d);
                    std::vector<double> retractpos(homepos.size());
                    for (size_t i = 0; i < homepos.size(); ++i)
                    {
                        retractpos[i] = homepos[i] - axes_d[i] * retract_r;
                    }
                    // printf("move_d %f retract_r %f retractpos %f %f %f %f axes_d %f %f %f %f\n",
                    //        move_d, retract_r, retractpos[0], retractpos[1], retractpos[2], retractpos[3],
                    //        axes_d[0], axes_d[1], axes_d[2], axes_d[3]);

                    toolhead->move(retractpos, hi.retract_speed);

                    for (size_t i = 0; i < retractpos.size(); ++i)
                    {
                        startpos[i] = retractpos[i] - axes_d[i] * retract_r;
                    }
                    // printf("retractpos %f %f %f %f startpos %f %f %f %f\n", retractpos[0], retractpos[1], retractpos[2], retractpos[3],
                    //        startpos[0], startpos[1], startpos[2], startpos[3]);

                    // 开始第二段归零
                    toolhead->set_position(startpos);
                    std::shared_ptr<HomingMove> hmove = std::make_shared<HomingMove>(printer, endstops);
                    SPDLOG_DEBUG("homing_move again start\n");
                    hmove->homing_move(homepos, hi.second_homing_speed);
                    SPDLOG_DEBUG("homing_move again done\n");

                    if (!hmove->check_no_movement().empty())
                    {
                        std::string error = "Endstop " + hmove->check_no_movement() + " still triggered after retract";
                        throw elegoo::common::CommandError(error);
                    }
                }
                toolhead->flush_step_generation();
                for (const auto &sp : hmove->stepper_positions)
                    trigger_mcu_pos[sp->stepper_name] = sp->trig_pos;
            }

            // endstop_phase会调用set_stepper_adjustment设置值来矫正限位精度
            adjust_pos.clear();
            elegoo::common::SignalManager::get_instance().emit_signal("homing:home_rails_end", shared_from_this(), rails);
            if (std::any_of(adjust_pos.begin(), adjust_pos.end(),
                            [](const std::pair<std::string, double> &kv)
                            { return kv.second != 0.0; }))
            {
                SPDLOG_DEBUG("adjust_pos\n");
                std::shared_ptr<Kinematics> kin = toolhead->get_kinematic();
                std::vector<double> homepos = toolhead->get_position();
                std::map<std::string, double> kin_spos;
                for (std::shared_ptr<MCU_stepper> s : kin->get_steppers())
                {
                    kin_spos[s->get_name()] = (s->get_commanded_position() + ((adjust_pos.find(s->get_name()) != adjust_pos.end()) ? adjust_pos[s->get_name()] : 0.));
                }
                std::vector<double> newpos = kin->calc_position(kin_spos);
                for (int axis : homing_axes)
                {
                    homepos[axis] = newpos[axis];
                }
                toolhead->set_position(homepos);
            }
            SPDLOG_DEBUG("home_rails end\n");
        }

        std::vector<double> Homing::_fill_coord(const std::vector<double> &coord)
        {
            std::vector<double> thcoord = toolhead->get_position();
            for (size_t i = 0; i < coord.size(); ++i)
            {
                if (!std::isnan(coord[i]))
                    thcoord[i] = coord[i];
            }
            return thcoord;
        }

        PrinterHoming::PrinterHoming(std::shared_ptr<ConfigWrapper> config)
        {
            printer = config->get_printer();
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            gcode->register_command("G28",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_G28(gcmd);
                                    });
            SPDLOG_DEBUG("PrinterHoming init success!!");
        }

        PrinterHoming::~PrinterHoming()
        {
        }

        void PrinterHoming::manual_home(std::shared_ptr<ToolHead> toolhead,
                                        const std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> &endstops,
                                        const std::vector<double> &pos, double speed, bool triggered, bool check_triggered)
        {
            std::shared_ptr<HomingMove> hmove =
                std::make_shared<HomingMove>(printer, endstops);
            try
            {
                hmove->homing_move(pos, speed, false, triggered, check_triggered);
            }
            catch (const elegoo::common::CommandError &e)
            {
                if (printer->is_shutdown())
                {
                    throw elegoo::common::CommandError("Homing failed due to printer shutdown");
                }
                throw;
            }
        }

        std::vector<double> PrinterHoming::probing_move(std::shared_ptr<MCU_pins> mcu_probe,
                                                        const std::vector<double> &pos, double speed)
        {
            std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> endstops = {{mcu_probe, "probe"}};
            std::shared_ptr<HomingMove> hmove = std::make_shared<HomingMove>(printer, endstops);
            std::vector<double> epos;
            try
            {
                epos = hmove->homing_move(pos, speed, true);
                SPDLOG_DEBUG("PrinterHoming probing_move #4");
            }
            catch (const elegoo::common::CommandError &e)
            {
                if (printer->is_shutdown())
                {
                    throw elegoo::common::CommandError("Probing failed due to printer shutdown");
                }
                throw;
            }
            if (!hmove->check_no_movement().empty())
            {
                throw elegoo::common::CommandError("Probe triggered prior to movement");
            }
            return epos;
        }

        void PrinterHoming::cmd_G28(std::shared_ptr<GCodeCommand> gcmd)
        {
            // 1. 解析需要归零的轴
            std::vector<int> axes;
            std::string axis_labels = "XYZ";

            for (size_t pos = 0; pos < axis_labels.size(); ++pos)
            {
                if (gcmd->get(std::string(1, axis_labels[pos]), "None") != "None")
                {
                    axes.push_back(static_cast<int>(pos));
                }
            }

            if (axes.empty())
            {
                axes = {0, 1, 2};
            }

            // 2. 创建归零过程跟踪类
            std::shared_ptr<Homing> homing_state = std::make_shared<Homing>(printer);
            homing_state->set_axes(axes);

            if (std::find(axes.begin(), axes.end(), 2) != axes.end())
            {
                std::shared_ptr<PrinterProbeInterface> prb = nullptr;
                try
                {
                    if (!printer->lookup_object("probe").empty())
                        prb = any_cast<std::shared_ptr<PrinterProbeInterface>>(printer->lookup_object("probe"));
                }
                catch (const elegoo::common::ConfigParserError &e)
                {
                }

                // 使用探针时设置归零参数
                if (prb)
                {
                    json params = prb->get_probe_params();
                    homing_state->samples_tolerance = gcmd->get_double("K", params["samples_tolerance"].get<double>(), 0.);
                    homing_state->samples = gcmd->get_int("L", params["samples"].get<int>() + 2, 1);
                }
            }

            std::shared_ptr<Kinematics> kin = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"))->get_kinematic();
            if (!kin)
            {
                throw std::runtime_error("Failed to lookup toolhead");
            }

            // 3. 调用运动学归零接口
            try
            {
                kin->home(homing_state);
            }
            catch (const elegoo::common::CommandError &e)
            {
                if (printer->is_shutdown())
                {
                    throw elegoo::common::CommandError("Homing failed due to printer shutdown");
                }
                gcode->respond_ecode("Z-axis homing error", elegoo::common::ErrorCode::HOMING_MOVE_Z, elegoo::common::ErrorLevel::WARNING);
                // 归零失败不关闭电机
                // auto stepper_enable = any_cast<std::shared_ptr<PrinterStepperEnable>>(printer->lookup_object("stepper_enable"));
                // stepper_enable->motor_off();
                throw;
            }
        }

        std::shared_ptr<PrinterHoming> homing_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<PrinterHoming>(config);
        }

    }
}