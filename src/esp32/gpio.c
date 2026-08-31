// ESP32 GPIO support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <string.h>
#include "board/irq.h"
#include "command.h"
#include "esp32_regs.h"
#include "gpio.h"
#include "internal.h"
#include "sched.h"

#if CONFIG_MACH_ESP32C3
// GPIO11-GPIO17 are reserved for SPI flash.
DECL_ENUMERATION_RANGE("pin", "gpio0", 0, 11);
DECL_ENUMERATION_RANGE("pin", "gpio18", 18, 4);
#elif CONFIG_MACH_ESP32S3
// GPIO22-GPIO37 are unavailable or reserved for flash and PSRAM.
DECL_ENUMERATION_RANGE("pin", "gpio0", 0, 22);
DECL_ENUMERATION_RANGE("pin", "gpio38", 38, 11);
#endif

static void
check_pin(uint32_t pin)
{
#if CONFIG_MACH_ESP32C3
    if (pin > 21 || (pin >= 11 && pin <= 17))
        shutdown("Not a valid ESP32-C3 pin");
#else
    if (pin > 48 || (pin >= 22 && pin <= 37))
        shutdown("Not a valid ESP32-S3 pin");
#endif
}

static void
setup_iomux(uint32_t pin, int input, int pull_up)
{
    check_pin(pin);
#if CONFIG_MACH_ESP32C3
    if (pin == 18 || pin == 19)
        REG32(USB_SERIAL_JTAG_CONF0) &= ~USB_SERIAL_JTAG_PAD_ENABLE;
#else
    if (pin == 19 || pin == 20) {
        REG32(USB_SERIAL_JTAG_CONF0) &= ~USB_SERIAL_JTAG_PAD_ENABLE;
        REG32(USB_WRAP_OTG_CONF) &= ~USB_WRAP_PAD_ENABLE;
    }
#endif
    uint32_t cfg = IOMUX_DRIVE_2 | IOMUX_FUNC_GPIO;
    if (input)
        cfg |= IOMUX_INPUT_ENABLE;
    if (pull_up > 0)
        cfg |= IOMUX_PULLUP;
    else if (pull_up < 0)
        cfg |= IOMUX_PULLDOWN;
    REG32(IOMUX_GPIO(pin)) = cfg;
}

void
esp32_gpio_peripheral(uint32_t pin, uint32_t signal, int output, int pull_up)
{
    setup_iomux(pin, !output, pull_up);
    if (output)
        // Route both output data and output enable.
        REG32(GPIO_FUNC_OUT(pin)) = signal;
    else
        REG32(GPIO_FUNC_IN(signal)) = GPIO_MATRIX_IN_ENABLE | pin;
}

void
esp32_gpio_peripheral_bidir(uint32_t pin, uint32_t signal, int pull_up)
{
    setup_iomux(pin, 1, pull_up);
    REG32(GPIO_FUNC_OUT(pin)) = signal;
    REG32(GPIO_FUNC_IN(signal)) = GPIO_MATRIX_IN_ENABLE | pin;
    REG32(GPIO_PIN(pin)) |= GPIO_PAD_DRIVER;
}

void
esp32_gpio_analog(uint32_t pin)
{
    check_pin(pin);
    REG32(pin < 32 ? GPIO_ENABLE_CLEAR : GPIO_ENABLE1_CLEAR) =
        BIT32(pin & 31);
    // Disconnect ADC pads from the digital input path.
    REG32(IOMUX_GPIO(pin)) = IOMUX_DRIVE_2 | IOMUX_FUNC_GPIO;
}

struct gpio_out
gpio_out_setup(uint8_t pin, uint8_t val)
{
    check_pin(pin);
    struct gpio_out g = {
        .reg = (volatile uint32_t *)(uintptr_t)(pin < 32
                                                ? GPIO_OUT : GPIO_OUT1),
        .bit = BIT32(pin & 31),
    };
    gpio_out_reset(g, val);
    return g;
}

void
gpio_out_reset(struct gpio_out g, uint8_t val)
{
    uint32_t pin = __builtin_ctz(g.bit);
#if CONFIG_MACH_ESP32S3
    pin += (g.reg == (void *)GPIO_OUT1) * 32;
#endif
    irqstatus_t flag = irq_save();
    gpio_out_write(g, val);
    setup_iomux(pin, 0, 0);
    // Route the GPIO output latch and output enable.
    REG32(GPIO_FUNC_OUT(pin)) =
        GPIO_MATRIX_OEN_GPIO | GPIO_MATRIX_GPIO_OUT;
    REG32(pin < 32 ? GPIO_ENABLE_SET : GPIO_ENABLE1_SET) = g.bit;
    irq_restore(flag);
}

void
gpio_out_write(struct gpio_out g, uint8_t val)
{
    // SET and CLEAR follow the OUT register.
    g.reg[val ? 1 : 2] = g.bit;
}

void
gpio_out_toggle_noirq(struct gpio_out g)
{
    *g.reg ^= g.bit;
}

void
gpio_out_toggle(struct gpio_out g)
{
    irqstatus_t flag = irq_save();
    gpio_out_toggle_noirq(g);
    irq_restore(flag);
}

struct gpio_in
gpio_in_setup(uint8_t pin, int8_t pull_up)
{
    check_pin(pin);
    struct gpio_in g = { .pin = pin };
    gpio_in_reset(g, pull_up);
    return g;
}

void
gpio_in_reset(struct gpio_in g, int8_t pull_up)
{
    uint32_t pin = g.pin;
    irqstatus_t flag = irq_save();
    REG32(pin < 32 ? GPIO_ENABLE_CLEAR : GPIO_ENABLE1_CLEAR) =
        BIT32(pin & 31);
    setup_iomux(pin, 1, pull_up);
    irq_restore(flag);
}

uint8_t
gpio_in_read(struct gpio_in g)
{
    uint32_t pin = g.pin;
    return !!(REG32(pin < 32 ? GPIO_IN : GPIO_IN1) & BIT32(pin & 31));
}
