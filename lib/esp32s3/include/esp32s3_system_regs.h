// ESP32-S3 system register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_SYSTEM_REGS_H
#define __ESP32S3_SYSTEM_REGS_H

#define SYSTEM_CPU_PER_CONF        (ESP_SYSTEM_BASE + 0x010u)
#define SYSTEM_PERIP_CLK_EN0       (ESP_SYSTEM_BASE + 0x018u)
#define SYSTEM_PERIP_CLK_EN1       (ESP_SYSTEM_BASE + 0x01cu)
#define SYSTEM_PERIP_RST_EN0       (ESP_SYSTEM_BASE + 0x020u)
#define SYSTEM_SYSCLK_CONF         (ESP_SYSTEM_BASE + 0x060u)
#define SYSTEM_UART0_BIT           BIT32(2)
#define SYSTEM_UART1_BIT           BIT32(5)
#define SYSTEM_UART2_BIT           BIT32(9)
#define SYSTEM_SPI2_BIT            BIT32(6)
#define SYSTEM_I2C0_BIT            BIT32(7)
#define SYSTEM_USB_DEVICE_BIT      BIT32(10)
#define SYSTEM_LEDC_BIT            BIT32(11)
#define SYSTEM_TIMERGROUP0_BIT     BIT32(13)
#define SYSTEM_SPI3_BIT            BIT32(16)
#define SYSTEM_I2C1_BIT            BIT32(18)
#define SYSTEM_TWAI_BIT            BIT32(19)
#define SYSTEM_USB_BIT             BIT32(23)
#define SYSTEM_SYSTIMER_BIT        BIT32(29)
#define SYSTEM_CPU_WAIT_MODE_FORCE_ON BIT32(3)
#define SYSTEM_PLL_FREQ_480M       BIT32(2)
#define SYSTEM_CPU_PERIOD_MASK     0x3u
#define SYSTEM_CPU_PERIOD_240M     0x2u
#define SYSTEM_PRE_DIV_MASK        0x3ffu
#define SYSTEM_SOC_CLK_SEL_MASK    (0x3u << 10)
#define SYSTEM_SOC_CLK_XTAL        (0x0u << 10)
#define SYSTEM_SOC_CLK_PLL         (0x1u << 10)
#define SYSTEM_WIFI_CLK_EN         (ESP_SYSCON_BASE + 0x014u)
#define SYSTEM_WIFI_CLK_I2C        BIT32(5)

#define RTC_OPTIONS0               (ESP_RTC_BASE + 0x000u)
#define RTC_SW_SYS_RESET           BIT32(31)
#define RTC_BB_I2C_FORCE_PD        BIT32(6)
#define RTC_BBPLL_I2C_FORCE_PD     BIT32(8)
#define RTC_BBPLL_FORCE_PD         BIT32(10)
#define RTC_DATE                   (ESP_RTC_BASE + 0x1fcu)
#define RTC_SLAVE_PD_MASK          (0x3fu << 13)
#define RTC_SLAVE_PD_240M          (0u << 13)

#define I2C_MST_ANA_CONF0          0x6000e040u
#define I2C_MST_COMMAND            0x6000e000u
#define I2C_MST_CONFIG             0x6000e048u
#define I2C_MST_COMMAND_BUSY       BIT32(25)
#define I2C_MST_COMMAND_WRITE      BIT32(24)
#define I2C_MST_COMMAND_DATA_SHIFT 16
#define I2C_MST_COMMAND_ADDR_SHIFT 8
#define I2C_MST_MAGIC_MASK         (0x1fffu << 4)
#define I2C_MST_MAGIC_VALUE        (0x1c40u << 4)
#define I2C_MST_BBPLL_STOP_HIGH    BIT32(2)
#define I2C_MST_BBPLL_STOP_LOW     BIT32(3)
#define I2C_MST_BBPLL_CAL_DONE     BIT32(24)
#define ANA_CONFIG                 0x6000e044u
#define ANA_CONFIG_RESET_MASK      (0x3ffu << 8)
#define ANA_I2C_DIG_DISABLE        BIT32(14)
#define ANA_I2C_BBPLL_DISABLE      BIT32(17)

#define EFUSE_MAC_LOW              (ESP_EFUSE_BASE + 0x044u)
#define EFUSE_MAC_HIGH             (ESP_EFUSE_BASE + 0x048u)

#endif // esp32s3_system_regs.h
