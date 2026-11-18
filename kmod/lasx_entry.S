/* SPDX-License-Identifier: GPL-2.0-only */
/* Hardware vector prefix has already saved t0/t1 in KS0/KS1. IRQs stay off
 * until pt_regs, kernel tp and the per-CPU register are ready for C code.
 * Use the installed kernel's generated offsets (including vendor 4.19 and 5.4). */
#include "lasx_entry_compat.h"
#include <generated/asm-offsets.h>
#if LASX_USE_IRQENTRY && __has_include(<asm/unwind_hints.h>)
#include <asm/unwind_hints.h>
#else
    .macro UNWIND_HINT_UNDEFINED
    .endm
    .macro UNWIND_HINT_REGS
    .endm
    .macro UNWIND_HINT_FUNC
    .endm
#endif

#if LASX_USE_IRQENTRY
    .section .noinstr.text,"ax"
#else
    .text
#endif
    .align 4
    .macro LASX_ENTRY name, handler
    .globl \name
    .type \name, @function
\name:
    UNWIND_HINT_UNDEFINED
    csrrd $r13, 0x1 /* PRMD */
    andi $r13, $r13, 3
    move $r12, $r3
    beqz $r13, 1f
    la.pcrel $r13, lasx_kernelsp
    ld.d $r13, $r13, 0
#ifdef CONFIG_SMP
    csrrd $r12, 0x33 /* PERCPU_BASE_KS */
    add.d $r13, $r13, $r12
#endif
    move $r12, $r3
    ld.d $r3, $r13, 0
1:
    addi.d $r3, $r3, -PT_SIZE
    st.d $r12, $r3, PT_R3
    st.d $r0, $r3, PT_R0
    st.d $r1, $r3, PT_R1
    st.d $r2, $r3, PT_R2
    st.d $r4, $r3, PT_R4
    st.d $r5, $r3, PT_R5
    st.d $r6, $r3, PT_R6
    st.d $r7, $r3, PT_R7
    st.d $r8, $r3, PT_R8
    st.d $r9, $r3, PT_R9
    st.d $r10, $r3, PT_R10
    st.d $r11, $r3, PT_R11
    st.d $r14, $r3, PT_R14
    st.d $r15, $r3, PT_R15
    st.d $r16, $r3, PT_R16
    st.d $r17, $r3, PT_R17
    st.d $r18, $r3, PT_R18
    st.d $r19, $r3, PT_R19
    st.d $r20, $r3, PT_R20
    st.d $r21, $r3, PT_R21
    st.d $r22, $r3, PT_R22
    st.d $r23, $r3, PT_R23
    st.d $r24, $r3, PT_R24
    st.d $r25, $r3, PT_R25
    st.d $r26, $r3, PT_R26
    st.d $r27, $r3, PT_R27
    st.d $r28, $r3, PT_R28
    st.d $r29, $r3, PT_R29
    st.d $r30, $r3, PT_R30
    st.d $r31, $r3, PT_R31
    csrrd $r12, 0x1
    st.d $r12, $r3, PT_PRMD
    csrrd $r12, 0x0
    st.d $r12, $r3, PT_CRMD
    csrrd $r12, 0x2
    st.d $r12, $r3, PT_EUEN
    csrrd $r12, 0x4
    st.d $r12, $r3, PT_ECFG
    csrrd $r12, 0x5
    st.d $r12, $r3, PT_ESTAT
    csrrd $r12, 0x6
    st.d $r12, $r3, PT_ERA
    csrrd $r12, 0x7
    st.d $r12, $r3, PT_BVADDR
    csrrd $r12, 0x30
    st.d $r12, $r3, PT_R12
    csrrd $r13, 0x31
    st.d $r13, $r3, PT_R13
    ld.d $r12, $r3, PT_PRMD
    andi $r12, $r12, 3
    beqz $r12, 2f
    li.d $r2, ~_THREAD_MASK
    and $r2, $r2, $r3
    csrrd $r21, 0x33
2:
#ifdef CONFIG_KGDB
    li.w $r12, 0x200 /* CRMD.WE */
    csrxchg $r12, $r12, 0
#endif
    UNWIND_HINT_REGS
    move $r4, $r3
    bl \handler
    b lasx_entry_return
    .size \name, .-\name
    .endm

    LASX_ENTRY lasx_entry_ri, my_do_ri
    LASX_ENTRY lasx_entry_lbt, my_do_lbt
    LASX_ENTRY lasx_entry_lasx, my_do_lasx

/* C drops the active count with IRQs off; unload sends an IPI to wait for
 * these last instructions too. Legacy kernels do pending work in their asm exit. */
    .type lasx_entry_return, @function
lasx_entry_return:
    UNWIND_HINT_REGS
#if !LASX_USE_IRQENTRY
    la.pcrel $r12, lasx_ret_from_exception
    ld.d $r12, $r12, 0
    jirl $r0, $r12, 0
#else
    /* Kernel r21 is the current CPU base and may change during scheduling.
     * Only user mode has a saved r21 to restore, as in RESTORE_SOME. */
    ld.d $r4, $r3, PT_PRMD
    andi $r4, $r4, 3
    beqz $r4, 3f
    ld.d $r21, $r3, PT_R21
3:
    ld.d $r4, $r3, PT_ERA
    csrwr $r4, 0x6
    ld.d $r4, $r3, PT_PRMD
    csrwr $r4, 0x1
    ld.d $r1, $r3, PT_R1
    ld.d $r2, $r3, PT_R2
    ld.d $r4, $r3, PT_R4
    ld.d $r5, $r3, PT_R5
    ld.d $r6, $r3, PT_R6
    ld.d $r7, $r3, PT_R7
    ld.d $r8, $r3, PT_R8
    ld.d $r9, $r3, PT_R9
    ld.d $r10, $r3, PT_R10
    ld.d $r11, $r3, PT_R11
    ld.d $r12, $r3, PT_R12
    ld.d $r13, $r3, PT_R13
    ld.d $r14, $r3, PT_R14
    ld.d $r15, $r3, PT_R15
    ld.d $r16, $r3, PT_R16
    ld.d $r17, $r3, PT_R17
    ld.d $r18, $r3, PT_R18
    ld.d $r19, $r3, PT_R19
    ld.d $r20, $r3, PT_R20
    ld.d $r22, $r3, PT_R22
    ld.d $r23, $r3, PT_R23
    ld.d $r24, $r3, PT_R24
    ld.d $r25, $r3, PT_R25
    ld.d $r26, $r3, PT_R26
    ld.d $r27, $r3, PT_R27
    ld.d $r28, $r3, PT_R28
    ld.d $r29, $r3, PT_R29
    ld.d $r30, $r3, PT_R30
    ld.d $r31, $r3, PT_R31
    ld.d $r3, $r3, PT_R3
    UNWIND_HINT_FUNC
    ertn
#endif
    .size lasx_entry_return, .-lasx_entry_return
    .section .note.GNU-stack,"",@progbits
