/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-12-16 17:45:45
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-20 10:39:27
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/
#pragma once

#include "any.h"
#include "json.h"
#include <memory>
#include <map>
#include <functional>
#include "adc_scaled.h"
#include "adc_temperature.h"
#include "controller_fan.h"
#include "error_mcu.h"
#include "exclude_object.h"
#include "extruder_stepper.h"
#include "fan_generic.h"
#include "cavity_fan.h"
#include "fan.h"
#include "force_move.h"
#include "gcode_arcs.h"
#include "gcode_macro.h"
#include "heater_bed.h"
#include "heater_fan.h"
#include "heaters.h"
#include "homing.h"
#include "output_pin.h"
#include "pwm_cycle_time.h"
#include "query_adc.h"
#include "safe_z_home.h"
#include "save_variables.h"
#include "sdcard_loop.h"
#include "tmc2209.h"
#include "tuning_tower.h"
#include "idle_timeout.h"
#include "virtual_sdcard.h"
#include "thermistor.h"
#include "stepper_enable.h"
#include "bed_mesh.h"
#include "canbus_ids.h"
#include "print_stats.h"
#include "homing_heaters.h"
#include "homing_override.h"
#include "motion_report.h"
#include "pause_resume.h"
#include "pid_calibrate.h"
#include "resonance_tester.h"
#include "probe.h"
#include "load_cell.h"
#include "load_cell_probe.h"
#include "hx71x.h"
#include "statistics.h"
#include "adxl345.h"
#include "lis2dh.h"
#include "lis2dw.h"
#include "input_shaper.h"
#include "buttons.h"
#include "filament_switch_sensor.h"
#include "por.h"
#include "z_compensation.h"
#include "filament_load_unload.h"
#include "mmu.h"
#include "tangling_switch_sensor.h"
#include "cover_switch_sensor.h"


class ConfigWrapper;

namespace elegoo {
namespace extras {


typedef std::shared_ptr<void> (*create_fn)(std::shared_ptr<ConfigWrapper>);

class ExtrasFactory 
{
public:
    static Any create_extras(//临时用法
        const std::string& extras_name,
        std::shared_ptr<ConfigWrapper> config);

};

json getStatus(Any object,const std::string&object_name, double eventtime);     // 计算Any类型的object对应的类型的get_status() 函数返回值
json getDefaultSubscribeStatus(std::shared_ptr<Printer> printer);   // 获取默认的订阅模块状态

std::function<std::pair<bool, std::string>(double)> get_stats_function(Any object);

}
}