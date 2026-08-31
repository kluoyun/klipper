// ESP32-S3 TWAI register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_TWAI_REGS_H
#define __ESP32S3_TWAI_REGS_H

#define TWAI_REG(offset)           REG32(ESP_TWAI_BASE + (offset))
#define TWAI_MODE                  TWAI_REG(0x00u)
#define TWAI_COMMAND               TWAI_REG(0x04u)
#define TWAI_STATUS                TWAI_REG(0x08u)
#define TWAI_INTERRUPT             TWAI_REG(0x0cu)
#define TWAI_INTERRUPT_ENABLE      TWAI_REG(0x10u)
#define TWAI_BUS_TIMING0           TWAI_REG(0x18u)
#define TWAI_BUS_TIMING1           TWAI_REG(0x1cu)
#define TWAI_ARB_LOST_CAPTURE      TWAI_REG(0x2cu)
#define TWAI_ERROR_CAPTURE         TWAI_REG(0x30u)
#define TWAI_ERROR_WARNING         TWAI_REG(0x34u)
#define TWAI_RX_ERROR_COUNT        TWAI_REG(0x38u)
#define TWAI_TX_ERROR_COUNT        TWAI_REG(0x3cu)
#define TWAI_BUFFER(pos)           TWAI_REG(0x40u + 4u * (pos))
#define TWAI_RX_MESSAGE_COUNT      TWAI_REG(0x74u)
#define TWAI_CLOCK_DIVIDER         TWAI_REG(0x7cu)

#define TWAI_MODE_RESET            BIT32(0)
#define TWAI_MODE_LISTEN_ONLY     BIT32(1)
#define TWAI_MODE_SELF_TEST       BIT32(2)
#define TWAI_MODE_FILTER_SINGLE    BIT32(3)
#define TWAI_CMD_TX                BIT32(0)
#define TWAI_CMD_ABORT_TX          BIT32(1)
#define TWAI_CMD_RELEASE_RX        BIT32(2)
#define TWAI_CMD_CLEAR_OVERRUN     BIT32(3)
#define TWAI_STATUS_RX_BUFFER      BIT32(0)
#define TWAI_STATUS_OVERRUN        BIT32(1)
#define TWAI_STATUS_TX_BUFFER      BIT32(2)
#define TWAI_STATUS_TX_COMPLETE    BIT32(3)
#define TWAI_STATUS_ERROR          BIT32(6)
#define TWAI_STATUS_BUS_OFF        BIT32(7)
#define TWAI_STATUS_MISS           BIT32(8)
#define TWAI_INTR_RX               BIT32(0)
#define TWAI_INTR_TX               BIT32(1)
#define TWAI_INTR_ERROR            BIT32(2)
#define TWAI_INTR_PASSIVE          BIT32(5)
#define TWAI_INTR_ARB_LOST         BIT32(6)
#define TWAI_INTR_BUS_ERROR        BIT32(7)
#define TWAI_INTR_ALL (TWAI_INTR_RX | TWAI_INTR_TX | TWAI_INTR_ERROR \
                       | TWAI_INTR_PASSIVE | TWAI_INTR_ARB_LOST \
                       | TWAI_INTR_BUS_ERROR)
#define TWAI_FRAME_RTR             BIT32(6)
#define TWAI_FRAME_EXTENDED        BIT32(7)
#define TWAI_APB_CLOCK             80000000u

#endif // esp32s3_twai_regs.h
