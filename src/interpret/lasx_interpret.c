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
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <unistd.h>
#include <ucontext.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <sys/mman.h>
#include <string.h>
#include <assert.h>
#include "thread_data.h"
#include "lasx_emu_private.h"
#include "lasx_interpret.h"
#include "lasx_interpret_opt.h"
#include "lasx_interpret_opt_reg.h"
#include "lasx_patterns_xvmap.h"
#include "lasx_interpret_opt_gen_usedef.h"
#include "lasx_profile.h"

extern int lasx_interpret_frag_opt;

//static pthread_mutex_t imutex = PTHREAD_MUTEX_INITIALIZER;
static int asdasd;
void interpret_lock(void)
{
    int *mem = &asdasd;
    __asm__ volatile(
            "1:\n\r"
            "ori $t1, $zero, 0x1\n\r"
            "amswap_db.w $t0, $t1, %0\n\r"
            "bnez $t0, 1b\n\r"
            :
            : "r"(mem)
            : "memory", "$t0", "$t1"); 
}
void interpret_unlock(void)
{
     __asm__ volatile("dbar 0\n\r":::);
    asdasd = 0;
     __asm__ volatile("dbar 0\n\r":::);
}


void *interpreter;
void *interpreter_entry;
void *interpreter_entry_ptr;
void *interpreter_entry_highwater;
void *interpreter_data;
void (*__mov_gr_scr0)(uint64_t gr);
void (*__mov_gr_scr1)(uint64_t gr);
void (*__mov_gr_scr2)(uint64_t gr);
void (*__mov_gr_scr3)(uint64_t gr);
uint64_t (*__mov_scr0_gr)(void);
uint64_t (*__mov_scr1_gr)(void);
uint64_t (*__mov_scr2_gr)(void);
uint64_t (*__mov_scr3_gr)(void);



bool lasx_emu_create_interpret(ucontext_t *uc, unsigned int instr)
{
	if (interpreter == NULL) return false;

    //pthread_mutex_lock(&imutex);
    interpret_lock();

    uintptr_t pc = UC_PC(uc);
    thread_data_t *td = thread_data_get();

    UC_SET_SCR(uc, 1, (uint64_t)interpreter_entry);
    UC_SET_SCR(uc, 2, (uint64_t)td);

    int n = 0;
    void *entry = interpreter_entry_ptr;
    void *jit_start = entry;

    n = profile_emit(NULL, entry, pc, PROFILE_SINGLE);
    entry = (void *)((uintptr_t)entry + (n * 4));

    lagoon_assembler_t as;
    la_init_assembler(&as, entry, 0x1000);

    if (lasx_interpret_inst_gen(&as, instr, 1)) {
        n = la_get_inst_count(&as);
    }
    if (n) {
        int off = (int64_t)jit_start - (int64_t)interpreter_entry;
        off >>= 2;
        uint32_t jiscr1_off = 0x48000300 | (((off>>16)&0x1f) << 0) | ((off&0xffff) << 10);

        uintptr_t page = pc & ~(0x4000-1);
        mprotect((void*)page, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC);

        *(uint32_t*)pc = jiscr1_off;

        __asm__ volatile("ibar 0\n\r":::);
        //mprotect((void*)page, 0x4000, PROT_READ | PROT_EXEC);
        interpreter_entry_ptr = (void *)((uintptr_t)entry + n * 4);
    }

    //pthread_mutex_unlock(&imutex);
    interpret_unlock();
    
    return (n != 0);
}




extern int lasx_interpret_mode;
extern uint64_t tp_offset;

bool lasx_emu_create_interpret_block(ucontext_t *uc)
{
    if (interpreter == NULL || !lasx_interpret_mode) return false;

    interpret_lock();

    unsigned long pc = UC_PC(uc);
    unsigned int *p = (void*)pc;

    void *entry = interpreter_entry_ptr;
    lagoon_assembler_t as;
    la_init_assembler(&as, entry, 0x1000);
    la_pcaddi(&as, LA_ZERO, 0);

    /* ===== Phase 1: Loop detection ===== */
    if (lasx_interpret_mode & LASX_MODE_LOOP) {
        if (try_optimize_loop(&as, p, entry)) {
            JITLOG("=== loop optimized PC=%016lx entry=%p ===\n", pc, entry);
            interpreter_entry_ptr = (void*)((uintptr_t)entry
                + la_get_inst_count(&as) * 4);
            interpret_unlock();
            return true;
        }
    }

    la_addi_d(&as, LA_TP, LA_TP, tp_offset);

    /* ===== Phase 2: Translation ===== */
    int block_pidx = profile_emit(&as, entry, pc, PROFILE_NONE) ? profile_current_pidx() : 0;

    int n = 0;
    int batch_len = detect_xv_batch(p);
    int usedef_succeeded = 0;

    if ((lasx_interpret_mode & LASX_MODE_USEDEF) && batch_len > 0) {
        block_usedef_t ud;
        int temp_vr_map[batch_len][32];
        uint32_t temp_vr_all_bm;
        memset(temp_vr_map, -1, sizeof(temp_vr_map));
        if (build_block_usedef(p, batch_len, &ud, temp_vr_map, &temp_vr_all_bm) == 0) {
            uint8_t *save_cursor = as.cursor;
            int all_ok = 1;
            /* Copy first instruction's mapping to ud.temp_vr for gen check */
            for (int x = 0; x < 32; x++)
                ud.temp_vr[x] = temp_vr_map[0][x];
            for (int i = 0; i < batch_len; i++) {
                /* Update per-instruction mapping before calling gen */
                for (int x = 0; x < 32; x++)
                    ud.temp_vr[x] = temp_vr_map[i][x];
                if (!lasx_interpret_inst_gen_usedef(&as, p[i], ud.temp_vr)) {
                    all_ok = 0; break;
                }
            }
            if (all_ok) {
                as.cursor = save_cursor;
                /* Update ud.temp_vr to first instruction for prologue */
                for (int x = 0; x < 32; x++)
                    ud.temp_vr[x] = temp_vr_map[0][x];
                emit_block_usedef_prologue(&as, &ud);

                int prev_map[32];
                for (int x = 0; x < 32; x++)
                    prev_map[x] = temp_vr_map[0][x];

                for (int i = 0; i < batch_len; i++) {
                    /* Mini prologue/epilogue between instructions */
                    if (i > 0)
                        emit_mini_prologue_epilogue(&as, i, prev_map,
                            temp_vr_map[i]);

                    for (int x = 0; x < 32; x++)
                        ud.temp_vr[x] = temp_vr_map[i][x];
                    lasx_interpret_inst_gen_usedef(&as, p[i], ud.temp_vr);

                    for (int x = 0; x < 32; x++)
                        prev_map[x] = temp_vr_map[i][x];
                }

                n = batch_len;
                usedef_succeeded = 1;
                emit_block_usedef_epilogue(&as, &ud, temp_vr_map[batch_len - 1],
                                            temp_vr_all_bm);
            } else {
                as.cursor = save_cursor;
            }
        }
    }

    if (n == 0 && batch_len > 0) {
        for (int i = 0; i < batch_len; i++) {
            if (!lasx_interpret_inst_gen(&as, p[i], 0))
                break;
            n++;
        }
    }

    /* ===== Phase 3: Finalize ===== */
    la_addi_d(&as, LA_TP, LA_TP, -tp_offset);
    if (n) {
        la_jiscr0(&as, n * 4);
        profile_set(block_pidx, usedef_succeeded ? PROFILE_USEDEF : PROFILE_BLOCK, n);

        JITLOG("=== batch %d instrs mode=%s PC=%016lx ===\n",
               n, usedef_succeeded ? "usedef" : "block", pc);
#ifdef LASX_PROFILE
        for (int i = 0; i < n; i++) {
            lagoon_insn_t insn;
            char buf[128];
            la_disasm_one(p[i], &insn);
            la_insn_to_str(&insn, buf, sizeof(buf));
            JITLOG("  %016lx: %08x  %s\n", pc + i * 4, p[i], buf);
        }
#endif

        interpreter_entry_ptr = (void*)((uintptr_t)entry
            + la_get_inst_count(&as) * 4);

        int off = (int64_t)entry - (int64_t)interpreter_entry;
        off >>= 2;
        uint32_t jiscr1_offs = 0x48000300
            | (((off>>16)&0x1f) << 0) | ((off&0xffff) << 10);

        uintptr_t page = pc & ~(0x4000-1);
        mprotect((void*)page, 0x4000, PROT_READ | PROT_WRITE | PROT_EXEC);
        *(uint32_t*)pc = jiscr1_offs;
        __asm__ volatile("ibar 0\n\r":::);
    }

    interpret_unlock();
    return (n != 0);
}

/* Count consecutive XV instructions starting at p.
   Returns number of XV instructions (stops at first non-XV or 128). */

static int is_xv_instr(unsigned int instr)
{
    unsigned int top8 = instr >> 24;
    if (top8 >= 0x74 && top8 <= 0x77) return 1;
    if (top8 == 0x0a) return 1;
    if (top8 == 0x0c) return (instr >> 23) & 1;
    if (top8 == 0x0d) {
        unsigned int lo = (instr >> 20) & 7;
        return (lo == 2 || lo == 6);
    }
    if (top8 == 0x2c) return (instr >> 23) & 1;
    if (top8 == 0x32 || top8 == 0x33) return 1;
    if (top8 == 0x38) return (instr >> 19) & 1;
    return 0;
}

int detect_xv_batch(unsigned int *p)
{
    int batch_len = 0;
    while (batch_len < 128) {
        if (!is_xv_instr(p[batch_len]))
            break;
        batch_len++;
    }
    return batch_len;
}

#define update_entry(size) do { \
    interpreter_entry_ptr = interpreter_entry   \
        = (void *)((uintptr_t)interpreter_entry + (size));    \
} while (0);


static void __lasx_init_interpret(uint32_t *code)
{
    interpreter_entry = (void *)((uintptr_t)code);
    interpreter_entry_ptr = interpreter_entry;

    lagoon_assembler_t as;
    la_init_assembler(&as, (uint8_t*)code, 0x4000);

    la_movgr2scr(&as, LA_SCR0, LA_A0); la_ret(&as); __mov_gr_scr0 = (void *)((uintptr_t)code + 0x00);
    la_movgr2scr(&as, LA_SCR1, LA_A0); la_ret(&as); __mov_gr_scr1 = (void *)((uintptr_t)code + 0x08);
    la_movgr2scr(&as, LA_SCR2, LA_A0); la_ret(&as); __mov_gr_scr2 = (void *)((uintptr_t)code + 0x10);
    la_movgr2scr(&as, LA_SCR3, LA_A0); la_ret(&as); __mov_gr_scr3 = (void *)((uintptr_t)code + 0x18);
    la_movscr2gr(&as, LA_A0, LA_SCR0); la_ret(&as); __mov_scr0_gr = (void *)((uintptr_t)code + 0x20);
    la_movscr2gr(&as, LA_A0, LA_SCR1); la_ret(&as); __mov_scr1_gr = (void *)((uintptr_t)code + 0x28);
    la_movscr2gr(&as, LA_A0, LA_SCR2); la_ret(&as); __mov_scr2_gr = (void *)((uintptr_t)code + 0x30);
    la_movscr2gr(&as, LA_A0, LA_SCR3); la_ret(&as); __mov_scr3_gr = (void *)((uintptr_t)code + 0x38);
    update_entry(16 << 2);
}

void lasx_init_interpret(void)
{
    void *codes = mmap((void *)0x60000000, 0x4000000,
                       PROT_READ | PROT_WRITE | PROT_EXEC,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    void *datas = mmap((void *)0x70000000, 0x4000,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    if (codes != MAP_FAILED && datas != MAP_FAILED) {
      interpreter = codes;
      interpreter_entry_highwater = interpreter + 0x400000;
      __lasx_init_interpret(codes);
      tdlog("%16lx : interpret\n", (uint64_t)interpreter);
      tdlog("%16lx : interpret entry\n", (uint64_t)interpreter_entry);
      tdlog("%16lx : interpret highwater\n", (uint64_t)interpreter_entry_highwater);

      interpreter_data = datas;
      tdlog("%16lx : interpret data\n", (uint64_t)interpreter_data);
      *(uint64_t*)interpreter_data = (uint64_t)interpreter_data;
      //   lock : 1  -> !0
      // unlock : !0 -> 1

#ifdef LASX_PROFILE
      *(uint64_t*)(interpreter_data + 8) = (uint64_t)profile_get_count_array();
      if (lasx_profile_mode & LASX_PROFILE_THREAD) {
          profile_start_periodic();
          atexit(profile_stop_periodic);
      }
#endif
    
      /*
       * SCR1 is a per-CPU register. Initialize it on every online CPU so
       * that future threads created on any CPU inherit the correct
       * interpreter_entry via OS context. This is a one-time hack because
       * we are loaded via LD_PRELOAD and don't know how many threads the
       * application will create.
       */
      {
          cpu_set_t saved_mask;
          sched_getaffinity(0, sizeof(saved_mask), &saved_mask);
          int ncpu = get_nprocs();
          for (int cpu = 0; cpu < ncpu; cpu++) {
              cpu_set_t mask;
              CPU_ZERO(&mask);
              CPU_SET(cpu, &mask);
              sched_setaffinity(0, sizeof(mask), &mask);
              usleep(1);
              __mov_gr_scr1((uint64_t)interpreter_entry);
          }
          sched_setaffinity(0, sizeof(saved_mask), &saved_mask);
      }
    } else {
      tdlog("interpret mmap fail\n");
    }
}
