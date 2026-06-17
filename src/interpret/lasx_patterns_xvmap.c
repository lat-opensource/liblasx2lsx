/* SPDX-License-Identifier: MIT
 *
 * liblasx2lsx — LASX instruction emulation library for LoongArch
 * Copyright (c) 2026 liblasx2lsx contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/mman.h>
#include <ucontext.h>
#include "thread_data.h"
#include "lasx_emu_private.h"
#include "lasx_interpret.h"
#include "lasx_profile.h"
#include "lasx_interpret_opt.h"
#include "lasx_interpret_opt_reg.h"
#include "lagoon.h"
#include "lasx_patterns_xvmap.h"
#include "lasx_profile.h"

#define OPCODE_BEQZ 0x10
#define OPCODE_BNEZ 0x11
#define OPCODE_JIRL 0x13
#define OPCODE_B 0x14
#define OPCODE_BL 0x15
#define OPCODE_BEQ 0x16
#define OPCODE_BNE 0x17
#define OPCODE_BLT 0x18
#define OPCODE_BGE 0x19
#define OPCODE_BLTU 0x1a
#define OPCODE_BGEU 0x1b
#define OPCODE_BLEZ 0x1c
#define OPCODE_ADDI_D 0x0B
#define OPCODE_ADD_D 0x20
#define OPCODE_BGTZ 0x1d
#define MAX_LOOP_INSN 128
#define OPCODE_JISCR1_BASE 0x48000000

static int is_loop_already_optimized(unsigned int *loop_start)
{
    unsigned int instr = *loop_start;
    if ((instr & 0xfc000000) == OPCODE_JISCR1_BASE) {
        LOOP_DBG("Loop already optimized (jiscr1 found at %p)", loop_start);
        return 1;
    }
    return 0;
}

static int is_jiscr(int insn)
{
    /* JISCR0/JISCR1: opcode=0x12, bit[9]=1 */
    return ((insn >> 26) == 0x12) && ((insn >> 9) & 1);
}

static int is_cond_branch(int insn)
{
    int opcode = insn >> 26;
    if (opcode >= OPCODE_BEQ && opcode <= OPCODE_BGEU)
        return 1;
    if (opcode == OPCODE_BEQZ || opcode == OPCODE_BNEZ)
        return 1;
    /* BCEQZ/BCNEZ: opcode=0x12, bit[9]=0 */
    if (opcode == 0x12 && !((insn >> 9) & 1))
        return 1;
    return 0;
}

static int is_uncond_branch(int insn)
{
    int opcode = insn >> 26;
    return opcode == OPCODE_B || opcode == OPCODE_BL || opcode == OPCODE_JIRL;
}

static int is_branch(int insn)
{
    return is_cond_branch(insn) || is_uncond_branch(insn) || is_jiscr(insn);
}

static int extract_branch_offset(unsigned int instr, int opcode)
{
    if (opcode == OPCODE_B || opcode == OPCODE_BL) {
        unsigned int offset = (((instr >> 0) & 0x3ff) << 16) | ((instr >> 10) & 0xffff);
        offset = (int32_t)(offset << 6) >> 6;
        return offset << 2;
    }
    if (opcode >= OPCODE_BEQ && opcode <= OPCODE_BGEU) {
        int32_t offset = (int16_t)((instr >> 10) & 0xffff);
        return offset << 2;
    }
    if (opcode >= OPCODE_BEQZ && opcode <= OPCODE_BNEZ) {
        int offset = (((instr >> 0) & 0x1f) << 16) | ((instr >> 10) & 0xffff);
        offset = (offset << 11) >> 11;
        return offset << 2;
    }
    /* BCEQZ/BCNEZ: same offset format as BEQZ/BNEZ */
    if (opcode == 0x12 && !((instr >> 9) & 1)) {
        int offset = (((instr >> 0) & 0x1f) << 16) | ((instr >> 10) & 0xffff);
        offset = (offset << 11) >> 11;
        return offset << 2;
    }
    return 0;
}

static int branch_target(unsigned int *p, int pc_idx, int instr_idx)
{
    unsigned int instr = p[instr_idx];
    int opcode = instr >> 26;
    int offset = extract_branch_offset(instr, opcode);
    return instr_idx + offset / 4;
}

int detect_loop_range(unsigned int *p, int *loop_start, int *loop_end)
{
    int limit = MAX_LOOP_INSN;

    for (int i = 0; i < limit; i++) {
        unsigned int insn = p[i];
        if (!is_branch(insn))
            continue;

        int target = branch_target(p, 0, i);

        if (target > 0) {
            if (i == 0 && is_cond_branch(p[0]))
                continue;
            return 0;
        }

        if (target <= 0) {
            if ((insn >> 26) == OPCODE_B && is_cond_branch(p[target])) {
                int has_branch = 0;
                for (int k = target + 1; k < i; k++) {
                    if (is_branch(p[k])) {
                        has_branch = 1;
                        break;
                    }
                }
                if (has_branch)
                    return 0;
                int loop_len = i + 1 - target;
                if (loop_len > MAX_LOOP_INSN)
                    return 0;
                *loop_start = target;
                *loop_end = i + 1;
                LOOP_DBG("detect_loop: pattern1 bcc+b at [%d]->[%d], len=%d", i, target, loop_len);
                return loop_len;
            }
            if (is_cond_branch(insn)) {
                int has_branch = 0;
                for (int k = target; k < i; k++) {
                    if (is_branch(p[k])) {
                        has_branch = 1;
                        break;
                    }
                }
                if (has_branch)
                    return 0;
                int loop_len = i + 1 - target;
                if (loop_len > MAX_LOOP_INSN)
                    return 0;
                *loop_start = target;
                *loop_end = i + 1;
                LOOP_DBG("detect_loop: pattern2 bcc at [%d]->[%d], len=%d", i, target, loop_len);
                return loop_len;
            }
            return 0;
        }
    }

    return 0;
}

int check_xvmap_feasibility(unsigned int *p, int loop_len,
                                    uint32_t *xvuse, uint32_t xv_insn_mask[4])
{
    uint32_t xr_mask = 0, vr_all = 0;
    lagoon_insn_t insn;

    memset(xv_insn_mask, 0, sizeof(uint32_t) * 4);

    for (int i = 0; i < loop_len; i++) {
        int is_xv = 0;
        la_disasm_one(p[i], &insn);
        for (int j = 0; j < insn.operand_count; j++) {
            la_operand_t *op = &insn.operands[j];
            if (op->kind == LA_OP_XVPR) {
                xr_mask |= (1u << op->xvpr);
                vr_all  |= (1u << op->xvpr);
                is_xv = 1;
            } else if (op->kind == LA_OP_VPR) {
                vr_all |= (1u << op->vpr);
            } else if (op->kind == LA_OP_FPR) {
                vr_all |= (1u << op->fpr);
            }
        }
        if (is_xv)
            xv_insn_mask[i / 32] |= (1u << (i & 31));
    }

    *xvuse = xr_mask;

    int xr_cnt = __builtin_popcount(xr_mask);
    if (xr_cnt == 0)
        return -1;
    if (xr_cnt > 16)
        return -1;

    int total_vr = __builtin_popcount(vr_all);
    int free_vr = 32 - total_vr;
    if (free_vr < xr_cnt)
        return -1;

    return 0;
}

#include "lasx_interpret_opt_reg.h"

extern uint64_t tp_offset;

void *generate_xvmap_loop(lagoon_assembler_t *as, unsigned int *p,
                                    int loop_start, int loop_len, uint32_t xvuse,
                                    uint32_t xv_insn_mask[4])
{
    int loop_end = loop_start + loop_len;

    int body_start = loop_start;
    int body_end = loop_end - 1;
    int is_pattern1 = 0;
    if (body_start < body_end && is_cond_branch(p[body_start])) {
        is_pattern1 = 1;
        body_start++;
    }

    uint8_t *save_cursor = as->cursor;
    void *jit_entry = la_get_cursor(as);

    la_pcaddi(as, LA_ZERO, 0);
    la_addi_d(as, LA_TP, LA_TP, tp_offset);

    as->xvmapped = 1;
    if (lasx_build_xvmap((int32_t)xvuse, as->xvmap)) {
        as->cursor = save_cursor;
        as->xvmapped = 0;
        assert(0 && "xvmap VR allocation failed");
        la_addi_d(as, LA_TP, LA_TP, -tp_offset);
        return NULL;
    }

    __gen_xvmap_prologue(as, as->xvmap);

    lagoon_label_t end_label = {0};
    lagoon_label_t loop_label = {0};

    la_bind(as, &loop_label);

    profile_emit(as, NULL,
        (unsigned long)(p + loop_start), PROFILE_LOOP);

    int xv_cnt = 0;
    for (int i = 0; i < 4; i++)
        xv_cnt += __builtin_popcount(xv_insn_mask[i]);

    if (is_pattern1) {
        unsigned int bcc = p[loop_start];
        int bcc_op = bcc >> 26;
        int rj = (bcc >> 5) & 0x1f;
        int rd = bcc & 0x1f;
        ptrdiff_t off = la_label(as, &end_label);
        switch (bcc_op) {
        case OPCODE_BEQ:  la_beq(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BNE:  la_bne(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BLT:  la_blt(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BGE:  la_bge(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BLTU: la_bltu(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BGEU: la_bgeu(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BEQZ: la_beqz(as, (la_gpr_t)rj, off); break;
        case OPCODE_BNEZ: la_bnez(as, (la_gpr_t)rj, off); break;
        case 0x12:
            if (!((bcc >> 9) & 1)) { /* BCEQZ/BCNEZ */
                int cj = (bcc >> 5) & 0x7;
                if ((bcc >> 8) & 1)
                    la_bcnez(as, (la_fcc_t)cj, off);
                else
                    la_bceqz(as, (la_fcc_t)cj, off);
            } else {
                goto fail;
            }
            break;
        default: goto fail;
        }
    }

    int loop_jit_start = la_get_inst_count(as);
    (void)loop_jit_start;
    for (int k = body_start; k < body_end; k++) {
        int mask_idx = k - loop_start;
        if (xv_insn_mask[mask_idx / 32] & (1u << (mask_idx & 31))) {
            if (!lasx_interpret_inst_gen_xvmap(as, p[k]))
                goto fail;
        } else {
            la_dup(as, p[k]);
        }
    }

    if (is_pattern1) {
        la_b(as, la_label(as, &loop_label));
    } else {
        unsigned int bcc = p[loop_end - 1];
        int bcc_op = bcc >> 26;
        int rj = (bcc >> 5) & 0x1f;
        int rd = bcc & 0x1f;
        ptrdiff_t off = la_label(as, &loop_label);
        switch (bcc_op) {
        case OPCODE_BEQ:  la_beq(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BNE:  la_bne(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BLT:  la_blt(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BGE:  la_bge(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BLTU: la_bltu(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BGEU: la_bgeu(as, (la_gpr_t)rj, (la_gpr_t)rd, off); break;
        case OPCODE_BEQZ: la_beqz(as, (la_gpr_t)rj, off); break;
        case OPCODE_BNEZ: la_bnez(as, (la_gpr_t)rj, off); break;
        case 0x12:
            if (!((bcc >> 9) & 1)) { /* BCEQZ/BCNEZ */
                int cj = (bcc >> 5) & 0x7;
                if ((bcc >> 8) & 1)
                    la_bcnez(as, (la_fcc_t)cj, off);
                else
                    la_bceqz(as, (la_fcc_t)cj, off);
            } else {
                goto fail;
            }
            break;
        default: goto fail;
        }
    }

    la_bind(as, &end_label);

    la_label_free(as, &end_label);
    la_label_free(as, &loop_label);

    __gen_xvmap_epilogue(as, as->xvmap);

    int exit_offset = loop_len * 4;
    la_addi_d(as, LA_TP, LA_TP, -tp_offset);
    la_jiscr0(as, exit_offset);

    as->xvmapped = 0;

    JITLOG("=== loop xvmap len=%d xv_cnt=%d PC=%016lx entry=%p ===\n",
           loop_len, xv_cnt, (unsigned long)(p + loop_start), jit_entry);

    profile_set(profile_current_pidx(), PROFILE_LOOP, xv_cnt);
    return jit_entry;

fail:
    __gen_xvmap_epilogue(as, as->xvmap);
    as->xvmapped = 0;
    as->cursor = save_cursor;
    assert(0 && "xvmap JIT generation failed");
    return NULL;
}

unsigned int *detected_loop_start = NULL;
static unsigned int *detected_loop_end = NULL;
void *detected_loop_jit_entry = NULL;
int detected_loop_xv_cnt = 0;

int check_lasx_loop_pattern(unsigned int *p, int offset)
{
    int loop_start, loop_end;
    int loop_len = 0;

    if (detected_loop_start && p >= detected_loop_start && p < detected_loop_end) {
        LOOP_DBG("Inside detected loop, skip detection");
        return 0;
    }

    loop_len = detect_loop_range(p + offset, &loop_start, &loop_end);

    if (loop_len == 0) {
        if (detected_loop_start) {
            LOOP_DBG("New PC outside detected loop, reset state");
            detected_loop_start = NULL;
            detected_loop_end = NULL;
            detected_loop_jit_entry = NULL;
        }
        return 0;
    }

    detected_loop_start = p + loop_start;
    detected_loop_end = p + loop_end;
    if (is_loop_already_optimized(p + loop_start)) {
        LOOP_DBG("Loop already optimized, skip");
        return 0;
    }

    LOOP_DBG("Detected loop: %d instructions", loop_len);
#ifdef LASX_PROFILE
    for (int i = loop_start; i < loop_end; i++) {
        char buf[64];
        lagoon_insn_t insn;
        la_disasm_one(p[i], &insn);
        la_insn_to_str(&insn, buf, sizeof(buf));
        LOOP_DBG("  [%d] %08x  %s", i, p[i], buf);
    }
#endif

    return loop_len;
}

/* Phase 1 of block translation: detect loop, check feasibility,
   generate xvmap JIT, patch loop_start.
   Returns true if loop was successfully optimized. */
bool try_optimize_loop(lagoon_assembler_t *as, unsigned int *p, void *entry)
{
    int loop_len = check_lasx_loop_pattern(p, 0);
    if (loop_len <= 0) return false;

    int loop_start_off = (int)(detected_loop_start - p);

    uint32_t xvuse = 0;
    uint32_t xv_insn_mask[4] = {0};
    if (check_xvmap_feasibility(detected_loop_start, loop_len,
                                &xvuse, xv_insn_mask) != 0)
        return false;

    detected_loop_jit_entry = generate_xvmap_loop(
        as, p, loop_start_off, loop_len, xvuse, xv_insn_mask);

    assert(detected_loop_jit_entry);

    int off = (int64_t)detected_loop_jit_entry - (int64_t)interpreter_entry;
    off >>= 2;
    uint32_t jiscr1_offs = 0x48000300
        | (((off>>16)&0x1f) << 0) | ((off&0xffff) << 10);

    uintptr_t page = (uintptr_t)detected_loop_start & ~(0x4000-1);
    mprotect((void*)page, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC);
    *detected_loop_start = jiscr1_offs;
    __asm__ volatile("ibar 0\n\r":::);

    return true;
}

typedef struct {
    const char *libname;
    unsigned long start;
    unsigned long end;
    unsigned int instr_start;
    int created;
    uint32_t xvuse;
} lasx_fragment_t;

static lasx_fragment_t frags[] = {
    {
       .libname = "libxcast.so",
       .start = 0x9b1c4c, /* 0x9b1c4c 2c800482 xvld $xr2,$r4,1(0x1)     */
       .end = 0x9b24d4,   /* 0x9b24d4 47f7791f bnez $r8,-2184(0x7ff778) */
       .instr_start = 0x2c800482,
       .created = 0,
       .xvuse = 0xf, /* xr0, xr1, xr2, xr3 */
    },
    {
        .libname = "libxcast.so",
        .start = 0x987cfc, /* 987cfc:   2c800086    xvld    $xr6,$r4,0 */
        .end = 0x988258,   /* 988258:   47faa53f    bnez    $r9,-1372(0x7ffaa4) */
        .instr_start = 0x2c800086,
        .created = 0,
        .xvuse = 0xfff, /* xr0 - xr11 */
    },
    {
        .libname = "libxcast.so",
        .start = 0x98848c, /* 98848c: 2c8000a6 xvld $xr6,$r5,0 */
        .end = 0x988fe4,   /* 988fe4: 47f4851f bnez $r8,-2940(0x7ff484) */
        .instr_start = 0x2c8000a6,
        .created = 0,
        .xvuse = 0xffff, /* xr0 - xr15 */
    },
    { .libname = NULL, }
};

int lasx_build_xvmap(int32_t mask, int *map) {
    // 初始化为 -1（空闲且未被映射）
    for (int i = 0; i < 32; i++) {
        map[i] = -1;
    }

    uint32_t umask = (uint32_t)mask;   // 使用无符号数进行位操作
    int free_reg = 31;                 // 从最高位开始寻找空闲寄存器

    for (int i = 0; i < 32; i++) {     // 按寄存器编号从小到大遍历
        if (umask & (1u << i)) {       // 寄存器 i 被使用
            // 跳过已被使用的寄存器，找到下一个空闲寄存器
            while (free_reg >= 0 && (umask & (1u << free_reg))) {
                free_reg--;
            }
            if (free_reg >= 0) {       // 成功找到空闲寄存器
                map[i] = free_reg;     // 使用寄存器映射到空闲寄存器
                map[free_reg] = -2;    // 标记该空闲寄存器已被用作目标
                free_reg--;            // 继续向低位寻找下一个空闲
            } else {                   // 没有空闲寄存器可用
                map[i] = -3;           // 无法映射
                return 1; // FAIL
            }
        }
        // 对于空闲寄存器（mask位为0），保持初始值 -1，除非后续被覆盖为 -2
    }
    return 0; // SUCCESS
}

void __gen_xvmap_prologue(void *as, int *map)
{
    int i, r;
    for(i = 0; i < 32; ++i) {
        r = map[i];
        if (r >= 0) {
            la_vst(as, r, LA_TP, TD_OFF_DATA(r, 0));
            la_vld(as, r, LA_TP, TD_OFF_DATA(i, 2));
        }
    }
}

void __gen_xvmap_epilogue(void *as, int *map)
{
    int i, r;
    for(i = 0; i < 32; ++i) {
        r = map[i];
        if (r >= 0) {
            la_vst(as, r, LA_TP, TD_OFF_DATA(i, 2));
            la_vld(as, r, LA_TP, TD_OFF_DATA(r, 0));
        }
    }
}

bool __lasx_emu_create_interpret_fragment(ucontext_t *uc,
        unsigned long pc_start, int range, int fi)
{
    if (interpreter == NULL) return false;
    if (!lasx_interpret_frag_opt) return false;

    interpret_lock();

    unsigned long pc = pc_start;
    unsigned int *p = (void*)pc;

    void *entry = interpreter_entry_ptr;
    lagoon_assembler_t as;

retry:
    la_init_assembler(&as, entry, 0x1000);
    la_pcaddi(&as, LA_ZERO, 0);
    la_addi_d(&as, LA_TP, LA_TP, tp_offset);

//    printf("%s:%d range %d at %p\n", __func__, __LINE__, range, entry);

    profile_emit(&as, entry, pc, PROFILE_FRAG); /* fragment */

    int n = 0;
    int not_xv_n = 0;
    unsigned int instr = p[n];

    if (frags[fi].xvuse) {
        as.xvmapped = 1;

        /*printf("%s:%d use xvmap\n", __func__, __LINE__);*/

        if (lasx_build_xvmap(frags[fi].xvuse, &(as.xvmap[0]))) {
            /* fail to build xvmap */
            frags[fi].xvuse = 0;
            goto retry;
        }

        __gen_xvmap_prologue(&as, &(as.xvmap[0]));

        int fallback = 0;
        while(n < range) {
            instr = p[n];
            if (!lasx_interpret_inst_gen_xvmap(&as, instr)) {
                if (lasx_interpret_inst_gen(&as, instr, 0)) {
                    fallback = 1; /* fallback to disable xvmap */
                    break;
                }
                la_dup(&as, instr);
                not_xv_n += 1;
            }
            n += 1;
        }
        if (fallback) {
            frags[fi].xvuse = 0;
            goto retry;
        }

        __gen_xvmap_epilogue(&as, &(as.xvmap[0]));

        goto done;
    }

    while(n < range) {
        instr = p[n];
        if (!lasx_interpret_inst_gen(&as, instr, 0)) {
            la_dup(&as, instr);
            not_xv_n += 1;
        }
        n += 1;
    }

done:
    la_addi_d(&as, LA_TP, LA_TP, -tp_offset);
    la_jiscr0(&as, n * 4);

    if (n) {
        profile_set(profile_current_pidx(), PROFILE_FRAG, n - not_xv_n);
        int off = (int64_t)entry - (int64_t)interpreter_entry;
        off >>= 2;
        uint32_t jiscr1_off = 0x48000300 | (((off>>16)&0x1f) << 0) | ((off&0xffff) << 10);

        uintptr_t page = pc & ~(0x4000-1);
        mprotect((void*)page, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC);

        *(uint32_t*)pc = jiscr1_off;

        __asm__ volatile("ibar 0\n\r":::);
        //mprotect((void*)page, 0x4000, PROT_READ | PROT_EXEC);
        interpreter_entry_ptr = (void *)((uintptr_t)entry
                + la_get_inst_count(&as) * 4);
    }

    interpret_unlock();

    return (n != 0);
}

bool lasx_emu_create_interpret_fragment(ucontext_t *uc)
{
    unsigned long pc = UC_PC(uc);
    unsigned long addr = 0;
    unsigned long base = 0;
    unsigned long pc_start = 0;
    int i = 0;

    if (!lasx_interpret_frag_opt) return false;

    Dl_info info;
    if (dladdr((void*)pc, &info) == 0) return false;
    base = (unsigned long)info.dli_fbase;
    addr = pc - base;

    while (frags[i].libname != NULL) {
        if (frags[i].created == 0 &&
            strstr(info.dli_fname, frags[i].libname) != NULL &&
            frags[i].start <= addr && addr <= frags[i].end)
        {
            frags[i].created = 1;
            int range = (frags[i].end + 0x4 - frags[i].start) >> 2;
            pc_start = frags[i].start + base;
            if (*((unsigned int*)pc_start) == frags[i].instr_start) {
//                printf("%s:%d [%s] at %p : pc %lx\n", __func__, __LINE__,
//                        info.dli_fname, info.dli_fbase, pc);
                __lasx_emu_create_interpret_fragment(uc, pc_start, range - 1, i);
                return true;
            } else {
                frags[i].created = 2;
                return false;
            }
        }
        i += 1;
    }

    return false;
}
