// RISC-V interrupt support for ESP32-C3
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
    asm volatile("csrci mstatus, 8" ::: "memory");
}

void
irq_enable(void)
{
    asm volatile("csrsi mstatus, 8" ::: "memory");
}

irqstatus_t
irq_save(void)
{
    irqstatus_t flag;
    asm volatile("csrrci %0, mstatus, 8" : "=r"(flag) :: "memory");
    return flag;
}

void
irq_restore(irqstatus_t flag)
{
    if (flag & 8)
        irq_enable();
    else
        irq_disable();
}

void
irq_wait(void)
{
    asm volatile("csrsi mstatus, 8\n"
                 "wfi\n"
                 "csrci mstatus, 8" ::: "memory");
}

void
irq_poll(void)
{
}

void
esp32_irq_setup(uint32_t source, uint32_t cpu_intr, uint32_t priority)
{
    irqstatus_t flag = irq_save();
    REG32(INT_MATRIX_MAP(source)) = cpu_intr;
    REG32(INT_CPU_TYPE) &= ~BIT32(cpu_intr);
    REG32(INT_CPU_PRIORITY(cpu_intr)) = priority;
    REG32(INT_CPU_ENABLE) |= BIT32(cpu_intr);
    irq_restore(flag);
}

void
esp32_irq_disable(uint32_t cpu_intr)
{
    irqstatus_t flag = irq_save();
    REG32(INT_CPU_ENABLE) &= ~BIT32(cpu_intr);
    irq_restore(flag);
}

void
esp32_irq_reset(void)
{
    irq_disable();
    REG32(INT_CPU_ENABLE) = 0;
    REG32(INT_CPU_THRESHOLD) = 0;
    for (uint32_t source = 0; source < INT_SOURCE_COUNT; source++)
        REG32(INT_MATRIX_MAP(source)) = CPU_INT_DISABLED;
}

void __visible
esp32_handle_trap(uint32_t mcause)
{
    if (!(mcause & BIT32(31)))
        for (;;)
            ;

    switch (mcause & 0x1f) {
    case CPU_INT_SYSTIMER:
        esp32_timer_irq();
        break;
#if CONFIG_ESP32_SERIAL_UART0 || CONFIG_ESP32_SERIAL_UART1
    case CPU_INT_UART:
        esp32_serial_irq();
        break;
#endif
#if CONFIG_ESP32_USB_SERIAL_JTAG
    case CPU_INT_USB_SERIAL_JTAG:
        esp32_usb_serial_jtag_irq();
        break;
#endif
    default:
        // Mask unexpected interrupts.
        REG32(INT_CPU_ENABLE) &= ~BIT32(mcause & 0x1f);
        break;
    }
}
