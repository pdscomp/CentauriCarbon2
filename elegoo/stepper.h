/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:54:19
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-11 15:26:44

 * @Description  : Printer stepper support
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#pragma once
#include "mcu.h"
#include "itersolve.h"
#include "pins.h"
#include "configfile.h"
#include "printer.h"
#include "chipbase.h"

class MCU_stepper : public std::enable_shared_from_this<MCU_stepper>
{
public:
    MCU_stepper(const std::string &name, std::shared_ptr<PinParams> &step_pin_params, std::shared_ptr<PinParams> &dir_pin_params,
                double rotation_dist, double steps_per_rotation,
                double step_pulse_duration = DOUBLE_NONE, bool units_in_radians = false);
    ~MCU_stepper();

    std::shared_ptr<MCU> get_mcu();
    std::string get_name(bool brief = false);
    bool units_in_radians();
    std::pair<double, bool> get_pulse_duration();
    void setup_default_pulse_duration(double pulse_duration, double step_both_edge);
    void setup_itersolve(std::function<struct stepper_kinematics *(char)> alloc_func, char params);
    int get_oid();
    double get_step_dist();
    std::pair<double, double> get_rotation_distance();
    void set_rotation_distance(double rotation_dist);
    std::pair<uint32_t, uint32_t> get_dir_inverted();
    void set_dir_inverted(uint32_t invert_dir);
    double calc_position_from_coord(std::vector<double> coord);
    void set_position(std::vector<double> coord);
    double get_commanded_position();
    int64_t get_mcu_position(double cmd_pos = DOUBLE_NONE);
    int64_t get_past_mcu_position(double print_time);
    double mcu_to_commanded_position(int64_t mcu_pos);
    std::pair<std::shared_ptr<pull_history_steps>, int> dump_steps(int count, uint64_t start_clock, uint64_t end_clock);
    std::shared_ptr<stepper_kinematics> get_stepper_kinematics();
    std::shared_ptr<stepper_kinematics> set_stepper_kinematics(std::shared_ptr<stepper_kinematics> sk);
    void note_homing_end();
    trapq *get_trapq();
    trapq *set_trapq(trapq *tp);
    void add_active_callback(std::function<void(double)> cb);
    void generate_steps(double flush_time);
    bool is_active_axis(char axis);
    void send_untrack_step(double print_time, uint8_t sdir, uint16_t interval, uint16_t count, int16_t add);

private:
    std::string _name;
    double _rotation_dist;
    double _steps_per_rotation;
    double _step_pulse_duration;
    bool _units_in_radians;
    double _step_dist;
    std::shared_ptr<MCU> _mcu;
    uint32_t _oid;
    std::string _step_pin;
    bool _invert_step;
    std::string _dir_pin;
    uint32_t _invert_dir;
    uint32_t _orig_invert_dir;
    bool _step_both_edge;
    bool _req_step_both_edge;
    double _mcu_position_offset;
    uint32_t _reset_cmd_tag;
    stepper_kinematics _set_stepper_kinematics;
    std::shared_ptr<CommandQueryWrapper> _get_position_cmd;
    std::vector<std::function<void(double)>> _active_callbacks;
    std::shared_ptr<stepcompress> _stepqueue;
    std::shared_ptr<stepper_kinematics> _stepper_kinematics;
    trapq *_trapq;
    void _build_config();
    void _query_mcu_position();
    void _set_mcu_position(int64_t &mcu_pos);
};

struct HomingInfo
{
    double speed;
    double position_endstop;
    double retract_speed;
    double retract_dist;
    bool positive_dir;
    double second_homing_speed;
};

struct EndstopParams
{
    std::shared_ptr<MCU_endstop> end_stop;
    int invert;
    int pullup;
};

class PrinterRail
{
public:
    PrinterRail(std::shared_ptr<ConfigWrapper> config, bool need_position_minmax = true,
                double default_position_endstop = DOUBLE_NONE, bool units_in_radians = false);
    ~PrinterRail();

    std::pair<double, double> get_range() const;
    HomingInfo get_homing_info() const;
    std::vector<std::shared_ptr<MCU_stepper>> get_steppers() const;
    std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> get_endstops() const;
    void add_extra_stepper(std::shared_ptr<ConfigWrapper> config);
    void setup_itersolve(std::function<struct stepper_kinematics *(char)> alloc_func, char params);
    void generate_steps(double flush_time);
    void set_trapq(trapq *tp);
    void set_position(std::vector<double> coord);
    std::function<std::string(bool)> get_name;
    double set_homing_speed(double speed);
    double get_position_endstop() const { return position_endstop; }
    bool is_probe() const { return _is_probe; }

private:
    std::shared_ptr<Printer> printer;
    bool stepper_units_in_radians;
    std::vector<std::shared_ptr<MCU_stepper>> steppers;
    std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> endstops;
    std::map<std::string, EndstopParams> endstop_map;
    double position_min, position_max, position_endstop;
    double homing_speed, second_homing_speed, homing_retract_speed, homing_retract_dist;
    bool homing_positive_dir;
    std::function<double()> get_commanded_position;
    std::function<double(std::vector<double> coord)> calc_position_from_coord;
    bool _is_probe;
};

std::shared_ptr<PrinterRail> LookupMultiRail(std::shared_ptr<ConfigWrapper> config,
                                             bool need_position_minmax = true,
                                             double default_position_endstop = DOUBLE_NONE,
                                             bool units_in_radians = false);

std::pair<double, double> parse_step_distance(std::shared_ptr<ConfigWrapper> config, BoolValue units_in_radians = BoolValue::BOOL_NONE, bool note_valid = false);
std::shared_ptr<MCU_stepper> PrinterStepper(std::shared_ptr<ConfigWrapper> config, bool units_in_radians = false);