// ESP32-C3 direct-boot entry point and RISC-V trap frame

    .section .text.entry, "ax", @progbits
    .global _entry
    .type _entry, @function
_entry:
    .option push
    .option norvc
    j _start
    .option pop
    .size _entry, .-_entry

    .section .text.start, "ax", @progbits
    .global _start
    .type _start, @function
_start:
    // Initialize the stack and global pointer.
    .option push
    .option norelax
    la gp, __global_pointer$
    la sp, _stack_top
    .option pop

    // Copy initialized data to SRAM.
    la t0, _data_load
    la t1, _data_start
    la t2, _data_end
1:
    bgeu t1, t2, 2f
    lw t3, 0(t0)
    sw t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    j 1b

    // Clear BSS.
2:
    la t1, _bss_start
    la t2, _bss_end
3:
    bgeu t1, t2, 4f
    sw zero, 0(t1)
    addi t1, t1, 4
    j 3b

4:
    csrci mstatus, 8
    // Select vectored interrupt mode.
    la t0, esp32_vector_table
    ori t0, t0, 1
    csrw mtvec, t0
    call esp32_main
5:
    wfi
    j 5b
    .size _start, .-_start

    // Route exceptions and interrupts to the common trap entry.
    .section .text.vectors, "ax", @progbits
    .balign 4
    .option push
    .option norvc
    .global esp32_vector_table
    .type esp32_vector_table, @function
esp32_vector_table:
    .rept 32
    j esp32_trap_entry
    .endr
    .size esp32_vector_table, .-esp32_vector_table
    .option pop

    // Save caller-saved registers and trap state.
    .section .text.trap, "ax", @progbits
    .balign 4
    .global esp32_trap_entry
    .type esp32_trap_entry, @function
esp32_trap_entry:
    addi sp, sp, -80
    sw ra,  0(sp)
    sw t0,  4(sp)
    sw t1,  8(sp)
    sw t2, 12(sp)
    sw t3, 16(sp)
    sw t4, 20(sp)
    sw t5, 24(sp)
    sw t6, 28(sp)
    sw a0, 32(sp)
    sw a1, 36(sp)
    sw a2, 40(sp)
    sw a3, 44(sp)
    sw a4, 48(sp)
    sw a5, 52(sp)
    sw a6, 56(sp)
    sw a7, 60(sp)
    csrr t0, mepc
    sw t0, 64(sp)
    csrr t0, mstatus
    sw t0, 68(sp)
    csrr a0, mcause
    call esp32_handle_trap
    lw t0, 64(sp)
    csrw mepc, t0
    lw t0, 68(sp)
    csrw mstatus, t0
    lw ra,  0(sp)
    lw t0,  4(sp)
    lw t1,  8(sp)
    lw t2, 12(sp)
    lw t3, 16(sp)
    lw t4, 20(sp)
    lw t5, 24(sp)
    lw t6, 28(sp)
    lw a0, 32(sp)
    lw a1, 36(sp)
    lw a2, 40(sp)
    lw a3, 44(sp)
    lw a4, 48(sp)
    lw a5, 52(sp)
    lw a6, 56(sp)
    lw a7, 60(sp)
    addi sp, sp, 80
    mret
    .size esp32_trap_entry, .-esp32_trap_entry

    .section .note.GNU-stack, "", @progbits
