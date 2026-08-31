// ESP32-C3 fixed-function USB Serial/JTAG CDC-ACM support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <stdint.h>
#include "board/irq.h"
#include "board/serial_irq.h"
#include "command.h"
#include "esp32_regs.h"
#include "internal.h"
#include "sched.h"

#define USB_SERIAL_JTAG_PACKET_SIZE 64u
#define USB_SERIAL_JTAG_RX_IRQS USB_SERIAL_JTAG_INT_RX

DECL_CONSTANT_STR("RESERVE_PINS_USB", "gpio18,gpio19");

static uint8_t tx_needs_zlp;

static void
kick_tx(void)
{
    if (!(REG32(USB_SERIAL_JTAG_EP1_CONF) & USB_SERIAL_JTAG_TX_FREE)) {
        REG32(USB_SERIAL_JTAG_INT_ENABLE) = (
            USB_SERIAL_JTAG_RX_IRQS | USB_SERIAL_JTAG_INT_TX);
        return;
    }

    uint_fast8_t count = 0;
    while (count < USB_SERIAL_JTAG_PACKET_SIZE) {
        uint8_t data;
        if (serial_get_tx_byte(&data)) {
            if (count || tx_needs_zlp)
                // Commit a short packet or a terminating ZLP.
                REG32(USB_SERIAL_JTAG_EP1_CONF) = USB_SERIAL_JTAG_WR_DONE;
            tx_needs_zlp = 0;
            REG32(USB_SERIAL_JTAG_INT_ENABLE) = USB_SERIAL_JTAG_RX_IRQS;
            return;
        }
        REG32(USB_SERIAL_JTAG_EP1) = data;
        count++;
    }

    // Hardware commits full packets; emit a ZLP if no more data follows.
    tx_needs_zlp = 1;
    REG32(USB_SERIAL_JTAG_INT_ENABLE) = (
        USB_SERIAL_JTAG_RX_IRQS | USB_SERIAL_JTAG_INT_TX);
}

void
esp32_usb_serial_jtag_irq(void)
{
    uint32_t status = REG32(USB_SERIAL_JTAG_INT_STATUS);
    REG32(USB_SERIAL_JTAG_INT_CLEAR) = status;

    if (status & USB_SERIAL_JTAG_INT_RX) {
        while (REG32(USB_SERIAL_JTAG_EP1_CONF) & USB_SERIAL_JTAG_RX_AVAIL)
            serial_rx_byte(REG32(USB_SERIAL_JTAG_EP1));
    }
    if (status & USB_SERIAL_JTAG_INT_TX)
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

    // Keep the ROM descriptors and endpoint state.
    REG32(SYSTEM_PERIP_CLK_EN0) |= SYSTEM_USB_DEVICE_BIT;
    REG32(USB_SERIAL_JTAG_CONF0) = (
        (REG32(USB_SERIAL_JTAG_CONF0) & ~USB_SERIAL_JTAG_PHY_SEL)
        | USB_SERIAL_JTAG_PAD_ENABLE);
    REG32(USB_SERIAL_JTAG_INT_ENABLE) = 0;

    esp32_irq_setup(INT_SOURCE_USB_SERIAL_JTAG,
                    CPU_INT_USB_SERIAL_JTAG, 1);

    // Preserve pending RX data and the initial TX-empty state.
    REG32(USB_SERIAL_JTAG_INT_ENABLE) = USB_SERIAL_JTAG_RX_IRQS;
    irq_restore(flag);
}
DECL_INIT(serial_init);
