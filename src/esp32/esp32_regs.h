#ifndef __ESP32_REGS_H
#define __ESP32_REGS_H

#include "autoconf.h"

#if CONFIG_MACH_ESP32C3
#include "esp32c3_regs.h"
#elif CONFIG_MACH_ESP32S3
#include "esp32s3_regs.h"
#else
#error Unsupported ESP32 processor
#endif

// Select the UART register block used by the Klipper serial interface.
#if CONFIG_ESP32_SERIAL_UART == 1
#define ESP_UART_BASE ESP_UART1_BASE
#elif CONFIG_ESP32_SERIAL_UART == 2
#define ESP_UART_BASE ESP_UART2_BASE
#else
#define ESP_UART_BASE ESP_UART0_BASE
#endif

#endif // esp32_regs.h
