/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Freestanding (-nostdlib) LASX probe (`test_lasx_raw_ri`).
 *
 * Starts directly at _start with CSR.EUEN == 0 (no ld.so / glibc startup):
 *   1. Installs a raw rt_sigaction(SIGILL) handler that inspects ucontext->__pc
 *      and advances __pc += 4 on SIGILL so execution can report exact trap info.
 *   2. Executes explicit 256-bit LASX instructions (`xvld`, `xvadd.w`, `xvst`)
 *      starting at `lasx_first_insn_pc`.
 *   3. Modes:
 *      - `./test_lasx_raw_ri -s`: expects 4 SIGILLs (`enable_runtime=0`).
 *      - `./test_lasx_raw_ri`: expects 0 SIGILLs and exact 256-bit vector output
 *        (`enable_runtime=1` with `lasx2lsx.ko` loaded, or native LASX CPU).
 */
#include <stddef.h>
#include <stdint.h>
#include <ucontext.h>

#define NR_WRITE 64
#define NR_EXIT 93
#define NR_RT_SIGACTION 134

#define SIGILL_NUM 4
#define SA_SIGINFO_FLG 0x00000004UL

struct kernel_sigaction {
    void (*k_sa_handler)(int, void*, void*);
    unsigned long sa_flags;
    unsigned long sa_mask;
};

struct mini_siginfo {
    int si_signo;
    int si_errno;
    int si_code;
    int _pad;
    void* si_addr;
};

static volatile int g_sigill_count = 0;
static volatile int g_first_si_code = 0;
static volatile uintptr_t g_first_fault_pc = 0;

static long sys_write(int fd, const void* buf, size_t len)
{
    register long a0 __asm__("$a0") = fd;
    register long a1 __asm__("$a1") = (long)buf;
    register long a2 __asm__("$a2") = (long)len;
    register long a7 __asm__("$a7") = NR_WRITE;
    __asm__ volatile("syscall 0" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8");
    return a0;
}

static long sys_rt_sigaction(int sig, const struct kernel_sigaction* act)
{
    register long a0 __asm__("$a0") = sig;
    register long a1 __asm__("$a1") = (long)act;
    register long a2 __asm__("$a2") = 0;
    register long a3 __asm__("$a3") = 8;
    register long a7 __asm__("$a7") = NR_RT_SIGACTION;
    __asm__ volatile("syscall 0" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8");
    return a0;
}

static void print_str(const char* s)
{
    size_t n = 0;
    while (s[n])
        n++;
    sys_write(1, s, n);
}

static void print_hex(uintptr_t v)
{
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        int nib = (v >> ((15 - i) * 4)) & 0xf;
        buf[2 + i] = (nib < 10) ? ('0' + nib) : ('a' + nib - 10);
    }
    sys_write(1, buf, 18);
}

static void print_dec(int v)
{
    char buf[16];
    int pos = 15;
    if (v == 0) {
        sys_write(1, "0", 1);
        return;
    }
    int neg = (v < 0);
    unsigned int u = neg ? (unsigned int)(-v) : (unsigned int)v;
    while (u > 0) {
        buf[pos--] = '0' + (u % 10);
        u /= 10;
    }
    if (neg)
        buf[pos--] = '-';
    sys_write(1, &buf[pos + 1], 15 - pos);
}

static void sigill_handler(int sig, void* info_p, void* uctx_p)
{
    (void)sig;
    struct mini_siginfo* si = (struct mini_siginfo*)info_p;
    ucontext_t* uc = (ucontext_t*)uctx_p;

    if (g_sigill_count == 0) {
        if (si)
            g_first_si_code = si->si_code;
        g_first_fault_pc = uc->uc_mcontext.__pc;
    }
    g_sigill_count++;
    uc->uc_mcontext.__pc += 4;
}

extern const uint32_t lasx_first_insn_pc;

static int32_t g_vec_a[8] __attribute__((aligned(32))) = {1, 2, 3, 4, 5, 6, 7, 8};
static int32_t g_vec_b[8] __attribute__((aligned(32))) = {10, 20, 30, 40, 50, 60, 70, 80};
static int32_t g_vec_out[8] __attribute__((aligned(32)));

int probe_main(int argc, char** argv)
{
    int expect_sigill = 0;
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 's')
        expect_sigill = 1;

    for (int i = 0; i < 8; i++)
        *(volatile int32_t*)&g_vec_out[i] = 0;

    struct kernel_sigaction ksa;
    ksa.k_sa_handler = sigill_handler;
    ksa.sa_flags = SA_SIGINFO_FLG;
    ksa.sa_mask = 0;
    if (sys_rt_sigaction(SIGILL_NUM, &ksa) != 0) {
        print_str("[FAIL] sys_rt_sigaction failed\n");
        return 2;
    }

    __asm__ volatile(".globl lasx_first_insn_pc\n\t"
                     "lasx_first_insn_pc:\n\t"
                     "xvld    $xr0, %0, 0\n\t"
                     "xvld    $xr1, %1, 0\n\t"
                     "xvadd.w $xr2, $xr0, $xr1\n\t"
                     "xvst    $xr2, %2, 0\n\t"
                     :
                     : "r"(g_vec_a), "r"(g_vec_b), "r"(g_vec_out)
                     : "memory", "$xr0", "$xr1", "$xr2");

    uintptr_t exp_pc = (uintptr_t)&lasx_first_insn_pc;

    if (expect_sigill) {
        if (g_sigill_count == 4 && g_first_fault_pc == exp_pc) {
            print_str("[PASS] test_lasx_raw_ri (-s, enable_runtime=0): caught ");
            print_dec(g_sigill_count);
            print_str(" SIGILLs (si_code=");
            print_dec(g_first_si_code);
            print_str("), first fault PC=");
            print_hex(g_first_fault_pc);
            print_str(" == lasx_first_insn_pc (");
            print_hex(exp_pc);
            print_str(")\n");
            return 0;
        }
        print_str("[FAIL] Expected 4 SIGILLs at lasx_first_insn_pc, got count=");
        print_dec(g_sigill_count);
        print_str(" pc=");
        print_hex(g_first_fault_pc);
        print_str("\n");
        return 1;
    }

    if (g_sigill_count != 0) {
        print_str("[FAIL] Unexpected SIGILL count=");
        print_dec(g_sigill_count);
        print_str("\n");
        return 1;
    }
    for (int i = 0; i < 8; i++) {
        if (g_vec_out[i] != g_vec_a[i] + g_vec_b[i]) {
            print_str("[FAIL] Lane mismatch in test_lasx_raw_ri\n");
            return 1;
        }
    }
    print_str("[PASS] test_lasx_raw_ri executed 256-bit LASX with 0 user SIGILLs: out[0..7]=[");
    for (int i = 0; i < 8; i++) {
        if (i)
            print_str(",");
        print_dec(g_vec_out[i]);
    }
    print_str("]\n");
    return 0;
}

__asm__(".section .text\n\t"
        ".globl _start\n\t"
        ".type _start, @function\n"
        "_start:\n\t"
        "ld.d   $a0, $sp, 0\n\t"
        "addi.d $a1, $sp, 8\n\t"
        "bstrins.d $sp, $zero, 3, 0\n\t"
        "bl     probe_main\n\t"
        "li.w   $a7, 93\n\t"
        "syscall 0\n\t");
