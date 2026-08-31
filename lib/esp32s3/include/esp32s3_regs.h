// ESP32-S3 register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_REGS_H
#define __ESP32S3_REGS_H

#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define BIT32(bit) (1u << (bit))

#define ESP_SYSTEM_BASE            0x600c0000u
#define ESP_INTERRUPT_BASE         0x600c2000u
#define ESP_UART0_BASE             0x60000000u
#define ESP_UART1_BASE             0x60010000u
#define ESP_UART2_BASE             0x6002e000u
#define ESP_GPIO_BASE              0x60004000u
#define ESP_EFUSE_BASE             0x60007000u
#define ESP_RTC_BASE               0x60008000u
#define ESP_IOMUX_BASE             0x60009000u
#define ESP_TIMERG0_BASE           0x6001f000u
#define ESP_TIMERG1_BASE           0x60020000u
#define ESP_SYSTIMER_BASE          0x60023000u
#define ESP_USB_SERIAL_JTAG_BASE   0x60038000u
#define ESP_USB_WRAP_BASE          0x60039000u
#define ESP_USB_DWC_BASE           0x60080000u
#define ESP_SYSCON_BASE            0x60026000u
#define ESP_SENS_BASE              0x60008800u
#define ESP_I2C0_BASE              0x60013000u
#define ESP_I2C1_BASE              0x60027000u
#define ESP_LEDC_BASE              0x60019000u
#define ESP_SPI2_BASE              0x60024000u
#define ESP_SPI3_BASE              0x60025000u
#define ESP_TWAI_BASE              0x6002b000u

#include "esp32s3_system_regs.h"
#include "esp32s3_watchdog_regs.h"
#include "esp32s3_interrupt_regs.h"
#include "esp32s3_gpio_regs.h"
#include "esp32s3_uart_regs.h"
#include "esp32s3_systimer_regs.h"
#include "esp32s3_usb_regs.h"
#include "esp32s3_adc_regs.h"
#include "esp32s3_i2c_regs.h"
#include "esp32s3_spi_regs.h"
#include "esp32s3_ledc_regs.h"
#include "esp32s3_twai_regs.h"

#endif // esp32s3_regs.h
