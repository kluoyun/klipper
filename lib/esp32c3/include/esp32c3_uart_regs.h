// ESP32-C3 UART register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32C3_UART_REGS_H
#define __ESP32C3_UART_REGS_H

#define UART_FIFO                  (ESP_UART_BASE + 0x000u)
#define UART_INT_RAW               (ESP_UART_BASE + 0x004u)
#define UART_INT_STATUS            (ESP_UART_BASE + 0x008u)
#define UART_INT_ENABLE            (ESP_UART_BASE + 0x00cu)
#define UART_INT_CLEAR             (ESP_UART_BASE + 0x010u)
#define UART_CLK_DIV               (ESP_UART_BASE + 0x014u)
#define UART_STATUS                (ESP_UART_BASE + 0x01cu)
#define UART_CONF0                 (ESP_UART_BASE + 0x020u)
#define UART_CONF1                 (ESP_UART_BASE + 0x024u)
#define UART_CLK_CONF              (ESP_UART_BASE + 0x078u)
#define UART_ID                    (ESP_UART_BASE + 0x080u)
#define UART0_STATUS               (ESP_UART0_BASE + 0x01cu)
#define UART_INT_RX_FULL           BIT32(0)
#define UART_INT_TX_EMPTY          BIT32(1)
#define UART_INT_RX_ERROR          (BIT32(2) | BIT32(3) | BIT32(4))
#define UART_RX_COUNT_MASK         0x3ffu
#define UART_TX_COUNT_SHIFT        16u
#define UART_TX_COUNT_MASK         0x3ffu
#define UART_FIFO_SIZE             128u
#define UART_TX_EMPTY_THRESHOLD_SHIFT 9u
#define UART_CONF0_8N1             ((3u << 2) | (1u << 4))
#define UART_CONF0_CLK_ENABLE      (BIT32(25) | BIT32(28))
#define UART_CLK_CONF_ENABLE       (BIT32(22) | BIT32(24) | BIT32(25))
#define UART_CLK_CONF_XTAL         (3u << 20)
#define UART_CLK_CONF_CORE_RESET   BIT32(23)
#define UART_ID_UPDATE             BIT32(31)

#endif // esp32c3_uart_regs.h
