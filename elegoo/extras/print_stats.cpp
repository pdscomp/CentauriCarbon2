/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-20 11:46:20
 * @Description  : Virtual SDCard print stat tracking
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#include "print_stats.h"
#include "printer.h"
#include "gcode_move.h"
#include "filament_switch_sensor.h"
#include "servo.h"
#include "filament_motion_sensor.h"
#include "cover_switch_sensor.h"
#include "tangling_switch_sensor.h"

namespace elegoo {
namespace extras {

PrintStats::PrintStats(std::shared_ptr<ConfigWrapper> config)
{
SPDLOG_INFO("PrintStats init!");
    printer = config->get_printer();
    gcode_move =
        any_cast<std::shared_ptr<GCodeMove>>(printer->load_object(config, "gcode_move"));

    reactor = printer->get_reactor();
    print_duration_before_power_loss = 0;
    reset();
    gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));

    gcode->register_command("SET_PRINT_STATS_INFO", 
        [this](std::shared_ptr<GCodeCommand> gcmd){ 
            cmd_SET_PRINT_STATS_INFO(gcmd); 
        }, false, "Pass slicer info like layer act and " \
        "total to elegoo");

    gcode->register_command("M73", 
        [this](std::shared_ptr<GCodeCommand> gcmd){ 
            cmd_M73(gcmd); 
        }, false, "Update current_layer");

    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:ready",
        std::function<void()>([this](){
            SPDLOG_DEBUG("elegoo:ready !");
            handle_ready();
            SPDLOG_DEBUG("elegoo:ready !");
        })
    );
    SPDLOG_INFO("PrintStats init success!!");
}

PrintStats::~PrintStats()
{

}


void PrintStats::set_current_file(const std::string& filename)
{
    reset();
    this->filename = filename;
}

void PrintStats::note_start()
{
    double curtime = get_monotonic();
        
    if (std::isnan(print_start_time)) 
    {
        print_start_time = curtime;
    }
    else if (!std::isnan(last_pause_time)) 
    {
        double pause_duration = curtime - last_pause_time;
        prev_pause_duration += pause_duration;
        last_pause_time = DOUBLE_NONE;
    }

    json gc_status = gcode_move->get_status(curtime);
    last_epos = gc_status["position"].back().get<double>();
    state = "printing";
    error_message = "";
    elegoo::common::SignalManager::get_instance().emit_signal<std::string>(
        "elegoo:print_stats", state);
}

void PrintStats::note_pause()
{
    if (std::isnan(last_pause_time))
    {
        double curtime = get_monotonic();
        last_pause_time = curtime;
        update_filament_usage(curtime);
    }

    if (state != "error") 
    {
        state = "paused";
    }
    
    elegoo::common::SignalManager::get_instance().emit_signal<std::string>(
        "elegoo:print_stats", state);
}

void PrintStats::note_complete()
{
    note_finish("complete");
}

void PrintStats::note_error(const std::string& message)
{
    note_finish("error", message);
}

void PrintStats::note_cancel()
{
    note_finish("cancelled");
}

void PrintStats::cmd_SET_PRINT_STATS_INFO(std::shared_ptr<GCodeCommand> gcmd)
{
    int total_layer = gcmd->get_int("TOTAL_LAYER", info_total_layer, 0);
    int current_layer = gcmd->get_int("CURRENT_LAYER", info_current_layer, 0);

    if (total_layer == 0) 
    {
        info_total_layer = INT_NONE;
        info_current_layer = INT_NONE;
    } 
    else if (total_layer != info_total_layer) 
    {
        info_total_layer = total_layer;
        info_current_layer = 0;
    }

    // 如果 TOTAL_LAYER 和 CURRENT_LAYER 都有效且不相等，更新当前层数
    if (info_total_layer != INT_NONE && info_current_layer != INT_NONE &&
        current_layer != info_current_layer) 
    {
        info_current_layer = std::min(current_layer, info_total_layer);
    }
}

void PrintStats::cmd_M73(std::shared_ptr<GCodeCommand> gcmd)
{
    int current_layer = gcmd->get_int("L", -1, 0);    // 当前层数
    if (current_layer != -1) {
        SPDLOG_INFO("{} current_layer:{}",__func__,current_layer);
        info_current_layer = current_layer;
    }
    
}

void PrintStats::reset()
{
    filename = error_message = "";
    state = "standby";
    prev_pause_duration = last_epos = 0.;
    filament_used = total_duration = 0.;
    print_duration_before_power_loss = 0;
    print_start_time = last_pause_time = DOUBLE_NONE;
    init_duration = 0.;
    info_total_layer = INT_NONE;
    info_current_layer = INT_NONE;
    elegoo::common::SignalManager::get_instance().emit_signal<std::string>(
        "elegoo:print_stats", state);
}

json PrintStats::get_status(double eventtime)
{
    double time_paused = prev_pause_duration;

    if (!std::isnan(print_start_time)) 
    {
        if (!std::isnan(last_pause_time)) 
        {
            time_paused += eventtime - last_pause_time;
        } 
        else 
        {
            update_filament_usage(eventtime);
        }

        total_duration = eventtime - print_start_time;

        if (filament_used < 0.0000001f)
        {
            init_duration = total_duration - time_paused;
        }
    }


    json status;
    if(all_sensors.size() > 0) {
        status = all_sensors.at(0)->get_status(0);
    }

    if(cover_sensor)
    {
        status["canvas_nozzle_fan_off"] = cover_sensor->get_status(0)["front_cover_detected"];
    }
    if(tangling_sensor)
    {
        status["canvas_wrap_filament"] = tangling_sensor->get_status(0)["tangling_detected"];
    }
    if(encoder_sensor)
    {
        status["canvas_locked_rotor"] = encoder_sensor->get_status(0)["filament_detected"];
    }

    if(servo)
    {
        status["cavity_temperature_mode"] = servo->get_status(0)["mode"];
    }
    status["filename"] = filename;
    status["total_duration"] = total_duration + print_duration_before_power_loss;
    status["print_duration"] = total_duration + print_duration_before_power_loss - init_duration - time_paused;
    status["filament_used"] = filament_used;
    status["state"] = state;
    status["message"] = error_message;
    if(info_total_layer == INT_NONE)
        status["info"]["total_layer"] = json::value_t::null;
    else
        status["info"]["total_layer"] = info_total_layer;

    if(info_current_layer == INT_NONE)
        status["info"]["current_layer"] = 0;
    else
        status["info"]["current_layer"] = info_current_layer;
    status["bed_mesh_detected"] = bed_mesh_exist;
    status["print_duration_before_power_loss"] = print_duration_before_power_loss;
    return status;
}

void PrintStats::set_print_duration_before_power_loss(double print_duration) 
{
    print_duration_before_power_loss = print_duration;
}

void PrintStats::update_filament_usage(double eventtime)
{
    json gc_status = gcode_move->get_status(eventtime);
    double cur_epos = gc_status["position"].back().get<double>();
    filament_used += (cur_epos - last_epos) / gc_status["extrude_factor"].get<double>();
    last_epos = cur_epos;
}

void PrintStats::update_bed_mesh_status(int32_t exist)
{
    bed_mesh_exist = exist;
    SPDLOG_INFO("bed_mesh_exist:0x{:04x}", bed_mesh_exist);
}

void PrintStats::note_finish(const std::string& state,
    const std::string& error_message)
{
    if (std::isnan(print_start_time)) 
    {
        return;
    }

    this->state = state;
    this->error_message = error_message;

    double eventtime = get_monotonic();
    total_duration = eventtime - print_start_time;

    if (filament_used < 0.0000001f) 
    {
        init_duration = total_duration - prev_pause_duration;
    }
    print_start_time = DOUBLE_NONE;

    elegoo::common::SignalManager::get_instance().emit_signal<std::string>(
        "elegoo:print_stats", state);
}

void PrintStats::handle_ready() 
{
    std::map<std::string, Any> sensor_map = printer->lookup_objects("filament_switch_sensor");
    for (auto &pair : sensor_map) {
        all_sensors.push_back(any_cast<std::shared_ptr<SwitchSensor>>(pair.second));
    }
    servo = any_cast<std::shared_ptr<PrinterServo>>(printer->lookup_object("servo",std::shared_ptr<PrinterServo>()));
    encoder_sensor = any_cast<std::shared_ptr<EncoderSensor>>(printer->lookup_object("z_filament_motion_sensor encoder_sensor",std::shared_ptr<EncoderSensor>()));
    cover_sensor = any_cast<std::shared_ptr<CoverSensor>>(printer->lookup_object("cover_switch_sensor",std::shared_ptr<CoverSensor>()));
    tangling_sensor = any_cast<std::shared_ptr<TanglingSensor>>(printer->lookup_object("tangling_switch_sensor",std::shared_ptr<TanglingSensor>()));
}


std::shared_ptr<PrintStats> print_stats_load_config(std::shared_ptr<ConfigWrapper> config) {
    return std::make_shared<PrintStats>(config);
}

}
}