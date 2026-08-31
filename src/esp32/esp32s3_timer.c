// ESP32-S3 Xtensa core timer support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h>
#include "autoconf.h"
#include "board/irq.h"
#include "board/misc.h"
#include "board/timer_irq.h"
#include "esp32_regs.h"
#include "internal.h"
#include "sched.h"

_Static_assert(CONFIG_CLOCK_FREQ == 240000000,
               "ESP32-S3 CCOUNT timer requires a fixed 240MHz CPU clock");

// Use the 240MHz core counter for low-overhead timer reads.
uint32_t
timer_read_time(void)
{
    uint32_t value;
    asm volatile("rsr %0, ccount" : "=a"(value));
    return value;
}

static void
timer_set(uint32_t next)
{
    // Writing CCOMPARE clears its pending interrupt.
    asm volatile("wsr %0, ccompare0\nesync" :: "a"(next) : "memory");
}

void
timer_kick(void)
{
    timer_set(timer_read_time() + timer_from_us(3));
}

void
esp32_timer_irq(void)
{
    // Mask the timer source while dispatch may temporarily enable interrupts.
    // Otherwise the pending comparator interrupt can recursively re-enter this
    // handler when timer_dispatch_many() waits for a near-future timer.
    esp32_irq_disable(CPU_INT_SYSTIMER);
    uint32_t next = timer_dispatch_many();
    timer_set(next);
    esp32_irq_enable(CPU_INT_SYSTIMER);
}

void
timer_init(void)
{
    irqstatus_t flag = irq_save();
    // Keep CCOUNT running while the CPU is idle.
    REG32(SYSTEM_CPU_PER_CONF) |= SYSTEM_CPU_WAIT_MODE_FORCE_ON;
    uint32_t zero = 0;
    asm volatile("wsr %0, ccount\nrsync" :: "a"(zero) : "memory");
    esp32_irq_enable(CPU_INT_SYSTIMER);
    timer_kick();
    irq_restore(flag);
}
DECL_INIT(timer_init);
