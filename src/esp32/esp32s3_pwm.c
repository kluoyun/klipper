// ESP32 LEDC hardware PWM support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h"
#include "board/irq.h"
#include "command.h"
#include "esp32_regs.h"
#include "gpio.h"
#include "internal.h"
#include "sched.h"

#define MAX_PWM (1u << 15)
DECL_CONSTANT("PWM_MAX", MAX_PWM);

struct ledc_timer_state {
    uint32_t cycle_time;
    uint32_t divider;
    uint8_t resolution;
    uint8_t used;
};

static struct ledc_timer_state ledc_timers[LEDC_TIMER_COUNT];
static uint8_t ledc_channel_count;
static uint8_t ledc_initialized;

static void
ledc_init(void)
{
    if (ledc_initialized)
        return;
    REG32(SYSTEM_PERIP_CLK_EN0) |= SYSTEM_LEDC_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) |= SYSTEM_LEDC_BIT;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~SYSTEM_LEDC_BIT;
    REG32(LEDC_CONF) = LEDC_CLOCK_ENABLE | LEDC_CLOCK_APB;
    ledc_initialized = 1;
}

static uint32_t
ledc_calculate_divider(uint32_t cycle_time, uint8_t resolution)
{
#if CONFIG_MACH_ESP32C3
    // Convert 16MHz timer ticks to the 80MHz LEDC clock.
    uint64_t scaled = (uint64_t)cycle_time * 1280u;
    return (scaled + (1u << (resolution - 1u))) >> resolution;
#else
    // Keep the S3 calculation 32-bit to avoid a division helper.
    if (resolution >= 8) {
        uint32_t denominator = 3u << (resolution - 8u);
        return (cycle_time + denominator / 2u) / denominator;
    }
    uint32_t scale = 1u << (8u - resolution);
    return (cycle_time / 3u) * scale
        + ((cycle_time % 3u) * scale + 1u) / 3u;
#endif
}

static uint8_t
ledc_find_timer(uint32_t cycle_time, uint8_t *resolution_out)
{
    uint8_t timer;
    for (timer = 0; timer < LEDC_TIMER_COUNT; timer++) {
        if (ledc_timers[timer].used
            && ledc_timers[timer].cycle_time == cycle_time) {
            *resolution_out = ledc_timers[timer].resolution;
            return timer;
        }
    }

    // Use 13 bits so a 100% duty value fits both LEDC implementations.
    uint8_t resolution = 13;
    uint32_t divider = ledc_calculate_divider(cycle_time, resolution);
    while (divider < 0x100u && resolution > 1) {
        resolution--;
        divider = ledc_calculate_divider(cycle_time, resolution);
    }
    if (divider < 0x100u || divider > 0x3ffffu)
        shutdown("Invalid ESP32 PWM cycle time");

    for (timer = 0; timer < LEDC_TIMER_COUNT; timer++)
        if (!ledc_timers[timer].used)
            break;
    if (timer >= LEDC_TIMER_COUNT)
        shutdown("No ESP32 LEDC timer available");

    ledc_timers[timer] = (struct ledc_timer_state) {
        .cycle_time = cycle_time,
        .divider = divider,
        .resolution = resolution,
        .used = 1,
    };
    uint32_t conf = resolution | (divider << 4);
    REG32(LEDC_TIMER_CONF(timer)) = conf | LEDC_TIMER_RESET;
    REG32(LEDC_TIMER_CONF(timer)) = conf | LEDC_TIMER_UPDATE;
    *resolution_out = resolution;
    return timer;
}

void
gpio_pwm_write(struct gpio_pwm g, uint32_t val)
{
    uint32_t top = 1u << g.resolution;
    uint32_t duty = (val * top + MAX_PWM / 2u) / MAX_PWM;
    REG32(LEDC_CH_DUTY(g.channel)) = duty << 4;
    // A one-step fade applies the duty immediately.
    REG32(LEDC_CH_CONF1(g.channel)) =
        LEDC_DUTY_START | LEDC_DUTY_IMMEDIATE;
    REG32(LEDC_CH_CONF0(g.channel)) |= LEDC_CH_UPDATE;
}

struct gpio_pwm
gpio_pwm_setup(uint32_t pin, uint32_t cycle_time, uint32_t val)
{
    if (!cycle_time)
        shutdown("Invalid ESP32 PWM cycle time");

    irqstatus_t flag = irq_save();
    ledc_init();
    if (ledc_channel_count >= LEDC_CHANNEL_COUNT)
        shutdown("No ESP32 LEDC channel available");
    uint8_t resolution;
    uint8_t timer = ledc_find_timer(cycle_time, &resolution);
    uint8_t channel = ledc_channel_count++;

    REG32(LEDC_CH_HPOINT(channel)) = 0;
    REG32(LEDC_CH_CONF0(channel)) =
        (timer & LEDC_CH_TIMER_MASK) | LEDC_CH_SIGNAL_ENABLE | LEDC_CH_UPDATE;
    esp32_gpio_peripheral(pin, GPIO_MATRIX_LEDC0_OUT + channel, 1, 0);
    struct gpio_pwm g = { .channel = channel, .resolution = resolution };
    gpio_pwm_write(g, val);
    irq_restore(flag);
    return g;
}
