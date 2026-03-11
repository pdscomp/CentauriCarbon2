/***************************************************************************** 
 * @Author       : Ben
 * @Date         : 2024-11-28 20:15:20
 * @LastEditors  : Ben
 * @LastEditTime : 2025-04-11 16:56:45
 * @Description  : Code for handling printer nozzle extruders
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#pragma once
#include <memory>
#include "json.h"
#include "c_helper.h"
#include "heaters.h"
#include "gcode_macro.h"

class ConfigWrapper;
class Printer;
class MCU_stepper;
class stepper_kinematics;
class GCodeCommand;
class PrinterExtruder;

namespace elegoo {
    namespace extras {
        class Heater;
        class PrinterHeaters;
    }
}
class Move;

class ExtruderStepper
{
public:
    ExtruderStepper(std::shared_ptr<ConfigWrapper> config);
    ~ExtruderStepper();

    json get_status(double eventtime);
    double find_past_position(double print_time);
    void sync_to_extruder(const std::string& extruder_name);
    void cmd_default_SET_PRESSURE_ADVANCE(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SET_PRESSURE_ADVANCE(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SET_E_ROTATION_DISTANCE(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SYNC_EXTRUDER_MOTION(std::shared_ptr<GCodeCommand> gcmd);
    std::shared_ptr<MCU_stepper> stepper;
private:
    void handle_connect();
    void set_pressure_advance(double pressure_advance, double smooth_time);

private:
    std::shared_ptr<Printer> printer;
    std::shared_ptr<stepper_kinematics> sk_extruder;
    double pressure_advance;
    double pressure_advance_smooth_time;
    double config_pa;
    double config_smooth_time;
    std::string name;
    std::string motion_queue;
};

class DummyExtruder : public std::enable_shared_from_this<DummyExtruder>
{
public:
    DummyExtruder(std::shared_ptr<Printer> printer);
    virtual ~DummyExtruder();

    virtual void update_move_time(double flush_time, double clear_history_time);
    virtual void check_move(Move* move);
    virtual double find_past_position(double print_time);
    virtual double calc_junction(Move* prev_move, Move* move);
    virtual std::string get_name();
    virtual std::shared_ptr<elegoo::extras::Heater> get_heater();
    virtual std::shared_ptr<trapq> get_trapq();
    virtual void cmd_M104(std::shared_ptr<GCodeCommand> gcmd, bool wait=false);
    virtual void cmd_M109(std::shared_ptr<GCodeCommand> gcmd);
    virtual void cmd_ACTIVATE_EXTRUDER(std::shared_ptr<GCodeCommand> gcmd);
    virtual json get_status(double eventtime);
    virtual std::pair<bool, std::string> stats(double eventtime);
    virtual void move(double print_time, Move* move);
protected:
public:
    std::shared_ptr<Printer> printer;
    double last_position;
    std::shared_ptr<ExtruderStepper> extruder_stepper;
};

class PrinterExtruder : public DummyExtruder
{
public:
    PrinterExtruder(std::shared_ptr<ConfigWrapper> config,
        int extruder_num);
    ~PrinterExtruder();

    void update_move_time(double flush_time, double clear_history_time);
    json get_status(double eventtime);
    std::string get_name();
    std::shared_ptr<elegoo::extras::Heater> get_heater();
    std::shared_ptr<trapq> get_trapq();
    std::pair<bool, std::string> stats(double eventtime);
    void check_move(Move* move);
    double calc_junction(Move* prev_move, Move* move);
    void move(double print_time, Move* move);
    double find_past_position(double print_time);
    void cmd_M104(std::shared_ptr<GCodeCommand> gcmd, bool wait=false);
    void cmd_M109(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_ACTIVATE_EXTRUDER(std::shared_ptr<GCodeCommand> gcmd);

private:
    std::shared_ptr<elegoo::extras::Heater> heater;
    std::shared_ptr<trapq> tra;
    std::string name;
    double nozzle_diameter;
    double filament_area;
    double max_extrude_ratio;
    double max_e_velocity;
    double max_e_accel;
    double max_e_dist;
    double instant_corner_v;
};
