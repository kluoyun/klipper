// ESP32-S3 ROM-image entry point and Xtensa level-one exception frame

    .section .UserExceptionVector.text, "ax", @progbits
    .global esp32s3_user_vector
    .type esp32s3_user_vector, @function
esp32s3_user_vector:
    wsr     a0, excsave1
    j       esp32s3_exception_entry
    .size esp32s3_user_vector, .-esp32s3_user_vector

    .section .text.entry, "ax", @progbits
    .global _start
    .type _start, @function
_start:
    // The ROM has loaded the IRAM and DRAM sections.
    movi    a1, _stack_top
    movi    a2, _vectors
    wsr     a2, vecbase
    movi    a2, 0
    wsr     a2, intenable
    // Enter the Call0 ABI with interrupts disabled.
    movi    a2, 0x2f
    wsr     a2, ps
    rsync

    // Clear BSS.
    movi    a2, _bss_start
    movi    a3, _bss_end
    movi    a4, 0
1:
    bgeu    a2, a3, 2f
    s32i    a4, a2, 0
    addi    a2, a2, 4
    j       1b
2:
    call0   esp32_main
3:
    waiti   15
    j       3b
    .size _start, .-_start

    // Save the Call0 register set and exception state.
    .section .text.exception, "ax", @progbits
    .balign 4
    .global esp32s3_exception_entry
    .type esp32s3_exception_entry, @function
esp32s3_exception_entry:
    addi    a1, a1, -96
    s32i    a2,  a1, 8
    s32i    a3,  a1, 12
    s32i    a4,  a1, 16
    s32i    a5,  a1, 20
    s32i    a6,  a1, 24
    s32i    a7,  a1, 28
    s32i    a8,  a1, 32
    s32i    a9,  a1, 36
    s32i    a10, a1, 40
    s32i    a11, a1, 44
    s32i    a12, a1, 48
    s32i    a13, a1, 52
    s32i    a14, a1, 56
    s32i    a15, a1, 60
    rsr     a2, excsave1
    s32i    a2, a1, 0
    addi    a2, a1, 96
    s32i    a2, a1, 4
    rsr     a2, ps
    s32i    a2, a1, 64
    rsr     a2, epc1
    s32i    a2, a1, 68
    rsr     a2, sar
    s32i    a2, a1, 72
    rsr     a2, lbeg
    s32i    a2, a1, 76
    rsr     a2, lend
    s32i    a2, a1, 80
    // Disable an interrupted zero-overhead loop while C runs.
    movi    a2, 0
    xsr     a2, lcount
    s32i    a2, a1, 84
    rsr     a2, exccause
    bnei    a2, 4, 4f

    // Clear EXCM and mask level-one interrupts while entering C.
    movi    a2, 0x21
    wsr     a2, ps
    rsync
    call0   esp32s3_handle_irq
    rsil    a2, 15

    l32i    a2, a1, 72
    wsr     a2, sar
    l32i    a2, a1, 76
    wsr     a2, lbeg
    l32i    a2, a1, 80
    wsr     a2, lend
    l32i    a2, a1, 84
    wsr     a2, lcount
    l32i    a2, a1, 64
    wsr     a2, ps
    l32i    a2, a1, 68
    wsr     a2, epc1
    l32i    a2,  a1, 8
    l32i    a3,  a1, 12
    l32i    a4,  a1, 16
    l32i    a5,  a1, 20
    l32i    a6,  a1, 24
    l32i    a7,  a1, 28
    l32i    a8,  a1, 32
    l32i    a9,  a1, 36
    l32i    a10, a1, 40
    l32i    a11, a1, 44
    l32i    a12, a1, 48
    l32i    a13, a1, 52
    l32i    a14, a1, 56
    l32i    a15, a1, 60
    l32i    a0, a1, 0
    l32i    a1, a1, 4
    rsync
    rfe
4:
    rsil    a2, 15
5:
    j       5b
    .size esp32s3_exception_entry, .-esp32s3_exception_entry

    .section .note.GNU-stack, "", @progbits
