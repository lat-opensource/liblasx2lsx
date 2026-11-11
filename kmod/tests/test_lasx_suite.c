/* SPDX-License-Identifier: GPL-2.0 OR MIT */
#define _GNU_SOURCE
#include <errno.h>
#include <lasxintrin.h>
#include <math.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../lasx_kproto.h"

extern char** environ;

static long read_kmod_stat(const char* name)
{
    char path[256];
    snprintf(path, sizeof(path), "/sys/module/lasx2lsx/parameters/%s", name);
    FILE* f = fopen(path, "r");
    if (!f)
        return -1;
    long val = -1;
    if (fscanf(f, "%ld", &val) != 1)
        val = -1;
    fclose(f);
    return val;
}

static int read_kmod_bool(const char* name)
{
    char path[256];
    char ch = 0;
    snprintf(path, sizeof(path), "/sys/module/lasx2lsx/parameters/%s", name);
    FILE* f = fopen(path, "r");
    if (!f)
        return -1;
    if (fscanf(f, " %c", &ch) != 1)
        ch = 0;
    fclose(f);
    return (ch == 'Y' || ch == 'y' || ch == '1') ? 1 : 0;
}

/* Prevent inlining so we can inspect the exact entry instruction before/after */
__attribute__((noinline, noclone)) static void lasx_hot_vec_op(const int32_t* a, const int32_t* b, int32_t* out)
{
    __m256i va = __lasx_xvld(a, 0);
    __m256i vb = __lasx_xvld(b, 0);
    __m256i vc = __lasx_xvadd_w(va, vb);
    __m256i vd = __lasx_xvmul_w(vc, vb);
    __lasx_xvst(vd, out, 0);
}

__attribute__((noinline, noclone)) static void lasx_isa_variety(const int64_t* in_i64, int64_t* out_perm, const float* fa, const float* fb, const float* fc, float* fout, const double* da,
                                                                double* dout)
{
    /* Cross-128-bit-lane permute: swap low 128-bit and high 128-bit lanes */
    __m256i v = __lasx_xvld(in_i64, 0);
    __m256i swapped = __lasx_xvpermi_q(v, v, 0x01);
    swapped = __lasx_xvinsgr2vr_d(swapped, 0x1122334455667788LL, 3);
    __lasx_xvst(swapped, out_perm, 0);

    /* 256-bit single-precision FMA: fa * fb + fc */
    __m256 vfa = (__m256)__lasx_xvld(fa, 0);
    __m256 vfb = (__m256)__lasx_xvld(fb, 0);
    __m256 vfc = (__m256)__lasx_xvld(fc, 0);
    __m256 vfm = __lasx_xvfmadd_s(vfa, vfb, vfc);
    __lasx_xvst(vfm, fout, 0);

    /* 256-bit double-precision sqrt */
    __m256d vda = (__m256d)__lasx_xvld(da, 0);
    __m256d vds = __lasx_xvfsqrt_d(vda);
    __lasx_xvst(vds, dout, 0);
}

__attribute__((noinline, noclone)) static void lasx_fresh_after_sigaction(const int32_t* a, int32_t* out)
{
    __m256i va = __lasx_xvld(a, 0);
    __m256i vr = __lasx_xvslli_w(va, 2);
    vr = __lasx_xvsub_w(vr, va); /* a * 3 */
    __lasx_xvst(vr, out, 0);
}

static sigjmp_buf g_ill_jmp;
static volatile sig_atomic_t g_user_sigill_hits = 0;
static volatile void* g_fault_addr = NULL;

static void custom_sigill_handler(int sig, siginfo_t* info, void* ucontext)
{
    (void)sig;
    (void)ucontext;
    g_user_sigill_hits++;
    g_fault_addr = info ? info->si_addr : NULL;
    siglongjmp(g_ill_jmp, 1);
}

/* Multi-thread barrier & unpatched target function */
#define NUM_PRE_THREADS 4
#define NUM_POST_THREADS 2
#define MT_ITERS 50000
#define CHURN_THREADS 16

static pthread_barrier_t g_pre_barrier;
static pthread_barrier_t g_post_patch_barrier;

__attribute__((noinline, noclone)) static void mt_lasx_kernel(const int32_t* a, const int32_t* b, int32_t* out)
{
    __m256i va = __lasx_xvld(a, 0);
    __m256i vb = __lasx_xvld(b, 0);
    /* Exercise high-half 256-bit lanes across multiple XV ops to stress $scr2 */
    __m256i s1 = __lasx_xvadd_w(va, vb);
    __m256i s2 = __lasx_xvxor_v(s1, vb);
    __m256i s3 = __lasx_xvpermi_d(s2, 0x1b); /* reverse all 4x64b lanes [3,2,1,0] */
    __lasx_xvst(s3, out, 0);
}

struct mt_arg {
    int tid_idx;
    int is_pre;
    int ok;
};

static void* mt_worker(void* argp)
{
    struct mt_arg* arg = (struct mt_arg*)argp;
    int32_t a[8] __attribute__((aligned(32)));
    int32_t b[8] __attribute__((aligned(32)));
    int32_t out[8] __attribute__((aligned(32)));
    int32_t exp[8];

    for (int i = 0; i < 8; i++) {
        a[i] = (arg->tid_idx + 1) * 1000 + i * 17;
        b[i] = (arg->tid_idx + 3) * 31 + i;
    }
    int32_t tmp32[8];
    int64_t tmp64[4];
    for (int i = 0; i < 8; i++)
        tmp32[i] = (a[i] + b[i]) ^ b[i];
    memcpy(tmp64, tmp32, sizeof(tmp64));
    int64_t rev64[4] = {tmp64[3], tmp64[2], tmp64[1], tmp64[0]};
    memcpy(exp, rev64, sizeof(exp));

    if (arg->is_pre) {
        pthread_barrier_wait(&g_pre_barrier);
        if (arg->tid_idx == 0) {
            /* Thread 0 triggers initial LASX trap + jiscr1 JIT patch */
            mt_lasx_kernel(a, b, out);
            pthread_barrier_wait(&g_post_patch_barrier);
        } else {
            /* Threads 1..3 wait until mt_lasx_kernel is ALREADY jiscr1-patched */
            pthread_barrier_wait(&g_post_patch_barrier);
        }
    }

    for (int it = 0; it < MT_ITERS; it++) {
        mt_lasx_kernel(a, b, out);
        if (memcmp(out, exp, sizeof(exp)) != 0) {
            arg->ok = 0;
            return NULL;
        }
        /* Mutate lane values each iteration so stale $scr2 data is caught */
        a[it & 7] += 3;
        for (int i = 0; i < 8; i++)
            tmp32[i] = (a[i] + b[i]) ^ b[i];
        memcpy(tmp64, tmp32, sizeof(tmp64));
        rev64[0] = tmp64[3];
        rev64[1] = tmp64[2];
        rev64[2] = tmp64[1];
        rev64[3] = tmp64[0];
        memcpy(exp, rev64, sizeof(exp));
    }

    arg->ok = 1;
    return NULL;
}

static void* churn_worker(void* argp)
{
    int idx = *(int*)argp;
    int32_t a[8] __attribute__((aligned(32)));
    int32_t b[8] __attribute__((aligned(32)));
    int32_t out[8] __attribute__((aligned(32)));
    int32_t tmp32[8];
    int64_t tmp64[4];
    int64_t rev64[4];

    for (int i = 0; i < 8; i++) {
        a[i] = (idx + 11) * 257 + i * 13;
        b[i] = (idx + 5) * 19 + i;
        tmp32[i] = (a[i] + b[i]) ^ b[i];
    }
    memcpy(tmp64, tmp32, sizeof(tmp64));
    rev64[0] = tmp64[3];
    rev64[1] = tmp64[2];
    rev64[2] = tmp64[1];
    rev64[3] = tmp64[0];

    for (int it = 0; it < 256; it++)
        mt_lasx_kernel(a, b, out);

    if (memcmp(out, rev64, sizeof(out)) != 0)
        return (void*)1;
    return NULL;
}

static long invoke_wx_patch_hcall(void* target_pc, uint32_t new_insn)
{
    register unsigned long r_a0 __asm__("$a0") = (unsigned long)target_pc;
    register unsigned long r_a1 __asm__("$a1") = (unsigned long)new_insn;
    register unsigned long r_a7 __asm__("$a7") = LASX_HCALL_MAGIC;
    __asm__ volatile(".word 0x00000000\n\tibar 0" : "+r"(r_a0) : "r"(r_a1), "r"(r_a7) : "memory");
    return (long)r_a0;
}

static int run_core_checks(int is_clean_env_child)
{
    printf("=== [%s] LD_PRELOAD=%s, env_vars_present=%d ===\n", is_clean_env_child ? "clean-env-child" : "main-suite", getenv("LD_PRELOAD") ? getenv("LD_PRELOAD") : "(null)",
           (environ && environ[0]) ? 1 : 0);

    /* ---- Test 1: On-demand trap interception + 1,000,000 iter jiscr1 JIT ---- */
    int32_t a[8] __attribute__((aligned(32))) = {10, -20, 30, -40, 50, 60, -70, 80};
    int32_t b[8] __attribute__((aligned(32))) = {3, 4, -5, 6, 7, -8, 9, 10};
    int32_t out[8] __attribute__((aligned(32))) = {0};

    uint32_t* fn_pc = (uint32_t*)(uintptr_t)lasx_hot_vec_op;
    int xv_idx = -1;
    for (int i = 0; i < 16; i++) {
        uint32_t op = fn_pc[i];
        if ((op >> 24) == 0x2c || ((op >> 24) >= 0x74 && (op >> 24) <= 0x77)) {
            xv_idx = i;
            break;
        }
    }
    if (xv_idx < 0) {
        fprintf(stderr, "[FAIL] Could not locate XV instruction in lasx_hot_vec_op\n");
        return 1;
    }
    uint32_t before_insn = fn_pc[xv_idx];
    int is_sim_3a6000 = read_kmod_bool("sim_3a6000");
    long intercept_before = read_kmod_stat("stat_lasx_intercepted");
    long upcalls_before = read_kmod_stat("stat_rt_upcalls");

    const int HOT_ITERS = is_clean_env_child ? 100000 : 1000000;
    for (int i = 0; i < HOT_ITERS; i++)
        lasx_hot_vec_op(a, b, out);

    long intercept_after = read_kmod_stat("stat_lasx_intercepted");
    long upcalls_after = read_kmod_stat("stat_rt_upcalls");
    uint32_t after_insn = fn_pc[xv_idx];

    for (int i = 0; i < 8; i++) {
        int32_t exp = (a[i] + b[i]) * b[i];
        if (out[i] != exp) {
            fprintf(stderr, "[FAIL] Test 1 lane %d mismatch: got %d, expected %d\n", i, out[i], exp);
            return 1;
        }
    }

    int patched_jiscr1 = ((after_insn & 0xfc0003e0U) == 0x48000300U);
    long delta_intercept = intercept_after - intercept_before;
    long delta_upcalls = upcalls_after - upcalls_before;

    printf("[PASS] Test 1 (on-demand trap + JIT): %d iters OK | insn 0x%08x -> 0x%08x (jiscr1=%d) | sim_traps=%ld, upcalls=%ld\n", HOT_ITERS, before_insn, after_insn, patched_jiscr1, delta_intercept,
           delta_upcalls);

    if (!patched_jiscr1 || delta_upcalls < 1 || delta_upcalls > 50 || (is_sim_3a6000 == 1 && delta_intercept < 1)) {
        fprintf(stderr, "[FAIL] Expected jiscr1 patch and O(1) upcalls (got jiscr1=%d, sim_traps=%ld, upcalls=%ld)\n", patched_jiscr1, delta_intercept, delta_upcalls);
        return 1;
    }

    /* ---- Test 2: Broad 256-bit LASX ISA coverage ---- */
    int64_t in_i64[4] __attribute__((aligned(32))) = {0x0102030405060708LL, 0x1112131415161718LL, 0x2122232425262728LL, 0x3132333435363738LL};
    int64_t out_perm[4] __attribute__((aligned(32))) = {0};
    float fa[8] __attribute__((aligned(32))) = {1.5f, -2.0f, 3.25f, 4.0f, -5.5f, 6.0f, 7.5f, -8.0f};
    float fb[8] __attribute__((aligned(32))) = {2.0f, 3.0f, 4.0f, -1.5f, 2.0f, 0.5f, -2.0f, 3.0f};
    float fc[8] __attribute__((aligned(32))) = {0.5f, 1.0f, -2.0f, 10.0f, 1.0f, 2.0f, 3.0f, 4.0f};
    float fout[8] __attribute__((aligned(32))) = {0};
    double da[4] __attribute__((aligned(32))) = {4.0, 9.0, 25.0, 144.0};
    double dout[4] __attribute__((aligned(32))) = {0};

    lasx_isa_variety(in_i64, out_perm, fa, fb, fc, fout, da, dout);

    if (out_perm[0] != in_i64[2] || out_perm[1] != in_i64[3] || out_perm[2] != in_i64[0] || out_perm[3] != 0x1122334455667788LL) {
        fprintf(stderr, "[FAIL] Test 2 permute/insert mismatch: %lx %lx %lx %lx\n", (unsigned long)out_perm[0], (unsigned long)out_perm[1], (unsigned long)out_perm[2], (unsigned long)out_perm[3]);
        return 1;
    }
    for (int i = 0; i < 8; i++) {
        float exp = fa[i] * fb[i] + fc[i];
        if (fabsf(fout[i] - exp) > 1e-5f) {
            fprintf(stderr, "[FAIL] Test 2 xvfmadd.s lane %d: got %f, exp %f\n", i, fout[i], exp);
            return 1;
        }
    }
    double exp_sqrt[4] = {2.0, 3.0, 5.0, 12.0};
    for (int i = 0; i < 4; i++) {
        if (fabs(dout[i] - exp_sqrt[i]) > 1e-12) {
            fprintf(stderr, "[FAIL] Test 2 xvfsqrt.d lane %d: got %f, exp %f\n", i, dout[i], exp_sqrt[i]);
            return 1;
        }
    }
    printf("[PASS] Test 2 (256-bit cross-lane permute + FMA + FP64 sqrt): exact match\n");

    if (is_clean_env_child)
        return 0;

    /* ---- Test 3: User sigaction(SIGILL) coexistence & real SIGILL passthrough ---- */
    struct sigaction sa, old_check;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = custom_sigill_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGILL, &sa, NULL) != 0) {
        perror("[FAIL] sigaction");
        return 1;
    }

    int32_t out_tri[8] __attribute__((aligned(32))) = {0};
    lasx_fresh_after_sigaction(a, out_tri);
    for (int i = 0; i < 8; i++) {
        if (out_tri[i] != a[i] * 3) {
            fprintf(stderr, "[FAIL] Test 3 LASX result mismatch on lane %d\n", i);
            return 1;
        }
    }
    if (g_user_sigill_hits != 0) {
        fprintf(stderr, "[FAIL] Test 3 user SIGILL handler falsely triggered on LASX (%d)\n", (int)g_user_sigill_hits);
        return 1;
    }

    long sigill_pt_before = read_kmod_stat("stat_sigill_passthrough");
    if (sigsetjmp(g_ill_jmp, 1) == 0) {
        __asm__ volatile(".word 0x00000001" ::: "memory");
        fprintf(stderr, "[FAIL] Test 3 non-LASX illegal insn did not raise SIGILL!\n");
        return 1;
    }
    long sigill_pt_after = read_kmod_stat("stat_sigill_passthrough");

    sigaction(SIGILL, NULL, &old_check);
    signal(SIGILL, SIG_DFL);

    if (g_user_sigill_hits != 1 || old_check.sa_sigaction != custom_sigill_handler || sigill_pt_after <= sigill_pt_before) {
        fprintf(stderr, "[FAIL] Test 3 SIGILL passthrough check failed (hits=%d, passthrough=%ld->%ld)\n", (int)g_user_sigill_hits, sigill_pt_before, sigill_pt_after);
        return 1;
    }
    printf("[PASS] Test 3 (sigaction(SIGILL) coexistence): LASX silent, real illegal insn delivered to user handler (addr=%p, passthrough+%ld)\n", g_fault_addr, sigill_pt_after - sigill_pt_before);

    /* ---- Test 4: Multi-thread pre-existing & post-patch threads (no pthread hook) ---- */
    long lbt_inits_before = read_kmod_stat("stat_lbt_inits");
    pthread_barrier_init(&g_pre_barrier, NULL, NUM_PRE_THREADS);
    pthread_barrier_init(&g_post_patch_barrier, NULL, NUM_PRE_THREADS);

    pthread_t pre_th[NUM_PRE_THREADS];
    struct mt_arg pre_args[NUM_PRE_THREADS];
    for (int t = 0; t < NUM_PRE_THREADS; t++) {
        pre_args[t].tid_idx = t;
        pre_args[t].is_pre = 1;
        pre_args[t].ok = 0;
        if (pthread_create(&pre_th[t], NULL, mt_worker, &pre_args[t]) != 0) {
            perror("[FAIL] pthread_create pre");
            return 1;
        }
    }
    for (int t = 0; t < NUM_PRE_THREADS; t++)
        pthread_join(pre_th[t], NULL);

    pthread_t post_th[NUM_POST_THREADS];
    struct mt_arg post_args[NUM_POST_THREADS];
    for (int t = 0; t < NUM_POST_THREADS; t++) {
        post_args[t].tid_idx = NUM_PRE_THREADS + t;
        post_args[t].is_pre = 0;
        post_args[t].ok = 0;
        if (pthread_create(&post_th[t], NULL, mt_worker, &post_args[t]) != 0) {
            perror("[FAIL] pthread_create post");
            return 1;
        }
    }
    for (int t = 0; t < NUM_POST_THREADS; t++)
        pthread_join(post_th[t], NULL);

    long lbt_inits_after = read_kmod_stat("stat_lbt_inits");
    for (int t = 0; t < NUM_PRE_THREADS; t++) {
        if (!pre_args[t].ok) {
            fprintf(stderr, "[FAIL] Test 4 pre-existing thread %d failed!\n", t);
            return 1;
        }
    }
    for (int t = 0; t < NUM_POST_THREADS; t++) {
        if (!post_args[t].ok) {
            fprintf(stderr, "[FAIL] Test 4 post-patch thread %d failed!\n", t);
            return 1;
        }
    }
    printf("[PASS] Test 4 (Multi-thread lazy LBT $scr1/$scr2 init): %d pre + %d post threads x %d iters OK | lbt_inits+%ld\n", NUM_PRE_THREADS, NUM_POST_THREADS, MT_ITERS,
           lbt_inits_after - lbt_inits_before);

    /* ---- Test 5: W^X strict R-X page kernel COW patch hypercall ---- */
    size_t pg_sz = (size_t)sysconf(_SC_PAGESIZE);
    uint32_t* rx_code = mmap(NULL, pg_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (rx_code == MAP_FAILED) {
        perror("[FAIL] mmap rx_code");
        return 1;
    }
    /* li.w $a0, 11 (0x02802c04); ret (0x4c000020) */
    rx_code[0] = 0x02802c04U;
    rx_code[1] = 0x4c000020U;
    __asm__ volatile("ibar 0" ::: "memory");
    if (mprotect(rx_code, pg_sz, PROT_READ | PROT_EXEC) != 0) {
        perror("[FAIL] mprotect R-X");
        return 1;
    }
    int (*rx_fn)(void) = (int (*)(void))(uintptr_t)rx_code;
    int v1 = rx_fn();
    long hcall_before = read_kmod_stat("stat_hcall_patches");
    long hret = invoke_wx_patch_hcall(&rx_code[0], 0x0280a804U);
    long hcall_after = read_kmod_stat("stat_hcall_patches");
    int v2 = rx_fn();
    munmap(rx_code, pg_sz);
    if (v1 != 11 || hret != 0 || v2 != 42 || hcall_after <= hcall_before) {
        fprintf(stderr, "[FAIL] Test 5 W^X patch failed (v1=%d, hret=%ld, v2=%d)\n", v1, hret, v2);
        return 1;
    }
    printf("[PASS] Test 5 (W^X R-X COW patch hypercall): R-X fn return %d -> %d without PROT_WRITE | hcall_patches+%ld\n", v1, v2, hcall_after - hcall_before);

    /* ---- Test 6: Exited-thread 64KB tctx slot recycling ---- */
    long allocs_before = read_kmod_stat("stat_tctx_allocs");
    long reused_before = read_kmod_stat("stat_tctx_reused");
    for (int k = 0; k < CHURN_THREADS; k++) {
        pthread_t th;
        void* res = NULL;
        if (pthread_create(&th, NULL, churn_worker, &k) != 0) {
            perror("[FAIL] pthread_create churn");
            return 1;
        }
        pthread_join(th, &res);
        if (res != NULL) {
            fprintf(stderr, "[FAIL] Test 6 churn worker %d output mismatch!\n", k);
            return 1;
        }
    }
    long allocs_after = read_kmod_stat("stat_tctx_allocs");
    long reused_after = read_kmod_stat("stat_tctx_reused");
    long delta_allocs = allocs_after - allocs_before;
    long delta_reused = reused_after - reused_before;
    if (delta_reused < CHURN_THREADS - 2 || delta_allocs > 2) {
        fprintf(stderr, "[FAIL] Test 6 tctx slot recycling did not trigger (allocs+%ld, reused+%ld)\n", delta_allocs, delta_reused);
        return 1;
    }
    printf("[PASS] Test 6 (Dead-thread 64KB tctx slot recycling): %d sequential threads -> allocs+%ld, reused+%ld\n", CHURN_THREADS, delta_allocs, delta_reused);

    /* ---- Test 7: fork() inheritance + Zero-env execve verification ---- */
    pid_t fpid = fork();
    if (fpid < 0) {
        perror("[FAIL] fork (no-exec)");
        return 1;
    }
    if (fpid == 0) {
        int32_t fout_vec[8] __attribute__((aligned(32))) = {0};
        for (int i = 0; i < 10000; i++)
            lasx_hot_vec_op(a, b, fout_vec);
        for (int i = 0; i < 8; i++) {
            if (fout_vec[i] != (a[i] + b[i]) * b[i])
                _exit(2);
        }
        _exit(0);
    }
    int fstatus = 0;
    waitpid(fpid, &fstatus, 0);
    if (!WIFEXITED(fstatus) || WEXITSTATUS(fstatus) != 0) {
        fprintf(stderr, "[FAIL] Test 7a forked child (without execve) failed: 0x%x\n", fstatus);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("[FAIL] fork");
        return 1;
    }
    if (pid == 0) {
        char* child_argv[] = {"./test_lasx_suite", "--child-clean-env", NULL};
        char* empty_env[] = {NULL};
        execve("./test_lasx_suite", child_argv, empty_env);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[FAIL] Test 7b clean-env child exited with status 0x%x\n", status);
        return 1;
    }
    printf("[PASS] Test 7 (fork() jiscr1 inheritance + Zero-env execve child, LD_PRELOAD absent): both passed!\n");

    return 0;
}

int main(int argc, char** argv)
{
    int is_child = (argc > 1 && strcmp(argv[1], "--child-clean-env") == 0);
    if (is_child && (getenv("LD_PRELOAD") != NULL || (environ && environ[0] != NULL))) {
        fprintf(stderr, "[FAIL] Child environment was not empty!\n");
        return 1;
    }
    return run_core_checks(is_child);
}
