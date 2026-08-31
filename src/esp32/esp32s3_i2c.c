// ESP32 I2C master support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "board/misc.h"
#include "command.h"
#include "esp32_regs.h"
#include "gpio.h"
#include "i2ccmds.h"
#include "internal.h"
#include "sched.h"

// Bus names list SDA before SCL.
DECL_ENUMERATION("i2c_bus", "i2c0", 0);
DECL_ENUMERATION("i2c_bus", "i2c0_gpio8_gpio9", 0);
#if CONFIG_MACH_ESP32C3
DECL_ENUMERATION("i2c_bus", "i2c0_gpio4_gpio5", 1);
#else
DECL_ENUMERATION("i2c_bus", "i2c0_gpio1_gpio2", 1);
DECL_ENUMERATION("i2c_bus", "i2c1", 2);
DECL_ENUMERATION("i2c_bus", "i2c1_gpio10_gpio11", 2);
DECL_ENUMERATION("i2c_bus", "i2c1_gpio17_gpio18", 3);
#endif

struct i2c_bus_map {
    uint32_t base;
    uint32_t clock_bit;
    uint8_t controller;
    uint8_t sda_pin;
    uint8_t scl_pin;
    uint8_t sda_signal;
    uint8_t scl_signal;
};

static const struct i2c_bus_map i2c_buses[] = {
    { ESP_I2C0_BASE, SYSTEM_I2C0_BIT, 0, 8, 9,
      GPIO_MATRIX_I2C0_SDA, GPIO_MATRIX_I2C0_SCL },
#if CONFIG_MACH_ESP32C3
    { ESP_I2C0_BASE, SYSTEM_I2C0_BIT, 0, 4, 5,
      GPIO_MATRIX_I2C0_SDA, GPIO_MATRIX_I2C0_SCL },
#else
    { ESP_I2C0_BASE, SYSTEM_I2C0_BIT, 0, 1, 2,
      GPIO_MATRIX_I2C0_SDA, GPIO_MATRIX_I2C0_SCL },
    { ESP_I2C1_BASE, SYSTEM_I2C1_BIT, 1, 10, 11,
      GPIO_MATRIX_I2C1_SDA, GPIO_MATRIX_I2C1_SCL },
    { ESP_I2C1_BASE, SYSTEM_I2C1_BIT, 1, 17, 18,
      GPIO_MATRIX_I2C1_SDA, GPIO_MATRIX_I2C1_SCL },
#endif
};

static uint32_t i2c_current_rate[I2C_CONTROLLER_COUNT];
static uint8_t i2c_selected_route[I2C_CONTROLLER_COUNT];

static uint32_t
i2c_cmd(uint8_t opcode, uint8_t count, uint8_t ack_enable, uint8_t ack_value)
{
    return count | ((uint32_t)ack_enable << 8)
        | ((uint32_t)ack_value << 10) | ((uint32_t)opcode << 11);
}

static void
i2c_set_rate(struct i2c_config config)
{
    uint32_t rate = config.rate;
    if (!rate || rate > 1000000u)
        shutdown("Invalid ESP32 I2C rate");
    if (rate == i2c_current_rate[config.controller])
        return;

    uint32_t clock_div = I2C_XTAL_HZ / (rate * 1024u) + 1u;
    uint32_t source = I2C_XTAL_HZ / clock_div;
    uint32_t half = source / rate / 2u;
    if (half < 4u || half > 511u || clock_div > 256u)
        shutdown("Unsupported ESP32 I2C rate");
    uint32_t wait_high = rate >= 80000u ? half / 2u - 2u : half / 4u;
    uint32_t high = half - wait_high;
    uint32_t sample = half / 2u;
    uint32_t hold = half / 4u;
    uint32_t timeout = 32u - __builtin_clz(5u * half) + 2u;

    I2C_REG(config, I2C_CLK_CONF) =
        (clock_div - 1u) | I2C_CLOCK_ACTIVE;
    I2C_REG(config, I2C_SCL_LOW_PERIOD) = half - 1u;
    I2C_REG(config, I2C_SCL_HIGH_PERIOD) = high | (wait_high << 9);
    I2C_REG(config, I2C_SDA_HOLD) = hold - 1u;
    I2C_REG(config, I2C_SDA_SAMPLE) = sample - 1u;
    I2C_REG(config, I2C_SCL_RSTART_SETUP) = half - 1u;
    I2C_REG(config, I2C_SCL_STOP_SETUP) = half - 1u;
    I2C_REG(config, I2C_SCL_START_HOLD) = half - 1u;
    I2C_REG(config, I2C_SCL_STOP_HOLD) = half - 1u;
    I2C_REG(config, I2C_TIMEOUT) = timeout | I2C_TIMEOUT_ENABLE;
    I2C_REG(config, I2C_CTR) |= I2C_CONF_UPDATE;
    i2c_current_rate[config.controller] = rate;
}

static void
i2c_init(struct i2c_config config, uint32_t bus)
{
    const struct i2c_bus_map *map = &i2c_buses[bus];
    uint8_t selected = i2c_selected_route[config.controller];
    if (selected) {
        if (selected != bus + 1u)
            shutdown("ESP32 I2C controller pin conflict");
        return;
    }
    REG32(SYSTEM_PERIP_CLK_EN0) |= map->clock_bit;
    REG32(SYSTEM_PERIP_RST_EN0) |= map->clock_bit;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~map->clock_bit;

    esp32_gpio_peripheral_bidir(map->scl_pin, map->scl_signal, 1);
    esp32_gpio_peripheral_bidir(map->sda_pin, map->sda_signal, 1);
    I2C_REG(config, I2C_CTR) = I2C_SDA_FORCE_OUT | I2C_SCL_FORCE_OUT
        | I2C_MASTER_MODE;
    I2C_REG(config, I2C_FIFO_CONF) = I2C_FIFO_POINTER_ENABLE;
    I2C_REG(config, I2C_FILTER_CFG) = BIT32(8) | BIT32(9) | 7u | (7u << 4);
    I2C_REG(config, I2C_INT_ENA) = 0;
    I2C_REG(config, I2C_INT_CLR) = I2C_INT_ALL;
    i2c_selected_route[config.controller] = bus + 1u;
}

struct i2c_config
i2c_setup(uint32_t bus, uint32_t rate, uint8_t addr)
{
    if (bus >= ARRAY_SIZE(i2c_buses) || addr > 0x7f)
        shutdown("Invalid ESP32 I2C configuration");
    const struct i2c_bus_map *map = &i2c_buses[bus];
    struct i2c_config config = {
        .base = map->base,
        .rate = rate,
        .controller = map->controller,
        .addr = addr,
    };
    i2c_init(config, bus);
    i2c_set_rate(config);
    return config;
}

static void
i2c_segment_begin(struct i2c_config config)
{
    i2c_set_rate(config);
    I2C_REG(config, I2C_FIFO_CONF) |=
        I2C_RX_FIFO_RESET | I2C_TX_FIFO_RESET;
    I2C_REG(config, I2C_FIFO_CONF) &=
        ~(I2C_RX_FIFO_RESET | I2C_TX_FIFO_RESET);
    I2C_REG(config, I2C_INT_CLR) = I2C_INT_ALL;
    for (uint8_t i = 0; i < 8; i++)
        I2C_REG(config, I2C_COMMAND(i)) =
            i2c_cmd(I2C_CMD_END, 0, 0, 0);
}

static int
i2c_wait_done(struct i2c_config config, uint32_t expected)
{
    I2C_REG(config, I2C_CTR) |= I2C_CONF_UPDATE;
    I2C_REG(config, I2C_CTR) |= I2C_TRANS_START;
    uint32_t end = timer_read_time() + timer_from_us(10000);
    for (;;) {
        uint32_t status = I2C_REG(config, I2C_INT_RAW);
        if (status & I2C_INT_NACK) {
            I2C_REG(config, I2C_CTR) |=
                I2C_FSM_RESET | I2C_CONF_UPDATE;
            return I2C_BUS_NACK;
        }
        if (status & (I2C_INT_TIMEOUT | I2C_INT_ARB_LOST)) {
            I2C_REG(config, I2C_CTR) |=
                I2C_FSM_RESET | I2C_CONF_UPDATE;
            return I2C_BUS_TIMEOUT;
        }
        if (status & expected)
            return I2C_BUS_SUCCESS;
        if (timer_is_before(end, timer_read_time())) {
            I2C_REG(config, I2C_CTR) |=
                I2C_FSM_RESET | I2C_CONF_UPDATE;
            return I2C_BUS_TIMEOUT;
        }
    }
}

int
i2c_write(struct i2c_config config, uint8_t write_len, uint8_t *write)
{
    uint16_t offset = 0;
    uint8_t first = 1;
    do {
        i2c_segment_begin(config);
        uint8_t capacity = first ? I2C_FIFO_SIZE - 1u : I2C_FIFO_SIZE;
        uint8_t count = write_len - offset;
        if (count > capacity)
            count = capacity;
        if (first)
            I2C_REG(config, I2C_DATA) = config.addr << 1;
        for (uint8_t i = 0; i < count; i++)
            I2C_REG(config, I2C_DATA) = write[offset + i];

        uint8_t command = 0;
        if (first)
            I2C_REG(config, I2C_COMMAND(command++)) =
                i2c_cmd(I2C_CMD_RESTART, 0, 0, 0);
        I2C_REG(config, I2C_COMMAND(command++)) =
            i2c_cmd(I2C_CMD_WRITE, count + first, 1, 0);
        offset += count;
        uint8_t last = offset == write_len;
        I2C_REG(config, I2C_COMMAND(command)) = i2c_cmd(
            last ? I2C_CMD_STOP : I2C_CMD_END, 0, 0, 0);
        int ret = i2c_wait_done(
            config, last ? I2C_INT_COMPLETE : I2C_INT_END);
        if (ret)
            return ret;
        first = 0;
    } while (offset < write_len);
    return I2C_BUS_SUCCESS;
}

int
i2c_read(struct i2c_config config, uint8_t reg_len, uint8_t *reg,
         uint8_t read_len, uint8_t *read)
{
    if (!read_len)
        return i2c_write(config, reg_len, reg);

    // Send the optional register prefix without releasing the bus.
    uint16_t reg_offset = 0;
    uint8_t first_write = 1;
    while (reg_offset < reg_len) {
        i2c_segment_begin(config);
        uint8_t capacity = first_write ? I2C_FIFO_SIZE - 1u : I2C_FIFO_SIZE;
        uint8_t count = reg_len - reg_offset;
        if (count > capacity)
            count = capacity;
        if (first_write)
            I2C_REG(config, I2C_DATA) = config.addr << 1;
        for (uint8_t i = 0; i < count; i++)
            I2C_REG(config, I2C_DATA) = reg[reg_offset + i];
        uint8_t command = 0;
        if (first_write)
            I2C_REG(config, I2C_COMMAND(command++)) =
                i2c_cmd(I2C_CMD_RESTART, 0, 0, 0);
        I2C_REG(config, I2C_COMMAND(command++)) =
            i2c_cmd(I2C_CMD_WRITE, count + first_write, 1, 0);
        I2C_REG(config, I2C_COMMAND(command)) =
            i2c_cmd(I2C_CMD_END, 0, 0, 0);
        int ret = i2c_wait_done(config, I2C_INT_END);
        if (ret)
            return ret;
        reg_offset += count;
        first_write = 0;
    }

    uint16_t read_offset = 0;
    uint8_t first_read = 1;
    while (read_offset < read_len) {
        i2c_segment_begin(config);
        uint8_t count = read_len - read_offset;
        if (count > I2C_FIFO_SIZE)
            count = I2C_FIFO_SIZE;
        uint8_t command = 0;
        if (first_read) {
            I2C_REG(config, I2C_DATA) = (config.addr << 1) | 1u;
            I2C_REG(config, I2C_COMMAND(command++)) =
                i2c_cmd(I2C_CMD_RESTART, 0, 0, 0);
            I2C_REG(config, I2C_COMMAND(command++)) =
                i2c_cmd(I2C_CMD_WRITE, 1, 1, 0);
        }
        uint8_t last = read_offset + count == read_len;
        if (!last) {
            I2C_REG(config, I2C_COMMAND(command++)) =
                i2c_cmd(I2C_CMD_READ, count, 0, 0);
            I2C_REG(config, I2C_COMMAND(command)) =
                i2c_cmd(I2C_CMD_END, 0, 0, 0);
        } else {
            if (count > 1)
                I2C_REG(config, I2C_COMMAND(command++)) =
                    i2c_cmd(I2C_CMD_READ, count - 1u, 0, 0);
            I2C_REG(config, I2C_COMMAND(command++)) =
                i2c_cmd(I2C_CMD_READ, 1, 0, 1);
            I2C_REG(config, I2C_COMMAND(command)) =
                i2c_cmd(I2C_CMD_STOP, 0, 0, 0);
        }
        int ret = i2c_wait_done(
            config, last ? I2C_INT_COMPLETE : I2C_INT_END);
        if (ret)
            return ret;
        for (uint8_t i = 0; i < count; i++)
            read[read_offset + i] = I2C_REG(config, I2C_DATA);
        read_offset += count;
        first_read = 0;
    }
    return I2C_BUS_SUCCESS;
}
