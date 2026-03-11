/*****************************************************************************
 * @Author       : Louisa
 * @Date         : 2024-11-25 09:54:17
 * @LastEditors  : loping
 * @LastEditTime : 2025-09-11 15:26:18
 * @Description  : Printer stepper support
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/

#include "stepper.h"
#include "itersolve.h"
#include "stepcompress.h"
#include "printer.h"
#include <dlfcn.h>
#include "stepper_enable.h"
#include "force_move.h"
#include "motion_report.h"
using namespace elegoo::extras;

MCU_stepper::MCU_stepper(const std::string &name, std::shared_ptr<PinParams> &step_pin_params, std::shared_ptr<PinParams> &dir_pin_params,
                         double rotation_dist, double steps_per_rotation,
                         double step_pulse_duration, bool units_in_radians)
    : _name(name),
      _rotation_dist(rotation_dist),
      _steps_per_rotation(steps_per_rotation),
      _step_pulse_duration(step_pulse_duration),
      _units_in_radians(units_in_radians),
      _step_dist(rotation_dist / steps_per_rotation),
      _step_both_edge(false),
      _req_step_both_edge(false),
      _mcu_position_offset(0),
      _reset_cmd_tag(0),
      _get_position_cmd(nullptr),
      _trapq(nullptr),
      _stepper_kinematics(nullptr)
{
    SPDLOG_DEBUG("MCU_stepper init!");
    _mcu = std::static_pointer_cast<MCU>(step_pin_params->chip);
    _oid = _mcu->create_oid();
    _step_pin = *step_pin_params->pin;
    _invert_step = step_pin_params->invert;
    SPDLOG_INFO("stepper:{} oid: {}", _name, _oid);

    _invert_dir = dir_pin_params->invert;
    _orig_invert_dir = dir_pin_params->invert;
    _dir_pin = *dir_pin_params->pin;

    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    _mcu->register_config_callback([this]()
                                   { this->_build_config(); });

    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    if (std::static_pointer_cast<MCU>(dir_pin_params->chip) != _mcu)
    {
        // throw _mcu->get_printer()->config_error("Stepper dir pin must be on same mcu as step pin");
    }

    _stepqueue = std::shared_ptr<stepcompress>(stepcompress_alloc(_oid), stepcompress_free);
    stepcompress_set_invert_sdir(_stepqueue.get(), _invert_dir);
    _mcu->register_stepqueue(_stepqueue);

    // _itersolve_generate_steps = itersolve_generate_steps;
    // _itersolve_check_active = itersolve_check_active;
    elegoo::common::SignalManager::get_instance().register_signal(
        "elegoo:connect",
        std::function<void()>([this]()
                              {
            SPDLOG_DEBUG("MCU_stepper connect~~~~~~~~~~~~~~~~~");
            _query_mcu_position();
            SPDLOG_DEBUG("MCU_stepper connect~~~~~~~~~~~~~~~~~ success!"); }));
    SPDLOG_DEBUG("MCU_stepper init success!!");
}

MCU_stepper::~MCU_stepper()
{
}

std::shared_ptr<MCU> MCU_stepper::get_mcu()
{
    return _mcu;
}

std::string MCU_stepper::get_name(bool brief)
{
    if (brief && (_name.rfind("stepper_", 0) == 0))
    {
        return _name.substr(8);
    }
    return _name;
}

bool MCU_stepper::units_in_radians()
{
    return _units_in_radians;
}

std::pair<double, bool> MCU_stepper::get_pulse_duration()
{
    std::pair<double, bool> duration = {_step_pulse_duration, _step_both_edge};
    return duration;
}

void MCU_stepper::setup_default_pulse_duration(double pulse_duration, double step_both_edge)
{
    if (std::isnan(_step_pulse_duration))
    {
        _step_pulse_duration = pulse_duration;
    }
    _req_step_both_edge = step_both_edge;
}

void MCU_stepper::setup_itersolve(std::function<struct stepper_kinematics *(char)> alloc_func, char params)
{
    std::shared_ptr<stepper_kinematics> sk = std::shared_ptr<stepper_kinematics>(
        alloc_func(params),
        free);
    set_stepper_kinematics(sk);
}

int MCU_stepper::get_oid()
{
    return _oid;
}

double MCU_stepper::get_step_dist()
{
    return _step_dist;
}

std::pair<double, double> MCU_stepper::get_rotation_distance()
{
    return {_rotation_dist, _steps_per_rotation};
}

void MCU_stepper::set_rotation_distance(double rotation_dist)
{
    int64_t mcu_pos = get_mcu_position();
    _rotation_dist = rotation_dist;
    _step_dist = rotation_dist / _steps_per_rotation;
    set_stepper_kinematics(_stepper_kinematics);
    _set_mcu_position(mcu_pos);
}

std::pair<uint32_t, uint32_t> MCU_stepper::get_dir_inverted()
{
    return {_invert_dir, _orig_invert_dir};
}

void MCU_stepper::set_dir_inverted(uint32_t invert_dir)
{
    if (invert_dir == _invert_dir)
    {
        return;
    }
    _invert_dir = invert_dir;
    stepcompress_set_invert_sdir(_stepqueue.get(), invert_dir);
    elegoo::common::SignalManager::get_instance().emit_signal(
        "stepper:set_dir_inverted", shared_from_this());
}

double MCU_stepper::calc_position_from_coord(std::vector<double> coord)
{
    return itersolve_calc_position_from_coord(_stepper_kinematics.get(), coord[0], coord[1], coord[2]);
}

void MCU_stepper::set_position(std::vector<double> coord)
{
    int64_t mcu_pos = get_mcu_position();
    std::shared_ptr<stepper_kinematics> sk = _stepper_kinematics;
    itersolve_set_position(sk.get(), coord[0], coord[1], coord[2]);
    _set_mcu_position(mcu_pos);
}

double MCU_stepper::get_commanded_position()
{
    return itersolve_get_commanded_pos(_stepper_kinematics.get());
}

int64_t MCU_stepper::get_mcu_position(double cmd_pos)
{
    if (std::isnan(cmd_pos))
        cmd_pos = get_commanded_position();
    double mcu_pos_dist = cmd_pos + _mcu_position_offset;
    double mcu_pos = mcu_pos_dist / _step_dist;
    // printf("get_mcu_position: cmd_pos %f mcu_pos_dist %f mcu_pos %f\n", cmd_pos, mcu_pos_dist, mcu_pos);
    if (mcu_pos >= 0.)
    {
        return static_cast<int64_t>(mcu_pos + 0.5);
    }
    return static_cast<int64_t>(mcu_pos - 0.5);
}

void MCU_stepper::_set_mcu_position(int64_t &mcu_pos)
{
    double mcu_pos_dist = mcu_pos * _step_dist;
    _mcu_position_offset = mcu_pos_dist - get_commanded_position();
}

int64_t MCU_stepper::get_past_mcu_position(double print_time)
{
    uint64_t clock = _mcu->print_time_to_clock(print_time);
    int64_t pos = stepcompress_find_past_position(_stepqueue.get(), clock);
    return static_cast<int64_t>(pos);
}

double MCU_stepper::mcu_to_commanded_position(int64_t mcu_pos)
{
    return mcu_pos * _step_dist - _mcu_position_offset;
}

std::pair<std::shared_ptr<pull_history_steps>, int> MCU_stepper::dump_steps(int count, uint64_t start_clock, uint64_t end_clock)
{
    std::shared_ptr<pull_history_steps> data(new pull_history_steps[count]);
    int cnt = stepcompress_extract_old(_stepqueue.get(), data.get(), count, start_clock, end_clock);
    return {data, cnt};
}

std::shared_ptr<stepper_kinematics> MCU_stepper::get_stepper_kinematics()
{
    return _stepper_kinematics;
}

std::shared_ptr<stepper_kinematics> MCU_stepper::set_stepper_kinematics(std::shared_ptr<stepper_kinematics> sk)
{
    std::shared_ptr<stepper_kinematics> old_sk = _stepper_kinematics;
    int64_t mcu_pos = 0;
    if (old_sk) // need to check
    {
        mcu_pos = get_mcu_position();
    }
    _stepper_kinematics = sk;
    itersolve_set_stepcompress(sk.get(), _stepqueue.get(), _step_dist);
    set_trapq(_trapq);
    _set_mcu_position(mcu_pos);
    return old_sk;
}

void MCU_stepper::note_homing_end()
{
    int ret = stepcompress_reset(_stepqueue.get(), 0);
    if (ret)
    {
        throw elegoo::common::Error("Internal error in stepcompress : stepcompress_reset");
    }
    uint32_t data[3] = {_reset_cmd_tag, _oid, 0};
    ret = stepcompress_queue_msg(_stepqueue.get(), data, sizeof(data) / sizeof(data[0]));
    if (ret)
    {
        throw elegoo::common::Error("Internal error in stepcompress : stepcompress_queue_msg");
    }
    _query_mcu_position();
}

void MCU_stepper::_query_mcu_position()
{
    // SPDLOG_INFO("_query_mcu_position");
    if (_mcu->is_fileoutput())
    {
        return;
    }

    // std::vector<uint64_t> data = {_oid};
    std::vector<Any> data = {std::to_string(_oid)};
    json params = _get_position_cmd->send(data);
    int64_t last_pos = stoll(params["pos"].get<std::string>());
    if (_invert_dir)
    {
        last_pos = -last_pos;
    }

    double print_time = _mcu->estimated_print_time(params["#receive_time"].get<double>());
    uint64_t clock = _mcu->print_time_to_clock(print_time);

    int ret = stepcompress_set_last_position(_stepqueue.get(), clock, last_pos);
    if (ret)
    {
        throw elegoo::common::Error("Internal error in stepcompress : stepcompress_set_last_position");
    }

    _set_mcu_position(last_pos);
    elegoo::common::SignalManager::get_instance().emit_signal(
        "stepper:sync_mcu_position", shared_from_this());
}

trapq *MCU_stepper::get_trapq()
{
    return _trapq;
}

trapq *MCU_stepper::set_trapq(trapq *tq)
{
    itersolve_set_trapq(_stepper_kinematics.get(), tq);
    trapq *old_tq = _trapq;
    _trapq = tq;
    return old_tq;
}

void MCU_stepper::add_active_callback(std::function<void(double)> cb)
{
    _active_callbacks.emplace_back(cb);
}

void MCU_stepper::generate_steps(double flush_time)
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    double ret;
    std::shared_ptr<stepper_kinematics> sk = _stepper_kinematics;
    if (!_active_callbacks.empty())
    {
        ret = itersolve_check_active(sk.get(), flush_time);
        if (ret) // need to check
        {
            auto cbs = _active_callbacks;
            _active_callbacks.clear();
            for (auto &cb : cbs)
            {
                cb(ret);
            }
        }
    }
    sk = _stepper_kinematics;
    ret = itersolve_generate_steps(sk.get(), flush_time);
    if (ret)
    {
        trapq_debug(sk->tq);
        throw elegoo::common::CommandError("Internal error in stepcompress : itersolve_generate_steps");
    }
}

bool MCU_stepper::is_active_axis(char axis)
{
    char a = axis;
    return itersolve_is_active_axis(_stepper_kinematics.get(), a);
}

const double MIN_BOTH_EDGE_DURATION = 0.000000200;

void MCU_stepper::_build_config()
{
    SPDLOG_DEBUG("__func__:{},__LINE__:{}", __func__, __LINE__);
    if (std::isnan(_step_pulse_duration)) // need to check
    {
        _step_pulse_duration = 0.000002;
    }
    int invert_step = _invert_step;
    auto sbe = _mcu->get_constants().find("STEPPER_BOTH_EDGE");
    if (_req_step_both_edge && sbe != _mcu->get_constants().end() && _step_pulse_duration <= MIN_BOTH_EDGE_DURATION) // need to check sbe
    {
        _step_both_edge = true;
        _step_pulse_duration = 0.0;
        invert_step = -1;
    }
    SPDLOG_DEBUG("_mcu->seconds_to_clock(_step_pulse_duration:{}):{}", _step_pulse_duration, _mcu->seconds_to_clock(_step_pulse_duration));
    uint64_t step_pulse_ticks = 0;
    if (!std::isnan(_step_pulse_duration))
    {
        step_pulse_ticks = _mcu->seconds_to_clock(_step_pulse_duration);
        SPDLOG_DEBUG("_step_pulse_duration:{},step_pulse_ticks:{}", _step_pulse_duration, step_pulse_ticks);
    }
    _mcu->add_config_cmd("config_stepper oid=" + std::to_string(_oid) + " step_pin=" + _step_pin +
                         " dir_pin=" + _dir_pin + " invert_step=" + std::to_string(invert_step) +
                         " step_pulse_ticks=" + std::to_string(step_pulse_ticks));
    _mcu->add_config_cmd("reset_step_clock oid=" + std::to_string(_oid) + " clock=0", false, true);

    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    uint32_t step_cmd_tag = _mcu->lookup_command("queue_step oid=%c interval=%u count=%hu add=%hi")->get_command_tag();
    uint32_t dir_cmd_tag = _mcu->lookup_command("set_next_step_dir oid=%c dir=%c")->get_command_tag();
    _reset_cmd_tag = _mcu->lookup_command("reset_step_clock oid=%c clock=%u")->get_command_tag();
    _get_position_cmd = _mcu->lookup_query_command("stepper_get_position oid=%c", "stepper_position oid=%c pos=%i", _oid);
    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    float max_error = _mcu->get_max_stepper_error();
    uint64_t max_error_ticks = _mcu->seconds_to_clock(max_error);
    stepcompress_fill(_stepqueue.get(), max_error_ticks, step_cmd_tag, dir_cmd_tag);
}

double parse_gear_ratio(std::shared_ptr<ConfigWrapper> config, bool note_valid)
{
    auto gear_ratio = config->getdoublepairs("gear_ratio", {{1, 1}}, {",", ":"}, note_valid);
    double result = 1.0;
    for (const auto &g : gear_ratio)
    {
        result *= g.first / g.second;
    }
    return result;
}

// Function to parse step distance
std::pair<double, double> parse_step_distance(std::shared_ptr<ConfigWrapper> config, BoolValue units_in_radians, bool note_valid)
{
    // need to check
    if (units_in_radians == BoolValue::BOOL_NONE)
    {
        std::string rd = config->get("rotation_distance", "", false);
        std::string gr = config->get("gear_ratio", "", false);
        if (rd.empty() && !gr.empty())
        {
            units_in_radians = BoolValue::BOOL_TRUE;
        }
        else
        {
            units_in_radians = BoolValue::BOOL_FALSE;
        }
    }

    double rotation_dist;
    if (units_in_radians == BoolValue::BOOL_TRUE)
    {
        rotation_dist = 2.0 * M_PI;
        config->get("gear_ratio", "_invalid_", note_valid);
    }
    else
    {
        rotation_dist = config->getdouble("rotation_distance", DOUBLE_INVALID, DOUBLE_NONE, DOUBLE_NONE, 0, DOUBLE_NONE, note_valid);
    }

    int microsteps = config->getint("microsteps", INT_NONE, 1, INT_NONE, note_valid);
    int full_steps = config->getint("full_steps_per_rotation", 200, 1, INT_NONE, note_valid);

    if (full_steps % 4 != 0)
    {
        throw std::runtime_error("full_steps_per_rotation invalid in section '" + config->get_name() + "'");
    }

    double gearing = parse_gear_ratio(config, note_valid);
    return {rotation_dist, full_steps * microsteps * gearing};
}

std::shared_ptr<MCU_stepper> PrinterStepper(std::shared_ptr<ConfigWrapper> config, bool units_in_radians)
{
    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    std::shared_ptr<Printer> printer = config->get_printer();
    std::string name = config->get_name();

    // Stepper definition
    std::shared_ptr<PrinterPins> ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
    std::string step_pin = config->get("step_pin");
    std::shared_ptr<PinParams> step_pin_params = ppins->lookup_pin(step_pin, true);
    std::string dir_pin = config->get("dir_pin");
    std::shared_ptr<PinParams> dir_pin_params = ppins->lookup_pin(dir_pin, true);
    // SPDLOG_WARN("step_pin:{},chip_name:{},pin:{},invert:{},pullup:{}",step_pin,*step_pin_params->chip_name,*step_pin_params->pin,step_pin_params->invert,step_pin_params->pullup);
    // SPDLOG_WARN("dir_pin:{},chip_name:{},pin:{},invert:{},pullup:{}",dir_pin,*dir_pin_params->chip_name,*dir_pin_params->pin,dir_pin_params->invert,dir_pin_params->pullup);

    std::pair<double, double> ret;
    if (units_in_radians)
    {
        ret = parse_step_distance(config, BoolValue::BOOL_TRUE, true);
    }
    else
    {
        ret = parse_step_distance(config, BoolValue::BOOL_FALSE, true);
    }

    double rotation_dist = ret.first;
    double steps_per_rotation = ret.second;
    double step_pulse_duration = config->getdouble("step_pulse_duration", DOUBLE_NONE, 0.0, 0.001);

    SPDLOG_WARN("name:{},rotation_dist:{},steps_per_rotation:{},step_pulse_duration:{},units_in_radians:{}", name, rotation_dist, steps_per_rotation, step_pulse_duration, units_in_radians);
    std::shared_ptr<MCU_stepper> mcu_stepper = std::make_shared<MCU_stepper>(name, step_pin_params, dir_pin_params,
                                                                             rotation_dist, steps_per_rotation,
                                                                             step_pulse_duration, units_in_radians);

    SPDLOG_DEBUG("mcu_stepper->get_name():{}", mcu_stepper->get_name());
    auto m1 = any_cast<std::shared_ptr<PrinterStepperEnable>>(printer->load_object(config, "stepper_enable"));
    // std::shared_ptr<PrinterStepperEnable> m1 = std::static_pointer_cast<PrinterStepperEnable>(stepper_enable_ptr);
    m1->register_stepper(config, mcu_stepper);

    auto m2 = any_cast<std::shared_ptr<ForceMove>>(printer->load_object(config, "force_move"));
    // std::shared_ptr<ForceMove> m2 = std::static_pointer_cast<ForceMove>(force_move_ptr);
    m2->register_stepper(config, mcu_stepper);

    auto m3 = any_cast<std::shared_ptr<PrinterMotionReport>>(printer->load_object(config, "motion_report"));
    // std::shared_ptr<PrinterMotionReport> m3 = std::static_pointer_cast<PrinterMotionReport>(motion_report_ptr);
    m3->register_stepper(config, mcu_stepper);

    // SPDLOG_DEBUG("__func__:{},__LINE__:{}",__func__,__LINE__);
    return mcu_stepper;
}

PrinterRail::PrinterRail(std::shared_ptr<ConfigWrapper> config, bool need_position_minmax,
                         double default_position_endstop, bool units_in_radians)
    : stepper_units_in_radians(units_in_radians)
{
    printer = config->get_printer();
    // Primary stepper and endstop
    add_extra_stepper(config);
    std::shared_ptr<MCU_stepper> mcu_stepper = steppers[0];
    get_name = [mcu_stepper](bool brief) -> std::string
    { return mcu_stepper->get_name(brief); };
    get_commanded_position = [mcu_stepper]() -> double
    { return mcu_stepper->get_commanded_position(); };
    calc_position_from_coord = [mcu_stepper](std::vector<double> coord) -> double
    { return mcu_stepper->calc_position_from_coord(coord); };

    // Primary endstop position
    auto mcu_endstop = endstops[0].first;
    // get_position_endstop使用
    if (mcu_endstop->get_position_endstop)
        position_endstop = mcu_endstop->get_position_endstop();
    else if (std::isnan(default_position_endstop))
        position_endstop = config->getdouble("position_endstop");
    else
        position_endstop = config->getdouble("position_endstop", default_position_endstop);

    // Axis range
    if (need_position_minmax)
    {
        position_min = config->getdouble("position_min", 0.0);
        position_max = config->getdouble("position_max", DOUBLE_INVALID, DOUBLE_NONE, DOUBLE_NONE, position_min);
    }
    else
    {
        position_min = 0.0;
        position_max = position_endstop;
    }

    if (position_endstop < position_min || position_endstop > position_max)
    {
        throw std::runtime_error("position_endstop in section '" + config->get_name() + "' must be between position_min and position_max");
    }

    // Homing mechanics
    homing_speed = config->getdouble("homing_speed", 5.0, DOUBLE_NONE, DOUBLE_NONE, 0.);
    second_homing_speed = config->getdouble("second_homing_speed", homing_speed / 2, DOUBLE_NONE, DOUBLE_NONE, 0.);
    homing_retract_speed = config->getdouble("homing_retract_speed", homing_speed, DOUBLE_NONE, DOUBLE_NONE, 0.);
    homing_retract_dist = config->getdouble("homing_retract_dist", 5., 0.);
    homing_positive_dir = config->getboolean("homing_positive_dir", BoolValue::BOOL_FALSE);

    if ((homing_positive_dir && position_endstop == position_min) ||
        (!homing_positive_dir && position_endstop == position_max))
    {
        throw elegoo::common::ConfigParserError("Invalid homing_positive_dir / position_endstop in '" + config->get_name() + "'");
    }

    // Z轴
    std::string name = get_name(true);
    if (name == "z")
    {
        elegoo::common::SignalManager::get_instance().register_signal("probe:update_position_endstop",
                                                                      std::function<void(double)>([this](double position_endstop)
                                                                                                  {
                                                                                                      this->position_endstop = position_endstop;
                                                                                                      // 保存限位参数
                                                                                                      std::shared_ptr<PrinterConfig> configfile = any_cast<std::shared_ptr<PrinterConfig>>(printer->lookup_object("configfile"));
                                                                                                      std::shared_ptr<GCodeDispatch> gcode = any_cast<std::shared_ptr<GCodeDispatch>>(printer->lookup_object("gcode"));
                                                                                                        std::map<std::string, std::string> params;
                                                                                                      configfile->set("stepper_z", "position_endstop", std::to_string(position_endstop)); 
            configfile->cmd_SAVE_CONFIG(gcode->create_gcode_command("SAVE_CONFIG", "SAVE_CONFIG", params)); }));
    }

    SPDLOG_INFO("position_endstop {}", position_endstop);
    SPDLOG_DEBUG("create PrinterRail success");
}

PrinterRail::~PrinterRail()
{
}

std::pair<double, double> PrinterRail::get_range() const
{
    return {position_min, position_max};
}

HomingInfo PrinterRail::get_homing_info() const
{
    HomingInfo home = {homing_speed, position_endstop, homing_retract_speed,
                       homing_retract_dist, homing_positive_dir, second_homing_speed};
    return home;
}

double PrinterRail::set_homing_speed(double speed)
{
    SPDLOG_INFO("set_homing_speed homing_speed {} speed {}", homing_speed, speed);
    double prev_speed = homing_speed;
    homing_speed = speed;
    return prev_speed;
}

std::vector<std::shared_ptr<MCU_stepper>> PrinterRail::get_steppers() const
{
    return steppers;
}

std::vector<std::pair<std::shared_ptr<MCU_pins>, std::string>> PrinterRail::get_endstops() const
{
    return endstops;
}

void PrinterRail::add_extra_stepper(std::shared_ptr<ConfigWrapper> config)
{
    auto stepper = PrinterStepper(config, stepper_units_in_radians);
    steppers.push_back(stepper);

    if (!endstops.empty() && config->get("endstop_pin", "").empty())
    {
        endstops[0].first->add_stepper(stepper);
        return;
    }

    std::string endstop_pin = config->get("endstop_pin");
    if (endstop_pin.rfind("probe:", 0) == 0)
        _is_probe = true;
    else
        _is_probe = false;
    SPDLOG_INFO("{} _is_probe {}", stepper->get_name(), _is_probe);
    std::shared_ptr<Printer> printer = config->get_printer();
    std::shared_ptr<PrinterPins> ppins = any_cast<std::shared_ptr<PrinterPins>>(printer->lookup_object("pins"));
    auto pin_params = ppins->parse_pin(endstop_pin, true, true);
    std::string pin_name = *std::static_pointer_cast<std::string>(pin_params->chip_name) + ":" + *std::static_pointer_cast<std::string>(pin_params->pin);

    std::shared_ptr<MCU_pins> mcu_endstop;
    auto it = endstop_map.find(pin_name);
    if (it == endstop_map.end())
    {
        mcu_endstop = ppins->setup_pin("endstop", endstop_pin);
        std::shared_ptr<MCU_endstop> endstop_ptr = std::static_pointer_cast<MCU_endstop>(mcu_endstop);
        endstop_map[pin_name] = {endstop_ptr, pin_params->invert, pin_params->pullup};
        std::string name = stepper->get_name(true);
        endstops.emplace_back(endstop_ptr, name);
    }
    else
    {
        mcu_endstop = it->second.end_stop;
        if (pin_params->invert != it->second.invert || pin_params->pullup != it->second.pullup)
        {
            throw std::runtime_error("Printer rail " + get_name(true) + " shared endstop pin " + pin_name + " must specify the same pullup/invert settings");
        }
    }
    mcu_endstop->add_stepper(stepper);
}
void PrinterRail::setup_itersolve(std::function<struct stepper_kinematics *(char)> alloc_func, char params)
{
    for (auto stepper : steppers)
    {
        stepper->setup_itersolve(alloc_func, params);
    }
}

// Generate steps
void PrinterRail::generate_steps(double flush_time)
{
    for (auto stepper : steppers)
    {
        stepper->generate_steps(flush_time);
    }
}

// Set trapq
void PrinterRail::set_trapq(trapq *tp)
{
    for (auto stepper : steppers)
    {
        stepper->set_trapq(tp);
    }
}

// Set position
void PrinterRail::set_position(std::vector<double> coord)
{
    for (auto stepper : steppers)
    {
        stepper->set_position(coord);
    }
}

std::shared_ptr<PrinterRail> LookupMultiRail(std::shared_ptr<ConfigWrapper> config, bool need_position_minmax,
                                             double default_position_endstop, bool units_in_radians)
{
    auto rail = std::make_shared<PrinterRail>(config, need_position_minmax, default_position_endstop, units_in_radians);
    for (int i = 1; i < 99; ++i)
    {
        std::string section_name = config->get_name() + std::to_string(i);
        if (!config->has_section(section_name))
            break;
        rail->add_extra_stepper(config->getsection(section_name));
    }
    return rail;
}
