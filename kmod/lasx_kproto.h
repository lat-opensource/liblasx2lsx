/* SPDX-License-Identifier: GPL-2.0 OR MIT */
#ifndef LASX_KPROTO_H
#define LASX_KPROTO_H

/* Shared constants & offsets for kernel C, freestanding C, and assembly */
#define LASX_TCTX_MAGIC 0x4c41535854435458ULL /* "LASXTCTX" */
#define LASX_TRAP_LASX_INSN 1
#define LASX_TRAP_LBT_INIT 2

/* in_rt state values (no 'U' suffix so GNU as accepts them in li.w) */
#define LASX_IN_RT_NONE 0
#define LASX_IN_RT_ACTIVE 1
#define LASX_IN_RT_PASSTHRU 2 /* Re-execute orig_pc and pass SIGILL to user */

/* Translation mode flags passed from kernel module parameter to lasx_rt */
#define LASX_KMODE_PURE_EMU 0
#define LASX_KMODE_ONE (1 << 0)
#define LASX_KMODE_BLOCK (1 << 1)
#define LASX_KMODE_LOOP (1 << 2)
#define LASX_KMODE_USEDEF (1 << 3)
#define LASX_KMODE_FRAG (1 << 4)
#define LASX_KMODE_DEFAULT (LASX_KMODE_ONE | LASX_KMODE_USEDEF | LASX_KMODE_LOOP | LASX_KMODE_FRAG)

/* Total per-thread mapping size: 64KB (4 x 16KB pages)
 *   [0x0000 .. 0x05ff] struct lasx_trap_frame (0x360 bytes)
 *   [0x0600 .. 0x0fff] thread_data_t (0x508 bytes, matches liblasx2lsx)
 *   [0x1000 .. 0xffff] 60KB dedicated private stack (grows down from +0x10000)
 * Note: LASX_TD_OFFSET (0x0600 = 1536) fits in signed 12-bit immediate.
 */
#define LASX_TCTX_SIZE 0x10000UL
#define LASX_TD_OFFSET 0x0600
#define LASX_TD_CLEAR_SIZE 0x0a00
#define LASX_STACK_TOP_OFF 0x10000UL

/* Undefined instruction used as W^X COW text-patching hypercall from lasx_rt:
 *   $a0 = target user PC (4-byte aligned)
 *   $a1 = 32-bit replacement instruction (e.g. jiscr1)
 *   $a7 = LASX_HCALL_MAGIC
 */
#define LASX_HCALL_INSN 0x00000000U
#define LASX_HCALL_MAGIC 0x4c41535850544348ULL /* "LASXPTCH" */

/* Exact byte offsets in struct lasx_trap_frame */
#define TF_OFF_MAGIC 0x000
#define TF_OFF_ORIG_PC 0x008
#define TF_OFF_ORIG_SP 0x010
#define TF_OFF_ORIG_A0 0x018
#define TF_OFF_ORIG_RA 0x020
#define TF_OFF_MODE_FLAGS 0x028
#define TF_OFF_PAD0 0x02c
#define TF_OFF_GPR 0x030
#define TF_OFF_VR 0x130
#define TF_OFF_FCC 0x330
#define TF_OFF_FCSR 0x338
#define TF_OFF_TRAP_KIND 0x33c
#define TF_OFF_RT_BASE 0x340
#define TF_OFF_RT_TEXT_LEN 0x348
#define TF_OFF_IN_RT 0x350
#define TF_OFF_TRAP_COUNT 0x354
#define TF_OFF_PAD1 0x358
#define TF_STRUCT_SIZE 0x360

#ifndef __ASSEMBLY__
#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
#endif

struct lasx_trap_frame {
    uint64_t magic;
    uint64_t orig_pc;
    uint64_t orig_sp;
    uint64_t orig_a0;
    uint64_t orig_ra;
    uint32_t mode_flags;
    uint32_t pad0;
    uint64_t gpr[32];
    uint64_t vr[32][2] __attribute__((aligned(16)));
    uint64_t fcc;
    uint32_t fcsr;
    uint32_t trap_kind;
    uint64_t rt_base;
    uint64_t rt_text_len;
    uint32_t in_rt;
    uint32_t trap_count;
    uint64_t pad1;
};

_Static_assert(offsetof(struct lasx_trap_frame, mode_flags) == TF_OFF_MODE_FLAGS, "TF_OFF_MODE_FLAGS mismatch");
_Static_assert(offsetof(struct lasx_trap_frame, gpr) == TF_OFF_GPR, "TF_OFF_GPR mismatch");
_Static_assert(offsetof(struct lasx_trap_frame, vr) == TF_OFF_VR, "TF_OFF_VR mismatch");
_Static_assert(offsetof(struct lasx_trap_frame, fcc) == TF_OFF_FCC, "TF_OFF_FCC mismatch");
_Static_assert(offsetof(struct lasx_trap_frame, fcsr) == TF_OFF_FCSR, "TF_OFF_FCSR mismatch");
_Static_assert(offsetof(struct lasx_trap_frame, trap_kind) == TF_OFF_TRAP_KIND, "TF_OFF_TRAP_KIND mismatch");
_Static_assert(offsetof(struct lasx_trap_frame, rt_base) == TF_OFF_RT_BASE, "TF_OFF_RT_BASE mismatch");
_Static_assert(offsetof(struct lasx_trap_frame, in_rt) == TF_OFF_IN_RT, "TF_OFF_IN_RT mismatch");
_Static_assert(sizeof(struct lasx_trap_frame) == TF_STRUCT_SIZE, "TF_STRUCT_SIZE mismatch");
#endif /* !__ASSEMBLY__ */

#endif /* LASX_KPROTO_H */
