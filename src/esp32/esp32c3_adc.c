// ESP32-C3 ADC1 oneshot support
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
static uint8_t adc_initialized;
static uint8_t adc_wait_count;
static uint8_t tsens_warmed;

typedef void (*rom_regi2c_write_mask_t)(uint8_t block, uint8_t host,
                                        uint8_t reg, uint8_t msb,
                                        uint8_t lsb, uint8_t data);
typedef void (*rom_delay_us_t)(uint32_t us);

static void
adc_analog_i2c_write_mask(uint8_t reg, uint8_t msb, uint8_t lsb,
                          uint8_t data)
{
    rom_regi2c_write_mask_t write_mask =
        (rom_regi2c_write_mask_t)(uintptr_t)ADC_ROM_REGI2C_WRITE_MASK;
    write_mask(ADC_ANALOG_I2C_BLOCK, 0, reg, msb, lsb, data);
}

static void
adc_init(void)
{
    if (adc_initialized)
        return;

    REG32(SYSTEM_PERIP_CLK_EN0) |= SYSTEM_APB_SARADC_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) |= SYSTEM_APB_SARADC_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~SYSTEM_APB_SARADC_BIT;

    // Force the RTC-domain SAR power on.
    REG32(RTC_SENSOR_CTRL) =
        (REG32(RTC_SENSOR_CTRL) & ~ADC_RTC_XPD_SAR_MASK)
        | ADC_RTC_XPD_SAR_ON;

    // Load the attenuation-3 calibration code.
    REG32(RTC_ANA_CONF) |= RTC_SAR_I2C_POWER_UP;
    REG32(ANA_CONFIG) &= ~ANA_I2C_SAR_DISABLE;
    REG32(ANA_CONFIG2) |= ANA_SAR_CONFIG_ENABLE;
    uint32_t initial_code = 2000u;
    if ((REG32(EFUSE_BLOCK2_VERSION) & 3u) == 1u)
        initial_code = ((REG32(EFUSE_BLOCK2_INIT_CODE) >> 18) & 0x3ffu)
            + 1000u;
    adc_analog_i2c_write_mask(2, 6, 4, 1);
    adc_analog_i2c_write_mask(1, 3, 0, initial_code >> 8);
    adc_analog_i2c_write_mask(0, 7, 0, initial_code);

    // Use APB/16 for the controller and divide the SAR clock by two.
    REG32(ADC_CLK_CONF) = ADC_CLK_SELECT_APB | ADC_CLK_ENABLE | 15u;
    REG32(ADC_CTRL) =
        (REG32(ADC_CTRL) & ~(ADC_XPD_SAR_FORCE_MASK
                            | ADC_SAR_CLK_DIV_MASK))
        | ADC_XPD_SAR_FORCE_ON | ADC_SAR_CLK_GATED | (1u << 7);
    REG32(ADC_FSM_WAIT) = (100u << 16) | (8u << 8) | 5u;
    REG32(ADC_INT_CLR) = ADC1_DONE;
    adc_initialized = 1;
}

static void
tsens_init(void)
{
    adc_init();

    // Use the crystal clock with divider 6.
    REG32(SYSTEM_PERIP_CLK_EN1) |= SYSTEM_TSENS_BIT;
    REG32(SYSTEM_PERIP_RST_EN1) |= SYSTEM_TSENS_BIT;
    REG32(SYSTEM_PERIP_RST_EN1) &= ~SYSTEM_TSENS_BIT;
    REG32(ADC_TSENS_CTRL2) |= ADC_TSENS_CLK_XTAL;
    adc_analog_i2c_write_mask(ADC_TSENS_DAC_REG, 3, 0,
                              ADC_TSENS_DAC_VALUE);
    REG32(ADC_TSENS_CTRL) =
        (REG32(ADC_TSENS_CTRL) & ~ADC_TSENS_CLK_DIV_MASK)
        | (6u << ADC_TSENS_CLK_DIV_SHIFT) | ADC_TSENS_POWER_UP;
}

struct gpio_adc
gpio_adc_setup(uint32_t pin)
{
    if (pin == ADC_TEMPERATURE_PIN) {
        tsens_init();
        return (struct gpio_adc) { .channel = ADC_TEMPERATURE_PIN };
    }
    // ADC1 channels 0..4 map to GPIO0..GPIO4.
    if (pin > 4)
        shutdown("Not a valid ESP32-C3 ADC1 pin");
    esp32_gpio_analog(pin);
    adc_init();
    return (struct gpio_adc) { .channel = pin };
}

uint32_t
gpio_adc_sample(struct gpio_adc g)
{
    if (adc_active_channel == g.channel) {
        if (g.channel == ADC_TEMPERATURE_PIN)
            return 0;
        if (REG32(ADC_INT_RAW) & ADC1_DONE)
            return 0;

        // Stop polling after 100us if the DONE flag is missing.
        if (++adc_wait_count >= 20)
            return 0;
        return timer_from_us(5);
    }
    if (adc_active_channel != ADC_NO_CHANNEL)
        return timer_from_us(5);

    if (g.channel == ADC_TEMPERATURE_PIN) {
        adc_active_channel = g.channel;
        if (!tsens_warmed) {
            // Wait for the first temperature sample to settle.
            tsens_warmed = 1;
            return timer_from_us(300);
        }
        return 0;
    }

    REG32(ADC_INT_CLR) = ADC1_DONE;
    uint32_t sample = ADC_ONETIME_ADC1_ENABLE
        | ((uint32_t)g.channel << ADC_ONETIME_CHANNEL_SHIFT)
        | (ADC_ATTEN_12DB << ADC_ONETIME_ATTEN_SHIFT);
    REG32(ADC_ONETIME_SAMPLE) = sample;
    REG32(ADC_ONETIME_SAMPLE) = sample | ADC_ONETIME_START;
    // Keep START high for at least three ADC clocks.
    ((rom_delay_us_t)(uintptr_t)ADC_ROM_DELAY_US)(3);
    REG32(ADC_ONETIME_SAMPLE) = sample;
    adc_active_channel = g.channel;
    adc_wait_count = 0;
    return timer_from_us(5);
}

uint16_t
gpio_adc_read(struct gpio_adc g)
{
    if (adc_active_channel != g.channel)
        shutdown("Invalid ESP32-C3 ADC read");
    if (g.channel == ADC_TEMPERATURE_PIN) {
        uint32_t raw = REG32(ADC_TSENS_CTRL) & ADC_TSENS_OUT_MASK;
        adc_active_channel = ADC_NO_CHANNEL;
        return (raw * 4095u + 127u) / 255u;
    }
    uint16_t value = REG32(ADC_DATA1) & 0xfffu;
    REG32(ADC_ONETIME_SAMPLE) = 0;
    REG32(ADC_INT_CLR) = ADC1_DONE;
    adc_active_channel = ADC_NO_CHANNEL;
    adc_wait_count = 0;
    return value;
}

void
gpio_adc_cancel_sample(struct gpio_adc g)
{
    if (adc_active_channel != g.channel)
        return;
    if (g.channel == ADC_TEMPERATURE_PIN) {
        adc_active_channel = ADC_NO_CHANNEL;
        return;
    }
    REG32(ADC_ONETIME_SAMPLE) = 0;
    REG32(ADC_INT_CLR) = ADC1_DONE;
    adc_active_channel = ADC_NO_CHANNEL;
    adc_wait_count = 0;
}
