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
#include "lagoon.h"
#include "lasx_interpret_opt.h"

/* ================================================================
 * Special functions with complex logic
 * ================================================================ */

void __gen_lasx_interpret_xvld_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int rj = (instr >> 5) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    int off = (instr >> 10) & 0xfff;
    off = (off << 20) >> 20;
    tdlog("                 : interpret(optgen-xvmap) xvld %08x \n", instr);

    int xd2 = as->xvmap[xd];

    if (off >= -2048 && off <= 2047 - 16) {
        la_vld(as, xd2, rj, off + 16);
        la_vld(as, xd, rj, off);
    } else {
        assert(0);
    }
}

void __gen_lasx_interpret_xvst_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int rj = (instr >> 5) & 0x1f;
    int xd = (instr >> 0) & 0x1f;
    int off = (instr >> 10) & 0xfff;
    off = (off << 20) >> 20;
    tdlog("                 : interpret(optgen-xvmap) xvst %08x \n", instr);

    int xd2 = as->xvmap[xd];

    if (off >= -2048 && off <= 2047 - 16) {
        la_vst(as, xd, rj, off);
        la_vst(as, xd2, rj, off + 16);
    } else {
        assert(0);
    }
}

void __gen_lasx_interpret_vext2xv_hu_bu_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_hu_bu %08x \n", instr);

    int xd2 = as->xvmap[xd];

    la_vexth_hu_bu(as, xd2, xj);
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_hu_bu(as, xd, xd);
}

void __gen_lasx_interpret_vext2xv_wu_hu_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_wu_hu %08x \n", instr);

    int xd2 = as->xvmap[xd];

    la_vexth_wu_hu(as, xd2, xj);
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_wu_hu(as, xd, xd);
}

void __gen_lasx_interpret_xvpermi_q_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xff;
    tdlog("                 : interpret(optgen-xvmap) xvpermi_q %08x \n", instr);

    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];

    int q0 = (imm >> 0) & 0x3;
    int q1 = (imm >> 4) & 0x3;

    /* vec = {xd2, xd, xj2, xj}; q0/q1 select from vec[0..3].
       q0=3 reads xd2 which q1 overwrites. Do q0 first in that case. */

    if (xd == xj) {
        if ((q0 == 0 || q0 == 2) && (q1 == 0 || q1 == 2)) {
	    la_vori_b(as, xd2, xd, 0);
        }
        if (q0 == 1 || q0 == 3) {
	    if (q1 == 0 || q1 == 2) {
		la_vxor_v(as, xd,  xd,  xd2);
		la_vxor_v(as, xd2,  xd,  xd2);
		la_vxor_v(as, xd,  xd,  xd2);
	    } else {
		la_vori_b(as, xd, xd2, 0);
	    }
        }
    } else {
        if (q0 == 0) {
            switch (q1) {
            case 0: la_vori_b(as, xd, xj, 0); la_vori_b(as, xd2, xj, 0); break;
            case 1: la_vori_b(as, xd, xj, 0); la_vori_b(as, xd2, xj2, 0); break;
            case 2: la_vori_b(as, xd2, xd, 0); la_vori_b(as, xd, xj, 0); break;
            case 3: la_vori_b(as, xd, xj, 0); break;
            }
        } else if (q0 == 1) {
            switch (q1) {
            case 0: la_vori_b(as, xd, xj2, 0); la_vori_b(as, xd2, xj, 0); break;
            case 1: la_vori_b(as, xd, xj2, 0); la_vori_b(as, xd2, xj2, 0); break;
            case 2: la_vori_b(as, xd2, xd, 0); la_vori_b(as, xd, xj2, 0); break;
            case 3: la_vori_b(as, xd, xj2, 0); break;
            }
        } else if (q0 == 2) {
            switch (q1) {
            case 0: la_vori_b(as, xd2, xj, 0); break;
            case 1: la_vori_b(as, xd2, xj2, 0); break;
            case 2: la_vori_b(as, xd2, xd, 0); break;
            case 3: break;
            }
        } else {
            switch (q1) {
            case 0: la_vori_b(as, xd, xd2, 0); la_vori_b(as, xd2, xj, 0); break;
            case 1: la_vori_b(as, xd, xd2, 0); la_vori_b(as, xd2, xj2, 0); break;
            case 2: la_vxor_v(as, xd,  xd,  xd2); la_vxor_v(as, xd2,  xd,  xd2); la_vxor_v(as, xd,  xd,  xd2); break;
            case 3: la_vori_b(as, xd, xd2, 0); break;
            }
        }
    }
}

void __gen_lasx_interpret_xvpickve2gr_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int rd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int uk = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen-xvmap) xvpickve2gr_w %08x \n", instr);

    int xj2 = as->xvmap[xj];

    if (uk < 4) {
        la_vpickve2gr_w(as, rd, xj, uk & 0x3);
    } else {
        la_vpickve2gr_w(as, rd, xj2, uk & 0x3);
    }
}

void __gen_lasx_interpret_xvldx_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int rk = (instr >> 10) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvldx %08x \n", instr);

    int xd2 = as->xvmap[xd];

    la_addi_d(as, (la_gpr_t)rk, (la_gpr_t)rk, 16);
    la_vldx(as, xd2, (la_gpr_t)rj, (la_gpr_t)rk);
    la_addi_d(as, (la_gpr_t)rk, (la_gpr_t)rk, -16);
    la_vldx(as, xd, (la_gpr_t)rj, (la_gpr_t)rk);
}

void __gen_lasx_interpret_xvstx_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int rk = (instr >> 10) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvstx %08x \n", instr);

    int xd2 = as->xvmap[xd];

    la_vstx(as, xd, (la_gpr_t)rj, (la_gpr_t)rk);
    la_addi_d(as, (la_gpr_t)rk, (la_gpr_t)rk, 16);
    la_vstx(as, xd2, (la_gpr_t)rj, (la_gpr_t)rk);
    la_addi_d(as, (la_gpr_t)rk, (la_gpr_t)rk, -16);
}

void __gen_lasx_interpret_xvldrepl_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int imm_raw = (instr >> 10) & 0x3ff;
    int imm = (int32_t)(imm_raw << 22 >> 22 << 2);
    tdlog("                 : interpret(optgen-xvmap) xvldrepl_w %08x \n", instr);

    int xd2 = as->xvmap[xd];

    la_vldrepl_w(as, xd, (la_gpr_t)rj, imm);
    la_vldrepl_w(as, xd2, (la_gpr_t)rj, imm);
}

void __gen_lasx_interpret_xvinsgr2vr_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int elem = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen-xvmap) xvinsgr2vr_w %08x \n", instr);
    int xd2 = as->xvmap[xd];
    if (elem < 4) {
        la_vinsgr2vr_w(as, xd, (la_gpr_t)rj, elem);
    } else {
        la_vinsgr2vr_w(as, xd2, (la_gpr_t)rj, elem - 4);
    }
}

void __gen_lasx_interpret_xvstelm_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xff;
    int elem = (instr >> 18) & 0x3;
    tdlog("                 : interpret(optgen-xvmap) xvstelm_d %08x \n", instr);
    int xd2 = as->xvmap[xd];
    /* la_vstelm_d divides imm by 8, compensate with << 3 */
    if (elem < 2) {
        la_vstelm_d(as, xd, (la_gpr_t)rj, imm << 3, elem);
    } else {
        la_vstelm_d(as, xd2, (la_gpr_t)rj, imm << 3, elem - 2);
    }
}

void __gen_lasx_interpret_xvstelm_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xff;
    int elem = (instr >> 18) & 0x7;
    tdlog("                 : interpret(optgen-xvmap) xvstelm_w %08x \n", instr);
    int xd2 = as->xvmap[xd];
    /* la_vstelm_w divides imm by 4, compensate with << 2 */
    if (elem < 4) {
        la_vstelm_w(as, xd, (la_gpr_t)rj, imm << 2, elem);
    } else {
        la_vstelm_w(as, xd2, (la_gpr_t)rj, imm << 2, elem - 4);
    }
}

void __gen_lasx_interpret_xvreplgr2vr_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int xd2 = as->xvmap[xd];
    la_vreplgr2vr_w(as, xd, rj);
    la_vreplgr2vr_w(as, xd2, rj);
}

void __gen_lasx_interpret_xvreplgr2vr_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int xd2 = as->xvmap[xd];
    la_vreplgr2vr_h(as, xd, (la_gpr_t)rj);
    la_vreplgr2vr_h(as, xd2, (la_gpr_t)rj);
}

void __gen_lasx_interpret_xvreplgr2vr_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int xd2 = as->xvmap[xd];
    la_vreplgr2vr_d(as, xd, (la_gpr_t)rj);
    la_vreplgr2vr_d(as, xd2, (la_gpr_t)rj);
}

void __gen_lasx_interpret_xvfcmp_cond_s_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int xk = (instr >> 10) & 0x1f;
    int cond = (instr >> 15) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvfcmp_cond_s %08x \n", instr);

    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    int xk2 = as->xvmap[xk];

    void (*f)(lagoon_assembler_t*, la_vpr_t, la_vpr_t, la_vpr_t) = la_vfcmp_caf_s;
    switch (cond) {
    case 0x00: f = la_vfcmp_caf_s; break;
    case 0x02: f = la_vfcmp_clt_s; break;
    case 0x04: f = la_vfcmp_ceq_s; break;
    case 0x06: f = la_vfcmp_cle_s; break;
    case 0x08: f = la_vfcmp_cun_s; break;
    case 0x0A: f = la_vfcmp_cult_s; break;
    case 0x0C: f = la_vfcmp_cueq_s; break;
    case 0x0E: f = la_vfcmp_cule_s; break;
    case 0x10: f = la_vfcmp_cne_s; break;
    case 0x14: f = la_vfcmp_cor_s; break;
    case 0x18: f = la_vfcmp_cune_s; break;
    }

    f(as, xd, xj, xk);
    f(as, xd2, xj2, xk2);
}

void __gen_lasx_interpret_xvpickve2gr_wu_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int rd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int uk = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen-xvmap) xvpickve2gr_wu %08x \n", instr);

    if (uk < 4) {
        la_vpickve2gr_wu(as, (la_gpr_t)rd, xj, uk);
    } else {
        int xj2 = as->xvmap[xj];
        la_vpickve2gr_wu(as, (la_gpr_t)rd, xj2, uk - 4);
    }
}

void __gen_lasx_interpret_xvpermi_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xff;
    tdlog("                 : interpret(optgen-xvmap) xvpermi_d %08x \n", instr);

    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];

    if (xd == xj) {
        /* xd==xj -> xd2==xj2.  vshuf4i_d needs xd.old (= xj_lo)
           as source[0..1] and vj (= xj_hi) as source[2..3].
           Save xj/xj2, reload xj_lo into xj (low half) and
           xj_lo→xd2 + xj_hi→xj (high half). */
        la_vst(as, xj,  LA_TP, TD_OFF_DATA(xj, 0));
        la_vst(as, xj2, LA_TP, TD_OFF_DATA(xj, 2));

        la_vshuf4i_d(as, xd,    xj2, (imm >> 0) & 0xf);
        la_vst(as, xd, LA_TP, TD_OFF_DATA(xj, 2));
        la_vld(as, xd, LA_TP, TD_OFF_DATA(xj, 0));
        la_vshuf4i_d(as, xd,   xj2, (imm >> 4) & 0xf);
        la_vori_b(as, xd2, xd, 0);
        la_vld(as, xd, LA_TP, TD_OFF_DATA(xj, 2));
    } else {
        la_vori_b(as, xd, xj, 0);
        la_vshuf4i_d(as, xd, xj2, (imm >> 0) & 0xf);
        la_vori_b(as, xd2, xj, 0);
        la_vshuf4i_d(as, xd2, xj2, (imm >> 4) & 0xf);
    }
}

void __gen_lasx_interpret_xvperm_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int xk = (instr >> 10) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvperm_w %08x \n", instr);

    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    int xk2 = as->xvmap[xk];

    if (xd == xj) {
        /* xd==xj -> xd2==xj2.  VSHUF.W reads VR[vd].W[i][2:0]
           as selection index; must init xd/xd2 with xk/xk2.
           Save xj/xj2 to thread_data, use xk/xk2 as work regs
           (they are independent when xd==xj!=xk).
           When xd==xj==xk, only need to save xj/xj2,
           no work regs needed (xk2==saved_xj2). */

        if (xd != xk) {
            /* xd==xj!=xk: xk/xk2 independent, use as work regs */
            la_vst(as, xj,  LA_TP, TD_OFF_DATA(xj, 0));
            la_vst(as, xj2, LA_TP, TD_OFF_DATA(xj, 2));
            la_vst(as, xk,  LA_TP, TD_OFF_DATA(xk, 0));

            /* high half: sel=xk2->xd2, src={saved_xj2, saved_xj} */
            la_vori_b(as, xd2, xk2, 0);
            la_vld(as, xk,  LA_TP, TD_OFF_DATA(xj, 2));
            la_vld(as, xk2, LA_TP, TD_OFF_DATA(xj, 0));
            la_vshuf_w(as, xd2, xk, xk2);

            /* low half: sel=xk->xd, src={saved_xj2, saved_xj} */
            la_vld(as, xk,  LA_TP, TD_OFF_DATA(xk, 0));
            la_vori_b(as, xd, xk, 0);
            la_vld(as, xk,  LA_TP, TD_OFF_DATA(xj, 2));
            la_vld(as, xk2, LA_TP, TD_OFF_DATA(xj, 0));
            la_vshuf_w(as, xd, xk, xk2);
        } else {
            /* xd==xj==xk: all-equal.  Save xj_lo/xj_hi, low half first,
               then high half.  Need extra storage for low_result. */
            int excl_vr[] = { xj, xj2 };
            int vt;
            gen_reg_find_free_vrs(&vt, 1, excl_vr, 2);
            gen_reg_save_vr(as, vt);
            la_vori_b(as, vt, xj, 0);

            la_vst(as, xj2, LA_TP, TD_OFF_DATA(xj, 2));

            la_vshuf_w(as, xd, xj2, vt);

            la_vst(as, xd,  LA_TP, TD_OFF_DATA(xj, 0));

            la_vld(as, xj,  LA_TP, TD_OFF_DATA(xj, 2));
            la_vshuf_w(as, xd2, xj, vt);

            la_vld(as, xj,  LA_TP, TD_OFF_DATA(xj, 0));

            gen_reg_restore_vr(as, vt);
        }
    } else {
        if (xd != xk) la_vori_b(as, xd, xk, 0);
        la_vshuf_w(as, xd, xj2, xj);

        la_vori_b(as, xd2, xk2, 0);
        la_vshuf_w(as, xd2, xj2, xj);
    }
}

/* ================================================================
 * Macro definitions
 * ================================================================ */

#define GEN_XVMAP_3OP(name, lv_func)                                  \
void __gen_lasx_interpret_##name##_opt_xvmap(void *pas, unsigned int instr) \
{                                                                      \
    lagoon_assembler_t *as = pas;                                      \
    int xd = (instr >> 0) & 0x1f;                                      \
    int xj = (instr >> 5) & 0x1f;                                      \
    int xk = (instr >> 10) & 0x1f;                                     \
    tdlog("                 : interpret(optgen-xvmap) " #name " %08x \n", instr); \
    int xd2 = as->xvmap[xd];                                           \
    int xj2 = as->xvmap[xj];                                           \
    int xk2 = as->xvmap[xk];                                           \
    lv_func(as, xd, xj, xk);                                           \
    lv_func(as, xd2, xj2, xk2);                                        \
}

#define GEN_XVMAP_4OP(name, lv_func)                                    \
void __gen_lasx_interpret_##name##_opt_xvmap(void *pas, unsigned int instr) \
{                                                                      \
    lagoon_assembler_t *as = pas;                                      \
    int xd = (instr >> 0) & 0x1f;                                      \
    int xj = (instr >> 5) & 0x1f;                                      \
    int xk = (instr >> 10) & 0x1f;                                     \
    int xa = (instr >> 15) & 0x1f;                                     \
    tdlog("                 : interpret(optgen-xvmap) " #name " %08x \n", instr); \
    int xd2 = as->xvmap[xd];                                           \
    int xj2 = as->xvmap[xj];                                           \
    int xk2 = as->xvmap[xk];                                           \
    int xa2 = as->xvmap[xa];                                           \
    lv_func(as, xd, xj, xk, xa);                                       \
    lv_func(as, xd2, xj2, xk2, xa2);                                   \
}

#define GEN_XVMAP_2OP(name, lv_func)                                    \
void __gen_lasx_interpret_##name##_opt_xvmap(void *pas, unsigned int instr) \
{                                                                      \
    lagoon_assembler_t *as = pas;                                      \
    int xd = (instr >> 0) & 0x1f;                                      \
    int xj = (instr >> 5) & 0x1f;                                      \
    tdlog("                 : interpret(optgen-xvmap) " #name " %08x \n", instr); \
    int xd2 = as->xvmap[xd];                                           \
    int xj2 = as->xvmap[xj];                                           \
    lv_func(as, xd, xj);                                               \
    lv_func(as, xd2, xj2);                                             \
}

#define GEN_XVMAP_2OP_IMM(name, lv_func, imm_bits, imm_shift)          \
void __gen_lasx_interpret_##name##_opt_xvmap(void *pas, unsigned int instr) \
{                                                                      \
    lagoon_assembler_t *as = pas;                                      \
    int xd = (instr >> 0) & 0x1f;                                      \
    int xj = (instr >> 5) & 0x1f;                                      \
    int imm = (instr >> imm_shift) & ((1u << imm_bits) - 1);           \
    tdlog("                 : interpret(optgen-xvmap) " #name " %08x \n", instr); \
    int xd2 = as->xvmap[xd];                                           \
    int xj2 = as->xvmap[xj];                                           \
    lv_func(as, xd, xj, imm);                                          \
    lv_func(as, xd2, xj2, imm);                                        \
}

/* ================================================================
 * 3-operand (xd, xj, xk)
 * ================================================================ */

/* ===== Custom xvmap functions ===== */
void __gen_lasx_interpret_xvldrepl_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvldrepl_b %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int off = (instr >> 10) & 0xfff;
    off = (off << 20) >> 20;
    la_vldrepl_b(as, xd2, (la_gpr_t)rj, off);
    la_vldrepl_b(as, xd, (la_gpr_t)rj, off);
}

void __gen_lasx_interpret_xvldrepl_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvldrepl_h %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int off = (instr >> 10) & 0x7ff;
    off = (off << 21) >> 21;
    la_vldrepl_h(as, xd2, (la_gpr_t)rj, off << 1);
    la_vldrepl_h(as, xd, (la_gpr_t)rj, off << 1);
}

void __gen_lasx_interpret_xvstelm_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xff;
    int elem_idx = (instr >> 18) & 31;
    tdlog("                 : interpret(optgen-xvmap) xvstelm_b %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    if (elem_idx < 16) {
        la_vstelm_b(as, xd, (la_gpr_t)rj, imm, elem_idx);
    } else {
        la_vstelm_b(as, xd2, (la_gpr_t)rj, imm, elem_idx - 16);
    }
}

void __gen_lasx_interpret_xvstelm_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xff;
    int elem_idx = (instr >> 18) & 15;
    tdlog("                 : interpret(optgen-xvmap) xvstelm_h %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    /* la_vstelm_h divides imm by 2, compensate with << 1 */
    if (elem_idx < 8) {
        la_vstelm_h(as, xd, (la_gpr_t)rj, imm << 1, elem_idx);
    } else {
        la_vstelm_h(as, xd2, (la_gpr_t)rj, imm << 1, elem_idx - 8);
    }
}

void __gen_lasx_interpret_xvpickve_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int idx = (instr >> 10) & 3;
    tdlog("                 : interpret(optgen-xvmap) xvpickve_d %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vxor_v(as, xd, xd, xd);
    la_vxor_v(as, xd2, xd2, xd2);
    if (idx < 2) {
        la_vpickve2gr_d(as, LA_RX, xj, idx);
    } else {
        la_vpickve2gr_d(as, LA_RX, xj2, idx - 2);
    }
    la_vinsgr2vr_d(as, xd, LA_RX, 0);
}
void __gen_lasx_interpret_xvpickve_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int idx = (instr >> 10) & 7;
    tdlog("                 : interpret(optgen-xvmap) xvpickve_w %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vxor_v(as, xd, xd, xd);
    la_vxor_v(as, xd2, xd2, xd2);
    if (idx < 4) {
        la_vpickve2gr_w(as, LA_RX, xj, idx);
    } else {
        la_vpickve2gr_w(as, LA_RX, xj2, idx - 4);
    }
    la_vinsgr2vr_w(as, xd, LA_RX, 0);
}
void __gen_lasx_interpret_xvpickve2gr_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int rd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int uk = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen-xvmap) xvpickve2gr_d %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    if (uk < 2) {
        la_vpickve2gr_d(as, (la_gpr_t)rd, xj, uk);
    } else {
        la_vpickve2gr_d(as, (la_gpr_t)rd, xj2, uk - 2);
    }
}

void __gen_lasx_interpret_xvpickve2gr_du_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int rd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int uk = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen-xvmap) xvpickve2gr_du %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    if (uk < 2) {
        la_vpickve2gr_du(as, (la_gpr_t)rd, xj, uk);
    } else {
        la_vpickve2gr_du(as, (la_gpr_t)rd, xj2, uk - 2);
    }
}

void __gen_lasx_interpret_xvsetallnez_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvsetallnez_b %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vsetallnez_b(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vsetallnez_b(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_and(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvsetallnez_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvsetallnez_d %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vsetallnez_d(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vsetallnez_d(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_and(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvsetallnez_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvsetallnez_h %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vsetallnez_h(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vsetallnez_h(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_and(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvsetallnez_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvsetallnez_w %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vsetallnez_w(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vsetallnez_w(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_and(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvsetanyeqz_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvsetanyeqz_b %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vsetanyeqz_b(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vsetanyeqz_b(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_or(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvsetanyeqz_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvsetanyeqz_d %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vsetanyeqz_d(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vsetanyeqz_d(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_or(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvsetanyeqz_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvsetanyeqz_h %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vsetanyeqz_h(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vsetanyeqz_h(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_or(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvsetanyeqz_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvsetanyeqz_w %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vsetanyeqz_w(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vsetanyeqz_w(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_or(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvseteqz_v_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvseteqz_v %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vseteqz_v(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vseteqz_v(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_and(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvsetnez_v_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int cd = (instr >> 0) & 0x7;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvsetnez_v %08x \\n", instr);
    int xj2 = as->xvmap[xj];
    int gr[2];
    gen_reg_find_free_grs(gr, 2, NULL, 0);
    gen_reg_save_gr(as, gr[0]);
    gen_reg_save_gr(as, gr[1]);
    la_vsetnez_v(as, LA_FCC0 + cd, xj);
    la_movcf2gr(as, (la_gpr_t)gr[0], LA_FCC0 + cd);
    la_vsetnez_v(as, LA_FCC0 + cd, xj2);
    la_movcf2gr(as, (la_gpr_t)gr[1], LA_FCC0 + cd);
    la_or(as, (la_gpr_t)gr[0], (la_gpr_t)gr[0], (la_gpr_t)gr[1]);
    la_movgr2cf(as, LA_FCC0 + cd, (la_gpr_t)gr[0]);
    gen_reg_restore_gr(as, gr[0]);
    gen_reg_restore_gr(as, gr[1]);
}

void __gen_lasx_interpret_xvreplve0_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplve0_b %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vreplvei_b(as, xd, xj, 0);
    la_vreplvei_b(as, xd2, xj, 0);
}

void __gen_lasx_interpret_xvreplve0_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplve0_d %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vreplvei_d(as, xd, xj, 0);
    la_vreplvei_d(as, xd2, xj, 0);
}

void __gen_lasx_interpret_xvreplve0_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplve0_h %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vreplvei_h(as, xd, xj, 0);
    la_vreplvei_h(as, xd2, xj, 0);
}

void __gen_lasx_interpret_xvreplve0_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplve0_w %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vreplvei_w(as, xd, xj, 0);
    la_vreplvei_w(as, xd2, xj, 0);
}

void __gen_lasx_interpret_xvreplve0_q_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplve0_q %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vori_b(as, xd, xj, 0);
    la_vori_b(as, xd2, xj, 0);
}

void __gen_lasx_interpret_xvreplve_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int rk = (instr >> 10) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplve_b %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vreplve_b(as, xd, xj, (la_gpr_t)rk);
    la_vreplve_b(as, xd2, xj2, (la_gpr_t)rk);
}

void __gen_lasx_interpret_xvreplve_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int rk = (instr >> 10) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplve_d %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vreplve_d(as, xd, xj, (la_gpr_t)rk);
    la_vreplve_d(as, xd2, xj2, (la_gpr_t)rk);
}

void __gen_lasx_interpret_xvreplve_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int rk = (instr >> 10) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplve_h %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vreplve_h(as, xd, xj, (la_gpr_t)rk);
    la_vreplve_h(as, xd2, xj2, (la_gpr_t)rk);
}

void __gen_lasx_interpret_xvreplve_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int rk = (instr >> 10) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplve_w %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vreplve_w(as, xd, xj, (la_gpr_t)rk);
    la_vreplve_w(as, xd2, xj2, (la_gpr_t)rk);
}

void __gen_lasx_interpret_vext2xv_d_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_d_b %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_h_b(as, xd, xd);
    la_vshuf4i_d(as, xd, xd, (3 << 0) | (2 << 2));
    la_vexth_w_h(as, xd, xd);
    la_vexth_d_w(as, xd2, xd);
    la_vshuf4i_d(as, xd, xd, (3 << 0) | (2 << 2));
    la_vexth_d_w(as, xd, xd);
}

void __gen_lasx_interpret_vext2xv_d_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_d_h %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_w_h(as, xd, xd);
    la_vexth_d_w(as, xd2, xd);
    la_vshuf4i_d(as, xd, xd, (3 << 0) | (2 << 2));
    la_vexth_d_w(as, xd, xd);
}

void __gen_lasx_interpret_vext2xv_du_bu_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_du_bu %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_hu_bu(as, xd, xd);
    la_vshuf4i_d(as, xd, xd, (3 << 0) | (2 << 2));
    la_vexth_wu_hu(as, xd, xd);
    la_vexth_du_wu(as, xd2, xd);
    la_vshuf4i_d(as, xd, xd, (3 << 0) | (2 << 2));
    la_vexth_du_wu(as, xd, xd);
}

void __gen_lasx_interpret_vext2xv_du_hu_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_du_hu %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_wu_hu(as, xd, xd);
    la_vexth_du_wu(as, xd2, xd);
    la_vshuf4i_d(as, xd, xd, (3 << 0) | (2 << 2));
    la_vexth_du_wu(as, xd, xd);
}

void __gen_lasx_interpret_vext2xv_du_wu_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_du_wu %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vexth_du_wu(as, xd2, xj);
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_du_wu(as, xd, xd);
}

void __gen_lasx_interpret_vext2xv_d_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_d_w %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vexth_d_w(as, xd2, xj);
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_d_w(as, xd, xd);
}

void __gen_lasx_interpret_vext2xv_w_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_w_b %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_h_b(as, xd, xd);
    la_vexth_w_h(as, xd2, xd);
    la_vshuf4i_d(as, xd, xd, (3 << 0) | (2 << 2));
    la_vexth_w_h(as, xd, xd);
}

GEN_XVMAP_4OP(xvfnmsub_d, la_vfnmsub_d)
GEN_XVMAP_4OP(xvfmsub_d, la_vfmsub_d)
GEN_XVMAP_4OP(xvfnmadd_s, la_vfnmadd_s)
GEN_XVMAP_4OP(xvfnmadd_d, la_vfnmadd_d)

void __gen_lasx_interpret_xvrepl128vei_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0xf;
    tdlog("                 : interpret(optgen-xvmap) xvrepl128vei_b %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vreplvei_b(as, xd, xj, imm);
    la_vreplvei_b(as, xd2, xj2, imm);
}

void __gen_lasx_interpret_xvrepl128vei_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen-xvmap) xvrepl128vei_h %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vreplvei_h(as, xd, xj, imm);
    la_vreplvei_h(as, xd2, xj2, imm);
}

void __gen_lasx_interpret_xvrepl128vei_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen-xvmap) xvrepl128vei_w %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vreplvei_w(as, xd, xj, imm);
    la_vreplvei_w(as, xd2, xj2, imm);
}

void __gen_lasx_interpret_xvrepl128vei_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0x1;
    tdlog("                 : interpret(optgen-xvmap) xvrepl128vei_d %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    la_vreplvei_d(as, xd, xj, imm);
    la_vreplvei_d(as, xd2, xj2, imm);
}

void __gen_lasx_interpret_xvinsve0_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen-xvmap) xvinsve0_d %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vpickve2gr_d(as, LA_RX, xj, 0);
    if (imm < 2) {
        la_vinsgr2vr_d(as, xd, LA_RX, imm);
    } else {
        la_vinsgr2vr_d(as, xd2, LA_RX, imm - 2);
    }
}

void __gen_lasx_interpret_xvinsve0_w_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int imm = (instr >> 10) & 0x7;
    tdlog("                 : interpret(optgen-xvmap) xvinsve0_w %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vpickve2gr_w(as, LA_RX, xj, 0);
    if (imm < 4) {
        la_vinsgr2vr_w(as, xd, LA_RX, imm);
    } else {
        la_vinsgr2vr_w(as, xd2, LA_RX, imm - 4);
    }
}

void __gen_lasx_interpret_xvldi_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int imm = (instr >> 5) & 0x1fff;
    tdlog("                 : interpret(optgen-xvmap) xvldi %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vldi(as, xd, imm);
    la_vldi(as, xd2, imm);
}

void __gen_lasx_interpret_xvldrepl_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvldrepl_d %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int off = (instr >> 10) & 0x1ff;
    off = (off << 23) >> 23;
    la_vldrepl_d(as, xd2, (la_gpr_t)rj, off << 3);
    la_vldrepl_d(as, xd, (la_gpr_t)rj, off << 3);
}

void __gen_lasx_interpret_xvinsgr2vr_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    int idx = (instr >> 10) & 0x3;
    tdlog("                 : interpret(optgen-xvmap) xvinsgr2vr_d %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    if (idx < 2) {
        la_vinsgr2vr_d(as, xd, (la_gpr_t)rj, idx);
    } else {
        la_vinsgr2vr_d(as, xd2, (la_gpr_t)rj, idx - 2);
    }
}

void __gen_lasx_interpret_xvreplgr2vr_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int rj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvreplgr2vr_b %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    la_vreplgr2vr_b(as, xd, rj);
    la_vreplgr2vr_b(as, xd2, rj);
}

void __gen_lasx_interpret_xvfcmp_cond_d_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int xk = (instr >> 10) & 0x1f;
    int cond = (instr >> 15) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) xvfcmp_cond_d %08x \\n", instr);
    int xd2 = as->xvmap[xd];
    int xj2 = as->xvmap[xj];
    int xk2 = as->xvmap[xk];
    void (*f)(lagoon_assembler_t*, la_vpr_t, la_vpr_t, la_vpr_t) = la_vfcmp_caf_d;
    switch (cond) {
    case 0x00: f = la_vfcmp_caf_d; break;
    case 0x02: f = la_vfcmp_clt_d; break;
    case 0x04: f = la_vfcmp_ceq_d; break;
    case 0x06: f = la_vfcmp_cle_d; break;
    case 0x08: f = la_vfcmp_cun_d; break;
    case 0x0A: f = la_vfcmp_cult_d; break;
    case 0x0C: f = la_vfcmp_cueq_d; break;
    case 0x0E: f = la_vfcmp_cule_d; break;
    case 0x10: f = la_vfcmp_cne_d; break;
    case 0x14: f = la_vfcmp_cor_d; break;
    case 0x18: f = la_vfcmp_cune_d; break;
    }
    f(as, xd, xj, xk);
    f(as, xd2, xj2, xk2);
}

GEN_XVMAP_3OP(xvavgr_hu, la_vavgr_hu)
GEN_XVMAP_3OP(xvavgr_wu, la_vavgr_wu)
GEN_XVMAP_3OP(xvmul_h, la_vmul_h)
GEN_XVMAP_3OP(xvmul_w, la_vmul_w)
GEN_XVMAP_3OP(xvmadd_h, la_vmadd_h)
GEN_XVMAP_3OP(xvmadd_w, la_vmadd_w)
GEN_XVMAP_3OP(xvadd_h, la_vadd_h)
GEN_XVMAP_3OP(xvadd_w, la_vadd_w)

GEN_XVMAP_3OP(xvilvl_b, la_vilvl_b)
GEN_XVMAP_3OP(xvilvl_h, la_vilvl_h)
GEN_XVMAP_3OP(xvilvl_w, la_vilvl_w)
GEN_XVMAP_3OP(xvilvl_d, la_vilvl_d)
GEN_XVMAP_3OP(xvilvh_b, la_vilvh_b)
GEN_XVMAP_3OP(xvilvh_h, la_vilvh_h)
GEN_XVMAP_3OP(xvilvh_w, la_vilvh_w)
GEN_XVMAP_3OP(xvilvh_d, la_vilvh_d)

GEN_XVMAP_3OP(xvor_v, la_vor_v)
GEN_XVMAP_3OP(xvand_v, la_vand_v)
GEN_XVMAP_3OP(xvxor_v, la_vxor_v)
GEN_XVMAP_3OP(xvnor_v, la_vnor_v)
GEN_XVMAP_3OP(xvsub_b, la_vsub_b)
GEN_XVMAP_3OP(xvsub_h, la_vsub_h)
GEN_XVMAP_3OP(xvsub_w, la_vsub_w)
GEN_XVMAP_3OP(xvsll_b, la_vsll_b)
GEN_XVMAP_3OP(xvsrl_b, la_vsrl_b)
GEN_XVMAP_3OP(xvsrl_w, la_vsrl_w)
GEN_XVMAP_3OP(xvslt_w, la_vslt_w)
GEN_XVMAP_3OP(xvsadd_bu, la_vsadd_bu)
GEN_XVMAP_3OP(xvssub_bu, la_vssub_bu)
GEN_XVMAP_3OP(xvmin_wu, la_vmin_wu)
GEN_XVMAP_3OP(xvavgr_bu, la_vavgr_bu)
GEN_XVMAP_3OP(xvmsub_h, la_vmsub_h)
GEN_XVMAP_3OP(xvmuh_hu, la_vmuh_hu)

GEN_XVMAP_3OP(xvfadd_s, la_vfadd_s)
GEN_XVMAP_3OP(xvfsub_s, la_vfsub_s)
GEN_XVMAP_3OP(xvfmax_s, la_vfmax_s)
GEN_XVMAP_3OP(xvfmin_s, la_vfmin_s)
GEN_XVMAP_3OP(xvfadd_d, la_vfadd_d)
GEN_XVMAP_3OP(xvfmul_d, la_vfmul_d)
GEN_XVMAP_3OP(xvfcvt_s_d, la_vfcvt_s_d)

GEN_XVMAP_3OP(xvshuf_h, la_vshuf_h)

GEN_XVMAP_3OP(xvfcmp_cle_s, la_vfcmp_cle_s)
GEN_XVMAP_3OP(xvfcmp_clt_s, la_vfcmp_clt_s)

GEN_XVMAP_3OP(xvpickev_b, la_vpickev_b)
GEN_XVMAP_3OP(xvpickev_h, la_vpickev_h)
GEN_XVMAP_3OP(xvpickod_b, la_vpickod_b)
GEN_XVMAP_3OP(xvpickod_h, la_vpickod_h)
GEN_XVMAP_3OP(xvpackev_b, la_vpackev_b)
GEN_XVMAP_3OP(xvpackev_h, la_vpackev_h)
GEN_XVMAP_3OP(xvpackod_b, la_vpackod_b)
GEN_XVMAP_3OP(xvpackod_h, la_vpackod_h)

GEN_XVMAP_3OP(xvmaddwev_h_bu, la_vmaddwev_h_bu)
GEN_XVMAP_3OP(xvmaddwod_h_bu, la_vmaddwod_h_bu)
GEN_XVMAP_3OP(xvmulwev_w_h, la_vmulwev_w_h)
GEN_XVMAP_3OP(xvmaddwod_w_h, la_vmaddwod_w_h)
GEN_XVMAP_3OP(xvmulwev_h_bu, la_vmulwev_h_bu)
GEN_XVMAP_3OP(xvmulwod_h_bu, la_vmulwod_h_bu)
GEN_XVMAP_3OP(xvmulwev_w_hu, la_vmulwev_w_hu)
GEN_XVMAP_3OP(xvmulwod_w_hu, la_vmulwod_w_hu)
GEN_XVMAP_3OP(xvmulwev_w_hu_h, la_vmulwev_w_hu_h)
GEN_XVMAP_3OP(xvmulwod_w_hu_h, la_vmulwod_w_hu_h)
GEN_XVMAP_3OP(xvmulwod_w_h, la_vmulwod_w_h)

GEN_XVMAP_3OP(xvaddwev_h_bu, la_vaddwev_h_bu)
GEN_XVMAP_3OP(xvaddwod_h_bu, la_vaddwod_h_bu)

GEN_XVMAP_3OP(xvhaddw_wu_hu, la_vhaddw_wu_hu)
GEN_XVMAP_3OP(xvhaddw_hu_bu, la_vhaddw_hu_bu)

/* ================================================================
 * 4-operand (xd, xj, xk, xa)
 * ================================================================ */

GEN_XVMAP_4OP(xvfmadd_s, la_vfmadd_s)
GEN_XVMAP_4OP(xvfnmsub_s, la_vfnmsub_s)
GEN_XVMAP_4OP(xvfmsub_s, la_vfmsub_s)
GEN_XVMAP_4OP(xvfmadd_d, la_vfmadd_d)
GEN_XVMAP_4OP(xvbitsel_v, la_vbitsel_v)
GEN_XVMAP_4OP(xvshuf_b, la_vshuf_b)

/* ================================================================
 * 2-operand + immediate (xd, xj, imm)
 * ================================================================ */

GEN_XVMAP_2OP_IMM(xvori_b, la_vori_b, 8, 10)
GEN_XVMAP_2OP_IMM(xvandi_b, la_vandi_b, 8, 10)

GEN_XVMAP_2OP_IMM(xvsrli_h, la_vsrli_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvsrli_w, la_vsrli_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvsrli_b, la_vsrli_b, 3, 10)

GEN_XVMAP_2OP_IMM(xvslli_b, la_vslli_b, 3, 10)
GEN_XVMAP_2OP_IMM(xvslli_h, la_vslli_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvslli_w, la_vslli_w, 5, 10)

GEN_XVMAP_2OP_IMM(xvsrai_h, la_vsrai_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvsrai_w, la_vsrai_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvsrari_h, la_vsrari_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvsrari_w, la_vsrari_w, 5, 10)

GEN_XVMAP_2OP_IMM(xvmaxi_h, la_vmaxi_h, 5, 10)
GEN_XVMAP_2OP_IMM(xvmaxi_w, la_vmaxi_w, 5, 10)

GEN_XVMAP_2OP_IMM(xvaddi_wu, la_vaddi_wu, 5, 10)
GEN_XVMAP_2OP_IMM(xvaddi_hu, la_vaddi_hu, 5, 10)

GEN_XVMAP_2OP_IMM(xvsat_wu, la_vsat_wu, 5, 10)
GEN_XVMAP_2OP_IMM(xvsat_hu, la_vsat_hu, 4, 10)

GEN_XVMAP_2OP_IMM(xvbsrl_v, la_vbsrl_v, 5, 10)
GEN_XVMAP_2OP_IMM(xvssrani_h_w, la_vssrani_h_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvssrani_b_h, la_vssrani_b_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvssrarni_bu_h, la_vssrarni_bu_h, 4, 10)

/* ================================================================
 * 2-operand (xd, xj)
 * ================================================================ */

/* vext2xv.h.b: byte→half sign extension, low 128 bits → 16 half-words */
void __gen_lasx_interpret_vext2xv_h_b_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_h_b %08x \n", instr);

    int xd2 = as->xvmap[xd];

    la_vexth_h_b(as, xd2, xj);
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_h_b(as, xd, xd);
}

/* vext2xv.w.h: half→word sign extension, low 128 bits → 8 words */
void __gen_lasx_interpret_vext2xv_w_h_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_w_h %08x \n", instr);

    int xd2 = as->xvmap[xd];

    la_vexth_w_h(as, xd2, xj);
    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_w_h(as, xd, xd);
}

/* vext2xv.wu.bu: byte→half→word unsigned, low 128 bits → 8 words */
void __gen_lasx_interpret_vext2xv_wu_bu_opt_xvmap(void *pas, unsigned int instr)
{
    lagoon_assembler_t *as = pas;
    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    tdlog("                 : interpret(optgen-xvmap) vext2xv_wu_bu %08x \n", instr);

    int xd2 = as->xvmap[xd];

    la_vshuf4i_d(as, xd, xj, (3 << 0) | (2 << 2));
    la_vexth_hu_bu(as, xd, xd);
    la_vexth_wu_hu(as, xd2, xd);
    la_vshuf4i_d(as, xd, xd, (3 << 0) | (2 << 2));
    la_vexth_wu_hu(as, xd, xd);
}

GEN_XVMAP_3OP(xvfmul_s, la_vfmul_s)

/* === Auto: missing 3OP === */
GEN_XVMAP_3OP(xvabsd_b, la_vabsd_b)
GEN_XVMAP_3OP(xvabsd_bu, la_vabsd_bu)
GEN_XVMAP_3OP(xvabsd_d, la_vabsd_d)
GEN_XVMAP_3OP(xvabsd_du, la_vabsd_du)
GEN_XVMAP_3OP(xvabsd_h, la_vabsd_h)
GEN_XVMAP_3OP(xvabsd_hu, la_vabsd_hu)
GEN_XVMAP_3OP(xvabsd_w, la_vabsd_w)
GEN_XVMAP_3OP(xvabsd_wu, la_vabsd_wu)
GEN_XVMAP_3OP(xvadda_b, la_vadda_b)
GEN_XVMAP_3OP(xvadda_d, la_vadda_d)
GEN_XVMAP_3OP(xvadda_h, la_vadda_h)
GEN_XVMAP_3OP(xvadda_w, la_vadda_w)
GEN_XVMAP_3OP(xvaddwev_d_w, la_vaddwev_d_w)
GEN_XVMAP_3OP(xvaddwev_d_wu, la_vaddwev_d_wu)
GEN_XVMAP_3OP(xvaddwev_d_wu_w, la_vaddwev_d_wu_w)
GEN_XVMAP_3OP(xvaddwev_h_b, la_vaddwev_h_b)
GEN_XVMAP_3OP(xvaddwev_h_bu_b, la_vaddwev_h_bu_b)
GEN_XVMAP_3OP(xvaddwev_q_d, la_vaddwev_q_d)
GEN_XVMAP_3OP(xvaddwev_q_du, la_vaddwev_q_du)
GEN_XVMAP_3OP(xvaddwev_q_du_d, la_vaddwev_q_du_d)
GEN_XVMAP_3OP(xvaddwev_w_h, la_vaddwev_w_h)
GEN_XVMAP_3OP(xvaddwev_w_hu, la_vaddwev_w_hu)
GEN_XVMAP_3OP(xvaddwev_w_hu_h, la_vaddwev_w_hu_h)
GEN_XVMAP_3OP(xvaddwod_d_w, la_vaddwod_d_w)
GEN_XVMAP_3OP(xvaddwod_d_wu, la_vaddwod_d_wu)
GEN_XVMAP_3OP(xvaddwod_d_wu_w, la_vaddwod_d_wu_w)
GEN_XVMAP_3OP(xvaddwod_h_b, la_vaddwod_h_b)
GEN_XVMAP_3OP(xvaddwod_h_bu_b, la_vaddwod_h_bu_b)
GEN_XVMAP_3OP(xvaddwod_q_d, la_vaddwod_q_d)
GEN_XVMAP_3OP(xvaddwod_q_du, la_vaddwod_q_du)
GEN_XVMAP_3OP(xvaddwod_q_du_d, la_vaddwod_q_du_d)
GEN_XVMAP_3OP(xvaddwod_w_h, la_vaddwod_w_h)
GEN_XVMAP_3OP(xvaddwod_w_hu, la_vaddwod_w_hu)
GEN_XVMAP_3OP(xvaddwod_w_hu_h, la_vaddwod_w_hu_h)
GEN_XVMAP_3OP(xvadd_b, la_vadd_b)
GEN_XVMAP_3OP(xvadd_d, la_vadd_d)
GEN_XVMAP_3OP(xvadd_q, la_vadd_q)
GEN_XVMAP_3OP(xvandn_v, la_vandn_v)
GEN_XVMAP_3OP(xvavgr_b, la_vavgr_b)
GEN_XVMAP_3OP(xvavgr_d, la_vavgr_d)
GEN_XVMAP_3OP(xvavgr_du, la_vavgr_du)
GEN_XVMAP_3OP(xvavgr_h, la_vavgr_h)
GEN_XVMAP_3OP(xvavgr_w, la_vavgr_w)
GEN_XVMAP_3OP(xvavg_b, la_vavg_b)
GEN_XVMAP_3OP(xvavg_bu, la_vavg_bu)
GEN_XVMAP_3OP(xvavg_d, la_vavg_d)
GEN_XVMAP_3OP(xvavg_du, la_vavg_du)
GEN_XVMAP_3OP(xvavg_h, la_vavg_h)
GEN_XVMAP_3OP(xvavg_hu, la_vavg_hu)
GEN_XVMAP_3OP(xvavg_w, la_vavg_w)
GEN_XVMAP_3OP(xvavg_wu, la_vavg_wu)
GEN_XVMAP_3OP(xvbitclr_b, la_vbitclr_b)
GEN_XVMAP_3OP(xvbitclr_d, la_vbitclr_d)
GEN_XVMAP_3OP(xvbitclr_h, la_vbitclr_h)
GEN_XVMAP_3OP(xvbitclr_w, la_vbitclr_w)
GEN_XVMAP_3OP(xvbitrev_b, la_vbitrev_b)
GEN_XVMAP_3OP(xvbitrev_d, la_vbitrev_d)
GEN_XVMAP_3OP(xvbitrev_h, la_vbitrev_h)
GEN_XVMAP_3OP(xvbitrev_w, la_vbitrev_w)
GEN_XVMAP_3OP(xvbitset_b, la_vbitset_b)
GEN_XVMAP_3OP(xvbitset_d, la_vbitset_d)
GEN_XVMAP_3OP(xvbitset_h, la_vbitset_h)
GEN_XVMAP_3OP(xvbitset_w, la_vbitset_w)
GEN_XVMAP_3OP(xvdiv_b, la_vdiv_b)
GEN_XVMAP_3OP(xvdiv_bu, la_vdiv_bu)
GEN_XVMAP_3OP(xvdiv_d, la_vdiv_d)
GEN_XVMAP_3OP(xvdiv_du, la_vdiv_du)
GEN_XVMAP_3OP(xvdiv_h, la_vdiv_h)
GEN_XVMAP_3OP(xvdiv_hu, la_vdiv_hu)
GEN_XVMAP_3OP(xvdiv_w, la_vdiv_w)
GEN_XVMAP_3OP(xvdiv_wu, la_vdiv_wu)
GEN_XVMAP_3OP(xvfcvt_h_s, la_vfcvt_h_s)
GEN_XVMAP_3OP(xvfdiv_d, la_vfdiv_d)
GEN_XVMAP_3OP(xvfdiv_s, la_vfdiv_s)
GEN_XVMAP_3OP(xvffint_s_l, la_vffint_s_l)
GEN_XVMAP_3OP(xvfmaxa_d, la_vfmaxa_d)
GEN_XVMAP_3OP(xvfmaxa_s, la_vfmaxa_s)
GEN_XVMAP_3OP(xvfmax_d, la_vfmax_d)
GEN_XVMAP_3OP(xvfmina_d, la_vfmina_d)
GEN_XVMAP_3OP(xvfmina_s, la_vfmina_s)
GEN_XVMAP_3OP(xvfmin_d, la_vfmin_d)
GEN_XVMAP_3OP(xvfrstp_b, la_vfrstp_b)
GEN_XVMAP_3OP(xvfrstp_h, la_vfrstp_h)
GEN_XVMAP_3OP(xvfsub_d, la_vfsub_d)
GEN_XVMAP_3OP(xvftintrm_w_d, la_vftintrm_w_d)
GEN_XVMAP_3OP(xvftintrne_w_d, la_vftintrne_w_d)
GEN_XVMAP_3OP(xvftintrp_w_d, la_vftintrp_w_d)
GEN_XVMAP_3OP(xvftintrz_w_d, la_vftintrz_w_d)
GEN_XVMAP_3OP(xvftint_w_d, la_vftint_w_d)
GEN_XVMAP_3OP(xvhaddw_du_wu, la_vhaddw_du_wu)
GEN_XVMAP_3OP(xvhaddw_d_w, la_vhaddw_d_w)
GEN_XVMAP_3OP(xvhaddw_h_b, la_vhaddw_h_b)
GEN_XVMAP_3OP(xvhaddw_qu_du, la_vhaddw_qu_du)
GEN_XVMAP_3OP(xvhaddw_q_d, la_vhaddw_q_d)
GEN_XVMAP_3OP(xvhaddw_w_h, la_vhaddw_w_h)
GEN_XVMAP_3OP(xvhsubw_du_wu, la_vhsubw_du_wu)
GEN_XVMAP_3OP(xvhsubw_d_w, la_vhsubw_d_w)
GEN_XVMAP_3OP(xvhsubw_hu_bu, la_vhsubw_hu_bu)
GEN_XVMAP_3OP(xvhsubw_h_b, la_vhsubw_h_b)
GEN_XVMAP_3OP(xvhsubw_qu_du, la_vhsubw_qu_du)
GEN_XVMAP_3OP(xvhsubw_q_d, la_vhsubw_q_d)
GEN_XVMAP_3OP(xvhsubw_wu_hu, la_vhsubw_wu_hu)
GEN_XVMAP_3OP(xvhsubw_w_h, la_vhsubw_w_h)
GEN_XVMAP_3OP(xvmaddwev_d_w, la_vmaddwev_d_w)
GEN_XVMAP_3OP(xvmaddwev_d_wu, la_vmaddwev_d_wu)
GEN_XVMAP_3OP(xvmaddwev_d_wu_w, la_vmaddwev_d_wu_w)
GEN_XVMAP_3OP(xvmaddwev_h_b, la_vmaddwev_h_b)
GEN_XVMAP_3OP(xvmaddwev_h_bu_b, la_vmaddwev_h_bu_b)
GEN_XVMAP_3OP(xvmaddwev_q_d, la_vmaddwev_q_d)
GEN_XVMAP_3OP(xvmaddwev_q_du, la_vmaddwev_q_du)
GEN_XVMAP_3OP(xvmaddwev_q_du_d, la_vmaddwev_q_du_d)
GEN_XVMAP_3OP(xvmaddwev_w_h, la_vmaddwev_w_h)
GEN_XVMAP_3OP(xvmaddwev_w_hu, la_vmaddwev_w_hu)
GEN_XVMAP_3OP(xvmaddwev_w_hu_h, la_vmaddwev_w_hu_h)
GEN_XVMAP_3OP(xvmaddwod_d_w, la_vmaddwod_d_w)
GEN_XVMAP_3OP(xvmaddwod_d_wu, la_vmaddwod_d_wu)
GEN_XVMAP_3OP(xvmaddwod_d_wu_w, la_vmaddwod_d_wu_w)
GEN_XVMAP_3OP(xvmaddwod_h_b, la_vmaddwod_h_b)
GEN_XVMAP_3OP(xvmaddwod_h_bu_b, la_vmaddwod_h_bu_b)
GEN_XVMAP_3OP(xvmaddwod_q_d, la_vmaddwod_q_d)
GEN_XVMAP_3OP(xvmaddwod_q_du, la_vmaddwod_q_du)
GEN_XVMAP_3OP(xvmaddwod_q_du_d, la_vmaddwod_q_du_d)
GEN_XVMAP_3OP(xvmaddwod_w_hu, la_vmaddwod_w_hu)
GEN_XVMAP_3OP(xvmaddwod_w_hu_h, la_vmaddwod_w_hu_h)
GEN_XVMAP_3OP(xvmadd_b, la_vmadd_b)
GEN_XVMAP_3OP(xvmadd_d, la_vmadd_d)
GEN_XVMAP_3OP(xvmax_b, la_vmax_b)
GEN_XVMAP_3OP(xvmax_bu, la_vmax_bu)
GEN_XVMAP_3OP(xvmax_d, la_vmax_d)
GEN_XVMAP_3OP(xvmax_du, la_vmax_du)
GEN_XVMAP_3OP(xvmax_h, la_vmax_h)
GEN_XVMAP_3OP(xvmax_hu, la_vmax_hu)
GEN_XVMAP_3OP(xvmax_w, la_vmax_w)
GEN_XVMAP_3OP(xvmax_wu, la_vmax_wu)
GEN_XVMAP_3OP(xvmin_b, la_vmin_b)
GEN_XVMAP_3OP(xvmin_bu, la_vmin_bu)
GEN_XVMAP_3OP(xvmin_d, la_vmin_d)
GEN_XVMAP_3OP(xvmin_du, la_vmin_du)
GEN_XVMAP_3OP(xvmin_h, la_vmin_h)
GEN_XVMAP_3OP(xvmin_hu, la_vmin_hu)
GEN_XVMAP_3OP(xvmin_w, la_vmin_w)
GEN_XVMAP_3OP(xvmod_b, la_vmod_b)
GEN_XVMAP_3OP(xvmod_bu, la_vmod_bu)
GEN_XVMAP_3OP(xvmod_d, la_vmod_d)
GEN_XVMAP_3OP(xvmod_du, la_vmod_du)
GEN_XVMAP_3OP(xvmod_h, la_vmod_h)
GEN_XVMAP_3OP(xvmod_hu, la_vmod_hu)
GEN_XVMAP_3OP(xvmod_w, la_vmod_w)
GEN_XVMAP_3OP(xvmod_wu, la_vmod_wu)
GEN_XVMAP_3OP(xvmsub_b, la_vmsub_b)
GEN_XVMAP_3OP(xvmsub_d, la_vmsub_d)
GEN_XVMAP_3OP(xvmsub_w, la_vmsub_w)
GEN_XVMAP_3OP(xvmuh_b, la_vmuh_b)
GEN_XVMAP_3OP(xvmuh_bu, la_vmuh_bu)
GEN_XVMAP_3OP(xvmuh_d, la_vmuh_d)
GEN_XVMAP_3OP(xvmuh_du, la_vmuh_du)
GEN_XVMAP_3OP(xvmuh_h, la_vmuh_h)
GEN_XVMAP_3OP(xvmuh_w, la_vmuh_w)
GEN_XVMAP_3OP(xvmuh_wu, la_vmuh_wu)
GEN_XVMAP_3OP(xvmulwev_d_w, la_vmulwev_d_w)
GEN_XVMAP_3OP(xvmulwev_d_wu, la_vmulwev_d_wu)
GEN_XVMAP_3OP(xvmulwev_d_wu_w, la_vmulwev_d_wu_w)
GEN_XVMAP_3OP(xvmulwev_h_b, la_vmulwev_h_b)
GEN_XVMAP_3OP(xvmulwev_h_bu_b, la_vmulwev_h_bu_b)
GEN_XVMAP_3OP(xvmulwev_q_d, la_vmulwev_q_d)
GEN_XVMAP_3OP(xvmulwev_q_du, la_vmulwev_q_du)
GEN_XVMAP_3OP(xvmulwev_q_du_d, la_vmulwev_q_du_d)
GEN_XVMAP_3OP(xvmulwod_d_w, la_vmulwod_d_w)
GEN_XVMAP_3OP(xvmulwod_d_wu, la_vmulwod_d_wu)
GEN_XVMAP_3OP(xvmulwod_d_wu_w, la_vmulwod_d_wu_w)
GEN_XVMAP_3OP(xvmulwod_h_b, la_vmulwod_h_b)
GEN_XVMAP_3OP(xvmulwod_h_bu_b, la_vmulwod_h_bu_b)
GEN_XVMAP_3OP(xvmulwod_q_d, la_vmulwod_q_d)
GEN_XVMAP_3OP(xvmulwod_q_du, la_vmulwod_q_du)
GEN_XVMAP_3OP(xvmulwod_q_du_d, la_vmulwod_q_du_d)
GEN_XVMAP_3OP(xvmul_b, la_vmul_b)
GEN_XVMAP_3OP(xvmul_d, la_vmul_d)
GEN_XVMAP_3OP(xvorn_v, la_vorn_v)
GEN_XVMAP_3OP(xvpackev_d, la_vpackev_d)
GEN_XVMAP_3OP(xvpackev_w, la_vpackev_w)
GEN_XVMAP_3OP(xvpackod_d, la_vpackod_d)
GEN_XVMAP_3OP(xvpackod_w, la_vpackod_w)
GEN_XVMAP_3OP(xvpickev_d, la_vpickev_d)
GEN_XVMAP_3OP(xvpickev_w, la_vpickev_w)
GEN_XVMAP_3OP(xvpickod_d, la_vpickod_d)
GEN_XVMAP_3OP(xvpickod_w, la_vpickod_w)
GEN_XVMAP_3OP(xvrotr_b, la_vrotr_b)
GEN_XVMAP_3OP(xvrotr_d, la_vrotr_d)
GEN_XVMAP_3OP(xvrotr_h, la_vrotr_h)
GEN_XVMAP_3OP(xvrotr_w, la_vrotr_w)
GEN_XVMAP_3OP(xvsadd_b, la_vsadd_b)
GEN_XVMAP_3OP(xvsadd_d, la_vsadd_d)
GEN_XVMAP_3OP(xvsadd_du, la_vsadd_du)
GEN_XVMAP_3OP(xvsadd_h, la_vsadd_h)
GEN_XVMAP_3OP(xvsadd_hu, la_vsadd_hu)
GEN_XVMAP_3OP(xvsadd_w, la_vsadd_w)
GEN_XVMAP_3OP(xvsadd_wu, la_vsadd_wu)
GEN_XVMAP_3OP(xvseq_b, la_vseq_b)
GEN_XVMAP_3OP(xvseq_d, la_vseq_d)
GEN_XVMAP_3OP(xvseq_h, la_vseq_h)
GEN_XVMAP_3OP(xvseq_w, la_vseq_w)
GEN_XVMAP_3OP(xvshuf_d, la_vshuf_d)
GEN_XVMAP_3OP(xvshuf_w, la_vshuf_w)
GEN_XVMAP_3OP(xvsigncov_b, la_vsigncov_b)
GEN_XVMAP_3OP(xvsigncov_d, la_vsigncov_d)
GEN_XVMAP_3OP(xvsigncov_h, la_vsigncov_h)
GEN_XVMAP_3OP(xvsigncov_w, la_vsigncov_w)
GEN_XVMAP_3OP(xvsle_b, la_vsle_b)
GEN_XVMAP_3OP(xvsle_bu, la_vsle_bu)
GEN_XVMAP_3OP(xvsle_d, la_vsle_d)
GEN_XVMAP_3OP(xvsle_du, la_vsle_du)
GEN_XVMAP_3OP(xvsle_h, la_vsle_h)
GEN_XVMAP_3OP(xvsle_hu, la_vsle_hu)
GEN_XVMAP_3OP(xvsle_w, la_vsle_w)
GEN_XVMAP_3OP(xvsle_wu, la_vsle_wu)
GEN_XVMAP_3OP(xvsll_d, la_vsll_d)
GEN_XVMAP_3OP(xvsll_h, la_vsll_h)
GEN_XVMAP_3OP(xvsll_w, la_vsll_w)
GEN_XVMAP_3OP(xvslt_b, la_vslt_b)
GEN_XVMAP_3OP(xvslt_bu, la_vslt_bu)
GEN_XVMAP_3OP(xvslt_d, la_vslt_d)
GEN_XVMAP_3OP(xvslt_du, la_vslt_du)
GEN_XVMAP_3OP(xvslt_h, la_vslt_h)
GEN_XVMAP_3OP(xvslt_hu, la_vslt_hu)
GEN_XVMAP_3OP(xvslt_wu, la_vslt_wu)
GEN_XVMAP_3OP(xvsran_b_h, la_vsran_b_h)
GEN_XVMAP_3OP(xvsran_h_w, la_vsran_h_w)
GEN_XVMAP_3OP(xvsran_w_d, la_vsran_w_d)
GEN_XVMAP_3OP(xvsrarn_b_h, la_vsrarn_b_h)
GEN_XVMAP_3OP(xvsrarn_h_w, la_vsrarn_h_w)
GEN_XVMAP_3OP(xvsrarn_w_d, la_vsrarn_w_d)
GEN_XVMAP_3OP(xvsrar_b, la_vsrar_b)
GEN_XVMAP_3OP(xvsrar_d, la_vsrar_d)
GEN_XVMAP_3OP(xvsrar_h, la_vsrar_h)
GEN_XVMAP_3OP(xvsrar_w, la_vsrar_w)
GEN_XVMAP_3OP(xvsra_b, la_vsra_b)
GEN_XVMAP_3OP(xvsra_d, la_vsra_d)
GEN_XVMAP_3OP(xvsra_h, la_vsra_h)
GEN_XVMAP_3OP(xvsra_w, la_vsra_w)
GEN_XVMAP_3OP(xvsrln_b_h, la_vsrln_b_h)
GEN_XVMAP_3OP(xvsrln_h_w, la_vsrln_h_w)
GEN_XVMAP_3OP(xvsrln_w_d, la_vsrln_w_d)
GEN_XVMAP_3OP(xvsrlrn_b_h, la_vsrlrn_b_h)
GEN_XVMAP_3OP(xvsrlrn_h_w, la_vsrlrn_h_w)
GEN_XVMAP_3OP(xvsrlrn_w_d, la_vsrlrn_w_d)
GEN_XVMAP_3OP(xvsrlr_b, la_vsrlr_b)
GEN_XVMAP_3OP(xvsrlr_d, la_vsrlr_d)
GEN_XVMAP_3OP(xvsrlr_h, la_vsrlr_h)
GEN_XVMAP_3OP(xvsrlr_w, la_vsrlr_w)
GEN_XVMAP_3OP(xvsrl_d, la_vsrl_d)
GEN_XVMAP_3OP(xvsrl_h, la_vsrl_h)
GEN_XVMAP_3OP(xvssran_bu_h, la_vssran_bu_h)
GEN_XVMAP_3OP(xvssran_b_h, la_vssran_b_h)
GEN_XVMAP_3OP(xvssran_hu_w, la_vssran_hu_w)
GEN_XVMAP_3OP(xvssran_h_w, la_vssran_h_w)
GEN_XVMAP_3OP(xvssran_wu_d, la_vssran_wu_d)
GEN_XVMAP_3OP(xvssran_w_d, la_vssran_w_d)
GEN_XVMAP_3OP(xvssrarn_bu_h, la_vssrarn_bu_h)
GEN_XVMAP_3OP(xvssrarn_b_h, la_vssrarn_b_h)
GEN_XVMAP_3OP(xvssrarn_hu_w, la_vssrarn_hu_w)
GEN_XVMAP_3OP(xvssrarn_h_w, la_vssrarn_h_w)
GEN_XVMAP_3OP(xvssrarn_wu_d, la_vssrarn_wu_d)
GEN_XVMAP_3OP(xvssrarn_w_d, la_vssrarn_w_d)
GEN_XVMAP_3OP(xvssrln_bu_h, la_vssrln_bu_h)
GEN_XVMAP_3OP(xvssrln_b_h, la_vssrln_b_h)
GEN_XVMAP_3OP(xvssrln_hu_w, la_vssrln_hu_w)
GEN_XVMAP_3OP(xvssrln_h_w, la_vssrln_h_w)
GEN_XVMAP_3OP(xvssrln_wu_d, la_vssrln_wu_d)
GEN_XVMAP_3OP(xvssrln_w_d, la_vssrln_w_d)
GEN_XVMAP_3OP(xvssrlrn_bu_h, la_vssrlrn_bu_h)
GEN_XVMAP_3OP(xvssrlrn_b_h, la_vssrlrn_b_h)
GEN_XVMAP_3OP(xvssrlrn_hu_w, la_vssrlrn_hu_w)
GEN_XVMAP_3OP(xvssrlrn_h_w, la_vssrlrn_h_w)
GEN_XVMAP_3OP(xvssrlrn_wu_d, la_vssrlrn_wu_d)
GEN_XVMAP_3OP(xvssrlrn_w_d, la_vssrlrn_w_d)
GEN_XVMAP_3OP(xvssub_b, la_vssub_b)
GEN_XVMAP_3OP(xvssub_d, la_vssub_d)
GEN_XVMAP_3OP(xvssub_du, la_vssub_du)
GEN_XVMAP_3OP(xvssub_h, la_vssub_h)
GEN_XVMAP_3OP(xvssub_hu, la_vssub_hu)
GEN_XVMAP_3OP(xvssub_w, la_vssub_w)
GEN_XVMAP_3OP(xvssub_wu, la_vssub_wu)
GEN_XVMAP_3OP(xvsubwev_d_w, la_vsubwev_d_w)
GEN_XVMAP_3OP(xvsubwev_d_wu, la_vsubwev_d_wu)
GEN_XVMAP_3OP(xvsubwev_h_b, la_vsubwev_h_b)
GEN_XVMAP_3OP(xvsubwev_h_bu, la_vsubwev_h_bu)
GEN_XVMAP_3OP(xvsubwev_q_d, la_vsubwev_q_d)
GEN_XVMAP_3OP(xvsubwev_q_du, la_vsubwev_q_du)
GEN_XVMAP_3OP(xvsubwev_w_h, la_vsubwev_w_h)
GEN_XVMAP_3OP(xvsubwev_w_hu, la_vsubwev_w_hu)
GEN_XVMAP_3OP(xvsubwod_d_w, la_vsubwod_d_w)
GEN_XVMAP_3OP(xvsubwod_d_wu, la_vsubwod_d_wu)
GEN_XVMAP_3OP(xvsubwod_h_b, la_vsubwod_h_b)
GEN_XVMAP_3OP(xvsubwod_h_bu, la_vsubwod_h_bu)
GEN_XVMAP_3OP(xvsubwod_q_d, la_vsubwod_q_d)
GEN_XVMAP_3OP(xvsubwod_q_du, la_vsubwod_q_du)
GEN_XVMAP_3OP(xvsubwod_w_h, la_vsubwod_w_h)
GEN_XVMAP_3OP(xvsubwod_w_hu, la_vsubwod_w_hu)
GEN_XVMAP_3OP(xvsub_d, la_vsub_d)
GEN_XVMAP_3OP(xvsub_q, la_vsub_q)
GEN_XVMAP_2OP(xvfcvth_d_s, la_vfcvth_d_s)
GEN_XVMAP_2OP(xvfcvtl_d_s, la_vfcvtl_d_s)
GEN_XVMAP_2OP(xvfrintrz_s, la_vfrintrz_s)

GEN_XVMAP_2OP(xvneg_b, la_vneg_b)
GEN_XVMAP_2OP(xvftintrne_w_s, la_vftintrne_w_s)
GEN_XVMAP_2OP(xvpcnt_w, la_vpcnt_w)
GEN_XVMAP_2OP(xvfrecip_s, la_vfrecip_s)
/* === Auto: missing 2OP === */
GEN_XVMAP_2OP(xvclo_b, la_vclo_b)
GEN_XVMAP_2OP(xvclo_d, la_vclo_d)
GEN_XVMAP_2OP(xvclo_h, la_vclo_h)
GEN_XVMAP_2OP(xvclo_w, la_vclo_w)
GEN_XVMAP_2OP(xvclz_b, la_vclz_b)
GEN_XVMAP_2OP(xvclz_d, la_vclz_d)
GEN_XVMAP_2OP(xvclz_h, la_vclz_h)
GEN_XVMAP_2OP(xvclz_w, la_vclz_w)
GEN_XVMAP_2OP(xvexth_du_wu, la_vexth_du_wu)
GEN_XVMAP_2OP(xvexth_d_w, la_vexth_d_w)
GEN_XVMAP_2OP(xvexth_hu_bu, la_vexth_hu_bu)
GEN_XVMAP_2OP(xvexth_h_b, la_vexth_h_b)
GEN_XVMAP_2OP(xvexth_qu_du, la_vexth_qu_du)
GEN_XVMAP_2OP(xvexth_q_d, la_vexth_q_d)
GEN_XVMAP_2OP(xvexth_wu_hu, la_vexth_wu_hu)
GEN_XVMAP_2OP(xvexth_w_h, la_vexth_w_h)
GEN_XVMAP_2OP(xvextl_qu_du, la_vextl_qu_du)
GEN_XVMAP_2OP(xvextl_q_d, la_vextl_q_d)
GEN_XVMAP_2OP(xvfclass_d, la_vfclass_d)
GEN_XVMAP_2OP(xvfclass_s, la_vfclass_s)
GEN_XVMAP_2OP(xvfcvth_s_h, la_vfcvth_s_h)
GEN_XVMAP_2OP(xvfcvtl_s_h, la_vfcvtl_s_h)
GEN_XVMAP_2OP(xvffinth_d_w, la_vffinth_d_w)
GEN_XVMAP_2OP(xvffintl_d_w, la_vffintl_d_w)
GEN_XVMAP_2OP(xvffint_d_l, la_vffint_d_l)
GEN_XVMAP_2OP(xvffint_d_lu, la_vffint_d_lu)
GEN_XVMAP_2OP(xvffint_s_w, la_vffint_s_w)
GEN_XVMAP_2OP(xvffint_s_wu, la_vffint_s_wu)
GEN_XVMAP_2OP(xvflogb_d, la_vflogb_d)
GEN_XVMAP_2OP(xvflogb_s, la_vflogb_s)
GEN_XVMAP_2OP(xvfrecipe_d, la_vfrecipe_d)
GEN_XVMAP_2OP(xvfrecipe_s, la_vfrecipe_s)
GEN_XVMAP_2OP(xvfrecip_d, la_vfrecip_d)
GEN_XVMAP_2OP(xvfrintrm_d, la_vfrintrm_d)
GEN_XVMAP_2OP(xvfrintrm_s, la_vfrintrm_s)
GEN_XVMAP_2OP(xvfrintrne_d, la_vfrintrne_d)
GEN_XVMAP_2OP(xvfrintrne_s, la_vfrintrne_s)
GEN_XVMAP_2OP(xvfrintrp_d, la_vfrintrp_d)
GEN_XVMAP_2OP(xvfrintrp_s, la_vfrintrp_s)
GEN_XVMAP_2OP(xvfrintrz_d, la_vfrintrz_d)
GEN_XVMAP_2OP(xvfrint_d, la_vfrint_d)
GEN_XVMAP_2OP(xvfrint_s, la_vfrint_s)
GEN_XVMAP_2OP(xvfrsqrte_d, la_vfrsqrte_d)
GEN_XVMAP_2OP(xvfrsqrte_s, la_vfrsqrte_s)
GEN_XVMAP_2OP(xvfrsqrt_d, la_vfrsqrt_d)
GEN_XVMAP_2OP(xvfrsqrt_s, la_vfrsqrt_s)
GEN_XVMAP_2OP(xvfsqrt_d, la_vfsqrt_d)
GEN_XVMAP_2OP(xvfsqrt_s, la_vfsqrt_s)
GEN_XVMAP_2OP(xvftinth_l_s, la_vftinth_l_s)
GEN_XVMAP_2OP(xvftintl_l_s, la_vftintl_l_s)
GEN_XVMAP_2OP(xvftintrmh_l_s, la_vftintrmh_l_s)
GEN_XVMAP_2OP(xvftintrml_l_s, la_vftintrml_l_s)
GEN_XVMAP_2OP(xvftintrm_l_d, la_vftintrm_l_d)
GEN_XVMAP_2OP(xvftintrm_w_s, la_vftintrm_w_s)
GEN_XVMAP_2OP(xvftintrneh_l_s, la_vftintrneh_l_s)
GEN_XVMAP_2OP(xvftintrnel_l_s, la_vftintrnel_l_s)
GEN_XVMAP_2OP(xvftintrne_l_d, la_vftintrne_l_d)
GEN_XVMAP_2OP(xvftintrph_l_s, la_vftintrph_l_s)
GEN_XVMAP_2OP(xvftintrpl_l_s, la_vftintrpl_l_s)
GEN_XVMAP_2OP(xvftintrp_l_d, la_vftintrp_l_d)
GEN_XVMAP_2OP(xvftintrp_w_s, la_vftintrp_w_s)
GEN_XVMAP_2OP(xvftintrzh_l_s, la_vftintrzh_l_s)
GEN_XVMAP_2OP(xvftintrzl_l_s, la_vftintrzl_l_s)
GEN_XVMAP_2OP(xvftintrz_lu_d, la_vftintrz_lu_d)
GEN_XVMAP_2OP(xvftintrz_l_d, la_vftintrz_l_d)
GEN_XVMAP_2OP(xvftintrz_wu_s, la_vftintrz_wu_s)
GEN_XVMAP_2OP(xvftintrz_w_s, la_vftintrz_w_s)
GEN_XVMAP_2OP(xvftint_lu_d, la_vftint_lu_d)
GEN_XVMAP_2OP(xvftint_l_d, la_vftint_l_d)
GEN_XVMAP_2OP(xvftint_wu_s, la_vftint_wu_s)
GEN_XVMAP_2OP(xvftint_w_s, la_vftint_w_s)
GEN_XVMAP_2OP(xvmskgez_b, la_vmskgez_b)
GEN_XVMAP_2OP(xvmskltz_b, la_vmskltz_b)
GEN_XVMAP_2OP(xvmskltz_d, la_vmskltz_d)
GEN_XVMAP_2OP(xvmskltz_h, la_vmskltz_h)
GEN_XVMAP_2OP(xvmskltz_w, la_vmskltz_w)
GEN_XVMAP_2OP(xvmsknz_b, la_vmsknz_b)
GEN_XVMAP_2OP(xvneg_d, la_vneg_d)
GEN_XVMAP_2OP(xvneg_h, la_vneg_h)
GEN_XVMAP_2OP(xvneg_w, la_vneg_w)
GEN_XVMAP_2OP(xvpcnt_b, la_vpcnt_b)
GEN_XVMAP_2OP(xvpcnt_d, la_vpcnt_d)
GEN_XVMAP_2OP(xvpcnt_h, la_vpcnt_h)
/* === Auto: missing 2OP_IMM === */
GEN_XVMAP_2OP_IMM(xvbitseli_b, la_vbitseli_b, 8, 10)
GEN_XVMAP_2OP_IMM(xvextrins_b, la_vextrins_b, 8, 10)
GEN_XVMAP_2OP_IMM(xvextrins_d, la_vextrins_d, 8, 10)
GEN_XVMAP_2OP_IMM(xvextrins_h, la_vextrins_h, 8, 10)
GEN_XVMAP_2OP_IMM(xvextrins_w, la_vextrins_w, 8, 10)
GEN_XVMAP_2OP_IMM(xvnori_b, la_vnori_b, 8, 10)
GEN_XVMAP_2OP_IMM(xvpermi_w, la_vpermi_w, 8, 10)
GEN_XVMAP_2OP_IMM(xvshuf4i_b, la_vshuf4i_b, 8, 10)
GEN_XVMAP_2OP_IMM(xvshuf4i_d, la_vshuf4i_d, 5, 10)
GEN_XVMAP_2OP_IMM(xvshuf4i_h, la_vshuf4i_h, 8, 10)
GEN_XVMAP_2OP_IMM(xvshuf4i_w, la_vshuf4i_w, 8, 10)
GEN_XVMAP_2OP_IMM(xvxori_b, la_vxori_b, 8, 10)
GEN_XVMAP_2OP_IMM(xvsrani_d_q, la_vsrani_d_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvsrarni_d_q, la_vsrarni_d_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvsrlni_d_q, la_vsrlni_d_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvsrlrni_d_q, la_vsrlrni_d_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvssrani_du_q, la_vssrani_du_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvssrani_d_q, la_vssrani_d_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvssrarni_du_q, la_vssrarni_du_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvssrarni_d_q, la_vssrarni_d_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvssrlni_du_q, la_vssrlni_du_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvssrlni_d_q, la_vssrlni_d_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvssrlrni_du_q, la_vssrlrni_du_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvssrlrni_d_q, la_vssrlrni_d_q, 7, 10)
GEN_XVMAP_2OP_IMM(xvbitclri_d, la_vbitclri_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvbitrevi_d, la_vbitrevi_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvbitseti_d, la_vbitseti_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvrotri_d, la_vrotri_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsat_d, la_vsat_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsat_du, la_vsat_du, 6, 10)
GEN_XVMAP_2OP_IMM(xvslli_d, la_vslli_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsrai_d, la_vsrai_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsrani_w_d, la_vsrani_w_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsrari_d, la_vsrari_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsrarni_w_d, la_vsrarni_w_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsrli_d, la_vsrli_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsrlni_w_d, la_vsrlni_w_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsrlri_d, la_vsrlri_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvsrlrni_w_d, la_vsrlrni_w_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvssrani_wu_d, la_vssrani_wu_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvssrani_w_d, la_vssrani_w_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvssrarni_wu_d, la_vssrarni_wu_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvssrarni_w_d, la_vssrarni_w_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvssrlni_wu_d, la_vssrlni_wu_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvssrlni_w_d, la_vssrlni_w_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvssrlrni_wu_d, la_vssrlrni_wu_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvssrlrni_w_d, la_vssrlrni_w_d, 6, 10)
GEN_XVMAP_2OP_IMM(xvaddi_bu, la_vaddi_bu, 5, 10)
GEN_XVMAP_2OP_IMM(xvaddi_du, la_vaddi_du, 5, 10)
GEN_XVMAP_2OP_IMM(xvbitclri_w, la_vbitclri_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvbitrevi_w, la_vbitrevi_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvbitseti_w, la_vbitseti_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvbsll_v, la_vbsll_v, 5, 10)
GEN_XVMAP_2OP_IMM(xvfrstpi_b, la_vfrstpi_b, 5, 10)
GEN_XVMAP_2OP_IMM(xvfrstpi_h, la_vfrstpi_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvmaxi_b, la_vmaxi_b, 5, 10)
GEN_XVMAP_2OP_IMM(xvmaxi_bu, la_vmaxi_bu, 5, 10)
GEN_XVMAP_2OP_IMM(xvmaxi_d, la_vmaxi_d, 5, 10)
GEN_XVMAP_2OP_IMM(xvmaxi_du, la_vmaxi_du, 5, 10)
GEN_XVMAP_2OP_IMM(xvmaxi_hu, la_vmaxi_hu, 5, 10)
GEN_XVMAP_2OP_IMM(xvmaxi_wu, la_vmaxi_wu, 5, 10)
GEN_XVMAP_2OP_IMM(xvmini_b, la_vmini_b, 5, 10)
GEN_XVMAP_2OP_IMM(xvmini_bu, la_vmini_bu, 5, 10)
GEN_XVMAP_2OP_IMM(xvmini_d, la_vmini_d, 5, 10)
GEN_XVMAP_2OP_IMM(xvmini_du, la_vmini_du, 5, 10)
GEN_XVMAP_2OP_IMM(xvmini_h, la_vmini_h, 5, 10)
GEN_XVMAP_2OP_IMM(xvmini_hu, la_vmini_hu, 5, 10)
GEN_XVMAP_2OP_IMM(xvmini_w, la_vmini_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvmini_wu, la_vmini_wu, 5, 10)
GEN_XVMAP_2OP_IMM(xvrotri_w, la_vrotri_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvsat_w, la_vsat_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvseqi_b, la_vseqi_b, 5, 10)
GEN_XVMAP_2OP_IMM(xvseqi_d, la_vseqi_d, 5, 10)
GEN_XVMAP_2OP_IMM(xvseqi_h, la_vseqi_h, 5, 10)
GEN_XVMAP_2OP_IMM(xvseqi_w, la_vseqi_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvslei_b, la_vslei_b, 5, 10)
GEN_XVMAP_2OP_IMM(xvslei_bu, la_vslei_bu, 5, 10)
GEN_XVMAP_2OP_IMM(xvslei_d, la_vslei_d, 5, 10)
GEN_XVMAP_2OP_IMM(xvslei_du, la_vslei_du, 5, 10)
GEN_XVMAP_2OP_IMM(xvslei_h, la_vslei_h, 5, 10)
GEN_XVMAP_2OP_IMM(xvslei_hu, la_vslei_hu, 5, 10)
GEN_XVMAP_2OP_IMM(xvslei_w, la_vslei_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvslei_wu, la_vslei_wu, 5, 10)
GEN_XVMAP_2OP_IMM(xvsllwil_du_wu, la_vsllwil_du_wu, 5, 10)
GEN_XVMAP_2OP_IMM(xvsllwil_d_w, la_vsllwil_d_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvslti_b, la_vslti_b, 5, 10)
GEN_XVMAP_2OP_IMM(xvslti_bu, la_vslti_bu, 5, 10)
GEN_XVMAP_2OP_IMM(xvslti_d, la_vslti_d, 5, 10)
GEN_XVMAP_2OP_IMM(xvslti_du, la_vslti_du, 5, 10)
GEN_XVMAP_2OP_IMM(xvslti_h, la_vslti_h, 5, 10)
GEN_XVMAP_2OP_IMM(xvslti_hu, la_vslti_hu, 5, 10)
GEN_XVMAP_2OP_IMM(xvslti_w, la_vslti_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvslti_wu, la_vslti_wu, 5, 10)
GEN_XVMAP_2OP_IMM(xvsrani_h_w, la_vsrani_h_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvsrarni_h_w, la_vsrarni_h_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvsrlni_h_w, la_vsrlni_h_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvsrlri_w, la_vsrlri_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvsrlrni_h_w, la_vsrlrni_h_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvssrani_hu_w, la_vssrani_hu_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvssrarni_hu_w, la_vssrarni_hu_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvssrarni_h_w, la_vssrarni_h_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvssrlni_hu_w, la_vssrlni_hu_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvssrlni_h_w, la_vssrlni_h_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvssrlrni_hu_w, la_vssrlrni_hu_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvssrlrni_h_w, la_vssrlrni_h_w, 5, 10)
GEN_XVMAP_2OP_IMM(xvsubi_bu, la_vsubi_bu, 5, 10)
GEN_XVMAP_2OP_IMM(xvsubi_du, la_vsubi_du, 5, 10)
GEN_XVMAP_2OP_IMM(xvsubi_hu, la_vsubi_hu, 5, 10)
GEN_XVMAP_2OP_IMM(xvsubi_wu, la_vsubi_wu, 5, 10)
GEN_XVMAP_2OP_IMM(xvbitclri_h, la_vbitclri_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvbitrevi_h, la_vbitrevi_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvbitseti_h, la_vbitseti_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvrotri_h, la_vrotri_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvsat_h, la_vsat_h, 5, 10)
GEN_XVMAP_2OP_IMM(xvsllwil_wu_hu, la_vsllwil_wu_hu, 4, 10)
GEN_XVMAP_2OP_IMM(xvsllwil_w_h, la_vsllwil_w_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvsrani_b_h, la_vsrani_b_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvsrarni_b_h, la_vsrarni_b_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvsrlni_b_h, la_vsrlni_b_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvsrlri_h, la_vsrlri_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvsrlrni_b_h, la_vsrlrni_b_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvssrani_bu_h, la_vssrani_bu_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvssrarni_b_h, la_vssrarni_b_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvssrlni_bu_h, la_vssrlni_bu_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvssrlni_b_h, la_vssrlni_b_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvssrlrni_bu_h, la_vssrlrni_bu_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvssrlrni_b_h, la_vssrlrni_b_h, 4, 10)
GEN_XVMAP_2OP_IMM(xvbitclri_b, la_vbitclri_b, 3, 10)
GEN_XVMAP_2OP_IMM(xvbitrevi_b, la_vbitrevi_b, 3, 10)
GEN_XVMAP_2OP_IMM(xvbitseti_b, la_vbitseti_b, 3, 10)
GEN_XVMAP_2OP_IMM(xvrotri_b, la_vrotri_b, 3, 10)
GEN_XVMAP_2OP_IMM(xvsat_b, la_vsat_b, 5, 10)
GEN_XVMAP_2OP_IMM(xvsat_bu, la_vsat_bu, 5, 10)
GEN_XVMAP_2OP_IMM(xvsllwil_hu_bu, la_vsllwil_hu_bu, 3, 10)
GEN_XVMAP_2OP_IMM(xvsllwil_h_b, la_vsllwil_h_b, 3, 10)
GEN_XVMAP_2OP_IMM(xvsrai_b, la_vsrai_b, 3, 10)
GEN_XVMAP_2OP_IMM(xvsrari_b, la_vsrari_b, 3, 10)
GEN_XVMAP_2OP_IMM(xvsrlri_b, la_vsrlri_b, 3, 10)

int lasx_interpret_inst_gen_xvmap(void *as, unsigned int instr)
{
    int ok = 0;
    switch (instr >> 8) {
        case OP24_XVSETALLNEZ_B: ok = 1; GEN_XVSETALLNEZ_B_XVMAP(as, instr); break;
        case OP24_XVSETALLNEZ_D: ok = 1; GEN_XVSETALLNEZ_D_XVMAP(as, instr); break;
        case OP24_XVSETALLNEZ_H: ok = 1; GEN_XVSETALLNEZ_H_XVMAP(as, instr); break;
        case OP24_XVSETALLNEZ_W: ok = 1; GEN_XVSETALLNEZ_W_XVMAP(as, instr); break;
        case OP24_XVSETANYEQZ_B: ok = 1; GEN_XVSETANYEQZ_B_XVMAP(as, instr); break;
        case OP24_XVSETANYEQZ_D: ok = 1; GEN_XVSETANYEQZ_D_XVMAP(as, instr); break;
        case OP24_XVSETANYEQZ_H: ok = 1; GEN_XVSETANYEQZ_H_XVMAP(as, instr); break;
        case OP24_XVSETANYEQZ_W: ok = 1; GEN_XVSETANYEQZ_W_XVMAP(as, instr); break;
        case OP24_XVSETEQZ_V: ok = 1; GEN_XVSETEQZ_V_XVMAP(as, instr); break;
        case OP24_XVSETNEZ_V: ok = 1; GEN_XVSETNEZ_V_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 10) {
        case OP22_VEXT2XV_DU_BU: ok = 1; GEN_VEXT2XV_DU_BU_XVMAP(as, instr); break;
        case OP22_VEXT2XV_DU_HU: ok = 1; GEN_VEXT2XV_DU_HU_XVMAP(as, instr); break;
        case OP22_VEXT2XV_DU_WU: ok = 1; GEN_VEXT2XV_DU_WU_XVMAP(as, instr); break;
        case OP22_VEXT2XV_D_B: ok = 1; GEN_VEXT2XV_D_B_XVMAP(as, instr); break;
        case OP22_VEXT2XV_D_H: ok = 1; GEN_VEXT2XV_D_H_XVMAP(as, instr); break;
        case OP22_VEXT2XV_D_W: ok = 1; GEN_VEXT2XV_D_W_XVMAP(as, instr); break;
        case OP22_VEXT2XV_HU_BU: ok = 1; GEN_VEXT2XV_HU_BU_XVMAP(as, instr); break;
        case OP22_VEXT2XV_H_B: ok = 1; GEN_VEXT2XV_H_B_XVMAP(as, instr); break;
        case OP22_VEXT2XV_WU_BU: ok = 1; GEN_VEXT2XV_WU_BU_XVMAP(as, instr); break;
        case OP22_VEXT2XV_WU_HU: ok = 1; GEN_VEXT2XV_WU_HU_XVMAP(as, instr); break;
        case OP22_VEXT2XV_W_B: ok = 1; GEN_VEXT2XV_W_B_XVMAP(as, instr); break;
        case OP22_VEXT2XV_W_H: ok = 1; GEN_VEXT2XV_W_H_XVMAP(as, instr); break;
        case OP22_XVCLO_B: ok = 1; GEN_XVCLO_B_XVMAP(as, instr); break;
        case OP22_XVCLO_D: ok = 1; GEN_XVCLO_D_XVMAP(as, instr); break;
        case OP22_XVCLO_H: ok = 1; GEN_XVCLO_H_XVMAP(as, instr); break;
        case OP22_XVCLO_W: ok = 1; GEN_XVCLO_W_XVMAP(as, instr); break;
        case OP22_XVCLZ_B: ok = 1; GEN_XVCLZ_B_XVMAP(as, instr); break;
        case OP22_XVCLZ_D: ok = 1; GEN_XVCLZ_D_XVMAP(as, instr); break;
        case OP22_XVCLZ_H: ok = 1; GEN_XVCLZ_H_XVMAP(as, instr); break;
        case OP22_XVCLZ_W: ok = 1; GEN_XVCLZ_W_XVMAP(as, instr); break;
        case OP22_XVEXTH_DU_WU: ok = 1; GEN_XVEXTH_DU_WU_XVMAP(as, instr); break;
        case OP22_XVEXTH_D_W: ok = 1; GEN_XVEXTH_D_W_XVMAP(as, instr); break;
        case OP22_XVEXTH_HU_BU: ok = 1; GEN_XVEXTH_HU_BU_XVMAP(as, instr); break;
        case OP22_XVEXTH_H_B: ok = 1; GEN_XVEXTH_H_B_XVMAP(as, instr); break;
        case OP22_XVEXTH_QU_DU: ok = 1; GEN_XVEXTH_QU_DU_XVMAP(as, instr); break;
        case OP22_XVEXTH_Q_D: ok = 1; GEN_XVEXTH_Q_D_XVMAP(as, instr); break;
        case OP22_XVEXTH_WU_HU: ok = 1; GEN_XVEXTH_WU_HU_XVMAP(as, instr); break;
        case OP22_XVEXTH_W_H: ok = 1; GEN_XVEXTH_W_H_XVMAP(as, instr); break;
        case OP22_XVEXTL_QU_DU: ok = 1; GEN_XVEXTL_QU_DU_XVMAP(as, instr); break;
        case OP22_XVEXTL_Q_D: ok = 1; GEN_XVEXTL_Q_D_XVMAP(as, instr); break;
        case OP22_XVFCLASS_D: ok = 1; GEN_XVFCLASS_D_XVMAP(as, instr); break;
        case OP22_XVFCLASS_S: ok = 1; GEN_XVFCLASS_S_XVMAP(as, instr); break;
        case OP22_XVFCVTH_D_S: ok = 1; GEN_XVFCVTH_D_S_XVMAP(as, instr); break;
        case OP22_XVFCVTH_S_H: ok = 1; GEN_XVFCVTH_S_H_XVMAP(as, instr); break;
        case OP22_XVFCVTL_D_S: ok = 1; GEN_XVFCVTL_D_S_XVMAP(as, instr); break;
        case OP22_XVFCVTL_S_H: ok = 1; GEN_XVFCVTL_S_H_XVMAP(as, instr); break;
        case OP22_XVFFINTH_D_W: ok = 1; GEN_XVFFINTH_D_W_XVMAP(as, instr); break;
        case OP22_XVFFINTL_D_W: ok = 1; GEN_XVFFINTL_D_W_XVMAP(as, instr); break;
        case OP22_XVFFINT_D_L: ok = 1; GEN_XVFFINT_D_L_XVMAP(as, instr); break;
        case OP22_XVFFINT_D_LU: ok = 1; GEN_XVFFINT_D_LU_XVMAP(as, instr); break;
        case OP22_XVFFINT_S_W: ok = 1; GEN_XVFFINT_S_W_XVMAP(as, instr); break;
        case OP22_XVFFINT_S_WU: ok = 1; GEN_XVFFINT_S_WU_XVMAP(as, instr); break;
        case OP22_XVFLOGB_D: ok = 1; GEN_XVFLOGB_D_XVMAP(as, instr); break;
        case OP22_XVFLOGB_S: ok = 1; GEN_XVFLOGB_S_XVMAP(as, instr); break;
        case OP22_XVFRECIPE_D: ok = 1; GEN_XVFRECIPE_D_XVMAP(as, instr); break;
        case OP22_XVFRECIPE_S: ok = 1; GEN_XVFRECIPE_S_XVMAP(as, instr); break;
        case OP22_XVFRECIP_D: ok = 1; GEN_XVFRECIP_D_XVMAP(as, instr); break;
        case OP22_XVFRECIP_S: ok = 1; GEN_XVFRECIP_S_XVMAP(as, instr); break;
        case OP22_XVFRINTRM_D: ok = 1; GEN_XVFRINTRM_D_XVMAP(as, instr); break;
        case OP22_XVFRINTRM_S: ok = 1; GEN_XVFRINTRM_S_XVMAP(as, instr); break;
        case OP22_XVFRINTRNE_D: ok = 1; GEN_XVFRINTRNE_D_XVMAP(as, instr); break;
        case OP22_XVFRINTRNE_S: ok = 1; GEN_XVFRINTRNE_S_XVMAP(as, instr); break;
        case OP22_XVFRINTRP_D: ok = 1; GEN_XVFRINTRP_D_XVMAP(as, instr); break;
        case OP22_XVFRINTRP_S: ok = 1; GEN_XVFRINTRP_S_XVMAP(as, instr); break;
        case OP22_XVFRINTRZ_D: ok = 1; GEN_XVFRINTRZ_D_XVMAP(as, instr); break;
        case OP22_XVFRINTRZ_S: ok = 1; GEN_XVFRINTRZ_S_XVMAP(as, instr); break;
        case OP22_XVFRINT_D: ok = 1; GEN_XVFRINT_D_XVMAP(as, instr); break;
        case OP22_XVFRINT_S: ok = 1; GEN_XVFRINT_S_XVMAP(as, instr); break;
        case OP22_XVFRSQRTE_D: ok = 1; GEN_XVFRSQRTE_D_XVMAP(as, instr); break;
        case OP22_XVFRSQRTE_S: ok = 1; GEN_XVFRSQRTE_S_XVMAP(as, instr); break;
        case OP22_XVFRSQRT_D: ok = 1; GEN_XVFRSQRT_D_XVMAP(as, instr); break;
        case OP22_XVFRSQRT_S: ok = 1; GEN_XVFRSQRT_S_XVMAP(as, instr); break;
        case OP22_XVFSQRT_D: ok = 1; GEN_XVFSQRT_D_XVMAP(as, instr); break;
        case OP22_XVFSQRT_S: ok = 1; GEN_XVFSQRT_S_XVMAP(as, instr); break;
        case OP22_XVFTINTH_L_S: ok = 1; GEN_XVFTINTH_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTL_L_S: ok = 1; GEN_XVFTINTL_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRMH_L_S: ok = 1; GEN_XVFTINTRMH_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRML_L_S: ok = 1; GEN_XVFTINTRML_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRM_L_D: ok = 1; GEN_XVFTINTRM_L_D_XVMAP(as, instr); break;
        case OP22_XVFTINTRM_W_S: ok = 1; GEN_XVFTINTRM_W_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRNEH_L_S: ok = 1; GEN_XVFTINTRNEH_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRNEL_L_S: ok = 1; GEN_XVFTINTRNEL_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRNE_L_D: ok = 1; GEN_XVFTINTRNE_L_D_XVMAP(as, instr); break;
        case OP22_XVFTINTRNE_W_S: ok = 1; GEN_XVFTINTRNE_W_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRPH_L_S: ok = 1; GEN_XVFTINTRPH_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRPL_L_S: ok = 1; GEN_XVFTINTRPL_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRP_L_D: ok = 1; GEN_XVFTINTRP_L_D_XVMAP(as, instr); break;
        case OP22_XVFTINTRP_W_S: ok = 1; GEN_XVFTINTRP_W_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRZH_L_S: ok = 1; GEN_XVFTINTRZH_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRZL_L_S: ok = 1; GEN_XVFTINTRZL_L_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRZ_LU_D: ok = 1; GEN_XVFTINTRZ_LU_D_XVMAP(as, instr); break;
        case OP22_XVFTINTRZ_L_D: ok = 1; GEN_XVFTINTRZ_L_D_XVMAP(as, instr); break;
        case OP22_XVFTINTRZ_WU_S: ok = 1; GEN_XVFTINTRZ_WU_S_XVMAP(as, instr); break;
        case OP22_XVFTINTRZ_W_S: ok = 1; GEN_XVFTINTRZ_W_S_XVMAP(as, instr); break;
        case OP22_XVFTINT_LU_D: ok = 1; GEN_XVFTINT_LU_D_XVMAP(as, instr); break;
        case OP22_XVFTINT_L_D: ok = 1; GEN_XVFTINT_L_D_XVMAP(as, instr); break;
        case OP22_XVFTINT_WU_S: ok = 1; GEN_XVFTINT_WU_S_XVMAP(as, instr); break;
        case OP22_XVFTINT_W_S: ok = 1; GEN_XVFTINT_W_S_XVMAP(as, instr); break;
        case OP22_XVMSKGEZ_B: ok = 1; GEN_XVMSKGEZ_B_XVMAP(as, instr); break;
        case OP22_XVMSKLTZ_B: ok = 1; GEN_XVMSKLTZ_B_XVMAP(as, instr); break;
        case OP22_XVMSKLTZ_D: ok = 1; GEN_XVMSKLTZ_D_XVMAP(as, instr); break;
        case OP22_XVMSKLTZ_H: ok = 1; GEN_XVMSKLTZ_H_XVMAP(as, instr); break;
        case OP22_XVMSKLTZ_W: ok = 1; GEN_XVMSKLTZ_W_XVMAP(as, instr); break;
        case OP22_XVMSKNZ_B: ok = 1; GEN_XVMSKNZ_B_XVMAP(as, instr); break;
        case OP22_XVNEG_B: ok = 1; GEN_XVNEG_B_XVMAP(as, instr); break;
        case OP22_XVNEG_D: ok = 1; GEN_XVNEG_D_XVMAP(as, instr); break;
        case OP22_XVNEG_H: ok = 1; GEN_XVNEG_H_XVMAP(as, instr); break;
        case OP22_XVNEG_W: ok = 1; GEN_XVNEG_W_XVMAP(as, instr); break;
        case OP22_XVPCNT_B: ok = 1; GEN_XVPCNT_B_XVMAP(as, instr); break;
        case OP22_XVPCNT_D: ok = 1; GEN_XVPCNT_D_XVMAP(as, instr); break;
        case OP22_XVPCNT_H: ok = 1; GEN_XVPCNT_H_XVMAP(as, instr); break;
        case OP22_XVPCNT_W: ok = 1; GEN_XVPCNT_W_XVMAP(as, instr); break;
        case OP22_XVREPLGR2VR_B: ok = 1; GEN_XVREPLGR2VR_B_XVMAP(as, instr); break;
        case OP22_XVREPLGR2VR_D: ok = 1; GEN_XVREPLGR2VR_D_XVMAP(as, instr); break;
        case OP22_XVREPLGR2VR_H: ok = 1; GEN_XVREPLGR2VR_H_XVMAP(as, instr); break;
        case OP22_XVREPLGR2VR_W: ok = 1; GEN_XVREPLGR2VR_W_XVMAP(as, instr); break;
        case OP22_XVREPLVE0_B: ok = 1; GEN_XVREPLVE0_B_XVMAP(as, instr); break;
        case OP22_XVREPLVE0_D: ok = 1; GEN_XVREPLVE0_D_XVMAP(as, instr); break;
        case OP22_XVREPLVE0_H: ok = 1; GEN_XVREPLVE0_H_XVMAP(as, instr); break;
        case OP22_XVREPLVE0_Q: ok = 1; GEN_XVREPLVE0_Q_XVMAP(as, instr); break;
        case OP22_XVREPLVE0_W: ok = 1; GEN_XVREPLVE0_W_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 11) {
        case OP21_XVREPL128VEI_D: ok = 1; GEN_XVREPL128VEI_D_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 12) {
        case OP20_XVINSGR2VR_D: ok = 1; GEN_XVINSGR2VR_D_XVMAP(as, instr); break;
        case OP20_XVINSVE0_D: ok = 1; GEN_XVINSVE0_D_XVMAP(as, instr); break;
        case OP20_XVPICKVE2GR_D: ok = 1; GEN_XVPICKVE2GR_D_XVMAP(as, instr); break;
        case OP20_XVPICKVE2GR_DU: ok = 1; GEN_XVPICKVE2GR_DU_XVMAP(as, instr); break;
        case OP20_XVPICKVE_D: ok = 1; GEN_XVPICKVE_D_XVMAP(as, instr); break;
        case OP20_XVREPL128VEI_W: ok = 1; GEN_XVREPL128VEI_W_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 13) {
        case OP19_XVBITCLRI_B: ok = 1; GEN_XVBITCLRI_B_XVMAP(as, instr); break;
        case OP19_XVBITREVI_B: ok = 1; GEN_XVBITREVI_B_XVMAP(as, instr); break;
        case OP19_XVBITSETI_B: ok = 1; GEN_XVBITSETI_B_XVMAP(as, instr); break;
        case OP19_XVINSGR2VR_W: ok = 1; GEN_XVINSGR2VR_W_XVMAP(as, instr); break;
        case OP19_XVINSVE0_W: ok = 1; GEN_XVINSVE0_W_XVMAP(as, instr); break;
        case OP19_XVPICKVE2GR_W: ok = 1; GEN_XVPICKVE2GR_W_XVMAP(as, instr); break;
        case OP19_XVPICKVE2GR_WU: ok = 1; GEN_XVPICKVE2GR_WU_XVMAP(as, instr); break;
        case OP19_XVPICKVE_W: ok = 1; GEN_XVPICKVE_W_XVMAP(as, instr); break;
        case OP19_XVREPL128VEI_H: ok = 1; GEN_XVREPL128VEI_H_XVMAP(as, instr); break;
        case OP19_XVROTRI_B: ok = 1; GEN_XVROTRI_B_XVMAP(as, instr); break;
        case OP19_XVSAT_B: ok = 1; GEN_XVSAT_B_XVMAP(as, instr); break;
        case OP19_XVSAT_BU: ok = 1; GEN_XVSAT_BU_XVMAP(as, instr); break;
        case OP19_XVSLLI_B: ok = 1; GEN_XVSLLI_B_XVMAP(as, instr); break;
        case OP19_XVSLLWIL_HU_BU: ok = 1; GEN_XVSLLWIL_HU_BU_XVMAP(as, instr); break;
        case OP19_XVSLLWIL_H_B: ok = 1; GEN_XVSLLWIL_H_B_XVMAP(as, instr); break;
        case OP19_XVSRAI_B: ok = 1; GEN_XVSRAI_B_XVMAP(as, instr); break;
        case OP19_XVSRARI_B: ok = 1; GEN_XVSRARI_B_XVMAP(as, instr); break;
        case OP19_XVSRLI_B: ok = 1; GEN_XVSRLI_B_XVMAP(as, instr); break;
        case OP19_XVSRLRI_B: ok = 1; GEN_XVSRLRI_B_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 14) {
        case OP18_XVBITCLRI_H: ok = 1; GEN_XVBITCLRI_H_XVMAP(as, instr); break;
        case OP18_XVBITREVI_H: ok = 1; GEN_XVBITREVI_H_XVMAP(as, instr); break;
        case OP18_XVBITSETI_H: ok = 1; GEN_XVBITSETI_H_XVMAP(as, instr); break;
        case OP18_XVREPL128VEI_B: ok = 1; GEN_XVREPL128VEI_B_XVMAP(as, instr); break;
        case OP18_XVROTRI_H: ok = 1; GEN_XVROTRI_H_XVMAP(as, instr); break;
        case OP18_XVSAT_H: ok = 1; GEN_XVSAT_H_XVMAP(as, instr); break;
        case OP18_XVSAT_HU: ok = 1; GEN_XVSAT_HU_XVMAP(as, instr); break;
        case OP18_XVSLLI_H: ok = 1; GEN_XVSLLI_H_XVMAP(as, instr); break;
        case OP18_XVSLLWIL_WU_HU: ok = 1; GEN_XVSLLWIL_WU_HU_XVMAP(as, instr); break;
        case OP18_XVSLLWIL_W_H: ok = 1; GEN_XVSLLWIL_W_H_XVMAP(as, instr); break;
        case OP18_XVSRAI_H: ok = 1; GEN_XVSRAI_H_XVMAP(as, instr); break;
        case OP18_XVSRANI_B_H: ok = 1; GEN_XVSRANI_B_H_XVMAP(as, instr); break;
        case OP18_XVSRARI_H: ok = 1; GEN_XVSRARI_H_XVMAP(as, instr); break;
        case OP18_XVSRARNI_B_H: ok = 1; GEN_XVSRARNI_B_H_XVMAP(as, instr); break;
        case OP18_XVSRLI_H: ok = 1; GEN_XVSRLI_H_XVMAP(as, instr); break;
        case OP18_XVSRLNI_B_H: ok = 1; GEN_XVSRLNI_B_H_XVMAP(as, instr); break;
        case OP18_XVSRLRI_H: ok = 1; GEN_XVSRLRI_H_XVMAP(as, instr); break;
        case OP18_XVSRLRNI_B_H: ok = 1; GEN_XVSRLRNI_B_H_XVMAP(as, instr); break;
        case OP18_XVSSRANI_BU_H: ok = 1; GEN_XVSSRANI_BU_H_XVMAP(as, instr); break;
        case OP18_XVSSRANI_B_H: ok = 1; GEN_XVSSRANI_B_H_XVMAP(as, instr); break;
        case OP18_XVSSRARNI_BU_H: ok = 1; GEN_XVSSRARNI_BU_H_XVMAP(as, instr); break;
        case OP18_XVSSRARNI_B_H: ok = 1; GEN_XVSSRARNI_B_H_XVMAP(as, instr); break;
        case OP18_XVSSRLNI_BU_H: ok = 1; GEN_XVSSRLNI_BU_H_XVMAP(as, instr); break;
        case OP18_XVSSRLNI_B_H: ok = 1; GEN_XVSSRLNI_B_H_XVMAP(as, instr); break;
        case OP18_XVSSRLRNI_BU_H: ok = 1; GEN_XVSSRLRNI_BU_H_XVMAP(as, instr); break;
        case OP18_XVSSRLRNI_B_H: ok = 1; GEN_XVSSRLRNI_B_H_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 15) {
        case OP17_XVABSD_B: ok = 1; GEN_XVABSD_B_XVMAP(as, instr); break;
        case OP17_XVABSD_BU: ok = 1; GEN_XVABSD_BU_XVMAP(as, instr); break;
        case OP17_XVABSD_D: ok = 1; GEN_XVABSD_D_XVMAP(as, instr); break;
        case OP17_XVABSD_DU: ok = 1; GEN_XVABSD_DU_XVMAP(as, instr); break;
        case OP17_XVABSD_H: ok = 1; GEN_XVABSD_H_XVMAP(as, instr); break;
        case OP17_XVABSD_HU: ok = 1; GEN_XVABSD_HU_XVMAP(as, instr); break;
        case OP17_XVABSD_W: ok = 1; GEN_XVABSD_W_XVMAP(as, instr); break;
        case OP17_XVABSD_WU: ok = 1; GEN_XVABSD_WU_XVMAP(as, instr); break;
        case OP17_XVADDA_B: ok = 1; GEN_XVADDA_B_XVMAP(as, instr); break;
        case OP17_XVADDA_D: ok = 1; GEN_XVADDA_D_XVMAP(as, instr); break;
        case OP17_XVADDA_H: ok = 1; GEN_XVADDA_H_XVMAP(as, instr); break;
        case OP17_XVADDA_W: ok = 1; GEN_XVADDA_W_XVMAP(as, instr); break;
        case OP17_XVADDI_BU: ok = 1; GEN_XVADDI_BU_XVMAP(as, instr); break;
        case OP17_XVADDI_DU: ok = 1; GEN_XVADDI_DU_XVMAP(as, instr); break;
        case OP17_XVADDI_HU: ok = 1; GEN_XVADDI_HU_XVMAP(as, instr); break;
        case OP17_XVADDI_WU: ok = 1; GEN_XVADDI_WU_XVMAP(as, instr); break;
        case OP17_XVADDWEV_D_W: ok = 1; GEN_XVADDWEV_D_W_XVMAP(as, instr); break;
        case OP17_XVADDWEV_D_WU: ok = 1; GEN_XVADDWEV_D_WU_XVMAP(as, instr); break;
        case OP17_XVADDWEV_D_WU_W: ok = 1; GEN_XVADDWEV_D_WU_W_XVMAP(as, instr); break;
        case OP17_XVADDWEV_H_B: ok = 1; GEN_XVADDWEV_H_B_XVMAP(as, instr); break;
        case OP17_XVADDWEV_H_BU: ok = 1; GEN_XVADDWEV_H_BU_XVMAP(as, instr); break;
        case OP17_XVADDWEV_H_BU_B: ok = 1; GEN_XVADDWEV_H_BU_B_XVMAP(as, instr); break;
        case OP17_XVADDWEV_Q_D: ok = 1; GEN_XVADDWEV_Q_D_XVMAP(as, instr); break;
        case OP17_XVADDWEV_Q_DU: ok = 1; GEN_XVADDWEV_Q_DU_XVMAP(as, instr); break;
        case OP17_XVADDWEV_Q_DU_D: ok = 1; GEN_XVADDWEV_Q_DU_D_XVMAP(as, instr); break;
        case OP17_XVADDWEV_W_H: ok = 1; GEN_XVADDWEV_W_H_XVMAP(as, instr); break;
        case OP17_XVADDWEV_W_HU: ok = 1; GEN_XVADDWEV_W_HU_XVMAP(as, instr); break;
        case OP17_XVADDWEV_W_HU_H: ok = 1; GEN_XVADDWEV_W_HU_H_XVMAP(as, instr); break;
        case OP17_XVADDWOD_D_W: ok = 1; GEN_XVADDWOD_D_W_XVMAP(as, instr); break;
        case OP17_XVADDWOD_D_WU: ok = 1; GEN_XVADDWOD_D_WU_XVMAP(as, instr); break;
        case OP17_XVADDWOD_D_WU_W: ok = 1; GEN_XVADDWOD_D_WU_W_XVMAP(as, instr); break;
        case OP17_XVADDWOD_H_B: ok = 1; GEN_XVADDWOD_H_B_XVMAP(as, instr); break;
        case OP17_XVADDWOD_H_BU: ok = 1; GEN_XVADDWOD_H_BU_XVMAP(as, instr); break;
        case OP17_XVADDWOD_H_BU_B: ok = 1; GEN_XVADDWOD_H_BU_B_XVMAP(as, instr); break;
        case OP17_XVADDWOD_Q_D: ok = 1; GEN_XVADDWOD_Q_D_XVMAP(as, instr); break;
        case OP17_XVADDWOD_Q_DU: ok = 1; GEN_XVADDWOD_Q_DU_XVMAP(as, instr); break;
        case OP17_XVADDWOD_Q_DU_D: ok = 1; GEN_XVADDWOD_Q_DU_D_XVMAP(as, instr); break;
        case OP17_XVADDWOD_W_H: ok = 1; GEN_XVADDWOD_W_H_XVMAP(as, instr); break;
        case OP17_XVADDWOD_W_HU: ok = 1; GEN_XVADDWOD_W_HU_XVMAP(as, instr); break;
        case OP17_XVADDWOD_W_HU_H: ok = 1; GEN_XVADDWOD_W_HU_H_XVMAP(as, instr); break;
        case OP17_XVADD_B: ok = 1; GEN_XVADD_B_XVMAP(as, instr); break;
        case OP17_XVADD_D: ok = 1; GEN_XVADD_D_XVMAP(as, instr); break;
        case OP17_XVADD_H: ok = 1; GEN_XVADD_H_XVMAP(as, instr); break;
        case OP17_XVADD_Q: ok = 1; GEN_XVADD_Q_XVMAP(as, instr); break;
        case OP17_XVADD_W: ok = 1; GEN_XVADD_W_XVMAP(as, instr); break;
        case OP17_XVANDN_V: ok = 1; GEN_XVANDN_V_XVMAP(as, instr); break;
        case OP17_XVAND_V: ok = 1; GEN_XVAND_V_XVMAP(as, instr); break;
        case OP17_XVAVGR_B: ok = 1; GEN_XVAVGR_B_XVMAP(as, instr); break;
        case OP17_XVAVGR_BU: ok = 1; GEN_XVAVGR_BU_XVMAP(as, instr); break;
        case OP17_XVAVGR_D: ok = 1; GEN_XVAVGR_D_XVMAP(as, instr); break;
        case OP17_XVAVGR_DU: ok = 1; GEN_XVAVGR_DU_XVMAP(as, instr); break;
        case OP17_XVAVGR_H: ok = 1; GEN_XVAVGR_H_XVMAP(as, instr); break;
        case OP17_XVAVGR_HU: ok = 1; GEN_XVAVGR_HU_XVMAP(as, instr); break;
        case OP17_XVAVGR_W: ok = 1; GEN_XVAVGR_W_XVMAP(as, instr); break;
        case OP17_XVAVGR_WU: ok = 1; GEN_XVAVGR_WU_XVMAP(as, instr); break;
        case OP17_XVAVG_B: ok = 1; GEN_XVAVG_B_XVMAP(as, instr); break;
        case OP17_XVAVG_BU: ok = 1; GEN_XVAVG_BU_XVMAP(as, instr); break;
        case OP17_XVAVG_D: ok = 1; GEN_XVAVG_D_XVMAP(as, instr); break;
        case OP17_XVAVG_DU: ok = 1; GEN_XVAVG_DU_XVMAP(as, instr); break;
        case OP17_XVAVG_H: ok = 1; GEN_XVAVG_H_XVMAP(as, instr); break;
        case OP17_XVAVG_HU: ok = 1; GEN_XVAVG_HU_XVMAP(as, instr); break;
        case OP17_XVAVG_W: ok = 1; GEN_XVAVG_W_XVMAP(as, instr); break;
        case OP17_XVAVG_WU: ok = 1; GEN_XVAVG_WU_XVMAP(as, instr); break;
        case OP17_XVBITCLRI_W: ok = 1; GEN_XVBITCLRI_W_XVMAP(as, instr); break;
        case OP17_XVBITCLR_B: ok = 1; GEN_XVBITCLR_B_XVMAP(as, instr); break;
        case OP17_XVBITCLR_D: ok = 1; GEN_XVBITCLR_D_XVMAP(as, instr); break;
        case OP17_XVBITCLR_H: ok = 1; GEN_XVBITCLR_H_XVMAP(as, instr); break;
        case OP17_XVBITCLR_W: ok = 1; GEN_XVBITCLR_W_XVMAP(as, instr); break;
        case OP17_XVBITREVI_W: ok = 1; GEN_XVBITREVI_W_XVMAP(as, instr); break;
        case OP17_XVBITREV_B: ok = 1; GEN_XVBITREV_B_XVMAP(as, instr); break;
        case OP17_XVBITREV_D: ok = 1; GEN_XVBITREV_D_XVMAP(as, instr); break;
        case OP17_XVBITREV_H: ok = 1; GEN_XVBITREV_H_XVMAP(as, instr); break;
        case OP17_XVBITREV_W: ok = 1; GEN_XVBITREV_W_XVMAP(as, instr); break;
        case OP17_XVBITSETI_W: ok = 1; GEN_XVBITSETI_W_XVMAP(as, instr); break;
        case OP17_XVBITSET_B: ok = 1; GEN_XVBITSET_B_XVMAP(as, instr); break;
        case OP17_XVBITSET_D: ok = 1; GEN_XVBITSET_D_XVMAP(as, instr); break;
        case OP17_XVBITSET_H: ok = 1; GEN_XVBITSET_H_XVMAP(as, instr); break;
        case OP17_XVBITSET_W: ok = 1; GEN_XVBITSET_W_XVMAP(as, instr); break;
        case OP17_XVBSLL_V: ok = 1; GEN_XVBSLL_V_XVMAP(as, instr); break;
        case OP17_XVBSRL_V: ok = 1; GEN_XVBSRL_V_XVMAP(as, instr); break;
        case OP17_XVDIV_B: ok = 1; GEN_XVDIV_B_XVMAP(as, instr); break;
        case OP17_XVDIV_BU: ok = 1; GEN_XVDIV_BU_XVMAP(as, instr); break;
        case OP17_XVDIV_D: ok = 1; GEN_XVDIV_D_XVMAP(as, instr); break;
        case OP17_XVDIV_DU: ok = 1; GEN_XVDIV_DU_XVMAP(as, instr); break;
        case OP17_XVDIV_H: ok = 1; GEN_XVDIV_H_XVMAP(as, instr); break;
        case OP17_XVDIV_HU: ok = 1; GEN_XVDIV_HU_XVMAP(as, instr); break;
        case OP17_XVDIV_W: ok = 1; GEN_XVDIV_W_XVMAP(as, instr); break;
        case OP17_XVDIV_WU: ok = 1; GEN_XVDIV_WU_XVMAP(as, instr); break;
        case OP17_XVFADD_D: ok = 1; GEN_XVFADD_D_XVMAP(as, instr); break;
        case OP17_XVFADD_S: ok = 1; GEN_XVFADD_S_XVMAP(as, instr); break;
        case OP17_XVFCVT_H_S: ok = 1; GEN_XVFCVT_H_S_XVMAP(as, instr); break;
        case OP17_XVFCVT_S_D: ok = 1; GEN_XVFCVT_S_D_XVMAP(as, instr); break;
        case OP17_XVFDIV_D: ok = 1; GEN_XVFDIV_D_XVMAP(as, instr); break;
        case OP17_XVFDIV_S: ok = 1; GEN_XVFDIV_S_XVMAP(as, instr); break;
        case OP17_XVFFINT_S_L: ok = 1; GEN_XVFFINT_S_L_XVMAP(as, instr); break;
        case OP17_XVFMAXA_D: ok = 1; GEN_XVFMAXA_D_XVMAP(as, instr); break;
        case OP17_XVFMAXA_S: ok = 1; GEN_XVFMAXA_S_XVMAP(as, instr); break;
        case OP17_XVFMAX_D: ok = 1; GEN_XVFMAX_D_XVMAP(as, instr); break;
        case OP17_XVFMAX_S: ok = 1; GEN_XVFMAX_S_XVMAP(as, instr); break;
        case OP17_XVFMINA_D: ok = 1; GEN_XVFMINA_D_XVMAP(as, instr); break;
        case OP17_XVFMINA_S: ok = 1; GEN_XVFMINA_S_XVMAP(as, instr); break;
        case OP17_XVFMIN_D: ok = 1; GEN_XVFMIN_D_XVMAP(as, instr); break;
        case OP17_XVFMIN_S: ok = 1; GEN_XVFMIN_S_XVMAP(as, instr); break;
        case OP17_XVFMUL_D: ok = 1; GEN_XVFMUL_D_XVMAP(as, instr); break;
        case OP17_XVFMUL_S: ok = 1; GEN_XVFMUL_S_XVMAP(as, instr); break;
        case OP17_XVFRSTPI_B: ok = 1; GEN_XVFRSTPI_B_XVMAP(as, instr); break;
        case OP17_XVFRSTPI_H: ok = 1; GEN_XVFRSTPI_H_XVMAP(as, instr); break;
        case OP17_XVFRSTP_B: ok = 1; GEN_XVFRSTP_B_XVMAP(as, instr); break;
        case OP17_XVFRSTP_H: ok = 1; GEN_XVFRSTP_H_XVMAP(as, instr); break;
        case OP17_XVFSUB_D: ok = 1; GEN_XVFSUB_D_XVMAP(as, instr); break;
        case OP17_XVFSUB_S: ok = 1; GEN_XVFSUB_S_XVMAP(as, instr); break;
        case OP17_XVFTINTRM_W_D: ok = 1; GEN_XVFTINTRM_W_D_XVMAP(as, instr); break;
        case OP17_XVFTINTRNE_W_D: ok = 1; GEN_XVFTINTRNE_W_D_XVMAP(as, instr); break;
        case OP17_XVFTINTRP_W_D: ok = 1; GEN_XVFTINTRP_W_D_XVMAP(as, instr); break;
        case OP17_XVFTINTRZ_W_D: ok = 1; GEN_XVFTINTRZ_W_D_XVMAP(as, instr); break;
        case OP17_XVFTINT_W_D: ok = 1; GEN_XVFTINT_W_D_XVMAP(as, instr); break;
        case OP17_XVHADDW_DU_WU: ok = 1; GEN_XVHADDW_DU_WU_XVMAP(as, instr); break;
        case OP17_XVHADDW_D_W: ok = 1; GEN_XVHADDW_D_W_XVMAP(as, instr); break;
        case OP17_XVHADDW_HU_BU: ok = 1; GEN_XVHADDW_HU_BU_XVMAP(as, instr); break;
        case OP17_XVHADDW_H_B: ok = 1; GEN_XVHADDW_H_B_XVMAP(as, instr); break;
        case OP17_XVHADDW_QU_DU: ok = 1; GEN_XVHADDW_QU_DU_XVMAP(as, instr); break;
        case OP17_XVHADDW_Q_D: ok = 1; GEN_XVHADDW_Q_D_XVMAP(as, instr); break;
        case OP17_XVHADDW_WU_HU: ok = 1; GEN_XVHADDW_WU_HU_XVMAP(as, instr); break;
        case OP17_XVHADDW_W_H: ok = 1; GEN_XVHADDW_W_H_XVMAP(as, instr); break;
        case OP17_XVHSUBW_DU_WU: ok = 1; GEN_XVHSUBW_DU_WU_XVMAP(as, instr); break;
        case OP17_XVHSUBW_D_W: ok = 1; GEN_XVHSUBW_D_W_XVMAP(as, instr); break;
        case OP17_XVHSUBW_HU_BU: ok = 1; GEN_XVHSUBW_HU_BU_XVMAP(as, instr); break;
        case OP17_XVHSUBW_H_B: ok = 1; GEN_XVHSUBW_H_B_XVMAP(as, instr); break;
        case OP17_XVHSUBW_QU_DU: ok = 1; GEN_XVHSUBW_QU_DU_XVMAP(as, instr); break;
        case OP17_XVHSUBW_Q_D: ok = 1; GEN_XVHSUBW_Q_D_XVMAP(as, instr); break;
        case OP17_XVHSUBW_WU_HU: ok = 1; GEN_XVHSUBW_WU_HU_XVMAP(as, instr); break;
        case OP17_XVHSUBW_W_H: ok = 1; GEN_XVHSUBW_W_H_XVMAP(as, instr); break;
        case OP17_XVILVH_B: ok = 1; GEN_XVILVH_B_XVMAP(as, instr); break;
        case OP17_XVILVH_D: ok = 1; GEN_XVILVH_D_XVMAP(as, instr); break;
        case OP17_XVILVH_H: ok = 1; GEN_XVILVH_H_XVMAP(as, instr); break;
        case OP17_XVILVH_W: ok = 1; GEN_XVILVH_W_XVMAP(as, instr); break;
        case OP17_XVILVL_B: ok = 1; GEN_XVILVL_B_XVMAP(as, instr); break;
        case OP17_XVILVL_D: ok = 1; GEN_XVILVL_D_XVMAP(as, instr); break;
        case OP17_XVILVL_H: ok = 1; GEN_XVILVL_H_XVMAP(as, instr); break;
        case OP17_XVILVL_W: ok = 1; GEN_XVILVL_W_XVMAP(as, instr); break;
        case OP17_XVLDX: ok = 1; GEN_XVLDX_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_D_W: ok = 1; GEN_XVMADDWEV_D_W_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_D_WU: ok = 1; GEN_XVMADDWEV_D_WU_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_D_WU_W: ok = 1; GEN_XVMADDWEV_D_WU_W_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_H_B: ok = 1; GEN_XVMADDWEV_H_B_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_H_BU: ok = 1; GEN_XVMADDWEV_H_BU_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_H_BU_B: ok = 1; GEN_XVMADDWEV_H_BU_B_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_Q_D: ok = 1; GEN_XVMADDWEV_Q_D_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_Q_DU: ok = 1; GEN_XVMADDWEV_Q_DU_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_Q_DU_D: ok = 1; GEN_XVMADDWEV_Q_DU_D_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_W_H: ok = 1; GEN_XVMADDWEV_W_H_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_W_HU: ok = 1; GEN_XVMADDWEV_W_HU_XVMAP(as, instr); break;
        case OP17_XVMADDWEV_W_HU_H: ok = 1; GEN_XVMADDWEV_W_HU_H_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_D_W: ok = 1; GEN_XVMADDWOD_D_W_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_D_WU: ok = 1; GEN_XVMADDWOD_D_WU_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_D_WU_W: ok = 1; GEN_XVMADDWOD_D_WU_W_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_H_B: ok = 1; GEN_XVMADDWOD_H_B_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_H_BU: ok = 1; GEN_XVMADDWOD_H_BU_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_H_BU_B: ok = 1; GEN_XVMADDWOD_H_BU_B_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_Q_D: ok = 1; GEN_XVMADDWOD_Q_D_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_Q_DU: ok = 1; GEN_XVMADDWOD_Q_DU_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_Q_DU_D: ok = 1; GEN_XVMADDWOD_Q_DU_D_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_W_H: ok = 1; GEN_XVMADDWOD_W_H_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_W_HU: ok = 1; GEN_XVMADDWOD_W_HU_XVMAP(as, instr); break;
        case OP17_XVMADDWOD_W_HU_H: ok = 1; GEN_XVMADDWOD_W_HU_H_XVMAP(as, instr); break;
        case OP17_XVMADD_B: ok = 1; GEN_XVMADD_B_XVMAP(as, instr); break;
        case OP17_XVMADD_D: ok = 1; GEN_XVMADD_D_XVMAP(as, instr); break;
        case OP17_XVMADD_H: ok = 1; GEN_XVMADD_H_XVMAP(as, instr); break;
        case OP17_XVMADD_W: ok = 1; GEN_XVMADD_W_XVMAP(as, instr); break;
        case OP17_XVMAXI_B: ok = 1; GEN_XVMAXI_B_XVMAP(as, instr); break;
        case OP17_XVMAXI_BU: ok = 1; GEN_XVMAXI_BU_XVMAP(as, instr); break;
        case OP17_XVMAXI_D: ok = 1; GEN_XVMAXI_D_XVMAP(as, instr); break;
        case OP17_XVMAXI_DU: ok = 1; GEN_XVMAXI_DU_XVMAP(as, instr); break;
        case OP17_XVMAXI_H: ok = 1; GEN_XVMAXI_H_XVMAP(as, instr); break;
        case OP17_XVMAXI_HU: ok = 1; GEN_XVMAXI_HU_XVMAP(as, instr); break;
        case OP17_XVMAXI_W: ok = 1; GEN_XVMAXI_W_XVMAP(as, instr); break;
        case OP17_XVMAXI_WU: ok = 1; GEN_XVMAXI_WU_XVMAP(as, instr); break;
        case OP17_XVMAX_B: ok = 1; GEN_XVMAX_B_XVMAP(as, instr); break;
        case OP17_XVMAX_BU: ok = 1; GEN_XVMAX_BU_XVMAP(as, instr); break;
        case OP17_XVMAX_D: ok = 1; GEN_XVMAX_D_XVMAP(as, instr); break;
        case OP17_XVMAX_DU: ok = 1; GEN_XVMAX_DU_XVMAP(as, instr); break;
        case OP17_XVMAX_H: ok = 1; GEN_XVMAX_H_XVMAP(as, instr); break;
        case OP17_XVMAX_HU: ok = 1; GEN_XVMAX_HU_XVMAP(as, instr); break;
        case OP17_XVMAX_W: ok = 1; GEN_XVMAX_W_XVMAP(as, instr); break;
        case OP17_XVMAX_WU: ok = 1; GEN_XVMAX_WU_XVMAP(as, instr); break;
        case OP17_XVMINI_B: ok = 1; GEN_XVMINI_B_XVMAP(as, instr); break;
        case OP17_XVMINI_BU: ok = 1; GEN_XVMINI_BU_XVMAP(as, instr); break;
        case OP17_XVMINI_D: ok = 1; GEN_XVMINI_D_XVMAP(as, instr); break;
        case OP17_XVMINI_DU: ok = 1; GEN_XVMINI_DU_XVMAP(as, instr); break;
        case OP17_XVMINI_H: ok = 1; GEN_XVMINI_H_XVMAP(as, instr); break;
        case OP17_XVMINI_HU: ok = 1; GEN_XVMINI_HU_XVMAP(as, instr); break;
        case OP17_XVMINI_W: ok = 1; GEN_XVMINI_W_XVMAP(as, instr); break;
        case OP17_XVMINI_WU: ok = 1; GEN_XVMINI_WU_XVMAP(as, instr); break;
        case OP17_XVMIN_B: ok = 1; GEN_XVMIN_B_XVMAP(as, instr); break;
        case OP17_XVMIN_BU: ok = 1; GEN_XVMIN_BU_XVMAP(as, instr); break;
        case OP17_XVMIN_D: ok = 1; GEN_XVMIN_D_XVMAP(as, instr); break;
        case OP17_XVMIN_DU: ok = 1; GEN_XVMIN_DU_XVMAP(as, instr); break;
        case OP17_XVMIN_H: ok = 1; GEN_XVMIN_H_XVMAP(as, instr); break;
        case OP17_XVMIN_HU: ok = 1; GEN_XVMIN_HU_XVMAP(as, instr); break;
        case OP17_XVMIN_W: ok = 1; GEN_XVMIN_W_XVMAP(as, instr); break;
        case OP17_XVMIN_WU: ok = 1; GEN_XVMIN_WU_XVMAP(as, instr); break;
        case OP17_XVMOD_B: ok = 1; GEN_XVMOD_B_XVMAP(as, instr); break;
        case OP17_XVMOD_BU: ok = 1; GEN_XVMOD_BU_XVMAP(as, instr); break;
        case OP17_XVMOD_D: ok = 1; GEN_XVMOD_D_XVMAP(as, instr); break;
        case OP17_XVMOD_DU: ok = 1; GEN_XVMOD_DU_XVMAP(as, instr); break;
        case OP17_XVMOD_H: ok = 1; GEN_XVMOD_H_XVMAP(as, instr); break;
        case OP17_XVMOD_HU: ok = 1; GEN_XVMOD_HU_XVMAP(as, instr); break;
        case OP17_XVMOD_W: ok = 1; GEN_XVMOD_W_XVMAP(as, instr); break;
        case OP17_XVMOD_WU: ok = 1; GEN_XVMOD_WU_XVMAP(as, instr); break;
        case OP17_XVMSUB_B: ok = 1; GEN_XVMSUB_B_XVMAP(as, instr); break;
        case OP17_XVMSUB_D: ok = 1; GEN_XVMSUB_D_XVMAP(as, instr); break;
        case OP17_XVMSUB_H: ok = 1; GEN_XVMSUB_H_XVMAP(as, instr); break;
        case OP17_XVMSUB_W: ok = 1; GEN_XVMSUB_W_XVMAP(as, instr); break;
        case OP17_XVMUH_B: ok = 1; GEN_XVMUH_B_XVMAP(as, instr); break;
        case OP17_XVMUH_BU: ok = 1; GEN_XVMUH_BU_XVMAP(as, instr); break;
        case OP17_XVMUH_D: ok = 1; GEN_XVMUH_D_XVMAP(as, instr); break;
        case OP17_XVMUH_DU: ok = 1; GEN_XVMUH_DU_XVMAP(as, instr); break;
        case OP17_XVMUH_H: ok = 1; GEN_XVMUH_H_XVMAP(as, instr); break;
        case OP17_XVMUH_HU: ok = 1; GEN_XVMUH_HU_XVMAP(as, instr); break;
        case OP17_XVMUH_W: ok = 1; GEN_XVMUH_W_XVMAP(as, instr); break;
        case OP17_XVMUH_WU: ok = 1; GEN_XVMUH_WU_XVMAP(as, instr); break;
        case OP17_XVMULWEV_D_W: ok = 1; GEN_XVMULWEV_D_W_XVMAP(as, instr); break;
        case OP17_XVMULWEV_D_WU: ok = 1; GEN_XVMULWEV_D_WU_XVMAP(as, instr); break;
        case OP17_XVMULWEV_D_WU_W: ok = 1; GEN_XVMULWEV_D_WU_W_XVMAP(as, instr); break;
        case OP17_XVMULWEV_H_B: ok = 1; GEN_XVMULWEV_H_B_XVMAP(as, instr); break;
        case OP17_XVMULWEV_H_BU: ok = 1; GEN_XVMULWEV_H_BU_XVMAP(as, instr); break;
        case OP17_XVMULWEV_H_BU_B: ok = 1; GEN_XVMULWEV_H_BU_B_XVMAP(as, instr); break;
        case OP17_XVMULWEV_Q_D: ok = 1; GEN_XVMULWEV_Q_D_XVMAP(as, instr); break;
        case OP17_XVMULWEV_Q_DU: ok = 1; GEN_XVMULWEV_Q_DU_XVMAP(as, instr); break;
        case OP17_XVMULWEV_Q_DU_D: ok = 1; GEN_XVMULWEV_Q_DU_D_XVMAP(as, instr); break;
        case OP17_XVMULWEV_W_H: ok = 1; GEN_XVMULWEV_W_H_XVMAP(as, instr); break;
        case OP17_XVMULWEV_W_HU: ok = 1; GEN_XVMULWEV_W_HU_XVMAP(as, instr); break;
        case OP17_XVMULWEV_W_HU_H: ok = 1; GEN_XVMULWEV_W_HU_H_XVMAP(as, instr); break;
        case OP17_XVMULWOD_D_W: ok = 1; GEN_XVMULWOD_D_W_XVMAP(as, instr); break;
        case OP17_XVMULWOD_D_WU: ok = 1; GEN_XVMULWOD_D_WU_XVMAP(as, instr); break;
        case OP17_XVMULWOD_D_WU_W: ok = 1; GEN_XVMULWOD_D_WU_W_XVMAP(as, instr); break;
        case OP17_XVMULWOD_H_B: ok = 1; GEN_XVMULWOD_H_B_XVMAP(as, instr); break;
        case OP17_XVMULWOD_H_BU: ok = 1; GEN_XVMULWOD_H_BU_XVMAP(as, instr); break;
        case OP17_XVMULWOD_H_BU_B: ok = 1; GEN_XVMULWOD_H_BU_B_XVMAP(as, instr); break;
        case OP17_XVMULWOD_Q_D: ok = 1; GEN_XVMULWOD_Q_D_XVMAP(as, instr); break;
        case OP17_XVMULWOD_Q_DU: ok = 1; GEN_XVMULWOD_Q_DU_XVMAP(as, instr); break;
        case OP17_XVMULWOD_Q_DU_D: ok = 1; GEN_XVMULWOD_Q_DU_D_XVMAP(as, instr); break;
        case OP17_XVMULWOD_W_H: ok = 1; GEN_XVMULWOD_W_H_XVMAP(as, instr); break;
        case OP17_XVMULWOD_W_HU: ok = 1; GEN_XVMULWOD_W_HU_XVMAP(as, instr); break;
        case OP17_XVMULWOD_W_HU_H: ok = 1; GEN_XVMULWOD_W_HU_H_XVMAP(as, instr); break;
        case OP17_XVMUL_B: ok = 1; GEN_XVMUL_B_XVMAP(as, instr); break;
        case OP17_XVMUL_D: ok = 1; GEN_XVMUL_D_XVMAP(as, instr); break;
        case OP17_XVMUL_H: ok = 1; GEN_XVMUL_H_XVMAP(as, instr); break;
        case OP17_XVMUL_W: ok = 1; GEN_XVMUL_W_XVMAP(as, instr); break;
        case OP17_XVNOR_V: ok = 1; GEN_XVNOR_V_XVMAP(as, instr); break;
        case OP17_XVORN_V: ok = 1; GEN_XVORN_V_XVMAP(as, instr); break;
        case OP17_XVOR_V: ok = 1; GEN_XVOR_V_XVMAP(as, instr); break;
        case OP17_XVPACKEV_B: ok = 1; GEN_XVPACKEV_B_XVMAP(as, instr); break;
        case OP17_XVPACKEV_D: ok = 1; GEN_XVPACKEV_D_XVMAP(as, instr); break;
        case OP17_XVPACKEV_H: ok = 1; GEN_XVPACKEV_H_XVMAP(as, instr); break;
        case OP17_XVPACKEV_W: ok = 1; GEN_XVPACKEV_W_XVMAP(as, instr); break;
        case OP17_XVPACKOD_B: ok = 1; GEN_XVPACKOD_B_XVMAP(as, instr); break;
        case OP17_XVPACKOD_D: ok = 1; GEN_XVPACKOD_D_XVMAP(as, instr); break;
        case OP17_XVPACKOD_H: ok = 1; GEN_XVPACKOD_H_XVMAP(as, instr); break;
        case OP17_XVPACKOD_W: ok = 1; GEN_XVPACKOD_W_XVMAP(as, instr); break;
        case OP17_XVPERM_W: ok = 1; GEN_XVPERM_W_XVMAP(as, instr); break;
        case OP17_XVPICKEV_B: ok = 1; GEN_XVPICKEV_B_XVMAP(as, instr); break;
        case OP17_XVPICKEV_D: ok = 1; GEN_XVPICKEV_D_XVMAP(as, instr); break;
        case OP17_XVPICKEV_H: ok = 1; GEN_XVPICKEV_H_XVMAP(as, instr); break;
        case OP17_XVPICKEV_W: ok = 1; GEN_XVPICKEV_W_XVMAP(as, instr); break;
        case OP17_XVPICKOD_B: ok = 1; GEN_XVPICKOD_B_XVMAP(as, instr); break;
        case OP17_XVPICKOD_D: ok = 1; GEN_XVPICKOD_D_XVMAP(as, instr); break;
        case OP17_XVPICKOD_H: ok = 1; GEN_XVPICKOD_H_XVMAP(as, instr); break;
        case OP17_XVPICKOD_W: ok = 1; GEN_XVPICKOD_W_XVMAP(as, instr); break;
        case OP17_XVREPLVE_B: ok = 1; GEN_XVREPLVE_B_XVMAP(as, instr); break;
        case OP17_XVREPLVE_D: ok = 1; GEN_XVREPLVE_D_XVMAP(as, instr); break;
        case OP17_XVREPLVE_H: ok = 1; GEN_XVREPLVE_H_XVMAP(as, instr); break;
        case OP17_XVREPLVE_W: ok = 1; GEN_XVREPLVE_W_XVMAP(as, instr); break;
        case OP17_XVROTRI_W: ok = 1; GEN_XVROTRI_W_XVMAP(as, instr); break;
        case OP17_XVROTR_B: ok = 1; GEN_XVROTR_B_XVMAP(as, instr); break;
        case OP17_XVROTR_D: ok = 1; GEN_XVROTR_D_XVMAP(as, instr); break;
        case OP17_XVROTR_H: ok = 1; GEN_XVROTR_H_XVMAP(as, instr); break;
        case OP17_XVROTR_W: ok = 1; GEN_XVROTR_W_XVMAP(as, instr); break;
        case OP17_XVSADD_B: ok = 1; GEN_XVSADD_B_XVMAP(as, instr); break;
        case OP17_XVSADD_BU: ok = 1; GEN_XVSADD_BU_XVMAP(as, instr); break;
        case OP17_XVSADD_D: ok = 1; GEN_XVSADD_D_XVMAP(as, instr); break;
        case OP17_XVSADD_DU: ok = 1; GEN_XVSADD_DU_XVMAP(as, instr); break;
        case OP17_XVSADD_H: ok = 1; GEN_XVSADD_H_XVMAP(as, instr); break;
        case OP17_XVSADD_HU: ok = 1; GEN_XVSADD_HU_XVMAP(as, instr); break;
        case OP17_XVSADD_W: ok = 1; GEN_XVSADD_W_XVMAP(as, instr); break;
        case OP17_XVSADD_WU: ok = 1; GEN_XVSADD_WU_XVMAP(as, instr); break;
        case OP17_XVSAT_W: ok = 1; GEN_XVSAT_W_XVMAP(as, instr); break;
        case OP17_XVSAT_WU: ok = 1; GEN_XVSAT_WU_XVMAP(as, instr); break;
        case OP17_XVSEQI_B: ok = 1; GEN_XVSEQI_B_XVMAP(as, instr); break;
        case OP17_XVSEQI_D: ok = 1; GEN_XVSEQI_D_XVMAP(as, instr); break;
        case OP17_XVSEQI_H: ok = 1; GEN_XVSEQI_H_XVMAP(as, instr); break;
        case OP17_XVSEQI_W: ok = 1; GEN_XVSEQI_W_XVMAP(as, instr); break;
        case OP17_XVSEQ_B: ok = 1; GEN_XVSEQ_B_XVMAP(as, instr); break;
        case OP17_XVSEQ_D: ok = 1; GEN_XVSEQ_D_XVMAP(as, instr); break;
        case OP17_XVSEQ_H: ok = 1; GEN_XVSEQ_H_XVMAP(as, instr); break;
        case OP17_XVSEQ_W: ok = 1; GEN_XVSEQ_W_XVMAP(as, instr); break;
        case OP17_XVSHUF_D: ok = 1; GEN_XVSHUF_D_XVMAP(as, instr); break;
        case OP17_XVSHUF_H: ok = 1; GEN_XVSHUF_H_XVMAP(as, instr); break;
        case OP17_XVSHUF_W: ok = 1; GEN_XVSHUF_W_XVMAP(as, instr); break;
        case OP17_XVSIGNCOV_B: ok = 1; GEN_XVSIGNCOV_B_XVMAP(as, instr); break;
        case OP17_XVSIGNCOV_D: ok = 1; GEN_XVSIGNCOV_D_XVMAP(as, instr); break;
        case OP17_XVSIGNCOV_H: ok = 1; GEN_XVSIGNCOV_H_XVMAP(as, instr); break;
        case OP17_XVSIGNCOV_W: ok = 1; GEN_XVSIGNCOV_W_XVMAP(as, instr); break;
        case OP17_XVSLEI_B: ok = 1; GEN_XVSLEI_B_XVMAP(as, instr); break;
        case OP17_XVSLEI_BU: ok = 1; GEN_XVSLEI_BU_XVMAP(as, instr); break;
        case OP17_XVSLEI_D: ok = 1; GEN_XVSLEI_D_XVMAP(as, instr); break;
        case OP17_XVSLEI_DU: ok = 1; GEN_XVSLEI_DU_XVMAP(as, instr); break;
        case OP17_XVSLEI_H: ok = 1; GEN_XVSLEI_H_XVMAP(as, instr); break;
        case OP17_XVSLEI_HU: ok = 1; GEN_XVSLEI_HU_XVMAP(as, instr); break;
        case OP17_XVSLEI_W: ok = 1; GEN_XVSLEI_W_XVMAP(as, instr); break;
        case OP17_XVSLEI_WU: ok = 1; GEN_XVSLEI_WU_XVMAP(as, instr); break;
        case OP17_XVSLE_B: ok = 1; GEN_XVSLE_B_XVMAP(as, instr); break;
        case OP17_XVSLE_BU: ok = 1; GEN_XVSLE_BU_XVMAP(as, instr); break;
        case OP17_XVSLE_D: ok = 1; GEN_XVSLE_D_XVMAP(as, instr); break;
        case OP17_XVSLE_DU: ok = 1; GEN_XVSLE_DU_XVMAP(as, instr); break;
        case OP17_XVSLE_H: ok = 1; GEN_XVSLE_H_XVMAP(as, instr); break;
        case OP17_XVSLE_HU: ok = 1; GEN_XVSLE_HU_XVMAP(as, instr); break;
        case OP17_XVSLE_W: ok = 1; GEN_XVSLE_W_XVMAP(as, instr); break;
        case OP17_XVSLE_WU: ok = 1; GEN_XVSLE_WU_XVMAP(as, instr); break;
        case OP17_XVSLLI_W: ok = 1; GEN_XVSLLI_W_XVMAP(as, instr); break;
        case OP17_XVSLLWIL_DU_WU: ok = 1; GEN_XVSLLWIL_DU_WU_XVMAP(as, instr); break;
        case OP17_XVSLLWIL_D_W: ok = 1; GEN_XVSLLWIL_D_W_XVMAP(as, instr); break;
        case OP17_XVSLL_B: ok = 1; GEN_XVSLL_B_XVMAP(as, instr); break;
        case OP17_XVSLL_D: ok = 1; GEN_XVSLL_D_XVMAP(as, instr); break;
        case OP17_XVSLL_H: ok = 1; GEN_XVSLL_H_XVMAP(as, instr); break;
        case OP17_XVSLL_W: ok = 1; GEN_XVSLL_W_XVMAP(as, instr); break;
        case OP17_XVSLTI_B: ok = 1; GEN_XVSLTI_B_XVMAP(as, instr); break;
        case OP17_XVSLTI_BU: ok = 1; GEN_XVSLTI_BU_XVMAP(as, instr); break;
        case OP17_XVSLTI_D: ok = 1; GEN_XVSLTI_D_XVMAP(as, instr); break;
        case OP17_XVSLTI_DU: ok = 1; GEN_XVSLTI_DU_XVMAP(as, instr); break;
        case OP17_XVSLTI_H: ok = 1; GEN_XVSLTI_H_XVMAP(as, instr); break;
        case OP17_XVSLTI_HU: ok = 1; GEN_XVSLTI_HU_XVMAP(as, instr); break;
        case OP17_XVSLTI_W: ok = 1; GEN_XVSLTI_W_XVMAP(as, instr); break;
        case OP17_XVSLTI_WU: ok = 1; GEN_XVSLTI_WU_XVMAP(as, instr); break;
        case OP17_XVSLT_B: ok = 1; GEN_XVSLT_B_XVMAP(as, instr); break;
        case OP17_XVSLT_BU: ok = 1; GEN_XVSLT_BU_XVMAP(as, instr); break;
        case OP17_XVSLT_D: ok = 1; GEN_XVSLT_D_XVMAP(as, instr); break;
        case OP17_XVSLT_DU: ok = 1; GEN_XVSLT_DU_XVMAP(as, instr); break;
        case OP17_XVSLT_H: ok = 1; GEN_XVSLT_H_XVMAP(as, instr); break;
        case OP17_XVSLT_HU: ok = 1; GEN_XVSLT_HU_XVMAP(as, instr); break;
        case OP17_XVSLT_W: ok = 1; GEN_XVSLT_W_XVMAP(as, instr); break;
        case OP17_XVSLT_WU: ok = 1; GEN_XVSLT_WU_XVMAP(as, instr); break;
        case OP17_XVSRAI_W: ok = 1; GEN_XVSRAI_W_XVMAP(as, instr); break;
        case OP17_XVSRANI_H_W: ok = 1; GEN_XVSRANI_H_W_XVMAP(as, instr); break;
        case OP17_XVSRAN_B_H: ok = 1; GEN_XVSRAN_B_H_XVMAP(as, instr); break;
        case OP17_XVSRAN_H_W: ok = 1; GEN_XVSRAN_H_W_XVMAP(as, instr); break;
        case OP17_XVSRAN_W_D: ok = 1; GEN_XVSRAN_W_D_XVMAP(as, instr); break;
        case OP17_XVSRARI_W: ok = 1; GEN_XVSRARI_W_XVMAP(as, instr); break;
        case OP17_XVSRARNI_H_W: ok = 1; GEN_XVSRARNI_H_W_XVMAP(as, instr); break;
        case OP17_XVSRARN_B_H: ok = 1; GEN_XVSRARN_B_H_XVMAP(as, instr); break;
        case OP17_XVSRARN_H_W: ok = 1; GEN_XVSRARN_H_W_XVMAP(as, instr); break;
        case OP17_XVSRARN_W_D: ok = 1; GEN_XVSRARN_W_D_XVMAP(as, instr); break;
        case OP17_XVSRAR_B: ok = 1; GEN_XVSRAR_B_XVMAP(as, instr); break;
        case OP17_XVSRAR_D: ok = 1; GEN_XVSRAR_D_XVMAP(as, instr); break;
        case OP17_XVSRAR_H: ok = 1; GEN_XVSRAR_H_XVMAP(as, instr); break;
        case OP17_XVSRAR_W: ok = 1; GEN_XVSRAR_W_XVMAP(as, instr); break;
        case OP17_XVSRA_B: ok = 1; GEN_XVSRA_B_XVMAP(as, instr); break;
        case OP17_XVSRA_D: ok = 1; GEN_XVSRA_D_XVMAP(as, instr); break;
        case OP17_XVSRA_H: ok = 1; GEN_XVSRA_H_XVMAP(as, instr); break;
        case OP17_XVSRA_W: ok = 1; GEN_XVSRA_W_XVMAP(as, instr); break;
        case OP17_XVSRLI_W: ok = 1; GEN_XVSRLI_W_XVMAP(as, instr); break;
        case OP17_XVSRLNI_H_W: ok = 1; GEN_XVSRLNI_H_W_XVMAP(as, instr); break;
        case OP17_XVSRLN_B_H: ok = 1; GEN_XVSRLN_B_H_XVMAP(as, instr); break;
        case OP17_XVSRLN_H_W: ok = 1; GEN_XVSRLN_H_W_XVMAP(as, instr); break;
        case OP17_XVSRLN_W_D: ok = 1; GEN_XVSRLN_W_D_XVMAP(as, instr); break;
        case OP17_XVSRLRI_W: ok = 1; GEN_XVSRLRI_W_XVMAP(as, instr); break;
        case OP17_XVSRLRNI_H_W: ok = 1; GEN_XVSRLRNI_H_W_XVMAP(as, instr); break;
        case OP17_XVSRLRN_B_H: ok = 1; GEN_XVSRLRN_B_H_XVMAP(as, instr); break;
        case OP17_XVSRLRN_H_W: ok = 1; GEN_XVSRLRN_H_W_XVMAP(as, instr); break;
        case OP17_XVSRLRN_W_D: ok = 1; GEN_XVSRLRN_W_D_XVMAP(as, instr); break;
        case OP17_XVSRLR_B: ok = 1; GEN_XVSRLR_B_XVMAP(as, instr); break;
        case OP17_XVSRLR_D: ok = 1; GEN_XVSRLR_D_XVMAP(as, instr); break;
        case OP17_XVSRLR_H: ok = 1; GEN_XVSRLR_H_XVMAP(as, instr); break;
        case OP17_XVSRLR_W: ok = 1; GEN_XVSRLR_W_XVMAP(as, instr); break;
        case OP17_XVSRL_B: ok = 1; GEN_XVSRL_B_XVMAP(as, instr); break;
        case OP17_XVSRL_D: ok = 1; GEN_XVSRL_D_XVMAP(as, instr); break;
        case OP17_XVSRL_H: ok = 1; GEN_XVSRL_H_XVMAP(as, instr); break;
        case OP17_XVSRL_W: ok = 1; GEN_XVSRL_W_XVMAP(as, instr); break;
        case OP17_XVSSRANI_HU_W: ok = 1; GEN_XVSSRANI_HU_W_XVMAP(as, instr); break;
        case OP17_XVSSRANI_H_W: ok = 1; GEN_XVSSRANI_H_W_XVMAP(as, instr); break;
        case OP17_XVSSRAN_BU_H: ok = 1; GEN_XVSSRAN_BU_H_XVMAP(as, instr); break;
        case OP17_XVSSRAN_B_H: ok = 1; GEN_XVSSRAN_B_H_XVMAP(as, instr); break;
        case OP17_XVSSRAN_HU_W: ok = 1; GEN_XVSSRAN_HU_W_XVMAP(as, instr); break;
        case OP17_XVSSRAN_H_W: ok = 1; GEN_XVSSRAN_H_W_XVMAP(as, instr); break;
        case OP17_XVSSRAN_WU_D: ok = 1; GEN_XVSSRAN_WU_D_XVMAP(as, instr); break;
        case OP17_XVSSRAN_W_D: ok = 1; GEN_XVSSRAN_W_D_XVMAP(as, instr); break;
        case OP17_XVSSRARNI_HU_W: ok = 1; GEN_XVSSRARNI_HU_W_XVMAP(as, instr); break;
        case OP17_XVSSRARNI_H_W: ok = 1; GEN_XVSSRARNI_H_W_XVMAP(as, instr); break;
        case OP17_XVSSRARN_BU_H: ok = 1; GEN_XVSSRARN_BU_H_XVMAP(as, instr); break;
        case OP17_XVSSRARN_B_H: ok = 1; GEN_XVSSRARN_B_H_XVMAP(as, instr); break;
        case OP17_XVSSRARN_HU_W: ok = 1; GEN_XVSSRARN_HU_W_XVMAP(as, instr); break;
        case OP17_XVSSRARN_H_W: ok = 1; GEN_XVSSRARN_H_W_XVMAP(as, instr); break;
        case OP17_XVSSRARN_WU_D: ok = 1; GEN_XVSSRARN_WU_D_XVMAP(as, instr); break;
        case OP17_XVSSRARN_W_D: ok = 1; GEN_XVSSRARN_W_D_XVMAP(as, instr); break;
        case OP17_XVSSRLNI_HU_W: ok = 1; GEN_XVSSRLNI_HU_W_XVMAP(as, instr); break;
        case OP17_XVSSRLNI_H_W: ok = 1; GEN_XVSSRLNI_H_W_XVMAP(as, instr); break;
        case OP17_XVSSRLN_BU_H: ok = 1; GEN_XVSSRLN_BU_H_XVMAP(as, instr); break;
        case OP17_XVSSRLN_B_H: ok = 1; GEN_XVSSRLN_B_H_XVMAP(as, instr); break;
        case OP17_XVSSRLN_HU_W: ok = 1; GEN_XVSSRLN_HU_W_XVMAP(as, instr); break;
        case OP17_XVSSRLN_H_W: ok = 1; GEN_XVSSRLN_H_W_XVMAP(as, instr); break;
        case OP17_XVSSRLN_WU_D: ok = 1; GEN_XVSSRLN_WU_D_XVMAP(as, instr); break;
        case OP17_XVSSRLN_W_D: ok = 1; GEN_XVSSRLN_W_D_XVMAP(as, instr); break;
        case OP17_XVSSRLRNI_HU_W: ok = 1; GEN_XVSSRLRNI_HU_W_XVMAP(as, instr); break;
        case OP17_XVSSRLRNI_H_W: ok = 1; GEN_XVSSRLRNI_H_W_XVMAP(as, instr); break;
        case OP17_XVSSRLRN_BU_H: ok = 1; GEN_XVSSRLRN_BU_H_XVMAP(as, instr); break;
        case OP17_XVSSRLRN_B_H: ok = 1; GEN_XVSSRLRN_B_H_XVMAP(as, instr); break;
        case OP17_XVSSRLRN_HU_W: ok = 1; GEN_XVSSRLRN_HU_W_XVMAP(as, instr); break;
        case OP17_XVSSRLRN_H_W: ok = 1; GEN_XVSSRLRN_H_W_XVMAP(as, instr); break;
        case OP17_XVSSRLRN_WU_D: ok = 1; GEN_XVSSRLRN_WU_D_XVMAP(as, instr); break;
        case OP17_XVSSRLRN_W_D: ok = 1; GEN_XVSSRLRN_W_D_XVMAP(as, instr); break;
        case OP17_XVSSUB_B: ok = 1; GEN_XVSSUB_B_XVMAP(as, instr); break;
        case OP17_XVSSUB_BU: ok = 1; GEN_XVSSUB_BU_XVMAP(as, instr); break;
        case OP17_XVSSUB_D: ok = 1; GEN_XVSSUB_D_XVMAP(as, instr); break;
        case OP17_XVSSUB_DU: ok = 1; GEN_XVSSUB_DU_XVMAP(as, instr); break;
        case OP17_XVSSUB_H: ok = 1; GEN_XVSSUB_H_XVMAP(as, instr); break;
        case OP17_XVSSUB_HU: ok = 1; GEN_XVSSUB_HU_XVMAP(as, instr); break;
        case OP17_XVSSUB_W: ok = 1; GEN_XVSSUB_W_XVMAP(as, instr); break;
        case OP17_XVSSUB_WU: ok = 1; GEN_XVSSUB_WU_XVMAP(as, instr); break;
        case OP17_XVSTX: ok = 1; GEN_XVSTX_XVMAP(as, instr); break;
        case OP17_XVSUBI_BU: ok = 1; GEN_XVSUBI_BU_XVMAP(as, instr); break;
        case OP17_XVSUBI_DU: ok = 1; GEN_XVSUBI_DU_XVMAP(as, instr); break;
        case OP17_XVSUBI_HU: ok = 1; GEN_XVSUBI_HU_XVMAP(as, instr); break;
        case OP17_XVSUBI_WU: ok = 1; GEN_XVSUBI_WU_XVMAP(as, instr); break;
        case OP17_XVSUBWEV_D_W: ok = 1; GEN_XVSUBWEV_D_W_XVMAP(as, instr); break;
        case OP17_XVSUBWEV_D_WU: ok = 1; GEN_XVSUBWEV_D_WU_XVMAP(as, instr); break;
        case OP17_XVSUBWEV_H_B: ok = 1; GEN_XVSUBWEV_H_B_XVMAP(as, instr); break;
        case OP17_XVSUBWEV_H_BU: ok = 1; GEN_XVSUBWEV_H_BU_XVMAP(as, instr); break;
        case OP17_XVSUBWEV_Q_D: ok = 1; GEN_XVSUBWEV_Q_D_XVMAP(as, instr); break;
        case OP17_XVSUBWEV_Q_DU: ok = 1; GEN_XVSUBWEV_Q_DU_XVMAP(as, instr); break;
        case OP17_XVSUBWEV_W_H: ok = 1; GEN_XVSUBWEV_W_H_XVMAP(as, instr); break;
        case OP17_XVSUBWEV_W_HU: ok = 1; GEN_XVSUBWEV_W_HU_XVMAP(as, instr); break;
        case OP17_XVSUBWOD_D_W: ok = 1; GEN_XVSUBWOD_D_W_XVMAP(as, instr); break;
        case OP17_XVSUBWOD_D_WU: ok = 1; GEN_XVSUBWOD_D_WU_XVMAP(as, instr); break;
        case OP17_XVSUBWOD_H_B: ok = 1; GEN_XVSUBWOD_H_B_XVMAP(as, instr); break;
        case OP17_XVSUBWOD_H_BU: ok = 1; GEN_XVSUBWOD_H_BU_XVMAP(as, instr); break;
        case OP17_XVSUBWOD_Q_D: ok = 1; GEN_XVSUBWOD_Q_D_XVMAP(as, instr); break;
        case OP17_XVSUBWOD_Q_DU: ok = 1; GEN_XVSUBWOD_Q_DU_XVMAP(as, instr); break;
        case OP17_XVSUBWOD_W_H: ok = 1; GEN_XVSUBWOD_W_H_XVMAP(as, instr); break;
        case OP17_XVSUBWOD_W_HU: ok = 1; GEN_XVSUBWOD_W_HU_XVMAP(as, instr); break;
        case OP17_XVSUB_B: ok = 1; GEN_XVSUB_B_XVMAP(as, instr); break;
        case OP17_XVSUB_D: ok = 1; GEN_XVSUB_D_XVMAP(as, instr); break;
        case OP17_XVSUB_H: ok = 1; GEN_XVSUB_H_XVMAP(as, instr); break;
        case OP17_XVSUB_Q: ok = 1; GEN_XVSUB_Q_XVMAP(as, instr); break;
        case OP17_XVSUB_W: ok = 1; GEN_XVSUB_W_XVMAP(as, instr); break;
        case OP17_XVXOR_V: ok = 1; GEN_XVXOR_V_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 16) {
        case OP16_XVBITCLRI_D: ok = 1; GEN_XVBITCLRI_D_XVMAP(as, instr); break;
        case OP16_XVBITREVI_D: ok = 1; GEN_XVBITREVI_D_XVMAP(as, instr); break;
        case OP16_XVBITSETI_D: ok = 1; GEN_XVBITSETI_D_XVMAP(as, instr); break;
        case OP16_XVROTRI_D: ok = 1; GEN_XVROTRI_D_XVMAP(as, instr); break;
        case OP16_XVSAT_D: ok = 1; GEN_XVSAT_D_XVMAP(as, instr); break;
        case OP16_XVSAT_DU: ok = 1; GEN_XVSAT_DU_XVMAP(as, instr); break;
        case OP16_XVSLLI_D: ok = 1; GEN_XVSLLI_D_XVMAP(as, instr); break;
        case OP16_XVSRAI_D: ok = 1; GEN_XVSRAI_D_XVMAP(as, instr); break;
        case OP16_XVSRANI_W_D: ok = 1; GEN_XVSRANI_W_D_XVMAP(as, instr); break;
        case OP16_XVSRARI_D: ok = 1; GEN_XVSRARI_D_XVMAP(as, instr); break;
        case OP16_XVSRARNI_W_D: ok = 1; GEN_XVSRARNI_W_D_XVMAP(as, instr); break;
        case OP16_XVSRLI_D: ok = 1; GEN_XVSRLI_D_XVMAP(as, instr); break;
        case OP16_XVSRLNI_W_D: ok = 1; GEN_XVSRLNI_W_D_XVMAP(as, instr); break;
        case OP16_XVSRLRI_D: ok = 1; GEN_XVSRLRI_D_XVMAP(as, instr); break;
        case OP16_XVSRLRNI_W_D: ok = 1; GEN_XVSRLRNI_W_D_XVMAP(as, instr); break;
        case OP16_XVSSRANI_WU_D: ok = 1; GEN_XVSSRANI_WU_D_XVMAP(as, instr); break;
        case OP16_XVSSRANI_W_D: ok = 1; GEN_XVSSRANI_W_D_XVMAP(as, instr); break;
        case OP16_XVSSRARNI_WU_D: ok = 1; GEN_XVSSRARNI_WU_D_XVMAP(as, instr); break;
        case OP16_XVSSRARNI_W_D: ok = 1; GEN_XVSSRARNI_W_D_XVMAP(as, instr); break;
        case OP16_XVSSRLNI_WU_D: ok = 1; GEN_XVSSRLNI_WU_D_XVMAP(as, instr); break;
        case OP16_XVSSRLNI_W_D: ok = 1; GEN_XVSSRLNI_W_D_XVMAP(as, instr); break;
        case OP16_XVSSRLRNI_WU_D: ok = 1; GEN_XVSSRLRNI_WU_D_XVMAP(as, instr); break;
        case OP16_XVSSRLRNI_W_D: ok = 1; GEN_XVSSRLRNI_W_D_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 17) {
        case OP15_XVSRANI_D_Q: ok = 1; GEN_XVSRANI_D_Q_XVMAP(as, instr); break;
        case OP15_XVSRARNI_D_Q: ok = 1; GEN_XVSRARNI_D_Q_XVMAP(as, instr); break;
        case OP15_XVSRLNI_D_Q: ok = 1; GEN_XVSRLNI_D_Q_XVMAP(as, instr); break;
        case OP15_XVSRLRNI_D_Q: ok = 1; GEN_XVSRLRNI_D_Q_XVMAP(as, instr); break;
        case OP15_XVSSRANI_DU_Q: ok = 1; GEN_XVSSRANI_DU_Q_XVMAP(as, instr); break;
        case OP15_XVSSRANI_D_Q: ok = 1; GEN_XVSSRANI_D_Q_XVMAP(as, instr); break;
        case OP15_XVSSRARNI_DU_Q: ok = 1; GEN_XVSSRARNI_DU_Q_XVMAP(as, instr); break;
        case OP15_XVSSRARNI_D_Q: ok = 1; GEN_XVSSRARNI_D_Q_XVMAP(as, instr); break;
        case OP15_XVSSRLNI_DU_Q: ok = 1; GEN_XVSSRLNI_DU_Q_XVMAP(as, instr); break;
        case OP15_XVSSRLNI_D_Q: ok = 1; GEN_XVSSRLNI_D_Q_XVMAP(as, instr); break;
        case OP15_XVSSRLRNI_DU_Q: ok = 1; GEN_XVSSRLRNI_DU_Q_XVMAP(as, instr); break;
        case OP15_XVSSRLRNI_D_Q: ok = 1; GEN_XVSSRLRNI_D_Q_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 18) {
        case OP14_XVANDI_B: ok = 1; GEN_XVANDI_B_XVMAP(as, instr); break;
        case OP14_XVBITSELI_B: ok = 1; GEN_XVBITSELI_B_XVMAP(as, instr); break;
        case OP14_XVEXTRINS_B: ok = 1; GEN_XVEXTRINS_B_XVMAP(as, instr); break;
        case OP14_XVEXTRINS_D: ok = 1; GEN_XVEXTRINS_D_XVMAP(as, instr); break;
        case OP14_XVEXTRINS_H: ok = 1; GEN_XVEXTRINS_H_XVMAP(as, instr); break;
        case OP14_XVEXTRINS_W: ok = 1; GEN_XVEXTRINS_W_XVMAP(as, instr); break;
        case OP14_XVLDI: ok = 1; GEN_XVLDI_XVMAP(as, instr); break;
        case OP14_XVNORI_B: ok = 1; GEN_XVNORI_B_XVMAP(as, instr); break;
        case OP14_XVORI_B: ok = 1; GEN_XVORI_B_XVMAP(as, instr); break;
        case OP14_XVPERMI_D: ok = 1; GEN_XVPERMI_D_XVMAP(as, instr); break;
        case OP14_XVPERMI_Q: ok = 1; GEN_XVPERMI_Q_XVMAP(as, instr); break;
        case OP14_XVPERMI_W: ok = 1; GEN_XVPERMI_W_XVMAP(as, instr); break;
        case OP14_XVSHUF4I_B: ok = 1; GEN_XVSHUF4I_B_XVMAP(as, instr); break;
        case OP14_XVSHUF4I_D: ok = 1; GEN_XVSHUF4I_D_XVMAP(as, instr); break;
        case OP14_XVSHUF4I_H: ok = 1; GEN_XVSHUF4I_H_XVMAP(as, instr); break;
        case OP14_XVSHUF4I_W: ok = 1; GEN_XVSHUF4I_W_XVMAP(as, instr); break;
        case OP14_XVXORI_B: ok = 1; GEN_XVXORI_B_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 19) {
        case OP13_XVLDREPL_D: ok = 1; GEN_XVLDREPL_D_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 20) {
        case OP12_XVBITSEL_V: ok = 1; GEN_XVBITSEL_V_XVMAP(as, instr); break;
        case OP12_XVFCMP_COND_D: ok = 1; GEN_XVFCMP_COND_D_XVMAP(as, instr); break;
        case OP12_XVFCMP_COND_S: ok = 1; GEN_XVFCMP_COND_S_XVMAP(as, instr); break;
        case OP12_XVFMADD_D: ok = 1; GEN_XVFMADD_D_XVMAP(as, instr); break;
        case OP12_XVFMADD_S: ok = 1; GEN_XVFMADD_S_XVMAP(as, instr); break;
        case OP12_XVFMSUB_D: ok = 1; GEN_XVFMSUB_D_XVMAP(as, instr); break;
        case OP12_XVFMSUB_S: ok = 1; GEN_XVFMSUB_S_XVMAP(as, instr); break;
        case OP12_XVFNMADD_D: ok = 1; GEN_XVFNMADD_D_XVMAP(as, instr); break;
        case OP12_XVFNMADD_S: ok = 1; GEN_XVFNMADD_S_XVMAP(as, instr); break;
        case OP12_XVFNMSUB_D: ok = 1; GEN_XVFNMSUB_D_XVMAP(as, instr); break;
        case OP12_XVFNMSUB_S: ok = 1; GEN_XVFNMSUB_S_XVMAP(as, instr); break;
        case OP12_XVLDREPL_W: ok = 1; GEN_XVLDREPL_W_XVMAP(as, instr); break;
        case OP12_XVSHUF_B: ok = 1; GEN_XVSHUF_B_XVMAP(as, instr); break;
        case OP12_XVSTELM_D: ok = 1; GEN_XVSTELM_D_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 21) {
        case OP11_XVLDREPL_H: ok = 1; GEN_XVLDREPL_H_XVMAP(as, instr); break;
        case OP11_XVSTELM_W: ok = 1; GEN_XVSTELM_W_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 22) {
        case OP10_XVLD: ok = 1; GEN_XVLD_XVMAP(as, instr); break;
        case OP10_XVLDREPL_B: ok = 1; GEN_XVLDREPL_B_XVMAP(as, instr); break;
        case OP10_XVST: ok = 1; GEN_XVST_XVMAP(as, instr); break;
        case OP10_XVSTELM_H: ok = 1; GEN_XVSTELM_H_XVMAP(as, instr); break;
        default: break;
    }
    switch (instr >> 23) {
        case OP9_XVSTELM_B: ok = 1; GEN_XVSTELM_B_XVMAP(as, instr); break;
        default: break;
    }
    return ok;
}
