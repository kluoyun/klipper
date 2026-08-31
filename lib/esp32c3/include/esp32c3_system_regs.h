// ESP32-C3 system register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32C3_SYSTEM_REGS_H
#define __ESP32C3_SYSTEM_REGS_H

#define SYSTEM_PERIP_CLK_EN0       (ESP_SYSTEM_BASE + 0x010u)
#define SYSTEM_PERIP_CLK_EN1       (ESP_SYSTEM_BASE + 0x014u)
#define SYSTEM_PERIP_RST_EN0       (ESP_SYSTEM_BASE + 0x018u)
#define SYSTEM_PERIP_RST_EN1       (ESP_SYSTEM_BASE + 0x01cu)
#define SYSTEM_UART0_BIT           BIT32(2)
#define SYSTEM_UART1_BIT           BIT32(5)
#define SYSTEM_SPI2_BIT            BIT32(6)
#define SYSTEM_I2C0_BIT            BIT32(7)
#define SYSTEM_TSENS_BIT           BIT32(10)
#define SYSTEM_LEDC_BIT            BIT32(11)
#define SYSTEM_TIMERGROUP0_BIT     BIT32(13)
#define SYSTEM_USB_DEVICE_BIT      BIT32(23)
#define SYSTEM_APB_SARADC_BIT      BIT32(28)
#define SYSTEM_SYSTIMER_BIT        BIT32(29)

#define RTC_OPTIONS0               (ESP_RTC_BASE + 0x000u)
#define RTC_SW_SYS_RESET           BIT32(31)
#define RTC_RESET_STATE            (ESP_RTC_BASE + 0x038u)

#define EFUSE_MAC_LOW              (ESP_EFUSE_BASE + 0x044u)
#define EFUSE_MAC_HIGH             (ESP_EFUSE_BASE + 0x048u)

#endif // esp32c3_system_regs.h
