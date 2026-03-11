/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2024-11-19 05:57:38
 * @LastEditors  : Coconut
 * @LastEditTime : 2024-11-26 20:14:02
 * @Description  : GCodeMoveTransform
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef GCODE_MOVE_TRANSFORM_H
#define GCODE_MOVE_TRANSFORM_H
#include <functional>

struct GCodeMoveTransform
{
    std::function<void(std::vector<double>&, double)> move_with_transform;
    std::function<std::vector<double>()> position_with_transform;
};
#endif
