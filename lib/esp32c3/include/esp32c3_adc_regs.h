// ESP32-C3 ADC register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32C3_ADC_REGS_H
#define __ESP32C3_ADC_REGS_H

#define ADC_CTRL                   (ESP_APB_SARADC_BASE + 0x000u)
#define ADC_FSM_WAIT               (ESP_APB_SARADC_BASE + 0x00cu)
#define ADC_ONETIME_SAMPLE         (ESP_APB_SARADC_BASE + 0x020u)
#define ADC_DATA1                  (ESP_APB_SARADC_BASE + 0x02cu)
#define ADC_INT_RAW                (ESP_APB_SARADC_BASE + 0x044u)
#define ADC_INT_CLR                (ESP_APB_SARADC_BASE + 0x04cu)
#define ADC_CLK_CONF               (ESP_APB_SARADC_BASE + 0x054u)
#define ADC_TSENS_CTRL             (ESP_APB_SARADC_BASE + 0x058u)
#define ADC_TSENS_CTRL2            (ESP_APB_SARADC_BASE + 0x05cu)
#define RTC_ANA_CONF               (ESP_RTC_BASE + 0x034u)
#define RTC_SENSOR_CTRL            (ESP_RTC_BASE + 0x11cu)
#define ANA_CONFIG                 0x6000e044u
#define ANA_CONFIG2                0x6000e048u
#define EFUSE_BLOCK2_VERSION       (ESP_EFUSE_BASE + 0x06cu)
#define EFUSE_BLOCK2_INIT_CODE     (ESP_EFUSE_BASE + 0x070u)

#define ADC_XPD_SAR_FORCE_MASK     (3u << 27)
#define ADC_XPD_SAR_FORCE_ON       (3u << 27)
#define ADC_SAR_CLK_DIV_MASK       (0xffu << 7)
#define ADC_SAR_CLK_GATED          BIT32(6)
#define ADC_ONETIME_ADC1_ENABLE    BIT32(31)
#define ADC_ONETIME_START          BIT32(29)
#define ADC_ONETIME_CHANNEL_SHIFT  25
#define ADC_ONETIME_ATTEN_SHIFT    23
#define ADC1_DONE                  BIT32(31)
#define ADC_CLK_SELECT_APB         (2u << 21)
#define ADC_CLK_ENABLE             BIT32(20)
#define ADC_RTC_XPD_SAR_MASK       (3u << 30)
#define ADC_RTC_XPD_SAR_ON         (3u << 30)
#define ADC_TSENS_OUT_MASK         0xffu
#define ADC_TSENS_CLK_DIV_SHIFT    14
#define ADC_TSENS_CLK_DIV_MASK     (0xffu << ADC_TSENS_CLK_DIV_SHIFT)
#define ADC_TSENS_POWER_UP         BIT32(22)
#define ADC_TSENS_CLK_XTAL         BIT32(15)
#define RTC_SAR_I2C_POWER_UP       BIT32(22)
#define ANA_I2C_SAR_DISABLE        BIT32(18)
#define ANA_SAR_CONFIG_ENABLE      BIT32(16)

#define ADC_ATTEN_12DB             3u
#define ADC_ANALOG_I2C_BLOCK       0x69u
#define ADC_TSENS_DAC_REG          6u
#define ADC_TSENS_DAC_VALUE        15u
#define ADC_ROM_REGI2C_WRITE_MASK  0x40001960u
#define ADC_ROM_DELAY_US           0x40000050u

#endif // esp32c3_adc_regs.h
