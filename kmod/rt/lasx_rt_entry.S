/* SPDX-License-Identifier: MIT */
#define __ASSEMBLY__
#include "../lasx_kproto.h"

/* LBT instruction encodings:
 *   movgr2scr $scrN, $rj : 0x00000800 | ((rj & 0x1f) << 5) | (scrN & 3)
 *   jiscr0 0             : 0x48000200
 */
#define INSN_MOVGR2SCR0_T0   (0x00000800 | (12 << 5) | 0)
#define INSN_MOVGR2SCR2_T0   (0x00000800 | (12 << 5) | 2)
#define INSN_JISCR0_0        0x48000200

    .section .bss
    .align 3
    .local __rt_reloc_state
__rt_reloc_state:
    .word 0

    .section .text.entry, "ax", @progbits
    .align 4
    .globl lasx_rt_trap_entry
    .type lasx_rt_trap_entry, @function
/*
 * Entered directly from kernel DIE_RI / my_do_lbt redirect at rt_base + 0x0:
 *   $a0 ($r4) = struct lasx_trap_frame *tf (64KB per-thread tctx base)
 *   $sp ($r3) = tctx + LASX_STACK_TOP_OFF (60KB private stack top)
 *   tf->orig_pc, orig_sp, orig_a0, orig_ra, mode_flags, trap_kind, rt_base, in_rt=1
 *   All other GPRs, $vr0..$vr31, $fcsr0, $fcc0..$fcc7 hold live user state.
 */
lasx_rt_trap_entry:
    /* 1. Save GPRs into tf->gpr[0..31] */
    st.d    $zero, $a0, TF_OFF_GPR + 0 * 8
    st.d    $r2,   $a0, TF_OFF_GPR + 2 * 8
    st.d    $r5,   $a0, TF_OFF_GPR + 5 * 8
    st.d    $r6,   $a0, TF_OFF_GPR + 6 * 8
    st.d    $r7,   $a0, TF_OFF_GPR + 7 * 8
    st.d    $r8,   $a0, TF_OFF_GPR + 8 * 8
    st.d    $r9,   $a0, TF_OFF_GPR + 9 * 8
    st.d    $r10,  $a0, TF_OFF_GPR + 10 * 8
    st.d    $r11,  $a0, TF_OFF_GPR + 11 * 8
    st.d    $r12,  $a0, TF_OFF_GPR + 12 * 8
    st.d    $r13,  $a0, TF_OFF_GPR + 13 * 8
    st.d    $r14,  $a0, TF_OFF_GPR + 14 * 8
    st.d    $r15,  $a0, TF_OFF_GPR + 15 * 8
    st.d    $r16,  $a0, TF_OFF_GPR + 16 * 8
    st.d    $r17,  $a0, TF_OFF_GPR + 17 * 8
    st.d    $r18,  $a0, TF_OFF_GPR + 18 * 8
    st.d    $r19,  $a0, TF_OFF_GPR + 19 * 8
    st.d    $r20,  $a0, TF_OFF_GPR + 20 * 8
    st.d    $r21,  $a0, TF_OFF_GPR + 21 * 8
    st.d    $r22,  $a0, TF_OFF_GPR + 22 * 8
    st.d    $r23,  $a0, TF_OFF_GPR + 23 * 8
    st.d    $r24,  $a0, TF_OFF_GPR + 24 * 8
    st.d    $r25,  $a0, TF_OFF_GPR + 25 * 8
    st.d    $r26,  $a0, TF_OFF_GPR + 26 * 8
    st.d    $r27,  $a0, TF_OFF_GPR + 27 * 8
    st.d    $r28,  $a0, TF_OFF_GPR + 28 * 8
    st.d    $r29,  $a0, TF_OFF_GPR + 29 * 8
    st.d    $r30,  $a0, TF_OFF_GPR + 30 * 8
    st.d    $r31,  $a0, TF_OFF_GPR + 31 * 8

    /* Copy kernel-stashed orig_ra ($r1), orig_sp ($r3), orig_a0 ($r4) */
    ld.d    $t0,   $a0, TF_OFF_ORIG_RA
    st.d    $t0,   $a0, TF_OFF_GPR + 1 * 8
    ld.d    $t0,   $a0, TF_OFF_ORIG_SP
    st.d    $t0,   $a0, TF_OFF_GPR + 3 * 8
    ld.d    $t0,   $a0, TF_OFF_ORIG_A0
    st.d    $t0,   $a0, TF_OFF_GPR + 4 * 8

    /* 2. Save FCSR0 and FCC0..FCC7 (packed 1 byte per fcc, matching sigcontext) */
    movfcsr2gr $t0, $fcsr0
    st.w    $t0,   $a0, TF_OFF_FCSR

    movcf2gr $t0,  $fcc0
    movcf2gr $t1,  $fcc1
    slli.d  $t1,   $t1, 8
    or      $t0,   $t0, $t1
    movcf2gr $t1,  $fcc2
    slli.d  $t1,   $t1, 16
    or      $t0,   $t0, $t1
    movcf2gr $t1,  $fcc3
    slli.d  $t1,   $t1, 24
    or      $t0,   $t0, $t1
    movcf2gr $t1,  $fcc4
    slli.d  $t1,   $t1, 32
    or      $t0,   $t0, $t1
    movcf2gr $t1,  $fcc5
    slli.d  $t1,   $t1, 40
    or      $t0,   $t0, $t1
    movcf2gr $t1,  $fcc6
    slli.d  $t1,   $t1, 48
    or      $t0,   $t0, $t1
    movcf2gr $t1,  $fcc7
    slli.d  $t1,   $t1, 56
    or      $t0,   $t0, $t1
    st.d    $t0,   $a0, TF_OFF_FCC

    /* 3. Save LSX 128-bit registers $vr0..$vr31 */
    vst     $vr0,  $a0, TF_OFF_VR + 0 * 16
    vst     $vr1,  $a0, TF_OFF_VR + 1 * 16
    vst     $vr2,  $a0, TF_OFF_VR + 2 * 16
    vst     $vr3,  $a0, TF_OFF_VR + 3 * 16
    vst     $vr4,  $a0, TF_OFF_VR + 4 * 16
    vst     $vr5,  $a0, TF_OFF_VR + 5 * 16
    vst     $vr6,  $a0, TF_OFF_VR + 6 * 16
    vst     $vr7,  $a0, TF_OFF_VR + 7 * 16
    vst     $vr8,  $a0, TF_OFF_VR + 8 * 16
    vst     $vr9,  $a0, TF_OFF_VR + 9 * 16
    vst     $vr10, $a0, TF_OFF_VR + 10 * 16
    vst     $vr11, $a0, TF_OFF_VR + 11 * 16
    vst     $vr12, $a0, TF_OFF_VR + 12 * 16
    vst     $vr13, $a0, TF_OFF_VR + 13 * 16
    vst     $vr14, $a0, TF_OFF_VR + 14 * 16
    vst     $vr15, $a0, TF_OFF_VR + 15 * 16
    vst     $vr16, $a0, TF_OFF_VR + 16 * 16
    vst     $vr17, $a0, TF_OFF_VR + 17 * 16
    vst     $vr18, $a0, TF_OFF_VR + 18 * 16
    vst     $vr19, $a0, TF_OFF_VR + 19 * 16
    vst     $vr20, $a0, TF_OFF_VR + 20 * 16
    vst     $vr21, $a0, TF_OFF_VR + 21 * 16
    vst     $vr22, $a0, TF_OFF_VR + 22 * 16
    vst     $vr23, $a0, TF_OFF_VR + 23 * 16
    vst     $vr24, $a0, TF_OFF_VR + 24 * 16
    vst     $vr25, $a0, TF_OFF_VR + 25 * 16
    vst     $vr26, $a0, TF_OFF_VR + 26 * 16
    vst     $vr27, $a0, TF_OFF_VR + 27 * 16
    vst     $vr28, $a0, TF_OFF_VR + 28 * 16
    vst     $vr29, $a0, TF_OFF_VR + 29 * 16
    vst     $vr30, $a0, TF_OFF_VR + 30 * 16
    vst     $vr31, $a0, TF_OFF_VR + 31 * 16

    /* 4. One-time multi-thread-safe R_LARCH_RELATIVE self-relocation */
    la.local $t0,  __rt_reloc_state
    ld.w    $t1,   $t0, 0
    li.w    $t2,   2
    beq     $t1,   $t2, .Lreloc_done

    li.w    $t2,   1
    amswap_db.w $t1, $t2, $t0
    bnez    $t1,   .Lreloc_wait

    ld.d    $t3,   $a0, TF_OFF_RT_BASE
    la.local $t4,  __rela_dyn_start
    la.local $t5,  __rela_dyn_end
.Lreloc_loop:
    bgeu    $t4,   $t5, .Lreloc_finish
    ld.d    $t6,   $t4, 0       /* r_offset */
    ld.d    $t7,   $t4, 8       /* r_info */
    ld.d    $t8,   $t4, 16      /* r_addend */
    bstrpick.d $t7, $t7, 31, 0
    li.w    $t2,   3            /* R_LARCH_RELATIVE == 3 */
    bne     $t7,   $t2, .Lreloc_next
    add.d   $t6,   $t3, $t6
    add.d   $t8,   $t3, $t8
    st.d    $t8,   $t6, 0
.Lreloc_next:
    addi.d  $t4,   $t4, 24
    b       .Lreloc_loop

.Lreloc_finish:
    dbar    0
    li.w    $t2,   2
    st.w    $t2,   $t0, 0
    dbar    0
    b       .Lreloc_done

.Lreloc_wait:
    li.w    $t2,   2
1:  ld.w    $t1,   $t0, 0
    bne     $t1,   $t2, 1b
    dbar    0

.Lreloc_done:
    /* 5. Bind $scr2 = tf + LASX_TD_OFFSET (thread_data_t *) and call C handler */
    addi.d  $t0,   $a0, LASX_TD_OFFSET
    .word   INSN_MOVGR2SCR2_T0

    move    $s0,   $a0
    bstrins.d $sp, $zero, 3, 0
    addi.d  $sp,   $sp, -32
    bl      lasx_rt_handle_trap

    move    $a0,   $s0

    /* 6. Restore LSX 128-bit registers $vr0..$vr31 */
    vld     $vr0,  $a0, TF_OFF_VR + 0 * 16
    vld     $vr1,  $a0, TF_OFF_VR + 1 * 16
    vld     $vr2,  $a0, TF_OFF_VR + 2 * 16
    vld     $vr3,  $a0, TF_OFF_VR + 3 * 16
    vld     $vr4,  $a0, TF_OFF_VR + 4 * 16
    vld     $vr5,  $a0, TF_OFF_VR + 5 * 16
    vld     $vr6,  $a0, TF_OFF_VR + 6 * 16
    vld     $vr7,  $a0, TF_OFF_VR + 7 * 16
    vld     $vr8,  $a0, TF_OFF_VR + 8 * 16
    vld     $vr9,  $a0, TF_OFF_VR + 9 * 16
    vld     $vr10, $a0, TF_OFF_VR + 10 * 16
    vld     $vr11, $a0, TF_OFF_VR + 11 * 16
    vld     $vr12, $a0, TF_OFF_VR + 12 * 16
    vld     $vr13, $a0, TF_OFF_VR + 13 * 16
    vld     $vr14, $a0, TF_OFF_VR + 14 * 16
    vld     $vr15, $a0, TF_OFF_VR + 15 * 16
    vld     $vr16, $a0, TF_OFF_VR + 16 * 16
    vld     $vr17, $a0, TF_OFF_VR + 17 * 16
    vld     $vr18, $a0, TF_OFF_VR + 18 * 16
    vld     $vr19, $a0, TF_OFF_VR + 19 * 16
    vld     $vr20, $a0, TF_OFF_VR + 20 * 16
    vld     $vr21, $a0, TF_OFF_VR + 21 * 16
    vld     $vr22, $a0, TF_OFF_VR + 22 * 16
    vld     $vr23, $a0, TF_OFF_VR + 23 * 16
    vld     $vr24, $a0, TF_OFF_VR + 24 * 16
    vld     $vr25, $a0, TF_OFF_VR + 25 * 16
    vld     $vr26, $a0, TF_OFF_VR + 26 * 16
    vld     $vr27, $a0, TF_OFF_VR + 27 * 16
    vld     $vr28, $a0, TF_OFF_VR + 28 * 16
    vld     $vr29, $a0, TF_OFF_VR + 29 * 16
    vld     $vr30, $a0, TF_OFF_VR + 30 * 16
    vld     $vr31, $a0, TF_OFF_VR + 31 * 16

    /* 7. Restore FCC0..FCC7 and FCSR0 */
    ld.d    $t0,   $a0, TF_OFF_FCC
    bstrpick.d $t1, $t0, 0, 0
    movgr2cf $fcc0, $t1
    bstrpick.d $t1, $t0, 8, 8
    movgr2cf $fcc1, $t1
    bstrpick.d $t1, $t0, 16, 16
    movgr2cf $fcc2, $t1
    bstrpick.d $t1, $t0, 24, 24
    movgr2cf $fcc3, $t1
    bstrpick.d $t1, $t0, 32, 32
    movgr2cf $fcc4, $t1
    bstrpick.d $t1, $t0, 40, 40
    movgr2cf $fcc5, $t1
    bstrpick.d $t1, $t0, 48, 48
    movgr2cf $fcc6, $t1
    bstrpick.d $t1, $t0, 56, 56
    movgr2cf $fcc7, $t1

    ld.w    $t0,   $a0, TF_OFF_FCSR
    movgr2fcsr $fcsr0, $t0

    /* 8. Put resume target tf->orig_pc into $scr0 */
    ld.d    $t0,   $a0, TF_OFF_ORIG_PC
    .word   INSN_MOVGR2SCR0_T0

    /* Clear tf->in_rt unless LASX_IN_RT_PASSTHRU (2) was requested */
    ld.w    $t0,   $a0, TF_OFF_IN_RT
    li.w    $t1,   LASX_IN_RT_PASSTHRU
    beq     $t0,   $t1, 1f
    st.w    $zero, $a0, TF_OFF_IN_RT
1:
    /* 9. Restore all 31 GPRs and jump to $scr0 via jiscr0 0 */
    ld.d    $r1,   $a0, TF_OFF_GPR + 1 * 8
    ld.d    $r2,   $a0, TF_OFF_GPR + 2 * 8
    ld.d    $r3,   $a0, TF_OFF_GPR + 3 * 8
    ld.d    $r5,   $a0, TF_OFF_GPR + 5 * 8
    ld.d    $r6,   $a0, TF_OFF_GPR + 6 * 8
    ld.d    $r7,   $a0, TF_OFF_GPR + 7 * 8
    ld.d    $r8,   $a0, TF_OFF_GPR + 8 * 8
    ld.d    $r9,   $a0, TF_OFF_GPR + 9 * 8
    ld.d    $r10,  $a0, TF_OFF_GPR + 10 * 8
    ld.d    $r11,  $a0, TF_OFF_GPR + 11 * 8
    ld.d    $r12,  $a0, TF_OFF_GPR + 12 * 8
    ld.d    $r13,  $a0, TF_OFF_GPR + 13 * 8
    ld.d    $r14,  $a0, TF_OFF_GPR + 14 * 8
    ld.d    $r15,  $a0, TF_OFF_GPR + 15 * 8
    ld.d    $r16,  $a0, TF_OFF_GPR + 16 * 8
    ld.d    $r17,  $a0, TF_OFF_GPR + 17 * 8
    ld.d    $r18,  $a0, TF_OFF_GPR + 18 * 8
    ld.d    $r19,  $a0, TF_OFF_GPR + 19 * 8
    ld.d    $r20,  $a0, TF_OFF_GPR + 20 * 8
    ld.d    $r21,  $a0, TF_OFF_GPR + 21 * 8
    ld.d    $r22,  $a0, TF_OFF_GPR + 22 * 8
    ld.d    $r23,  $a0, TF_OFF_GPR + 23 * 8
    ld.d    $r24,  $a0, TF_OFF_GPR + 24 * 8
    ld.d    $r25,  $a0, TF_OFF_GPR + 25 * 8
    ld.d    $r26,  $a0, TF_OFF_GPR + 26 * 8
    ld.d    $r27,  $a0, TF_OFF_GPR + 27 * 8
    ld.d    $r28,  $a0, TF_OFF_GPR + 28 * 8
    ld.d    $r29,  $a0, TF_OFF_GPR + 29 * 8
    ld.d    $r30,  $a0, TF_OFF_GPR + 30 * 8
    ld.d    $r31,  $a0, TF_OFF_GPR + 31 * 8
    ld.d    $a0,   $a0, TF_OFF_GPR + 4 * 8

    .word   INSN_JISCR0_0
    .size lasx_rt_trap_entry, . - lasx_rt_trap_entry
