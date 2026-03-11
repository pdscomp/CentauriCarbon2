// Load Cell based end stops.
//
// Copyright (C) 2023  Gareth Farrington <gareth@waves.ky>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "basecmd.h"           // oid_alloc
#include "board/irq.h"         // irq_disable
#include "board/gpio.h"        // irq_disable
#include "command.h"           // DECL_COMMAND
#include "sched.h"             // shutdown
#include "trsync.h"            // trsync_do_trigger
#include "board/misc.h"        // timer_read_time
#include "sos_filter.h"        // fixedQ12_t
#include "load_cell_endstop.h" //load_cell_endstop_report_sample
#include <stdint.h>            // int32_t
#include <stdlib.h>            // abs

// Q33.30
typedef int64_t fixedQ33_t; // Q1.30 value stored in int64
#define FIXEDQ33_FRAC_BITS FIXEDQ1_FRAC_BITS
#define MAX_TRIGGER_GRAMS ((1 << FIXEDQ16) - 1)
#define FIXEDQ1_ONE 1 << FIXEDQ1_FRAC_BITS
const uint8_t DEFAULT_SAMPLE_COUNT = 2;

// Flags
enum
{
    FLAG_IS_HOMING = 1 << 0,
    FLAG_IS_TRIGGERED = 1 << 1,
    FLAG_IS_HOMING_TRIGGER = 1 << 2,
    FLAG_IS_AWAIT_HOMING = 1 << 3,
};

enum
{
    LOAD_CELL_ERROR_NONE = 0,
    LOAD_CELL_ERROR_SAFETY_LIMIT,
    LOAD_CELL_ERROR_DRIFT_LIMIT,
    LOAD_CELL_ERROR_WATCHDOG,
};

// Endstop Structure
struct load_cell_endstop
{
    struct timer time;
    uint32_t trigger_grams, trigger_ticks, trigger_emit_ticks, last_sample_ticks, rest_ticks;
    struct trsync *ts;
    int32_t trigger_sample, trigger_emit_sample, last_sample, safety_counts_min, safety_counts_max, tare_counts;
    uint8_t flags, sample_count, trigger_count, trigger_reason, error_reason, watchdog_max, watchdog_count;
    fixedQ16_t trigger_grams_fixed;
    fixedQ1_t grams_per_count;
    struct sos_filter *sf;
    uint32_t homing_clock;
    int error;
    struct gpio_out output_pin;
    int use_gpio;
};

// returns the integer part of a fixedQ48_t
static inline int64_t
round_fixedQ48(const fixedQ48_t fixed_value)
{
    return fixed_value >> FIXEDQ48_FRAC_BITS;
}

// Convert sensor counts to grams
static inline fixedQ48_t
counts_to_grams(struct load_cell_endstop *lce, const int32_t counts)
{
    // tearing ensures readings are referenced to 0.0g
    const int32_t delta = counts - lce->tare_counts;
    // convert sensor counts to grams by multiplication: 124 * 0.051 = 6.324
    // this optimizes to single cycle SMULL instruction
    const fixedQ33_t product = (int64_t)delta * (int64_t)lce->grams_per_count;
    // after multiplication there are 30 fraction bits, reduce to 15
    // caller verifies this wont overflow a 32bit int when truncated
    const fixedQ48_t grams = product >>
                             (FIXEDQ33_FRAC_BITS - FIXEDQ48_FRAC_BITS);
    return grams;
}

static inline uint8_t
is_flag_set(const uint8_t mask, struct load_cell_endstop *lce)
{
    return !!(mask & lce->flags);
}

static inline void
set_flag(uint8_t mask, struct load_cell_endstop *lce)
{
    lce->flags |= mask;
}

static inline void
clear_flag(uint8_t mask, struct load_cell_endstop *lce)
{
    lce->flags &= ~mask;
}

void try_trigger(struct load_cell_endstop *lce, uint32_t ticks)
{
    set_flag(FLAG_IS_TRIGGERED, lce);

    uint8_t is_homing = is_flag_set(FLAG_IS_HOMING, lce);
    uint8_t is_homing_triggered = is_flag_set(FLAG_IS_HOMING_TRIGGER, lce);
    if (is_homing && !is_homing_triggered)
    {
        // this flag latches until a reset, disabling further triggering
        set_flag(FLAG_IS_HOMING_TRIGGER, lce);
        if (lce->use_gpio)
            gpio_out_write(lce->output_pin, 0);
        else
            trsync_do_trigger(lce->ts, lce->trigger_reason);
    }
}

void try_trigger_error(struct load_cell_endstop *lce)
{
    uint8_t is_homing = is_flag_set(FLAG_IS_HOMING, lce);
    if (is_homing)
    {
        if (lce->use_gpio)
            gpio_out_write(lce->output_pin, 0);
        else
            trsync_do_trigger(lce->ts, lce->error_reason);
    }
}

// Used by Sensors to report new raw ADC sample
void load_cell_endstop_report_sample(struct load_cell_endstop *lce, const int32_t sample, const uint32_t ticks)
{
    // save new sample
    lce->last_sample = sample;
    lce->last_sample_ticks = ticks;
    lce->watchdog_count = 0;
    uint8_t is_homing = is_flag_set(FLAG_IS_HOMING, lce);
    uint8_t is_await_homing = is_flag_set(FLAG_IS_AWAIT_HOMING, lce);

    // 保证归零动作起始后再开始检测
    if (is_await_homing && !timer_is_before(ticks, lce->homing_clock))
    {
        clear_flag(FLAG_IS_AWAIT_HOMING, lce);
        set_flag(FLAG_IS_HOMING, lce);
        is_homing = is_flag_set(FLAG_IS_HOMING, lce);
    }

    // check for safety limit violations first
    const uint8_t is_safety_trigger = sample <= lce->safety_counts_min || sample >= lce->safety_counts_max;
    // too much force, this is an error while homing
    if (is_homing && is_safety_trigger)
    {
        lce->error = LOAD_CELL_ERROR_SAFETY_LIMIT;
        try_trigger_error(lce);
    }
    else if(!is_homing)
    {
        clear_flag(FLAG_IS_TRIGGERED, lce);
        if (lce->use_gpio)
            gpio_out_write(lce->output_pin, 1);
        return;
    }

    // only trigger when trigger_grams is greater than 0
    if (lce->trigger_grams == 0)
    {
        // clear "live" trigger view for QUERY_ENDSTOPS
        clear_flag(FLAG_IS_TRIGGERED, lce);
        if (lce->use_gpio)
            gpio_out_write(lce->output_pin, 1);
        return;
    }

    uint8_t is_trigger = 0;
    // 原始数据转换为克重
    const fixedQ48_t raw_grams = counts_to_grams(lce, sample);
    const uint8_t is_overflow = overflows_int32(raw_grams);
    // 数据正常
    if (!is_overflow)
    {
        // 获取滤波后的数据
        const fixedQ16_t filtered_grams = sosfilt(lce->sf, (fixedQ16_t)raw_grams);
        // 判断是否触发
        // is_trigger = abs(filtered_grams) >= lce->trigger_grams_fixed;
        // 20250402-修改为单向触发避免干扰
        is_trigger = (filtered_grams) <= -lce->trigger_grams_fixed;
    }
    // 数据溢出
    else
    {
        // assume triggered in the case of an overflow
        is_trigger = 1;
        lce->error = LOAD_CELL_ERROR_DRIFT_LIMIT;
        // While homing an overflow is an error
        try_trigger_error(lce);
    }

    uint8_t is_homing_triggered = is_flag_set(FLAG_IS_HOMING_TRIGGER, lce);

    // 锁存状态直至释放
    // latching trigger for QUERY_ENDSTOPS: dont clear until force is removed
    uint8_t alredy_triggered = is_flag_set(FLAG_IS_TRIGGERED, lce);
    if (alredy_triggered)
    {
        const int64_t abs_int_grams = abs(round_fixedQ48(raw_grams));
        uint8_t is_abs_trigger = abs_int_grams >= lce->trigger_grams;
        is_trigger = is_abs_trigger || is_trigger;
    }

    // update trigger state
    if (is_trigger && lce->trigger_count > 0)
    {
        // 记录第一次触发的时间与采样值
        if (is_homing && !is_homing_triggered && lce->trigger_count == lce->sample_count)
        {
            lce->trigger_ticks = ticks;
            lce->trigger_sample = sample;
        }

        lce->trigger_count -= 1;

        // 真正触发
        if (lce->trigger_count == 0)
        {
            lce->trigger_emit_ticks = ticks;
            lce->trigger_emit_sample = sample;
            try_trigger(lce, ticks);
        }
    }
    else if (!is_trigger && lce->trigger_count < lce->sample_count)
    {
        // clear "live" trigger view for QUERY_ENDSTOPS
        lce->trigger_count += 1;
        if (lce->trigger_count > lce->sample_count)
            lce->trigger_count = lce->sample_count;
        clear_flag(FLAG_IS_TRIGGERED, lce);
        if (lce->use_gpio)
            gpio_out_write(lce->output_pin, 1);
        if (is_homing && !is_homing_triggered)
        {
            lce->trigger_ticks = 0;
        }
    }
}

// Timer callback that monitors for timeouts
static uint_fast8_t
watchdog_event(struct timer *t)
{
    struct load_cell_endstop *lce = container_of(t, struct load_cell_endstop, time);
    uint8_t is_homing = is_flag_set(FLAG_IS_HOMING, lce);
    uint8_t is_await_homing = is_flag_set(FLAG_IS_AWAIT_HOMING, lce);
    uint8_t is_homing_trigger = is_flag_set(FLAG_IS_HOMING_TRIGGER, lce);
    // the watchdog stops when not homing or when trsync becomes triggered
    if (!is_homing || !is_await_homing || is_homing_trigger)
    {
        return SF_DONE;
    }

    irq_disable();
    if (lce->watchdog_count > lce->watchdog_max)
    {
        lce->error = LOAD_CELL_ERROR_WATCHDOG;
        try_trigger_error(lce);
    }
    lce->watchdog_count += 1;
    irq_enable();

    // A sample was recently delivered, continue monitoring
    lce->time.waketime += lce->rest_ticks;
    return SF_RESCHEDULE;
}

static void
set_endstop_range(struct load_cell_endstop *lce, int32_t safety_counts_min, int32_t safety_counts_max, int32_t tare_counts, uint32_t trigger_grams, fixedQ1_t grams_per_count)
{
    if (!(safety_counts_max >= safety_counts_min))
    {
        shutdown("Safety range reversed");
    }
    if (trigger_grams > MAX_TRIGGER_GRAMS)
    {
        shutdown("trigger_grams too large");
    }
    // grams_per_count must be a positive fraction in Q1 format
    const fixedQ1_t one = 1 << FIXEDQ1_FRAC_BITS;
    if (grams_per_count < 0 || grams_per_count >= one)
    {
        shutdown("grams_per_count is invalid");
    }
    lce->safety_counts_min = safety_counts_min;
    lce->safety_counts_max = safety_counts_max;
    lce->tare_counts = tare_counts;
    lce->trigger_grams = trigger_grams;
    lce->trigger_grams_fixed = trigger_grams << FIXEDQ16_FRAC_BITS;
    lce->grams_per_count = grams_per_count;
    reset_filter_state(lce->sf);
}

// Create a load_cell_endstop
void command_config_load_cell_endstop(uint32_t *args)
{
    struct load_cell_endstop *lce = oid_alloc(args[0], command_config_load_cell_endstop, sizeof(*lce));
    lce->flags = 0;
    lce->trigger_count = lce->sample_count = DEFAULT_SAMPLE_COUNT;
    lce->trigger_ticks = 0;
    lce->watchdog_max = 0;
    lce->watchdog_count = 0;
    lce->sf = sos_filter_oid_lookup(args[1]);
    set_endstop_range(lce, 0, 0, 0, 0, 0);
}
DECL_COMMAND(command_config_load_cell_endstop, "config_load_cell_endstop"
                                               " oid=%c sos_filter_oid=%c");

void command_config_load_cell_endstop_with_pin(uint32_t *args)
{
    command_config_load_cell_endstop(args);
    struct load_cell_endstop *lce = oid_lookup(args[0], command_config_load_cell_endstop);
    lce->output_pin = gpio_out_setup(args[2], 1);
    lce->use_gpio = 1;
}
DECL_COMMAND(command_config_load_cell_endstop_with_pin, "config_load_cell_endstop_with_pin"
                                                        " oid=%c sos_filter_oid=%c pin=%u");

// Lookup a load_cell_endstop
struct load_cell_endstop *
load_cell_endstop_oid_lookup(uint8_t oid)
{
    return oid_lookup(oid, command_config_load_cell_endstop);
}

// Set the triggering range and tare value
void command_set_range_load_cell_endstop(uint32_t *args)
{
    struct load_cell_endstop *lce = load_cell_endstop_oid_lookup(args[0]);
    set_endstop_range(lce, args[1], args[2], args[3], args[4], (fixedQ16_t)args[5]);
}
DECL_COMMAND(command_set_range_load_cell_endstop, "set_range_load_cell_endstop"
                                                  " oid=%c safety_counts_min=%i safety_counts_max=%i tare_counts=%i"
                                                  " trigger_grams=%u grams_per_count=%i");

// Home an axis
void command_load_cell_endstop_home(uint32_t *args)
{
    struct load_cell_endstop *lce = load_cell_endstop_oid_lookup(args[0]);
    sched_del_timer(&lce->time);
    // clear the homing trigger flag
    clear_flag(FLAG_IS_HOMING_TRIGGER, lce);
    clear_flag(FLAG_IS_HOMING, lce);
    clear_flag(FLAG_IS_AWAIT_HOMING, lce);

    lce->trigger_ticks = 0;
    lce->ts = NULL;
    lce->error = LOAD_CELL_ERROR_NONE;
    // 0 samples indicates homing is finished
    if (args[3] == 0)
    {
        // Disable end stop checking
        return;
    }
    if (!lce->use_gpio)
        lce->ts = trsync_oid_lookup(args[1]);
    lce->trigger_reason = args[2];
    lce->error_reason = args[3];
    lce->time.waketime = args[4];
    lce->homing_clock = args[4];
    lce->sample_count = args[5];
    lce->rest_ticks = args[6];
    lce->watchdog_max = args[7];
    lce->watchdog_count = 0;
    reset_filter_state(lce->sf);
    lce->time.func = watchdog_event;
    sched_add_timer(&lce->time);
    set_flag(FLAG_IS_AWAIT_HOMING, lce);
}
DECL_COMMAND(command_load_cell_endstop_home,
             "load_cell_endstop_home oid=%c trsync_oid=%c trigger_reason=%c"
             " error_reason=%c clock=%u sample_count=%c rest_ticks=%u"
             " timeout=%u");

void command_load_cell_endstop_query_state(uint32_t *args)
{
    uint8_t oid = args[0];
    struct load_cell_endstop *lce = load_cell_endstop_oid_lookup(args[0]);
    output("trigger_ticks=%u trigger_emit_ticks=%u", lce->trigger_ticks, lce->trigger_emit_ticks);
    sendf("load_cell_endstop_state oid=%c homing=%c homing_triggered=%c"
          " is_triggered=%c trigger_ticks=%u trigger_emit_ticks=%u trigger_sample=%i trigger_emit_sample=%i sample=%i sample_ticks=%u error=%c",
          oid, is_flag_set(FLAG_IS_HOMING, lce), is_flag_set(FLAG_IS_HOMING_TRIGGER, lce), is_flag_set(FLAG_IS_TRIGGERED, lce),
          lce->trigger_ticks, lce->trigger_emit_ticks, lce->trigger_sample, lce->trigger_emit_sample, lce->last_sample, lce->last_sample_ticks, lce->error);
}
DECL_COMMAND(command_load_cell_endstop_query_state, "load_cell_endstop_query_state oid=%c");
