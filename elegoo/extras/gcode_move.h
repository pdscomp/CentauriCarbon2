/*****************************************************************************
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:19
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-29 16:57:18
 * @Description  : G-Code G1 movement commands (and associated coordinate
 * manipulation)
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once

#include <memory>
#include <functional>
#include <map>
#include "json.h"
#include "bed_mesh.h"
#include "gcode_move_transform.h"
class ConfigWrapper;
class Printer;
class GCodeDispatch;
class MCU_stepper;
class ToolHead;
class GCodeCommand;
class PrinterRail;

namespace elegoo {
namespace extras {

class BedMesh;
class Homing;
class TemplateWrapper;


class G180
{
public:
    G180(std::shared_ptr<ConfigWrapper> config);
    ~G180();
    void cmd_G180(std::shared_ptr<GCodeCommand> gcmd);
private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<GCodeDispatch> gcode;
    std::vector<std::shared_ptr<TemplateWrapper>> tws;
};

class GCodeMove
{
public:
    GCodeMove(std::shared_ptr<ConfigWrapper> config);
    ~GCodeMove();
    std::shared_ptr<GCodeMoveTransform> set_move_transform(std::shared_ptr<GCodeMoveTransform> transform, bool force=false);
    json get_status(double eventtime=0);
    void reset_last_position();
    void cmd_G1(std::shared_ptr<GCodeCommand> gcmd, bool is_round=false);
    void cmd_G20(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_G21(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_M82(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_M83(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_G90(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_G91(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_G92(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_M114(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_M220(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_M221(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SET_GCODE_OFFSET(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SET_BED_ROUGHNESS_OFFSET(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SAVE_GCODE_STATE(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_RESTORE_GCODE_STATE(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_GET_POSITION(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_MANUAL_MOVE(std::shared_ptr<GCodeCommand> gcmd);

    void initialize_handlers();
    json get_gcode_state();
    void clean_gcode_state();
    void set_e_last_pos(double e_pos);
    void clear_gcode_speed();

private:
    void handle_ready();
    void handle_shutdown();
    void handle_activate_extruder();
    void handle_home_rails_end(std::shared_ptr<Homing> homing_state,
        std::vector<std::shared_ptr<PrinterRail>> rails);
    std::vector<double> get_gcode_position();
    double get_gcode_speed();
    double get_gcode_speed_override();
    std::vector<double> json_to_vector(const json& json_array);
    json _padding_position(const std::vector<double> &pos);

    std::shared_ptr<Printer> printer;
    std::shared_ptr<G180> g180;
    bool is_printer_ready;
    bool absolute_coord;
    bool absolute_extrude;
    std::vector<double> Coord;
    std::vector<double> base_position;
    std::vector<double> last_position;
    std::vector<double> homing_position;
    double speed;
    double speed_factor;
    double extrude_factor;
    json saved_states;
    std::shared_ptr<GCodeMoveTransform> move_transform;
    std::function<void(std::vector<double>&, double)> move_with_transform;
    std::function<std::vector<double>()> position_with_transform;
    double bed_roughness_offset;
    double z_offset;
};

std::shared_ptr<GCodeMove> gcode_move_load_config(std::shared_ptr<ConfigWrapper> config);


}
}