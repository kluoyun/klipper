// ESP32-S3 GPIO register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_GPIO_REGS_H
#define __ESP32S3_GPIO_REGS_H

#define GPIO_OUT                   (ESP_GPIO_BASE + 0x004u)
#define GPIO_OUT_SET               (ESP_GPIO_BASE + 0x008u)
#define GPIO_OUT_CLEAR             (ESP_GPIO_BASE + 0x00cu)
#define GPIO_OUT1                  (ESP_GPIO_BASE + 0x010u)
#define GPIO_OUT1_SET              (ESP_GPIO_BASE + 0x014u)
#define GPIO_OUT1_CLEAR            (ESP_GPIO_BASE + 0x018u)
#define GPIO_ENABLE_SET            (ESP_GPIO_BASE + 0x024u)
#define GPIO_ENABLE_CLEAR          (ESP_GPIO_BASE + 0x028u)
#define GPIO_ENABLE1_SET           (ESP_GPIO_BASE + 0x030u)
#define GPIO_ENABLE1_CLEAR         (ESP_GPIO_BASE + 0x034u)
#define GPIO_IN                    (ESP_GPIO_BASE + 0x03cu)
#define GPIO_IN1                   (ESP_GPIO_BASE + 0x040u)
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
#define GPIO_MATRIX_IN_ENABLE      BIT32(7)
#define GPIO_MATRIX_CONST_ONE      0x38u
#define GPIO_MATRIX_CONST_ZERO     0x3cu
#define GPIO_MATRIX_OEN_GPIO       BIT32(10)
#define GPIO_MATRIX_GPIO_OUT       256u
#define GPIO_PAD_DRIVER            BIT32(2)
#define GPIO_MATRIX_UART0_RX       12u
#define GPIO_MATRIX_UART0_TX       12u
#define GPIO_MATRIX_UART1_RX       15u
#define GPIO_MATRIX_UART1_TX       15u
#define GPIO_MATRIX_UART2_RX       18u
#define GPIO_MATRIX_UART2_TX       18u
#define GPIO_MATRIX_SPI3_CLK       66u
#define GPIO_MATRIX_SPI3_MISO      67u
#define GPIO_MATRIX_SPI3_MOSI      68u
#define GPIO_MATRIX_LEDC0_OUT      73u
#define GPIO_MATRIX_I2C0_SCL       89u
#define GPIO_MATRIX_I2C0_SDA       90u
#define GPIO_MATRIX_I2C1_SCL       91u
#define GPIO_MATRIX_I2C1_SDA       92u
#define GPIO_MATRIX_SPI2_CLK       101u
#define GPIO_MATRIX_SPI2_MISO      102u
#define GPIO_MATRIX_SPI2_MOSI      103u
#define GPIO_MATRIX_TWAI           116u
#define GPIO_MATRIX_USB_IDDIG      58u
#define GPIO_MATRIX_USB_AVALID     59u
#define GPIO_MATRIX_USB_BVALID     60u
#define GPIO_MATRIX_USB_VBUSVALID  61u

#endif // esp32s3_gpio_regs.h
