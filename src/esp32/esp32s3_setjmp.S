// setjmp/longjmp for the ESP32-S3 Call0 ABI

    .section .text.setjmp, "ax", @progbits
    .global setjmp
    .type setjmp, @function
setjmp:
    // jmp_buf: a0, a1, a12, a13, a14, a15
    s32i    a0,  a2, 0
    s32i    a1,  a2, 4
    s32i    a12, a2, 8
    s32i    a13, a2, 12
    s32i    a14, a2, 16
    s32i    a15, a2, 20
    movi    a2, 0
    ret
    .size setjmp, .-setjmp

    .global longjmp
    .type longjmp, @function
longjmp:
    mov     a4, a2
    mov     a2, a3
    bnez    a2, 1f
    movi    a2, 1
1:
    l32i    a0,  a4, 0
    l32i    a1,  a4, 4
    l32i    a12, a4, 8
    l32i    a13, a4, 12
    l32i    a14, a4, 16
    l32i    a15, a4, 20
    ret
    .size longjmp, .-longjmp

    .section .note.GNU-stack, "", @progbits
