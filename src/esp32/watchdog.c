// ESP32 watchdog support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "esp32_regs.h"
#include "internal.h"
#include "sched.h"

#if CONFIG_MACH_ESP32S3
// Keep S3 brownout reset disabled.  Clear both the analog reset selection and
// the RTC interrupt latch; register offsets are kept local to this port.
#define RTC_BROWN_OUT_REG              (ESP_RTC_BASE + 0x0e8u)
#define RTC_INT_ENA_REG                (ESP_RTC_BASE + 0x040u)
#define RTC_INT_CLR_REG                (ESP_RTC_BASE + 0x04cu)
#define RTC_FIB_SEL_REG                (ESP_RTC_BASE + 0x148u)
#define RTC_BROWN_OUT_INT_ENA          BIT32(9)
#define RTC_BROWN_OUT_INT_CLR          BIT32(9)
#define RTC_FIB_BOD_RST                BIT32(1)
#define RTC_BROWN_OUT_ENA              BIT32(30)
#define RTC_BROWN_OUT_CNT_CLR          BIT32(29)
#define RTC_BROWN_OUT_RST_ENA          BIT32(26)
#define RTC_BROWN_OUT_ANA_RST_EN       BIT32(28)
#define RTC_BROWN_OUT_RST_SEL          BIT32(27)
#define RTC_BROWN_OUT_PD_RF_ENA        BIT32(15)
#define RTC_BROWN_OUT_CLOSE_FLASH_ENA  BIT32(14)

static void
disable_brownout_reset(void)
{
    // Clear the analog reset selection before changing the detector state.
    REG32(RTC_FIB_SEL_REG) &= ~RTC_FIB_BOD_RST;

    uint32_t brown = REG32(RTC_BROWN_OUT_REG);
    brown &= ~(RTC_BROWN_OUT_ENA | RTC_BROWN_OUT_RST_ENA
               | RTC_BROWN_OUT_ANA_RST_EN | RTC_BROWN_OUT_RST_SEL
               | RTC_BROWN_OUT_PD_RF_ENA | RTC_BROWN_OUT_CLOSE_FLASH_ENA);
    brown &= ~RTC_BROWN_OUT_CNT_CLR;

    // CNT_CLR is a write pulse; clear any count left by the boot ROM.
    REG32(RTC_BROWN_OUT_REG) = brown | RTC_BROWN_OUT_CNT_CLR;
    REG32(RTC_BROWN_OUT_REG) = brown;

    // Disable and clear the RTC brownout interrupt latch.
    REG32(RTC_INT_ENA_REG) &= ~RTC_BROWN_OUT_INT_ENA;
    REG32(RTC_INT_CLR_REG) = RTC_BROWN_OUT_INT_CLR;
}
#endif

#define WATCHDOG_PRESCALER 40000u
#define WATCHDOG_TIMEOUT_TICKS 1000u

void
watchdog_early_init(void)
{
#if CONFIG_MACH_ESP32S3
    disable_brownout_reset();
#endif
    REG32(RTC_WDT_WPROTECT) = WDT_WPROTECT_KEY;
    REG32(RTC_WDT_FEED) = RTC_WDT_FEED_VALUE;
    REG32(RTC_WDT_CONFIG0) &= ~(WDT_ENABLE | RTC_WDT_FLASHBOOT_ENABLE);
    REG32(RTC_WDT_WPROTECT) = 0;

    const uint32_t timer_groups[] = { ESP_TIMERG0_BASE, ESP_TIMERG1_BASE };
    for (uint_fast8_t i = 0; i < ARRAY_SIZE(timer_groups); i++) {
        uint32_t base = timer_groups[i];
        REG32(TIMERG_WDT_WPROTECT(base)) = WDT_WPROTECT_KEY;
        REG32(TIMERG_WDT_FEED(base)) = 1;
        REG32(TIMERG_WDT_CONFIG0(base)) = (
            (REG32(TIMERG_WDT_CONFIG0(base))
             & ~(WDT_ENABLE | TIMERG_WDT_FLASHBOOT_ENABLE))
            | TIMERG_WDT_CONF_UPDATE);
        REG32(TIMERG_WDT_WPROTECT(base)) = 0;
    }

    REG32(RTC_SWD_WPROTECT) = RTC_SWD_WPROTECT_KEY;
    REG32(RTC_SWD_CONF) |= RTC_SWD_DISABLE | RTC_SWD_AUTO_FEED;
    REG32(RTC_SWD_WPROTECT) = 0;
}

void
watchdog_reset(void)
{
    REG32(TIMERG_WDT_WPROTECT(ESP_TIMERG0_BASE)) = WDT_WPROTECT_KEY;
    REG32(TIMERG_WDT_FEED(ESP_TIMERG0_BASE)) = 1;
    REG32(TIMERG_WDT_WPROTECT(ESP_TIMERG0_BASE)) = 0;
}
DECL_TASK(watchdog_reset);

void
watchdog_init(void)
{
    REG32(SYSTEM_PERIP_CLK_EN0) |= SYSTEM_TIMERGROUP0_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) |= SYSTEM_TIMERGROUP0_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~SYSTEM_TIMERGROUP0_BIT;

    REG32(TIMERG_WDT_WPROTECT(ESP_TIMERG0_BASE)) = WDT_WPROTECT_KEY;
    REG32(TIMERG_WDT_CONFIG0(ESP_TIMERG0_BASE)) =
        TIMERG_WDT_CONF_UPDATE;
    REG32(TIMERG_WDT_CONFIG1(ESP_TIMERG0_BASE)) =
        TIMERG_WDT_PRESCALER(WATCHDOG_PRESCALER);
    REG32(TIMERG_WDT_CONFIG2(ESP_TIMERG0_BASE)) = WATCHDOG_TIMEOUT_TICKS;
    REG32(TIMERG_WDT_FEED(ESP_TIMERG0_BASE)) = 1;
    REG32(TIMERG_WDT_CONFIG0(ESP_TIMERG0_BASE)) = (
        WDT_ENABLE | TIMERG_WDT_STAGE0_RESET_SYSTEM
        | TIMERG_WDT_CONF_UPDATE);
    REG32(TIMERG_WDT_WPROTECT(ESP_TIMERG0_BASE)) = 0;
}
DECL_INIT(watchdog_init);
