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

#ifndef LASX_INTERPRET_H
#define LASX_INTERPRET_H

#include "lagoon.h"

/* Interpretation mode bitmask flags */
#define LASX_MODE_BLOCK   (1 << 0)
#define LASX_MODE_LOOP    (1 << 1)
#define LASX_MODE_USEDEF  (1 << 2)

extern int lasx_interpret_mode;
extern int lasx_interpret_frag_opt;

extern void *interpreter;
extern void *interpreter_entry;
extern void *interpreter_entry_ptr;
extern void *interpreter_entry_highwater;
extern void (*__mov_gr_scr0)(uint64_t gr);
extern void (*__mov_gr_scr1)(uint64_t gr);
extern void (*__mov_gr_scr2)(uint64_t gr);
extern void (*__mov_gr_scr3)(uint64_t gr);
extern uint64_t (*__mov_scr0_gr)(void);
extern uint64_t (*__mov_scr1_gr)(void);
extern uint64_t (*__mov_scr2_gr)(void);
extern uint64_t (*__mov_scr3_gr)(void);

#define TD_OFF_DATA(id, n)  \
    (offsetof(thread_data_t, data[id][n]))
    //(offsetof(thread_data_t, data[0][0]) + (id) * 32 + (n) * 8)
#define TD_OFF_DATA_W(id, n)  \
    (offsetof(thread_data_t, data_w[id][n]))
#define TD_OFF_DATA_H(id, n)  \
    (offsetof(thread_data_t, data_h[id][n]))
#define TD_OFF_DATA_B(id, n)  \
    (offsetof(thread_data_t, data_b[id][n]))

#define TD_OFF_GPR(id)  \
    (offsetof(thread_data_t, gpr[id]))

void lasx_init_interpret(void);
void lasx_interpret_prepare_thread(void);
void lasx_interpret_prepare_context(ucontext_t *uc);
void interpret_lock(void);
void interpret_unlock(void);
bool lasx_emu_create_interpret(ucontext_t *uc, unsigned int instr);
bool lasx_emu_create_interpret_block(ucontext_t *uc);
bool lasx_emu_create_interpret_fragment(ucontext_t *uc);

int lasx_interpret_inst_gen_xvmap(void *as, unsigned int instr);
int lasx_interpret_inst_gen(void *as, unsigned int instr, int with_prologue);
int detect_xv_batch(unsigned int *p);

#endif
