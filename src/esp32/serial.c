// ESP32 UART support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h>
#include "autoconf.h"
#include "board/irq.h"
#include "board/serial_irq.h"
#include "esp32_regs.h"
#include "internal.h"
#include "sched.h"

#if CONFIG_ESP32_SERIAL_UART0
#define UART_CLOCK_BIT SYSTEM_UART0_BIT
#define UART_INT_SOURCE INT_SOURCE_UART0
#define UART_RX_SIGNAL GPIO_MATRIX_UART0_RX
#define UART_TX_SIGNAL GPIO_MATRIX_UART0_TX
#define UART_RX_PIN CONFIG_ESP32_SERIAL_RX_PIN
#define UART_TX_PIN CONFIG_ESP32_SERIAL_TX_PIN
#elif CONFIG_ESP32_SERIAL_UART1
#define UART_CLOCK_BIT SYSTEM_UART1_BIT
#define UART_INT_SOURCE INT_SOURCE_UART1
#define UART_RX_SIGNAL GPIO_MATRIX_UART1_RX
#define UART_TX_SIGNAL GPIO_MATRIX_UART1_TX
#define UART_RX_PIN CONFIG_ESP32_SERIAL_UART1_RX_PIN
#define UART_TX_PIN CONFIG_ESP32_SERIAL_UART1_TX_PIN
#elif CONFIG_ESP32_SERIAL_UART2
#define UART_CLOCK_BIT SYSTEM_UART2_BIT
#define UART_INT_SOURCE INT_SOURCE_UART2
#define UART_RX_SIGNAL GPIO_MATRIX_UART2_RX
#define UART_TX_SIGNAL GPIO_MATRIX_UART2_TX
#define UART_RX_PIN CONFIG_ESP32_SERIAL_UART2_RX_PIN
#define UART_TX_PIN CONFIG_ESP32_SERIAL_UART2_TX_PIN
#else
#error "ESP32 serial support requires a UART communication interface"
#endif

#define UART_RX_IRQS (UART_INT_RX_FULL | UART_INT_RX_ERROR)

static void
kick_tx(void)
{
    for (;;) {
        uint32_t status = REG32(UART_STATUS);
        uint32_t tx_count = (status >> UART_TX_COUNT_SHIFT) &
                            UART_TX_COUNT_MASK;
        if (tx_count >= UART_FIFO_SIZE) {
            REG32(UART_INT_ENABLE) = UART_RX_IRQS | UART_INT_TX_EMPTY;
            return;
        }
        uint8_t data;
        if (serial_get_tx_byte(&data)) {
            REG32(UART_INT_ENABLE) = UART_RX_IRQS;
            return;
        }
        REG32(UART_FIFO) = data;
    }
}

void
esp32_serial_irq(void)
{
    uint32_t status = REG32(UART_INT_STATUS);
    if (status & UART_RX_IRQS) {
        uint32_t count = REG32(UART_STATUS) & UART_RX_COUNT_MASK;
        while (count--)
            serial_rx_byte(REG32(UART_FIFO));
    }
    REG32(UART_INT_CLEAR) = status;
    if (status & UART_INT_TX_EMPTY)
        kick_tx();
}

void
serial_enable_tx_irq(void)
{
    irqstatus_t flag = irq_save();
    kick_tx();
    irq_restore(flag);
}

void
serial_init(void)
{
    irqstatus_t flag = irq_save();

    REG32(SYSTEM_PERIP_CLK_EN0) |= UART_CLOCK_BIT;
    REG32(UART_CLK_CONF) |= UART_CLK_CONF_CORE_RESET;
    REG32(SYSTEM_PERIP_RST_EN0) |= UART_CLOCK_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~UART_CLOCK_BIT;
    REG32(UART_CLK_CONF) &= ~UART_CLK_CONF_CORE_RESET;

    // Use the 40MHz crystal as the UART clock.
    uint32_t divider16 = (40000000u << 4) / CONFIG_SERIAL_BAUD;
    REG32(UART_CLK_DIV) = (divider16 >> 4) | ((divider16 & 0xf) << 20);
    REG32(UART_CLK_CONF) = UART_CLK_CONF_ENABLE | UART_CLK_CONF_XTAL;
    REG32(UART_CONF0) = UART_CONF0_8N1 | UART_CONF0_CLK_ENABLE;
    REG32(UART_CONF1) = 1u | (10u << UART_TX_EMPTY_THRESHOLD_SHIFT);
    REG32(UART_INT_ENABLE) = 0;
    REG32(UART_INT_CLEAR) = 0xfffffu;
    REG32(UART_ID) |= UART_ID_UPDATE;

    esp32_gpio_peripheral(UART_RX_PIN, UART_RX_SIGNAL, 0, 1);
    esp32_gpio_peripheral(UART_TX_PIN, UART_TX_SIGNAL, 1, 0);
    esp32_irq_setup(UART_INT_SOURCE, CPU_INT_UART, 1);
    REG32(UART_INT_ENABLE) = UART_RX_IRQS;
    irq_restore(flag);
}
DECL_INIT(serial_init);
