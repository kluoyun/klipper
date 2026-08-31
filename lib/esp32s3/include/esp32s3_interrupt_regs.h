// ESP32-S3 interrupt register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_INTERRUPT_REGS_H
#define __ESP32S3_INTERRUPT_REGS_H

#define INT_MATRIX_MAP(source)     (ESP_INTERRUPT_BASE + 4u * (source))
#define INT_SOURCE_UART0           27u
#define INT_SOURCE_UART1           28u
#define INT_SOURCE_UART2           29u
#define INT_SOURCE_TWAI            37u
#define INT_SOURCE_USB             38u
#define INT_SOURCE_SYSTIMER0       57u
#define INT_SOURCE_COUNT           99u
#define CPU_INT_DISABLED           4u
#define CPU_INT_SYSTIMER           6u
#define CPU_INT_UART               2u
#define CPU_INT_USB                3u
#define CPU_INT_CAN                5u

#endif // esp32s3_interrupt_regs.h
