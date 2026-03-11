/***************************************************************************** 
 * @Author       : Louisa
 * @Date         : 2024-11-22 15:58:51
 * @LastEditors  : Ben
 * @LastEditTime : 2025-09-20 11:46:30
 * @Description  : Virtual SDCard print stat tracking
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory>
#include <string>
#include "json.h"
class ConfigWrapper;
class GCodeCommand;
class SelectReactor;
class Printer;
class GCodeDispatch;

namespace elegoo {
namespace extras {

class GCodeMove;
class SwitchSensor;
class PrinterServo;
class EncoderSensor;
class CoverSensor;
class TanglingSensor;

class PrintStats
{
public:
    PrintStats(std::shared_ptr<ConfigWrapper> config);
    ~PrintStats();

    void set_current_file(const std::string& filename);
    void note_start();
    void note_pause();
    void note_complete();
    void note_error(const std::string& message);
    void note_cancel();
    void cmd_SET_PRINT_STATS_INFO(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_M73(std::shared_ptr<GCodeCommand> gcmd);  // 自定义用来实现更新当前打印层数的命令
    void reset();
    json get_status(double eventtime);
    void set_print_duration_before_power_loss(double print_duration);
    void update_bed_mesh_status(int32_t exist);
private:
    void update_filament_usage(double eventtime);
    void note_finish(const std::string& state,
        const std::string& error_message="");
    void handle_ready();
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<GCodeMove> gcode_move;
    std::shared_ptr<SelectReactor> reactor;
    std::shared_ptr<GCodeDispatch> gcode;
    std::vector<std::shared_ptr<SwitchSensor>> all_sensors;
    std::shared_ptr<EncoderSensor> encoder_sensor;
    std::shared_ptr<CoverSensor> cover_sensor;
    std::shared_ptr<TanglingSensor> tangling_sensor;
    std::shared_ptr<PrinterServo> servo;
    std::string filename;
    std::string state;
    std::string error_message;
    double filament_used;
    double last_epos;
    double print_start_time;
    double last_pause_time;
    double prev_pause_duration;
    double total_duration;
    double init_duration;
    int info_total_layer;
    int info_current_layer;
    int32_t bed_mesh_exist = 0;
    double print_duration_before_power_loss;
};

std::shared_ptr<PrintStats> print_stats_load_config(std::shared_ptr<ConfigWrapper> config);

}
}