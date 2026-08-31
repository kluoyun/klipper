// ESP32-S3 chip initialization and main entry point
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h"
#include "command.h"
#include "esp32_regs.h"
#include "internal.h"
#include "sched.h"

DECL_CONSTANT_STR("MCU", CONFIG_MCU);
DECL_CONSTANT("CPU_FREQ", 240000000);

#define I2C_BBPLL 0x66u
#define I2C_DIG_REG 0x6du
#define I2C_DIG_RTC_DREG 4u
#define I2C_DIG_DIG_DREG 6u
#define I2C_DIG_DBIAS_240M 28u

static void
analog_i2c_enable(void)
{
    REG32(SYSTEM_WIFI_CLK_EN) |= SYSTEM_WIFI_CLK_I2C;
    REG32(I2C_MST_CONFIG) = ((REG32(I2C_MST_CONFIG)
                              & ~I2C_MST_MAGIC_MASK)
                             | I2C_MST_MAGIC_VALUE);
}

static uint8_t
analog_i2c_read(uint8_t block, uint8_t reg)
{
    REG32(I2C_MST_COMMAND) = (block
                              | ((uint32_t)reg
                                 << I2C_MST_COMMAND_ADDR_SHIFT));
    while (REG32(I2C_MST_COMMAND) & I2C_MST_COMMAND_BUSY)
        ;
    return REG32(I2C_MST_COMMAND) >> I2C_MST_COMMAND_DATA_SHIFT;
}

static void
analog_i2c_write(uint8_t block, uint8_t reg, uint8_t data)
{
    REG32(I2C_MST_COMMAND) = (block
                              | ((uint32_t)reg
                                 << I2C_MST_COMMAND_ADDR_SHIFT)
                              | ((uint32_t)data
                                 << I2C_MST_COMMAND_DATA_SHIFT)
                              | I2C_MST_COMMAND_WRITE);
    while (REG32(I2C_MST_COMMAND) & I2C_MST_COMMAND_BUSY)
        ;
}

void
esp32_analog_i2c_write_mask(uint8_t block, uint8_t reg, uint8_t msb,
                            uint8_t lsb, uint8_t data)
{
    uint32_t width = msb - lsb + 1;
    uint32_t mask = ((1u << width) - 1u) << lsb;
    uint8_t old = analog_i2c_read(block, reg);
    analog_i2c_write(block, reg,
                     (old & ~mask) | ((data << lsb) & mask));
}

// Configure the 480MHz PLL and switch the CPU to 240MHz.
static void
clock_setup(void)
{
    uint32_t sysclk = REG32(SYSTEM_SYSCLK_CONF);
    uint32_t cpu_conf = REG32(SYSTEM_CPU_PER_CONF);
    uint_fast8_t pll_ready = (
        (sysclk & SYSTEM_SOC_CLK_SEL_MASK) == SYSTEM_SOC_CLK_PLL
        && (cpu_conf & SYSTEM_PLL_FREQ_480M));

    // Wait for any ROM UART output to finish.
    while ((REG32(UART0_STATUS) >> UART_TX_COUNT_SHIFT) & UART_TX_COUNT_MASK)
        ;

    analog_i2c_enable();
    if (!pll_ready) {
        // Run PLL calibration from the 40MHz crystal.
        REG32(SYSTEM_SYSCLK_CONF) = (
            (sysclk & ~(SYSTEM_PRE_DIV_MASK | SYSTEM_SOC_CLK_SEL_MASK))
            | SYSTEM_SOC_CLK_XTAL);

        // Enable the BBPLL and regulator analog-I2C blocks.
        REG32(ANA_CONFIG) |= ANA_CONFIG_RESET_MASK;
        REG32(ANA_CONFIG) &= ~(ANA_I2C_BBPLL_DISABLE
                               | ANA_I2C_DIG_DISABLE);
        // Power the BBPLL and select 480MHz mode.
        REG32(RTC_OPTIONS0) &= ~(RTC_BB_I2C_FORCE_PD
                                 | RTC_BBPLL_I2C_FORCE_PD
                                 | RTC_BBPLL_FORCE_PD);
        REG32(SYSTEM_CPU_PER_CONF) |= SYSTEM_PLL_FREQ_480M;

        REG32(I2C_MST_ANA_CONF0) &= ~I2C_MST_BBPLL_STOP_HIGH;
        REG32(I2C_MST_ANA_CONF0) |= I2C_MST_BBPLL_STOP_LOW;
        analog_i2c_write(I2C_BBPLL, 4, 0x6b); // MODE_HF
        analog_i2c_write(I2C_BBPLL, 2, 0x50); // REF_DIV and charge pump
        analog_i2c_write(I2C_BBPLL, 3, 0x08); // DIV_7_0
        esp32_analog_i2c_write_mask(I2C_BBPLL, 5, 2, 0, 0); // DR1
        // Set DR3 without changing EN_USB.
        esp32_analog_i2c_write_mask(I2C_BBPLL, 5, 6, 4, 0);
        analog_i2c_write(I2C_BBPLL, 6, 0x73); // reference selects
        esp32_analog_i2c_write_mask(I2C_BBPLL, 9, 1, 0, 3); // VCO_DBIAS
        while (!(REG32(I2C_MST_ANA_CONF0) & I2C_MST_BBPLL_CAL_DONE))
            ;
        // Allow the PLL to settle.
        for (volatile uint32_t i = 0; i < 1000; i++)
            ;
        REG32(I2C_MST_ANA_CONF0) &= ~I2C_MST_BBPLL_STOP_LOW;
        REG32(I2C_MST_ANA_CONF0) |= I2C_MST_BBPLL_STOP_HIGH;
    } else {
        REG32(ANA_CONFIG) &= ~ANA_I2C_DIG_DISABLE;
    }

    // Raise the regulator bias before switching to 240MHz.
    esp32_analog_i2c_write_mask(I2C_DIG_REG, I2C_DIG_RTC_DREG, 4, 0,
                                I2C_DIG_DBIAS_240M);
    esp32_analog_i2c_write_mask(I2C_DIG_REG, I2C_DIG_DIG_DREG, 4, 0,
                                I2C_DIG_DBIAS_240M);
    // Allow the regulator to settle.
    for (volatile uint32_t i = 0; i < 10000; i++)
        ;

    // Enable the LDOs and select 480MHz PLL / 2.
    REG32(RTC_DATE) = ((REG32(RTC_DATE) & ~RTC_SLAVE_PD_MASK)
                       | RTC_SLAVE_PD_240M);
    REG32(SYSTEM_CPU_PER_CONF) = (
        (REG32(SYSTEM_CPU_PER_CONF) & ~SYSTEM_CPU_PERIOD_MASK)
        | SYSTEM_PLL_FREQ_480M | SYSTEM_CPU_PERIOD_240M);
    REG32(SYSTEM_SYSCLK_CONF) = (
        (REG32(SYSTEM_SYSCLK_CONF)
         & ~(SYSTEM_PRE_DIV_MASK | SYSTEM_SOC_CLK_SEL_MASK))
        | SYSTEM_SOC_CLK_PLL);
}

void __visible __noreturn
esp32_main(void)
{
    watchdog_early_init();
    esp32_irq_reset();
    clock_setup();
    sched_main();
    for (;;)
        ;
}
