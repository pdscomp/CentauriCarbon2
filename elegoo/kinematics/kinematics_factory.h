/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-06 16:46:09
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-13 15:00:29
 * @Description  : Manage the creation of kinematics module objects.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once

#include "kinematics.h"
#include "cartesian.h"
#include "corexy.h"
#include "corexz.h"
#include "delta.h"
#include "deltesian.h"
#include "hybrid_corexy.h"
#include "hybrid_corexz.h"
#include "none.h"
#include "polar.h"
#include "rotary_delta.h"
#include "winch.h"

using namespace elegoo::extras;
class KinematicsFactory 
{
public:
    static std::shared_ptr<Kinematics> create_kinematics(
        const std::string& type,
        std::shared_ptr<ToolHead> toolhead,
        std::shared_ptr<ConfigWrapper> config);
};