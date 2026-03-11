// Handling of end stops.
//
// Copyright (C) 2016-2021  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "basecmd.h"    // oid_alloc
#include "board/gpio.h" // struct gpio
#include "command.h"    // DECL_COMMAND
#include "sched.h"      // struct timer
#include "board/misc.h" // alloc_maxsize

struct shutdown_pins
{
    uint8_t shutdown_pin[64];
    uint8_t shutdown_value[64];
    uint8_t shutdown_pin_count;
};

void config_shutdown_pins(uint32_t *args)
{
    struct shutdown_pins *e = oid_alloc(args[0], config_shutdown_pins, sizeof(*e));
}
DECL_COMMAND(config_shutdown_pins, "config_shutdown_pins oid=%c");

void command_shutdown_pins_add(uint32_t *args)
{
    struct shutdown_pins *e = oid_lookup(args[0], config_shutdown_pins);
    if (e->shutdown_pin_count + 1 > ARRAY_SIZE(e->shutdown_pin))
        shutdown("shutdown_pin_count limits");
    e->shutdown_pin[e->shutdown_pin_count] = args[1];
    e->shutdown_value[e->shutdown_pin_count] = args[2];
    e->shutdown_pin_count++;
}
DECL_COMMAND(command_shutdown_pins_add, "shutdown_pins_add oid=%c pin=%c value=%c");

void shutdown_pins_set(struct shutdown_pins *e)
{
    for (int i = 0; i < e->shutdown_pin_count; i++)
        gpio_out_setup(e->shutdown_pin[i], e->shutdown_value[i]);
}

void command_shutdown_pins_set(uint32_t *args)
{
    struct shutdown_pins *e = oid_lookup(args[0], config_shutdown_pins);
    shutdown_pins_set(e);
    shutdown("shutdown_pins");
}
DECL_COMMAND(command_shutdown_pins_set, "shutdown_pins_set oid=%c");

// POR

struct por
{
    struct timer time;
    struct gpio_in pin;
    uint32_t rest_time, sample_time, nextwake;
    uint8_t flags, sample_count, trigger_count;
    struct shutdown_pins *sp;
};

enum
{
    ESF_PIN_HIGH = 1 << 0,
};

static uint_fast8_t por_oversample_event(struct timer *t);

// Timer callback for an end stop
static uint_fast8_t
por_event(struct timer *t)
{
    struct por *e = container_of(t, struct por, time);
    uint8_t val = gpio_in_read(e->pin);
    uint32_t nextwake = e->time.waketime + e->rest_time;
    if ((val ? ~e->flags : e->flags) & ESF_PIN_HIGH)
    {
        // No match - reschedule for the next attempt
        e->time.waketime = nextwake;
        return SF_RESCHEDULE;
    }
    e->nextwake = nextwake;
    e->time.func = por_oversample_event;
    return por_oversample_event(t);
}

// Timer callback for an end stop that is sampling extra times
static uint_fast8_t
por_oversample_event(struct timer *t)
{
    struct por *e = container_of(t, struct por, time);
    uint8_t val = gpio_in_read(e->pin);
    if ((val ? ~e->flags : e->flags) & ESF_PIN_HIGH)
    {
        // No longer matching - reschedule for the next attempt
        e->time.func = por_event;
        e->time.waketime = e->nextwake;
        e->trigger_count = e->sample_count;
        return SF_RESCHEDULE;
    }
    uint8_t count = e->trigger_count - 1;
    if (!count)
    {
        shutdown_pins_set(e->sp);
        shutdown("power_off");
        return SF_DONE;
    }
    e->trigger_count = count;
    e->time.waketime += e->sample_time;
    return SF_RESCHEDULE;
}

void command_config_por(uint32_t *args)
{
    struct por *e = oid_alloc(args[0], command_config_por, sizeof(*e));
    //
    e->sp = oid_lookup(args[1], config_shutdown_pins);
    // 配置检测引脚
    e->pin = gpio_in_setup(args[2], args[3]);
    e->flags = (args[4] ? ESF_PIN_HIGH : 0);
    // 采样时间，采样次数
    e->sample_time = args[5];
    e->sample_count = args[6];
    e->rest_time = args[7];
    e->trigger_count = e->sample_count;
    // 启动定时器检测断电引脚
    sched_del_timer(&e->time);
    e->time.func = por_event;
    e->time.waketime = timer_read_time() + e->rest_time;
    sched_add_timer(&e->time);
}
DECL_COMMAND(command_config_por, "config_por oid=%c shutdown_pins_oid=%c pin=%c pull_up=%c pin_value=%c sample_ticks=%u sample_count=%c"
                                 " rest_ticks=%u");