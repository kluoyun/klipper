// ESP32-S3 ADC1 oneshot support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "board/misc.h"
#include "command.h"
#include "esp32_regs.h"
#include "gpio.h"
#include "internal.h"
#include "sched.h"

#define ADC_NO_CHANNEL 0xffu
#define ADC_TEMPERATURE_PIN 0xfeu

DECL_CONSTANT("ADC_MAX", 4095);
DECL_ENUMERATION("pin", "ADC_TEMPERATURE", ADC_TEMPERATURE_PIN);

static uint8_t adc_active_channel = ADC_NO_CHANNEL;
static uint8_t adc_calibrated;
static uint8_t tsens_warmed;
static uint8_t tsens_measure_started;

static void
adc_power(uint8_t on)
{
    REG32(SENS_SAR_PERI_CLK_GATE_CONF) |= SENS_SARADC_CLK_ENABLE;
    uint32_t power = REG32(SENS_SAR_POWER_XPD_SAR);
    power &= ~SENS_FORCE_XPD_SAR_MASK;
    if (on)
        power |= SENS_FORCE_XPD_SAR_ON;
    REG32(SENS_SAR_POWER_XPD_SAR) = power;
}

static uint32_t
efuse_block2_read(uint16_t bit, uint8_t count)
{
    uint8_t word = bit / 32u;
    uint8_t shift = bit & 31u;
    uint32_t value = REG32(EFUSE_BLOCK2_READ_BASE + 4u * word) >> shift;
    if (shift + count > 32u)
        value |= REG32(EFUSE_BLOCK2_READ_BASE + 4u * (word + 1u))
            << (32u - shift);
    return value & ((1u << count) - 1u);
}

static void
adc_calibration_init(void)
{
    if (adc_calibrated)
        return;
    // Enable the SAR analog-I2C block.
    REG32(RTC_ANA_CONF) |= RTC_SAR_I2C_POWER_UP;
    REG32(ANA_CONFIG) &= ~ANA_I2C_SAR_DISABLE;
    REG32(ANA_CONFIG2) |= ANA_SAR_CONFIG_ENABLE;
    // Reconstruct the initial code from eFuse calibration data.
    uint32_t code = 2010u;
    if (efuse_block2_read(128, 2) == 1u) {
        code = efuse_block2_read(149, 8) + 1850u;
        code += efuse_block2_read(157, 6) + 90u;
        code += efuse_block2_read(163, 6);
        code += efuse_block2_read(169, 6) + 70u;
    }
    esp32_analog_i2c_write_mask(ADC_ANALOG_I2C_BLOCK, 2, 6, 4, 4);
    esp32_analog_i2c_write_mask(ADC_ANALOG_I2C_BLOCK, 1, 3, 0, code >> 8);
    esp32_analog_i2c_write_mask(ADC_ANALOG_I2C_BLOCK, 0, 7, 0, code);
    adc_calibrated = 1;
}

static void
tsens_init(void)
{
    adc_calibration_init();

    // Select TSENS range 0 and keep the sensor powered.
    REG32(SENS_SAR_PERI_CLK_GATE_CONF) |= SENS_TSENS_CLK_ENABLE;
    REG32(SENS_SAR_PERI_RESET_CONF) |= SENS_TSENS_RESET;
    REG32(SENS_SAR_PERI_RESET_CONF) &= ~SENS_TSENS_RESET;
    esp32_analog_i2c_write_mask(ADC_ANALOG_I2C_BLOCK, ADC_TSENS_DAC_REG,
                               3, 0, ADC_TSENS_DAC_VALUE);
    uint32_t ctrl = REG32(SENS_SAR_TSENS_CTRL);
    ctrl &= ~(SENS_TSENS_CLK_DIV_MASK | SENS_TSENS_INT_ENABLE
              | SENS_TSENS_DUMP_OUT);
    ctrl |= 6u << SENS_TSENS_CLK_DIV_SHIFT;
    REG32(SENS_SAR_TSENS_CTRL) = ctrl;
    REG32(SENS_SAR_TSENS_CTRL2) =
        (REG32(SENS_SAR_TSENS_CTRL2) & ~SENS_TSENS_XPD_FORCE_MASK)
        | SENS_TSENS_XPD_FORCE_ON;
    REG32(SENS_SAR_TSENS_CTRL) = ctrl | SENS_TSENS_POWER_UP_FORCE
        | SENS_TSENS_POWER_UP;
}

struct gpio_adc
gpio_adc_setup(uint32_t pin)
{
    if (pin == ADC_TEMPERATURE_PIN) {
        tsens_init();
        return (struct gpio_adc) { .channel = ADC_TEMPERATURE_PIN };
    }
    // ADC1 channels 0..9 map to GPIO1..GPIO10.
    if (pin < 1 || pin > 10)
        shutdown("Not a valid ESP32-S3 ADC1 pin");
    uint8_t channel = pin - 1;
    esp32_gpio_analog(pin);
    adc_calibration_init();

    // Select 12-bit RTC one-shot mode and 12dB attenuation.
    REG32(SENS_SAR_READER1_CTRL) =
        (REG32(SENS_SAR_READER1_CTRL) & ~SENS_SAR1_CLK_DIV_MASK
         & ~SENS_SAR1_DATA_INVERT) | SENS_SAR1_CLK_GATED | 1u;
    REG32(SENS_SAR_MEAS1_MUX) &= ~SENS_SAR1_DIG_FORCE;
    REG32(SENS_SAR_MEAS1_CTRL2) |=
        SENS_MEAS1_START_FORCE | SENS_SAR1_EN_PAD_FORCE;
    uint32_t shift = channel * 2u;
    REG32(SENS_SAR_ATTEN1) =
        (REG32(SENS_SAR_ATTEN1) & ~(3u << shift))
        | (ADC_ATTEN_12DB << shift);
    adc_power(0);
    return (struct gpio_adc) { .channel = channel };
}

uint32_t
gpio_adc_sample(struct gpio_adc g)
{
    if (adc_active_channel == g.channel) {
        if (g.channel == ADC_TEMPERATURE_PIN) {
            if (!tsens_measure_started) {
                REG32(SENS_SAR_TSENS_CTRL) |= SENS_TSENS_DUMP_OUT;
                tsens_measure_started = 1;
                return timer_from_us(5);
            }
            if (REG32(SENS_SAR_TSENS_CTRL) & SENS_TSENS_READY)
                return 0;
            return timer_from_us(5);
        }
        if (REG32(SENS_SAR_MEAS1_CTRL2) & SENS_MEAS1_DONE)
            return 0;
        return timer_from_us(5);
    }
    if (adc_active_channel != ADC_NO_CHANNEL)
        return timer_from_us(5);
    if (g.channel == ADC_TEMPERATURE_PIN) {
        adc_active_channel = g.channel;
        tsens_measure_started = 0;
        if (!tsens_warmed) {
            tsens_warmed = 1;
            return timer_from_us(300);
        }
        REG32(SENS_SAR_TSENS_CTRL) |= SENS_TSENS_DUMP_OUT;
        tsens_measure_started = 1;
        return timer_from_us(5);
    }
    if (REG32(SENS_SAR_SLAVE_ADDR1) & SENS_MEAS_STATUS_MASK)
        return timer_from_us(5);

    adc_power(1);
    adc_active_channel = g.channel;
    uint32_t ctrl = REG32(SENS_SAR_MEAS1_CTRL2);
    ctrl &= ~(SENS_SAR1_EN_PAD_MASK | SENS_MEAS1_START);
    ctrl |= SENS_MEAS1_START_FORCE | SENS_SAR1_EN_PAD_FORCE
        | (1u << (SENS_SAR1_EN_PAD_SHIFT + g.channel));
    REG32(SENS_SAR_MEAS1_CTRL2) = ctrl;
    REG32(SENS_SAR_MEAS1_CTRL2) = ctrl | SENS_MEAS1_START;
    return timer_from_us(5);
}

uint16_t
gpio_adc_read(struct gpio_adc g)
{
    if (adc_active_channel != g.channel)
        shutdown("Invalid ESP32-S3 ADC read");
    if (g.channel == ADC_TEMPERATURE_PIN) {
        uint32_t raw = REG32(SENS_SAR_TSENS_CTRL) & SENS_TSENS_OUT_MASK;
        REG32(SENS_SAR_TSENS_CTRL) &= ~SENS_TSENS_DUMP_OUT;
        adc_active_channel = ADC_NO_CHANNEL;
        tsens_measure_started = 0;
        return (raw * 4095u + 127u) / 255u;
    }
    uint32_t raw = REG32(SENS_SAR_MEAS1_CTRL2) & SENS_MEAS1_DATA_MASK;
    adc_active_channel = ADC_NO_CHANNEL;
    adc_power(0);
    return raw & 0xfffu;
}

void
gpio_adc_cancel_sample(struct gpio_adc g)
{
    if (adc_active_channel != g.channel)
        return;
    if (g.channel == ADC_TEMPERATURE_PIN) {
        REG32(SENS_SAR_TSENS_CTRL) &= ~SENS_TSENS_DUMP_OUT;
        adc_active_channel = ADC_NO_CHANNEL;
        tsens_measure_started = 0;
        return;
    }
    REG32(SENS_SAR_MEAS1_CTRL2) &=
        ~(SENS_SAR1_EN_PAD_MASK | SENS_MEAS1_START);
    adc_active_channel = ADC_NO_CHANNEL;
    adc_power(0);
}
