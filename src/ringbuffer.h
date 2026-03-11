/*****************************************************************************
 * @Author       : Coconut
 * @Date         : 2025-03-22 11:23:58
 * @LastEditors  : coconut
 * @LastEditTime : 2025-03-22 11:37:56
 * @Description  :
 *
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved.
 *****************************************************************************/
#ifndef __RINGBUFFER_H
#define __RINGBUFFER_H
#include <stdint.h>
#include <string.h>
typedef struct
{
    uint8_t *buf;
    uint32_t in;
    uint32_t out;
    uint32_t size;
} ringbuffer_t;

#define DEFINE_RINGBUFFER(name, __size)   \
    uint8_t __magic_##name##_buf[__size]; \
    ringbuffer_t name = {               \
        .buf = __magic_##name##_buf,    \
        .in = 0,                        \
        .out = 0,                       \
        .size = __size};

#define ringbuffer_min(a, b) (((a) > (b)) ? (b) : (a))

static inline void ringbuffer_init(ringbuffer_t *rb, uint8_t *buf, uint32_t size)
{
    rb->buf = buf;
    rb->in = rb->out = 0;
    rb->size = size;
}

static inline uint32_t ringbuffer_put(ringbuffer_t *rb, const uint8_t *buf, uint32_t len)
{
    len = ringbuffer_min(len, rb->size - rb->in + rb->out);
    uint16_t l = ringbuffer_min(len, rb->size - (rb->in & (rb->size - 1)));
    memcpy(rb->buf + (rb->in & (rb->size - 1)), buf, l);
    memcpy(rb->buf, buf + l, len - l);
    rb->in += len;
    return len;
}

static inline uint32_t ringbuffer_get(ringbuffer_t *rb, uint8_t *buf, uint32_t len)
{
    len = ringbuffer_min(len, rb->in - rb->out);
    uint16_t l = ringbuffer_min(len, rb->size - (rb->out & (rb->size - 1)));
    memcpy(buf, rb->buf + (rb->out & (rb->size - 1)), l);
    memcpy(buf + l, rb->buf, len - l);
    rb->out += len;
    return len;
}

static inline uint32_t ringbuffer_available_get_length(ringbuffer_t *rb)
{
    return rb->in - rb->out;
}

static inline uint32_t ringbuffer_available_put_length(ringbuffer_t *rb)
{
    return rb->size - ringbuffer_available_get_length(rb);
}

#endif // ringbuffer.h
