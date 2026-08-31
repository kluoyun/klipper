// Xtensa interrupt support for ESP32-S3 core 0
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h>
#include "autoconf.h"
#include "board/irq.h"
#include "compiler.h"
#include "esp32_regs.h"
#include "internal.h"

void
irq_disable(void)
{
    uint32_t unused;
    asm volatile("rsil %0, 15" : "=a"(unused) :: "memory");
}

void
irq_enable(void)
{
    uint32_t unused;
    asm volatile("rsil %0, 0" : "=a"(unused) :: "memory");
}

irqstatus_t
irq_save(void)
{
    irqstatus_t flag;
    asm volatile("rsil %0, 15" : "=a"(flag) :: "memory");
    return flag;
}

void
irq_restore(irqstatus_t flag)
{
    asm volatile("wsr %0, ps\nrsync" :: "a"(flag) : "memory");
}

void
irq_wait(void)
{
    uint32_t unused;
    asm volatile("rsil %0, 0\nwaiti 0\nrsil %0, 15"
                 : "=a"(unused) :: "memory");
}

void
irq_poll(void)
{
}

static uint32_t
read_intenable(void)
{
    uint32_t value;
    asm volatile("rsr %0, intenable" : "=a"(value));
    return value;
}

static void
write_intenable(uint32_t value)
{
    asm volatile("wsr %0, intenable\nrsync" :: "a"(value) : "memory");
}

void
esp32_irq_enable(uint32_t cpu_intr)
{
    irqstatus_t flag = irq_save();
    write_intenable(read_intenable() | BIT32(cpu_intr));
    irq_restore(flag);
}

void
esp32_irq_disable(uint32_t cpu_intr)
{
    irqstatus_t flag = irq_save();
    write_intenable(read_intenable() & ~BIT32(cpu_intr));
    irq_restore(flag);
}

void
esp32_irq_setup(uint32_t source, uint32_t cpu_intr, uint32_t priority)
{
    (void)priority;
    REG32(INT_MATRIX_MAP(source)) = cpu_intr;
    esp32_irq_enable(cpu_intr);
}

void
esp32_irq_reset(void)
{
    irq_disable();
    write_intenable(0);
    for (uint32_t source = 0; source < INT_SOURCE_COUNT; source++)
        REG32(INT_MATRIX_MAP(source)) = CPU_INT_DISABLED;
}

void __visible
esp32s3_handle_irq(void)
{
    uint32_t pending;
    asm volatile("rsr %0, interrupt" : "=a"(pending));
    pending &= read_intenable();

    if (pending & BIT32(CPU_INT_SYSTIMER))
        esp32_timer_irq();
#if CONFIG_ESP32_SERIAL_UART0 || CONFIG_ESP32_SERIAL_UART1 \
    || CONFIG_ESP32_SERIAL_UART2
    if (pending & BIT32(CPU_INT_UART))
        esp32_serial_irq();
#endif
#if CONFIG_USB
    if (pending & BIT32(CPU_INT_USB))
        esp32_usb_irq();
#endif
#if CONFIG_CANBUS
    if (pending & BIT32(CPU_INT_CAN))
        esp32_can_irq();
#endif

    uint32_t known = BIT32(CPU_INT_SYSTIMER);
#if CONFIG_ESP32_SERIAL_UART0 || CONFIG_ESP32_SERIAL_UART1 \
    || CONFIG_ESP32_SERIAL_UART2
    known |= BIT32(CPU_INT_UART);
#endif
#if CONFIG_USB
    known |= BIT32(CPU_INT_USB);
#endif
#if CONFIG_CANBUS
    known |= BIT32(CPU_INT_CAN);
#endif
    if (pending & ~known)
        write_intenable(read_intenable() & ~(pending & ~known));
}
