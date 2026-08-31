#ifndef __ESP32_INTERNAL_H
#define __ESP32_INTERNAL_H

#include <stdint.h>

void esp32_irq_setup(uint32_t source, uint32_t cpu_intr, uint32_t priority);
void esp32_irq_disable(uint32_t cpu_intr);
void esp32_irq_enable(uint32_t cpu_intr);
void esp32_irq_reset(void);

void esp32_gpio_peripheral(uint32_t pin, uint32_t signal, int output,
                           int pull_up);
void esp32_gpio_peripheral_bidir(uint32_t pin, uint32_t signal, int pull_up);
void esp32_gpio_analog(uint32_t pin);
void esp32_analog_i2c_write_mask(uint8_t block, uint8_t reg, uint8_t msb,
                                 uint8_t lsb, uint8_t data);
void esp32_timer_irq(void);
void esp32_serial_irq(void);
void esp32_usb_serial_jtag_irq(void);
void esp32_usb_irq(void);
void esp32_can_irq(void);
void watchdog_early_init(void);
#endif // internal.h
