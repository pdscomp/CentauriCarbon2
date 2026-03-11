/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-06 16:46:39
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-21 15:48:10
 * @Description  : Manage the creation of kinematics module objects.
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "kinematics_factory.h"
#include <stdexcept>

std::shared_ptr<Kinematics> KinematicsFactory::create_kinematics(
    const std::string& type,
    std::shared_ptr<ToolHead> toolhead,
    std::shared_ptr<ConfigWrapper> config) 
{
    if (type == "cartesian") 
    {
        return std::make_shared<CartKinematics>(toolhead, config);
    } 
    else if (type == "corexy") 
    {
        return std::make_shared<CoreXYKinematics>(toolhead, config);
    } 
    else if (type == "corexz") 
    {
        return std::make_shared<CoreXZKinematics>(toolhead, config);
    } 
    else if (type == "delta") 
    {
        return std::make_shared<DeltaKinematics>(toolhead, config);
    } 
    else if (type == "deltesian") 
    {
        return std::make_shared<DeltesianKinematics>(toolhead, config);
    } 
    else if (type == "hybrid_corexy") 
    {
        return std::make_shared<HybridCoreXYKinematics>(toolhead, config);
    } 
    else if (type == "hybrid_corexz") 
    {
        return std::make_shared<HybridCoreXZKinematics>(toolhead, config);
    } 
    else if (type == "none") 
    {
        return std::make_shared<NoneKinematics>(toolhead, config);
    } 
    else if (type == "polar") 
    {
        return std::make_shared<PolarKinematics>(toolhead, config);
    } 
    else if (type == "rotary_delta") 
    {
        return std::make_shared<RotaryDeltaKinematics>(toolhead, config);
    } 
    else if (type == "winch") 
    {
        return std::make_shared<WinchKinematics>(toolhead, config);
    } 
    else 
    {
        // throw std::invalid_argument("Unknown product type");
    }

    return nullptr;
}