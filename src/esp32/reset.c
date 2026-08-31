// ESP32 software reset support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "board/irq.h" // irq_disable
#include "command.h" // DECL_COMMAND_FLAGS
#include "esp32_regs.h" // RTC_OPTIONS0

void
command_reset(uint32_t *args)
{
    irq_disable();
    // Reset the digital system and return to the mask ROM.
    REG32(RTC_OPTIONS0) = RTC_SW_SYS_RESET;
    for (;;)
        ;
}
DECL_COMMAND_FLAGS(command_reset, HF_IN_SHUTDOWN, "reset");
