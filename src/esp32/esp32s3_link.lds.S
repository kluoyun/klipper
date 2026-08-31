// ESP32-S3 ROM-loaded RAM image linker script

#include "autoconf.h"

OUTPUT_FORMAT("elf32-xtensa-le")
OUTPUT_ARCH(xtensa)
ENTRY(_start)

MEMORY
{
    // The first 32KiB is dedicated IRAM. The following 96KiB has a DRAM
    // alias below CONFIG_RAM_START, so the two load segments do not overlap.
    iram (rx)  : ORIGIN = 0x40370000, LENGTH = 0x20000
    dram (rw)  : ORIGIN = CONFIG_RAM_START, LENGTH = CONFIG_RAM_SIZE
}

PHDRS
{
    iram_segment PT_LOAD FLAGS(5);
    dram_segment PT_LOAD FLAGS(6);
    bss_segment PT_LOAD FLAGS(6);
    stack_segment PT_LOAD FLAGS(6);
}

SECTIONS
{
    _vectors = ORIGIN(iram);
    .vectors ORIGIN(iram) :
    {
        // ESP32-S3 Xtensa LX7 user/level-one exception vector offset.
        . = ORIGIN(iram) + 0x340;
        KEEP(*(.UserExceptionVector.text))
        . = ORIGIN(iram) + 0x400;
    } > iram :iram_segment

    .text :
    {
        *(.literal .literal.*)
        KEEP(*(.text.entry))
        KEEP(*(.text.exception))
        *(.text .text.*)
        . = ALIGN(4);
    } > iram :iram_segment

    .data ORIGIN(dram) :
    {
        . = ALIGN(4);
        _data_start = .;
        *(.rodata .rodata.*)
        *(.data .data.* .sdata .sdata.*)
        . = ALIGN(4);
        _data_end = .;
    } > dram :dram_segment

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _bss_start = .;
        *(.bss .bss.* .sbss .sbss.*)
        *(COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > dram :bss_segment

    _stack_start = ORIGIN(dram) + LENGTH(dram) - CONFIG_STACK_SIZE;
    .stack _stack_start (NOLOAD) :
    {
        . = . + CONFIG_STACK_SIZE;
        _stack_top = .;
    } > dram :stack_segment

    ASSERT(_bss_end <= _stack_start, "ESP32-S3 DRAM overflow")
    ASSERT(SIZEOF(.vectors) + SIZEOF(.text) <= LENGTH(iram),
           "ESP32-S3 IRAM overflow")

    /DISCARD/ :
    {
        *(.init)
        *(.fini)
        *(.eh_frame*)
        *(.note.GNU-stack)
        *(.gnu.lto_*)
    }
}
