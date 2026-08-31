// ESP32-C3 interrupt register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32C3_INTERRUPT_REGS_H
#define __ESP32C3_INTERRUPT_REGS_H

#define INT_MATRIX_MAP(source)     (ESP_INTERRUPT_BASE + 4u * (source))
#define INT_CPU_ENABLE             (ESP_INTERRUPT_BASE + 0x104u)
#define INT_CPU_TYPE               (ESP_INTERRUPT_BASE + 0x108u)
#define INT_CPU_CLEAR              (ESP_INTERRUPT_BASE + 0x10cu)
#define INT_CPU_PRIORITY(intr)     (ESP_INTERRUPT_BASE + 0x114u \
                                    + 4u * (intr))
#define INT_CPU_THRESHOLD          (ESP_INTERRUPT_BASE + 0x194u)
#define INT_SOURCE_UART0           21u
#define INT_SOURCE_UART1           22u
#define INT_SOURCE_USB_SERIAL_JTAG 26u
#define INT_SOURCE_SYSTIMER0       37u
#define INT_SOURCE_COUNT           62u
#define CPU_INT_DISABLED           0u
#define CPU_INT_SYSTIMER           1u
#define CPU_INT_UART               2u
#define CPU_INT_USB_SERIAL_JTAG    3u

#endif // esp32c3_interrupt_regs.h
