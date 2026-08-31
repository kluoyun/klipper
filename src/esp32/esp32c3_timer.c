// ESP32-C3 SYSTIMER support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h>
#include "board/irq.h"
#include "board/misc.h"
#include "board/timer_irq.h"
#include "esp32_regs.h"
#include "internal.h"
#include "sched.h"

static uint64_t
timer_read_time64(void)
{
    // UNIT0 has one snapshot latch, so protect the complete read sequence.
    irqstatus_t flag = irq_save();
    REG32(SYSTIMER_UNIT0_OP) = SYSTIMER_UNIT0_UPDATE;
    while (!(REG32(SYSTIMER_UNIT0_OP) & SYSTIMER_UNIT0_VALID))
        ;
    uint32_t low, low_check = REG32(SYSTIMER_UNIT0_VALUE_LO), high;
    do {
        low = low_check;
        high = REG32(SYSTIMER_UNIT0_VALUE_HI) & 0xfffff;
        low_check = REG32(SYSTIMER_UNIT0_VALUE_LO);
    } while (low_check != low);
    irq_restore(flag);
    return ((uint64_t)high << 32) | low;
}

uint32_t
timer_read_time(void)
{
    return timer_read_time64();
}

static void
timer_set(uint32_t next)
{
    uint64_t now = timer_read_time64();
    uint32_t now_low = now;
    uint64_t target = (now & ~0xffffffffULL) | next;
    if ((int32_t)(next - now_low) > 0 && next < now_low)
        target += 1ULL << 32;

    // Disable the alarm while loading a new one-shot target.
    REG32(SYSTIMER_CONF) &= ~SYSTIMER_TARGET0_ENABLE;
    REG32(SYSTIMER_TARGET0_HI) = target >> 32;
    REG32(SYSTIMER_TARGET0_LO) = target;
    REG32(SYSTIMER_COMP0_LOAD) = 1;
    REG32(SYSTIMER_CONF) |= SYSTIMER_TARGET0_ENABLE;
}

void
timer_kick(void)
{
    REG32(SYSTIMER_INT_CLEAR) = SYSTIMER_TARGET0_INT;
    timer_set(timer_read_time() + 50);
}

void
esp32_timer_irq(void)
{
    irq_disable();
    REG32(SYSTIMER_INT_CLEAR) = SYSTIMER_TARGET0_INT;
    uint32_t next = timer_dispatch_many();
    timer_set(next);
    irq_enable();
}

void
timer_init(void)
{
    irqstatus_t flag = irq_save();
    REG32(SYSTEM_PERIP_CLK_EN0) |= SYSTEM_SYSTIMER_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) |= SYSTEM_SYSTIMER_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~SYSTEM_SYSTIMER_BIT;

    REG32(SYSTIMER_CONF) = SYSTIMER_CLOCK_ENABLE | SYSTIMER_UNIT0_ENABLE;
    REG32(SYSTIMER_TARGET0_CONF) = 0;
    REG32(SYSTIMER_INT_CLEAR) = SYSTIMER_TARGET0_INT;
    REG32(SYSTIMER_INT_ENABLE) = SYSTIMER_TARGET0_INT;
    esp32_irq_setup(INT_SOURCE_SYSTIMER0, CPU_INT_SYSTIMER, 2);
    timer_kick();
    irq_restore(flag);
}
DECL_INIT(timer_init);
