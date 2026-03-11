/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Jack
 * @LastEditTime : 2025-02-24 14:46:59
 * @Description  : adds support fro ARC commands via G2/G3
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "gcode_arcs.h"
#include "configfile.h"
#include "printer.h"
#include "gcode.h"
#include "gcode_move.h"
#include <cmath>
#include <algorithm>

namespace elegoo
{
    namespace extras
    {
        ArcSupport::ArcSupport(std::shared_ptr<ConfigWrapper> config)
        {
            printer = config->get_printer();
            mm_per_arc_segment = config->getdouble("resolution", 1, DOUBLE_NONE, DOUBLE_NONE, 0);
            gcode_move = any_cast<std::shared_ptr<GCodeMove>>(printer->load_object(config, "gcode_move"));
            gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));

            gcode->register_command("G2",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_G2(gcmd);
                                    });

            gcode->register_command("G3",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_G3(gcmd);
                                    });

            gcode->register_command("G17",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_G17(gcmd);
                                    });

            gcode->register_command("G18",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_G18(gcmd);
                                    });

            gcode->register_command("G19",
                                    [this](std::shared_ptr<GCodeCommand> gcmd)
                                    {
                                        cmd_G19(gcmd);
                                    });

            plane = ArcPlane::ARC_PLANE_X_Y;
        }

        ArcSupport::~ArcSupport()
        {
        }

        void ArcSupport::cmd_G2(std::shared_ptr<GCodeCommand> gcmd)
        {
            _cmd_inner(gcmd, true);
        }

        void ArcSupport::cmd_G3(std::shared_ptr<GCodeCommand> gcmd)
        {
            _cmd_inner(gcmd, false);
        }

        void ArcSupport::cmd_G17(std::shared_ptr<GCodeCommand> gcmd)
        {
            plane = ARC_PLANE_X_Y;
        }

        void ArcSupport::cmd_G18(std::shared_ptr<GCodeCommand> gcmd)
        {
            plane = ARC_PLANE_X_Z;
        }

        void ArcSupport::cmd_G19(std::shared_ptr<GCodeCommand> gcmd)
        {
            plane = ARC_PLANE_Y_Z;
        }

        void ArcSupport::_cmd_inner(std::shared_ptr<GCodeCommand> gcmd, bool clockwise)
        {
            json gcodestatus = gcode_move->get_status();
            if (!gcodestatus["absolute_coordinates"].get<bool>())
            {
                throw elegoo::common::CommandError("G2/G3 does not support relative move mode");
            }

            json currentPosJson = gcodestatus["gcode_position"];
            bool absolute_extrude = gcodestatus["absolute_extrude"].get<bool>();
            if (!currentPosJson.is_array() || currentPosJson.size() < 3)
            {
                throw elegoo::common::CommandError("Invalid gcode_position format");
            }

            double x_defalut = currentPosJson[0].get<double>();
            double y_defalut = currentPosJson[1].get<double>();
            double z_defalut = currentPosJson[2].get<double>();
            double x = gcmd->get_double("X", x_defalut);
            double y = gcmd->get_double("Y", y_defalut);
            double z = gcmd->get_double("Z", z_defalut);
            std::vector<double> currentPos{x_defalut, y_defalut, z_defalut};
            std::vector<double> asTarget{x, y, z};

            if (!std::isnan(gcmd->get_double("R", DOUBLE_NONE)))
            {
                throw elegoo::common::CommandError("G2/G3 does not support R moves");
            }

            double I = gcmd->get_double("I", 0);
            double J = gcmd->get_double("J", 0);
            std::vector<double> asPlanar = {I, J};
            std::vector<Axis> axes = {X_AXIS, Y_AXIS, Z_AXIS};

            if (plane == ARC_PLANE_X_Z)
            {
                double K = gcmd->get_double("K", 0);
                asPlanar = {I, K};
                axes = {X_AXIS, Z_AXIS, Y_AXIS};
            }
            else if (plane == ARC_PLANE_Y_Z)
            {
                double K = gcmd->get_double("K", 0);
                asPlanar = {J, K};
                axes = {Y_AXIS, Z_AXIS, X_AXIS};
            }

            if (asPlanar.at(0) == 0.0f && asPlanar.at(1) == 0.0f)
            {
                throw elegoo::common::CommandError("G2/G3 requires IJ, IK or JK parameters");
            }

            planArc(currentPos, asTarget, asPlanar, clockwise,
                    gcmd, absolute_extrude, axes[0], axes[1], axes[2]);
        }

        void ArcSupport::planArc(const std::vector<double> &currentPos,
                                 const std::vector<double> &targetPos,
                                 const std::vector<double> &offset, bool clockwise,
                                 std::shared_ptr<GCodeCommand> gcmd, bool absolute_extrude,
                                 Axis alpha_axis, Axis beta_axis, Axis helical_axis)
        {
            double r_P = -offset.at(0);
            double r_Q = -offset.at(1);

            // 计算中心点坐标和角度变化
            double center_P = currentPos[alpha_axis] - r_P;
            double center_Q = currentPos[beta_axis] - r_Q;
            double rt_Alpha = targetPos[alpha_axis] - center_P;
            double rt_Beta = targetPos[beta_axis] - center_Q;
            double angular_travel = std::atan2(r_P * rt_Beta - r_Q * rt_Alpha,
                                               r_P * rt_Alpha + r_Q * rt_Beta);
            if (angular_travel < 0.0f)
                angular_travel += 2.0f * M_PI;
            if (clockwise)
                angular_travel -= 2.0f * M_PI;

            if (angular_travel == 0.0f &&
                currentPos[alpha_axis] == targetPos[alpha_axis] &&
                currentPos[beta_axis] == targetPos[beta_axis])
            {
                angular_travel = 2.0f * M_PI;
            }

            double linear_travel = targetPos[helical_axis] - currentPos[helical_axis];
            double radius = std::hypot(r_P, r_Q);
            double flat_mm = radius * angular_travel;

            double mm_of_travel = (linear_travel != 0.0f) ? std::hypot(flat_mm, linear_travel) : std::fabs(flat_mm);

            double segments = std::max(1.0, std::floor(mm_of_travel / mm_per_arc_segment));

            double theta_per_segment = angular_travel / segments;
            double linear_per_segment = linear_travel / segments;

            double asE = gcmd->get_double("E", DOUBLE_NONE);
            double asF = gcmd->get_double("F", DOUBLE_NONE);
            double e_base = 0.0f;
            double e_per_move = 0.0f;

            if (!std::isnan(asE))
            {
                if (absolute_extrude)
                {
                    e_base = currentPos[3];
                }         
                e_per_move = (asE - e_base) / segments;
            }

            for (int i = 1; i < int(segments) + 1; ++i)
            {
                double dist_Helical = i * linear_per_segment;
                double c_theta = i * theta_per_segment;
                double cos_Ti = std::cos(c_theta);
                double sin_Ti = std::sin(c_theta);

                double r_P = -offset.at(0) * cos_Ti + offset.at(1) * sin_Ti;
                double r_Q = -offset.at(0) * sin_Ti - offset.at(1) * cos_Ti;

                std::vector<double> c(3);
                c[alpha_axis] = center_P + r_P;
                c[beta_axis] = center_Q + r_Q;
                c[helical_axis] = currentPos[helical_axis] + dist_Helical;

                if (i == int(segments))
                    c = targetPos;

                std::map<std::string, double> g1_params = {{"X", c[0]}, {"Y", c[1]}, {"Z", c[2]}};
                if (e_per_move)
                {
                    g1_params["E"] =e_base + e_per_move;
                    if (absolute_extrude)
                    {
                        e_base += e_per_move;
                    }
                }
                if (!std::isnan(asF))
                    g1_params["F"] = asF;

                std::shared_ptr<GCodeCommand> g1_gcmd = gcode->create_gcode_command_double("G1", "G1", g1_params);
                gcode_move->cmd_G1(g1_gcmd, true);
            }
        }

        std::shared_ptr<ArcSupport> gcode_arcs_load_config(std::shared_ptr<ConfigWrapper> config)
        {
            return std::make_shared<ArcSupport>(config);
        }

    }
}
