// ESP32-C3 GPIO register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32C3_GPIO_REGS_H
#define __ESP32C3_GPIO_REGS_H

#define GPIO_OUT                   (ESP_GPIO_BASE + 0x004u)
#define GPIO_OUT_SET               (ESP_GPIO_BASE + 0x008u)
#define GPIO_OUT_CLEAR             (ESP_GPIO_BASE + 0x00cu)
#define GPIO_ENABLE                (ESP_GPIO_BASE + 0x020u)
#define GPIO_ENABLE_SET            (ESP_GPIO_BASE + 0x024u)
#define GPIO_ENABLE_CLEAR          (ESP_GPIO_BASE + 0x028u)
#define GPIO_IN                    (ESP_GPIO_BASE + 0x03cu)
#define GPIO_OUT1                  GPIO_OUT
#define GPIO_OUT1_SET              GPIO_OUT_SET
#define GPIO_OUT1_CLEAR            GPIO_OUT_CLEAR
#define GPIO_ENABLE1_SET           GPIO_ENABLE_SET
#define GPIO_ENABLE1_CLEAR         GPIO_ENABLE_CLEAR
#define GPIO_IN1                   GPIO_IN
#define GPIO_PIN(pin)              (ESP_GPIO_BASE + 0x074u + 4u * (pin))
#define GPIO_FUNC_IN(signal)       (ESP_GPIO_BASE + 0x154u \
                                    + 4u * (signal))
#define GPIO_FUNC_OUT(pin)         (ESP_GPIO_BASE + 0x554u + 4u * (pin))
#define IOMUX_GPIO(pin)            (ESP_IOMUX_BASE + 0x004u + 4u * (pin))
#define IOMUX_PULLDOWN             BIT32(7)
#define IOMUX_PULLUP               BIT32(8)
#define IOMUX_INPUT_ENABLE         BIT32(9)
#define IOMUX_DRIVE_2              (2u << 10)
#define IOMUX_FUNC_GPIO            (1u << 12)
#define GPIO_MATRIX_IN_ENABLE      BIT32(6)
#define GPIO_MATRIX_OEN_GPIO       BIT32(9)
#define GPIO_MATRIX_GPIO_OUT       128u
#define GPIO_PAD_DRIVER            BIT32(2)
#define GPIO_MATRIX_UART0_RX       6u
#define GPIO_MATRIX_UART0_TX       6u
#define GPIO_MATRIX_UART1_RX       9u
#define GPIO_MATRIX_UART1_TX       9u
#define GPIO_MATRIX_LEDC0_OUT      45u
#define GPIO_MATRIX_I2C0_SCL       53u
#define GPIO_MATRIX_I2C0_SDA       54u
#define GPIO_MATRIX_SPI2_CLK       63u
#define GPIO_MATRIX_SPI2_MISO      64u
#define GPIO_MATRIX_SPI2_MOSI      65u

#endif // esp32c3_gpio_regs.h
