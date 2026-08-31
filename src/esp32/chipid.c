// Support for extracting the ESP32 factory identity
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <string.h>
#include "autoconf.h"
#include "esp32_regs.h"
#include "generic/canserial.h"
#include "generic/usb_cdc.h"
#include "generic/usbstd.h"
#include "sched.h"

#define CHIP_UID_LEN 6

static struct {
    struct usb_string_descriptor desc;
    uint16_t data[CHIP_UID_LEN * 2];
} cdc_chipid;

struct usb_string_descriptor *
usbserial_get_serialid(void)
{
    return &cdc_chipid.desc;
}

void
chipid_init(void)
{
    if (!CONFIG_USB_SERIAL_NUMBER_CHIPID && !CONFIG_CANBUS)
        return;
    uint32_t mac_low = REG32(EFUSE_MAC_LOW);
    uint16_t mac_high = REG32(EFUSE_MAC_HIGH);
    uint8_t chipid[CHIP_UID_LEN];
    memcpy(chipid, &mac_low, sizeof(mac_low));
    memcpy(&chipid[sizeof(mac_low)], &mac_high, sizeof(mac_high));
    if (CONFIG_USB_SERIAL_NUMBER_CHIPID)
        usb_fill_serial(&cdc_chipid.desc, ARRAY_SIZE(cdc_chipid.data), chipid);
    if (CONFIG_CANBUS)
        canserial_set_uuid(chipid, CHIP_UID_LEN);
}
DECL_INIT(chipid_init);
