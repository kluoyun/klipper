// ESP32-S3 LEDC register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_LEDC_REGS_H
#define __ESP32S3_LEDC_REGS_H

#define LEDC_CHANNEL_COUNT         8
#define LEDC_TIMER_COUNT           4
#define LEDC_CH_CONF0(ch)          (ESP_LEDC_BASE + (ch) * 0x14u)
#define LEDC_CH_HPOINT(ch)         (ESP_LEDC_BASE + (ch) * 0x14u + 0x04u)
#define LEDC_CH_DUTY(ch)           (ESP_LEDC_BASE + (ch) * 0x14u + 0x08u)
#define LEDC_CH_CONF1(ch)          (ESP_LEDC_BASE + (ch) * 0x14u + 0x0cu)
#define LEDC_TIMER_CONF(timer)     (ESP_LEDC_BASE + 0x0a0u + (timer) * 8u)
#define LEDC_CONF                  (ESP_LEDC_BASE + 0x0d0u)

#define LEDC_CH_TIMER_MASK         0x3u
#define LEDC_CH_SIGNAL_ENABLE      BIT32(2)
#define LEDC_CH_UPDATE             BIT32(4)
#define LEDC_DUTY_START            BIT32(31)
#define LEDC_DUTY_IMMEDIATE        (BIT32(30) | BIT32(20) | BIT32(10))
#define LEDC_TIMER_RESET           BIT32(23)
#define LEDC_TIMER_UPDATE          BIT32(25)
#define LEDC_CLOCK_ENABLE          BIT32(31)
#define LEDC_CLOCK_APB             1u

#endif // esp32s3_ledc_regs.h
