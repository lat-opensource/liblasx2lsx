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

#include "lasx_arith.h"
#include "lasx_emu_private.h"
#include "debug.h"
#include "thread_data.h"
#include "lasx_interpret.h"
#include "lasx_interpret_opt_reg.h"
#include "lasx_interpret_opt.h"


// GEN_OPT_4OP: 4-operand (xd, xj, xk, xa)
#define GEN_OPT_4OP(name, lf) \
void __gen_lasx_interpret_##name##_opt(void *as, unsigned int instr) \
{ \
    int xd = (instr >> 0) & 0x1f; \
    int xj = (instr >> 5) & 0x1f; \
    int xk = (instr >> 10) & 0x1f; \
    int xa = (instr >> 15) & 0x1f; \
    tdlog("                 : interpret(optgen) " #name " %08x \\n", instr); \
    int excl_vr[] = { xd, xj, xk, xa }; \
    int vt[3]; \
    gen_reg_find_free_vrs(vt, 3, excl_vr, 4); \
    gen_reg_save_vr(as, vt[0]); \
    gen_reg_save_vr(as, vt[1]); \
    gen_reg_save_vr(as, vt[2]); \
    la_##lf(as, xd, xj, xk, xa); \
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2)); \
    la_vld(as, vt[1], LA_TP, TD_OFF_DATA(xk, 2)); \
    la_vld(as, vt[2], LA_TP, TD_OFF_DATA(xa, 2)); \
    la_##lf(as, vt[0], vt[0], vt[1], vt[2]); \
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2)); \
    gen_reg_restore_vr(as, vt[0]); \
    gen_reg_restore_vr(as, vt[1]); \
    gen_reg_restore_vr(as, vt[2]); \
}

// GEN_OPT_SET_FCC: FCC-output set instructions (AND/OR combine)
#define GEN_OPT_SET_FCC(name, lf, combine) \
void __gen_lasx_interpret_##name##_opt(void *as, unsigned int instr) \
{ \
    int cd = (instr >> 0) & 0x1f; \
    int xj = (instr >> 5) & 0x1f; \
    tdlog("                 : interpret(optgen) " #name " %08x \\n", instr); \
    int excl_vr[] = { xj }; \
    int vt[1]; \
    gen_reg_find_free_vrs(vt, 1, excl_vr, 1); \
    gen_reg_save_vr(as, vt[0]); \
    int excl_gr[] = { 0 }; \
    int gr[2]; \
    gen_reg_find_free_grs(gr, 2, excl_gr, 0); \
    gen_reg_save_gr(as, gr[0]); \
    gen_reg_save_gr(as, gr[1]); \
    la_##lf(as, LA_FCC0 + cd, xj); \
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd); \
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2)); \
    la_##lf(as, LA_FCC0 + cd, vt[0]); \
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd); \
    la_##combine(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]); \
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]); \
    gen_reg_restore_gr(as, gr[0]); \
    gen_reg_restore_gr(as, gr[1]); \
    gen_reg_restore_vr(as, vt[0]); \
}

/* ================================================================
 * Macro definitions — auto-generated from lagoon function signatures
 * ================================================================ */

// GEN_OPT_3OP: 3-operand (xd, xj, xk) — arithmetic, logic, compare, shift
#define GEN_OPT_3OP(name, lf) \
void __gen_lasx_interpret_##name##_opt(void *as, unsigned int instr) \
{ \
    int xd = (instr >> 0) & 0x1f; \
    int xj = (instr >> 5) & 0x1f; \
    int xk = (instr >> 10) & 0x1f; \
    tdlog("                 : interpret(optgen) " #name " %08x \n", instr); \
    int excl_vr[] = { xd, xj, xk }; \
    int vt[2]; \
    gen_reg_find_free_vrs(vt, 2, excl_vr, 3); \
    gen_reg_save_vr(as, vt[0]); \
    gen_reg_save_vr(as, vt[1]); \
    la_##lf(as, xd, xj, xk); \
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2)); \
    la_vld(as, vt[1], LA_TP, TD_OFF_DATA(xk, 2)); \
    la_##lf(as, vt[0], vt[0], vt[1]); \
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2)); \
    gen_reg_restore_vr(as, vt[0]); \
    gen_reg_restore_vr(as, vt[1]); \
}

// GEN_OPT_2OP: 2-operand (xd, xj) — unary, conversion, sign-extend
#define GEN_OPT_2OP(name, lf) \
void __gen_lasx_interpret_##name##_opt(void *as, unsigned int instr) \
{ \
    int xd = (instr >> 0) & 0x1f; \
    int xj = (instr >> 5) & 0x1f; \
    tdlog("                 : interpret(optgen) " #name " %08x \n", instr); \
    int excl_vr[] = { xd, xj }; \
    int vt[1]; \
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2); \
    gen_reg_save_vr(as, vt[0]); \
    la_##lf(as, xd, xj); \
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2)); \
    la_##lf(as, vt[0], vt[0]); \
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2)); \
    gen_reg_restore_vr(as, vt[0]); \
}

// GEN_OPT_2OP_IMM: 2-operand + immediate (xd, xj, imm)
#define GEN_OPT_2OP_IMM(name, lf, imm_bits, imm_shift) \
void __gen_lasx_interpret_##name##_opt(void *as, unsigned int instr) \
{ \
    int xd = (instr >> 0) & 0x1f; \
    int xj = (instr >> 5) & 0x1f; \
    int imm = (instr >> imm_shift) & ((1u << imm_bits) - 1); \
    tdlog("                 : interpret(optgen) " #name " %08x \n", instr); \
    int excl_vr[] = { xd, xj }; \
    int vt[1]; \
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2); \
    gen_reg_save_vr(as, vt[0]); \
    la_##lf(as, xd, xj, imm); \
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2)); \
    la_##lf(as, vt[0], vt[0], imm); \
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2)); \
    gen_reg_restore_vr(as, vt[0]); \
}

// GEN_OPT_3OP_RMW: 3-operand read-modify-write (xd as src+dest)
#define GEN_OPT_3OP_RMW(name, lf) \
void __gen_lasx_interpret_##name##_opt(void *as, unsigned int instr) \
{ \
    int xd = (instr >> 0) & 0x1f; \
    int xj = (instr >> 5) & 0x1f; \
    int xk = (instr >> 10) & 0x1f; \
    tdlog("                 : interpret(optgen) " #name " %08x \n", instr); \
    int excl_vr[] = { xd, xj, xk }; \
    int vt[3]; \
    gen_reg_find_free_vrs(vt, 3, excl_vr, 3); \
    gen_reg_save_vr(as, vt[0]); \
    gen_reg_save_vr(as, vt[1]); \
    gen_reg_save_vr(as, vt[2]); \
    la_##lf(as, xd, xj, xk); \
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2)); \
    la_vld(as, vt[1], LA_TP, TD_OFF_DATA(xk, 2)); \
    la_vld(as, vt[2], LA_TP, TD_OFF_DATA(xd, 2)); \
    la_##lf(as, vt[2], vt[0], vt[1]); \
    la_vst(as, vt[2], LA_TP, TD_OFF_DATA(xd, 2)); \
    gen_reg_restore_vr(as, vt[0]); \
    gen_reg_restore_vr(as, vt[1]); \
    gen_reg_restore_vr(as, vt[2]); \
}

// GEN_OPT_2OP_IMM_RMW: 2-operand+imm with read-modify-write (narrow shift, permi_w)
#define GEN_OPT_2OP_IMM_RMW(name, lf, imm_bits, imm_shift) \
void __gen_lasx_interpret_##name##_opt(void *as, unsigned int instr) \
{ \
    int xd = (instr >> 0) & 0x1f; \
    int xj = (instr >> 5) & 0x1f; \
    int imm = (instr >> imm_shift) & ((1u << imm_bits) - 1); \
    tdlog("                 : interpret(optgen) " #name " %08x \\n", instr); \
    int excl_vr[] = { xd, xj }; \
    int vt[2]; \
    gen_reg_find_free_vrs(vt, 2, excl_vr, 2); \
    gen_reg_save_vr(as, vt[0]); \
    gen_reg_save_vr(as, vt[1]); \
    la_##lf(as, xd, xj, imm); \
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2)); \
    la_vld(as, vt[1], LA_TP, TD_OFF_DATA(xd, 2)); \
    la_##lf(as, vt[1], vt[0], imm); \
    la_vst(as, vt[1], LA_TP, TD_OFF_DATA(xd, 2)); \
    gen_reg_restore_vr(as, vt[0]); \
    gen_reg_restore_vr(as, vt[1]); \
}

/* ================================================================
 * Custom / special-case implementations
 * ================================================================ */

void __gen_lasx_interpret_vext2xv_d_b_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_d_b %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vshuf4i_d(as, vt0, xj, (3 << 0) | (2 << 2));
    la_vexth_h_b(as, vt0, vt0);
    la_vshuf4i_d(as, vt0, vt0, (3 << 0) | (2 << 2));
    la_vexth_w_h(as, vt0, vt0);

    la_vexth_d_w(as, xd, vt0);
    la_vst(as, xd, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, vt0, vt0, (3 << 0) | (2 << 2));
    la_vexth_d_w(as, xd, vt0);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_d_h_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_d_h %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vshuf4i_d(as, vt0, xj, (3 << 0) | (2 << 2));
    la_vexth_w_h(as, vt0, vt0);

    la_vexth_d_w(as, xd, vt0);
    la_vst(as, xd, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, vt0, vt0, (3 << 0) | (2 << 2));
    la_vexth_d_w(as, xd, vt0);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_d_w_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_d_w %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vexth_d_w(as, vt0, xj);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_d_w(as, xd, xd);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_du_bu_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_du_bu %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vshuf4i_d(as, vt0, xj, (3 << 0) | (2 << 2));
    la_vexth_hu_bu(as, vt0, vt0);
    la_vshuf4i_d(as, vt0, vt0, (3 << 0) | (2 << 2));
    la_vexth_wu_hu(as, vt0, vt0);

    la_vexth_du_wu(as, xd, vt0);
    la_vst(as, xd, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, vt0, vt0, (3 << 0) | (2 << 2));
    la_vexth_du_wu(as, xd, vt0);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_du_hu_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_du_hu %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vshuf4i_d(as, vt0, xj, (3 << 0) | (2 << 2));
    la_vexth_wu_hu(as, vt0, vt0);

    la_vexth_du_wu(as, xd, vt0);
    la_vst(as, xd, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, vt0, vt0, (3 << 0) | (2 << 2));
    la_vexth_du_wu(as, xd, vt0);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_du_wu_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_du_wu %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vexth_du_wu(as, vt0, xj);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_du_wu(as, xd, xd);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_h_b_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_h_b %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vexth_h_b(as, vt0, xj);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_h_b(as, xd, xd);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_hu_bu_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_hu_bu %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vexth_hu_bu(as, vt0, xj);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_hu_bu(as, xd, xd);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_w_b_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_w_b %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vshuf4i_d(as, vt0, xj, (3 << 0) | (2 << 2));
    la_vexth_h_b(as, vt0, vt0);

    la_vexth_w_h(as, xd, vt0);
    la_vst(as, xd, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, vt0, vt0, (3 << 0) | (2 << 2));
    la_vexth_w_h(as, xd, vt0);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_w_h_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_w_h %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vexth_w_h(as, vt0, xj);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_w_h(as, xd, xd);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_wu_bu_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_wu_bu %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vshuf4i_d(as, vt0, xj, (3 << 0) | (2 << 2));
    la_vexth_hu_bu(as, vt0, vt0);

    la_vexth_wu_hu(as, xd, vt0);
    la_vst(as, xd, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, vt0, vt0, (3 << 0) | (2 << 2));
    la_vexth_wu_hu(as, xd, vt0);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_vext2xv_wu_hu_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) vext2xv_wu_hu %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_vexth_wu_hu(as, vt0, xj);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_wu_hu(as, xd, xd);

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_xvfcmp_cond_d_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int xk = (instr >> 10) & 0x1f;
    int cond = (instr >> 15) & 0x1f;
    tdlog("                 : interpret(optgen) xvfcmp_cond_d %08x \n", instr);


    int excl_vr[] = { xd, xj, xk };
    int vt[2];
    gen_reg_find_free_vrs(vt, 2, excl_vr, 3);

    gen_reg_save_vr(as, vt[0]);
    gen_reg_save_vr(as, vt[1]);

    void (*cmp_func)(lagoon_assembler_t*, la_vpr_t, la_vpr_t, la_vpr_t) = la_vfcmp_caf_d;
    switch (cond) {
    case 0x00: cmp_func = la_vfcmp_caf_d; break; // CAF - always false
    case 0x01: cmp_func = la_vfcmp_saf_d; break; // SAF - signaling always false
    case 0x02: cmp_func = la_vfcmp_clt_d; break; // CLT - compare less than
    case 0x03: cmp_func = la_vfcmp_slt_d; break; // SLT - signaling less than
    case 0x04: cmp_func = la_vfcmp_ceq_d; break; // CEQ - compare equal
    case 0x05: cmp_func = la_vfcmp_seq_d; break; // SEQ - signaling equal
    case 0x06: cmp_func = la_vfcmp_cle_d; break; // CLE - compare less or equal
    case 0x07: cmp_func = la_vfcmp_sle_d; break; // SLE - signaling less or equal
    case 0x08: cmp_func = la_vfcmp_cun_d; break; // CUN - compare unordered
    case 0x09: cmp_func = la_vfcmp_sun_d; break; // SUN - signaling ordered
    case 0x0A: cmp_func = la_vfcmp_cult_d; break; // CULT - compare unordered or less
    case 0x0B: cmp_func = la_vfcmp_sult_d; break; // SULT - signaling unordered or less
    case 0x0C: cmp_func = la_vfcmp_cueq_d; break; // CUEQ - compare unordered or equal
    case 0x0D: cmp_func = la_vfcmp_sueq_d; break; // SUEQ - signaling unordered or equal
    case 0x0E: cmp_func = la_vfcmp_cule_d; break; // CULE - compare unordered or less or equal
    case 0x0F: cmp_func = la_vfcmp_sule_d; break; // SULE - signaling unordered or less or equal
    case 0x10: cmp_func = la_vfcmp_cne_d; break; // CNE - compare not equal
    case 0x11: cmp_func = la_vfcmp_sne_d; break; // SNE - signaling not equal
    case 0x12: cmp_func = la_vfcmp_cor_d; break; // COR - compare ordered
    case 0x13: cmp_func = la_vfcmp_sor_d; break; // SOR - signaling unordered
    case 0x14: cmp_func = la_vfcmp_cor_d; break; // COR (alternate) - compare ordered (fix for binutils encoding)
    case 0x15: cmp_func = la_vfcmp_sune_d; break; // SUNE - signaling unordered or not equal
    case 0x18: cmp_func = la_vfcmp_cune_d; break; // CUNE (alt encoding) - compare unordered or not equal
    case 0x19: cmp_func = la_vfcmp_sune_d; break; // SUNE (alt encoding) - signaling unordered or not equal
    default: cmp_func = la_vfcmp_caf_d; break;
    }

    cmp_func(as, xd, xj, xk);

    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vld(as, vt[1], LA_TP, TD_OFF_DATA(xk, 2));
    cmp_func(as, vt[0], vt[0], vt[1]);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);
    gen_reg_restore_vr(as, vt[1]);

}

void __gen_lasx_interpret_xvfcmp_cond_s_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int xk = (instr >> 10) & 0x1f;
    int cond = (instr >> 15) & 0x1f;
    tdlog("                 : interpret(optgen) xvfcmp_cond_s %08x \n", instr);


    int excl_vr[] = { xd, xj, xk };
    int vt[2];
    gen_reg_find_free_vrs(vt, 2, excl_vr, 3);

    gen_reg_save_vr(as, vt[0]);
    gen_reg_save_vr(as, vt[1]);

    void (*cmp_func)(lagoon_assembler_t*, la_vpr_t, la_vpr_t, la_vpr_t) = la_vfcmp_caf_s;
    switch (cond) {
    case 0x00: cmp_func = la_vfcmp_caf_s; break; // CAF - always false
    case 0x01: cmp_func = la_vfcmp_saf_s; break; // SAF - signaling always false
    case 0x02: cmp_func = la_vfcmp_clt_s; break; // CLT - compare less than
    case 0x03: cmp_func = la_vfcmp_slt_s; break; // SLT - signaling less than
    case 0x04: cmp_func = la_vfcmp_ceq_s; break; // CEQ - compare equal
    case 0x05: cmp_func = la_vfcmp_seq_s; break; // SEQ - signaling equal
    case 0x06: cmp_func = la_vfcmp_cle_s; break; // CLE - compare less or equal
    case 0x07: cmp_func = la_vfcmp_sle_s; break; // SLE - signaling less or equal
    case 0x08: cmp_func = la_vfcmp_cun_s; break; // CUN - compare unordered
    case 0x09: cmp_func = la_vfcmp_sun_s; break; // SUN - signaling ordered
    case 0x0A: cmp_func = la_vfcmp_cult_s; break; // CULT - compare unordered or less
    case 0x0B: cmp_func = la_vfcmp_sult_s; break; // SULT - signaling unordered or less
    case 0x0C: cmp_func = la_vfcmp_cueq_s; break; // CUEQ - compare unordered or equal
    case 0x0D: cmp_func = la_vfcmp_sueq_s; break; // SUEQ - signaling unordered or equal
    case 0x0E: cmp_func = la_vfcmp_cule_s; break; // CULE - compare unordered or less or equal
    case 0x0F: cmp_func = la_vfcmp_sule_s; break; // SULE - signaling unordered or less or equal
    case 0x10: cmp_func = la_vfcmp_cne_s; break; // CNE - compare not equal
    case 0x11: cmp_func = la_vfcmp_sne_s; break; // SNE - signaling not equal
    case 0x12: cmp_func = la_vfcmp_cor_s; break; // COR - compare ordered
    case 0x13: cmp_func = la_vfcmp_sor_s; break; // SOR - signaling unordered
    case 0x14: cmp_func = la_vfcmp_cor_s; break; // COR (alternate) - compare ordered (fix for binutils encoding)
    case 0x15: cmp_func = la_vfcmp_sune_s; break; // SUNE - signaling unordered or not equal
    case 0x18: cmp_func = la_vfcmp_cune_s; break; // CUNE (alt encoding) - compare unordered or not equal
    case 0x19: cmp_func = la_vfcmp_sune_s; break; // SUNE (alt encoding) - signaling unordered or not equal
    default: cmp_func = la_vfcmp_caf_s; break;
    }

    cmp_func(as, xd, xj, xk);

    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vld(as, vt[1], LA_TP, TD_OFF_DATA(xk, 2));
    cmp_func(as, vt[0], vt[0], vt[1]);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);
    gen_reg_restore_vr(as, vt[1]);

}



void __gen_lasx_interpret_xvinsgr2vr_w_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int idx = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen) xvinsgr2vr_w %08x \n", instr);


    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);

    gen_reg_save_vr(as, vt0);

    if (idx < 4) {
        la_vinsgr2vr_w(as, xd, rj, idx);
    } else {
        la_vld(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
        la_vinsgr2vr_w(as, vt0, rj, idx - 4);
        la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
    }

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_xvinsve0_d_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int idx = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen) xvinsve0_d %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_movfr2gr_d(as, LA_RX, xj);
    if (idx < 2) {
        la_vinsgr2vr_d(as, xd, LA_RX, idx);
    } else {
        la_vld(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
        la_vinsgr2vr_d(as, vt0, LA_RX, idx - 2);
        la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
    }

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_xvinsve0_w_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int idx = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen) xvinsve0_w %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt0);

    la_movfr2gr_s(as, LA_RX, xj);
    if (idx < 4) {
        la_vinsgr2vr_w(as, xd, LA_RX, idx);
    } else {
        la_vld(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
        la_vinsgr2vr_w(as, vt0, LA_RX, idx - 4);
        la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
    }

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_xvld_opt(void *as, unsigned int instr)
{
    int rj = (instr >> 5) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    int off = (instr >> 10) & 0xfff;
    off = (off << 20) >> 20;
    tdlog("                 : interpret(optgen) xvld %08x \n", instr);

    if (off >= -2048 && off <= 2047 - 16) {
        la_vld(as, xd, rj, off + 16);
        la_vst(as, xd, LA_TP, TD_OFF_DATA(xd, 2));
        la_vld(as, xd, rj, off);
    } else {
        int excl_gr[] = { rj };
        int t0;
        gen_reg_find_free_grs(&t0, 1, excl_gr, 1);
        gen_reg_save_gr(as, t0);
        la_load_immediate32(as, t0, off);
        la_add_d(as, t0, rj, t0);
        la_vld(as, xd, t0, 16);
        la_vst(as, xd, LA_TP, TD_OFF_DATA(xd, 2));
        la_vld(as, xd, t0, 0);
        gen_reg_restore_gr(as, t0);
    }
}

void __gen_lasx_interpret_xvldi_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int imm = (instr >> 5) & 0x1fff;
    tdlog("                 : interpret(optgen) xvldi %08x \n", instr);


    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);

    gen_reg_save_vr(as, vt0);

    la_vldi(as, xd, imm);
    la_vldi(as, vt0, imm);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_xvldrepl_b_opt(void *as, unsigned int instr)
{
    int rj = (instr >> 5) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    int off = (instr >> 10) & 0xfff;
    off = (off << 20) >> 20;
    tdlog("                 : interpret(optgen) xvldrepl_b %08x \n", instr);

    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt0);

    la_vldrepl_b(as, xd, rj, off);
    la_vldrepl_b(as, vt0, rj, off);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt0);
}

void __gen_lasx_interpret_xvldrepl_d_opt(void *as, unsigned int instr)
{
    int rj = (instr >> 5) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    int off = (instr >> 10) & 0x1ff;
    off = (off << 23) >> 23;
    tdlog("                 : interpret(optgen) xvldrepl_d %08x \n", instr);

    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt0);

    la_vldrepl_d(as, xd, rj, off << 3);
    la_vldrepl_d(as, vt0, rj, off << 3);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt0);
}

void __gen_lasx_interpret_xvldrepl_h_opt(void *as, unsigned int instr)
{
    int rj = (instr >> 5) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    int off = (instr >> 10) & 0x7ff;
    off = (off << 21) >> 21;
    tdlog("                 : interpret(optgen) xvldrepl_h %08x \n", instr);

    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt0);

    la_vldrepl_h(as, xd, rj, off << 1);
    la_vldrepl_h(as, vt0, rj, off << 1);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt0);
}

void __gen_lasx_interpret_xvldrepl_w_opt(void *as, unsigned int instr)
{
    int rj = (instr >> 5) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    int off = (instr >> 10) & 0x3ff;
    off = (off << 22) >> 22;
    tdlog("                 : interpret(optgen) xvldrepl_w %08x \n", instr);

    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt0);

    la_vldrepl_w(as, xd, rj, off << 2);
    la_vldrepl_w(as, vt0, rj, off << 2);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt0);
}

void __gen_lasx_interpret_xvldx_opt(void *as, unsigned int instr)
{
    int rj = (instr >> 5) & 0x1f;
    int rk = (instr >> 10) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    tdlog("                 : interpret(optgen) xvldx %08x \n", instr);

    int excl_gr[] = { rj, rk };
    int t0;
    gen_reg_find_free_grs(&t0, 1, excl_gr, 2);
    gen_reg_save_gr(as, t0);

    la_add_d(as, t0, rj, rk);
    la_vld(as, xd, t0, 16);
    la_vst(as, xd, LA_TP, TD_OFF_DATA(xd, 2));
    la_vld(as, xd, t0, 0);

    gen_reg_restore_gr(as, t0);
}

void __gen_lasx_interpret_xvperm_w_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int xk = (instr >> 10) & 0x1f;
    tdlog("                 : interpret(optgen) xvperm.w %08x \n", instr);


    int excl_vr[] = { xd, xj, xk };
    int vt[3];
    gen_reg_find_free_vrs(vt, 3, excl_vr, 3);

    gen_reg_save_vr(as, vt[0]);
    gen_reg_save_vr(as, vt[1]);
    gen_reg_save_vr(as, vt[2]);

    la_vori_b(as, vt[0], xj, 0);
    la_vld(as, vt[1], LA_TP, TD_OFF_DATA(xj, 2));

    la_vori_b(as, xd, xk, 0);
    la_vshuf_w(as, xd, vt[1], vt[0]);

    la_vld(as, vt[2], LA_TP, TD_OFF_DATA(xk, 2));
    la_vshuf_w(as, vt[2], vt[1], vt[0]);
    la_vst(as, vt[2], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);
    gen_reg_restore_vr(as, vt[1]);
    gen_reg_restore_vr(as, vt[2]);

}

void __gen_lasx_interpret_xvpermi_d_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xff;
    tdlog("                 : interpret(optgen) xvpermi_d %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt[2];
    gen_reg_find_free_vrs(vt, 2, excl_vr, 2);

    gen_reg_save_vr(as, vt[0]);
    gen_reg_save_vr(as, vt[1]);

    if (xd != xj) la_vori_b(as, xd, xj, 0);
    la_vori_b(as, vt[0], xj, 0);

    la_vld(as, vt[1], LA_TP, TD_OFF_DATA(xj, 2));

    la_vshuf4i_d(as, xd,    vt[1], (imm >> 0) & 0xf);
    la_vshuf4i_d(as, vt[0], vt[1], (imm >> 4) & 0xf);

    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);
    gen_reg_restore_vr(as, vt[1]);

}

void __gen_lasx_interpret_xvpermi_q_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xff;
    tdlog("                 : interpret(optgen) xvpermi_q %08x \n", instr);


    int excl_vr[] = { xd, xj };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt[0]);

    int q0 = (imm >> 0) & 0x3;
    int q1 = (imm >> 4) & 0x3;

    switch (q1) {
    case 0: la_vori_b(as, vt[0], xj, 0); break;
    case 2: la_vori_b(as, vt[0], xd, 0); break;
    case 1: la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2)); break;
    case 3: la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2)); break;
    }

    switch (q0) {
    case 0: la_vori_b(as, xd, xj, 0); break;
    case 2: la_vori_b(as, xd, xd, 0); break;
    case 1: la_vld(as, xd, LA_TP, TD_OFF_DATA(xj, 2)); break;
    case 3: la_vld(as, xd, LA_TP, TD_OFF_DATA(xd, 2)); break;
    }

    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);

}

void __gen_lasx_interpret_xvpickve2gr_d_opt(void *as, unsigned int instr)
{
    int rd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int uk = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen) xvpickve2gr_d %08x \n", instr);


    if (uk < 2) {
        la_vpickve2gr_d(as, rd, xj, uk & 0x1);
    } else {
        la_ld_d(as, rd, LA_TP, TD_OFF_DATA(xj, uk));
    }

}

void __gen_lasx_interpret_xvpickve2gr_du_opt(void *as, unsigned int instr)
{
    int rd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int uk = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen) xvpickve2gr_du %08x \n", instr);


    if (uk < 2) {
        la_vpickve2gr_du(as, rd, xj, uk & 0x1);
    } else {
        la_ld_d(as, rd, LA_TP, TD_OFF_DATA(xj, uk));
    }

}

void __gen_lasx_interpret_xvpickve2gr_w_opt(void *as, unsigned int instr)
{
    int rd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int uk = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen) xvpickve2gr_w %08x \n", instr);


    if (uk < 4) {
        la_vpickve2gr_w(as, rd, xj, uk & 0x3);
    } else {
        la_ld_w(as, rd, LA_TP, TD_OFF_DATA_W(xj, uk));
    }

}

void __gen_lasx_interpret_xvpickve2gr_wu_opt(void *as, unsigned int instr)
{
    int rd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int uk = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen) xvpickve2gr_wu %08x \n", instr);


    if (uk < 4) {
        la_vpickve2gr_wu(as, rd, xj, uk & 0x3);
    } else {
        la_ld_wu(as, rd, LA_TP, TD_OFF_DATA_W(xj, uk));
    }

}

void __gen_lasx_interpret_xvpickve_d_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int idx = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen) xvpickve_d %08x \n", instr);

    if (idx < 2) {
        la_vpickve2gr_d(as, LA_RX, xj, idx);
    } else {
        int excl_vr[] = { xd };
        int vt[1];
        gen_reg_find_free_vrs(vt, 1, excl_vr, 1);
        gen_reg_save_vr(as, vt[0]);
        la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
        la_vpickve2gr_d(as, LA_RX, vt[0], idx - 2);
        gen_reg_restore_vr(as, vt[0]);
    }
    int excl_vr[] = { xd };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt[0]);
    la_vinsgr2vr_d(as, xd, LA_RX, 0);
    la_vinsgr2vr_d(as, xd, LA_ZERO, 1);
    la_vxor_v(as, vt[0], vt[0], vt[0]);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));
    gen_reg_restore_vr(as, vt[0]);
}

void __gen_lasx_interpret_xvpickve_w_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int idx = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen) xvpickve_w %08x \n", instr);


    if (idx < 4) {
        la_vpickve2gr_w(as, LA_RX, xj, idx & 0x3);
    } else {
        la_ld_w(as, LA_RX, LA_TP, TD_OFF_DATA_W(xj, idx));
    }
    la_vinsgr2vr_d(as, xd, LA_ZERO, 0);
    la_vinsgr2vr_d(as, xd, LA_ZERO, 1);
    la_st_d(as, LA_ZERO, LA_TP, TD_OFF_DATA(xd, 2));
    la_st_d(as, LA_ZERO, LA_TP, TD_OFF_DATA(xd, 3));
    la_vinsgr2vr_w(as, xd, LA_RX, 0);

}

void __gen_lasx_interpret_xvrepl128vei_b_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xf;
    tdlog("                 : interpret(optgen) xvrepl128vei_b %08x \\\n", instr);


    int excl_vr[] = { xd, xj };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt[0]);

    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vreplvei_b(as, xd, xj, imm);
    la_vreplvei_b(as, vt[0], vt[0], imm);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);

}



void __gen_lasx_interpret_xvrepl128vei_h_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen) xvrepl128vei_h %08x \\\n", instr);


    int excl_vr[] = { xd, xj };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt[0]);

    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vreplvei_h(as, xd, xj, imm);
    la_vreplvei_h(as, vt[0], vt[0], imm);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);

}

void __gen_lasx_interpret_xvrepl128vei_w_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen) xvrepl128vei_w %08x \\\n", instr);


    int excl_vr[] = { xd, xj };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2);

    gen_reg_save_vr(as, vt[0]);

    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vreplvei_w(as, xd, xj, imm);
    la_vreplvei_w(as, vt[0], vt[0], imm);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);

}

void __gen_lasx_interpret_xvreplgr2vr_b_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) xvreplgr2vr_b %08x \n", instr);


    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);

    gen_reg_save_vr(as, vt0);

    la_vreplgr2vr_b(as, xd, rj);
    la_vreplgr2vr_b(as, vt0, rj);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt0);

}



void __gen_lasx_interpret_xvreplgr2vr_h_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) xvreplgr2vr_h %08x \n", instr);


    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);

    gen_reg_save_vr(as, vt0);

    la_vreplgr2vr_h(as, xd, rj);
    la_vreplgr2vr_h(as, vt0, rj);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_xvreplgr2vr_w_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) xvreplgr2vr_w %08x \n", instr);


    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);

    gen_reg_save_vr(as, vt0);

    la_vreplgr2vr_w(as, xd, rj);
    la_vreplgr2vr_w(as, vt0, rj);
    la_vst(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt0);

}

void __gen_lasx_interpret_xvreplve0_b_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) xvreplve0_b %08x \n", instr);
    int excl_vr[] = { xd, xj };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2);
    gen_reg_save_vr(as, vt[0]);
    la_vreplvei_b(as, xd, xj, 0);
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vreplvei_b(as, vt[0], xj, 0);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));
    gen_reg_restore_vr(as, vt[0]);
}

void __gen_lasx_interpret_xvreplve0_d_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) xvreplve0_d %08x \n", instr);
    int excl_vr[] = { xd, xj };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2);
    gen_reg_save_vr(as, vt[0]);
    la_vreplvei_d(as, xd, xj, 0);
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vreplvei_d(as, vt[0], xj, 0);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));
    gen_reg_restore_vr(as, vt[0]);
}

void __gen_lasx_interpret_xvreplve0_h_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) xvreplve0_h %08x \n", instr);
    int excl_vr[] = { xd, xj };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2);
    gen_reg_save_vr(as, vt[0]);
    la_vreplvei_h(as, xd, xj, 0);
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vreplvei_h(as, vt[0], xj, 0);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));
    gen_reg_restore_vr(as, vt[0]);
}

void __gen_lasx_interpret_xvreplve0_q_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) xvreplve0_q %08x \n", instr);
    int excl_vr[] = { xd, xj };
    int vt[2];
    gen_reg_find_free_vrs(vt, 2, excl_vr, 2);
    gen_reg_save_vr(as, vt[0]);
    gen_reg_save_vr(as, vt[1]);
    la_vori_b(as, xd, xj, 0);
    la_vori_b(as, vt[0], xd, 0);
    la_vld(as, vt[1], LA_TP, TD_OFF_DATA(xj, 2));
    la_vori_b(as, vt[0], vt[0], 0);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));
    gen_reg_restore_vr(as, vt[0]);
    gen_reg_restore_vr(as, vt[1]);
}

void __gen_lasx_interpret_xvreplve0_w_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) xvreplve0_w %08x \n", instr);
    int excl_vr[] = { xd, xj };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2);
    gen_reg_save_vr(as, vt[0]);
    la_vreplvei_w(as, xd, xj, 0);
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vreplvei_w(as, vt[0], xj, 0);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));
    gen_reg_restore_vr(as, vt[0]);
}



/* --- FCC-output set instructions --- */
GEN_OPT_SET_FCC(xvsetallnez_b, vsetallnez_b, and)
GEN_OPT_SET_FCC(xvsetallnez_d, vsetallnez_d, and)
GEN_OPT_SET_FCC(xvsetallnez_h, vsetallnez_h, and)
GEN_OPT_SET_FCC(xvsetallnez_w, vsetallnez_w, and)
GEN_OPT_SET_FCC(xvseteqz_v, vseteqz_v, and)
GEN_OPT_SET_FCC(xvsetanyeqz_b, vsetanyeqz_b, or)
GEN_OPT_SET_FCC(xvsetanyeqz_d, vsetanyeqz_d, or)
GEN_OPT_SET_FCC(xvsetanyeqz_h, vsetanyeqz_h, or)
GEN_OPT_SET_FCC(xvsetanyeqz_w, vsetanyeqz_w, or)
GEN_OPT_SET_FCC(xvsetnez_v, vsetnez_v, or)

void __gen_lasx_interpret_xvst_opt(void *as, unsigned int instr)
{
    int rj = (instr >> 5) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    int off = (instr >> 10) & 0xfff;
    off = (off << 20) >> 20;
    tdlog("                 : interpret(optgen) xvst %08x \n", instr);

    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);

    gen_reg_save_vr(as, vt0);

    if (off >= -2048 && off <= 2047 - 16) {
        la_vst(as, xd, rj, off);
        la_vld(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
        la_vst(as, vt0, rj, off + 16);
    } else {
        int excl_gr[] = { rj };
        int t0;
        gen_reg_find_free_grs(&t0, 1, excl_gr, 1);
        gen_reg_save_gr(as, t0);
        la_load_immediate32(as, t0, off);
        la_add_d(as, t0, rj, t0);
        la_vst(as, xd, t0, 0);
        la_vld(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
        la_vst(as, vt0, t0, 16);
        gen_reg_restore_gr(as, t0);
    }
    gen_reg_restore_vr(as, vt0);
}

void __gen_lasx_interpret_xvstelm_b_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int si8 = (instr >> 10) & 0xff;
    si8 = (si8 << 24) >> 24;
    int idx = (instr >> 18) & 0x1f;
    tdlog("                 : interpret(optgen) xvstelm_b %08x \n", instr);

    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt0);

    if (idx >= 0x10) {
        la_vld(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
        la_vstelm_b(as, vt0, rj, si8, idx % 16);
    } else {
        la_vstelm_b(as, xd, rj, si8, idx % 16);
    }

    gen_reg_restore_vr(as, vt0);
}

void __gen_lasx_interpret_xvstelm_d_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int si8 = (instr >> 10) & 0xff;
    si8 = (si8 << 24) >> 24;
    int idx = (instr >> 18) & 0x3;
    tdlog("                 : interpret(optgen) xvstelm_d %08x \n", instr);

    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt0);

    if (idx >= 0x2) {
        la_vld(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
        la_vstelm_d(as, vt0, rj, si8 << 3, idx % 2);
    } else {
        la_vstelm_d(as, xd, rj, si8 << 3, idx % 2);
    }

    gen_reg_restore_vr(as, vt0);
}

void __gen_lasx_interpret_xvstelm_h_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int si8 = (instr >> 10) & 0xff;
    si8 = (si8 << 24) >> 24;
    int idx = (instr >> 18) & 0xf;
    tdlog("                 : interpret(optgen) xvstelm_h %08x \n", instr);

    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt0);

    if (idx >= 0x8) {
        la_vld(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
        la_vstelm_h(as, vt0, rj, si8 << 1, idx % 8);
    } else {
        la_vstelm_h(as, xd, rj, si8 << 1, idx % 8);
    }

    gen_reg_restore_vr(as, vt0);
}

void __gen_lasx_interpret_xvstelm_w_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int si8 = (instr >> 10) & 0xff;
    si8 = (si8 << 24) >> 24;
    int idx = (instr >> 18) & 0x7;
    tdlog("                 : interpret(optgen) xvstelm_w %08x \n", instr);

    int excl_vr[] = { xd };
    int vt0;
    gen_reg_find_free_vrs(&vt0, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt0);

    if (idx >= 0x4) {
        la_vld(as, vt0, LA_TP, TD_OFF_DATA(xd, 2));
        la_vstelm_w(as, vt0, rj, si8 << 2, idx % 4);
    } else {
        la_vstelm_w(as, xd, rj, si8 << 2, idx % 4);
    }

    gen_reg_restore_vr(as, vt0);
}

void __gen_lasx_interpret_xvstx_opt(void *as, unsigned int instr)
{
    int rj = (instr >> 5) & 0x1f;
    int rk = (instr >> 10) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    tdlog("                 : interpret(optgen) xvstx %08x \n", instr);

    int excl_gr[] = { rj, rk };
    int t0;
    gen_reg_find_free_grs(&t0, 1, excl_gr, 2);
    gen_reg_save_gr(as, t0);

    la_add_d(as, t0, rj, rk);
    la_vst(as, xd, t0, 0);
    la_vld(as, xd, LA_TP, TD_OFF_DATA(xd, 2));
    la_vst(as, xd, t0, 16);

    gen_reg_restore_gr(as, t0);
}

/* ================================================================
 * Macro-generated implementations
 * ================================================================ */

/* --- 3-operand (xd, xj, xk) --- */
GEN_OPT_3OP(xvabsd_b, vabsd_b)
GEN_OPT_3OP(xvabsd_bu, vabsd_bu)
GEN_OPT_3OP(xvabsd_d, vabsd_d)
GEN_OPT_3OP(xvabsd_du, vabsd_du)
GEN_OPT_3OP(xvabsd_h, vabsd_h)
GEN_OPT_3OP(xvabsd_hu, vabsd_hu)
GEN_OPT_3OP(xvabsd_w, vabsd_w)
GEN_OPT_3OP(xvabsd_wu, vabsd_wu)
GEN_OPT_3OP(xvadd_b, vadd_b)
GEN_OPT_3OP(xvadd_d, vadd_d)
GEN_OPT_3OP(xvadd_h, vadd_h)
GEN_OPT_3OP(xvadd_q, vadd_q)
GEN_OPT_3OP(xvadd_w, vadd_w)
GEN_OPT_3OP(xvadda_b, vadda_b)
GEN_OPT_3OP(xvadda_d, vadda_d)
GEN_OPT_3OP(xvadda_h, vadda_h)
GEN_OPT_3OP(xvadda_w, vadda_w)
GEN_OPT_3OP(xvaddwev_d_w, vaddwev_d_w)
GEN_OPT_3OP(xvaddwev_d_wu, vaddwev_d_wu)
GEN_OPT_3OP(xvaddwev_d_wu_w, vaddwev_d_wu_w)
GEN_OPT_3OP(xvaddwev_h_b, vaddwev_h_b)
GEN_OPT_3OP(xvaddwev_h_bu, vaddwev_h_bu)
GEN_OPT_3OP(xvaddwev_h_bu_b, vaddwev_h_bu_b)
GEN_OPT_3OP(xvaddwev_q_d, vaddwev_q_d)
GEN_OPT_3OP(xvaddwev_q_du, vaddwev_q_du)
GEN_OPT_3OP(xvaddwev_q_du_d, vaddwev_q_du_d)
GEN_OPT_3OP(xvaddwev_w_h, vaddwev_w_h)
GEN_OPT_3OP(xvaddwev_w_hu, vaddwev_w_hu)
GEN_OPT_3OP(xvaddwev_w_hu_h, vaddwev_w_hu_h)
GEN_OPT_3OP(xvaddwod_d_w, vaddwod_d_w)
GEN_OPT_3OP(xvaddwod_d_wu, vaddwod_d_wu)
GEN_OPT_3OP(xvaddwod_d_wu_w, vaddwod_d_wu_w)
GEN_OPT_3OP(xvaddwod_h_b, vaddwod_h_b)
GEN_OPT_3OP(xvaddwod_h_bu, vaddwod_h_bu)
GEN_OPT_3OP(xvaddwod_h_bu_b, vaddwod_h_bu_b)
GEN_OPT_3OP(xvaddwod_q_d, vaddwod_q_d)
GEN_OPT_3OP(xvaddwod_q_du, vaddwod_q_du)
GEN_OPT_3OP(xvaddwod_q_du_d, vaddwod_q_du_d)
GEN_OPT_3OP(xvaddwod_w_h, vaddwod_w_h)
GEN_OPT_3OP(xvaddwod_w_hu, vaddwod_w_hu)
GEN_OPT_3OP(xvaddwod_w_hu_h, vaddwod_w_hu_h)
GEN_OPT_3OP(xvand_v, vand_v)
GEN_OPT_3OP(xvandn_v, vandn_v)
GEN_OPT_3OP(xvavg_b, vavg_b)
GEN_OPT_3OP(xvavg_bu, vavg_bu)
GEN_OPT_3OP(xvavg_d, vavg_d)
GEN_OPT_3OP(xvavg_du, vavg_du)
GEN_OPT_3OP(xvavg_h, vavg_h)
GEN_OPT_3OP(xvavg_hu, vavg_hu)
GEN_OPT_3OP(xvavg_w, vavg_w)
GEN_OPT_3OP(xvavg_wu, vavg_wu)
GEN_OPT_3OP(xvavgr_b, vavgr_b)
GEN_OPT_3OP(xvavgr_bu, vavgr_bu)
GEN_OPT_3OP(xvavgr_d, vavgr_d)
GEN_OPT_3OP(xvavgr_du, vavgr_du)
GEN_OPT_3OP(xvavgr_h, vavgr_h)
GEN_OPT_3OP(xvavgr_hu, vavgr_hu)
GEN_OPT_3OP(xvavgr_w, vavgr_w)
GEN_OPT_3OP(xvavgr_wu, vavgr_wu)
GEN_OPT_3OP(xvbitclr_b, vbitclr_b)
GEN_OPT_3OP(xvbitclr_d, vbitclr_d)
GEN_OPT_3OP(xvbitclr_h, vbitclr_h)
GEN_OPT_3OP(xvbitclr_w, vbitclr_w)
GEN_OPT_3OP(xvbitrev_b, vbitrev_b)
GEN_OPT_3OP(xvbitrev_d, vbitrev_d)
GEN_OPT_3OP(xvbitrev_h, vbitrev_h)
GEN_OPT_3OP(xvbitrev_w, vbitrev_w)
GEN_OPT_3OP(xvbitset_b, vbitset_b)
GEN_OPT_3OP(xvbitset_d, vbitset_d)
GEN_OPT_3OP(xvbitset_h, vbitset_h)
GEN_OPT_3OP(xvbitset_w, vbitset_w)
GEN_OPT_3OP(xvdiv_b, vdiv_b)
GEN_OPT_3OP(xvdiv_bu, vdiv_bu)
GEN_OPT_3OP(xvdiv_d, vdiv_d)
GEN_OPT_3OP(xvdiv_du, vdiv_du)
GEN_OPT_3OP(xvdiv_h, vdiv_h)
GEN_OPT_3OP(xvdiv_hu, vdiv_hu)
GEN_OPT_3OP(xvdiv_w, vdiv_w)
GEN_OPT_3OP(xvdiv_wu, vdiv_wu)
GEN_OPT_3OP(xvfadd_d, vfadd_d)
GEN_OPT_3OP(xvfadd_s, vfadd_s)
GEN_OPT_3OP(xvfcvt_h_s, vfcvt_h_s)
GEN_OPT_3OP(xvfcvt_s_d, vfcvt_s_d)
GEN_OPT_3OP(xvfdiv_d, vfdiv_d)
GEN_OPT_3OP(xvfdiv_s, vfdiv_s)
GEN_OPT_3OP(xvffint_s_l, vffint_s_l)
GEN_OPT_3OP(xvfmax_d, vfmax_d)
GEN_OPT_3OP(xvfmax_s, vfmax_s)
GEN_OPT_3OP(xvfmaxa_d, vfmaxa_d)
GEN_OPT_3OP(xvfmaxa_s, vfmaxa_s)
GEN_OPT_3OP(xvfmin_d, vfmin_d)
GEN_OPT_3OP(xvfmin_s, vfmin_s)
GEN_OPT_3OP(xvfmina_d, vfmina_d)
GEN_OPT_3OP(xvfmina_s, vfmina_s)
GEN_OPT_3OP(xvfmul_d, vfmul_d)
GEN_OPT_3OP(xvfmul_s, vfmul_s)
GEN_OPT_3OP_RMW(xvfrstp_b, vfrstp_b)
GEN_OPT_3OP_RMW(xvfrstp_h, vfrstp_h)
GEN_OPT_3OP(xvfsub_d, vfsub_d)
GEN_OPT_3OP(xvfsub_s, vfsub_s)
GEN_OPT_3OP(xvftint_w_d, vftint_w_d)
GEN_OPT_3OP(xvftintrm_w_d, vftintrm_w_d)
GEN_OPT_3OP(xvftintrne_w_d, vftintrne_w_d)
GEN_OPT_3OP(xvftintrp_w_d, vftintrp_w_d)
GEN_OPT_3OP(xvftintrz_w_d, vftintrz_w_d)
GEN_OPT_3OP(xvhaddw_d_w, vhaddw_d_w)
GEN_OPT_3OP(xvhaddw_du_wu, vhaddw_du_wu)
GEN_OPT_3OP(xvhaddw_h_b, vhaddw_h_b)
GEN_OPT_3OP(xvhaddw_hu_bu, vhaddw_hu_bu)
GEN_OPT_3OP(xvhaddw_q_d, vhaddw_q_d)
GEN_OPT_3OP(xvhaddw_qu_du, vhaddw_qu_du)
GEN_OPT_3OP(xvhaddw_w_h, vhaddw_w_h)
GEN_OPT_3OP(xvhaddw_wu_hu, vhaddw_wu_hu)
GEN_OPT_3OP(xvhsubw_d_w, vhsubw_d_w)
GEN_OPT_3OP(xvhsubw_du_wu, vhsubw_du_wu)
GEN_OPT_3OP(xvhsubw_h_b, vhsubw_h_b)
GEN_OPT_3OP(xvhsubw_hu_bu, vhsubw_hu_bu)
GEN_OPT_3OP(xvhsubw_q_d, vhsubw_q_d)
GEN_OPT_3OP(xvhsubw_qu_du, vhsubw_qu_du)
GEN_OPT_3OP(xvhsubw_w_h, vhsubw_w_h)
GEN_OPT_3OP(xvhsubw_wu_hu, vhsubw_wu_hu)
GEN_OPT_3OP(xvilvh_b, vilvh_b)
GEN_OPT_3OP(xvilvh_d, vilvh_d)
GEN_OPT_3OP(xvilvh_h, vilvh_h)
GEN_OPT_3OP(xvilvh_w, vilvh_w)
GEN_OPT_3OP(xvilvl_b, vilvl_b)
GEN_OPT_3OP(xvilvl_d, vilvl_d)
GEN_OPT_3OP(xvilvl_h, vilvl_h)
GEN_OPT_3OP(xvilvl_w, vilvl_w)
GEN_OPT_3OP(xvmax_b, vmax_b)
GEN_OPT_3OP(xvmax_bu, vmax_bu)
GEN_OPT_3OP(xvmax_d, vmax_d)
GEN_OPT_3OP(xvmax_du, vmax_du)
GEN_OPT_3OP(xvmax_h, vmax_h)
GEN_OPT_3OP(xvmax_hu, vmax_hu)
GEN_OPT_3OP(xvmax_w, vmax_w)
GEN_OPT_3OP(xvmax_wu, vmax_wu)
GEN_OPT_3OP(xvmin_b, vmin_b)
GEN_OPT_3OP(xvmin_bu, vmin_bu)
GEN_OPT_3OP(xvmin_d, vmin_d)
GEN_OPT_3OP(xvmin_du, vmin_du)
GEN_OPT_3OP(xvmin_h, vmin_h)
GEN_OPT_3OP(xvmin_hu, vmin_hu)
GEN_OPT_3OP(xvmin_w, vmin_w)
GEN_OPT_3OP(xvmin_wu, vmin_wu)
GEN_OPT_3OP(xvmod_b, vmod_b)
GEN_OPT_3OP(xvmod_bu, vmod_bu)
GEN_OPT_3OP(xvmod_d, vmod_d)
GEN_OPT_3OP(xvmod_du, vmod_du)
GEN_OPT_3OP(xvmod_h, vmod_h)
GEN_OPT_3OP(xvmod_hu, vmod_hu)
GEN_OPT_3OP(xvmod_w, vmod_w)
GEN_OPT_3OP(xvmod_wu, vmod_wu)
GEN_OPT_3OP(xvmuh_b, vmuh_b)
GEN_OPT_3OP(xvmuh_bu, vmuh_bu)
GEN_OPT_3OP(xvmuh_d, vmuh_d)
GEN_OPT_3OP(xvmuh_du, vmuh_du)
GEN_OPT_3OP(xvmuh_h, vmuh_h)
GEN_OPT_3OP(xvmuh_hu, vmuh_hu)
GEN_OPT_3OP(xvmuh_w, vmuh_w)
GEN_OPT_3OP(xvmuh_wu, vmuh_wu)
GEN_OPT_3OP(xvmul_b, vmul_b)
GEN_OPT_3OP(xvmul_d, vmul_d)
GEN_OPT_3OP(xvmul_h, vmul_h)
GEN_OPT_3OP(xvmul_w, vmul_w)
GEN_OPT_3OP(xvmulwev_d_w, vmulwev_d_w)
GEN_OPT_3OP(xvmulwev_d_wu, vmulwev_d_wu)
GEN_OPT_3OP(xvmulwev_d_wu_w, vmulwev_d_wu_w)
GEN_OPT_3OP(xvmulwev_h_b, vmulwev_h_b)
GEN_OPT_3OP(xvmulwev_h_bu, vmulwev_h_bu)
GEN_OPT_3OP(xvmulwev_h_bu_b, vmulwev_h_bu_b)
GEN_OPT_3OP(xvmulwev_q_d, vmulwev_q_d)
GEN_OPT_3OP(xvmulwev_q_du, vmulwev_q_du)
GEN_OPT_3OP(xvmulwev_q_du_d, vmulwev_q_du_d)
GEN_OPT_3OP(xvmulwev_w_h, vmulwev_w_h)
GEN_OPT_3OP(xvmulwev_w_hu, vmulwev_w_hu)
GEN_OPT_3OP(xvmulwev_w_hu_h, vmulwev_w_hu_h)
GEN_OPT_3OP(xvmulwod_d_w, vmulwod_d_w)
GEN_OPT_3OP(xvmulwod_d_wu, vmulwod_d_wu)
GEN_OPT_3OP(xvmulwod_d_wu_w, vmulwod_d_wu_w)
GEN_OPT_3OP(xvmulwod_h_b, vmulwod_h_b)
GEN_OPT_3OP(xvmulwod_h_bu, vmulwod_h_bu)
GEN_OPT_3OP(xvmulwod_h_bu_b, vmulwod_h_bu_b)
GEN_OPT_3OP(xvmulwod_q_d, vmulwod_q_d)
GEN_OPT_3OP(xvmulwod_q_du, vmulwod_q_du)
GEN_OPT_3OP(xvmulwod_q_du_d, vmulwod_q_du_d)
GEN_OPT_3OP(xvmulwod_w_h, vmulwod_w_h)
GEN_OPT_3OP(xvmulwod_w_hu, vmulwod_w_hu)
GEN_OPT_3OP(xvmulwod_w_hu_h, vmulwod_w_hu_h)
GEN_OPT_3OP(xvnor_v, vnor_v)
GEN_OPT_3OP(xvor_v, vor_v)
GEN_OPT_3OP(xvorn_v, vorn_v)
GEN_OPT_3OP(xvpackev_b, vpackev_b)
GEN_OPT_3OP(xvpackev_d, vpackev_d)
GEN_OPT_3OP(xvpackev_h, vpackev_h)
GEN_OPT_3OP(xvpackev_w, vpackev_w)
GEN_OPT_3OP(xvpackod_b, vpackod_b)
GEN_OPT_3OP(xvpackod_d, vpackod_d)
GEN_OPT_3OP(xvpackod_h, vpackod_h)
GEN_OPT_3OP(xvpackod_w, vpackod_w)
GEN_OPT_3OP(xvpickev_b, vpickev_b)
GEN_OPT_3OP(xvpickev_d, vpickev_d)
GEN_OPT_3OP(xvpickev_h, vpickev_h)
GEN_OPT_3OP(xvpickev_w, vpickev_w)
GEN_OPT_3OP(xvpickod_b, vpickod_b)
GEN_OPT_3OP(xvpickod_d, vpickod_d)
GEN_OPT_3OP(xvpickod_h, vpickod_h)
GEN_OPT_3OP(xvpickod_w, vpickod_w)
GEN_OPT_3OP(xvrotr_b, vrotr_b)
GEN_OPT_3OP(xvrotr_d, vrotr_d)
GEN_OPT_3OP(xvrotr_h, vrotr_h)
GEN_OPT_3OP(xvrotr_w, vrotr_w)
GEN_OPT_3OP(xvsadd_b, vsadd_b)
GEN_OPT_3OP(xvsadd_bu, vsadd_bu)
GEN_OPT_3OP(xvsadd_d, vsadd_d)
GEN_OPT_3OP(xvsadd_du, vsadd_du)
GEN_OPT_3OP(xvsadd_h, vsadd_h)
GEN_OPT_3OP(xvsadd_hu, vsadd_hu)
GEN_OPT_3OP(xvsadd_w, vsadd_w)
GEN_OPT_3OP(xvsadd_wu, vsadd_wu)
GEN_OPT_3OP(xvseq_b, vseq_b)
GEN_OPT_3OP(xvseq_d, vseq_d)
GEN_OPT_3OP(xvseq_h, vseq_h)
GEN_OPT_3OP(xvseq_w, vseq_w)
GEN_OPT_3OP(xvsigncov_b, vsigncov_b)
GEN_OPT_3OP(xvsigncov_d, vsigncov_d)
GEN_OPT_3OP(xvsigncov_h, vsigncov_h)
GEN_OPT_3OP(xvsigncov_w, vsigncov_w)
GEN_OPT_3OP(xvsle_b, vsle_b)
GEN_OPT_3OP(xvsle_bu, vsle_bu)
GEN_OPT_3OP(xvsle_d, vsle_d)
GEN_OPT_3OP(xvsle_du, vsle_du)
GEN_OPT_3OP(xvsle_h, vsle_h)
GEN_OPT_3OP(xvsle_hu, vsle_hu)
GEN_OPT_3OP(xvsle_w, vsle_w)
GEN_OPT_3OP(xvsle_wu, vsle_wu)
GEN_OPT_3OP(xvsll_b, vsll_b)
GEN_OPT_3OP(xvsll_d, vsll_d)
GEN_OPT_3OP(xvsll_h, vsll_h)
GEN_OPT_3OP(xvsll_w, vsll_w)
GEN_OPT_3OP(xvslt_b, vslt_b)
GEN_OPT_3OP(xvslt_bu, vslt_bu)
GEN_OPT_3OP(xvslt_d, vslt_d)
GEN_OPT_3OP(xvslt_du, vslt_du)
GEN_OPT_3OP(xvslt_h, vslt_h)
GEN_OPT_3OP(xvslt_hu, vslt_hu)
GEN_OPT_3OP(xvslt_w, vslt_w)
GEN_OPT_3OP(xvslt_wu, vslt_wu)
GEN_OPT_3OP(xvsra_b, vsra_b)
GEN_OPT_3OP(xvsra_d, vsra_d)
GEN_OPT_3OP(xvsra_h, vsra_h)
GEN_OPT_3OP(xvsra_w, vsra_w)
GEN_OPT_3OP(xvsran_b_h, vsran_b_h)
GEN_OPT_3OP(xvsran_h_w, vsran_h_w)
GEN_OPT_3OP(xvsran_w_d, vsran_w_d)
GEN_OPT_3OP(xvsrar_b, vsrar_b)
GEN_OPT_3OP(xvsrar_d, vsrar_d)
GEN_OPT_3OP(xvsrar_h, vsrar_h)
GEN_OPT_3OP(xvsrar_w, vsrar_w)
GEN_OPT_3OP(xvsrarn_b_h, vsrarn_b_h)
GEN_OPT_3OP(xvsrarn_h_w, vsrarn_h_w)
GEN_OPT_3OP(xvsrarn_w_d, vsrarn_w_d)
GEN_OPT_3OP(xvsrl_b, vsrl_b)
GEN_OPT_3OP(xvsrl_d, vsrl_d)
GEN_OPT_3OP(xvsrl_h, vsrl_h)
GEN_OPT_3OP(xvsrl_w, vsrl_w)
GEN_OPT_3OP(xvsrln_b_h, vsrln_b_h)
GEN_OPT_3OP(xvsrln_h_w, vsrln_h_w)
GEN_OPT_3OP(xvsrln_w_d, vsrln_w_d)
GEN_OPT_3OP(xvsrlr_b, vsrlr_b)
GEN_OPT_3OP(xvsrlr_d, vsrlr_d)
GEN_OPT_3OP(xvsrlr_h, vsrlr_h)
GEN_OPT_3OP(xvsrlr_w, vsrlr_w)
GEN_OPT_3OP(xvsrlrn_b_h, vsrlrn_b_h)
GEN_OPT_3OP(xvsrlrn_h_w, vsrlrn_h_w)
GEN_OPT_3OP(xvsrlrn_w_d, vsrlrn_w_d)
GEN_OPT_3OP(xvssran_b_h, vssran_b_h)
GEN_OPT_3OP(xvssran_bu_h, vssran_bu_h)
GEN_OPT_3OP(xvssran_h_w, vssran_h_w)
GEN_OPT_3OP(xvssran_hu_w, vssran_hu_w)
GEN_OPT_3OP(xvssran_w_d, vssran_w_d)
GEN_OPT_3OP(xvssran_wu_d, vssran_wu_d)
GEN_OPT_3OP(xvssrarn_b_h, vssrarn_b_h)
GEN_OPT_3OP(xvssrarn_bu_h, vssrarn_bu_h)
GEN_OPT_3OP(xvssrarn_h_w, vssrarn_h_w)
GEN_OPT_3OP(xvssrarn_hu_w, vssrarn_hu_w)
GEN_OPT_3OP(xvssrarn_w_d, vssrarn_w_d)
GEN_OPT_3OP(xvssrarn_wu_d, vssrarn_wu_d)
GEN_OPT_3OP(xvssrln_b_h, vssrln_b_h)
GEN_OPT_3OP(xvssrln_bu_h, vssrln_bu_h)
GEN_OPT_3OP(xvssrln_h_w, vssrln_h_w)
GEN_OPT_3OP(xvssrln_hu_w, vssrln_hu_w)
GEN_OPT_3OP(xvssrln_w_d, vssrln_w_d)
GEN_OPT_3OP(xvssrln_wu_d, vssrln_wu_d)
GEN_OPT_3OP(xvssrlrn_b_h, vssrlrn_b_h)
GEN_OPT_3OP(xvssrlrn_bu_h, vssrlrn_bu_h)
GEN_OPT_3OP(xvssrlrn_h_w, vssrlrn_h_w)
GEN_OPT_3OP(xvssrlrn_hu_w, vssrlrn_hu_w)
GEN_OPT_3OP(xvssrlrn_w_d, vssrlrn_w_d)
GEN_OPT_3OP(xvssrlrn_wu_d, vssrlrn_wu_d)
GEN_OPT_3OP(xvssub_b, vssub_b)
GEN_OPT_3OP(xvssub_bu, vssub_bu)
GEN_OPT_3OP(xvssub_d, vssub_d)
GEN_OPT_3OP(xvssub_du, vssub_du)
GEN_OPT_3OP(xvssub_h, vssub_h)
GEN_OPT_3OP(xvssub_hu, vssub_hu)
GEN_OPT_3OP(xvssub_w, vssub_w)
GEN_OPT_3OP(xvssub_wu, vssub_wu)
GEN_OPT_3OP(xvsub_b, vsub_b)
GEN_OPT_3OP(xvsub_d, vsub_d)
GEN_OPT_3OP(xvsub_h, vsub_h)
GEN_OPT_3OP(xvsub_q, vsub_q)
GEN_OPT_3OP(xvsub_w, vsub_w)
GEN_OPT_3OP(xvsubwev_d_w, vsubwev_d_w)
GEN_OPT_3OP(xvsubwev_d_wu, vsubwev_d_wu)
GEN_OPT_3OP(xvsubwev_h_b, vsubwev_h_b)
GEN_OPT_3OP(xvsubwev_h_bu, vsubwev_h_bu)
GEN_OPT_3OP(xvsubwev_q_d, vsubwev_q_d)
GEN_OPT_3OP(xvsubwev_q_du, vsubwev_q_du)
GEN_OPT_3OP(xvsubwev_w_h, vsubwev_w_h)
GEN_OPT_3OP(xvsubwev_w_hu, vsubwev_w_hu)
GEN_OPT_3OP(xvsubwod_d_w, vsubwod_d_w)
GEN_OPT_3OP(xvsubwod_d_wu, vsubwod_d_wu)
GEN_OPT_3OP(xvsubwod_h_b, vsubwod_h_b)
GEN_OPT_3OP(xvsubwod_h_bu, vsubwod_h_bu)
GEN_OPT_3OP(xvsubwod_q_d, vsubwod_q_d)
GEN_OPT_3OP(xvsubwod_q_du, vsubwod_q_du)
GEN_OPT_3OP(xvsubwod_w_h, vsubwod_w_h)
GEN_OPT_3OP(xvsubwod_w_hu, vsubwod_w_hu)
GEN_OPT_3OP(xvxor_v, vxor_v)

/* --- 3-operand RMW (xd += xj * xk style) --- */
GEN_OPT_3OP_RMW(xvmadd_b, vmadd_b)
GEN_OPT_3OP_RMW(xvmadd_d, vmadd_d)
GEN_OPT_3OP_RMW(xvmadd_h, vmadd_h)
GEN_OPT_3OP_RMW(xvmadd_w, vmadd_w)
GEN_OPT_3OP_RMW(xvmaddwev_d_w, vmaddwev_d_w)
GEN_OPT_3OP_RMW(xvmaddwev_d_wu, vmaddwev_d_wu)
GEN_OPT_3OP_RMW(xvmaddwev_d_wu_w, vmaddwev_d_wu_w)
GEN_OPT_3OP_RMW(xvmaddwev_h_b, vmaddwev_h_b)
GEN_OPT_3OP_RMW(xvmaddwev_h_bu, vmaddwev_h_bu)
GEN_OPT_3OP_RMW(xvmaddwev_h_bu_b, vmaddwev_h_bu_b)
GEN_OPT_3OP_RMW(xvmaddwev_q_d, vmaddwev_q_d)
GEN_OPT_3OP_RMW(xvmaddwev_q_du, vmaddwev_q_du)
GEN_OPT_3OP_RMW(xvmaddwev_q_du_d, vmaddwev_q_du_d)
GEN_OPT_3OP_RMW(xvmaddwev_w_h, vmaddwev_w_h)
GEN_OPT_3OP_RMW(xvmaddwev_w_hu, vmaddwev_w_hu)
GEN_OPT_3OP_RMW(xvmaddwev_w_hu_h, vmaddwev_w_hu_h)
GEN_OPT_3OP_RMW(xvmaddwod_d_w, vmaddwod_d_w)
GEN_OPT_3OP_RMW(xvmaddwod_d_wu, vmaddwod_d_wu)
GEN_OPT_3OP_RMW(xvmaddwod_d_wu_w, vmaddwod_d_wu_w)
GEN_OPT_3OP_RMW(xvmaddwod_h_b, vmaddwod_h_b)
GEN_OPT_3OP_RMW(xvmaddwod_h_bu, vmaddwod_h_bu)
GEN_OPT_3OP_RMW(xvmaddwod_h_bu_b, vmaddwod_h_bu_b)
GEN_OPT_3OP_RMW(xvmaddwod_q_d, vmaddwod_q_d)
GEN_OPT_3OP_RMW(xvmaddwod_q_du, vmaddwod_q_du)
GEN_OPT_3OP_RMW(xvmaddwod_q_du_d, vmaddwod_q_du_d)
GEN_OPT_3OP_RMW(xvmaddwod_w_h, vmaddwod_w_h)
GEN_OPT_3OP_RMW(xvmaddwod_w_hu, vmaddwod_w_hu)
GEN_OPT_3OP_RMW(xvmaddwod_w_hu_h, vmaddwod_w_hu_h)
GEN_OPT_3OP_RMW(xvmsub_b, vmsub_b)
GEN_OPT_3OP_RMW(xvmsub_d, vmsub_d)
GEN_OPT_3OP_RMW(xvmsub_h, vmsub_h)
GEN_OPT_3OP_RMW(xvmsub_w, vmsub_w)

GEN_OPT_3OP_RMW(xvshuf_h, vshuf_h)
GEN_OPT_3OP_RMW(xvshuf_w, vshuf_w)
GEN_OPT_3OP_RMW(xvshuf_d, vshuf_d)

/* --- 2-operand (xd, xj) --- */
GEN_OPT_2OP(xvclo_b, vclo_b)
GEN_OPT_2OP(xvclo_d, vclo_d)
GEN_OPT_2OP(xvclo_h, vclo_h)
GEN_OPT_2OP(xvclo_w, vclo_w)
GEN_OPT_2OP(xvclz_b, vclz_b)
GEN_OPT_2OP(xvclz_d, vclz_d)
GEN_OPT_2OP(xvclz_h, vclz_h)
GEN_OPT_2OP(xvclz_w, vclz_w)
GEN_OPT_2OP(xvexth_d_w, vexth_d_w)
GEN_OPT_2OP(xvexth_du_wu, vexth_du_wu)
GEN_OPT_2OP(xvexth_h_b, vexth_h_b)
GEN_OPT_2OP(xvexth_hu_bu, vexth_hu_bu)
GEN_OPT_2OP(xvexth_q_d, vexth_q_d)
GEN_OPT_2OP(xvexth_qu_du, vexth_qu_du)
GEN_OPT_2OP(xvexth_w_h, vexth_w_h)
GEN_OPT_2OP(xvexth_wu_hu, vexth_wu_hu)
GEN_OPT_2OP(xvextl_q_d, vextl_q_d)
GEN_OPT_2OP(xvextl_qu_du, vextl_qu_du)
GEN_OPT_2OP(xvfclass_d, vfclass_d)
GEN_OPT_2OP(xvfclass_s, vfclass_s)
GEN_OPT_2OP(xvfcvth_d_s, vfcvth_d_s)
GEN_OPT_2OP(xvfcvth_s_h, vfcvth_s_h)
GEN_OPT_2OP(xvfcvtl_d_s, vfcvtl_d_s)
GEN_OPT_2OP(xvfcvtl_s_h, vfcvtl_s_h)
GEN_OPT_2OP(xvffint_d_l, vffint_d_l)
GEN_OPT_2OP(xvffint_d_lu, vffint_d_lu)
GEN_OPT_2OP(xvffint_s_w, vffint_s_w)
GEN_OPT_2OP(xvffint_s_wu, vffint_s_wu)
GEN_OPT_2OP(xvffinth_d_w, vffinth_d_w)
GEN_OPT_2OP(xvffintl_d_w, vffintl_d_w)
GEN_OPT_2OP(xvflogb_d, vflogb_d)
GEN_OPT_2OP(xvflogb_s, vflogb_s)
GEN_OPT_2OP(xvfrecip_d, vfrecip_d)
GEN_OPT_2OP(xvfrecip_s, vfrecip_s)
GEN_OPT_2OP(xvfrecipe_d, vfrecipe_d)
GEN_OPT_2OP(xvfrecipe_s, vfrecipe_s)
GEN_OPT_2OP(xvfrint_d, vfrint_d)
GEN_OPT_2OP(xvfrint_s, vfrint_s)
GEN_OPT_2OP(xvfrintrm_d, vfrintrm_d)
GEN_OPT_2OP(xvfrintrm_s, vfrintrm_s)
GEN_OPT_2OP(xvfrintrne_d, vfrintrne_d)
GEN_OPT_2OP(xvfrintrne_s, vfrintrne_s)
GEN_OPT_2OP(xvfrintrp_d, vfrintrp_d)
GEN_OPT_2OP(xvfrintrp_s, vfrintrp_s)
GEN_OPT_2OP(xvfrintrz_d, vfrintrz_d)
GEN_OPT_2OP(xvfrintrz_s, vfrintrz_s)
GEN_OPT_2OP(xvfrsqrt_d, vfrsqrt_d)
GEN_OPT_2OP(xvfrsqrt_s, vfrsqrt_s)
GEN_OPT_2OP(xvfrsqrte_d, vfrsqrte_d)
GEN_OPT_2OP(xvfrsqrte_s, vfrsqrte_s)
GEN_OPT_2OP(xvfsqrt_d, vfsqrt_d)
GEN_OPT_2OP(xvfsqrt_s, vfsqrt_s)
GEN_OPT_2OP(xvftint_l_d, vftint_l_d)
GEN_OPT_2OP(xvftint_lu_d, vftint_lu_d)
GEN_OPT_2OP(xvftint_w_s, vftint_w_s)
GEN_OPT_2OP(xvftint_wu_s, vftint_wu_s)
GEN_OPT_2OP(xvftinth_l_s, vftinth_l_s)
GEN_OPT_2OP(xvftintl_l_s, vftintl_l_s)
GEN_OPT_2OP(xvftintrm_l_d, vftintrm_l_d)
GEN_OPT_2OP(xvftintrm_w_s, vftintrm_w_s)
GEN_OPT_2OP(xvftintrmh_l_s, vftintrmh_l_s)
GEN_OPT_2OP(xvftintrml_l_s, vftintrml_l_s)
GEN_OPT_2OP(xvftintrne_l_d, vftintrne_l_d)
GEN_OPT_2OP(xvftintrne_w_s, vftintrne_w_s)
GEN_OPT_2OP(xvftintrneh_l_s, vftintrneh_l_s)
GEN_OPT_2OP(xvftintrnel_l_s, vftintrnel_l_s)
GEN_OPT_2OP(xvftintrp_l_d, vftintrp_l_d)
GEN_OPT_2OP(xvftintrp_w_s, vftintrp_w_s)
GEN_OPT_2OP(xvftintrph_l_s, vftintrph_l_s)
GEN_OPT_2OP(xvftintrpl_l_s, vftintrpl_l_s)
GEN_OPT_2OP(xvftintrz_l_d, vftintrz_l_d)
GEN_OPT_2OP(xvftintrz_lu_d, vftintrz_lu_d)
GEN_OPT_2OP(xvftintrz_w_s, vftintrz_w_s)
GEN_OPT_2OP(xvftintrz_wu_s, vftintrz_wu_s)
GEN_OPT_2OP(xvftintrzh_l_s, vftintrzh_l_s)
GEN_OPT_2OP(xvftintrzl_l_s, vftintrzl_l_s)
GEN_OPT_2OP(xvmskgez_b, vmskgez_b)
GEN_OPT_2OP(xvmskltz_b, vmskltz_b)
GEN_OPT_2OP(xvmskltz_d, vmskltz_d)
GEN_OPT_2OP(xvmskltz_h, vmskltz_h)
GEN_OPT_2OP(xvmskltz_w, vmskltz_w)
GEN_OPT_2OP(xvmsknz_b, vmsknz_b)
GEN_OPT_2OP(xvneg_b, vneg_b)
GEN_OPT_2OP(xvneg_d, vneg_d)
GEN_OPT_2OP(xvneg_h, vneg_h)
GEN_OPT_2OP(xvneg_w, vneg_w)
GEN_OPT_2OP(xvpcnt_b, vpcnt_b)
GEN_OPT_2OP(xvpcnt_d, vpcnt_d)
GEN_OPT_2OP(xvpcnt_h, vpcnt_h)
GEN_OPT_2OP(xvpcnt_w, vpcnt_w)

/* --- 2-operand + immediate (xd, xj, imm) --- */
GEN_OPT_2OP_IMM(xvaddi_bu, vaddi_bu, 5, 10)
GEN_OPT_2OP_IMM(xvaddi_du, vaddi_du, 5, 10)
GEN_OPT_2OP_IMM(xvaddi_hu, vaddi_hu, 5, 10)
GEN_OPT_2OP_IMM(xvaddi_wu, vaddi_wu, 5, 10)
GEN_OPT_2OP_IMM(xvandi_b, vandi_b, 8, 10)
GEN_OPT_2OP_IMM(xvbitclri_b, vbitclri_b, 3, 10)
GEN_OPT_2OP_IMM(xvbitclri_d, vbitclri_d, 6, 10)
GEN_OPT_2OP_IMM(xvbitclri_h, vbitclri_h, 4, 10)
GEN_OPT_2OP_IMM(xvbitclri_w, vbitclri_w, 5, 10)
GEN_OPT_2OP_IMM(xvbitrevi_b, vbitrevi_b, 3, 10)
GEN_OPT_2OP_IMM(xvbitrevi_d, vbitrevi_d, 6, 10)
GEN_OPT_2OP_IMM(xvbitrevi_h, vbitrevi_h, 4, 10)
GEN_OPT_2OP_IMM(xvbitrevi_w, vbitrevi_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvbitseli_b, vbitseli_b, 8, 10)
GEN_OPT_2OP_IMM(xvbitseti_b, vbitseti_b, 3, 10)
GEN_OPT_2OP_IMM(xvbitseti_d, vbitseti_d, 6, 10)
GEN_OPT_2OP_IMM(xvbitseti_h, vbitseti_h, 4, 10)
GEN_OPT_2OP_IMM(xvbitseti_w, vbitseti_w, 5, 10)
GEN_OPT_2OP_IMM(xvbsll_v, vbsll_v, 5, 10)
GEN_OPT_2OP_IMM(xvbsrl_v, vbsrl_v, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvextrins_b, vextrins_b, 8, 10)
GEN_OPT_2OP_IMM_RMW(xvextrins_d, vextrins_d, 8, 10)
GEN_OPT_2OP_IMM_RMW(xvextrins_h, vextrins_h, 8, 10)
GEN_OPT_2OP_IMM_RMW(xvextrins_w, vextrins_w, 8, 10)
GEN_OPT_2OP_IMM_RMW(xvfrstpi_b, vfrstpi_b, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvfrstpi_h, vfrstpi_h, 5, 10)
GEN_OPT_2OP_IMM(xvmaxi_b, vmaxi_b, 5, 10)
GEN_OPT_2OP_IMM(xvmaxi_bu, vmaxi_bu, 5, 10)
GEN_OPT_2OP_IMM(xvmaxi_d, vmaxi_d, 5, 10)
GEN_OPT_2OP_IMM(xvmaxi_du, vmaxi_du, 5, 10)
GEN_OPT_2OP_IMM(xvmaxi_h, vmaxi_h, 5, 10)
GEN_OPT_2OP_IMM(xvmaxi_hu, vmaxi_hu, 5, 10)
GEN_OPT_2OP_IMM(xvmaxi_w, vmaxi_w, 5, 10)
GEN_OPT_2OP_IMM(xvmaxi_wu, vmaxi_wu, 5, 10)
GEN_OPT_2OP_IMM(xvmini_b, vmini_b, 5, 10)
GEN_OPT_2OP_IMM(xvmini_bu, vmini_bu, 5, 10)
GEN_OPT_2OP_IMM(xvmini_d, vmini_d, 5, 10)
GEN_OPT_2OP_IMM(xvmini_du, vmini_du, 5, 10)
GEN_OPT_2OP_IMM(xvmini_h, vmini_h, 5, 10)
GEN_OPT_2OP_IMM(xvmini_hu, vmini_hu, 5, 10)
GEN_OPT_2OP_IMM(xvmini_w, vmini_w, 5, 10)
GEN_OPT_2OP_IMM(xvmini_wu, vmini_wu, 5, 10)
GEN_OPT_2OP_IMM(xvnori_b, vnori_b, 8, 10)
GEN_OPT_2OP_IMM(xvori_b, vori_b, 8, 10)
GEN_OPT_2OP_IMM_RMW(xvpermi_w, vpermi_w, 8, 10)
GEN_OPT_2OP_IMM(xvrotri_b, vrotri_b, 3, 10)
GEN_OPT_2OP_IMM(xvrotri_d, vrotri_d, 6, 10)
GEN_OPT_2OP_IMM(xvrotri_h, vrotri_h, 4, 10)
GEN_OPT_2OP_IMM(xvrotri_w, vrotri_w, 5, 10)
GEN_OPT_2OP_IMM(xvsat_b, vsat_b, 3, 10)
GEN_OPT_2OP_IMM(xvsat_bu, vsat_bu, 3, 10)
GEN_OPT_2OP_IMM(xvsat_d, vsat_d, 6, 10)
GEN_OPT_2OP_IMM(xvsat_du, vsat_du, 6, 10)
GEN_OPT_2OP_IMM(xvsat_h, vsat_h, 4, 10)
GEN_OPT_2OP_IMM(xvsat_hu, vsat_hu, 4, 10)
GEN_OPT_2OP_IMM(xvsat_w, vsat_w, 5, 10)
GEN_OPT_2OP_IMM(xvsat_wu, vsat_wu, 5, 10)
GEN_OPT_2OP_IMM(xvseqi_b, vseqi_b, 5, 10)
GEN_OPT_2OP_IMM(xvseqi_d, vseqi_d, 5, 10)
GEN_OPT_2OP_IMM(xvseqi_h, vseqi_h, 5, 10)
GEN_OPT_2OP_IMM(xvseqi_w, vseqi_w, 5, 10)
GEN_OPT_2OP_IMM(xvshuf4i_b, vshuf4i_b, 8, 10)
GEN_OPT_2OP_IMM_RMW(xvshuf4i_d, vshuf4i_d, 8, 10)
GEN_OPT_2OP_IMM(xvshuf4i_h, vshuf4i_h, 8, 10)
GEN_OPT_2OP_IMM(xvshuf4i_w, vshuf4i_w, 8, 10)
GEN_OPT_2OP_IMM(xvslei_b, vslei_b, 5, 10)
GEN_OPT_2OP_IMM(xvslei_bu, vslei_bu, 5, 10)
GEN_OPT_2OP_IMM(xvslei_d, vslei_d, 5, 10)
GEN_OPT_2OP_IMM(xvslei_du, vslei_du, 5, 10)
GEN_OPT_2OP_IMM(xvslei_h, vslei_h, 5, 10)
GEN_OPT_2OP_IMM(xvslei_hu, vslei_hu, 5, 10)
GEN_OPT_2OP_IMM(xvslei_w, vslei_w, 5, 10)
GEN_OPT_2OP_IMM(xvslei_wu, vslei_wu, 5, 10)
GEN_OPT_2OP_IMM(xvslli_b, vslli_b, 3, 10)
GEN_OPT_2OP_IMM(xvslli_d, vslli_d, 6, 10)
GEN_OPT_2OP_IMM(xvslli_h, vslli_h, 4, 10)
GEN_OPT_2OP_IMM(xvslli_w, vslli_w, 5, 10)
GEN_OPT_2OP_IMM(xvsllwil_d_w, vsllwil_d_w, 5, 10)
GEN_OPT_2OP_IMM(xvsllwil_du_wu, vsllwil_du_wu, 5, 10)
GEN_OPT_2OP_IMM(xvsllwil_h_b, vsllwil_h_b, 3, 10)
GEN_OPT_2OP_IMM(xvsllwil_hu_bu, vsllwil_hu_bu, 3, 10)
GEN_OPT_2OP_IMM(xvsllwil_w_h, vsllwil_w_h, 4, 10)
GEN_OPT_2OP_IMM(xvsllwil_wu_hu, vsllwil_wu_hu, 4, 10)
GEN_OPT_2OP_IMM(xvslti_b, vslti_b, 5, 10)
GEN_OPT_2OP_IMM(xvslti_bu, vslti_bu, 5, 10)
GEN_OPT_2OP_IMM(xvslti_d, vslti_d, 5, 10)
GEN_OPT_2OP_IMM(xvslti_du, vslti_du, 5, 10)
GEN_OPT_2OP_IMM(xvslti_h, vslti_h, 5, 10)
GEN_OPT_2OP_IMM(xvslti_hu, vslti_hu, 5, 10)
GEN_OPT_2OP_IMM(xvslti_w, vslti_w, 5, 10)
GEN_OPT_2OP_IMM(xvslti_wu, vslti_wu, 5, 10)
GEN_OPT_2OP_IMM(xvsrai_b, vsrai_b, 3, 10)
GEN_OPT_2OP_IMM(xvsrai_d, vsrai_d, 6, 10)
GEN_OPT_2OP_IMM(xvsrai_h, vsrai_h, 4, 10)
GEN_OPT_2OP_IMM(xvsrai_w, vsrai_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvsrani_b_h, vsrani_b_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvsrani_d_q, vsrani_d_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvsrani_h_w, vsrani_h_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvsrani_w_d, vsrani_w_d, 6, 10)
GEN_OPT_2OP_IMM(xvsrari_b, vsrari_b, 3, 10)
GEN_OPT_2OP_IMM(xvsrari_d, vsrari_d, 6, 10)
GEN_OPT_2OP_IMM(xvsrari_h, vsrari_h, 4, 10)
GEN_OPT_2OP_IMM(xvsrari_w, vsrari_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvsrarni_b_h, vsrarni_b_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvsrarni_d_q, vsrarni_d_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvsrarni_h_w, vsrarni_h_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvsrarni_w_d, vsrarni_w_d, 6, 10)
GEN_OPT_2OP_IMM(xvsrli_b, vsrli_b, 3, 10)
GEN_OPT_2OP_IMM(xvsrli_d, vsrli_d, 6, 10)
GEN_OPT_2OP_IMM(xvsrli_h, vsrli_h, 4, 10)
GEN_OPT_2OP_IMM(xvsrli_w, vsrli_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvsrlni_b_h, vsrlni_b_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvsrlni_d_q, vsrlni_d_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvsrlni_h_w, vsrlni_h_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvsrlni_w_d, vsrlni_w_d, 6, 10)
GEN_OPT_2OP_IMM(xvsrlri_b, vsrlri_b, 3, 10)
GEN_OPT_2OP_IMM(xvsrlri_d, vsrlri_d, 6, 10)
GEN_OPT_2OP_IMM(xvsrlri_h, vsrlri_h, 4, 10)
GEN_OPT_2OP_IMM(xvsrlri_w, vsrlri_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvsrlrni_b_h, vsrlrni_b_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvsrlrni_d_q, vsrlrni_d_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvsrlrni_h_w, vsrlrni_h_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvsrlrni_w_d, vsrlrni_w_d, 6, 10)
GEN_OPT_2OP_IMM_RMW(xvssrani_b_h, vssrani_b_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvssrani_bu_h, vssrani_bu_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvssrani_d_q, vssrani_d_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvssrani_du_q, vssrani_du_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvssrani_h_w, vssrani_h_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvssrani_hu_w, vssrani_hu_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvssrani_w_d, vssrani_w_d, 6, 10)
GEN_OPT_2OP_IMM_RMW(xvssrani_wu_d, vssrani_wu_d, 6, 10)
GEN_OPT_2OP_IMM_RMW(xvssrarni_b_h, vssrarni_b_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvssrarni_bu_h, vssrarni_bu_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvssrarni_d_q, vssrarni_d_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvssrarni_du_q, vssrarni_du_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvssrarni_h_w, vssrarni_h_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvssrarni_hu_w, vssrarni_hu_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvssrarni_w_d, vssrarni_w_d, 6, 10)
GEN_OPT_2OP_IMM_RMW(xvssrarni_wu_d, vssrarni_wu_d, 6, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlni_b_h, vssrlni_b_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlni_bu_h, vssrlni_bu_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlni_d_q, vssrlni_d_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlni_du_q, vssrlni_du_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlni_h_w, vssrlni_h_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlni_hu_w, vssrlni_hu_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlni_w_d, vssrlni_w_d, 6, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlni_wu_d, vssrlni_wu_d, 6, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlrni_b_h, vssrlrni_b_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlrni_bu_h, vssrlrni_bu_h, 4, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlrni_d_q, vssrlrni_d_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlrni_du_q, vssrlrni_du_q, 7, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlrni_h_w, vssrlrni_h_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlrni_hu_w, vssrlrni_hu_w, 5, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlrni_w_d, vssrlrni_w_d, 6, 10)
GEN_OPT_2OP_IMM_RMW(xvssrlrni_wu_d, vssrlrni_wu_d, 6, 10)
GEN_OPT_2OP_IMM(xvsubi_bu, vsubi_bu, 5, 10)
GEN_OPT_2OP_IMM(xvsubi_du, vsubi_du, 5, 10)
GEN_OPT_2OP_IMM(xvsubi_hu, vsubi_hu, 5, 10)
GEN_OPT_2OP_IMM(xvsubi_wu, vsubi_wu, 5, 10)
GEN_OPT_2OP_IMM(xvxori_b, vxori_b, 8, 10)
/* --- 4-operand (xd, xj, xk, xa) --- */
GEN_OPT_4OP(xvbitsel_v, vbitsel_v)
GEN_OPT_4OP(xvshuf_b, vshuf_b)
GEN_OPT_4OP(xvfmadd_d, vfmadd_d)
GEN_OPT_4OP(xvfmadd_s, vfmadd_s)
GEN_OPT_4OP(xvfmsub_d, vfmsub_d)
GEN_OPT_4OP(xvfmsub_s, vfmsub_s)
GEN_OPT_4OP(xvfnmadd_d, vfnmadd_d)
GEN_OPT_4OP(xvfnmadd_s, vfnmadd_s)
GEN_OPT_4OP(xvfnmsub_d, vfnmsub_d)
GEN_OPT_4OP(xvfnmsub_s, vfnmsub_s)


/* ================================================================
 * Custom GPR-interaction implementations
 * ================================================================ */

/* xvreplgr2vr_d: replicate GPR to vector (no xj source) */
void __gen_lasx_interpret_xvreplgr2vr_d_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen) xvreplgr2vr_d %08x \n", instr);

    int excl_vr[] = { xd };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 1);
    gen_reg_save_vr(as, vt[0]);

    la_vreplgr2vr_d(as, xd, (la_gpr_t)rj);
    la_vreplgr2vr_d(as, vt[0], (la_gpr_t)rj);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);
}

/* xvinsgr2vr_d: insert GPR into vector element */
void __gen_lasx_interpret_xvinsgr2vr_d_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int idx = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen) xvinsgr2vr_d %08x \n", instr);

    if (idx < 2) {
        la_vinsgr2vr_d(as, xd, (la_gpr_t)rj, idx);
    } else {
        int excl_vr[] = { xd };
        int vt[1];
        gen_reg_find_free_vrs(vt, 1, excl_vr, 1);
        gen_reg_save_vr(as, vt[0]);
        la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));
        la_vinsgr2vr_d(as, vt[0], (la_gpr_t)rj, idx - 2);
        la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));
        gen_reg_restore_vr(as, vt[0]);
    }
}

/* xvreplve_b/h/w/d: replicate element from vector using GPR index */
#define GEN_OPT_2OP_GPR(name, lf, elem_bits) \
void __gen_lasx_interpret_##name##_opt(void *as, unsigned int instr) \
{ \
    int xd = (instr >> 0) & 0x1f; \
    int xj = (instr >> 5) & 0x1f; \
    int rk = (instr >> 10) & 0x1f; \
    tdlog("                 : interpret(optgen) " #name " %08x \\n", instr); \
    int excl_vr[] = { xd, xj }; \
    int vt[1]; \
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2); \
    gen_reg_save_vr(as, vt[0]); \
    la_##lf(as, xd, xj, (la_gpr_t)rk); \
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2)); \
    la_##lf(as, vt[0], vt[0], (la_gpr_t)rk); \
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2)); \
    gen_reg_restore_vr(as, vt[0]); \
}

GEN_OPT_2OP_GPR(xvreplve_b, vreplve_b, 5)
GEN_OPT_2OP_GPR(xvreplve_h, vreplve_h, 6)
GEN_OPT_2OP_GPR(xvreplve_w, vreplve_w, 7)
GEN_OPT_2OP_GPR(xvreplve_d, vreplve_d, 8)

/* xvrepl128vei_d: replicate 128-bit immediate to vector (ref: _w variant) */
void __gen_lasx_interpret_xvrepl128vei_d_opt(void *as, unsigned int instr)
{
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0x1;
    tdlog("                 : interpret(optgen) xvrepl128vei_d %08x \n", instr);

    int excl_vr[] = { xd, xj };
    int vt[1];
    gen_reg_find_free_vrs(vt, 1, excl_vr, 2);
    gen_reg_save_vr(as, vt[0]);

    la_vreplvei_d(as, xd, xj, imm);
    la_vld(as, vt[0], LA_TP, TD_OFF_DATA(xj, 2));
    la_vreplvei_d(as, vt[0], vt[0], imm);
    la_vst(as, vt[0], LA_TP, TD_OFF_DATA(xd, 2));

    gen_reg_restore_vr(as, vt[0]);
}

extern uint64_t tp_offset;

int lasx_interpret_inst_gen(void *as, unsigned int instr, int with_prologue)
{
    if (with_prologue) {
        la_pcaddi(as, LA_ZERO, 0);
        la_addi_d(as, LA_TP, LA_TP, tp_offset);
    }
    int ok = 0;
    switch (instr >> 22) {
    case OP10_XVLD: ok = 1; GEN_XVLD(as, instr); break;
    case OP10_XVST: ok = 1; GEN_XVST(as, instr); break;
    case OP10_XVLDREPL_B: ok = 1; GEN_XVLDREPL_B(as, instr); break;
    case OP10_XVSTELM_H: ok = 1; GEN_XVSTELM_H(as, instr); break;
    default: break;
    }
    switch (instr >> 21) {
    case OP11_XVSTELM_W: ok = 1; GEN_XVSTELM_W(as, instr); break;
    case OP11_XVLDREPL_H: ok = 1; GEN_XVLDREPL_H(as, instr); break;
    default: break; }
    switch (instr >> 20) {
    case OP12_XVBITSEL_V: ok = 1; GEN_XVBITSEL_V(as, instr); break;
    case OP12_XVFCMP_COND_S: ok = 1; GEN_XVFCMP_COND_S(as, instr); break;
    case OP12_XVFMADD_D: ok = 1; GEN_XVFMADD_D(as, instr); break;
    case OP12_XVFMADD_S: ok = 1; GEN_XVFMADD_S(as, instr); break;
    case OP12_XVFMSUB_S: ok = 1; GEN_XVFMSUB_S(as, instr); break;
    case OP12_XVFNMSUB_S: ok = 1; GEN_XVFNMSUB_S(as, instr); break;
    case OP12_XVLDREPL_W: ok = 1; GEN_XVLDREPL_W(as, instr); break;
    case OP12_XVSHUF_B: ok = 1; GEN_XVSHUF_B(as, instr); break;
    case OP12_XVSTELM_D: ok = 1; GEN_XVSTELM_D(as, instr); break;
    case OP12_XVFMSUB_D: ok = 1; GEN_XVFMSUB_D(as, instr); break;
    case OP12_XVFNMADD_S: ok = 1; GEN_XVFNMADD_S(as, instr); break;
    case OP12_XVFNMADD_D: ok = 1; GEN_XVFNMADD_D(as, instr); break;
    case OP12_XVFNMSUB_D: ok = 1; GEN_XVFNMSUB_D(as, instr); break;
    case OP12_XVFCMP_COND_D: ok = 1; GEN_XVFCMP_COND_D(as, instr); break;
    default: break;
    }
    switch (instr >> 19) {
    case OP13_XVLDREPL_D: ok = 1; GEN_XVLDREPL_D(as, instr); break;
    default: break; }
    switch (instr >> 18) {
    case OP14_XVANDI_B: ok = 1; GEN_XVANDI_B(as, instr); break;
    case OP14_XVORI_B: ok = 1; GEN_XVORI_B(as, instr); break;
    case OP14_XVPERMI_D: ok = 1; GEN_XVPERMI_D(as, instr); break;
    case OP14_XVPERMI_Q: ok = 1; GEN_XVPERMI_Q(as, instr); break;
    case OP14_XVPERMI_W: ok = 1; GEN_XVPERMI_W(as, instr); break;
    case OP14_XVXORI_B: ok = 1; GEN_XVXORI_B(as, instr); break;
    case OP14_XVNORI_B: ok = 1; GEN_XVNORI_B(as, instr); break;
    case OP14_XVLDI: ok = 1; GEN_XVLDI(as, instr); break;
    case OP14_XVSHUF4I_W: ok = 1; GEN_XVSHUF4I_W(as, instr); break;
    case OP14_XVSHUF4I_B: ok = 1; GEN_XVSHUF4I_B(as, instr); break;
    case OP14_XVSHUF4I_H: ok = 1; GEN_XVSHUF4I_H(as, instr); break;
    case OP14_XVSHUF4I_D: ok = 1; GEN_XVSHUF4I_D(as, instr); break;
    case OP14_XVEXTRINS_W: ok = 1; GEN_XVEXTRINS_W(as, instr); break;
    case OP14_XVEXTRINS_H: ok = 1; GEN_XVEXTRINS_H(as, instr); break;
    case OP14_XVEXTRINS_D: ok = 1; GEN_XVEXTRINS_D(as, instr); break;
    case OP14_XVEXTRINS_B: ok = 1; GEN_XVEXTRINS_B(as, instr); break;
    case OP14_XVBITSELI_B: ok = 1; GEN_XVBITSELI_B(as, instr); break;
    default: break;
    }
    switch (instr >> 16) {
    case OP16_XVSRLI_D: ok = 1; GEN_XVSRLI_D(as, instr); break;
    case OP16_XVBITCLRI_D: ok = 1; GEN_XVBITCLRI_D(as, instr); break;
    case OP16_XVBITREVI_D: ok = 1; GEN_XVBITREVI_D(as, instr); break;
    case OP16_XVBITSETI_D: ok = 1; GEN_XVBITSETI_D(as, instr); break;
    case OP16_XVSAT_D: ok = 1; GEN_XVSAT_D(as, instr); break;
    case OP16_XVSAT_DU: ok = 1; GEN_XVSAT_DU(as, instr); break;
    case OP16_XVSLLI_D: ok = 1; GEN_XVSLLI_D(as, instr); break;
    case OP16_XVSRAI_D: ok = 1; GEN_XVSRAI_D(as, instr); break;
    case OP16_XVSRANI_W_D: ok = 1; GEN_XVSRANI_W_D(as, instr); break;
    case OP16_XVSRARI_D: ok = 1; GEN_XVSRARI_D(as, instr); break;
    case OP16_XVSSRARNI_W_D: ok = 1; GEN_XVSSRARNI_W_D(as, instr); break;
    case OP16_XVSSRLNI_W_D: ok = 1; GEN_XVSSRLNI_W_D(as, instr); break;
    case OP16_XVSSRLRNI_WU_D: ok = 1; GEN_XVSSRLRNI_WU_D(as, instr); break;
    case OP16_XVSSRLRNI_W_D: ok = 1; GEN_XVSSRLRNI_W_D(as, instr); break;
    case OP16_XVSSRLNI_WU_D: ok = 1; GEN_XVSSRLNI_WU_D(as, instr); break;
    case OP16_XVSSRARNI_WU_D: ok = 1; GEN_XVSSRARNI_WU_D(as, instr); break;
    case OP16_XVSSRANI_WU_D: ok = 1; GEN_XVSSRANI_WU_D(as, instr); break;
    case OP16_XVSSRANI_W_D: ok = 1; GEN_XVSSRANI_W_D(as, instr); break;
    case OP16_XVSRLRNI_W_D: ok = 1; GEN_XVSRLRNI_W_D(as, instr); break;
    case OP16_XVSRLRI_D: ok = 1; GEN_XVSRLRI_D(as, instr); break;
    case OP16_XVSRLNI_W_D: ok = 1; GEN_XVSRLNI_W_D(as, instr); break;
    case OP16_XVSRARNI_W_D: ok = 1; GEN_XVSRARNI_W_D(as, instr); break;
    case OP16_XVROTRI_D: ok = 1; GEN_XVROTRI_D(as, instr); break;
    default: break;
    }
    switch (instr >> 15) {
    case OP17_XVADDI_HU: ok = 1; GEN_XVADDI_HU(as, instr); break;
    case OP17_XVADDI_WU: ok = 1; GEN_XVADDI_WU(as, instr); break;
    case OP17_XVADDWEV_H_BU: ok = 1; GEN_XVADDWEV_H_BU(as, instr); break;
    case OP17_XVADDWOD_H_BU: ok = 1; GEN_XVADDWOD_H_BU(as, instr); break;
    case OP17_XVADD_H: ok = 1; GEN_XVADD_H(as, instr); break;
    case OP17_XVADD_W: ok = 1; GEN_XVADD_W(as, instr); break;
    case OP17_XVAND_V: ok = 1; GEN_XVAND_V(as, instr); break;
    case OP17_XVAVGR_BU: ok = 1; GEN_XVAVGR_BU(as, instr); break;
    case OP17_XVAVGR_HU: ok = 1; GEN_XVAVGR_HU(as, instr); break;
    case OP17_XVAVGR_WU: ok = 1; GEN_XVAVGR_WU(as, instr); break;
    case OP17_XVBSRL_V: ok = 1; GEN_XVBSRL_V(as, instr); break;
    case OP17_XVFADD_D: ok = 1; GEN_XVFADD_D(as, instr); break;
    case OP17_XVFADD_S: ok = 1; GEN_XVFADD_S(as, instr); break;
    case OP17_XVFCVT_S_D: ok = 1; GEN_XVFCVT_S_D(as, instr); break;
    case OP17_XVFMAX_S: ok = 1; GEN_XVFMAX_S(as, instr); break;
    case OP17_XVFMIN_S: ok = 1; GEN_XVFMIN_S(as, instr); break;
    case OP17_XVFMUL_D: ok = 1; GEN_XVFMUL_D(as, instr); break;
    case OP17_XVFMUL_S: ok = 1; GEN_XVFMUL_S(as, instr); break;
    case OP17_XVFSUB_S: ok = 1; GEN_XVFSUB_S(as, instr); break;
    case OP17_XVHADDW_HU_BU: ok = 1; GEN_XVHADDW_HU_BU(as, instr); break;
    case OP17_XVHADDW_WU_HU: ok = 1; GEN_XVHADDW_WU_HU(as, instr); break;
    case OP17_XVILVH_B: ok = 1; GEN_XVILVH_B(as, instr); break;
    case OP17_XVILVH_D: ok = 1; GEN_XVILVH_D(as, instr); break;
    case OP17_XVILVH_H: ok = 1; GEN_XVILVH_H(as, instr); break;
    case OP17_XVILVH_W: ok = 1; GEN_XVILVH_W(as, instr); break;
    case OP17_XVILVL_B: ok = 1; GEN_XVILVL_B(as, instr); break;
    case OP17_XVILVL_D: ok = 1; GEN_XVILVL_D(as, instr); break;
    case OP17_XVILVL_H: ok = 1; GEN_XVILVL_H(as, instr); break;
    case OP17_XVILVL_W: ok = 1; GEN_XVILVL_W(as, instr); break;
    case OP17_XVLDX: ok = 1; GEN_XVLDX(as, instr); break;
    case OP17_XVMADDWEV_H_BU: ok = 1; GEN_XVMADDWEV_H_BU(as, instr); break;
    case OP17_XVMADDWOD_H_BU: ok = 1; GEN_XVMADDWOD_H_BU(as, instr); break;
    case OP17_XVMADDWOD_W_H: ok = 1; GEN_XVMADDWOD_W_H(as, instr); break;
    case OP17_XVMADD_H: ok = 1; GEN_XVMADD_H(as, instr); break;
    case OP17_XVMADD_W: ok = 1; GEN_XVMADD_W(as, instr); break;
    case OP17_XVMAXI_H: ok = 1; GEN_XVMAXI_H(as, instr); break;
    case OP17_XVMAXI_W: ok = 1; GEN_XVMAXI_W(as, instr); break;
    case OP17_XVMIN_WU: ok = 1; GEN_XVMIN_WU(as, instr); break;
    case OP17_XVMSUB_H: ok = 1; GEN_XVMSUB_H(as, instr); break;
    case OP17_XVMUH_HU: ok = 1; GEN_XVMUH_HU(as, instr); break;
    case OP17_XVMULWEV_H_BU: ok = 1; GEN_XVMULWEV_H_BU(as, instr); break;
    case OP17_XVMULWEV_W_H: ok = 1; GEN_XVMULWEV_W_H(as, instr); break;
    case OP17_XVMULWEV_W_HU: ok = 1; GEN_XVMULWEV_W_HU(as, instr); break;
    case OP17_XVMULWEV_W_HU_H: ok = 1; GEN_XVMULWEV_W_HU_H(as, instr); break;
    case OP17_XVMULWOD_H_BU: ok = 1; GEN_XVMULWOD_H_BU(as, instr); break;
    case OP17_XVMULWOD_W_H: ok = 1; GEN_XVMULWOD_W_H(as, instr); break;
    case OP17_XVMULWOD_W_HU: ok = 1; GEN_XVMULWOD_W_HU(as, instr); break;
    case OP17_XVMULWOD_W_HU_H: ok = 1; GEN_XVMULWOD_W_HU_H(as, instr); break;
    case OP17_XVMUL_H: ok = 1; GEN_XVMUL_H(as, instr); break;
    case OP17_XVMUL_W: ok = 1; GEN_XVMUL_W(as, instr); break;
    case OP17_XVNOR_V: ok = 1; GEN_XVNOR_V(as, instr); break;
    case OP17_XVOR_V: ok = 1; GEN_XVOR_V(as, instr); break;
    case OP17_XVPACKEV_B: ok = 1; GEN_XVPACKEV_B(as, instr); break;
    case OP17_XVPACKEV_H: ok = 1; GEN_XVPACKEV_H(as, instr); break;
    case OP17_XVPACKOD_B: ok = 1; GEN_XVPACKOD_B(as, instr); break;
    case OP17_XVPACKOD_H: ok = 1; GEN_XVPACKOD_H(as, instr); break;
    case OP17_XVPERM_W: ok = 1; GEN_XVPERM_W(as, instr); break;
    case OP17_XVPICKEV_B: ok = 1; GEN_XVPICKEV_B(as, instr); break;
    case OP17_XVPICKEV_H: ok = 1; GEN_XVPICKEV_H(as, instr); break;
    case OP17_XVPICKOD_B: ok = 1; GEN_XVPICKOD_B(as, instr); break;
    case OP17_XVPICKOD_H: ok = 1; GEN_XVPICKOD_H(as, instr); break;
    case OP17_XVSADD_BU: ok = 1; GEN_XVSADD_BU(as, instr); break;
    case OP17_XVSAT_WU: ok = 1; GEN_XVSAT_WU(as, instr); break;
    case OP17_XVSHUF_H: ok = 1; GEN_XVSHUF_H(as, instr); break;
    case OP17_XVSLLI_W: ok = 1; GEN_XVSLLI_W(as, instr); break;
    case OP17_XVSLL_B: ok = 1; GEN_XVSLL_B(as, instr); break;
    case OP17_XVSLT_W: ok = 1; GEN_XVSLT_W(as, instr); break;
    case OP17_XVSRAI_W: ok = 1; GEN_XVSRAI_W(as, instr); break;
    case OP17_XVSRARI_W: ok = 1; GEN_XVSRARI_W(as, instr); break;
    case OP17_XVSRLI_W: ok = 1; GEN_XVSRLI_W(as, instr); break;
    case OP17_XVSRL_B: ok = 1; GEN_XVSRL_B(as, instr); break;
    case OP17_XVSRL_W: ok = 1; GEN_XVSRL_W(as, instr); break;
    case OP17_XVSSRANI_H_W: ok = 1; GEN_XVSSRANI_H_W(as, instr); break;
    case OP17_XVSSUB_BU: ok = 1; GEN_XVSSUB_BU(as, instr); break;
    case OP17_XVSTX: ok = 1; GEN_XVSTX(as, instr); break;
    case OP17_XVSUB_B: ok = 1; GEN_XVSUB_B(as, instr); break;
    case OP17_XVSUB_H: ok = 1; GEN_XVSUB_H(as, instr); break;
    case OP17_XVSUB_W: ok = 1; GEN_XVSUB_W(as, instr); break;
    case OP17_XVXOR_V: ok = 1; GEN_XVXOR_V(as, instr); break;
    case OP17_XVADD_B: ok = 1; GEN_XVADD_B(as, instr); break;
    case OP17_XVADD_D: ok = 1; GEN_XVADD_D(as, instr); break;
    case OP17_XVADD_Q: ok = 1; GEN_XVADD_Q(as, instr); break;
    case OP17_XVSUB_D: ok = 1; GEN_XVSUB_D(as, instr); break;
    case OP17_XVSUB_Q: ok = 1; GEN_XVSUB_Q(as, instr); break;
    case OP17_XVANDN_V:ok = 1; GEN_XVANDN_V(as, instr); break;
    case OP17_XVORN_V: ok = 1; GEN_XVORN_V(as, instr); break;
    case OP17_XVMUL_B: ok = 1; GEN_XVMUL_B(as, instr); break;
    case OP17_XVMUL_D: ok = 1; GEN_XVMUL_D(as, instr); break;
    case OP17_XVMADD_B: ok = 1; GEN_XVMADD_B(as, instr); break;
    case OP17_XVMADD_D: ok = 1; GEN_XVMADD_D(as, instr); break;
    case OP17_XVFSUB_D: ok = 1; GEN_XVFSUB_D(as, instr); break;
    case OP17_XVFDIV_S: ok = 1; GEN_XVFDIV_S(as, instr); break;
    case OP17_XVFDIV_D: ok = 1; GEN_XVFDIV_D(as, instr); break;
    case OP17_XVFMAX_D: ok = 1; GEN_XVFMAX_D(as, instr); break;
    case OP17_XVFMIN_D: ok = 1; GEN_XVFMIN_D(as, instr); break;
    case OP17_XVAVGR_B: ok = 1; GEN_XVAVGR_B(as, instr); break;
    case OP17_XVAVGR_H: ok = 1; GEN_XVAVGR_H(as, instr); break;
    case OP17_XVAVGR_W: ok = 1; GEN_XVAVGR_W(as, instr); break;
    case OP17_XVAVGR_D: ok = 1; GEN_XVAVGR_D(as, instr); break;
    case OP17_XVAVGR_DU: ok = 1; GEN_XVAVGR_DU(as, instr); break;
    case OP17_XVPACKOD_W: ok = 1; GEN_XVPACKOD_W(as, instr); break;
    case OP17_XVPACKEV_W: ok = 1; GEN_XVPACKEV_W(as, instr); break;
    case OP17_XVABSD_BU: ok = 1; GEN_XVABSD_BU(as, instr); break;
    case OP17_XVHADDW_D_W: ok = 1; GEN_XVHADDW_D_W(as, instr); break;
    case OP17_XVHADDW_Q_D: ok = 1; GEN_XVHADDW_Q_D(as, instr); break;
    case OP17_XVADDA_H: ok = 1; GEN_XVADDA_H(as, instr); break;
    case OP17_XVADDA_W: ok = 1; GEN_XVADDA_W(as, instr); break;
    case OP17_XVSUBWEV_H_BU: ok = 1; GEN_XVSUBWEV_H_BU(as, instr); break;
    case OP17_XVSUBWOD_H_BU: ok = 1; GEN_XVSUBWOD_H_BU(as, instr); break;
    case OP17_XVSLEI_H: ok = 1; GEN_XVSLEI_H(as, instr); break;
    case OP17_XVSRANI_H_W: ok = 1; GEN_XVSRANI_H_W(as, instr); break;
    case OP17_XVHADDW_W_H: ok = 1; GEN_XVHADDW_W_H(as, instr); break;
    case OP17_XVMADDWEV_H_BU_B: ok = 1; GEN_XVMADDWEV_H_BU_B(as, instr); break;
    case OP17_XVMADDWEV_W_H: ok = 1; GEN_XVMADDWEV_W_H(as, instr); break;
    case OP17_XVMADDWOD_H_BU_B: ok = 1; GEN_XVMADDWOD_H_BU_B(as, instr); break;
    case OP17_XVMULWEV_H_BU_B: ok = 1; GEN_XVMULWEV_H_BU_B(as, instr); break;
    case OP17_XVADDWEV_W_H: ok = 1; GEN_XVADDWEV_W_H(as, instr); break;
    case OP17_XVADDI_DU: ok = 1; GEN_XVADDI_DU(as, instr); break;
    case OP17_XVMAX_H: ok = 1; GEN_XVMAX_H(as, instr); break;
    case OP17_XVMIN_H: ok = 1; GEN_XVMIN_H(as, instr); break;
    case OP17_XVSLE_BU: ok = 1; GEN_XVSLE_BU(as, instr); break;
    case OP17_XVSLT_BU: ok = 1; GEN_XVSLT_BU(as, instr); break;
    case OP17_XVSIGNCOV_H: ok = 1; GEN_XVSIGNCOV_H(as, instr); break;
    case OP17_XVSLL_W: ok = 1; GEN_XVSLL_W(as, instr); break;
    case OP17_XVSRAR_W: ok = 1; GEN_XVSRAR_W(as, instr); break;
    case OP17_XVSSRLNI_H_W: ok = 1; GEN_XVSSRLNI_H_W(as, instr); break;
    case OP17_XVHADDW_DU_WU: ok = 1; GEN_XVHADDW_DU_WU(as, instr); break;
    case OP17_XVHADDW_QU_DU: ok = 1; GEN_XVHADDW_QU_DU(as, instr); break;
    case OP17_XVHSUBW_HU_BU: ok = 1; GEN_XVHSUBW_HU_BU(as, instr); break;
    case OP17_XVSUBWEV_W_H: ok = 1; GEN_XVSUBWEV_W_H(as, instr); break;
    case OP17_XVPACKEV_D: ok = 1; GEN_XVPACKEV_D(as, instr); break;
    case OP17_XVPACKOD_D: ok = 1; GEN_XVPACKOD_D(as, instr); break;
    case OP17_XVPICKEV_W: ok = 1; GEN_XVPICKEV_W(as, instr); break;
    case OP17_XVSEQI_W: ok = 1; GEN_XVSEQI_W(as, instr); break;
    case OP17_XVSLL_H: ok = 1; GEN_XVSLL_H(as, instr); break;
    case OP17_XVSLTI_B: ok = 1; GEN_XVSLTI_B(as, instr); break;
    case OP17_XVSSRARNI_H_W: ok = 1; GEN_XVSSRARNI_H_W(as, instr); break;
    case OP17_XVSSRARNI_HU_W: ok = 1; GEN_XVSSRARNI_HU_W(as, instr); break;
    case OP17_XVSSRLNI_HU_W: ok = 1; GEN_XVSSRLNI_HU_W(as, instr); break;
    case OP17_XVADDI_BU: ok = 1; GEN_XVADDI_BU(as, instr); break;
    case OP17_XVABSD_B: ok = 1; GEN_XVABSD_B(as, instr); break;
    case OP17_XVABSD_D: ok = 1; GEN_XVABSD_D(as, instr); break;
    case OP17_XVABSD_DU: ok = 1; GEN_XVABSD_DU(as, instr); break;
    case OP17_XVABSD_H: ok = 1; GEN_XVABSD_H(as, instr); break;
    case OP17_XVABSD_HU: ok = 1; GEN_XVABSD_HU(as, instr); break;
    case OP17_XVABSD_W: ok = 1; GEN_XVABSD_W(as, instr); break;
    case OP17_XVABSD_WU: ok = 1; GEN_XVABSD_WU(as, instr); break;
    case OP17_XVADDA_B: ok = 1; GEN_XVADDA_B(as, instr); break;
    case OP17_XVADDA_D: ok = 1; GEN_XVADDA_D(as, instr); break;
    case OP17_XVADDWEV_D_W: ok = 1; GEN_XVADDWEV_D_W(as, instr); break;
    case OP17_XVADDWEV_D_WU: ok = 1; GEN_XVADDWEV_D_WU(as, instr); break;
    case OP17_XVADDWEV_D_WU_W: ok = 1; GEN_XVADDWEV_D_WU_W(as, instr); break;
    case OP17_XVADDWEV_H_B: ok = 1; GEN_XVADDWEV_H_B(as, instr); break;
    case OP17_XVADDWEV_H_BU_B: ok = 1; GEN_XVADDWEV_H_BU_B(as, instr); break;
    case OP17_XVADDWEV_Q_D: ok = 1; GEN_XVADDWEV_Q_D(as, instr); break;
    case OP17_XVADDWEV_Q_DU: ok = 1; GEN_XVADDWEV_Q_DU(as, instr); break;
    case OP17_XVADDWEV_Q_DU_D: ok = 1; GEN_XVADDWEV_Q_DU_D(as, instr); break;
    case OP17_XVADDWEV_W_HU: ok = 1; GEN_XVADDWEV_W_HU(as, instr); break;
    case OP17_XVADDWEV_W_HU_H: ok = 1; GEN_XVADDWEV_W_HU_H(as, instr); break;
    case OP17_XVADDWOD_D_W: ok = 1; GEN_XVADDWOD_D_W(as, instr); break;
    case OP17_XVADDWOD_D_WU: ok = 1; GEN_XVADDWOD_D_WU(as, instr); break;
    case OP17_XVADDWOD_D_WU_W: ok = 1; GEN_XVADDWOD_D_WU_W(as, instr); break;
    case OP17_XVADDWOD_H_B: ok = 1; GEN_XVADDWOD_H_B(as, instr); break;
    case OP17_XVADDWOD_H_BU_B: ok = 1; GEN_XVADDWOD_H_BU_B(as, instr); break;
    case OP17_XVADDWOD_Q_D: ok = 1; GEN_XVADDWOD_Q_D(as, instr); break;
    case OP17_XVADDWOD_Q_DU: ok = 1; GEN_XVADDWOD_Q_DU(as, instr); break;
    case OP17_XVADDWOD_Q_DU_D: ok = 1; GEN_XVADDWOD_Q_DU_D(as, instr); break;
    case OP17_XVADDWOD_W_H: ok = 1; GEN_XVADDWOD_W_H(as, instr); break;
    case OP17_XVADDWOD_W_HU: ok = 1; GEN_XVADDWOD_W_HU(as, instr); break;
    case OP17_XVADDWOD_W_HU_H: ok = 1; GEN_XVADDWOD_W_HU_H(as, instr); break;
    case OP17_XVAVG_B: ok = 1; GEN_XVAVG_B(as, instr); break;
    case OP17_XVAVG_BU: ok = 1; GEN_XVAVG_BU(as, instr); break;
    case OP17_XVAVG_D: ok = 1; GEN_XVAVG_D(as, instr); break;
    case OP17_XVAVG_DU: ok = 1; GEN_XVAVG_DU(as, instr); break;
    case OP17_XVAVG_H: ok = 1; GEN_XVAVG_H(as, instr); break;
    case OP17_XVAVG_HU: ok = 1; GEN_XVAVG_HU(as, instr); break;
    case OP17_XVAVG_W: ok = 1; GEN_XVAVG_W(as, instr); break;
    case OP17_XVAVG_WU: ok = 1; GEN_XVAVG_WU(as, instr); break;
    case OP17_XVBITCLRI_W: ok = 1; GEN_XVBITCLRI_W(as, instr); break;
    case OP17_XVBITREVI_W: ok = 1; GEN_XVBITREVI_W(as, instr); break;
    case OP17_XVBITSETI_W: ok = 1; GEN_XVBITSETI_W(as, instr); break;
    case OP17_XVBSLL_V: ok = 1; GEN_XVBSLL_V(as, instr); break;
    case OP17_XVHADDW_H_B: ok = 1; GEN_XVHADDW_H_B(as, instr); break;
    case OP17_XVHSUBW_D_W: ok = 1; GEN_XVHSUBW_D_W(as, instr); break;
    case OP17_XVHSUBW_DU_WU: ok = 1; GEN_XVHSUBW_DU_WU(as, instr); break;
    case OP17_XVHSUBW_H_B: ok = 1; GEN_XVHSUBW_H_B(as, instr); break;
    case OP17_XVHSUBW_Q_D: ok = 1; GEN_XVHSUBW_Q_D(as, instr); break;
    case OP17_XVHSUBW_QU_DU: ok = 1; GEN_XVHSUBW_QU_DU(as, instr); break;
    case OP17_XVHSUBW_W_H: ok = 1; GEN_XVHSUBW_W_H(as, instr); break;
    case OP17_XVHSUBW_WU_HU: ok = 1; GEN_XVHSUBW_WU_HU(as, instr); break;
    case OP17_XVMADDWEV_D_W: ok = 1; GEN_XVMADDWEV_D_W(as, instr); break;
    case OP17_XVMADDWEV_D_WU_W: ok = 1; GEN_XVMADDWEV_D_WU_W(as, instr); break;
    case OP17_XVMADDWEV_H_B: ok = 1; GEN_XVMADDWEV_H_B(as, instr); break;
    case OP17_XVMADDWEV_Q_D: ok = 1; GEN_XVMADDWEV_Q_D(as, instr); break;
    case OP17_XVMADDWEV_Q_DU_D: ok = 1; GEN_XVMADDWEV_Q_DU_D(as, instr); break;
    case OP17_XVMADDWEV_W_HU_H: ok = 1; GEN_XVMADDWEV_W_HU_H(as, instr); break;
    case OP17_XVMADDWOD_D_W: ok = 1; GEN_XVMADDWOD_D_W(as, instr); break;
    case OP17_XVMADDWOD_D_WU_W: ok = 1; GEN_XVMADDWOD_D_WU_W(as, instr); break;
    case OP17_XVMADDWOD_H_B: ok = 1; GEN_XVMADDWOD_H_B(as, instr); break;
    case OP17_XVMADDWOD_Q_D: ok = 1; GEN_XVMADDWOD_Q_D(as, instr); break;
    case OP17_XVMADDWOD_Q_DU_D: ok = 1; GEN_XVMADDWOD_Q_DU_D(as, instr); break;
    case OP17_XVMADDWOD_W_HU_H: ok = 1; GEN_XVMADDWOD_W_HU_H(as, instr); break;
    case OP17_XVMAX_B: ok = 1; GEN_XVMAX_B(as, instr); break;
    case OP17_XVMAX_BU: ok = 1; GEN_XVMAX_BU(as, instr); break;
    case OP17_XVMAX_D: ok = 1; GEN_XVMAX_D(as, instr); break;
    case OP17_XVMAX_DU: ok = 1; GEN_XVMAX_DU(as, instr); break;
    case OP17_XVMAX_HU: ok = 1; GEN_XVMAX_HU(as, instr); break;
    case OP17_XVMAX_W: ok = 1; GEN_XVMAX_W(as, instr); break;
    case OP17_XVMAX_WU: ok = 1; GEN_XVMAX_WU(as, instr); break;
    case OP17_XVMAXI_B: ok = 1; GEN_XVMAXI_B(as, instr); break;
    case OP17_XVMAXI_D: ok = 1; GEN_XVMAXI_D(as, instr); break;
    case OP17_XVMIN_B: ok = 1; GEN_XVMIN_B(as, instr); break;
    case OP17_XVMIN_BU: ok = 1; GEN_XVMIN_BU(as, instr); break;
    case OP17_XVMIN_D: ok = 1; GEN_XVMIN_D(as, instr); break;
    case OP17_XVMIN_DU: ok = 1; GEN_XVMIN_DU(as, instr); break;
    case OP17_XVMIN_HU: ok = 1; GEN_XVMIN_HU(as, instr); break;
    case OP17_XVMIN_W: ok = 1; GEN_XVMIN_W(as, instr); break;
    case OP17_XVMINI_B: ok = 1; GEN_XVMINI_B(as, instr); break;
    case OP17_XVMINI_D: ok = 1; GEN_XVMINI_D(as, instr); break;
    case OP17_XVMINI_H: ok = 1; GEN_XVMINI_H(as, instr); break;
    case OP17_XVMINI_W: ok = 1; GEN_XVMINI_W(as, instr); break;
    case OP17_XVMSUB_B: ok = 1; GEN_XVMSUB_B(as, instr); break;
    case OP17_XVMSUB_D: ok = 1; GEN_XVMSUB_D(as, instr); break;
    case OP17_XVMSUB_W: ok = 1; GEN_XVMSUB_W(as, instr); break;
    case OP17_XVMUH_B: ok = 1; GEN_XVMUH_B(as, instr); break;
    case OP17_XVMUH_BU: ok = 1; GEN_XVMUH_BU(as, instr); break;
    case OP17_XVMUH_D: ok = 1; GEN_XVMUH_D(as, instr); break;
    case OP17_XVMUH_DU: ok = 1; GEN_XVMUH_DU(as, instr); break;
    case OP17_XVMUH_H: ok = 1; GEN_XVMUH_H(as, instr); break;
    case OP17_XVMUH_W: ok = 1; GEN_XVMUH_W(as, instr); break;
    case OP17_XVMUH_WU: ok = 1; GEN_XVMUH_WU(as, instr); break;
    case OP17_XVMULWEV_D_W: ok = 1; GEN_XVMULWEV_D_W(as, instr); break;
    case OP17_XVMULWEV_D_WU: ok = 1; GEN_XVMULWEV_D_WU(as, instr); break;
    case OP17_XVMULWEV_D_WU_W: ok = 1; GEN_XVMULWEV_D_WU_W(as, instr); break;
    case OP17_XVMULWEV_H_B: ok = 1; GEN_XVMULWEV_H_B(as, instr); break;
    case OP17_XVMULWEV_Q_D: ok = 1; GEN_XVMULWEV_Q_D(as, instr); break;
    case OP17_XVMULWEV_Q_DU: ok = 1; GEN_XVMULWEV_Q_DU(as, instr); break;
    case OP17_XVMULWEV_Q_DU_D: ok = 1; GEN_XVMULWEV_Q_DU_D(as, instr); break;
    case OP17_XVMULWOD_D_W: ok = 1; GEN_XVMULWOD_D_W(as, instr); break;
    case OP17_XVMULWOD_D_WU: ok = 1; GEN_XVMULWOD_D_WU(as, instr); break;
    case OP17_XVMULWOD_D_WU_W: ok = 1; GEN_XVMULWOD_D_WU_W(as, instr); break;
    case OP17_XVMULWOD_H_B: ok = 1; GEN_XVMULWOD_H_B(as, instr); break;
    case OP17_XVMULWOD_H_BU_B: ok = 1; GEN_XVMULWOD_H_BU_B(as, instr); break;
    case OP17_XVMULWOD_Q_D: ok = 1; GEN_XVMULWOD_Q_D(as, instr); break;
    case OP17_XVMULWOD_Q_DU: ok = 1; GEN_XVMULWOD_Q_DU(as, instr); break;
    case OP17_XVMULWOD_Q_DU_D: ok = 1; GEN_XVMULWOD_Q_DU_D(as, instr); break;
    case OP17_XVPICKEV_D: ok = 1; GEN_XVPICKEV_D(as, instr); break;
    case OP17_XVSAT_W: ok = 1; GEN_XVSAT_W(as, instr); break;
    case OP17_XVSEQ_B: ok = 1; GEN_XVSEQ_B(as, instr); break;
    case OP17_XVSEQ_D: ok = 1; GEN_XVSEQ_D(as, instr); break;
    case OP17_XVSEQ_H: ok = 1; GEN_XVSEQ_H(as, instr); break;
    case OP17_XVSEQ_W: ok = 1; GEN_XVSEQ_W(as, instr); break;
    case OP17_XVSEQI_B: ok = 1; GEN_XVSEQI_B(as, instr); break;
    case OP17_XVSEQI_D: ok = 1; GEN_XVSEQI_D(as, instr); break;
    case OP17_XVSEQI_H: ok = 1; GEN_XVSEQI_H(as, instr); break;
    case OP17_XVSHUF_D: ok = 1; GEN_XVSHUF_D(as, instr); break;
    case OP17_XVSHUF_W: ok = 1; GEN_XVSHUF_W(as, instr); break;
    case OP17_XVSIGNCOV_B: ok = 1; GEN_XVSIGNCOV_B(as, instr); break;
    case OP17_XVSIGNCOV_D: ok = 1; GEN_XVSIGNCOV_D(as, instr); break;
    case OP17_XVSIGNCOV_W: ok = 1; GEN_XVSIGNCOV_W(as, instr); break;
    case OP17_XVSLE_B: ok = 1; GEN_XVSLE_B(as, instr); break;
    case OP17_XVSLE_D: ok = 1; GEN_XVSLE_D(as, instr); break;
    case OP17_XVSLE_DU: ok = 1; GEN_XVSLE_DU(as, instr); break;
    case OP17_XVSLE_H: ok = 1; GEN_XVSLE_H(as, instr); break;
    case OP17_XVSLE_HU: ok = 1; GEN_XVSLE_HU(as, instr); break;
    case OP17_XVSLE_W: ok = 1; GEN_XVSLE_W(as, instr); break;
    case OP17_XVSLE_WU: ok = 1; GEN_XVSLE_WU(as, instr); break;
    case OP17_XVSLEI_B: ok = 1; GEN_XVSLEI_B(as, instr); break;
    case OP17_XVSLEI_D: ok = 1; GEN_XVSLEI_D(as, instr); break;
    case OP17_XVSLEI_W: ok = 1; GEN_XVSLEI_W(as, instr); break;
    case OP17_XVSLL_D: ok = 1; GEN_XVSLL_D(as, instr); break;
    case OP17_XVSLLWIL_D_W: ok = 1; GEN_XVSLLWIL_D_W(as, instr); break;
    case OP17_XVSLLWIL_DU_WU: ok = 1; GEN_XVSLLWIL_DU_WU(as, instr); break;
    case OP17_XVSLT_B: ok = 1; GEN_XVSLT_B(as, instr); break;
    case OP17_XVSLT_D: ok = 1; GEN_XVSLT_D(as, instr); break;
    case OP17_XVSLT_DU: ok = 1; GEN_XVSLT_DU(as, instr); break;
    case OP17_XVSLT_H: ok = 1; GEN_XVSLT_H(as, instr); break;
    case OP17_XVSLT_HU: ok = 1; GEN_XVSLT_HU(as, instr); break;
    case OP17_XVSLT_WU: ok = 1; GEN_XVSLT_WU(as, instr); break;
    case OP17_XVSLTI_D: ok = 1; GEN_XVSLTI_D(as, instr); break;
    case OP17_XVSLTI_H: ok = 1; GEN_XVSLTI_H(as, instr); break;
    case OP17_XVSLTI_W: ok = 1; GEN_XVSLTI_W(as, instr); break;
    case OP17_XVSRA_B: ok = 1; GEN_XVSRA_B(as, instr); break;
    case OP17_XVSRA_D: ok = 1; GEN_XVSRA_D(as, instr); break;
    case OP17_XVSRA_H: ok = 1; GEN_XVSRA_H(as, instr); break;
    case OP17_XVSRA_W: ok = 1; GEN_XVSRA_W(as, instr); break;
    case OP17_XVSRAR_B: ok = 1; GEN_XVSRAR_B(as, instr); break;
    case OP17_XVSRAR_D: ok = 1; GEN_XVSRAR_D(as, instr); break;
    case OP17_XVSRAR_H: ok = 1; GEN_XVSRAR_H(as, instr); break;
    case OP17_XVSRL_D: ok = 1; GEN_XVSRL_D(as, instr); break;
    case OP17_XVSRL_H: ok = 1; GEN_XVSRL_H(as, instr); break;
    case OP17_XVSRLR_B: ok = 1; GEN_XVSRLR_B(as, instr); break;
    case OP17_XVSRLR_D: ok = 1; GEN_XVSRLR_D(as, instr); break;
    case OP17_XVSRLR_H: ok = 1; GEN_XVSRLR_H(as, instr); break;
    case OP17_XVSRLR_W: ok = 1; GEN_XVSRLR_W(as, instr); break;
    case OP17_XVSUBWEV_D_W: ok = 1; GEN_XVSUBWEV_D_W(as, instr); break;
    case OP17_XVSUBWEV_D_WU: ok = 1; GEN_XVSUBWEV_D_WU(as, instr); break;
    case OP17_XVSUBWEV_H_B: ok = 1; GEN_XVSUBWEV_H_B(as, instr); break;
    case OP17_XVSUBWEV_Q_D: ok = 1; GEN_XVSUBWEV_Q_D(as, instr); break;
    case OP17_XVSUBWEV_Q_DU: ok = 1; GEN_XVSUBWEV_Q_DU(as, instr); break;
    case OP17_XVSUBWEV_W_HU: ok = 1; GEN_XVSUBWEV_W_HU(as, instr); break;
    case OP17_XVSUBWOD_D_W: ok = 1; GEN_XVSUBWOD_D_W(as, instr); break;
    case OP17_XVSUBWOD_D_WU: ok = 1; GEN_XVSUBWOD_D_WU(as, instr); break;
    case OP17_XVSUBWOD_H_B: ok = 1; GEN_XVSUBWOD_H_B(as, instr); break;
    case OP17_XVSUBWOD_Q_D: ok = 1; GEN_XVSUBWOD_Q_D(as, instr); break;
    case OP17_XVSUBWOD_Q_DU: ok = 1; GEN_XVSUBWOD_Q_DU(as, instr); break;
    case OP17_XVSUBWOD_W_H: ok = 1; GEN_XVSUBWOD_W_H(as, instr); break;
    case OP17_XVSUBWOD_W_HU: ok = 1; GEN_XVSUBWOD_W_HU(as, instr); break;
    case OP17_XVSUBI_WU: ok = 1; GEN_XVSUBI_WU(as, instr); break;
    case OP17_XVSUBI_HU: ok = 1; GEN_XVSUBI_HU(as, instr); break;
    case OP17_XVSUBI_DU: ok = 1; GEN_XVSUBI_DU(as, instr); break;
    case OP17_XVSUBI_BU: ok = 1; GEN_XVSUBI_BU(as, instr); break;
    case OP17_XVSSUB_WU: ok = 1; GEN_XVSSUB_WU(as, instr); break;
    case OP17_XVSSUB_W: ok = 1; GEN_XVSSUB_W(as, instr); break;
    case OP17_XVSSUB_HU: ok = 1; GEN_XVSSUB_HU(as, instr); break;
    case OP17_XVSSUB_H: ok = 1; GEN_XVSSUB_H(as, instr); break;
    case OP17_XVSSUB_DU: ok = 1; GEN_XVSSUB_DU(as, instr); break;
    case OP17_XVSSUB_D: ok = 1; GEN_XVSSUB_D(as, instr); break;
    case OP17_XVSSUB_B: ok = 1; GEN_XVSSUB_B(as, instr); break;
    case OP17_XVSSRLRNI_HU_W: ok = 1; GEN_XVSSRLRNI_HU_W(as, instr); break;
    case OP17_XVSSRLRNI_H_W: ok = 1; GEN_XVSSRLRNI_H_W(as, instr); break;
    case OP17_XVSSRLRN_WU_D: ok = 1; GEN_XVSSRLRN_WU_D(as, instr); break;
    case OP17_XVSSRLRN_W_D: ok = 1; GEN_XVSSRLRN_W_D(as, instr); break;
    case OP17_XVSSRLRN_HU_W: ok = 1; GEN_XVSSRLRN_HU_W(as, instr); break;
    case OP17_XVSSRLRN_H_W: ok = 1; GEN_XVSSRLRN_H_W(as, instr); break;
    case OP17_XVSSRLRN_BU_H: ok = 1; GEN_XVSSRLRN_BU_H(as, instr); break;
    case OP17_XVSSRLRN_B_H: ok = 1; GEN_XVSSRLRN_B_H(as, instr); break;
    case OP17_XVSSRLN_WU_D: ok = 1; GEN_XVSSRLN_WU_D(as, instr); break;
    case OP17_XVSSRLN_W_D: ok = 1; GEN_XVSSRLN_W_D(as, instr); break;
    case OP17_XVSSRLN_HU_W: ok = 1; GEN_XVSSRLN_HU_W(as, instr); break;
    case OP17_XVSSRLN_H_W: ok = 1; GEN_XVSSRLN_H_W(as, instr); break;
    case OP17_XVSSRLN_BU_H: ok = 1; GEN_XVSSRLN_BU_H(as, instr); break;
    case OP17_XVSSRLN_B_H: ok = 1; GEN_XVSSRLN_B_H(as, instr); break;
    case OP17_XVSSRARN_WU_D: ok = 1; GEN_XVSSRARN_WU_D(as, instr); break;
    case OP17_XVSSRARN_W_D: ok = 1; GEN_XVSSRARN_W_D(as, instr); break;
    case OP17_XVSSRARN_HU_W: ok = 1; GEN_XVSSRARN_HU_W(as, instr); break;
    case OP17_XVSSRARN_H_W: ok = 1; GEN_XVSSRARN_H_W(as, instr); break;
    case OP17_XVSSRARN_BU_H: ok = 1; GEN_XVSSRARN_BU_H(as, instr); break;
    case OP17_XVSSRARN_B_H: ok = 1; GEN_XVSSRARN_B_H(as, instr); break;
    case OP17_XVSSRANI_HU_W: ok = 1; GEN_XVSSRANI_HU_W(as, instr); break;
    case OP17_XVSSRAN_WU_D: ok = 1; GEN_XVSSRAN_WU_D(as, instr); break;
    case OP17_XVSSRAN_W_D: ok = 1; GEN_XVSSRAN_W_D(as, instr); break;
    case OP17_XVSSRAN_HU_W: ok = 1; GEN_XVSSRAN_HU_W(as, instr); break;
    case OP17_XVSSRAN_H_W: ok = 1; GEN_XVSSRAN_H_W(as, instr); break;
    case OP17_XVSSRAN_BU_H: ok = 1; GEN_XVSSRAN_BU_H(as, instr); break;
    case OP17_XVSSRAN_B_H: ok = 1; GEN_XVSSRAN_B_H(as, instr); break;
    case OP17_XVSRLRNI_H_W: ok = 1; GEN_XVSRLRNI_H_W(as, instr); break;
    case OP17_XVSRLRN_W_D: ok = 1; GEN_XVSRLRN_W_D(as, instr); break;
    case OP17_XVSRLRN_H_W: ok = 1; GEN_XVSRLRN_H_W(as, instr); break;
    case OP17_XVSRLRN_B_H: ok = 1; GEN_XVSRLRN_B_H(as, instr); break;
    case OP17_XVSRLRI_W: ok = 1; GEN_XVSRLRI_W(as, instr); break;
    case OP17_XVSRLNI_H_W: ok = 1; GEN_XVSRLNI_H_W(as, instr); break;
    case OP17_XVSRLN_W_D: ok = 1; GEN_XVSRLN_W_D(as, instr); break;
    case OP17_XVSRLN_H_W: ok = 1; GEN_XVSRLN_H_W(as, instr); break;
    case OP17_XVSRLN_B_H: ok = 1; GEN_XVSRLN_B_H(as, instr); break;
    case OP17_XVSRARNI_H_W: ok = 1; GEN_XVSRARNI_H_W(as, instr); break;
    case OP17_XVSRARN_W_D: ok = 1; GEN_XVSRARN_W_D(as, instr); break;
    case OP17_XVSRARN_H_W: ok = 1; GEN_XVSRARN_H_W(as, instr); break;
    case OP17_XVSRARN_B_H: ok = 1; GEN_XVSRARN_B_H(as, instr); break;
    case OP17_XVSRAN_W_D: ok = 1; GEN_XVSRAN_W_D(as, instr); break;
    case OP17_XVSRAN_H_W: ok = 1; GEN_XVSRAN_H_W(as, instr); break;
    case OP17_XVSRAN_B_H: ok = 1; GEN_XVSRAN_B_H(as, instr); break;
    case OP17_XVSLTI_WU: ok = 1; GEN_XVSLTI_WU(as, instr); break;
    case OP17_XVSLTI_HU: ok = 1; GEN_XVSLTI_HU(as, instr); break;
    case OP17_XVSLTI_DU: ok = 1; GEN_XVSLTI_DU(as, instr); break;
    case OP17_XVSLTI_BU: ok = 1; GEN_XVSLTI_BU(as, instr); break;
    case OP17_XVSLEI_WU: ok = 1; GEN_XVSLEI_WU(as, instr); break;
    case OP17_XVSLEI_HU: ok = 1; GEN_XVSLEI_HU(as, instr); break;
    case OP17_XVSLEI_DU: ok = 1; GEN_XVSLEI_DU(as, instr); break;
    case OP17_XVSLEI_BU: ok = 1; GEN_XVSLEI_BU(as, instr); break;
    case OP17_XVSADD_WU: ok = 1; GEN_XVSADD_WU(as, instr); break;
    case OP17_XVSADD_W: ok = 1; GEN_XVSADD_W(as, instr); break;
    case OP17_XVSADD_HU: ok = 1; GEN_XVSADD_HU(as, instr); break;
    case OP17_XVSADD_H: ok = 1; GEN_XVSADD_H(as, instr); break;
    case OP17_XVSADD_DU: ok = 1; GEN_XVSADD_DU(as, instr); break;
    case OP17_XVSADD_D: ok = 1; GEN_XVSADD_D(as, instr); break;
    case OP17_XVSADD_B: ok = 1; GEN_XVSADD_B(as, instr); break;
    case OP17_XVROTRI_W: ok = 1; GEN_XVROTRI_W(as, instr); break;
    case OP17_XVROTR_W: ok = 1; GEN_XVROTR_W(as, instr); break;
    case OP17_XVROTR_H: ok = 1; GEN_XVROTR_H(as, instr); break;
    case OP17_XVROTR_D: ok = 1; GEN_XVROTR_D(as, instr); break;
    case OP17_XVROTR_B: ok = 1; GEN_XVROTR_B(as, instr); break;
    case OP17_XVPICKOD_W: ok = 1; GEN_XVPICKOD_W(as, instr); break;
    case OP17_XVPICKOD_D: ok = 1; GEN_XVPICKOD_D(as, instr); break;
    case OP17_XVMOD_WU: ok = 1; GEN_XVMOD_WU(as, instr); break;
    case OP17_XVMOD_W: ok = 1; GEN_XVMOD_W(as, instr); break;
    case OP17_XVMOD_HU: ok = 1; GEN_XVMOD_HU(as, instr); break;
    case OP17_XVMOD_H: ok = 1; GEN_XVMOD_H(as, instr); break;
    case OP17_XVMOD_DU: ok = 1; GEN_XVMOD_DU(as, instr); break;
    case OP17_XVMOD_D: ok = 1; GEN_XVMOD_D(as, instr); break;
    case OP17_XVMOD_BU: ok = 1; GEN_XVMOD_BU(as, instr); break;
    case OP17_XVMOD_B: ok = 1; GEN_XVMOD_B(as, instr); break;
    case OP17_XVMINI_WU: ok = 1; GEN_XVMINI_WU(as, instr); break;
    case OP17_XVMINI_HU: ok = 1; GEN_XVMINI_HU(as, instr); break;
    case OP17_XVMINI_DU: ok = 1; GEN_XVMINI_DU(as, instr); break;
    case OP17_XVMINI_BU: ok = 1; GEN_XVMINI_BU(as, instr); break;
    case OP17_XVMAXI_WU: ok = 1; GEN_XVMAXI_WU(as, instr); break;
    case OP17_XVMAXI_HU: ok = 1; GEN_XVMAXI_HU(as, instr); break;
    case OP17_XVMAXI_DU: ok = 1; GEN_XVMAXI_DU(as, instr); break;
    case OP17_XVMAXI_BU: ok = 1; GEN_XVMAXI_BU(as, instr); break;
    case OP17_XVMADDWOD_W_HU: ok = 1; GEN_XVMADDWOD_W_HU(as, instr); break;
    case OP17_XVMADDWOD_Q_DU: ok = 1; GEN_XVMADDWOD_Q_DU(as, instr); break;
    case OP17_XVMADDWOD_D_WU: ok = 1; GEN_XVMADDWOD_D_WU(as, instr); break;
    case OP17_XVMADDWEV_W_HU: ok = 1; GEN_XVMADDWEV_W_HU(as, instr); break;
    case OP17_XVMADDWEV_Q_DU: ok = 1; GEN_XVMADDWEV_Q_DU(as, instr); break;
    case OP17_XVMADDWEV_D_WU: ok = 1; GEN_XVMADDWEV_D_WU(as, instr); break;
    case OP17_XVFTINTRZ_W_D: ok = 1; GEN_XVFTINTRZ_W_D(as, instr); break;
    case OP17_XVFTINTRP_W_D: ok = 1; GEN_XVFTINTRP_W_D(as, instr); break;
    case OP17_XVFTINTRNE_W_D: ok = 1; GEN_XVFTINTRNE_W_D(as, instr); break;
    case OP17_XVFTINTRM_W_D: ok = 1; GEN_XVFTINTRM_W_D(as, instr); break;
    case OP17_XVFTINT_W_D: ok = 1; GEN_XVFTINT_W_D(as, instr); break;
    case OP17_XVFRSTPI_H: ok = 1; GEN_XVFRSTPI_H(as, instr); break;
    case OP17_XVFRSTPI_B: ok = 1; GEN_XVFRSTPI_B(as, instr); break;
    case OP17_XVFRSTP_H: ok = 1; GEN_XVFRSTP_H(as, instr); break;
    case OP17_XVFRSTP_B: ok = 1; GEN_XVFRSTP_B(as, instr); break;
    case OP17_XVFMINA_S: ok = 1; GEN_XVFMINA_S(as, instr); break;
    case OP17_XVFMINA_D: ok = 1; GEN_XVFMINA_D(as, instr); break;
    case OP17_XVFMAXA_S: ok = 1; GEN_XVFMAXA_S(as, instr); break;
    case OP17_XVFMAXA_D: ok = 1; GEN_XVFMAXA_D(as, instr); break;
    case OP17_XVFFINT_S_L: ok = 1; GEN_XVFFINT_S_L(as, instr); break;
    case OP17_XVFCVT_H_S: ok = 1; GEN_XVFCVT_H_S(as, instr); break;
    case OP17_XVDIV_WU: ok = 1; GEN_XVDIV_WU(as, instr); break;
    case OP17_XVDIV_W: ok = 1; GEN_XVDIV_W(as, instr); break;
    case OP17_XVDIV_HU: ok = 1; GEN_XVDIV_HU(as, instr); break;
    case OP17_XVDIV_H: ok = 1; GEN_XVDIV_H(as, instr); break;
    case OP17_XVDIV_DU: ok = 1; GEN_XVDIV_DU(as, instr); break;
    case OP17_XVDIV_D: ok = 1; GEN_XVDIV_D(as, instr); break;
    case OP17_XVDIV_BU: ok = 1; GEN_XVDIV_BU(as, instr); break;
    case OP17_XVDIV_B: ok = 1; GEN_XVDIV_B(as, instr); break;
    case OP17_XVBITSET_W: ok = 1; GEN_XVBITSET_W(as, instr); break;
    case OP17_XVBITSET_H: ok = 1; GEN_XVBITSET_H(as, instr); break;
    case OP17_XVBITSET_D: ok = 1; GEN_XVBITSET_D(as, instr); break;
    case OP17_XVBITSET_B: ok = 1; GEN_XVBITSET_B(as, instr); break;
    case OP17_XVBITREV_W: ok = 1; GEN_XVBITREV_W(as, instr); break;
    case OP17_XVBITREV_H: ok = 1; GEN_XVBITREV_H(as, instr); break;
    case OP17_XVBITREV_D: ok = 1; GEN_XVBITREV_D(as, instr); break;
    case OP17_XVBITREV_B: ok = 1; GEN_XVBITREV_B(as, instr); break;
    case OP17_XVBITCLR_W: ok = 1; GEN_XVBITCLR_W(as, instr); break;
    case OP17_XVBITCLR_H: ok = 1; GEN_XVBITCLR_H(as, instr); break;
    case OP17_XVBITCLR_D: ok = 1; GEN_XVBITCLR_D(as, instr); break;
    case OP17_XVBITCLR_B: ok = 1; GEN_XVBITCLR_B(as, instr); break;
    case OP17_XVREPLVE_B: ok = 1; GEN_XVREPLVE_B(as, instr); break;
    case OP17_XVREPLVE_D: ok = 1; GEN_XVREPLVE_D(as, instr); break;
    case OP17_XVREPLVE_H: ok = 1; GEN_XVREPLVE_H(as, instr); break;
    case OP17_XVREPLVE_W: ok = 1; GEN_XVREPLVE_W(as, instr); break;
    default: break;
    }
     switch (instr >> 14) {
    case OP18_XVSAT_HU: ok = 1; GEN_XVSAT_HU(as, instr); break;
    case OP18_XVSLLI_H: ok = 1; GEN_XVSLLI_H(as, instr); break;
    case OP18_XVSRAI_H: ok = 1; GEN_XVSRAI_H(as, instr); break;
    case OP18_XVSRARI_H: ok = 1; GEN_XVSRARI_H(as, instr); break;
    case OP18_XVSRLI_H: ok = 1; GEN_XVSRLI_H(as, instr); break;
    case OP18_XVSSRANI_B_H: ok = 1; GEN_XVSSRANI_B_H(as, instr); break;
    case OP18_XVSSRARNI_BU_H: ok = 1; GEN_XVSSRARNI_BU_H(as, instr); break;
    case OP18_XVREPL128VEI_B: ok = 1; GEN_XVREPL128VEI_B(as, instr); break;
    case OP18_XVSSRLNI_BU_H: ok = 1; GEN_XVSSRLNI_BU_H(as, instr); break;
    case OP18_XVSLLWIL_W_H: ok = 1; GEN_XVSLLWIL_W_H(as, instr); break;
    case OP18_XVBITCLRI_H: ok = 1; GEN_XVBITCLRI_H(as, instr); break;
    case OP18_XVBITREVI_H: ok = 1; GEN_XVBITREVI_H(as, instr); break;
    case OP18_XVBITSETI_H: ok = 1; GEN_XVBITSETI_H(as, instr); break;
    case OP18_XVSAT_H: ok = 1; GEN_XVSAT_H(as, instr); break;
    case OP18_XVSLLWIL_WU_HU: ok = 1; GEN_XVSLLWIL_WU_HU(as, instr); break;
    case OP18_XVSRANI_B_H: ok = 1; GEN_XVSRANI_B_H(as, instr); break;
    case OP18_XVSSRLRNI_BU_H: ok = 1; GEN_XVSSRLRNI_BU_H(as, instr); break;
    case OP18_XVSSRLRNI_B_H: ok = 1; GEN_XVSSRLRNI_B_H(as, instr); break;
    case OP18_XVSSRLNI_B_H: ok = 1; GEN_XVSSRLNI_B_H(as, instr); break;
    case OP18_XVSSRARNI_B_H: ok = 1; GEN_XVSSRARNI_B_H(as, instr); break;
    case OP18_XVSSRANI_BU_H: ok = 1; GEN_XVSSRANI_BU_H(as, instr); break;
    case OP18_XVSRLRNI_B_H: ok = 1; GEN_XVSRLRNI_B_H(as, instr); break;
    case OP18_XVSRLRI_H: ok = 1; GEN_XVSRLRI_H(as, instr); break;
    case OP18_XVSRLNI_B_H: ok = 1; GEN_XVSRLNI_B_H(as, instr); break;
    case OP18_XVSRARNI_B_H: ok = 1; GEN_XVSRARNI_B_H(as, instr); break;
    case OP18_XVROTRI_H: ok = 1; GEN_XVROTRI_H(as, instr); break;
    default: break;
    }
     switch (instr >> 13) {
    case OP19_XVINSGR2VR_W: ok = 1; GEN_XVINSGR2VR_W(as, instr); break;
    case OP19_XVPICKVE2GR_W: ok = 1; GEN_XVPICKVE2GR_W(as, instr); break;
    case OP19_XVPICKVE2GR_WU: ok = 1; GEN_XVPICKVE2GR_WU(as, instr); break;
    case OP19_XVSLLI_B: ok = 1; GEN_XVSLLI_B(as, instr); break;
    case OP19_XVSRLI_B: ok = 1; GEN_XVSRLI_B(as, instr); break;
    case OP19_XVINSVE0_W: ok = 1; GEN_XVINSVE0_W(as, instr); break;
    case OP19_XVREPL128VEI_H: ok = 1; GEN_XVREPL128VEI_H(as, instr); break;
    case OP19_XVBITCLRI_B: ok = 1; GEN_XVBITCLRI_B(as, instr); break;
    case OP19_XVBITREVI_B: ok = 1; GEN_XVBITREVI_B(as, instr); break;
    case OP19_XVBITSETI_B: ok = 1; GEN_XVBITSETI_B(as, instr); break;
    case OP19_XVSAT_B: ok = 1; GEN_XVSAT_B(as, instr); break;
    case OP19_XVSAT_BU: ok = 1; GEN_XVSAT_BU(as, instr); break;
    case OP19_XVSLLWIL_H_B: ok = 1; GEN_XVSLLWIL_H_B(as, instr); break;
    case OP19_XVSLLWIL_HU_BU: ok = 1; GEN_XVSLLWIL_HU_BU(as, instr); break;
    case OP19_XVSRAI_B: ok = 1; GEN_XVSRAI_B(as, instr); break;
    case OP19_XVSRARI_B: ok = 1; GEN_XVSRARI_B(as, instr); break;
    case OP19_XVSRLRI_B: ok = 1; GEN_XVSRLRI_B(as, instr); break;
    case OP19_XVROTRI_B: ok = 1; GEN_XVROTRI_B(as, instr); break;
    case OP19_XVPICKVE_W: ok = 1; GEN_XVPICKVE_W(as, instr); break;
    default: break;
    }
    switch (instr >> 12) {
    case OP20_XVPICKVE2GR_D: ok = 1; GEN_XVPICKVE2GR_D(as, instr); break;
    case OP20_XVPICKVE2GR_DU: ok = 1; GEN_XVPICKVE2GR_DU(as, instr); break;
    case OP20_XVINSVE0_D: ok = 1; GEN_XVINSVE0_D(as, instr); break;
    case OP20_XVREPL128VEI_W: ok = 1; GEN_XVREPL128VEI_W(as, instr); break;
    case OP20_XVINSGR2VR_D: ok = 1; GEN_XVINSGR2VR_D(as, instr); break;
    case OP20_XVPICKVE_D: ok = 1; GEN_XVPICKVE_D(as, instr); break;
    default: break;
    }
     switch (instr >> 10) {
    case OP22_VEXT2XV_HU_BU: ok = 1; GEN_VEXT2XV_HU_BU(as, instr); break;
    case OP22_VEXT2XV_H_B: ok = 1; GEN_VEXT2XV_H_B(as, instr); break;
    case OP22_VEXT2XV_WU_BU: ok = 1; GEN_VEXT2XV_WU_BU(as, instr); break;
    case OP22_VEXT2XV_WU_HU: ok = 1; GEN_VEXT2XV_WU_HU(as, instr); break;
    case OP22_VEXT2XV_W_H: ok = 1; GEN_VEXT2XV_W_H(as, instr); break;
    case OP22_XVFCVTH_D_S: ok = 1; GEN_XVFCVTH_D_S(as, instr); break;
    case OP22_XVFCVTL_D_S: ok = 1; GEN_XVFCVTL_D_S(as, instr); break;
    case OP22_XVFRECIP_S: ok = 1; GEN_XVFRECIP_S(as, instr); break;
    case OP22_XVFRINTRZ_S: ok = 1; GEN_XVFRINTRZ_S(as, instr); break;
    case OP22_XVFTINTRNE_W_S: ok = 1; GEN_XVFTINTRNE_W_S(as, instr); break;
    case OP22_XVNEG_B: ok = 1; GEN_XVNEG_B(as, instr); break;
    case OP22_XVPCNT_W: ok = 1; GEN_XVPCNT_W(as, instr); break;
    case OP22_XVREPLGR2VR_H: ok = 1; GEN_XVREPLGR2VR_H(as, instr); break;
    case OP22_XVREPLGR2VR_W: ok = 1; GEN_XVREPLGR2VR_W(as, instr); break;
    case OP22_XVPCNT_B: ok = 1; GEN_XVPCNT_B(as, instr); break;
    case OP22_XVPCNT_H: ok = 1; GEN_XVPCNT_H(as, instr); break;
    case OP22_XVPCNT_D: ok = 1; GEN_XVPCNT_D(as, instr); break;
    case OP22_VEXT2XV_W_B: ok = 1; GEN_VEXT2XV_W_B(as, instr); break;
    case OP22_VEXT2XV_D_B: ok = 1; GEN_VEXT2XV_D_B(as, instr); break;
    case OP22_VEXT2XV_D_H: ok = 1; GEN_VEXT2XV_D_H(as, instr); break;
    case OP22_VEXT2XV_D_W: ok = 1; GEN_VEXT2XV_D_W(as, instr); break;
    case OP22_VEXT2XV_DU_BU: ok = 1; GEN_VEXT2XV_DU_BU(as, instr); break;
    case OP22_VEXT2XV_DU_HU: ok = 1; GEN_VEXT2XV_DU_HU(as, instr); break;
    case OP22_VEXT2XV_DU_WU: ok = 1; GEN_VEXT2XV_DU_WU(as, instr); break;
    case OP22_XVNEG_H: ok = 1; GEN_XVNEG_H(as, instr); break;
    case OP22_XVCLZ_D: ok = 1; GEN_XVCLZ_D(as, instr); break;
    case OP22_XVCLZ_W: ok = 1; GEN_XVCLZ_W(as, instr); break;
    case OP22_XVMSKLTZ_W: ok = 1; GEN_XVMSKLTZ_W(as, instr); break;
    case OP22_XVREPLGR2VR_B: ok = 1; GEN_XVREPLGR2VR_B(as, instr); break;
    case OP22_XVCLZ_B: ok = 1; GEN_XVCLZ_B(as, instr); break;
    case OP22_XVCLZ_H: ok = 1; GEN_XVCLZ_H(as, instr); break;
    case OP22_XVMSKLTZ_B: ok = 1; GEN_XVMSKLTZ_B(as, instr); break;
    case OP22_XVMSKLTZ_D: ok = 1; GEN_XVMSKLTZ_D(as, instr); break;
    case OP22_XVMSKLTZ_H: ok = 1; GEN_XVMSKLTZ_H(as, instr); break;
    case OP22_XVNEG_D: ok = 1; GEN_XVNEG_D(as, instr); break;
    case OP22_XVNEG_W: ok = 1; GEN_XVNEG_W(as, instr); break;
    case OP22_XVMSKNZ_B: ok = 1; GEN_XVMSKNZ_B(as, instr); break;
    case OP22_XVMSKGEZ_B: ok = 1; GEN_XVMSKGEZ_B(as, instr); break;
    case OP22_XVFTINTRZL_L_S: ok = 1; GEN_XVFTINTRZL_L_S(as, instr); break;
    case OP22_XVFTINTRZH_L_S: ok = 1; GEN_XVFTINTRZH_L_S(as, instr); break;
    case OP22_XVFTINTRZ_WU_S: ok = 1; GEN_XVFTINTRZ_WU_S(as, instr); break;
    case OP22_XVFTINTRZ_W_S: ok = 1; GEN_XVFTINTRZ_W_S(as, instr); break;
    case OP22_XVFTINTRZ_LU_D: ok = 1; GEN_XVFTINTRZ_LU_D(as, instr); break;
    case OP22_XVFTINTRZ_L_D: ok = 1; GEN_XVFTINTRZ_L_D(as, instr); break;
    case OP22_XVFTINTRPL_L_S: ok = 1; GEN_XVFTINTRPL_L_S(as, instr); break;
    case OP22_XVFTINTRPH_L_S: ok = 1; GEN_XVFTINTRPH_L_S(as, instr); break;
    case OP22_XVFTINTRP_W_S: ok = 1; GEN_XVFTINTRP_W_S(as, instr); break;
    case OP22_XVFTINTRP_L_D: ok = 1; GEN_XVFTINTRP_L_D(as, instr); break;
    case OP22_XVFTINTRNEL_L_S: ok = 1; GEN_XVFTINTRNEL_L_S(as, instr); break;
    case OP22_XVFTINTRNEH_L_S: ok = 1; GEN_XVFTINTRNEH_L_S(as, instr); break;
    case OP22_XVFTINTRNE_L_D: ok = 1; GEN_XVFTINTRNE_L_D(as, instr); break;
    case OP22_XVFTINTRML_L_S: ok = 1; GEN_XVFTINTRML_L_S(as, instr); break;
    case OP22_XVFTINTRMH_L_S: ok = 1; GEN_XVFTINTRMH_L_S(as, instr); break;
    case OP22_XVFTINTRM_W_S: ok = 1; GEN_XVFTINTRM_W_S(as, instr); break;
    case OP22_XVFTINTRM_L_D: ok = 1; GEN_XVFTINTRM_L_D(as, instr); break;
    case OP22_XVFTINTL_L_S: ok = 1; GEN_XVFTINTL_L_S(as, instr); break;
    case OP22_XVFTINTH_L_S: ok = 1; GEN_XVFTINTH_L_S(as, instr); break;
    case OP22_XVFTINT_WU_S: ok = 1; GEN_XVFTINT_WU_S(as, instr); break;
    case OP22_XVFTINT_W_S: ok = 1; GEN_XVFTINT_W_S(as, instr); break;
    case OP22_XVFTINT_LU_D: ok = 1; GEN_XVFTINT_LU_D(as, instr); break;
    case OP22_XVFTINT_L_D: ok = 1; GEN_XVFTINT_L_D(as, instr); break;
    case OP22_XVFSQRT_S: ok = 1; GEN_XVFSQRT_S(as, instr); break;
    case OP22_XVFSQRT_D: ok = 1; GEN_XVFSQRT_D(as, instr); break;
    case OP22_XVFRSQRTE_S: ok = 1; GEN_XVFRSQRTE_S(as, instr); break;
    case OP22_XVFRSQRTE_D: ok = 1; GEN_XVFRSQRTE_D(as, instr); break;
    case OP22_XVFRSQRT_S: ok = 1; GEN_XVFRSQRT_S(as, instr); break;
    case OP22_XVFRSQRT_D: ok = 1; GEN_XVFRSQRT_D(as, instr); break;
    case OP22_XVFRINTRZ_D: ok = 1; GEN_XVFRINTRZ_D(as, instr); break;
    case OP22_XVFRINTRP_S: ok = 1; GEN_XVFRINTRP_S(as, instr); break;
    case OP22_XVFRINTRP_D: ok = 1; GEN_XVFRINTRP_D(as, instr); break;
    case OP22_XVFRINTRNE_S: ok = 1; GEN_XVFRINTRNE_S(as, instr); break;
    case OP22_XVFRINTRNE_D: ok = 1; GEN_XVFRINTRNE_D(as, instr); break;
    case OP22_XVFRINTRM_S: ok = 1; GEN_XVFRINTRM_S(as, instr); break;
    case OP22_XVFRINTRM_D: ok = 1; GEN_XVFRINTRM_D(as, instr); break;
    case OP22_XVFRINT_S: ok = 1; GEN_XVFRINT_S(as, instr); break;
    case OP22_XVFRINT_D: ok = 1; GEN_XVFRINT_D(as, instr); break;
    case OP22_XVFRECIPE_S: ok = 1; GEN_XVFRECIPE_S(as, instr); break;
    case OP22_XVFRECIPE_D: ok = 1; GEN_XVFRECIPE_D(as, instr); break;
    case OP22_XVFRECIP_D: ok = 1; GEN_XVFRECIP_D(as, instr); break;
    case OP22_XVFLOGB_S: ok = 1; GEN_XVFLOGB_S(as, instr); break;
    case OP22_XVFLOGB_D: ok = 1; GEN_XVFLOGB_D(as, instr); break;
    case OP22_XVFFINTL_D_W: ok = 1; GEN_XVFFINTL_D_W(as, instr); break;
    case OP22_XVFFINTH_D_W: ok = 1; GEN_XVFFINTH_D_W(as, instr); break;
    case OP22_XVFFINT_S_WU: ok = 1; GEN_XVFFINT_S_WU(as, instr); break;
    case OP22_XVFFINT_S_W: ok = 1; GEN_XVFFINT_S_W(as, instr); break;
    case OP22_XVFFINT_D_LU: ok = 1; GEN_XVFFINT_D_LU(as, instr); break;
    case OP22_XVFFINT_D_L: ok = 1; GEN_XVFFINT_D_L(as, instr); break;
    case OP22_XVFCVTL_S_H: ok = 1; GEN_XVFCVTL_S_H(as, instr); break;
    case OP22_XVFCVTH_S_H: ok = 1; GEN_XVFCVTH_S_H(as, instr); break;
    case OP22_XVFCLASS_S: ok = 1; GEN_XVFCLASS_S(as, instr); break;
    case OP22_XVFCLASS_D: ok = 1; GEN_XVFCLASS_D(as, instr); break;
    case OP22_XVEXTL_QU_DU: ok = 1; GEN_XVEXTL_QU_DU(as, instr); break;
    case OP22_XVEXTL_Q_D: ok = 1; GEN_XVEXTL_Q_D(as, instr); break;
    case OP22_XVEXTH_WU_HU: ok = 1; GEN_XVEXTH_WU_HU(as, instr); break;
    case OP22_XVEXTH_W_H: ok = 1; GEN_XVEXTH_W_H(as, instr); break;
    case OP22_XVEXTH_QU_DU: ok = 1; GEN_XVEXTH_QU_DU(as, instr); break;
    case OP22_XVEXTH_Q_D: ok = 1; GEN_XVEXTH_Q_D(as, instr); break;
    case OP22_XVEXTH_HU_BU: ok = 1; GEN_XVEXTH_HU_BU(as, instr); break;
    case OP22_XVEXTH_H_B: ok = 1; GEN_XVEXTH_H_B(as, instr); break;
    case OP22_XVEXTH_DU_WU: ok = 1; GEN_XVEXTH_DU_WU(as, instr); break;
    case OP22_XVEXTH_D_W: ok = 1; GEN_XVEXTH_D_W(as, instr); break;
    case OP22_XVCLO_W: ok = 1; GEN_XVCLO_W(as, instr); break;
    case OP22_XVCLO_H: ok = 1; GEN_XVCLO_H(as, instr); break;
    case OP22_XVCLO_D: ok = 1; GEN_XVCLO_D(as, instr); break;
    case OP22_XVCLO_B: ok = 1; GEN_XVCLO_B(as, instr); break;
    case OP22_XVREPLGR2VR_D: ok = 1; GEN_XVREPLGR2VR_D(as, instr); break;
    case OP22_XVREPLVE0_B: ok = 1; GEN_XVREPLVE0_B(as, instr); break;
    case OP22_XVREPLVE0_D: ok = 1; GEN_XVREPLVE0_D(as, instr); break;
    case OP22_XVREPLVE0_H: ok = 1; GEN_XVREPLVE0_H(as, instr); break;
    case OP22_XVREPLVE0_Q: ok = 1; GEN_XVREPLVE0_Q(as, instr); break;
    case OP22_XVREPLVE0_W: ok = 1; GEN_XVREPLVE0_W(as, instr); break;
    default: break;
    }
    switch (instr >> 8) {
    case OP24_XVSETALLNEZ_B: ok = 1; GEN_XVSETALLNEZ_B(as, instr); break;
    case OP24_XVSETALLNEZ_D: ok = 1; GEN_XVSETALLNEZ_D(as, instr); break;
    case OP24_XVSETALLNEZ_H: ok = 1; GEN_XVSETALLNEZ_H(as, instr); break;
    case OP24_XVSETALLNEZ_W: ok = 1; GEN_XVSETALLNEZ_W(as, instr); break;
    case OP24_XVSETANYEQZ_B: ok = 1; GEN_XVSETANYEQZ_B(as, instr); break;
    case OP24_XVSETANYEQZ_D: ok = 1; GEN_XVSETANYEQZ_D(as, instr); break;
    case OP24_XVSETANYEQZ_H: ok = 1; GEN_XVSETANYEQZ_H(as, instr); break;
    case OP24_XVSETANYEQZ_W: ok = 1; GEN_XVSETANYEQZ_W(as, instr); break;
    case OP24_XVSETEQZ_V: ok = 1; GEN_XVSETEQZ_V(as, instr); break;
    case OP24_XVSETNEZ_V: ok = 1; GEN_XVSETNEZ_V(as, instr); break;
    default: break;
    }
    switch (instr >> 11) {
    case OP21_XVREPL128VEI_D: ok = 1; GEN_XVREPL128VEI_D(as, instr); break;
    default: break;
    }
    switch (instr >> 17) {
    case OP15_XVSRANI_D_Q: ok = 1; GEN_XVSRANI_D_Q(as, instr); break;
    case OP15_XVSRARNI_D_Q: ok = 1; GEN_XVSRARNI_D_Q(as, instr); break;
    case OP15_XVSRLNI_D_Q: ok = 1; GEN_XVSRLNI_D_Q(as, instr); break;
    case OP15_XVSRLRNI_D_Q: ok = 1; GEN_XVSRLRNI_D_Q(as, instr); break;
    case OP15_XVSSRANI_D_Q: ok = 1; GEN_XVSSRANI_D_Q(as, instr); break;
    case OP15_XVSSRANI_DU_Q: ok = 1; GEN_XVSSRANI_DU_Q(as, instr); break;
    case OP15_XVSSRARNI_D_Q: ok = 1; GEN_XVSSRARNI_D_Q(as, instr); break;
    case OP15_XVSSRARNI_DU_Q: ok = 1; GEN_XVSSRARNI_DU_Q(as, instr); break;
    case OP15_XVSSRLNI_D_Q: ok = 1; GEN_XVSSRLNI_D_Q(as, instr); break;
    case OP15_XVSSRLNI_DU_Q: ok = 1; GEN_XVSSRLNI_DU_Q(as, instr); break;
    case OP15_XVSSRLRNI_D_Q: ok = 1; GEN_XVSSRLRNI_D_Q(as, instr); break;
    case OP15_XVSSRLRNI_DU_Q: ok = 1; GEN_XVSSRLRNI_DU_Q(as, instr); break;
    default: break;
    }
    switch (instr >> 23) {
    case OP9_XVSTELM_B: ok = 1; GEN_XVSTELM_B(as, instr); break;
    default: break;
    }
    if (with_prologue) {
        la_addi_d(as, LA_TP, LA_TP, -tp_offset);
        la_jiscr0(as, 4);
    }
    return ok;
}
