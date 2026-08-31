// ESP32 general-purpose SPI master support
//
// Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "board/misc.h"
#include "command.h"
#include "esp32_regs.h"
#include "gpio.h"
#include "internal.h"
#include "sched.h"

// Bus names list MOSI, MISO, then SCLK.
DECL_ENUMERATION("spi_bus", "spi2", 0);
#if CONFIG_MACH_ESP32C3
DECL_ENUMERATION("spi_bus", "spi2_gpio7_gpio2_gpio6", 0);
DECL_ENUMERATION("spi_bus", "spi2_gpio4_gpio5_gpio3", 1);
#else
DECL_ENUMERATION("spi_bus", "spi2_gpio11_gpio13_gpio12", 0);
DECL_ENUMERATION("spi_bus", "spi2_gpio4_gpio6_gpio5", 1);
DECL_ENUMERATION("spi_bus", "spi3", 2);
DECL_ENUMERATION("spi_bus", "spi3_gpio15_gpio17_gpio16", 2);
DECL_ENUMERATION("spi_bus", "spi3_gpio39_gpio41_gpio40", 3);
#endif

struct spi_bus_map {
    uint32_t base;
    uint32_t clock_bit;
    uint8_t controller;
    uint8_t mosi_pin;
    uint8_t miso_pin;
    uint8_t sclk_pin;
    uint8_t mosi_signal;
    uint8_t miso_signal;
    uint8_t sclk_signal;
};

static const struct spi_bus_map spi_buses[] = {
#if CONFIG_MACH_ESP32C3
    { ESP_SPI2_BASE, SYSTEM_SPI2_BIT, 0, 7, 2, 6,
      GPIO_MATRIX_SPI2_MOSI, GPIO_MATRIX_SPI2_MISO,
      GPIO_MATRIX_SPI2_CLK },
    { ESP_SPI2_BASE, SYSTEM_SPI2_BIT, 0, 4, 5, 3,
      GPIO_MATRIX_SPI2_MOSI, GPIO_MATRIX_SPI2_MISO,
      GPIO_MATRIX_SPI2_CLK },
#else
    { ESP_SPI2_BASE, SYSTEM_SPI2_BIT, 0, 11, 13, 12,
      GPIO_MATRIX_SPI2_MOSI, GPIO_MATRIX_SPI2_MISO,
      GPIO_MATRIX_SPI2_CLK },
    { ESP_SPI2_BASE, SYSTEM_SPI2_BIT, 0, 4, 6, 5,
      GPIO_MATRIX_SPI2_MOSI, GPIO_MATRIX_SPI2_MISO,
      GPIO_MATRIX_SPI2_CLK },
    { ESP_SPI3_BASE, SYSTEM_SPI3_BIT, 1, 15, 17, 16,
      GPIO_MATRIX_SPI3_MOSI, GPIO_MATRIX_SPI3_MISO,
      GPIO_MATRIX_SPI3_CLK },
    { ESP_SPI3_BASE, SYSTEM_SPI3_BIT, 1, 39, 41, 40,
      GPIO_MATRIX_SPI3_MOSI, GPIO_MATRIX_SPI3_MISO,
      GPIO_MATRIX_SPI3_CLK },
#endif
};

static uint32_t spi_current_clock[SPI_CONTROLLER_COUNT];
static uint8_t spi_current_mode[SPI_CONTROLLER_COUNT] = {
#if CONFIG_MACH_ESP32C3
    0xffu
#else
    0xffu, 0xffu
#endif
};
static uint8_t spi_selected_route[SPI_CONTROLLER_COUNT];

static void
spi_wait(struct spi_config config, uint32_t mask)
{
    uint32_t end = timer_read_time() + timer_from_us(5000);
    while (SPI_REG(config, SPI_CMD) & mask) {
        if (timer_is_before(end, timer_read_time()))
            shutdown("ESP32 SPI timeout");
    }
}

static uint32_t
spi_calculate_clock(uint32_t rate)
{
    if (!rate || rate > SPI_APB_HZ)
        shutdown("Invalid ESP32 SPI rate");
    if (rate > SPI_APB_HZ * 3u / 4u)
        return SPI_CLOCK_EQU_SYSCLK;

    uint32_t best_error = 0xffffffffu;
    uint8_t best_n = 2, best_pre = 1;
    for (uint8_t n = 2; n <= 64; n++) {
        uint32_t pre = (SPI_APB_HZ / n + rate / 2u) / rate;
        if (pre < 1u)
            pre = 1u;
        if (pre > 16u)
            pre = 16u;
        uint32_t actual = SPI_APB_HZ / pre / n;
        uint32_t error = actual > rate ? actual - rate : rate - actual;
        if (error <= best_error) {
            best_error = error;
            best_n = n;
            best_pre = pre;
        }
    }
    uint32_t high = (best_n + 1u) / 2u;
    return ((uint32_t)(best_pre - 1u) << 18)
        | ((uint32_t)(best_n - 1u) << 12)
        | ((high - 1u) << 6) | (best_n - 1u);
}

static void
spi_init(struct spi_config config, uint32_t bus)
{
    const struct spi_bus_map *map = &spi_buses[bus];
    uint8_t selected = spi_selected_route[config.controller];
    if (selected) {
        if (selected != bus + 1u)
            shutdown("ESP32 SPI controller pin conflict");
        return;
    }
    REG32(SYSTEM_PERIP_CLK_EN0) |= map->clock_bit;
    REG32(SYSTEM_PERIP_RST_EN0) |= map->clock_bit;
    REG32(SYSTEM_PERIP_RST_EN0) &= ~map->clock_bit;

    esp32_gpio_peripheral(map->sclk_pin, map->sclk_signal, 1, 0);
    esp32_gpio_peripheral(map->mosi_pin, map->mosi_signal, 1, 0);
    esp32_gpio_peripheral(map->miso_pin, map->miso_signal, 0, 0);

    SPI_REG(config, SPI_SLAVE) = 0;
    SPI_REG(config, SPI_CTRL) = 0;
    SPI_REG(config, SPI_USER1) = 0;
    SPI_REG(config, SPI_DMA_CONF) = 0;
    SPI_REG(config, SPI_MISC) = SPI_MISC_ALL_CS_DISABLE;
    SPI_REG(config, SPI_USER) = SPI_USER_MISO | SPI_USER_MOSI
        | SPI_USER_FULL_DUPLEX;
    SPI_REG(config, SPI_CLK_GATE) = SPI_CLK_GATE_ENABLE;
    spi_selected_route[config.controller] = bus + 1u;
}

struct spi_config
spi_setup(uint32_t bus, uint8_t mode, uint32_t rate)
{
    if (bus >= ARRAY_SIZE(spi_buses) || mode > 3)
        shutdown("Invalid ESP32 SPI configuration");
    const struct spi_bus_map *map = &spi_buses[bus];
    struct spi_config config = {
        .base = map->base,
        .clock = spi_calculate_clock(rate),
        .controller = map->controller,
        .mode = mode,
    };
    spi_init(config, bus);
    return config;
}

void
spi_prepare(struct spi_config config)
{
    uint8_t controller = config.controller;
    if (spi_current_clock[controller] == config.clock
        && spi_current_mode[controller] == config.mode)
        return;
    spi_wait(config, SPI_CMD_USER | SPI_CMD_UPDATE);
    SPI_REG(config, SPI_CLOCK) = config.clock;
    uint32_t user = SPI_USER_MISO | SPI_USER_MOSI | SPI_USER_FULL_DUPLEX;
    uint32_t misc = SPI_MISC_ALL_CS_DISABLE;
    if (config.mode == 1 || config.mode == 2)
        user |= SPI_USER_CLK_EDGE;
    if (config.mode >= 2)
        misc |= SPI_MISC_CLK_IDLE;
    SPI_REG(config, SPI_USER) = user;
    SPI_REG(config, SPI_MISC) = misc;
    SPI_REG(config, SPI_CMD) = SPI_CMD_UPDATE;
    spi_wait(config, SPI_CMD_UPDATE);
    spi_current_clock[controller] = config.clock;
    spi_current_mode[controller] = config.mode;
}

void
spi_transfer(struct spi_config config, uint8_t receive_data,
             uint8_t len, uint8_t *data)
{
    spi_prepare(config);
    while (len) {
        uint8_t count = len > SPI_FIFO_SIZE ? SPI_FIFO_SIZE : len;
        for (uint8_t word_index = 0; word_index < (count + 3u) / 4u;
             word_index++) {
            uint32_t word = 0;
            uint8_t offset = word_index * 4u;
            uint8_t word_len = count - offset;
            if (word_len > 4u)
                word_len = 4u;
            for (uint8_t i = 0; i < word_len; i++)
                word |= (uint32_t)data[offset + i] << (i * 8u);
            SPI_REG(config, SPI_DATA(word_index)) = word;
        }
        SPI_REG(config, SPI_MS_DLEN) = (uint32_t)count * 8u - 1u;
        SPI_REG(config, SPI_CMD) = SPI_CMD_UPDATE;
        spi_wait(config, SPI_CMD_UPDATE);
        SPI_REG(config, SPI_CMD) = SPI_CMD_USER;
        spi_wait(config, SPI_CMD_USER);
        if (receive_data) {
            for (uint8_t word_index = 0; word_index < (count + 3u) / 4u;
                 word_index++) {
                uint32_t word = SPI_REG(config, SPI_DATA(word_index));
                uint8_t offset = word_index * 4u;
                uint8_t word_len = count - offset;
                if (word_len > 4u)
                    word_len = 4u;
                for (uint8_t i = 0; i < word_len; i++)
                    data[offset + i] = word >> (i * 8u);
            }
        }
        data += count;
        len -= count;
    }
}
