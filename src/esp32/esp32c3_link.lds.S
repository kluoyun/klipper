// ESP32-C3 direct-boot linker script

#include "autoconf.h"

OUTPUT_FORMAT("elf32-littleriscv")
OUTPUT_ARCH(riscv)
ENTRY(_entry)

MEMORY
{
    irom (rx)  : ORIGIN = 0x42000000, LENGTH = CONFIG_FLASH_SIZE
    drom (r)   : ORIGIN = 0x3c000000, LENGTH = CONFIG_FLASH_SIZE
    ram  (rwx) : ORIGIN = CONFIG_RAM_START, LENGTH = CONFIG_RAM_SIZE
}

PHDRS
{
    irom_segment PT_LOAD FLAGS(5);
    drom_segment PT_LOAD FLAGS(4);
    data_segment PT_LOAD FLAGS(6);
    bss_segment PT_LOAD FLAGS(6);
    stack_segment PT_LOAD FLAGS(6);
}

SECTIONS
{
    // The ESP32-C3 rev.3+ ROM recognizes this pair of words and maps the
    // complete flash at IROM/DROM before jumping to flash offset eight.
    .header : AT(0)
    {
        _irom_start = .;
        LONG(0xaedb041d)
        LONG(0xaedb041d)
    } > irom :irom_segment

    .text.entry ORIGIN(irom) + 8 :
    {
        KEEP(*(.text.entry))
    } > irom :irom_segment

    .text :
    {
        KEEP(*(.text.start))
        . = ALIGN(0x100);
        KEEP(*(.text.vectors))
        KEEP(*(.text.trap))
        *(.text .text.*)
    } > irom :irom_segment
    . = ALIGN(4);
    _irom_size = . - _irom_start;

    _drom_start = ORIGIN(drom) + _irom_size;
    .rodata _drom_start : AT(_irom_size)
    {
        *(.rodata .rodata.* .srodata .srodata.*)
        . = ALIGN(4);
    } > drom :drom_segment
    _drom_size = . - _drom_start;

    _data_lma = _irom_size + _drom_size;
    .data ORIGIN(ram) : AT(_data_lma)
    {
        . = ALIGN(4);
        _data_start = .;
        *(.data .data.* .sdata .sdata.*)
        . = ALIGN(4);
        _data_end = .;
    } > ram :data_segment
    _data_load = ORIGIN(drom) + LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > ram :bss_segment

    __global_pointer$ = _data_start + 0x800;
    _stack_start = ORIGIN(ram) + LENGTH(ram) - CONFIG_STACK_SIZE;
    .stack _stack_start (NOLOAD) :
    {
        . = . + CONFIG_STACK_SIZE;
        _stack_top = .;
    } > ram :stack_segment

    ASSERT(_bss_end <= _stack_start, "ESP32-C3 RAM overflow")
    ASSERT((esp32_vector_table & 0xff) == 0,
           "ESP32-C3 interrupt vector table must be 256-byte aligned")
    ASSERT(_data_lma + SIZEOF(.data) <= CONFIG_FLASH_SIZE,
           "ESP32-C3 direct-boot image exceeds flash mapping")

    /DISCARD/ :
    {
        *(.init)
        *(.fini)
        *(.eh_frame*)
        *(.note.GNU-stack)
        *(.gnu.lto_*)
    }
}
