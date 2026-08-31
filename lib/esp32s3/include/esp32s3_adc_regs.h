// ESP32-S3 ADC register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_ADC_REGS_H
#define __ESP32S3_ADC_REGS_H

#define SENS_SAR_READER1_CTRL      (ESP_SENS_BASE + 0x000u)
#define SENS_SAR_MEAS1_CTRL2       (ESP_SENS_BASE + 0x00cu)
#define SENS_SAR_MEAS1_MUX         (ESP_SENS_BASE + 0x010u)
#define SENS_SAR_ATTEN1            (ESP_SENS_BASE + 0x014u)
#define SENS_SAR_POWER_XPD_SAR     (ESP_SENS_BASE + 0x03cu)
#define SENS_SAR_SLAVE_ADDR1       (ESP_SENS_BASE + 0x040u)
#define SENS_SAR_TSENS_CTRL        (ESP_SENS_BASE + 0x050u)
#define SENS_SAR_TSENS_CTRL2       (ESP_SENS_BASE + 0x054u)
#define SENS_SAR_PERI_CLK_GATE_CONF (ESP_SENS_BASE + 0x104u)
#define SENS_SAR_PERI_RESET_CONF   (ESP_SENS_BASE + 0x108u)
#define RTC_ANA_CONF               (ESP_RTC_BASE + 0x034u)
#define ANA_CONFIG2                0x6000e048u
#define EFUSE_BLOCK2_READ_BASE     (ESP_EFUSE_BASE + 0x05cu)

#define SENS_SAR1_CLK_DIV_MASK     0xffu
#define SENS_SAR1_CLK_GATED        BIT32(18)
#define SENS_SAR1_DATA_INVERT      BIT32(28)
#define SENS_SAR1_EN_PAD_SHIFT     19
#define SENS_SAR1_EN_PAD_MASK      (0xfffu << SENS_SAR1_EN_PAD_SHIFT)
#define SENS_MEAS1_START_FORCE     BIT32(18)
#define SENS_MEAS1_START           BIT32(17)
#define SENS_MEAS1_DONE            BIT32(16)
#define SENS_MEAS1_DATA_MASK       0xffffu
#define SENS_SAR1_EN_PAD_FORCE     BIT32(31)
#define SENS_SAR1_DIG_FORCE        BIT32(31)
#define SENS_MEAS_STATUS_MASK      (0xffu << 22)
#define SENS_FORCE_XPD_SAR_MASK    (3u << 29)
#define SENS_FORCE_XPD_SAR_ON      (3u << 29)
#define SENS_SARADC_CLK_ENABLE     BIT32(30)
#define SENS_TSENS_CLK_ENABLE      BIT32(29)
#define SENS_TSENS_RESET           BIT32(29)
#define SENS_TSENS_OUT_MASK        0xffu
#define SENS_TSENS_READY           BIT32(8)
#define SENS_TSENS_INT_ENABLE      BIT32(12)
#define SENS_TSENS_CLK_DIV_SHIFT   14
#define SENS_TSENS_CLK_DIV_MASK    (0xffu << SENS_TSENS_CLK_DIV_SHIFT)
#define SENS_TSENS_POWER_UP        BIT32(22)
#define SENS_TSENS_POWER_UP_FORCE  BIT32(23)
#define SENS_TSENS_DUMP_OUT        BIT32(24)
#define SENS_TSENS_XPD_FORCE_SHIFT 12
#define SENS_TSENS_XPD_FORCE_MASK  (3u << SENS_TSENS_XPD_FORCE_SHIFT)
#define SENS_TSENS_XPD_FORCE_ON    (1u << SENS_TSENS_XPD_FORCE_SHIFT)
#define RTC_SAR_I2C_POWER_UP       BIT32(22)
#define ANA_I2C_SAR_DISABLE        BIT32(18)
#define ANA_SAR_CONFIG_ENABLE      BIT32(16)

#define ADC_ATTEN_12DB             3u
#define ADC_ANALOG_I2C_BLOCK       0x69u
#define ADC_TSENS_DAC_REG          6u
#define ADC_TSENS_DAC_VALUE        15u

#endif // esp32s3_adc_regs.h
