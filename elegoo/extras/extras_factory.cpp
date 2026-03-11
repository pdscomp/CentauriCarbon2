/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-12-16 17:46:01
 * @LastEditors  : loping
 * @LastEditTime : 2025-08-25 21:49:39
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "extras_factory.h"
#include "logger.h"
#include "auto_detect.h"
#include "update_ota.h"
#include "temperature_sensor.h"
#include "verify_heater.h"
#include "product_test.h"
#include "filament_motion_sensor.h"
#include "servo.h"

namespace elegoo {
namespace extras {

Any ExtrasFactory::create_extras(
    const std::string& extras_name,
    std::shared_ptr<ConfigWrapper> config)
{
    // SPDLOG_INFO("__func__:{},extras_name:{}",__func__,extras_name);
    if (extras_name == "adc_scaled_load_config_prefix")
    {
        return adc_scaled_load_config_prefix(config);
    }
    if (extras_name == "adc_temperature_load_config")
    {
        return adc_temperature_load_config(config);
    }
    if (extras_name == "adc_temperature_load_config_prefix")
    {
        return adc_temperature_load_config_prefix(config);
    }
    else if (extras_name == "controller_fan_load_config_prefix")
    {
        return controller_fan_load_config_prefix(config);
    }
    else if (extras_name == "error_mcu_load_config")
    {
        return error_mcu_load_config(config);
    }
    else if (extras_name == "exclude_object_load_config")
    {
        return exclude_object_load_config(config);
    }
    else if (extras_name == "extruder_stepper_load_config_prefix")
    {
        return extruder_stepper_load_config_prefix(config);
    }
    else if (extras_name == "fan_generic_load_config_prefix")
    {
        return fan_generic_load_config_prefix(config);
    }
    else if (extras_name == "cavity_fan_load_config")
    {
        return cavity_fan_load_config(config);
    }
    else if (extras_name == "fan_load_config")
    {
        return fan_load_config(config);
    }
    else if (extras_name == "force_move_load_config")
    {
        return force_move_load_config(config);
    }
    else if (extras_name == "gcode_arcs_load_config")
    {
        return gcode_arcs_load_config(config);
    }
    else if(extras_name == "gcode_macro_load_config")
    {
        return gcode_macro_load_config(config);
    }
    else if(extras_name == "gcode_macro_load_config_prefix")
    {
        return gcode_macro_load_config_prefix(config);
    }
    else if (extras_name == "gcode_move_load_config")
    {
        return gcode_move_load_config(config);
    }
    else if (extras_name == "heater_bed_load_config")
    {
        return heater_bed_load_config(config);
    }
    else if (extras_name == "heater_fan_load_config_prefix")
    {
        return heater_fan_load_config_prefix(config);
    }
    else if (extras_name == "heaters_load_config")
    {
        return heaters_load_config(config);
    }
    else if (extras_name == "homing_load_config")
    {
        return homing_load_config(config);
    }
    else if (extras_name == "output_pin_load_config_prefix")
    {
        return output_pin_load_config_prefix(config);
    }
    else if (extras_name == "pwm_cycle_time_load_config_prefix")
    {
        return pwm_cycle_time_load_config_prefix(config);
    }
    else if (extras_name == "query_adc_load_config")
    {
        return query_adc_load_config(config);
    }
    else if (extras_name == "safe_z_home_load_config")
    {
        return safe_z_home_load_config(config);
    }
    else if (extras_name == "save_variables_load_config")
    {
        return save_variables_load_config(config);
    }
    else if (extras_name == "sdcard_loop_load_config")
    {
        return sdcard_loop_load_config(config);
    }
    else if (extras_name == "tmc2209_load_config_prefix")
    {
        return tmc2209_load_config_prefix(config);
    }
    else if (extras_name == "tuning_tower_load_config")
    {
        return tuning_tower_load_config(config);
    }
    else if (extras_name == "idle_timeout_load_config")
    {
        return idle_timeout_load_config(config);
    }
    else if (extras_name == "virtual_sdcard_load_config")
    {
        return virtual_sdcard_load_config(config);
    }
    else if (extras_name == "thermistor_load_config_prefix")
    {
        return thermistor_load_config_prefix(config);
    }
    else if (extras_name == "bed_mesh_load_config")
    {
        return bed_mesh_load_config(config);
    }
    else if(extras_name == "buttons_load_config")
    {
        return buttons_load_config(config);
    }
    else if (extras_name == "canbus_ids_load_config")
    {
        return canbus_ids_load_config(config);
    }
    else if (extras_name == "stepper_enable_load_config")
    {
        return stepper_enable_load_config(config);
    }
    else if (extras_name == "print_stats_load_config")
    {
        return print_stats_load_config(config);
    }
    else if (extras_name == "homing_heaters_load_config_prefix")
    {
        return homing_heaters_load_config_prefix(config);
    }
    else if (extras_name == "homing_override_load_config")
    {
        return homing_override_load_config(config);
    }
    else if (extras_name == "motion_report_load_config")
    {
        return motion_report_load_config(config);
    }
    else if (extras_name == "pause_resume_load_config")
    {
        return pause_resume_load_config(config);
    }
    else if (extras_name == "pid_calibrate_load_config")
    {
        return pid_calibrate_load_config(config);
    }
    // else if (extras_name == "resonance_tester_load_config_prefix")
    // {
    //     return resonance_tester_load_config_prefix(config);
    // }
    else if (extras_name == "probe_load_config")
    {
        return probe_load_config(config);
    }
    else if (extras_name == "load_cell_load_config")
    {
        return load_cell_load_config(config);
    }
    else if (extras_name == "load_cell_load_config_prefix")
    {
        return load_cell_load_config_prefix(config);
    }
    else if (extras_name == "load_cell_probe_load_config")
    {
        return load_cell_probe_load_config(config);
    }
    else if (extras_name == "statistics_load_config")
    {
        return statistics_load_config(config);
    }
    else if("resonance_tester_load_config" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return resonance_tester_load_config(config);
    }
    else if("adxl345_load_config_prefix" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return adxl345_load_config_prefix(config);
    }
    else if("lis2dw_load_config" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return lis2dw_load_config(config);
    }
    else if("lis2dh_load_config" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return lis2dh_load_config(config);
    }
    else if("input_shaper_load_config" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return input_shaper_load_config(config);
    }
    else if("rfauto_detect_load_config" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return rfauto_detect_load_config(config);
    }
    // else if("canvas_dev_load_config" == extras_name)
    // {
    //     SPDLOG_DEBUG("__func__:{} #1",__func__);
    //     return canvas_dev_load_config(config);
    // }
    else if("canvas_dev_load_config" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return canvas_dev_load_config(config);
    }
    else if("servo_load_config_prefix" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return servo_load_config_prefix(config);
    }
    else if("zproduct_test_load_config" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return zproduct_test_load_config(config);
    }
    // else if("update_ota_load_config" == extras_name)
    // {
    //     SPDLOG_DEBUG("__func__:{} #1",__func__);
    //     return update_ota_load_config(config);
    // }
    else if("ztemperature_sensor_load_config_prefix" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return ztemperature_sensor_load_config_prefix(config);
    }
    else if("verify_heater_load_config_prefix" == extras_name)
    {
        SPDLOG_DEBUG("__func__:{} #1",__func__);
        return verify_heater_load_config_prefix(config);
    }
    else if (extras_name == "filament_switch_sensor_load_config_prefix")
    {
        return switch_sensor_load_config_prefix(config);
    }
    else if (extras_name == "z_filament_motion_sensor_load_config_prefix")
    {
        return encoder_sensor_load_config_prefix(config);
    }
    else if("por_load_config" == extras_name)
    {
        return por_load_config(config);
    }
    else if("z_compensation_load_config" == extras_name)
    {
        return z_compensation_load_config(config);
    }
    else if("filament_load_unload_load_config" == extras_name)
    {
        return filament_load_unload_load_config(config);
    }
    else if("cover_switch_sensor_load_config" == extras_name)
    {
        return cover_switch_sensor_load_config(config);
    }
    else if("tangling_switch_sensor_load_config" == extras_name)
    {
        return tangling_switch_sensor_load_config(config);
    }
    else
    {
        SPDLOG_WARN("create extras: \"{}\" failer! ", extras_name);
    }
    return Any();
}

json getStatus(Any object, const std::string&object_name, double eventtime) {

    if (object.empty())
    {
        SPDLOG_ERROR("getStatus: Failed to get object #1 {}", object_name);
        return json::object();
    }
    json status;
    if (object.type() == typeid(std::shared_ptr<ToolHead>)) {
        // SPDLOG_DEBUG("ToolHead->get_status");
        status = any_cast<std::shared_ptr<ToolHead>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<WebHooks>)) {
        // SPDLOG_DEBUG("WebHooks->get_status");
        status = any_cast<std::shared_ptr<WebHooks>>(object)->get_status();
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterConfig>)) {
        // SPDLOG_DEBUG("PrinterConfig->get_status");
        status = any_cast<std::shared_ptr<PrinterConfig>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterPins>)) {
        // SPDLOG_DEBUG("PrinterPins->get_status");
        // status = any_cast<std::shared_ptr<PrinterPins>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<MCU>)) {
        // SPDLOG_DEBUG("MCU->get_status");
        status = any_cast<std::shared_ptr<MCU>>(object)->get_status();
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterExtruder>)) {
        // SPDLOG_DEBUG("PrinterExtruder->get_status");
        status = any_cast<std::shared_ptr<PrinterExtruder>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<ControllerFan>)) {
        // SPDLOG_DEBUG("ControllerFan->get_status");
        status = any_cast<std::shared_ptr<ControllerFan>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<ExcludeObject>)) {
        // SPDLOG_DEBUG("ExcludeObject->get_status");
        status = any_cast<std::shared_ptr<ExcludeObject>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterExtruderStepper>)) {
        // SPDLOG_DEBUG("PrinterExtruderStepper->get_status");
        status = any_cast<std::shared_ptr<PrinterExtruderStepper>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterFanGeneric>)) {
        // SPDLOG_DEBUG("PrinterFanGeneric->get_status");
        status = any_cast<std::shared_ptr<PrinterFanGeneric>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterFanCavity>)) {
        // SPDLOG_DEBUG("PrinterFanCavity->get_status");
        status = any_cast<std::shared_ptr<PrinterFanCavity>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterFan>)) {
        // SPDLOG_DEBUG("PrinterFan->get_status");
        status = any_cast<std::shared_ptr<PrinterFan>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<GCodeDispatch>)) {
        // SPDLOG_DEBUG("GCodeDispatch->get_status");
        status = any_cast<std::shared_ptr<GCodeDispatch>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<GCodeMacro>)) {
        // SPDLOG_DEBUG("GCodeMacro->get_status");
        status = any_cast<std::shared_ptr<GCodeMacro>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<GCodeMove>)) {
        // SPDLOG_DEBUG("GCodeMove->get_status");
        status = any_cast<std::shared_ptr<GCodeMove>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterHeaterBed>)) {
        // SPDLOG_DEBUG("PrinterHeaterBed->get_status");
        status = any_cast<std::shared_ptr<PrinterHeaterBed>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterHeaterFan>)) {
        // SPDLOG_DEBUG("PrinterHeaterFan->get_status");
        status = any_cast<std::shared_ptr<PrinterHeaterFan>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterHeaters>)) {
        // SPDLOG_DEBUG("PrinterHeaters->get_status");
        status = any_cast<std::shared_ptr<PrinterHeaters>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<Heater>)) {
        status = any_cast<std::shared_ptr<Heater>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterOutputPin>)) {
        // SPDLOG_DEBUG("PrinterOutputPin->get_status");
        status = any_cast<std::shared_ptr<PrinterOutputPin>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterOutputPWMCycle>)) {
        // SPDLOG_DEBUG("PrinterOutputPWMCycle->get_status");
        status = any_cast<std::shared_ptr<PrinterOutputPWMCycle>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<SaveVariables>)) {
        // SPDLOG_DEBUG("SaveVariables->get_status");
        status = any_cast<std::shared_ptr<SaveVariables>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<IdleTimeout>)) {
        // SPDLOG_DEBUG("IdleTimeout->get_status");
        status = any_cast<std::shared_ptr<IdleTimeout>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<VirtualSD>)) {
        // SPDLOG_DEBUG("VirtualSD->get_status");
        status = any_cast<std::shared_ptr<VirtualSD>>(object)->get_status();
    }
    else if (object.type() == typeid(std::shared_ptr<BedMesh>)) {
        status = any_cast<std::shared_ptr<BedMesh>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterStepperEnable>)) {
        // SPDLOG_DEBUG("PrinterStepperEnable->get_status");
        status = any_cast<std::shared_ptr<PrinterStepperEnable>>(object)->get_status();
    }
    else if (object.type() == typeid(std::shared_ptr<PrintStats>)) {
        // SPDLOG_DEBUG("PrintStats->get_status");
        status = any_cast<std::shared_ptr<PrintStats>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PauseResume>)) {
        // SPDLOG_DEBUG("PauseResume->get_status");
        status = any_cast<std::shared_ptr<PauseResume>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterSensorGeneric>)) {
        // SPDLOG_DEBUG("PrinterSensorGeneric->get_status");
        status = any_cast<std::shared_ptr<PrinterSensorGeneric>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterProbeInterface>)) {
        status = any_cast<std::shared_ptr<PrinterProbeInterface>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterMotionReport>)) {
        status = any_cast<std::shared_ptr<PrinterMotionReport>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrinterSysStats>)) {
        status = any_cast<std::shared_ptr<PrinterSysStats>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<PrintStats>)) {
        status = any_cast<std::shared_ptr<PrintStats>>(object)->get_status(eventtime);
    }
    else if (object.type() == typeid(std::shared_ptr<Canvas>)) {
        status = any_cast<std::shared_ptr<Canvas>>(object)->get_status(eventtime);
    }
    else
    {
        // SPDLOG_WARN("No get_status API!");
        SPDLOG_DEBUG("getStatus: Failed to get object #2 {}", object_name);
    }
    return status;

}

json getDefaultSubscribeStatus(std::shared_ptr<Printer> printer)
{
    // SPDLOG_INFO("getDefaultSubscribeStatus");
    json total_status;
    double eventtime = get_monotonic();

    // std::shared_ptr<Printer> printer = config->get_printer();
    Any object = printer->lookup_object("toolhead");
    if (!object.empty()) {
        total_status["toolhead"] = any_cast<std::shared_ptr<ToolHead>>(object)->get_status(eventtime);
    }
    
    object = printer->lookup_object("fan");
    if (!object.empty()) {
        total_status["fan"] = any_cast<std::shared_ptr<PrinterFan>>(object)->get_status(eventtime);
    }

    object = printer->lookup_object("heater_fan heatbreak_cooling_fan");
    if (!object.empty()) {
        total_status["heater_fan"] = any_cast<std::shared_ptr<PrinterHeaterFan>>(object)->get_status(eventtime);
    }
    
    object = printer->lookup_object("controller_fan board_cooling_fan");
    if (!object.empty()) {
        total_status["controller_fan"] = any_cast<std::shared_ptr<ControllerFan>>(object)->get_status(eventtime);
    }
    
    object = printer->lookup_object("cavity_fan");
    if (!object.empty()) {
        total_status["fan_generic"] = any_cast<std::shared_ptr<PrinterFanCavity>>(object)->get_status(eventtime);
    }

    object = printer->lookup_object("fan_generic fan1");
    if (!object.empty()) {
        total_status["aux_fan"] = any_cast<std::shared_ptr<PrinterFanGeneric>>(object)->get_status(eventtime);
    }
    
    object = printer->lookup_object("ztemperature_sensor box");
    if (!object.empty()) {
        total_status["ztemperature_sensor"] = any_cast<std::shared_ptr<PrinterSensorGeneric>>(object)->get_status(eventtime);
    }

    object = printer->lookup_object("extruder");
    if(!object.empty()) {
        total_status["extruder"] = any_cast<std::shared_ptr<PrinterExtruder>>(object)->get_status(eventtime);
    }

    object = printer->lookup_object("heater_bed");
    if(!object.empty()) {
        total_status["heater_bed"] = any_cast<std::shared_ptr<PrinterHeaterBed>>(object)->get_status(eventtime);
    }

    object = printer->lookup_object("canvas_dev");
    if (!object.empty()) {
        total_status["canvas_dev"] = any_cast<std::shared_ptr<Canvas>>(object)->get_status(eventtime);
    }

    // object = printer->lookup_object("heater_generic");
    // if(!object.empty()) {
    //     total_status["heater_generic"] = any_cast<std::shared_ptr<PrinterHeaters>>(object)->get_status(eventtime);
    // } 一开始访问会有段错误

    object = printer->lookup_object("print_stats");
    if(!object.empty()) {
        total_status["print_stats"] = any_cast<std::shared_ptr<PrintStats>>(object)->get_status(eventtime);
    }

    object = printer->lookup_object("virtual_sdcard");
    if(!object.empty()) {
        total_status["virtual_sdcard"] = any_cast<std::shared_ptr<VirtualSD>>(object)->get_status();
    }

    object = printer->lookup_object("gcode_move");
    if(!object.empty()) {
        total_status["gcode_move"] = any_cast<std::shared_ptr<GCodeMove>>(object)->get_status(eventtime);
    }

    object = printer->lookup_object("output_pin led_pin");
    if(!object.empty()) {
        total_status["led_pin"] = any_cast<std::shared_ptr<PrinterOutputPin>>(object)->get_status(eventtime);
    }

    // SPDLOG_INFO("total_status {}", total_status.dump());
    return total_status;
}

//目前只调试了toolhead->stats()接口，其他模块还未调试，打开注释使用有风险。
std::function<std::pair<bool, std::string>(double)> get_stats_function(Any object) {
    if (object.empty()) {
        return std::function<std::pair<bool, std::string>(double)>();
    }

    std::function<std::pair<bool, std::string>(double)> func;

    if (object.type() == typeid(std::shared_ptr<ToolHead>)) {
        // SPDLOG_DEBUG("ToolHead->stats()");
        std::shared_ptr<ToolHead> toolhead = any_cast<std::shared_ptr<ToolHead>>(object);
        func = [toolhead](double eventtime) {
            return toolhead->stats(eventtime);
        };
    } else if (object.type() == typeid(std::shared_ptr<WebHooks>)) {
        // SPDLOG_DEBUG("WebHooks->stats()");
        std::shared_ptr<WebHooks> webhooks = any_cast<std::shared_ptr<WebHooks>>(object);
        func = [webhooks](double eventtime) {
            return webhooks->stats(eventtime);
        };
    } else if (object.type() == typeid(std::shared_ptr<MCU>)) {
        std::shared_ptr<MCU> mcu = any_cast<std::shared_ptr<MCU>>(object);
        func = [mcu](double eventtime) {
            return mcu->stats(eventtime);
        };
    } else if (object.type() == typeid(std::shared_ptr<Heater>)) {
        // SPDLOG_DEBUG("Heater->stats()");
        std::shared_ptr<Heater> heaters = any_cast<std::shared_ptr<Heater>>(object);
        func = [heaters](double eventtime) {
            return heaters->stats(eventtime);
        };
    } else if (object.type() == typeid(std::shared_ptr<PrinterSysStats>)) {
        // SPDLOG_DEBUG("PrinterSysStats->stats()");
        std::shared_ptr<PrinterSysStats> printer_sys_stats = any_cast<std::shared_ptr<PrinterSysStats>>(object);
        func = [printer_sys_stats](double eventtime) {
            return printer_sys_stats->stats(eventtime);
        };
    } else if (object.type() == typeid(std::shared_ptr<VirtualSD>)) {
        // SPDLOG_DEBUG("VirtualSD->stats()");
        std::shared_ptr<VirtualSD> virtual_sd = any_cast<std::shared_ptr<VirtualSD>>(object);
        func = [virtual_sd](double eventtime) {
            return virtual_sd->stats(eventtime);
        };
    } else if (object.type() == typeid(std::shared_ptr<PrinterExtruder>)) {
        // SPDLOG_DEBUG("PrinterExtruder->stats()");
        std::shared_ptr<PrinterExtruder> extruder = any_cast<std::shared_ptr<PrinterExtruder>>(object);
        func = [extruder](double eventtime) {
            return extruder->stats(eventtime);
        };
    }else if (object.type() == typeid(std::shared_ptr<PrinterHeaterBed>)) {
        // SPDLOG_DEBUG("PrinterHeaterBed->stats()");
        std::shared_ptr<PrinterHeaterBed> heater_bed = any_cast<std::shared_ptr<PrinterHeaterBed>>(object);
        func = [heater_bed](double eventtime) {
            return heater_bed->stats(eventtime);
        };
    }
    else
    {

    }
    return func;
}

}
}