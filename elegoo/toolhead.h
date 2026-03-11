/*****************************************************************************
 * @Author       : Gary
 * @Date         : 2024-11-19 05:57:38
 * @LastEditors  : Gary
 * @LastEditTime : 2024-11-26 20:14:02
 * @Description  : The toolhead module in Elegoo is responsible for managing
 * the printer's extruder (or toolhead) and its related functions.
 * It coordinates the movement of the extruder, handles temperature control,
 * and provides status queries, ensuring that the printer can perform printing
 * tasks efficiently and accurately.
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#ifndef TOOLHEAD_H
#define TOOLHEAD_H
#include <vector>
#include <stdexcept>
#include <algorithm>
#include "configfile.h"
#include "reactor.h"
#include "gcode.h"
#include "mcu.h"
#include "c_helper.h"
#include <string>
#include <exception>
#include "extruder.h"
#include "trapq.h"
#include "gcode_move_transform.h"
#include "exception_handler.h"

typedef void (*TrapqAppendFunc)(struct trapq *tq, double print_time,
                                double accel_t, double cruise_t, double decel_t,
                                double start_pos_x, double start_pos_y, double start_pos_z,
                                double axes_r_x, double axes_r_y, double axes_r_z,
                                double start_v, double cruise_v, double accel);

typedef void (*TrapqFinalizeMovesFunc)(trapq *tq, double print_time, double clear_history_time);

class LookAheadQueue;
class ToolHead;
class ObjectFactory;
class Move;
class CartKinematics;
class PrinterExtruder;
class Kinematics;
class DummyExtruder;

typedef struct
{
    double print_time;
    double est_print_time;
    bool lookahead_empty;
} Busy;

class ToolHead : public std::enable_shared_from_this<ToolHead>
{
public:
    ToolHead(std::shared_ptr<ConfigWrapper> config);
    ~ToolHead();

public:
    std::shared_ptr<Printer> printer;
    std::vector<std::shared_ptr<MCU>> all_mcus;
    std::shared_ptr<DummyExtruder> extruder;
    std::shared_ptr<LookAheadQueue> lookahead;
    std::shared_ptr<MCU> mcu;
    std::vector<double> commanded_pos;
    double max_velocity, min_cruise_ratio, req_accel_to_decel, square_corner_velocity, check_stall_time, need_check_pause, print_time;
    int print_stall;
    double junction_deviation, max_accel_to_decel;
    bool can_pause;
    double max_accel;
    std::shared_ptr<ReactorTimer> priming_timer;
    std::shared_ptr<ReactorTimer> flush_timer;
    std::shared_ptr<ReactorCompletion> drip_completion;
    std::string kin_name;
    std::vector<std::string> modules;
    std::vector<std::function<void(double)>> step_generators;
    std::string special_queuing_state;
    std::shared_ptr<Kinematics> kin;
    std::shared_ptr<GCodeDispatch> gcode;
    std::vector<double> Coord;
    std::shared_ptr<trapq> tra;
    TrapqAppendFunc trapq_append_func;
    TrapqFinalizeMovesFunc trapq_finalize_moves_func;
    std::shared_ptr<GCodeMoveTransform> move_transform;
private:
    std::function<double(double)> _handle_shutdow;
    void _advance_flush_time(double flush_time);
    void _advance_move_time(double next_print_time);
    void _calc_print_time(void);
    void _flush_lookahead(void);
    void _check_pause();
    double _priming_handler(double ettime);
    double _flush_handler(double eventtime);
    std::vector<double> kin_flush_times;
    double kin_flush_delay = 0.0;
    double need_flush_time = 0.0;
    double step_gen_time = 0.0;
    double clear_history_time = 0.0;
    bool do_kick_flush_timer = false;
    double last_flush_time, min_restart_time;
    std::shared_ptr<SelectReactor> reactor;
    void _calc_junction_deviation();
    void _handle_shutdown(void);
    std::map<std::string, std::shared_ptr<void>> load_obj;
    std::shared_ptr<ConfigWrapper> config;

public:
    void init();
    void _update_drip_move_time(double next_print_time);
    void note_mcu_movequeue_activity(double mq_time, bool set_step_gen_time = false);
    void note_step_generation_scan_time(double delay, double old_delay);
    void register_lookahead_callback(std::function<void(double)> callback);
    void cmd_G4(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_M400(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_M204(std::shared_ptr<GCodeCommand> gcmd);
    void cmd_SET_VELOCITY_LIMIT(std::shared_ptr<GCodeCommand> gcmd);
    std::pair<double, double> get_max_velocity();
    std::vector<double> get_position();
    void set_position(const std::vector<double> &newpos, const std::vector<int> &homing_axes = {});
    void limit_next_junction_speed(double speed);
    void move(const std::vector<double> &, double);
    void manual_move(const std::vector<double> &, const double &);
    void set_extruder(std::shared_ptr<DummyExtruder> extruder, double exturder_pos);
    std::shared_ptr<DummyExtruder> get_extruder();
    void drip_move(std::vector<double> &newpos, double speed, std::shared_ptr<ReactorCompletion> drip);
    std::pair<bool, std::string> stats(double event_time);
    Busy check_busy(double eventtime);
    json get_status(double eventtime);
    std::shared_ptr<Kinematics> get_kinematic();
    std::shared_ptr<trapq> get_trapq();
    void register_step_generator(std::function<void(double)> handler);
    std::string get_extruder_name();
    void _process_moves(std::vector<Move *> moves);
    void flush_step_generation();
    void dwell(double delay);
    double get_last_move_time();
    void wait_moves();
};

class Move
{
public:
    Move(std::shared_ptr<ToolHead> th, std::vector<double> s_pos, std::vector<double> e_pos, double spd);
    ~Move();
    void limit_speed(double, double);
    void limit_next_junction_speed(double);
    elegoo::common::CommandError move_error(const std::string &m = "Move out of range");
    void calc_junction(Move *prev_move);
    void set_junction(double start_v2, double cruise_v2, double end_v2);
    bool is_kinematic_move = true;
    void printf_data();
public:
    double delta_v2, max_start_v2, smooth_delta_v2, max_smoothed_v2, max_cruise_v2, min_move_t, move_d, accel, next_junction_v2;
    double junction_deviation, prev_move_centriple_v2,next_time;
    double accel_t, cruise_t, decel_t, start_v, cruise_v, end_v;
    std::vector<double> start_pos;
    std::vector<double> end_pos;

    std::vector<double> axes_d;
    std::vector<double> axes_r;
    std::shared_ptr<ToolHead> toolhead;
    std::vector<std::function<void(double)>> timing_callbacks;

private:
    int64_t velocity;

    double t_start_v2;
    double t_cruise_v2;
    double t_end_v2;
    
};

class LookAheadQueue
{
public:
    LookAheadQueue(std::shared_ptr<ToolHead> toolhead);
    ~LookAheadQueue();

    std::shared_ptr<ToolHead> toolhead;
    std::vector<Move *> queue;
    std::vector<Move *> last_queue;
    double junction_flush;
    void reset();
    void set_flush_time(double flush_time);
    Move *get_last();
    void flush(bool lazy = false);
    void add_move(Move *move);
};

#endif
