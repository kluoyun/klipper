// ESP32-C3 watchdog register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32C3_WATCHDOG_REGS_H
#define __ESP32C3_WATCHDOG_REGS_H

#define RTC_WDT_CONFIG0            (ESP_RTC_BASE + 0x090u)
#define RTC_WDT_FEED               (ESP_RTC_BASE + 0x0a4u)
#define RTC_WDT_WPROTECT           (ESP_RTC_BASE + 0x0a8u)
#define RTC_SWD_CONF               (ESP_RTC_BASE + 0x0acu)
#define RTC_SWD_WPROTECT           (ESP_RTC_BASE + 0x0b0u)

#define TIMERG_WDT_CONFIG0(base)   ((base) + 0x048u)
#define TIMERG_WDT_CONFIG1(base)   ((base) + 0x04cu)
#define TIMERG_WDT_CONFIG2(base)   ((base) + 0x050u)
#define TIMERG_WDT_FEED(base)      ((base) + 0x060u)
#define TIMERG_WDT_WPROTECT(base)  ((base) + 0x064u)

#define WDT_ENABLE                 BIT32(31)
#define RTC_WDT_FLASHBOOT_ENABLE   BIT32(12)
#define RTC_WDT_FEED_VALUE         BIT32(31)
#define TIMERG_WDT_FLASHBOOT_ENABLE BIT32(14)
#define TIMERG_WDT_CONF_UPDATE     BIT32(22)
#define TIMERG_WDT_STAGE0_RESET_SYSTEM (3u << 29)
#define TIMERG_WDT_PRESCALER(value) ((value) << 16)
#define WDT_WPROTECT_KEY           0x50d83aa1u
#define RTC_SWD_WPROTECT_KEY       0x8f1d312au
#define RTC_SWD_AUTO_FEED          BIT32(31)
#define RTC_SWD_DISABLE            BIT32(30)

#endif // esp32c3_watchdog_regs.h
