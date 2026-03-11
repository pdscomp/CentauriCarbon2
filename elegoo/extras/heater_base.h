/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-27 17:57:20
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-09 16:35:09
 * @Description  : Abstract class for heater sensors
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

#include <functional>

namespace elegoo {
namespace extras {

class HeaterBase{
public:
    virtual ~HeaterBase() = default;

    virtual void setup_minmax(double min_temp, double max_temp) = 0;
    virtual void setup_callback(std::function<void(double, double)> callback) = 0;
    virtual double get_report_time_delta() const = 0;
};


}
}
