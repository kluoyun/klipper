// ESP32-S3 TWAI (classic CAN) support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <string.h>
#include "autoconf.h"
#include "board/irq.h"
#include "board/misc.h"
#include "command.h"
#include "esp32_regs.h"
#include "generic/canbus.h"
#include "internal.h"
#include "sched.h"

#define GPIO_STR_CAN_RX "gpio" __stringify(CONFIG_ESP32_CANBUS_GPIO_RX)
#define GPIO_STR_CAN_TX "gpio" __stringify(CONFIG_ESP32_CANBUS_GPIO_TX)
DECL_CONSTANT_STR("RESERVE_PINS_CAN", GPIO_STR_CAN_RX "," GPIO_STR_CAN_TX);

#define TWAI_ERROR_WARNING_LIMIT 96u
#define TWAI_ERROR_PASSIVE_LIMIT 128u
// Bound mode transitions so a faulty TWAI block cannot stall startup or
// gs_usb control requests indefinitely.
#define TWAI_MODE_TIMEOUT_LOOPS 100000u
// Limit the number of frames drained in one interrupt.
#define TWAI_RX_DRAIN_LIMIT 128u

static struct {
    struct canbus_msg tx_msg;
    uint32_t rx_error, tx_error, tx_retries;
    uint8_t tx_pending, recovering, started;
    uint8_t error_passive;
} CanState;

// TWAI configuration registers are writable only in reset mode.  Bound both
// transitions so a faulty peripheral cannot block startup or control handling.
static int
twai_wait_reset_state(uint8_t want_reset)
{
    for (uint32_t i = 0; i < TWAI_MODE_TIMEOUT_LOOPS; i++) {
        uint8_t in_reset = !!(TWAI_MODE & TWAI_MODE_RESET);
        if (in_reset == want_reset)
            return 0;
    }
    return -1;
}

static int
twai_enter_reset(void)
{
    TWAI_MODE |= TWAI_MODE_RESET;
    return twai_wait_reset_state(1);
}

static int
twai_exit_reset(void)
{
    TWAI_MODE &= ~TWAI_MODE_RESET;
    return twai_wait_reset_state(0);
}

static inline uint32_t
twai_buffer_read(uint32_t pos)
{
    return TWAI_BUFFER(pos) & 0xffu;
}

static inline void
twai_buffer_write(uint32_t pos, uint32_t value)
{
    TWAI_BUFFER(pos) = value & 0xffu;
}

static void
twai_write_frame(const struct canbus_msg *msg)
{
    uint32_t len = CANMSG_DATA_LEN(msg);
    uint32_t info = len;
    if (msg->id & CANMSG_ID_RTR)
        info |= TWAI_FRAME_RTR;
    twai_buffer_write(0, info | ((msg->id & CANMSG_ID_EFF)
                                 ? TWAI_FRAME_EXTENDED : 0));

    uint32_t data_pos;
    if (msg->id & CANMSG_ID_EFF) {
        uint32_t id = msg->id & 0x1fffffffu;
        twai_buffer_write(1, id >> 21);
        twai_buffer_write(2, id >> 13);
        twai_buffer_write(3, id >> 5);
        twai_buffer_write(4, id << 3);
        data_pos = 5;
    } else {
        uint32_t id = msg->id & 0x7ffu;
        twai_buffer_write(1, id >> 3);
        twai_buffer_write(2, id << 5);
        data_pos = 3;
    }
    if (!(msg->id & CANMSG_ID_RTR)) {
        for (uint32_t i = 0; i < len; i++)
            twai_buffer_write(data_pos + i, msg->data[i]);
    }
}

static void
twai_start_tx(const struct canbus_msg *msg)
{
    twai_write_frame(msg);
    TWAI_COMMAND = TWAI_CMD_TX;
}

int
canhw_send(struct canbus_msg *msg)
{
    irqstatus_t flag = irq_save();
    uint32_t status = TWAI_STATUS;
    // Error-passive CAN can still transmit.  Listen-only and recovery states
    // reject frames; BEI is masked after an error to avoid interrupt storms.
    if (!CanState.started || CanState.tx_pending || CanState.recovering
        || (TWAI_MODE & TWAI_MODE_LISTEN_ONLY)
        || !(status & TWAI_STATUS_TX_BUFFER)) {
        irq_restore(flag);
        return -1;
    }
    memcpy(&CanState.tx_msg, msg, sizeof(CanState.tx_msg));
    CanState.tx_pending = 1;
    twai_start_tx(msg);
    irq_restore(flag);
    return CANMSG_DATA_LEN(msg);
}

// Apply a bit-timing request from the Linux gs_usb driver.
int
esp32_can_set_bittiming(uint32_t prop_seg, uint32_t phase_seg1,
                        uint32_t phase_seg2, uint32_t sjw, uint32_t brp)
{
    uint32_t tseg1 = prop_seg + phase_seg1;
    if (brp < 2 || brp > 16384 || (brp & 1u)
        || !sjw || sjw > 4 || !phase_seg2 || phase_seg2 > 8
        || !tseg1 || tseg1 > 16 || sjw > phase_seg2)
        return -1;

    uint32_t btr0 = (brp / 2u - 1u) | ((sjw - 1u) << 14);
    uint32_t btr1 = (tseg1 - 1u) | ((phase_seg2 - 1u) << 4);
    irqstatus_t flag = irq_save();
    uint8_t was_started = CanState.started;
    uint32_t mode_flags = TWAI_MODE
                          & (TWAI_MODE_LISTEN_ONLY | TWAI_MODE_SELF_TEST);

    // Stop the controller before updating the write-protected timing
    // registers.  Any in-flight frame is discarded.
    if (twai_enter_reset()) {
        // Disable the source and fail closed if reset cannot be reached.
        TWAI_INTERRUPT_ENABLE = 0;
        CanState.started = 0;
        CanState.recovering = 1;
        CanState.tx_pending = 0;
        irq_restore(flag);
        canbus_notify_tx();
        return -1;
    }
    TWAI_COMMAND = TWAI_CMD_ABORT_TX;
    TWAI_INTERRUPT_ENABLE = 0;
    TWAI_BUS_TIMING0 = btr0;
    TWAI_BUS_TIMING1 = btr1;
    TWAI_RX_ERROR_COUNT = 0;
    TWAI_TX_ERROR_COUNT = 0;
    (void)TWAI_INTERRUPT;
    TWAI_COMMAND = TWAI_CMD_CLEAR_OVERRUN;
    TWAI_MODE = TWAI_MODE_RESET | TWAI_MODE_FILTER_SINGLE | mode_flags;
    CanState.tx_pending = 0;
    CanState.recovering = 0;
    CanState.error_passive = !!(mode_flags & TWAI_MODE_LISTEN_ONLY);
    if (was_started) {
        esp32_irq_setup(INT_SOURCE_TWAI, CPU_INT_CAN, 1);
        TWAI_INTERRUPT_ENABLE = TWAI_INTR_ALL;
        // Preserve mode flags across a timing update.  S3 listen-only mode
        // requires REC=128 before reset is released.
        TWAI_RX_ERROR_COUNT = (mode_flags & TWAI_MODE_LISTEN_ONLY
                               ? TWAI_ERROR_PASSIVE_LIMIT : 0);
        TWAI_TX_ERROR_COUNT = 0;
        if (twai_exit_reset()) {
            TWAI_INTERRUPT_ENABLE = 0;
            CanState.started = 0;
            CanState.recovering = 1;
            irq_restore(flag);
            canbus_notify_tx();
            return -1;
        }
        CanState.started = 1;
    }
    irq_restore(flag);
    if (was_started)
        canbus_notify_tx();
    return 0;
}

// Apply gs_usb channel mode and listen-only/loopback flags.
int
esp32_can_set_mode(uint32_t mode, uint32_t flags)
{
    irqstatus_t flag = irq_save();
    if (mode == 0) {
        int reset_error = twai_enter_reset();
        TWAI_COMMAND = TWAI_CMD_ABORT_TX;
        TWAI_INTERRUPT_ENABLE = 0;
        CanState.started = 0;
        CanState.tx_pending = 0;
        CanState.recovering = 0;
        CanState.error_passive = 0;
        if (reset_error) {
            CanState.recovering = 1;
        }
        irq_restore(flag);
        if (reset_error)
            canbus_notify_tx();
        return reset_error ? -1 : 0;
    }
    if (mode != 1) {
        irq_restore(flag);
        return -1;
    }

    uint32_t twai_mode = TWAI_MODE_RESET | TWAI_MODE_FILTER_SINGLE;
    // gs_usb bit 0 selects listen-only; bit 1 selects loopback/self-test.
    if (flags & BIT32(0))
        twai_mode |= TWAI_MODE_LISTEN_ONLY;
    if (flags & BIT32(1))
        twai_mode |= TWAI_MODE_SELF_TEST;

    // Enter reset before changing protected registers.  Bus-off also enters
    // reset autonomously.
    if (twai_enter_reset()) {
        TWAI_INTERRUPT_ENABLE = 0;
        CanState.started = 0;
        CanState.recovering = 1;
        CanState.tx_pending = 0;
        CanState.error_passive = 1;
        irq_restore(flag);
        canbus_notify_tx();
        return -1;
    }
    TWAI_MODE = twai_mode;
    TWAI_INTERRUPT_ENABLE = 0;
    (void)TWAI_INTERRUPT;
    // Clear pending interrupts and RX overrun while reset is asserted.
    TWAI_COMMAND = TWAI_CMD_CLEAR_OVERRUN;
    // Reset the error counters for a clean restart.  S3 listen-only mode
    // requires REC=128 before reset is released.
    uint8_t listen_only = !!(flags & BIT32(0));
    TWAI_RX_ERROR_COUNT = listen_only ? TWAI_ERROR_PASSIVE_LIMIT : 0;
    TWAI_TX_ERROR_COUNT = 0;
    CanState.tx_pending = 0;
    CanState.recovering = 0;
    CanState.error_passive = listen_only;
    esp32_irq_setup(INT_SOURCE_TWAI, CPU_INT_CAN, 1);
    TWAI_INTERRUPT_ENABLE = TWAI_INTR_ALL;
    if (twai_exit_reset()) {
        TWAI_INTERRUPT_ENABLE = 0;
        CanState.started = 0;
        CanState.recovering = 1;
        CanState.error_passive = 1;
        irq_restore(flag);
        canbus_notify_tx();
        return -1;
    }
    // Do not advertise a running channel until the reset edge has completed.
    CanState.started = 1;
    irq_restore(flag);
    return 0;
}

void
canhw_set_filter(uint32_t id)
{
    // Filtering is handled by the generic CAN layer.
    (void)id;
}

void
canhw_get_status(struct canbus_status *status)
{
    irqstatus_t flag = irq_save();
    uint32_t hw_status = TWAI_STATUS;
    uint32_t rx_count = TWAI_RX_ERROR_COUNT & 0xffu;
    uint32_t tx_count = TWAI_TX_ERROR_COUNT & 0xffu;
    status->rx_error = CanState.rx_error;
    status->tx_error = CanState.tx_error;
    status->tx_retries = CanState.tx_retries;
    uint8_t recovering = CanState.recovering;
    irq_restore(flag);

    if (recovering || (hw_status & TWAI_STATUS_BUS_OFF))
        status->bus_state = CANBUS_STATE_OFF;
    else if (rx_count >= TWAI_ERROR_PASSIVE_LIMIT
             || tx_count >= TWAI_ERROR_PASSIVE_LIMIT)
        status->bus_state = CANBUS_STATE_PASSIVE;
    else if (hw_status & TWAI_STATUS_ERROR)
        status->bus_state = CANBUS_STATE_WARN;
    else
        status->bus_state = CANBUS_STATE_ACTIVE;
}

static void
twai_process_rx(void)
{
    uint32_t budget = TWAI_RX_DRAIN_LIMIT;
    while ((TWAI_RX_MESSAGE_COUNT & 0x7fu) && budget--) {
        uint32_t status = TWAI_STATUS;
        if (status & TWAI_STATUS_MISS) {
            TWAI_COMMAND = TWAI_CMD_RELEASE_RX;
            CanState.rx_error++;
            continue;
        }

        uint32_t info = twai_buffer_read(0);
        struct canbus_msg msg = {};
        uint32_t data_pos;
        if (info & TWAI_FRAME_EXTENDED) {
            msg.id = ((twai_buffer_read(1) << 21)
                      | (twai_buffer_read(2) << 13)
                      | (twai_buffer_read(3) << 5)
                      | (twai_buffer_read(4) >> 3)) & 0x1fffffffu;
            msg.id |= CANMSG_ID_EFF;
            data_pos = 5;
        } else {
            msg.id = ((twai_buffer_read(1) << 3)
                      | (twai_buffer_read(2) >> 5)) & 0x7ffu;
            data_pos = 3;
        }
        if (info & TWAI_FRAME_RTR)
            msg.id |= CANMSG_ID_RTR;
        msg.dlc = info & 0x0fu;
        uint32_t len = CANMSG_DATA_LEN(&msg);
        if (!(info & TWAI_FRAME_RTR)) {
            for (uint32_t i = 0; i < len; i++)
                msg.data[i] = twai_buffer_read(data_pos + i);
        }
        TWAI_COMMAND = TWAI_CMD_RELEASE_RX;
        canbus_process_data(&msg);
    }

    uint32_t remaining = TWAI_RX_MESSAGE_COUNT & 0x7fu;
    if (remaining) {
        // A nonzero count after the bounded drain indicates a stuck or
        // corrupted FIFO.  Mask RX interrupts before attempting cleanup.
        TWAI_INTERRUPT_ENABLE &= ~TWAI_INTR_RX;
        uint32_t cleanup = TWAI_RX_DRAIN_LIMIT;
        while ((TWAI_RX_MESSAGE_COUNT & 0x7fu) && cleanup--)
            TWAI_COMMAND = TWAI_CMD_RELEASE_RX;
        remaining = TWAI_RX_MESSAGE_COUNT & 0x7fu;
        TWAI_COMMAND = TWAI_CMD_CLEAR_OVERRUN;
        CanState.rx_error++;

        if (remaining) {
            // Hold the controller in reset/listen-only until the host sends
            // an explicit MODE restart.
            TWAI_MODE |= TWAI_MODE_RESET | TWAI_MODE_LISTEN_ONLY;
            TWAI_INTERRUPT_ENABLE = 0;
            (void)TWAI_INTERRUPT;
            CanState.started = 0;
            CanState.recovering = 1;
            CanState.error_passive = 1;
            CanState.tx_pending = 0;
            canbus_notify_tx();
            return;
        }

        // Resume RX interrupts after successful cleanup.
        if (CanState.started && !CanState.recovering)
            TWAI_INTERRUPT_ENABLE |= TWAI_INTR_RX;
    }

    if (TWAI_STATUS & TWAI_STATUS_OVERRUN) {
        TWAI_COMMAND = TWAI_CMD_CLEAR_OVERRUN;
        CanState.rx_error++;
    }
}

void
esp32_can_irq(void)
{
    // Reading TWAI_INTERRUPT clears edge-triggered events.
    uint32_t intr = TWAI_INTERRUPT;
    uint32_t status = TWAI_STATUS;

    if (intr & TWAI_INTR_RX)
        twai_process_rx();

    if (intr & TWAI_INTR_BUS_ERROR) {
        uint32_t error = TWAI_ERROR_CAPTURE;
        if (error & BIT32(5))
            CanState.rx_error++;
        else {
            CanState.tx_error++;
            CanState.tx_retries++;
        }
        // BEI may fire for every failed bit or ACK.  Mask it after an error
        // and rely on EI/EPI for state changes.
        TWAI_INTERRUPT_ENABLE &= ~TWAI_INTR_BUS_ERROR;
    }
    if (intr & TWAI_INTR_ARB_LOST) {
        (void)TWAI_ARB_LOST_CAPTURE;
        CanState.tx_retries++;
    }
    if (intr & TWAI_INTR_PASSIVE) {
        uint32_t rx_count = TWAI_RX_ERROR_COUNT & 0xffu;
        uint32_t tx_count = TWAI_TX_ERROR_COUNT & 0xffu;
        if (rx_count >= TWAI_ERROR_PASSIVE_LIMIT
            || tx_count >= TWAI_ERROR_PASSIVE_LIMIT) {
            CanState.error_passive = 1;
            // Avoid repeated BEI interrupts while a passive controller
            // retries on a missing or quiet bus.
            TWAI_INTERRUPT_ENABLE &= ~TWAI_INTR_BUS_ERROR;
        } else {
            // EPI also reports recovery to the error-active state.
            CanState.error_passive = 0;
            TWAI_INTERRUPT_ENABLE |= TWAI_INTR_BUS_ERROR;
        }
    }

    uint8_t recovery_complete = 0;
    status = TWAI_STATUS;
    if ((intr & TWAI_INTR_ERROR) && (status & TWAI_STATUS_BUS_OFF)) {
        // Keep bus-off stopped in reset/listen-only until the host explicitly
        // restarts the channel.
        CanState.recovering = 1;
        CanState.error_passive = 1;
        TWAI_MODE |= TWAI_MODE_RESET | TWAI_MODE_LISTEN_ONLY;
        TWAI_INTERRUPT_ENABLE = 0;
        (void)TWAI_INTERRUPT;
        if (CanState.tx_pending) {
            // Explicitly abort TX so software and controller state agree.
            TWAI_COMMAND = TWAI_CMD_ABORT_TX;
            CanState.tx_pending = 0;
        }
        canbus_notify_tx();
    } else if (CanState.recovering && !(TWAI_MODE & TWAI_MODE_RESET)
               && !(status & TWAI_STATUS_BUS_OFF)) {
        // MODE normally performs recovery; handle an autonomous reset exit
        // defensively.
        CanState.recovering = 0;
        CanState.error_passive = 0;
        recovery_complete = 1;
        TWAI_INTERRUPT_ENABLE = TWAI_INTR_ALL;
        canbus_notify_tx();
    }

    if ((intr & TWAI_INTR_TX) && !recovery_complete
        && !(status & TWAI_STATUS_BUS_OFF)
        && !CanState.recovering && CanState.tx_pending
        && (status & TWAI_STATUS_TX_COMPLETE)) {
        CanState.tx_pending = 0;
        canbus_notify_tx();
    }
}

static int
twai_compute_timing(uint32_t bitrate, uint32_t *btr0, uint32_t *btr1)
{
    if (!bitrate || TWAI_APB_CLOCK % bitrate)
        return -1;
    uint32_t bit_clocks = TWAI_APB_CLOCK / bitrate;
    uint32_t target = bitrate >= 800000u ? 750u
                      : bitrate >= 500000u ? 800u : 875u;
    uint32_t best_error = 1000u, best_brp = 0;
    uint32_t best_tseg1 = 0, best_tseg2 = 0;

    // Find the closest standard sample point for an even BRP.
    for (uint32_t brp = 2; brp <= 16384u; brp += 2) {
        if (bit_clocks % brp)
            continue;
        uint32_t total = bit_clocks / brp;
        if (total < 3 || total > 25)
            continue;
        uint32_t sample = (total * target + 500u) / 1000u;
        if (sample < 2)
            sample = 2;
        if (sample > 17)
            sample = 17;
        uint32_t tseg1 = sample - 1u;
        uint32_t tseg2 = total - sample;
        if (!tseg1 || tseg1 > 16 || !tseg2 || tseg2 > 8)
            continue;
        uint32_t actual = sample * 1000u / total;
        uint32_t error = actual > target ? actual - target : target - actual;
        if (error < best_error) {
            best_error = error;
            best_brp = brp;
            best_tseg1 = tseg1;
            best_tseg2 = tseg2;
        }
    }
    if (!best_brp)
        return -1;

    uint32_t sjw = best_tseg2 >= 2 ? 2 : 1;
    *btr0 = (best_brp / 2u - 1u) | ((sjw - 1u) << 14);
    *btr1 = (best_tseg1 - 1u) | ((best_tseg2 - 1u) << 4);
    return 0;
}

void
can_init(void)
{
    uint32_t btr0, btr1;
    if (twai_compute_timing(CONFIG_CANBUS_FREQUENCY, &btr0, &btr1))
        shutdown("Unsupported ESP32-S3 CAN bitrate");

    REG32(SYSTEM_PERIP_CLK_EN0) |= SYSTEM_TWAI_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) |= SYSTEM_TWAI_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~SYSTEM_TWAI_BIT;
    esp32_gpio_peripheral(CONFIG_ESP32_CANBUS_GPIO_RX,
                          GPIO_MATRIX_TWAI, 0, 1);
    esp32_gpio_peripheral(CONFIG_ESP32_CANBUS_GPIO_TX,
                          GPIO_MATRIX_TWAI, 1, 0);

    CanState.started = 0;
    if (twai_enter_reset()) {
        TWAI_INTERRUPT_ENABLE = 0;
        shutdown("ESP32-S3 CAN reset timeout");
    }
    TWAI_INTERRUPT_ENABLE = 0;
    TWAI_BUS_TIMING0 = btr0;
    TWAI_BUS_TIMING1 = btr1;
    TWAI_ERROR_WARNING = TWAI_ERROR_WARNING_LIMIT;
    TWAI_RX_ERROR_COUNT = 0;
    TWAI_TX_ERROR_COUNT = 0;
    // Accept all identifiers.
    for (uint32_t i = 0; i < 4; i++) {
        twai_buffer_write(i, 0);
        twai_buffer_write(4 + i, 0xffu);
    }
    TWAI_MODE = TWAI_MODE_RESET | TWAI_MODE_FILTER_SINGLE;
    TWAI_CLOCK_DIVIDER = BIT32(8);
    (void)TWAI_INTERRUPT;
    TWAI_COMMAND = TWAI_CMD_CLEAR_OVERRUN;

    esp32_irq_setup(INT_SOURCE_TWAI, CPU_INT_CAN, 1);
    TWAI_INTERRUPT_ENABLE = TWAI_INTR_ALL;
    // Start error-active.  MODE requests configure listen-only later.
    if (twai_exit_reset()) {
        TWAI_INTERRUPT_ENABLE = 0;
        shutdown("ESP32-S3 CAN start timeout");
    }
    CanState.started = 1;
    CanState.error_passive = 0;
}
DECL_INIT(can_init);
