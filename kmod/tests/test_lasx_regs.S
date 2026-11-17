/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* No libc/TLS: exercise all GPRs (including tp/t0/t1/u0) across first-use
 * LASX and LBT exceptions. Run with and without the module's comm filter. */
    .text
    .globl _start
_start:
    addi.d $r3, $r3, -272
    st.d $r3, $r3, 256
    .macro fill_regs
    .irp reg,1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    ori $r\reg, $r0, 0x100+\reg
    .endr
    .endm
    .macro check_regs
    .irp reg,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    st.d $r\reg, $r3, 8*\reg
    .endr
    ld.d $r4, $r3, 256
    bne $r3, $r4, fail
    ori $r6, $r0, 1
1:
    slli.d $r4, $r6, 3
    ldx.d $r4, $r3, $r4
    addi.d $r5, $r6, 0x100
    bne $r4, $r5, fail
    addi.d $r6, $r6, 1
    ori $r5, $r0, 3
    bne $r6, $r5, 2f
    addi.d $r6, $r6, 1 /* sp was checked separately */
2:
    ori $r5, $r0, 32
    bne $r6, $r5, 1b
    .endm
    fill_regs
    xvldi $xr0, 0
    check_regs
    fill_regs
    movgr2scr $scr0, $r0
    check_regs
    ori $r4, $r0, 0
    b exit
fail:
    ori $r4, $r0, 1
exit:
    ori $r11, $r0, 93 /* exit */
    syscall 0
    .section .note.GNU-stack,"",@progbits
