#include "z_compensation.h"
#include "configfile.h"
#include "gcode.h"
#include "gcode_macro.h"
#include "printer.h"
#include "gcode_move.h"
#include "kinematics_factory.h"
#include "homing.h"
#include "probe.h"

namespace elegoo
{
    namespace extras
    {

        static std::pair<double, double> _lstsq_line(const std::vector<double> &x, const std::vector<double> &y, int start = 0, int end = 0)
        {
            if (end == 0)
                end = static_cast<int>(x.size());

            if (start < 0 || end > static_cast<int>(x.size()) || end <= start)
            {
                printf("_lstsq_line failed: start=%d end=%d size=%lu\n", start, end, x.size());
                return {0., 0.};
            }

            size_t N = end - start;

            // Step 1: Compute means
            double mean_x = 0.0, mean_y = 0.0;
            for (size_t i = start; i < static_cast<size_t>(end); ++i)
            {
                mean_x += x[i];
                mean_y += y[i];
            }
            mean_x /= N;
            mean_y /= N;

            // Step 2: Compute centered sums
            double Sxx = 0.0, Sxy = 0.0;
            for (size_t i = start; i < static_cast<size_t>(end); ++i)
            {
                double dx = x[i] - mean_x;
                double dy = y[i] - mean_y;
                Sxx += dx * dx;
                Sxy += dx * dy;
            }

            if (Sxx == 0.0) // Avoid division by zero
                return {0., static_cast<double>(mean_y)};

            // Step 3: Compute slope and intercept
            double m = Sxy / Sxx;
            double b = mean_y - m * mean_x;

            return {static_cast<double>(m), static_cast<double>(b)};
        }

        ZCompensationHeater::ZCompensationHeater(std::shared_ptr<Printer> printer, const std::string &name, double start, double diff, std::vector<double> datas, bool use_fix_ref_temperature, double fix_ref_temperature)
            : name(name), start(start), diff(diff), datas(datas), use_fix_ref_temperature(use_fix_ref_temperature), fix_ref_temperature(fix_ref_temperature)
        {
            std::shared_ptr<PrinterHeaters> pheaters = any_cast<std::shared_ptr<PrinterHeaters>>(printer->lookup_object("heaters"));
            heater = pheaters->lookup_heater(name);
            if (!heater)
                throw elegoo::common::ConfigParserError("can't found heater " + name);
            heater->setup_callback(
                [this](double read_time, double temp)
                {
                    smoothed_temp = temp;
                });

            // 使用二乘法计算斜率
            double target = start;
            for (auto d : datas)
            {
                temps.push_back(target);
                // SPDLOG_INFO("temp {} data {}", target, d);
                target += diff;
            }

            std::tie(slope, intercept) = _lstsq_line(temps, datas);
            SPDLOG_INFO("ZCompensationHeater slope {} intercept {} use_fix_ref_temperature {} fix_ref_temperature {}", slope, intercept, use_fix_ref_temperature, fix_ref_temperature);
        }

        // 计算相对归零时刻的偏差
        void ZCompensationHeater::handle_home_rails_end()
        {
            ref_temperature = smoothed_temp;
            SPDLOG_INFO("use_fix_ref_temperature {} fix_ref_temperature {} ref_temperature {}",
                        use_fix_ref_temperature,
                        fix_ref_temperature,
                        ref_temperature);
        }
        
        double ZCompensationHeater::calc_abs_offset(double temp)
        {
            // 当前温度在测量范围外,使用斜率计算补偿值
            if (temp <= temps[0] || temp >= temps.back())
                return slope * temp + intercept;
            // 温度在测量范围内,用相邻点进行插值
            int index = (temp - start) / diff;
            return datas[index] + ((temp - temps[index]) / (temps[index + 1] - temps[index])) * (datas[index + 1] - datas[index]);
        }

        // 计算温差引起的偏移
        double ZCompensationHeater::calc_adjust()
        {
            // 当前温度在校准测量范围外,使用斜率计算补偿值
            // SPDLOG_INFO("smoothed_temp {} temps[0] {} temps[-1] {}", smoothed_temp, temps[0], temps.back());
            if (smoothed_temp <= temps[0] || smoothed_temp >= temps.back())
            {
                // SPDLOG_INFO("smoothed_temp {} ref_temperature {} adjust {}", smoothed_temp, ref_temperature, slope * (smoothed_temp - ref_temperature));
                return slope * (smoothed_temp - use_fix_ref_temperature ? fix_ref_temperature : ref_temperature);
            }
            double s = calc_abs_offset(smoothed_temp);
            double r = use_fix_ref_temperature ? calc_abs_offset(fix_ref_temperature) : calc_abs_offset(ref_temperature);
            // SPDLOG_INFO("s {} r {} offset {}", s, r, s - r);
            return s - r;
        }

        ZCompensation::ZCompensation(std::shared_ptr<ConfigWrapper> config)
        {
            printer = config->get_printer();
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
            nozzle_temp_start = config->getdouble("nozzle_temp_start", 100.);
            nozzle_temp_end = config->getdouble("nozzle_temp_end", 250.);
            nozzle_temp_diff = config->getdouble("nozzle_temp_diff", 10.);
            nozzle_calibrate_start = config->getdouble("nozzle_calibrate_start", DOUBLE_NONE);
            fix_ref_temperature = config->getdouble("fix_ref_temperature", DOUBLE_NONE);
            use_fix_ref_temperature = !std::isnan(fix_ref_temperature);

            if (!std::isnan(nozzle_calibrate_start))
            {
                nozzle_calibrate_diff = config->getdouble("nozzle_calibrate_diff");
                nozzle_calibrate_datas = config->getdoublelist("nozzle_calibrate_datas");
                // SPDLOG_INFO("start {} diff {} datas {}", nozzle_calibrate_start, nozzle_calibrate_diff, nozzle_calibrate_datas.size());
                // for (auto d : nozzle_calibrate_datas)
                //     SPDLOG_INFO("d {}", d);
            }
            std::shared_ptr<PrinterGCodeMacro> gcode_macro = any_cast<std::shared_ptr<PrinterGCodeMacro>>(printer->load_object(config, "gcode_macro"));
            clean_nozzle_gcode = gcode_macro->load_template(config, "clean_nozzle_gcode", "");
            last_position = {0., 0., 0., 0.};
            z_adjust_mm = last_z_adjust_mm = 0.;
            z_homed = false;
            elegoo::common::SignalManager::get_instance().register_signal(
                "elegoo:connect",
                std::function<void()>([this]()
                                      { handle_connect(); }));
            elegoo::common::SignalManager::get_instance().register_signal("homing:home_rails_end",
                                                                          std::function<void(std::shared_ptr<Homing>, std::vector<std::shared_ptr<PrinterRail>>)>([this](std::shared_ptr<Homing> homing_state, std::vector<std::shared_ptr<PrinterRail>> rails)
                                                                                                                                                                  { handle_home_rails_end(homing_state, rails); }));
            gcode->register_command("NOZZLE_TEMP_CALIBRATE", [this](std::shared_ptr<GCodeCommand> gcmd)
                                    { cmd_NOZZLE_TEMP_CALIBRATE(gcmd); }, false, "");
        }

        void ZCompensation::cmd_NOZZLE_TEMP_CALIBRATE(std::shared_ptr<GCodeCommand> gcmd)
        {
            // 校准喷嘴温度系数
            char cmd[256];
            double cmd_nozzle_temp_start = gcmd->get_double("START", nozzle_temp_start);
            double cmd_nozzle_temp_end = gcmd->get_double("END", nozzle_temp_end);
            double cmd_nozzle_temp_diff = gcmd->get_double("DIFF", nozzle_temp_diff);
            double cmd_nozzle_temp_delay = gcmd->get_double("DELAY", 15.);
            int cmd_nozzle_temp_samples = gcmd->get_int("SAMPLES", 10);
            double target = cmd_nozzle_temp_start;

            std::shared_ptr<PrinterProbeInterface> probe = any_cast<std::shared_ptr<PrinterProbeInterface>>(printer->lookup_object("probe"));

            // 归零
            snprintf(cmd, sizeof(cmd), "G28");
            gcode->run_script_from_command(cmd);

            // 清理喷嘴
            clean_nozzle_gcode->run_gcode_from_command();

            // 等待温度到达起始温度
            snprintf(cmd, sizeof(cmd), "M109 S%f", target);
            gcode->run_script_from_command(cmd);

            // 第二次归零
            snprintf(cmd, sizeof(cmd), "G28");
            gcode->run_script_from_command(cmd);
            snprintf(cmd, sizeof(cmd), "BED_MESH_CLEAR");
            gcode->run_script_from_command(cmd);
            snprintf(cmd, sizeof(cmd), "G1 X128 Y128 F20000");
            gcode->run_script_from_command(cmd);

            std::map<std::string, std::string> params;
            std::vector<double> positions;
            while (target <= cmd_nozzle_temp_end)
            {
                // 加热
                snprintf(cmd, sizeof(cmd), "M109 S%f", target);
                gcode->run_script_from_command(cmd);

                // 延时等待温度稳定
                snprintf(cmd, sizeof(cmd), "G4 P%d", (int)(cmd_nozzle_temp_delay * 1000));
                gcode->run_script_from_command(cmd);

                // 探测
                params["SAMPLES"] = std::to_string(cmd_nozzle_temp_samples);
                params["PROBE_SPEED"] = std::to_string(5.);
                params["PULLBACK_SPEED"] = std::to_string(1.);
                params["NAME"] = "NOZZLE_TEMP_CALIBRATE_" + std::to_string(target);
                std::shared_ptr<GCodeCommand> dummy_gcode_cmd = gcode->create_gcode_command("", "", params);
                std::vector<double> pos = run_single_probe(probe, dummy_gcode_cmd);
                std::vector<double> curpos = toolhead->get_position();
                positions.push_back(pos[2]);

                // 抬升
                std::vector<double> liftpos = {DOUBLE_NONE, DOUBLE_NONE, curpos[2] + 5.};
                any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"))->manual_move(liftpos, 10.);

                // 更新目标
                target += cmd_nozzle_temp_diff;
            }

            std::ostringstream z_values;
            for (int i = 0; i < positions.size(); i++)
            {
                z_values << std::fixed << std::setprecision(6) << positions[i];
                if (!(i == (positions.size() - 1)))
                    z_values << ", ";
                // SPDLOG_INFO("temp {} z_value {}", cmd_nozzle_temp_start + i * cmd_nozzle_temp_diff, positions[i]);
            }

            std::shared_ptr<PrinterConfig> configfile = any_cast<std::shared_ptr<PrinterConfig>>(printer->lookup_object("configfile"));
            configfile->set("z_compensation", "nozzle_calibrate_start", std::to_string(cmd_nozzle_temp_start));
            configfile->set("z_compensation", "nozzle_calibrate_diff", std::to_string(cmd_nozzle_temp_diff));
            configfile->set("z_compensation", "nozzle_calibrate_datas", z_values.str());
        }

        void ZCompensation::handle_connect()
        {
            toolhead = any_cast<std::shared_ptr<ToolHead>>(printer->lookup_object("toolhead"));
            std::shared_ptr<elegoo::extras::GCodeMove> gcode_move = any_cast<std::shared_ptr<elegoo::extras::GCodeMove>>(printer->lookup_object("gcode_move"));
            move_transform = std::make_shared<GCodeMoveTransform>();
            move_transform->move_with_transform = std::bind(&ZCompensation::move, this, std::placeholders::_1, std::placeholders::_2);
            move_transform->position_with_transform = std::bind(&ZCompensation::get_position, this);
            next_transform = gcode_move->set_move_transform(move_transform, true);

            // 获取Z轴步进距离
            std::shared_ptr<Kinematics> kin = toolhead->get_kinematic();
            std::vector<std::shared_ptr<MCU_stepper>> steppers = kin->get_steppers();
            for (auto &s : steppers)
            {
                if (s->get_name() == "stepper_z")
                {
                    z_step_dist = s->get_step_dist();
                    break;
                }
            }

            if (!std::isnan(nozzle_calibrate_start))
                heaters.push_back(std::make_shared<ZCompensationHeater>(printer, "extruder",
                                                                        nozzle_calibrate_start, nozzle_calibrate_diff, nozzle_calibrate_datas, use_fix_ref_temperature, fix_ref_temperature));
        }

        void ZCompensation::handle_home_rails_end(std::shared_ptr<Homing> homing_state, std::vector<std::shared_ptr<PrinterRail>> rails)
        {
            std::vector<int> axes = homing_state->get_axes();
            if (std::find(axes.begin(), axes.end(), 2) != axes.end())
            {
                SPDLOG_INFO("ZCompensation handle_home_rails_end");
                for (auto &h : heaters)
                    h->handle_home_rails_end();
                // 标记已经归零,归零后才会开始补偿
                z_homed = true;
                z_adjust_mm = 0.;
            }
        }

        std::vector<double> ZCompensation::calc_adjust(std::vector<double> pos)
        {
            // 获取并重新计算补偿值
            if (z_homed)
            {
                double adjust = 0.;
                for (auto &h : heaters)
                    adjust += h->calc_adjust();
                // 补偿值超过一个步距离才生效
                if (fabs(adjust - z_adjust_mm) > z_step_dist || adjust == 0.)
                {
                    z_adjust_mm = adjust;
                    SPDLOG_INFO("ZCompensation z_adjust_mm {}", z_adjust_mm);
                }
            }

            double new_z = pos[2] + z_adjust_mm;
            last_z_adjust_mm = z_adjust_mm;
            return {pos[0], pos[1], new_z, pos[3]};
        }

        std::vector<double> ZCompensation::calc_unadjust(std::vector<double> pos)
        {
            return {pos[0], pos[1], pos[2] - z_adjust_mm, pos[3]};
        }

        std::vector<double> ZCompensation::get_position()
        {
            std::vector<double> position = calc_unadjust(next_transform->position_with_transform());
            last_position = calc_adjust(position);
            return position;
        }

        void ZCompensation::move(const std::vector<double> &newpos, double speed)
        {
            std::vector<double> adjusted_pos;
            // XYZ没有变化不需要补偿或者没有归零,使用上一次的补偿值
            if ((newpos[0] == last_position[0] && newpos[1] == last_position[1] && newpos[2] == last_position[2]))
            {
                adjusted_pos = {newpos[0], newpos[1], newpos[2] + last_z_adjust_mm, newpos[3]};
                next_transform->move_with_transform(adjusted_pos, speed);
            }
            // 重新计算补偿位置
            else
            {
                adjusted_pos = calc_adjust(newpos);
                next_transform->move_with_transform(adjusted_pos, speed);
            }
            last_position = newpos;
            // SPDLOG_INFO("last_z {} new_z {} adjust_z {}", last_position[2], newpos[2], adjusted_pos[2]);
        }

        std::shared_ptr<ZCompensation> z_compensation_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            std::shared_ptr<ZCompensation> zc = std::make_shared<ZCompensation>(config);
            return zc;
        }
    }
}