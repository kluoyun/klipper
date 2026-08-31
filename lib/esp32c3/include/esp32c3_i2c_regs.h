// ESP32-C3 I2C register definitions
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __ESP32C3_I2C_REGS_H
#define __ESP32C3_I2C_REGS_H

#define I2C_SCL_LOW_PERIOD         0x00u
#define I2C_CTR                    0x04u
#define I2C_TIMEOUT                0x0cu
#define I2C_FIFO_CONF              0x18u
#define I2C_DATA                   0x1cu
#define I2C_INT_RAW                0x20u
#define I2C_INT_CLR                0x24u
#define I2C_INT_ENA                0x28u
#define I2C_SDA_HOLD               0x30u
#define I2C_SDA_SAMPLE             0x34u
#define I2C_SCL_HIGH_PERIOD        0x38u
#define I2C_SCL_START_HOLD         0x40u
#define I2C_SCL_RSTART_SETUP       0x44u
#define I2C_SCL_STOP_HOLD          0x48u
#define I2C_SCL_STOP_SETUP         0x4cu
#define I2C_FILTER_CFG             0x50u
#define I2C_CLK_CONF               0x54u
#define I2C_COMMAND(index)         (0x58u + 4u * (index))
#define I2C_REG(config, offset)    REG32((config).base + (offset))

#define I2C_SDA_FORCE_OUT          BIT32(0)
#define I2C_SCL_FORCE_OUT          BIT32(1)
#define I2C_MASTER_MODE            BIT32(4)
#define I2C_TRANS_START            BIT32(5)
#define I2C_FSM_RESET              BIT32(10)
#define I2C_CONF_UPDATE            BIT32(11)
#define I2C_RX_FIFO_RESET          BIT32(12)
#define I2C_TX_FIFO_RESET          BIT32(13)
#define I2C_FIFO_POINTER_ENABLE    BIT32(14)
#define I2C_CLOCK_ACTIVE           BIT32(21)
#define I2C_TIMEOUT_ENABLE         BIT32(5)
#define I2C_INT_ARB_LOST           BIT32(5)
#define I2C_INT_END                BIT32(3)
#define I2C_INT_COMPLETE           BIT32(7)
#define I2C_INT_TIMEOUT            BIT32(8)
#define I2C_INT_NACK               BIT32(10)
#define I2C_INT_ALL                0x1ffffu

#define I2C_CMD_RESTART            6u
#define I2C_CMD_WRITE              1u
#define I2C_CMD_READ               3u
#define I2C_CMD_STOP               2u
#define I2C_CMD_END                4u
#define I2C_FIFO_SIZE              32u
#define I2C_XTAL_HZ                40000000u
#define I2C_CONTROLLER_COUNT       1

#endif // esp32c3_i2c_regs.h
