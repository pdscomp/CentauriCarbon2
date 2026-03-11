/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-14 10:44:15
 * @Description  : adds support fro ARC commands via G2/G3
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <memory>
#include <vector>
class GCodeCommand;
class ConfigWrapper;
class Printer;
class GCodeDispatch;

namespace elegoo
{
    namespace extras
    {

        class GCodeMove;
        enum ArcPlane
        {
            ARC_PLANE_X_Y = 0,
            ARC_PLANE_X_Z = 1,
            ARC_PLANE_Y_Z = 2
        };

        enum Axis
        {
            X_AXIS = 0,
            Y_AXIS = 1,
            Z_AXIS = 2,
            E_AXIS = 3
        };

        class ArcSupport
        {
        public:
            ArcSupport(std::shared_ptr<ConfigWrapper> config);
            ~ArcSupport();

            void cmd_G2(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_G3(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_G17(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_G18(std::shared_ptr<GCodeCommand> gcmd);
            void cmd_G19(std::shared_ptr<GCodeCommand> gcmd);
            void planArc(const std::vector<double> &currentPos,
                         const std::vector<double> &asTarget,
                         const std::vector<double> &asPlanar, bool clockwise,
                         std::shared_ptr<GCodeCommand> gcmd, bool absolute_extrude,
                         Axis alpha_axis, Axis beta_axis, Axis helical_axis);

        private:
            void _cmd_inner(std::shared_ptr<GCodeCommand> gcmd, bool clockwise);

        private:
            std::shared_ptr<Printer> printer;
            std::shared_ptr<GCodeMove> gcode_move;
            std::shared_ptr<GCodeDispatch> gcode;
            double mm_per_arc_segment;
            ArcPlane plane;
        };

        std::shared_ptr<ArcSupport> gcode_arcs_load_config(std::shared_ptr<ConfigWrapper> config);

    }
}