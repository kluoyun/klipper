// Native USB CDC support using the ESP32-S3 DWC2 USB-OTG controller
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <string.h>
#include "autoconf.h"
#include "board/irq.h"
#include "board/misc.h"
#include "board/usb_cdc.h"
#include "board/usb_cdc_ep.h"
#include "command.h"
#include "esp32_regs.h"
#include "generic/usbstd.h"
#include "internal.h"
#include "sched.h"

DECL_CONSTANT_STR("RESERVE_PINS_USB", "gpio19,gpio20");

#if CONFIG_MACH_ESP32S3 && CONFIG_USBCANBUS
// Handle S3-specific gs_usb requests here without changing the generic
// protocol layer.
#define ESP32_GS_BREQ_BITTIMING 1u
#define ESP32_GS_BREQ_MODE      2u
#define ESP32_GS_BREQ_BT_CONST  4u
#define ESP32_GS_FEATURE_LISTEN_ONLY BIT32(0)
#define ESP32_GS_FEATURE_LOOP_BACK   BIT32(1)

extern int esp32_can_set_bittiming(uint32_t prop_seg, uint32_t phase_seg1,
                                   uint32_t phase_seg2, uint32_t sjw,
                                   uint32_t brp);
extern int esp32_can_set_mode(uint32_t mode, uint32_t flags);

static uint8_t esp32_gs_setup_valid;
static uint8_t esp32_gs_setup_type;
static uint8_t esp32_gs_setup_request;
static uint16_t esp32_gs_setup_length;
static uint8_t esp32_gs_out_applied;
static uint16_t esp32_gs_out_received;
static uint16_t esp32_gs_in_offset;
static uint8_t esp32_gs_bt_const_ready;
// Buffer control OUT data until the complete request arrives.  This also
// handles hosts that split a request across multiple EP0 packets.
static uint8_t esp32_gs_out_buf[64] __attribute__((aligned(4)));
static uint8_t esp32_gs_bt_const[40] __attribute__((aligned(4)));

static uint32_t
esp32_gs_get_le32(const uint8_t *p)
{
    return ((uint32_t)p[0] | ((uint32_t)p[1] << 8)
            | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void
esp32_gs_put_le32(uint8_t *p, uint32_t value)
{
    p[0] = value;
    p[1] = value >> 8;
    p[2] = value >> 16;
    p[3] = value >> 24;
}

static void
esp32_gs_track_setup(const uint8_t *setup)
{
    esp32_gs_setup_type = setup[0];
    esp32_gs_setup_request = setup[1];
    esp32_gs_setup_length = ((uint16_t)setup[6]
                             | ((uint16_t)setup[7] << 8));
    esp32_gs_setup_valid = ((setup[0] & 0x60u) == 0x40u);
    esp32_gs_out_applied = 0;
    esp32_gs_out_received = 0;
    esp32_gs_in_offset = 0;
    esp32_gs_bt_const_ready = 0;
}

static void
esp32_gs_apply_out(const uint8_t *data, uint_fast8_t len)
{
    if (!esp32_gs_setup_valid || (esp32_gs_setup_type & USB_DIR_IN)
        || esp32_gs_out_applied || !len)
        return;

    // Reject requests larger than the local buffer without overrunning it.
    if (esp32_gs_out_received > sizeof(esp32_gs_out_buf)
        || len > sizeof(esp32_gs_out_buf) - esp32_gs_out_received) {
        esp32_gs_out_applied = 1;
        return;
    }
    memcpy(&esp32_gs_out_buf[esp32_gs_out_received], data, len);
    esp32_gs_out_received += len;
    if (esp32_gs_out_received < esp32_gs_setup_length)
        return;

    if (esp32_gs_setup_request == ESP32_GS_BREQ_BITTIMING
        && esp32_gs_out_received >= 20) {
        (void)esp32_can_set_bittiming(
            esp32_gs_get_le32(&esp32_gs_out_buf[0]),
            esp32_gs_get_le32(&esp32_gs_out_buf[4]),
            esp32_gs_get_le32(&esp32_gs_out_buf[8]),
            esp32_gs_get_le32(&esp32_gs_out_buf[12]),
            esp32_gs_get_le32(&esp32_gs_out_buf[16]));
        esp32_gs_out_applied = 1;
    } else if (esp32_gs_setup_request == ESP32_GS_BREQ_MODE
               && esp32_gs_out_received >= 8) {
        uint32_t mode = esp32_gs_get_le32(&esp32_gs_out_buf[0]);
        uint32_t flags = esp32_gs_get_le32(&esp32_gs_out_buf[4]);
        (void)esp32_can_set_mode(mode, flags);
        esp32_gs_out_applied = 1;
    } else if (esp32_gs_out_received >= esp32_gs_setup_length) {
        // Unknown/short vendor OUT requests are left to the generic router;
        // do not repeatedly consume later control data as part of this one.
        esp32_gs_out_applied = 1;
    }
}

static const uint8_t *
esp32_gs_ep0_data(const void *data, uint_fast8_t len)
{
    if (!esp32_gs_setup_valid
        || !(esp32_gs_setup_type & USB_DIR_IN)
        || esp32_gs_setup_request != ESP32_GS_BREQ_BT_CONST
        || !len
        || esp32_gs_in_offset > sizeof(esp32_gs_bt_const)
        || len > sizeof(esp32_gs_bt_const) - esp32_gs_in_offset)
        return data;

    if (!esp32_gs_bt_const_ready) {
        // Report the S3 TWAI clock and timing limits used by Linux.
        memset(esp32_gs_bt_const, 0, sizeof(esp32_gs_bt_const));
        esp32_gs_put_le32(&esp32_gs_bt_const[0],
                          ESP32_GS_FEATURE_LISTEN_ONLY
                          | ESP32_GS_FEATURE_LOOP_BACK);
        esp32_gs_put_le32(&esp32_gs_bt_const[4], 80000000u);
        esp32_gs_put_le32(&esp32_gs_bt_const[8], 1u);
        esp32_gs_put_le32(&esp32_gs_bt_const[12], 16u);
        esp32_gs_put_le32(&esp32_gs_bt_const[16], 1u);
        esp32_gs_put_le32(&esp32_gs_bt_const[20], 8u);
        esp32_gs_put_le32(&esp32_gs_bt_const[24], 4u);
        esp32_gs_put_le32(&esp32_gs_bt_const[28], 2u);
        esp32_gs_put_le32(&esp32_gs_bt_const[32], 16384u);
        esp32_gs_put_le32(&esp32_gs_bt_const[36], 2u);
        esp32_gs_bt_const_ready = 1;
    }
    const uint8_t *ret = &esp32_gs_bt_const[esp32_gs_in_offset];
    // Advance the offset only after the packet is accepted by the endpoint.
    esp32_gs_in_offset += len;
    return ret;
}
#endif


/****************************************************************
 * FIFO and endpoint access
 ****************************************************************/

static uint32_t ep0_tx_len;
static uint8_t ep0_setup_buf[8], ep0_out_buf[USB_CDC_EP0_SIZE];
static uint8_t ep0_setup_ready, ep0_out_ready, ep0_out_len;

#define BULK_OUT_SLOTS 32u
#define BULK_OUT_SLOT_MASK (BULK_OUT_SLOTS - 1u)
_Static_assert(!(BULK_OUT_SLOTS & BULK_OUT_SLOT_MASK),
               "BULK_OUT_SLOTS must be a power of two");
_Static_assert(USB_CDC_EP_BULK_IN_MAX_XFER
               <= 4 * USB_CDC_EP_BULK_IN_SIZE,
               "EP1 FIFO only holds four packets");
static uint8_t bulk_out_buf[BULK_OUT_SLOTS][USB_CDC_EP_BULK_OUT_SIZE]
    __attribute__((aligned(4)));
static uint8_t bulk_out_len[BULK_OUT_SLOTS];
static uint8_t bulk_out_head, bulk_out_tail;

// Retain frames until the DWC2 endpoint accepts them.  Queueing avoids
// blocking the protocol task while an earlier transfer is in flight.
#define BULK_IN_SLOTS 8u
#define BULK_IN_SLOT_MASK (BULK_IN_SLOTS - 1u)
_Static_assert(!(BULK_IN_SLOTS & BULK_IN_SLOT_MASK),
               "BULK_IN_SLOTS must be a power of two");
static uint8_t bulk_in_buf[BULK_IN_SLOTS][USB_CDC_EP_BULK_IN_MAX_XFER]
    __attribute__((aligned(4)));
static uint8_t bulk_in_len[BULK_IN_SLOTS];
static uint8_t bulk_in_head, bulk_in_tail;
static uint8_t bulk_in_hw_pending;
static uint8_t bulk_in_recover_pending;
static uint8_t usb_rx_recover_pending;
static uint8_t usb_recovery_timer_active;
static uint_fast8_t usb_recovery_timer_event(struct timer *t);
static struct timer usb_recovery_timer = {
    .func = usb_recovery_timer_event,
};
static void bulk_in_kick(void);
static uint8_t bulk_in_recover(void);
static uint8_t usb_flush(uint32_t command);

static uint_fast8_t
usb_recovery_timer_event(struct timer *t)
{
    (void)t;
    usb_recovery_timer_active = 0;
    sched_wake_tasks();
    return SF_DONE;
}

static void
usb_schedule_recovery(void)
{
    irqstatus_t flag = irq_save();
    if (!usb_recovery_timer_active) {
        usb_recovery_timer.waketime = (timer_read_time()
                                       + timer_from_us(10000));
        usb_recovery_timer_active = 1;
        sched_add_timer(&usb_recovery_timer);
    }
    irq_restore(flag);
}

// Bound controller waits.  Endpoint recovery uses a shorter budget than
// core/FIFO initialization.
#define USB_EP_WAIT_LOOPS       10000u
#define USB_CORE_WAIT_LOOPS     100000u
#define USB_RX_DRAIN_LIMIT      256u

// Retry endpoint recovery and queued IN transfers outside the USB ISR.
static uint32_t usb_recovery_poll_time;
void
usb_recovery_task(void)
{
    uint32_t now = timer_read_time();
    if ((uint32_t)(now - usb_recovery_poll_time) < timer_from_us(10000)
        && !bulk_in_recover_pending && !usb_rx_recover_pending)
        return;
    usb_recovery_poll_time = now;

    // Keep interrupts enabled while polling so a wedged endpoint cannot delay
    // CAN or timer interrupts.
    if (bulk_in_recover_pending && bulk_in_recover()) {
        irqstatus_t flag = irq_save();
        // A USB reset may have reinitialized the endpoint while polling.
        if (bulk_in_recover_pending) {
            bulk_in_recover_pending = 0;
            uint32_t ctl = USB_DIEPCTL(USB_CDC_EP_BULK_IN);
            if ((ctl & USB_EPCTL_ACTIVE)
                && bulk_in_tail != bulk_in_head)
                USB_DAINTMSK |= BIT32(USB_CDC_EP_BULK_IN);
        }
        irq_restore(flag);
    } else if (bulk_in_recover_pending) {
        usb_schedule_recovery();
    }

    if (usb_rx_recover_pending && usb_flush(USB_GRSTCTL_RX_FLUSH)) {
        irqstatus_t flag = irq_save();
        if (usb_rx_recover_pending) {
            usb_rx_recover_pending = 0;
            USB_GINTMSK |= USB_GINT_RXFLVL;
        }
        irq_restore(flag);
    } else if (usb_rx_recover_pending) {
        usb_schedule_recovery();
    }

    irqstatus_t flag = irq_save();
    bulk_in_kick();
    irq_restore(flag);
}
DECL_TASK(usb_recovery_task);

static uint8_t
usb_flush(uint32_t command)
{
    USB_GRSTCTL = command;
    uint32_t busy = command & (USB_GRSTCTL_RX_FLUSH
                               | USB_GRSTCTL_TX_FLUSH);
    uint32_t timeout = USB_CORE_WAIT_LOOPS;
    while ((USB_GRSTCTL & busy) && timeout--)
        ;
    if (USB_GRSTCTL & busy)
        return 0;
    return 1;
}

// Abort a failed transfer, flush EP1's FIFO, and restore its configured state.
static uint8_t
bulk_in_recover(void)
{
    uint32_t ep = USB_CDC_EP_BULK_IN;
    uint32_t ctl = USB_DIEPCTL(ep);
    if (ctl & USB_EPCTL_ENABLE) {
        // Stop accepting IN tokens before disabling the endpoint.
        if (!(ctl & USB_EPCTL_NAKSTS)) {
            USB_DIEPCTL(ep) = ctl | USB_EPCTL_SNAK;
            uint32_t timeout = USB_EP_WAIT_LOOPS;
            while (!(USB_DIEPINT(ep) & USB_DIEPINT_INEPNAKEFF)
                   && timeout--)
                ;
            uint32_t nak_status = USB_DIEPINT(ep);
            if (nak_status & USB_DIEPINT_INEPNAKEFF)
                USB_DIEPINT(ep) = USB_DIEPINT_INEPNAKEFF;
        }

        ctl = USB_DIEPCTL(ep);
        USB_DIEPCTL(ep) = ctl | USB_EPCTL_DISABLE | USB_EPCTL_SNAK;
        uint32_t timeout = USB_EP_WAIT_LOOPS;
        while (!(USB_DIEPINT(ep) & USB_DIEPINT_EP_DISABLED)
               && (USB_DIEPCTL(ep) & USB_EPCTL_ENABLE) && timeout--)
            ;
        uint32_t ctl_after = USB_DIEPCTL(ep);
        uint32_t int_after = USB_DIEPINT(ep);
        if (int_after & USB_DIEPINT_EP_DISABLED)
            USB_DIEPINT(ep) = USB_DIEPINT_EP_DISABLED;
        if (ctl_after & USB_EPCTL_ENABLE)
            return 0;
    }

    USB_DIEPINT(ep) = 0xffffffffu;
    USB_GRSTCTL = USB_GRSTCTL_TX_FIFO(ep) | USB_GRSTCTL_TX_FLUSH;
    uint32_t timeout = USB_EP_WAIT_LOOPS;
    while ((USB_GRSTCTL & USB_GRSTCTL_TX_FLUSH) && timeout--)
        ;
    if (USB_GRSTCTL & USB_GRSTCTL_TX_FLUSH)
        return 0;

    // Clear command bits while retaining the endpoint configuration.
    ctl = USB_DIEPCTL(ep);
    ctl &= ~(USB_EPCTL_DISABLE | USB_EPCTL_SNAK);
    USB_DIEPCTL(ep) = ctl;
    return 1;
}

static void
fifo_configure(void)
{
    enum {
        fifo_words = 256,
        ep0_words = USB_CDC_EP0_SIZE / 4,
        acm_words = USB_CDC_EP_ACM_SIZE / 4,
        bulk_in_words = (USB_CDC_EP_BULK_IN_MAX_XFER + 3) / 4,
        ep0_start = fifo_words - ep0_words,
        acm_start = ep0_start - acm_words,
        bulk_in_fifo_start = acm_start - bulk_in_words,
        rx_words = 13 + 1
                   + 2 * ((USB_CDC_EP_BULK_OUT_SIZE / 4) + 1)
                   + 2 * 7,
    };
    _Static_assert(rx_words <= bulk_in_fifo_start,
                   "ESP32-S3 USB FIFO regions overlap");

    // Split the 256-word FIFO RAM between RX, EP0, and bulk IN.
    USB_GRXFSIZ = rx_words;
    USB_GNPTXFSIZ = ep0_start | (ep0_words << 16);
    USB_DIEPTXF(USB_CDC_EP_ACM) = acm_start | (acm_words << 16);
    // EP1 holds one complete four-packet transfer.
    USB_DIEPTXF(USB_CDC_EP_BULK_IN) = (
        bulk_in_fifo_start | (bulk_in_words << 16));
}

static void
fifo_write_data(uint32_t ep, const uint8_t *src, uint32_t len)
{
    uint32_t count = len;
    // Bulk buffers are word aligned; EP0 data may be unaligned.
    if (!((uintptr_t)src & 3u)) {
        while (count >= 4) {
            REG32(USB_FIFO(ep)) = *(const uint32_t *)src;
            count -= 4;
            src += 4;
        }
    }
    while (count >= 4) {
        uint32_t data;
        memcpy(&data, src, sizeof(data));
        REG32(USB_FIFO(ep)) = data;
        count -= 4;
        src += 4;
    }
    if (count) {
        uint32_t data = 0;
        memcpy(&data, src, count);
        REG32(USB_FIFO(ep)) = data;
    }
}

static int_fast8_t
fifo_write_packet(uint32_t ep, const uint8_t *src, uint32_t len)
{
    if (!ep)
        ep0_tx_len = len;
    USB_DIEPINT(ep) = USB_DIEPINT_XFER_COMPLETE;
    USB_DIEPTSIZ(ep) = len | USB_EPTSIZ_PACKET_COUNT(1);
    USB_DAINTMSK |= BIT32(ep);
    USB_DIEPCTL(ep) |= USB_EPCTL_ENABLE | USB_EPCTL_CNAK;

    // The packet already fits in the endpoint FIFO.
    fifo_write_data(ep, src, len);
    return len;
}

// Submit up to four bulk packets in one transfer.
static int_fast8_t
bulk_in_start(const uint8_t *src, uint_fast8_t len)
{
    uint32_t words = (len + 3u) / 4u;
    uint32_t fifo_free = USB_DTXFSTS(USB_CDC_EP_BULK_IN) & 0xffffu;
    if (fifo_free < words)
        return -1;
    uint32_t packets = (len + USB_CDC_EP_BULK_IN_SIZE - 1u)
                       / USB_CDC_EP_BULK_IN_SIZE;
    if (!packets)
        packets = 1;
    USB_DIEPINT(USB_CDC_EP_BULK_IN) = 0xffffffffu;
    USB_DIEPTSIZ(USB_CDC_EP_BULK_IN) = (
        len | USB_EPTSIZ_PACKET_COUNT(packets));
    USB_DAINTMSK |= BIT32(USB_CDC_EP_BULK_IN);
    USB_DIEPCTL(USB_CDC_EP_BULK_IN) |= (
        USB_EPCTL_ENABLE | USB_EPCTL_CNAK);
    fifo_write_data(USB_CDC_EP_BULK_IN, src, len);
    bulk_in_hw_pending = 1;
    return len;
}

// Submit the oldest queued transfer when the endpoint is idle.
static void
bulk_in_kick(void)
{
    uint32_t ctl = USB_DIEPCTL(USB_CDC_EP_BULK_IN);
    if (!(ctl & USB_EPCTL_ACTIVE) || bulk_in_hw_pending
        || bulk_in_recover_pending
        || bulk_in_tail == bulk_in_head)
        return;

    uint32_t slot = bulk_in_tail & BULK_IN_SLOT_MASK;
    uint_fast8_t len = bulk_in_len[slot];
    if (bulk_in_start(bulk_in_buf[slot], len) >= 0)
        bulk_in_tail++;
}

// Queue one frame.  The caller holds the interrupt lock.
static int
bulk_in_enqueue(const uint8_t *src, uint_fast8_t len)
{
    uint32_t used = (uint8_t)(bulk_in_head - bulk_in_tail);
    if (used >= BULK_IN_SLOTS)
        return -1;
    uint32_t slot = bulk_in_head & BULK_IN_SLOT_MASK;
    memcpy(bulk_in_buf[slot], src, len);
    bulk_in_len[slot] = len;
    bulk_in_head++;
    return len;
}

static uint_fast8_t
fifo_read_payload(uint32_t grx, uint8_t *dest, uint_fast8_t max_len)
{
    uint32_t byte_count = ((grx & USB_GRX_BCNT_MASK)
                           >> USB_GRX_BCNT_SHIFT);
    uint32_t transfer = byte_count > max_len ? max_len : byte_count;
    // Bulk OUT buffers are word aligned and packet sized.
    if (dest && transfer == byte_count && !((uintptr_t)dest & 3u)) {
        uint32_t count = byte_count;
        while (count >= 4) {
            *(uint32_t *)dest = REG32(USB_FIFO(0));
            dest += 4;
            count -= 4;
        }
        if (count) {
            uint32_t data = REG32(USB_FIFO(0));
            memcpy(dest, &data, count);
        }
        return transfer;
    }
    uint32_t pos = 0;
    while (pos < byte_count) {
        uint32_t data = REG32(USB_FIFO(0));
        uint32_t count = byte_count - pos;
        if (count > sizeof(data))
            count = sizeof(data);
        if (dest && pos < transfer) {
            uint32_t copy = transfer - pos;
            if (copy > count)
                copy = count;
            memcpy(dest + pos, &data, copy);
        }
        pos += count;
    }
    return transfer;
}

// Reserve all free software slots in one hardware OUT transfer.
static void
bulk_out_arm(void)
{
    uint32_t ep = USB_CDC_EP_BULK_OUT;
    uint32_t ctl = USB_DOEPCTL(ep);
    if (ctl & USB_EPCTL_ENABLE)
        return;
    uint32_t used = (uint8_t)(bulk_out_head - bulk_out_tail);
    if (used >= BULK_OUT_SLOTS)
        return;
    uint32_t packets = BULK_OUT_SLOTS - used;
    USB_DOEPTSIZ(ep) = (packets * USB_CDC_EP_BULK_OUT_SIZE
                         | USB_EPTSIZ_PACKET_COUNT(packets));
    USB_DOEPCTL(ep) = ctl | USB_EPCTL_ENABLE | USB_EPCTL_CNAK;
}

static void
enable_ep0_out(uint32_t len)
{
    uint32_t ctl = USB_DOEPCTL(0);
    if (ctl & USB_EPCTL_ENABLE)
        return;
    // Keep SETUP reception armed during control transfers.
    USB_DOEPTSIZ(0) = (USB_DOEPTSIZ_SETUP_COUNT
                        | USB_EPTSIZ_PACKET_COUNT(1) | len);
    USB_DOEPCTL(0) = ctl | USB_EPCTL_ENABLE | USB_EPCTL_CNAK;
}

// Drain the receive FIFO in the interrupt handler.
static void
handle_rx_fifo(void)
{
    USB_GINTMSK &= ~USB_GINT_RXFLVL;
    uint32_t budget = USB_RX_DRAIN_LIMIT;
    while ((USB_GINTSTS & USB_GINT_RXFLVL) && budget--) {
        uint32_t grx = USB_GRXSTSP;
        uint32_t grx_ep = grx & USB_GRX_EPNUM_MASK;
        uint32_t pktsts = ((grx & USB_GRX_PKTSTS_MASK)
                           >> USB_GRX_PKTSTS_SHIFT);

        if (pktsts == 6 && grx_ep == 0) { // SETUP packet received
            fifo_read_payload(grx, ep0_setup_buf, sizeof(ep0_setup_buf));
        } else if (pktsts == 4 && grx_ep == 0) { // SETUP transaction done
            USB_DOEPTSIZ(0) |= USB_DOEPTSIZ_SETUP_COUNT;
            USB_DOEPINT(0) = (USB_DOEPINT_SETUP
                              | USB_DOEPINT_SETUP_RECEIVED);
            ep0_setup_ready = 1;
            usb_notify_ep0();
        } else if (pktsts == 2) { // OUT data packet received
            uint32_t byte_count = ((grx & USB_GRX_BCNT_MASK)
                                   >> USB_GRX_BCNT_SHIFT);
            if (grx_ep == 0) {
                ep0_out_len = fifo_read_payload(
                    grx, ep0_out_buf, sizeof(ep0_out_buf));
                // A zero-length EP0 OUT completes a control read.
                if (byte_count) {
                    ep0_out_ready = 1;
                    usb_notify_ep0();
                }
            } else if (grx_ep == USB_CDC_EP_BULK_OUT) {
                uint32_t used = (uint8_t)(bulk_out_head - bulk_out_tail);
                if (used < BULK_OUT_SLOTS) {
                    uint32_t slot = bulk_out_head & BULK_OUT_SLOT_MASK;
                    bulk_out_len[slot] = fifo_read_payload(
                        grx, bulk_out_buf[slot],
                        USB_CDC_EP_BULK_OUT_SIZE);
                    bulk_out_head++;
                    usb_notify_bulk_out();
                } else {
                    // Hardware is armed only for available slots.
                    fifo_read_payload(grx, NULL, 0);
                }
            } else {
                fifo_read_payload(grx, NULL, 0);
            }
        } else if (pktsts == 3) { // OUT transaction complete
            USB_DOEPINT(grx_ep) = USB_DOEPINT_XFER_COMPLETE;
            if (grx_ep == USB_CDC_EP_BULK_OUT)
                bulk_out_arm();
        } else {
            // Drain any payload reported for an unused status.
            fifo_read_payload(grx, NULL, 0);
        }
    }
    if (USB_GINTSTS & USB_GINT_RXFLVL) {
        // Defer FIFO recovery to task context; keep the USB ISR bounded.
        usb_rx_recover_pending = 1;
        usb_schedule_recovery();
        sched_wake_tasks();
    } else {
        USB_GINTMSK |= USB_GINT_RXFLVL;
    }
}


/****************************************************************
 * Interface used by generic/usb_cdc.c
 ****************************************************************/

int_fast8_t
usb_read_bulk_out(void *data, uint_fast8_t max_len)
{
    irqstatus_t flag = irq_save();
    if (bulk_out_tail == bulk_out_head) {
        irq_restore(flag);
        return -1;
    }
    uint32_t slot = bulk_out_tail & BULK_OUT_SLOT_MASK;
    uint_fast8_t len = bulk_out_len[slot];
    uint_fast8_t ret = len > max_len ? max_len : len;
    memcpy(data, bulk_out_buf[slot], ret);
    bulk_out_tail++;
    bulk_out_arm();
    irq_restore(flag);
    return ret;
}

int_fast8_t
usb_send_bulk_in(void *data, uint_fast8_t len)
{
    irqstatus_t flag = irq_save();
    if (len > USB_CDC_EP_BULK_IN_MAX_XFER)
        len = USB_CDC_EP_BULK_IN_MAX_XFER;
    if (!len) {
        irq_restore(flag);
        return 0;
    }
    uint32_t ctl = USB_DIEPCTL(USB_CDC_EP_BULK_IN);
    // Submit directly only when the endpoint and software queue are idle.
    // Otherwise copy the frame into the software queue.
    int_fast8_t ret = -1;
    if ((ctl & USB_EPCTL_ACTIVE) && !bulk_in_hw_pending
        && !bulk_in_recover_pending && bulk_in_tail == bulk_in_head)
        ret = bulk_in_start(data, len);
    if (ret < 0) {
        ret = bulk_in_enqueue(data, len);
        if (ret < 0) {
            // Return backpressure when all software slots are occupied.
            USB_DAINTMSK |= BIT32(USB_CDC_EP_BULK_IN);
            bulk_in_kick();
            irq_restore(flag);
            return -1;
        }
        // The completion ISR submits the next queued frame.
        USB_DAINTMSK |= BIT32(USB_CDC_EP_BULK_IN);
        bulk_in_kick();
        ret = len;
    }
    irq_restore(flag);
    return ret;
}

int_fast8_t
usb_read_ep0(void *data, uint_fast8_t max_len)
{
    irqstatus_t flag = irq_save();
    if (!ep0_out_ready) {
        enable_ep0_out(max_len);
        irq_restore(flag);
        return -1;
    }
    uint_fast8_t ret = ep0_out_len > max_len ? max_len : ep0_out_len;
    memcpy(data, ep0_out_buf, ret);
    ep0_out_ready = 0;
    enable_ep0_out(max_len);
    irq_restore(flag);
#if CONFIG_MACH_ESP32S3 && CONFIG_USBCANBUS
    // Apply S3-specific gs_usb data after releasing the USB critical section.
    esp32_gs_apply_out(data, ret);
#endif
    return ret;
}

int_fast8_t
usb_read_ep0_setup(void *data, uint_fast8_t max_len)
{
    irqstatus_t flag = irq_save();
    if (!ep0_setup_ready) {
        irq_restore(flag);
        return -1;
    }
    ep0_setup_ready = 0;
#if CONFIG_MACH_ESP32S3 && CONFIG_USBCANBUS
    esp32_gs_track_setup(ep0_setup_buf);
#endif
    uint32_t ctl = USB_DIEPCTL(0);
    if (ctl & USB_EPCTL_ENABLE) {
        USB_DIEPCTL(0) = ctl | USB_EPCTL_DISABLE | USB_EPCTL_SNAK;
        uint32_t timeout = USB_EP_WAIT_LOOPS;
        while ((USB_DIEPCTL(0) & USB_EPCTL_ENABLE) && timeout--)
            ;
        usb_flush(USB_GRSTCTL_TX_FLUSH);
    }
    irq_restore(flag);
    memcpy(data, ep0_setup_buf, max_len);
    return max_len;
}

int_fast8_t
usb_send_ep0(const void *data, uint_fast8_t len)
{
    irqstatus_t flag = irq_save();
    if (ep0_setup_ready) {
        irq_restore(flag);
        return -2;
    }
    if (USB_DIEPCTL(0) & USB_EPCTL_ENABLE) {
        USB_DAINTMSK |= BIT32(0);
        irq_restore(flag);
        return -1;
    }
#if CONFIG_MACH_ESP32S3 && CONFIG_USBCANBUS
    data = esp32_gs_ep0_data(data, len);
#endif
    int_fast8_t ret = fifo_write_packet(0, data, len);
    irq_restore(flag);
    return ret;
}

void
usb_stall_ep0(void)
{
    irqstatus_t flag = irq_save();
    USB_DIEPCTL(0) |= USB_EPCTL_STALL;
    usb_notify_ep0();
    irq_restore(flag);
}

void
usb_set_address(uint_fast8_t addr)
{
    irqstatus_t flag = irq_save();
    USB_DCFG = ((USB_DCFG & ~USB_DCFG_ADDRESS_MASK)
                | (addr << USB_DCFG_ADDRESS_SHIFT));
    irq_restore(flag);
    usb_send_ep0(NULL, 0);
    usb_notify_ep0();
}

void
usb_set_configure(void)
{
    irqstatus_t flag = irq_save();

    USB_DIEPTSIZ(USB_CDC_EP_ACM) = (USB_CDC_EP_ACM_SIZE
                                    | USB_EPTSIZ_PACKET_COUNT(1));
    USB_DIEPCTL(USB_CDC_EP_ACM) = (
        USB_EPCTL_SNAK | USB_EPCTL_ACTIVE | USB_EPCTL_TYPE(3)
        | USB_EPCTL_SET_DATA0 | USB_DIEPCTL_TX_FIFO(USB_CDC_EP_ACM)
        | USB_CDC_EP_ACM_SIZE);

    bulk_out_head = bulk_out_tail = 0;
    USB_DOEPCTL(USB_CDC_EP_BULK_OUT) = (
        USB_EPCTL_SNAK | USB_EPCTL_ACTIVE
        | USB_EPCTL_TYPE(2) | USB_EPCTL_SET_DATA0
        | USB_CDC_EP_BULK_OUT_SIZE);
    bulk_out_arm();

    USB_DIEPTSIZ(USB_CDC_EP_BULK_IN) = (USB_CDC_EP_BULK_IN_SIZE
                                        | USB_EPTSIZ_PACKET_COUNT(1));
    USB_DIEPCTL(USB_CDC_EP_BULK_IN) = (
        USB_EPCTL_SNAK | USB_EPCTL_DISABLE | USB_EPCTL_ACTIVE
        | USB_EPCTL_TYPE(2) | USB_EPCTL_SET_DATA0
        | USB_DIEPCTL_TX_FIFO(USB_CDC_EP_BULK_IN)
        | USB_CDC_EP_BULK_IN_SIZE);
    uint32_t timeout = USB_EP_WAIT_LOOPS;
    while ((USB_DIEPCTL(USB_CDC_EP_BULK_IN) & USB_EPCTL_ENABLE)
           && timeout--)
        ;
    usb_flush(USB_GRSTCTL_TX_FIFO(USB_CDC_EP_BULK_IN)
              | USB_GRSTCTL_TX_FLUSH);
    bulk_in_head = bulk_in_tail = 0;
    bulk_in_hw_pending = 0;
    bulk_in_recover_pending = 0;
    usb_rx_recover_pending = 0;
    irq_restore(flag);
}


/****************************************************************
 * Controller setup and interrupt handling
 ****************************************************************/

static void
configure_ep0(void)
{
    USB_DCFG &= ~USB_DCFG_ADDRESS_MASK;
    USB_DIEPEMPMSK = 0;
    ep0_tx_len = 0;
    ep0_setup_ready = ep0_out_ready = ep0_out_len = 0;
#if CONFIG_MACH_ESP32S3 && CONFIG_USBCANBUS
    esp32_gs_setup_valid = 0;
    esp32_gs_out_applied = 0;
#endif
    bulk_out_head = bulk_out_tail = 0;
    bulk_in_head = bulk_in_tail = 0;
    bulk_in_hw_pending = 0;
    bulk_in_recover_pending = 0;
    usb_rx_recover_pending = 0;
    USB_DIEPINT(0) = 0xffffffffu;
    USB_DOEPINT(0) = 0xffffffffu;
    // Zero encodes a 64-byte EP0 on ESP32-S3.
    USB_DIEPCTL(0) = USB_EPCTL_SNAK;
    USB_DOEPTSIZ(0) = USB_DOEPTSIZ_SETUP_COUNT;
    USB_DOEPCTL(0) = USB_EPCTL_SNAK;
    USB_DAINTMSK = BIT32(0) | BIT32(16);
    USB_DIEPMSK = (USB_DIEPMSK_XFER_COMPLETE
                    | USB_DIEPMSK_EPDISBLD
                    | USB_DIEPMSK_AHBERR
                    | USB_DIEPMSK_TIMEOUT);
    USB_DOEPMSK = USB_DOEPMSK_XFER_COMPLETE | USB_DOEPMSK_SETUP;
}

void
esp32_usb_irq(void)
{
    uint32_t status = USB_GINTSTS & USB_GINTMSK;
    if (status & USB_GINT_USBRST) {
        USB_GINTSTS = USB_GINT_USBRST;
        usb_flush(USB_GRSTCTL_ALL_TX_FIFOS | USB_GRSTCTL_TX_FLUSH);
        usb_flush(USB_GRSTCTL_RX_FLUSH);
        fifo_configure();
        configure_ep0();
        status &= ~(USB_GINT_RXFLVL | USB_GINT_IEPINT);
    }
    if (status & USB_GINT_ENUMDONE) {
        USB_GINTSTS = USB_GINT_ENUMDONE;
    }
    if (status & USB_GINT_RXFLVL) {
        handle_rx_fifo();
    }
    if (status & USB_GINT_IEPINT) {
        uint32_t pending = USB_DAINT & USB_DAINTMSK;
        if (pending & BIT32(0)) {
            uint32_t in_status = USB_DIEPINT(0);
            if (in_status & USB_DIEPINT_XFER_COMPLETE) {
                USB_DAINTMSK &= ~BIT32(0);
                USB_DIEPINT(0) = in_status;
                if (ep0_tx_len) {
                    // Start the status stage after the IN data completes.
                    ep0_tx_len = 0;
                    enable_ep0_out(0);
                }
                usb_notify_ep0();
            } else if (in_status & USB_DIEPINT_TIMEOUT) {
                // DIEPINT is W1C; clear all latched causes.
                USB_DIEPINT(0) = in_status;
            } else if (in_status) {
                USB_DIEPINT(0) = in_status;
            }
        }
        if (pending & BIT32(USB_CDC_EP_BULK_IN)) {
            uint32_t in_status = USB_DIEPINT(USB_CDC_EP_BULK_IN);
            USB_DIEPINT(USB_CDC_EP_BULK_IN) = in_status;
            uint32_t error_status = (USB_DIEPINT_EP_DISABLED
                                     | USB_DIEPINT_AHB_ERROR
                                     | USB_DIEPINT_TIMEOUT
                                     | USB_DIEPINT_TX_FIFO_UNDERRUN);
            if (in_status & error_status) {
                // Drop the failed transfer and recover the endpoint in task
                // context before submitting the next queued frame.
                bulk_in_hw_pending = 0;
                bulk_in_recover_pending = 1;
                USB_DAINTMSK &= ~BIT32(USB_CDC_EP_BULK_IN);
                // Keep endpoint/FIFO waits out of the USB ISR.
                usb_schedule_recovery();
                usb_notify_bulk_in();
            } else if (in_status & USB_DIEPINT_XFER_COMPLETE) {
                // Submit the next software-owned frame, if any.
                bulk_in_hw_pending = 0;
                bulk_in_kick();
                if (bulk_in_tail == bulk_in_head)
                    USB_DAINTMSK &= ~BIT32(USB_CDC_EP_BULK_IN);
                usb_notify_bulk_in();
            } else if (in_status) {
                usb_notify_bulk_in();
            }
        }
    }
}

void
usb_init(void)
{
    // Enable and reset the USB wrapper.
    REG32(SYSTEM_PERIP_CLK_EN0) |= SYSTEM_USB_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) |= SYSTEM_USB_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~SYSTEM_USB_BIT;

    // Hold both data lines low while changing controller ownership.
    uint32_t wrap = REG32(USB_WRAP_OTG_CONF);
    wrap &= ~(USB_WRAP_PHY_SELECT_EXT | USB_WRAP_EXCHG_PINS
              | USB_WRAP_EXCHG_OVERRIDE
              | USB_WRAP_DP_PULLUP | USB_WRAP_DP_PULLDOWN
              | USB_WRAP_DM_PULLUP | USB_WRAP_DM_PULLDOWN);
    REG32(USB_WRAP_OTG_CONF) = (
        wrap | USB_WRAP_PAD_ENABLE | USB_WRAP_AHB_CLK_FORCE_ON
        | USB_WRAP_PHY_CLK_FORCE_ON | USB_WRAP_PAD_PULL_OVERRIDE
        | USB_WRAP_DP_PULLDOWN | USB_WRAP_DM_PULLDOWN);

    // Select the internal full-speed PHY.
    REG32(RTC_USB_CONF) |= RTC_SW_HW_USB_PHY_SELECT | RTC_SW_USB_PHY_SELECT;

    // Route the fixed device-mode session signals.
    REG32(GPIO_FUNC_IN(GPIO_MATRIX_USB_IDDIG)) = (
        GPIO_MATRIX_IN_ENABLE | GPIO_MATRIX_CONST_ONE);
    REG32(GPIO_FUNC_IN(GPIO_MATRIX_USB_BVALID)) = (
        GPIO_MATRIX_IN_ENABLE | GPIO_MATRIX_CONST_ONE);
    REG32(GPIO_FUNC_IN(GPIO_MATRIX_USB_VBUSVALID)) = (
        GPIO_MATRIX_IN_ENABLE | GPIO_MATRIX_CONST_ONE);
    REG32(GPIO_FUNC_IN(GPIO_MATRIX_USB_AVALID)) = (
        GPIO_MATRIX_IN_ENABLE | GPIO_MATRIX_CONST_ZERO);
    USB_DCTL |= USB_DCTL_SOFT_DISCONNECT;

    // Use the strongest drive setting on the USB pads.
    REG32(IOMUX_GPIO(19)) = ((REG32(IOMUX_GPIO(19)) & ~(3u << 10))
                            | (3u << 10));
    REG32(IOMUX_GPIO(20)) = ((REG32(IOMUX_GPIO(20)) & ~(3u << 10))
                            | (3u << 10));

    USB_GAHBCFG = 0;
    USB_GUSBCFG |= USB_GUSBCFG_PHYSEL;
    uint32_t core_timeout = USB_CORE_WAIT_LOOPS;
    while (!(USB_GRSTCTL & USB_GRSTCTL_AHB_IDLE) && core_timeout--)
        ;
    USB_GRSTCTL |= USB_GRSTCTL_CORE_RESET;
    core_timeout = USB_CORE_WAIT_LOOPS;
    while ((USB_GRSTCTL & USB_GRSTCTL_CORE_RESET) && core_timeout--)
        ;
    core_timeout = USB_CORE_WAIT_LOOPS;
    while (!(USB_GRSTCTL & USB_GRSTCTL_AHB_IDLE) && core_timeout--)
        ;
    USB_DCTL |= USB_DCTL_SOFT_DISCONNECT;
    USB_PCGCCTL = 0;
    uint32_t gusbcfg = USB_GUSBCFG;
    gusbcfg &= ~(USB_GUSBCFG_TOCAL_MASK | USB_GUSBCFG_TRDT_MASK
                 | USB_GUSBCFG_FORCE_HOST);
    USB_GUSBCFG = (gusbcfg | USB_GUSBCFG_TOCAL(7)
                   | USB_GUSBCFG_PHYSEL | USB_GUSBCFG_TRDT(5)
                   | USB_GUSBCFG_FORCE_DEVICE);
    // Wait for the core to enter device mode.
    uint32_t mode_timeout = timer_read_time() + timer_from_us(30000);
    while ((USB_GINTSTS & USB_GINT_CURRENT_MODE_HOST)
           && timer_is_before(timer_read_time(), mode_timeout))
        ;
    // The GPIO matrix supplies the device-mode session signals.
    USB_GOTGCTL &= ~(USB_GOTGCTL_VBVALID_VALUE
                     | USB_GOTGCTL_BVALID_ENABLE
                     | USB_GOTGCTL_BVALID_VALUE);
    // Select full speed to match the internal PHY.
    USB_DCFG = ((USB_DCFG & ~USB_DCFG_SPEED_MASK)
                | USB_DCFG_SPEED_FULL | USB_DCFG_NZ_STATUS_STALL);
    usb_flush(USB_GRSTCTL_ALL_TX_FIFOS | USB_GRSTCTL_TX_FLUSH);
    usb_flush(USB_GRSTCTL_RX_FLUSH);
    USB_GINTSTS = 0xffffffffu;
    // Set the FIFO depth and endpoint-info base to 256 words.
    USB_GDFIFOCFG = (256u << 16) | 256u;
    fifo_configure();
    configure_ep0();

    USB_GINTMSK = (USB_GINT_RXFLVL | USB_GINT_IEPINT
                   | USB_GINT_USBRST | USB_GINT_ENUMDONE);
    esp32_irq_setup(INT_SOURCE_USB, CPU_INT_USB, 1);
    USB_GAHBCFG = USB_GAHBCFG_GLOBAL_INT;

    // Complete the detach delay, then connect the OTG device.
    uint32_t endtime = timer_read_time() + timer_from_us(10000);
    while (timer_is_before(timer_read_time(), endtime))
        ;
    REG32(USB_WRAP_OTG_CONF) &= ~(USB_WRAP_PAD_PULL_OVERRIDE
                                  | USB_WRAP_DP_PULLUP
                                  | USB_WRAP_DP_PULLDOWN
                                  | USB_WRAP_DM_PULLUP
                                  | USB_WRAP_DM_PULLDOWN);
    USB_DCTL &= ~USB_DCTL_SOFT_DISCONNECT;
}
DECL_INIT(usb_init);
