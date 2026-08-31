# Additional build rules for Espressif ESP32 chips

CROSS_PREFIX-$(CONFIG_MACH_ESP32C3) = riscv32-esp-elf-
CROSS_PREFIX-$(CONFIG_MACH_ESP32S3) = xtensa-esp32s3-elf-
CROSS_PREFIX = $(CROSS_PREFIX-y)

dirs-y += src/esp32 src/generic lib/fast-hash

CFLAGS-$(CONFIG_MACH_ESP32C3) += -march=rv32imc_zicsr -mabi=ilp32
CFLAGS-$(CONFIG_MACH_ESP32C3) += -mcmodel=medany -msmall-data-limit=0
CFLAGS-$(CONFIG_MACH_ESP32C3) += -Ilib/esp32c3/include
CFLAGS-$(CONFIG_MACH_ESP32S3) += -mabi=call0 -mtext-section-literals
CFLAGS-$(CONFIG_MACH_ESP32S3) += -mlongcalls -ffreestanding -fno-builtin
CFLAGS-$(CONFIG_MACH_ESP32S3) += -Ilib/esp32s3/include
CFLAGS += $(CFLAGS-y) -Ilib/fast-hash

src-y += esp32/gpio.c esp32/chipid.c esp32/reset.c esp32/watchdog.c
src-$(CONFIG_MACH_ESP32C3) += esp32/esp32c3.c esp32/esp32c3_irq.c
src-$(CONFIG_MACH_ESP32C3) += esp32/esp32c3_timer.c esp32/esp32c3_adc.c
src-$(CONFIG_MACH_ESP32S3) += esp32/esp32s3.c esp32/esp32s3_irq.c
src-$(CONFIG_MACH_ESP32S3) += esp32/esp32s3_timer.c
src-$(CONFIG_MACH_ESP32S3) += esp32/esp32s3_runtime.c
src-$(CONFIG_MACH_ESP32S3) += esp32/esp32s3_adc.c
src-y += esp32/esp32s3_pwm.c esp32/esp32s3_i2c.c esp32/esp32s3_spi.c
src-y += generic/timer_irq.c generic/crc16_ccitt.c generic/alloc.c
src-$(CONFIG_SERIAL) += generic/serial_irq.c
src-$(CONFIG_ESP32_SERIAL_UART0) += esp32/serial.c
src-$(CONFIG_ESP32_SERIAL_UART1) += esp32/serial.c
src-$(CONFIG_ESP32_SERIAL_UART2) += esp32/serial.c
src-$(CONFIG_ESP32_USB_SERIAL_JTAG) += esp32/usb_serial_jtag.c
src-$(CONFIG_USB) += esp32/usb_dwc2.c
src-$(CONFIG_USBSERIAL) += generic/usb_cdc.c
canbus-src-y := esp32/esp32s3_can.c generic/canserial.c
canbus-src-y += ../lib/fast-hash/fasthash.c
src-$(CONFIG_CANSERIAL) += $(canbus-src-y) generic/canbus.c
src-$(CONFIG_USBCANBUS) += $(canbus-src-y) generic/usb_canbus.c

$(OUT)src/esp32/esp32c3_start.o: src/esp32/esp32c3_start.S $(OUT)autoconf.h
	@echo "  Compiling $@"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(OUT)src/esp32/esp32s3_start.o: src/esp32/esp32s3_start.S $(OUT)autoconf.h
	@echo "  Compiling $@"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(OUT)src/esp32/esp32s3_setjmp.o: src/esp32/esp32s3_setjmp.S $(OUT)autoconf.h
	@echo "  Compiling $@"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

START_OBJ-$(CONFIG_MACH_ESP32C3) = $(OUT)src/esp32/esp32c3_start.o
START_OBJ-$(CONFIG_MACH_ESP32S3) = $(OUT)src/esp32/esp32s3_start.o
START_OBJ = $(START_OBJ-y)
RUNTIME_OBJ-$(CONFIG_MACH_ESP32S3) = $(OUT)src/esp32/esp32s3_setjmp.o
RUNTIME_OBJ = $(RUNTIME_OBJ-y)
LINKER_SRC-$(CONFIG_MACH_ESP32C3) = esp32c3_link
LINKER_SRC-$(CONFIG_MACH_ESP32S3) = esp32s3_link
LINKER_SRC = $(LINKER_SRC-y)

ifeq ($(CONFIG_MACH_ESP32C3),y)
$(OUT)klipper.bin: $(OUT)klipper.elf
	@echo "  Creating $@"
	$(Q)$(OBJCOPY) -O binary $< $@
endif

ifeq ($(CONFIG_MACH_ESP32S3),y)
$(OUT)klipper.iram.bin: $(OUT)klipper.elf
	@echo "  Extracting $@"
	$(Q)$(OBJCOPY) -O binary -j .vectors -j .text $< $@

$(OUT)klipper.dram.bin: $(OUT)klipper.elf
	@echo "  Extracting $@"
	$(Q)$(OBJCOPY) -O binary -j .data $< $@

$(OUT)klipper.bin: $(OUT)klipper.elf $(OUT)klipper.iram.bin \
	$(OUT)klipper.dram.bin scripts/build-esp32s3-image.py
	@echo "  Creating $@"
	$(Q)$(PYTHON) scripts/build-esp32s3-image.py --elf $(OUT)klipper.elf \
		--iram $(OUT)klipper.iram.bin --dram $(OUT)klipper.dram.bin \
		--flash-size-code $(CONFIG_ESP32_FLASH_SIZE_CODE) --output $@
endif

target-y += $(OUT)klipper.bin
CFLAGS_klipper.elf += -nostartfiles -nostdlib -Wl,-z,noexecstack
LIBC-$(CONFIG_MACH_ESP32C3) = -lc_nano
LIBGCC-$(CONFIG_MACH_ESP32C3) = -lgcc
CFLAGS_klipper.elf += -Wl,--start-group $(LIBC-y) $(LIBGCC-y) -Wl,--end-group
CFLAGS_klipper.elf += -T $(OUT)src/esp32/$(LINKER_SRC).ld
OBJS_klipper.elf += $(START_OBJ) $(RUNTIME_OBJ)
$(OUT)klipper.elf: $(START_OBJ) $(RUNTIME_OBJ) \
	$(OUT)src/esp32/$(LINKER_SRC).ld

flash: $(OUT)klipper.bin
	@echo "  Flashing $< to $(FLASH_DEVICE)"
	$(Q)$(PYTHON) -m esptool --chip $(CONFIG_MCU) -p "$(FLASH_DEVICE)" \
		write-flash 0x0 $<
