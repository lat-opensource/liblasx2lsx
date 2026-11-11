/* SPDX-License-Identifier: MIT */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fenv.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <ucontext.h>

#include "../lasx_kproto.h"
#include "lagoon.h"
#include "lasx_emu.h"
#include "lasx_emu_private.h"
#include "lasx_interpret.h"
#include "loongarch-extcontext.h"
#include "perf_stats.h"
#include "thread_data.h"

/* LoongArch64 asm-generic syscall numbers */
#define NR_LOONGARCH_OPENAT 56
#define NR_LOONGARCH_CLOSE 57
#define NR_LOONGARCH_READ 63
#define NR_LOONGARCH_MMAP 222
#define NR_LOONGARCH_MPROTECT 226
#define RT_AT_FDCWD (-100)

/* Sentinel intercepted by __wrap_la_addi_d to swap $tp <-> $scr2 via $scr3 */
#define TP_SWAP_SENTINEL 0x7ac

/* Globals referenced by liblasx2lsx core */
int use_lsx_intrinsics = 1;
int lasx_interpret_mode = LASX_MODE_USEDEF | LASX_MODE_LOOP;
int lasx_interpret_frag_opt = 1;
uint64_t tp_offset = TP_SWAP_SENTINEL;

uint64_t g_perf_counts[P_MAX];
int g_perf_enabled = 0;

extern void* interpreter_entry;
extern void* interpreter_entry_ptr;
extern void* interpreter_entry_highwater;

int perf_stats_enabled(void) { return 0; }
void perf_stats_init(void) {}
void perf_stats_print(void) {}
void perf_stats_cleanup(void) {}

/* Raw LoongArch syscall helpers */
static inline long raw_syscall6(long nr, long a0, long a1, long a2, long a3, long a4, long a5)
{
    register long r_a0 __asm__("$a0") = a0;
    register long r_a1 __asm__("$a1") = a1;
    register long r_a2 __asm__("$a2") = a2;
    register long r_a3 __asm__("$a3") = a3;
    register long r_a4 __asm__("$a4") = a4;
    register long r_a5 __asm__("$a5") = a5;
    register long r_a7 __asm__("$a7") = nr;
    __asm__ volatile("syscall 0" : "+r"(r_a0) : "r"(r_a1), "r"(r_a2), "r"(r_a3), "r"(r_a4), "r"(r_a5), "r"(r_a7) : "memory", "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7", "$t8");
    return r_a0;
}

static inline long raw_syscall4(long nr, long a0, long a1, long a2, long a3) { return raw_syscall6(nr, a0, a1, a2, a3, 0, 0); }

static inline long raw_syscall3(long nr, long a0, long a1, long a2) { return raw_syscall6(nr, a0, a1, a2, 0, 0, 0); }

static inline long raw_syscall1(long nr, long a0) { return raw_syscall6(nr, a0, 0, 0, 0, 0, 0); }

/* LBT scratch register helpers:
 *   movgr2scr $scr1, $a0 : 0x00000800 | (4 << 5) | 1 = 0x00000881
 *   movgr2scr $scr2, $a0 : 0x00000800 | (4 << 5) | 2 = 0x00000882
 *   movscr2gr $a0, $scr2 : 0x00000c00 | (2 << 5) | 4 = 0x00000c44
 */
static inline void set_scr1(uint64_t val)
{
    register uint64_t r_a0 __asm__("$a0") = val;
    __asm__ volatile(".word 0x00000881" : : "r"(r_a0) : "memory");
}

static inline void set_scr2(uint64_t val)
{
    register uint64_t r_a0 __asm__("$a0") = val;
    __asm__ volatile(".word 0x00000882" : : "r"(r_a0) : "memory");
}

static inline uint64_t get_scr2(void)
{
    register uint64_t r_a0 __asm__("$a0");
    __asm__ volatile(".word 0x00000c44" : "=r"(r_a0));
    return r_a0;
}

/* Per-thread data pointer lives directly in $scr2 (no TLS / pthread needed) */
void* thread_data_get(void) { return (void*)(uintptr_t)get_scr2(); }

void thread_data_free(void) {}
void thread_data_cleanup(void* unused) { (void)unused; }

/* Linker wrap: intercept JIT prologue/epilogue `addi.d $tp, $tp, +/-tp_offset`
 * and replace with $scr3-saved swap from $scr2 so dynamic per-thread mappings
 * work for arbitrary threads without static TLS offsets.
 */
extern void __real_la_addi_d(lagoon_assembler_t* as, la_gpr_t rd, la_gpr_t rj, int32_t sk12);

void __wrap_la_addi_d(lagoon_assembler_t* as, la_gpr_t rd, la_gpr_t rj, int32_t sk12)
{
    if (rd == LA_TP && rj == LA_TP && sk12 == TP_SWAP_SENTINEL) {
        la_movgr2scr(as, LA_SCR3, LA_TP);
        la_movscr2gr(as, LA_TP, LA_SCR2);
        return;
    }
    if (rd == LA_TP && rj == LA_TP && sk12 == -TP_SWAP_SENTINEL) {
        la_movscr2gr(as, LA_TP, LA_SCR3);
        return;
    }
    __real_la_addi_d(as, rd, rj, sk12);
}

/* Linker wrap: drop MAP_FIXED so JIT code/data caches never collide with
 * target process VMA mappings.
 */
void* __wrap_mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    (void)addr;
    flags &= ~MAP_FIXED;
    long ret = raw_syscall6(NR_LOONGARCH_MMAP, 0, (long)len, prot, flags, fd, (long)offset);
    if ((unsigned long)ret >= (unsigned long)-4095L)
        return MAP_FAILED;
    return (void*)ret;
}

int __wrap_mprotect(void* addr, size_t len, int prot)
{
    long ret = raw_syscall3(NR_LOONGARCH_MPROTECT, (long)addr, (long)len, prot);
    if (ret == 0)
        return 0;

    /* On 4KB-page kernels, a 16KB-aligned range may cross a VMA boundary */
    int any_ok = 0;
    for (size_t off = 0; off < len; off += 0x1000UL) {
        if (raw_syscall3(NR_LOONGARCH_MPROTECT, (long)((uintptr_t)addr + off), 0x1000L, prot) == 0)
            any_ok = 1;
    }
    return any_ok ? 0 : -1;
}

/* Freestanding math & fenv routines using native LoongArch FP/LSX instructions */
float ceilf(float x)
{
    float r;
    __asm__("vreplvei.w %w0, %w1, 0\n\tvfrintrp.s %w0, %w0" : "=&f"(r) : "f"(x));
    return r;
}

float floorf(float x)
{
    float r;
    __asm__("vreplvei.w %w0, %w1, 0\n\tvfrintrm.s %w0, %w0" : "=&f"(r) : "f"(x));
    return r;
}

float truncf(float x)
{
    float r;
    __asm__("vreplvei.w %w0, %w1, 0\n\tvfrintrz.s %w0, %w0" : "=&f"(r) : "f"(x));
    return r;
}

double trunc(double x)
{
    double r;
    __asm__("vreplvei.d %w0, %w1, 0\n\tvfrintrz.d %w0, %w0" : "=&f"(r) : "f"(x));
    return r;
}

float rintf(float x)
{
    float r;
    __asm__("frint.s %0, %1" : "=f"(r) : "f"(x));
    return r;
}

double rint(double x)
{
    double r;
    __asm__("frint.d %0, %1" : "=f"(r) : "f"(x));
    return r;
}

float sqrtf(float x)
{
    float r;
    __asm__("fsqrt.s %0, %1" : "=f"(r) : "f"(x));
    return r;
}

double sqrt(double x)
{
    double r;
    __asm__("fsqrt.d %0, %1" : "=f"(r) : "f"(x));
    return r;
}

int fegetenv(fenv_t* envp)
{
    unsigned int fcsr;
    __asm__ volatile("movfcsr2gr %0, $fcsr0" : "=r"(fcsr));
    envp->__fp_control_register = fcsr;
    return 0;
}

int fesetenv(const fenv_t* envp)
{
    unsigned int fcsr = (envp == FE_DFL_ENV) ? 0U : envp->__fp_control_register;
    __asm__ volatile("movgr2fcsr $fcsr0, %0" : : "r"(fcsr));
    return 0;
}

int fesetround(int round_mode)
{
    if (round_mode & ~0x300)
        return -1;
    unsigned int fcsr;
    __asm__ volatile("movfcsr2gr %0, $fcsr0" : "=r"(fcsr));
    fcsr = (fcsr & ~0x300U) | (unsigned int)round_mode;
    __asm__ volatile("movgr2fcsr $fcsr0, %0" : : "r"(fcsr));
    return 0;
}

/* Freestanding string & memory primitives (compiler barrier blocks self-call pattern recognition) */
void* memset(void* s, int c, size_t n)
{
    unsigned char* p = (unsigned char*)s;
    unsigned char v = (unsigned char)c;
    for (size_t i = 0; i < n; i++) {
        p[i] = v;
        __asm__ volatile("" ::: "memory");
    }
    return s;
}

void* memcpy(void* dst, const void* src, size_t n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
        __asm__ volatile("" ::: "memory");
    }
    return dst;
}

void* memmove(void* dst, const void* src, size_t n)
{
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    if (d < s || d >= s + n) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
            __asm__ volatile("" ::: "memory");
        }
    } else {
        for (size_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
            __asm__ volatile("" ::: "memory");
        }
    }
    return dst;
}

int memcmp(const void* s1, const void* s2, size_t n)
{
    const unsigned char* a = (const unsigned char*)s1;
    const unsigned char* b = (const unsigned char*)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
        __asm__ volatile("" ::: "memory");
    }
    return 0;
}

size_t strlen(const char* s)
{
    size_t len = 0;
    while (s[len]) {
        len++;
        __asm__ volatile("" ::: "memory");
    }
    return len;
}

char* strstr(const char* haystack, const char* needle)
{
    if (!*needle)
        return (char*)haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (*haystack == *needle && memcmp(haystack, needle, nlen) == 0)
            return (char*)haystack;
    }
    return NULL;
}

/* Lock-protected arena for lagoon_label_t temporary arrays */
static uint8_t rt_heap[65536] __attribute__((aligned(16)));
static size_t rt_heap_off = 0;
static int rt_heap_lock_word = 0;

static inline void heap_lock(void)
{
    int* mem = &rt_heap_lock_word;
    __asm__ volatile("1:\n\t"
                     "ori $t1, $zero, 1\n\t"
                     "amswap_db.w $t0, $t1, %0\n\t"
                     "bnez $t0, 1b\n\t"
                     :
                     : "r"(mem)
                     : "memory", "$t0", "$t1");
}

static inline void heap_unlock(void)
{
    __asm__ volatile("dbar 0" ::: "memory");
    rt_heap_lock_word = 0;
    __asm__ volatile("dbar 0" ::: "memory");
}

void* malloc(size_t size)
{
    size_t aligned = (size + 15UL) & ~15UL;
    heap_lock();
    if (rt_heap_off + 16UL + aligned > sizeof(rt_heap)) {
        rt_heap_off = 0;
        if (16UL + aligned > sizeof(rt_heap)) {
            heap_unlock();
            return NULL;
        }
    }
    uint64_t* hdr = (uint64_t*)(rt_heap + rt_heap_off);
    hdr[0] = size;
    hdr[1] = 0;
    rt_heap_off += 16UL + aligned;
    heap_unlock();
    return (void*)(hdr + 2);
}

void free(void* ptr) { (void)ptr; }

void* realloc(void* ptr, size_t new_size)
{
    if (!ptr)
        return malloc(new_size);
    void* np = malloc(new_size);
    if (!np)
        return NULL;
    uint64_t old_size = ((uint64_t*)ptr)[-2];
    size_t copy_len = (old_size < new_size) ? (size_t)old_size : new_size;
    memcpy(np, ptr, copy_len);
    return np;
}

/* Freestanding dladdr() via /proc/self/maps for fragment pattern matching */
static char dli_fname_buf[256];

static int parse_hex_u64(const char** pp, uint64_t* out)
{
    const char* p = *pp;
    uint64_t v = 0;
    int digits = 0;

    while (*p) {
        char c = *p;
        uint64_t d;
        if (c >= '0' && c <= '9')
            d = (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f')
            d = (uint64_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            d = (uint64_t)(c - 'A' + 10);
        else
            break;
        v = (v << 4) | d;
        p++;
        digits++;
    }
    *pp = p;
    *out = v;
    return digits > 0;
}

int dladdr(const void* addr, Dl_info* info)
{
    if (!addr)
        return 0;

    long fd = raw_syscall4(NR_LOONGARCH_OPENAT, RT_AT_FDCWD, (long)"/proc/self/maps", 0, 0);
    if (fd < 0)
        return 0;

    uint64_t target = (uint64_t)(uintptr_t)addr;
    char buf[2048];
    size_t carry = 0;
    int found = 0;

    while (!found) {
        long nr = raw_syscall3(NR_LOONGARCH_READ, fd, (long)(buf + carry), (long)(sizeof(buf) - 1 - carry));
        if (nr <= 0)
            break;
        size_t total = carry + (size_t)nr;
        buf[total] = '\0';

        size_t line_start = 0;
        for (size_t i = 0; i < total; i++) {
            if (buf[i] != '\n')
                continue;
            buf[i] = '\0';
            const char* p = buf + line_start;
            uint64_t vstart = 0, vend = 0, pgoff = 0;
            if (parse_hex_u64(&p, &vstart) && *p == '-') {
                p++;
                if (parse_hex_u64(&p, &vend) && target >= vstart && target < vend) {
                    while (*p == ' ')
                        p++;
                    while (*p && *p != ' ')
                        p++;
                    while (*p == ' ')
                        p++;
                    if (parse_hex_u64(&p, &pgoff)) {
                        for (int skip = 0; skip < 2; skip++) {
                            while (*p == ' ')
                                p++;
                            while (*p && *p != ' ')
                                p++;
                        }
                        while (*p == ' ')
                            p++;
                        size_t flen = strlen(p);
                        if (flen >= sizeof(dli_fname_buf))
                            flen = sizeof(dli_fname_buf) - 1;
                        memcpy(dli_fname_buf, p, flen);
                        dli_fname_buf[flen] = '\0';
                        info->dli_fbase = (void*)(uintptr_t)(vstart - pgoff);
                        info->dli_fname = dli_fname_buf;
                        info->dli_sname = NULL;
                        info->dli_saddr = NULL;
                        found = 1;
                        break;
                    }
                }
            }
            line_start = i + 1;
        }
        carry = total - line_start;
        if (carry > 0 && line_start > 0)
            memmove(buf, buf + line_start, carry);
        else if (carry >= sizeof(buf) - 1)
            carry = 0;
    }

    raw_syscall1(NR_LOONGARCH_CLOSE, fd);
    return found;
}

int snprintf(char* str, size_t size, const char* format, ...)
{
    (void)format;
    if (str && size > 0)
        str[0] = '\0';
    return 0;
}

void __assert_fail(const char* assertion, const char* file, unsigned int line, const char* function)
{
    (void)assertion;
    (void)file;
    (void)line;
    (void)function;
    for (;;)
        __asm__ volatile("break 0");
}

#if defined(CONFIG_LOONGARCH_NEW_WORLD)
/* Synthesized LoongArch New-World ucontext_t + extended LSX/LBT sigcontext */
#define ALIGN16(x) (((x) + 15UL) & ~15UL)
#define LSX_SCTX_BYTES ALIGN16(sizeof(struct sctx_info) + sizeof(struct lsx_context))
#define LBT_SCTX_BYTES ALIGN16(sizeof(struct sctx_info) + sizeof(struct lbt_context))

struct synth_ucontext {
    ucontext_t uc;
    uint8_t ext_space[LSX_SCTX_BYTES + LBT_SCTX_BYTES + 64];
} __attribute__((aligned(16)));
#else
struct synth_ucontext {
    ucontext_t uc;
} __attribute__((aligned(16)));
#endif

static volatile int rt_init_state = 0;

static void ensure_rt_initialized(void)
{
    if (__builtin_expect(rt_init_state == 2, 1))
        return;

    int* mem = (int*)&rt_init_state;
    int prev;
    __asm__ volatile("ori $t1, $zero, 1\n\t"
                     "amswap_db.w %0, $t1, %1\n\t"
                     : "=r"(prev)
                     : "r"(mem)
                     : "memory", "$t1");

    if (prev == 0) {
        use_lsx_intrinsics = 1;
        tp_offset = TP_SWAP_SENTINEL;
        lasx_init_interpret();
        __asm__ volatile("dbar 0" ::: "memory");
        rt_init_state = 2;
        __asm__ volatile("dbar 0" ::: "memory");
    } else {
        while (rt_init_state != 2)
            __asm__ volatile("" ::: "memory");
        __asm__ volatile("dbar 0" ::: "memory");
    }
}

static void apply_mode_flags(uint32_t mflags)
{
    int imode = 0;
    if (mflags & LASX_KMODE_BLOCK)
        imode |= LASX_MODE_BLOCK;
    if (mflags & LASX_KMODE_LOOP)
        imode |= LASX_MODE_LOOP;
    if (mflags & LASX_KMODE_USEDEF)
        imode |= LASX_MODE_USEDEF;
    if ((imode & LASX_MODE_BLOCK) && (imode & LASX_MODE_USEDEF))
        imode &= ~LASX_MODE_BLOCK;

    lasx_interpret_mode = imode;
    lasx_interpret_frag_opt = (mflags & LASX_KMODE_FRAG) ? 1 : 0;
}

void lasx_rt_handle_trap(struct lasx_trap_frame* tf)
{
    uintptr_t td_addr = (uintptr_t)tf + LASX_TD_OFFSET;
    set_scr2(td_addr);

    ensure_rt_initialized();

    if (interpreter_entry)
        set_scr1((uint64_t)(uintptr_t)interpreter_entry);
    set_scr2(td_addr);

    tf->trap_count++;

    /* Lazy LBT thread initialization: $scr1 and $scr2 are now bound, resume jiscr1 */
    if (tf->trap_kind == LASX_TRAP_LBT_INIT)
        return;

    uint32_t instr = *(const uint32_t*)(uintptr_t)tf->orig_pc;

    /* Another thread already patched this PC to jiscr1 concurrently */
    if ((instr & 0xfc0003e0U) == 0x48000300U) {
        __asm__ volatile("ibar 0" ::: "memory");
        return;
    }

    apply_mode_flags(tf->mode_flags);

    struct synth_ucontext suc;
    memset(&suc, 0, sizeof(suc));

    suc.uc.uc_mcontext.__pc = tf->orig_pc;
    for (int i = 0; i < 32; i++)
        suc.uc.uc_mcontext.__gregs[i] = tf->gpr[i];

#if defined(CONFIG_LOONGARCH_NEW_WORLD)
    uint8_t* ext_p = (uint8_t*)&suc.uc.uc_mcontext.__extcontext;

    struct sctx_info* lsx_info = (struct sctx_info*)ext_p;
    lsx_info->magic = LSX_CTX_MAGIC;
    lsx_info->size = LSX_SCTX_BYTES;
    struct lsx_context* lsx_ctx = (struct lsx_context*)(ext_p + sizeof(struct sctx_info));
    memcpy(lsx_ctx->regs, tf->vr, sizeof(tf->vr));
    lsx_ctx->fcc = tf->fcc;
    lsx_ctx->fcsr = tf->fcsr;

    struct sctx_info* lbt_info = (struct sctx_info*)(ext_p + LSX_SCTX_BYTES);
    lbt_info->magic = LBT_CTX_MAGIC;
    lbt_info->size = LBT_SCTX_BYTES;

    struct sctx_info* end_info = (struct sctx_info*)(ext_p + LSX_SCTX_BYTES + LBT_SCTX_BYTES);
    end_info->magic = 0;
    end_info->size = 0;
#else
    for (int i = 0; i < 32; i++) {
        suc.uc.uc_mcontext.__fpregs[i + 1].__val64[0] = tf->vr[i][0];
        suc.uc.uc_mcontext.__fpregs[i + 1].__val64[1] = tf->vr[i][1];
    }
    suc.uc.uc_mcontext.__fcc = tf->fcc;
    suc.uc.uc_mcontext.__fcsr = tf->fcsr;
#endif

    bool can_jit = (tf->mode_flags != LASX_KMODE_PURE_EMU) && interpreter_entry != NULL && interpreter_entry_ptr != NULL && interpreter_entry_highwater != NULL &&
                   ((uintptr_t)interpreter_entry_ptr + 0x2000UL < (uintptr_t)interpreter_entry_highwater);

    bool handled = false;
    if (can_jit && lasx_emu_create_interpret_fragment(&suc.uc)) {
        handled = true;
    } else if (can_jit && lasx_emu_create_interpret_block(&suc.uc)) {
        handled = true;
    } else if (can_jit && (tf->mode_flags & LASX_KMODE_ONE) && lasx_emu_create_interpret(&suc.uc, instr)) {
        handled = true;
    } else if (do_lasx_emu(&suc.uc, instr)) {
        suc.uc.uc_mcontext.__pc += 4;
        handled = true;
    }

    if (!handled) {
        /* Unrecognized opcode: bounce back to orig_pc so kernel delivers SIGILL */
        tf->in_rt = LASX_IN_RT_PASSTHRU;
        return;
    }

    tf->orig_pc = suc.uc.uc_mcontext.__pc;
    for (int i = 0; i < 32; i++)
        tf->gpr[i] = suc.uc.uc_mcontext.__gregs[i];

#if defined(CONFIG_LOONGARCH_NEW_WORLD)
    memcpy(tf->vr, lsx_ctx->regs, sizeof(tf->vr));
    tf->fcc = lsx_ctx->fcc;
    tf->fcsr = lsx_ctx->fcsr;
#else
    for (int i = 0; i < 32; i++) {
        tf->vr[i][0] = suc.uc.uc_mcontext.__fpregs[i + 1].__val64[0];
        tf->vr[i][1] = suc.uc.uc_mcontext.__fpregs[i + 1].__val64[1];
    }
    tf->fcc = suc.uc.uc_mcontext.__fcc;
    tf->fcsr = suc.uc.uc_mcontext.__fcsr;
#endif

    if (interpreter_entry)
        set_scr1((uint64_t)(uintptr_t)interpreter_entry);
    set_scr2(td_addr);
}
