// Bare-metal startup support for ESP32-C3
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h"
#include "command.h"
#include "internal.h"
#include "sched.h"

DECL_CONSTANT_STR("MCU", CONFIG_MCU);

void __visible __noreturn
esp32_main(void)
{
    watchdog_early_init();
    esp32_irq_reset();
    sched_main();
    for (;;)
        ;
}
