// ESP32-S3 system timer register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_SYSTIMER_REGS_H
#define __ESP32S3_SYSTIMER_REGS_H

#define SYSTIMER_CONF              (ESP_SYSTIMER_BASE + 0x000u)
#define SYSTIMER_UNIT0_OP          (ESP_SYSTIMER_BASE + 0x004u)
#define SYSTIMER_TARGET0_HI        (ESP_SYSTIMER_BASE + 0x01cu)
#define SYSTIMER_TARGET0_LO        (ESP_SYSTIMER_BASE + 0x020u)
#define SYSTIMER_TARGET0_CONF      (ESP_SYSTIMER_BASE + 0x034u)
#define SYSTIMER_UNIT0_VALUE_HI    (ESP_SYSTIMER_BASE + 0x040u)
#define SYSTIMER_UNIT0_VALUE_LO    (ESP_SYSTIMER_BASE + 0x044u)
#define SYSTIMER_COMP0_LOAD        (ESP_SYSTIMER_BASE + 0x050u)
#define SYSTIMER_INT_ENABLE        (ESP_SYSTIMER_BASE + 0x064u)
#define SYSTIMER_INT_CLEAR         (ESP_SYSTIMER_BASE + 0x06cu)
#define SYSTIMER_CLOCK_ENABLE      BIT32(31)
#define SYSTIMER_UNIT0_ENABLE      BIT32(30)
#define SYSTIMER_TARGET0_ENABLE    BIT32(24)
#define SYSTIMER_UNIT0_UPDATE      BIT32(30)
#define SYSTIMER_UNIT0_VALID       BIT32(29)
#define SYSTIMER_TARGET0_INT       BIT32(0)

#endif // esp32s3_systimer_regs.h
