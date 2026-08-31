// ESP32-S3 SPI register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32S3_SPI_REGS_H
#define __ESP32S3_SPI_REGS_H

#define SPI_CMD                    0x00u
#define SPI_CTRL                   0x08u
#define SPI_CLOCK                  0x0cu
#define SPI_USER                   0x10u
#define SPI_USER1                  0x14u
#define SPI_MS_DLEN                0x1cu
#define SPI_MISC                   0x20u
#define SPI_DMA_CONF               0x30u
#define SPI_DATA(index)            (0x98u + 4u * (index))
#define SPI_SLAVE                  0xe0u
#define SPI_CLK_GATE               0xe8u
#define SPI_REG(config, offset)    REG32((config).base + (offset))

#define SPI_CMD_USER               BIT32(24)
#define SPI_CMD_UPDATE             BIT32(23)
#define SPI_CLOCK_EQU_SYSCLK       BIT32(31)
#define SPI_USER_MISO              BIT32(28)
#define SPI_USER_MOSI              BIT32(27)
#define SPI_USER_CLK_EDGE          BIT32(9)
#define SPI_USER_FULL_DUPLEX       BIT32(0)
#define SPI_MISC_CLK_IDLE          BIT32(29)
#define SPI_MISC_ALL_CS_DISABLE    0x3fu
#define SPI_CLK_GATE_ENABLE        (BIT32(0) | BIT32(1) | BIT32(2))
#define SPI_APB_HZ                 80000000u
#define SPI_FIFO_SIZE              64u
#define SPI_CONTROLLER_COUNT       2

#endif // esp32s3_spi_regs.h
