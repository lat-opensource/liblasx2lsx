// SPDX-License-Identifier: GPL-2.0-only
/*
 * lasx2lsx_kmod.c — On-demand, LD_PRELOAD-free LASX-to-LSX kernel module
 *
 * Production target: Loongson 3B6000M / 2K3000 (LSX + LBT, no hardware LASX).
 *   - Hardware natively raises EXCCODE_INE -> do_ri on LASX instructions.
 *   - Module intercepts user RI traps on demand, maps the embedded freestanding
 *     liblasx2lsx PIE runtime (lasx_rt_blob) and a 64KB per-thread context
 *     (reused across thread exits), and redirects user execution to
 *     lasx_rt_trap_entry without signals, TLS, or pthread hooks.
 *   - Hooks first-use EXCCODE_BTDIS (do_lbt) on patched jiscr1 sites so
 *     pre-existing, newly cloned, and fork()ed threads lazily bind $scr1/$scr2.
 *
 * Verification mode (sim_3a6000=1 target_comm=<prefix> on Loongson 3A6000):
 *   - Additionally intercepts EXCCODE_LASXDIS (do_lasx) for target_comm
 *     processes so CSR.EUEN.LASXEN stays 0, faithfully simulating 3B6000M /
 *     2K3000 reserved-instruction traps on 3A6000 hardware.
 */

#include <linux/atomic.h>
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/cpumask.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/hashtable.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#if __has_include(<linux/instrumentation.h>)
#include <linux/instrumentation.h>
#endif
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mman.h>
#if __has_include(<linux/mmap_lock.h>)
#include <linux/mmap_lock.h>
#endif
#include <linux/mmu_notifier.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/ptrace.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/stop_machine.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
#include <linux/entry-common.h>
#include <linux/tracepoint.h>
#include <trace/events/sched.h>
#endif

#include <asm/cpu-features.h>
#include <asm/cpu-info.h>
#include <asm/cpu.h>
#if __has_include(<asm/loongarch.h>)
#include <asm/loongarch.h>
#else
#include <asm/loongarchregs.h>
#endif
#include <asm/ptrace.h>
#include <asm/thread_info.h>

#include "lasx_kproto.h"
#include "lasx_rt_meta.h"

#ifndef noinstr
#define noinstr
#endif

#ifndef cpu_has_iocsr
#define cpu_has_iocsr (read_cpucfg(LOONGARCH_CPUCFG1) & CPUCFG1_IOCSR)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
#define lasx_noinstr_atomic_inc(v) raw_atomic_long_inc(v)
#else
#define lasx_noinstr_atomic_inc(v) atomic_long_inc(v)
#endif

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("On-demand LD_PRELOAD-free LASX-to-LSX binary translation for Loongson 3B6000M / 2K3000");

extern const u8 lasx_rt_blob[];

/* Module parameters */
static bool sim_3a6000 = false;
module_param(sim_3a6000, bool, 0444);
MODULE_PARM_DESC(sim_3a6000, "Enable 3A6000 LASX-disable simulation for verification only (default: false)");

static char target_comm[TASK_COMM_LEN] = "";
module_param_string(target_comm, target_comm, sizeof(target_comm), 0644);
MODULE_PARM_DESC(target_comm, "Optional comm prefix filter (empty = all processes on 3B6000M/2K3000; required when sim_3a6000=1)");

static bool enable_runtime = true;
module_param(enable_runtime, bool, 0644);
MODULE_PARM_DESC(enable_runtime, "Enable on-demand lasx_rt injection (false = pure SIGILL trap verification)");

static atomic_t g_mode_flags = ATOMIC_INIT(LASX_KMODE_DEFAULT);
static bool g_patch_lasxdis = false;
static enum cpuhp_state g_cpuhp_state;
static bool g_cpuhp_registered = false;

static int parse_interpret_mode_str(const char* val, u32* out_flags)
{
    char buf[64];
    char *p, *tok;
    u32 flags = LASX_KMODE_ONE;
    bool saw_any = false;

    if (!val)
        return -EINVAL;

    strscpy(buf, val, sizeof(buf));
    p = strim(buf);
    if (*p == '\0') {
        *out_flags = LASX_KMODE_DEFAULT;
        return 0;
    }

    while ((tok = strsep(&p, ",|; \t\n")) != NULL) {
        if (*tok == '\0')
            continue;
        saw_any = true;
        if (strcmp(tok, "off") == 0 || strcmp(tok, "pure_emu") == 0) {
            *out_flags = LASX_KMODE_PURE_EMU;
            return 0;
        } else if (strcmp(tok, "one") == 0 || strcmp(tok, "emu") == 0) {
            flags = LASX_KMODE_ONE;
        } else if (strcmp(tok, "block") == 0) {
            flags |= LASX_KMODE_BLOCK;
        } else if (strcmp(tok, "loop") == 0) {
            flags |= LASX_KMODE_LOOP;
        } else if (strcmp(tok, "usedef") == 0) {
            flags |= LASX_KMODE_USEDEF;
        } else if (strcmp(tok, "frag") == 0) {
            flags |= LASX_KMODE_FRAG;
        } else if (strcmp(tok, "blockloop") == 0) {
            flags |= LASX_KMODE_BLOCK | LASX_KMODE_LOOP;
        } else if (strcmp(tok, "default") == 0) {
            flags = LASX_KMODE_DEFAULT;
        } else {
            return -EINVAL;
        }
    }

    if (!saw_any)
        return -EINVAL;

    if ((flags & LASX_KMODE_BLOCK) && (flags & LASX_KMODE_USEDEF))
        flags &= ~LASX_KMODE_BLOCK;

    *out_flags = flags;
    return 0;
}

static int param_set_interpret_mode(const char* val, const struct kernel_param* kp)
{
    u32 flags = 0;
    int ret;

    (void)kp;
    ret = parse_interpret_mode_str(val, &flags);
    if (ret)
        return ret;
    atomic_set(&g_mode_flags, (int)flags);
    return 0;
}

static int param_get_interpret_mode(char* buf, const struct kernel_param* kp)
{
    u32 flags = (u32)atomic_read(&g_mode_flags);
    int len = 0;

    (void)kp;
    if (flags == LASX_KMODE_PURE_EMU)
        return scnprintf(buf, PAGE_SIZE, "off\n");
    if (flags == LASX_KMODE_ONE)
        return scnprintf(buf, PAGE_SIZE, "one\n");

    if (flags & LASX_KMODE_USEDEF)
        len += scnprintf(buf + len, PAGE_SIZE - len, "%susedef", len ? "," : "");
    else if (flags & LASX_KMODE_BLOCK)
        len += scnprintf(buf + len, PAGE_SIZE - len, "%sblock", len ? "," : "");
    if (flags & LASX_KMODE_LOOP)
        len += scnprintf(buf + len, PAGE_SIZE - len, "%sloop", len ? "," : "");
    if (flags & LASX_KMODE_FRAG)
        len += scnprintf(buf + len, PAGE_SIZE - len, "%sfrag", len ? "," : "");
    len += scnprintf(buf + len, PAGE_SIZE - len, "\n");
    return len;
}

static const struct kernel_param_ops interpret_mode_ops = {
    .set = param_set_interpret_mode,
    .get = param_get_interpret_mode,
};
module_param_cb(interpret_mode, &interpret_mode_ops, NULL, 0644);
MODULE_PARM_DESC(interpret_mode, "JIT mode: usedef,loop,frag (default), block, one/emu, or off");

/* Read-only observability counters via /sys/module/lasx2lsx/parameters/ */
static atomic_long_t cnt_lasx_intercepted = ATOMIC_LONG_INIT(0);
static atomic_long_t cnt_lasx_passthrough = ATOMIC_LONG_INIT(0);
static atomic_long_t cnt_rt_upcalls = ATOMIC_LONG_INIT(0);
static atomic_long_t cnt_lbt_inits = ATOMIC_LONG_INIT(0);
static atomic_long_t cnt_mm_injections = ATOMIC_LONG_INIT(0);
static atomic_long_t cnt_tctx_allocs = ATOMIC_LONG_INIT(0);
static atomic_long_t cnt_tctx_reused = ATOMIC_LONG_INIT(0);
static atomic_long_t cnt_sigill_passthrough = ATOMIC_LONG_INIT(0);
static atomic_long_t cnt_hcall_patches = ATOMIC_LONG_INIT(0);

static int param_get_atomic_long(char* buf, const struct kernel_param* kp)
{
    const atomic_long_t* v = (const atomic_long_t*)kp->arg;
    return scnprintf(buf, PAGE_SIZE, "%ld\n", atomic_long_read(v));
}

static const struct kernel_param_ops ro_atomic_ops = {
    .get = param_get_atomic_long,
};

module_param_cb(stat_lasx_intercepted, &ro_atomic_ops, &cnt_lasx_intercepted, 0444);
module_param_cb(stat_lasx_passthrough, &ro_atomic_ops, &cnt_lasx_passthrough, 0444);
module_param_cb(stat_rt_upcalls, &ro_atomic_ops, &cnt_rt_upcalls, 0444);
module_param_cb(stat_lbt_inits, &ro_atomic_ops, &cnt_lbt_inits, 0444);
module_param_cb(stat_mm_injections, &ro_atomic_ops, &cnt_mm_injections, 0444);
module_param_cb(stat_tctx_allocs, &ro_atomic_ops, &cnt_tctx_allocs, 0444);
module_param_cb(stat_tctx_reused, &ro_atomic_ops, &cnt_tctx_reused, 0444);
module_param_cb(stat_sigill_passthrough, &ro_atomic_ops, &cnt_sigill_passthrough, 0444);
module_param_cb(stat_hcall_patches, &ro_atomic_ops, &cnt_hcall_patches, 0444);

/* Per-thread context tracking inside an injected mm_struct */
struct lasx_tctx_node {
    struct list_head list;
    pid_t pid;
    u64 start_time;
    unsigned long uaddr;
};

/* Per-process (mm_struct) runtime state */
struct lasx_mm_ctx {
    struct hlist_node hnode;
    struct rcu_head rcu;
    struct mm_struct* mm;
    unsigned long rt_base;
    spinlock_t lock;
    struct list_head tctx_list;
    struct mmu_notifier mn;
    bool mn_registered;
};

static DEFINE_HASHTABLE(lasx_mm_hash, 8);
static DEFINE_SPINLOCK(lasx_mm_hash_lock);
static DEFINE_MUTEX(lasx_inject_mutex);

/* Original kernel exception C handlers decoded from live CSR_EENTRY */
typedef asmlinkage void (*loongarch_exc_fn_t)(struct pt_regs* regs);
static loongarch_exc_fn_t orig_do_ri;
static loongarch_exc_fn_t orig_do_lasx;
static loongarch_exc_fn_t orig_do_lbt;

static u32 saved_lasx_insns[4];
static u32 saved_lbt_insns[4];
static u32 patched_lasx_insns[4];
static u32 patched_lbt_insns[4];
static u32 saved_ri_insns[4];
static u32 patched_ri_insns[4];
static atomic_t active_traps = ATOMIC_INIT(0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
static DECLARE_WAIT_QUEUE_HEAD(active_traps_wait);
#else
static irqentry_state_t (*kernel_irqentry_enter)(struct pt_regs*);
static void (*kernel_irqentry_exit)(struct pt_regs*, irqentry_state_t);
#endif

/* A modern irqentry exit can terminate the task instead of returning. Keep
 * stack frames on this short list so sched_process_exit can release them. */
struct lasx_active_trap {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    struct lasx_active_trap *next, **prev;
    struct task_struct* task;
#endif
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
static struct lasx_active_trap* active_head;
static atomic_t active_lock = ATOMIC_INIT(0);
static struct tracepoint* task_exit_tp;
static void* task_exit_probe;

/* These operations also run outside context tracking. Avoid lockdep, tracing,
 * and debug-list helpers there; the lock only protects pointer updates. */
static __always_inline unsigned long lasx_active_lock(void)
{
    unsigned long flags;
    raw_local_irq_save(flags);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
    while (raw_atomic_cmpxchg_acquire(&active_lock, 0, 1))
#else
    while (atomic_cmpxchg_acquire(&active_lock, 0, 1))
#endif
        cpu_relax();
    return flags;
}

static __always_inline void lasx_active_unlock(unsigned long flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
    raw_atomic_set_release(&active_lock, 0);
#else
    atomic_set_release(&active_lock, 0);
#endif
    raw_local_irq_restore(flags);
}

static __always_inline void lasx_untrack_trap(struct lasx_active_trap* trap)
{
    *trap->prev = trap->next;
    if (trap->next)
        trap->next->prev = trap->prev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
    raw_atomic_dec(&active_traps);
#else
    atomic_dec(&active_traps);
#endif
}

static void lasx_task_exit(void* unused, struct task_struct* task)
{
    struct lasx_active_trap *trap, *next;
    unsigned long flags = lasx_active_lock();
    (void)unused;
    for (trap = active_head; trap; trap = next) {
        next = trap->next;
        if (trap->task == task)
            lasx_untrack_trap(trap);
    }
    lasx_active_unlock(flags);
}

static void lasx_task_exit_group(void* unused, struct task_struct* task, bool group_dead)
{
    (void)group_dead;
    lasx_task_exit(unused, task);
}

#ifdef CONFIG_TRACEPOINTS
static void find_task_exit_tp(struct tracepoint* tp, void* unused)
{
    (void)unused;
    if (!strcmp(tp->name, "sched_process_exit"))
        task_exit_tp = tp;
}
#endif

static int register_task_exit(void)
{
#ifdef CONFIG_TRACEPOINTS
    /* Select the exact prototype from these headers, including vendor kernels. */
    if (__same_type(&register_trace_sched_process_exit, int (*)(void (*)(void*, struct task_struct*, bool), void*)))
        task_exit_probe = lasx_task_exit_group;
    else if (__same_type(&register_trace_sched_process_exit, int (*)(void (*)(void*, struct task_struct*), void*)))
        task_exit_probe = lasx_task_exit;
    else
        return -EOPNOTSUPP;
    for_each_kernel_tracepoint(find_task_exit_tp, NULL);
    if (task_exit_tp)
        return tracepoint_probe_register(task_exit_tp, task_exit_probe, NULL);
#endif
    pr_err("lasx2lsx: sched_process_exit tracepoint is required on this kernel\n");
    return -EOPNOTSUPP;
}

static void unregister_task_exit(void)
{
#ifdef CONFIG_TRACEPOINTS
    tracepoint_probe_unregister(task_exit_tp, task_exit_probe, NULL);
    tracepoint_synchronize_unregister();
#endif
}
#endif

static void noinstr lasx_trap_exit(struct lasx_active_trap* trap)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    unsigned long flags = lasx_active_lock();
    lasx_untrack_trap(trap);
    lasx_active_unlock(flags);
#else
    (void)trap;
    if (atomic_dec_and_test(&active_traps))
        wake_up(&active_traps_wait);
#endif
}

static __always_inline void lasx_trap_enter(struct lasx_active_trap* trap)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    unsigned long flags = lasx_active_lock();
    trap->task = current;
    trap->next = active_head;
    trap->prev = &active_head;
    if (active_head)
        active_head->prev = &trap->next;
    active_head = trap;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
    raw_atomic_inc(&active_traps);
#else
    atomic_inc(&active_traps);
#endif
    lasx_active_unlock(flags);
#else
    (void)trap;
    atomic_inc(&active_traps);
#endif
}

/* LoongArch exception vector slot layout (CSR_EENTRY + exccode * 0x200):
 *   +0x114: 0x00150064 (move $a0, $sp)
 *   +0x118: lu12i.w $t0, addr[31:12]
 *   +0x11c: ori     $t0, $t0, addr[11:0]
 *   +0x120: lu32i.d $t0, addr[51:32]
 *   +0x124: lu52i.d $t0, $t0, addr[63:52]
 *   +0x128: 0x4c000181 (jirl $ra, $t0, 0)
 */
#define VEC_SLOT_STRIDE 0x200UL
#define VEC_MOVE_A0_SP_IDX (0x114 / 4)

#define INSN_MOVE_A0_SP 0x00150064U
#define INSN_JIRL_RA_T0 0x4c000181U

/* Loongnix 4.19 uses a different prologue for each exception. */
static unsigned int vec_move_idx(int exccode)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
    if (exccode == EXCCODE_INE)
        return 0x11c / 4;
    if (exccode == EXCCODE_LASXDIS)
        return 0xf4 / 4;
    if (exccode == EXCCODE_BTDIS)
        return 0x120 / 4;
#endif
    return VEC_MOVE_A0_SP_IDX;
}

static void encode_t0_imm64(unsigned long addr, u32 out[4])
{
    out[0] = 0x1400000cU | (u32)(((addr >> 12) & 0xfffffUL) << 5);
    out[1] = 0x0380018cU | (u32)((addr & 0xfffUL) << 10);
    out[2] = 0x1600000cU | (u32)(((addr >> 32) & 0xfffffUL) << 5);
    out[3] = 0x0300018cU | (u32)(((addr >> 52) & 0xfffUL) << 10);
}

static int decode_and_verify_vec_target(unsigned long eentry, int exccode, unsigned long* fn_out, u32 orig_insns[4])
{
    const u32* vec = (const u32*)(eentry + (unsigned long)exccode * VEC_SLOT_STRIDE);
    u32 i0, i1, i2, i3;
    u32 check[4];
    unsigned long addr;
    unsigned int move_idx = vec_move_idx(exccode);

    if ((vec[move_idx] != INSN_MOVE_A0_SP && vec[move_idx] != 0x00410064U) || vec[move_idx + 5] != INSN_JIRL_RA_T0) {
        pr_err("lasx2lsx: exccode %d prologue/call mismatch (%08x / %08x)\n", exccode, vec[move_idx], vec[move_idx + 5]);
        return -EINVAL;
    }

    i0 = vec[move_idx + 1];
    i1 = vec[move_idx + 2];
    i2 = vec[move_idx + 3];
    i3 = vec[move_idx + 4];

    if ((i0 & 0xfe00001fU) != 0x1400000cU || (i1 & 0xffc003ffU) != 0x0380018cU || (i2 & 0xfe00001fU) != 0x1600000cU || (i3 & 0xffc003ffU) != 0x0300018cU) {
        pr_err("lasx2lsx: exccode %d t0 imm64 pattern mismatch (%08x %08x %08x %08x)\n", exccode, i0, i1, i2, i3);
        return -EINVAL;
    }

    addr = ((unsigned long)((i1 >> 10) & 0xfffU));
    addr |= ((unsigned long)((i0 >> 5) & 0xfffffU)) << 12;
    addr |= ((unsigned long)((i2 >> 5) & 0xfffffU)) << 32;
    addr |= ((unsigned long)((i3 >> 10) & 0xfffU)) << 52;

    encode_t0_imm64(addr, check);
    if (check[0] != i0 || check[1] != i1 || check[2] != i2 || check[3] != i3) {
        pr_err("lasx2lsx: exccode %d round-trip encode check failed\n", exccode);
        return -EINVAL;
    }

    if (orig_insns) {
        orig_insns[0] = i0;
        orig_insns[1] = i1;
        orig_insns[2] = i2;
        orig_insns[3] = i3;
    }
    *fn_out = addr;
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
static int read_kernel_insn(unsigned long pc, u32* insn)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    return copy_from_kernel_nofault(insn, (void*)pc, sizeof(*insn));
#else
    return probe_kernel_read(insn, (void*)pc, sizeof(*insn));
#endif
}

static unsigned long branch_target(unsigned long pc, u32 insn)
{
    u32 off = ((insn & 0x3ffU) << 16) | ((insn >> 10) & 0xffffU);
    return pc + sign_extend64(off, 25) * 4;
}

/* Reuse the exact entry/exit functions called by the installed kernel, without
 * requiring private symbol exports or KPROBES. Reject unknown compiler layouts.
 * The first BL follows the stack prologue; the exit is a tail B immediately
 * after restoring the same stack frame. Cross-check all hooked handlers. */
static int decode_irqentry(unsigned long fn, unsigned long* enter, unsigned long* exit)
{
    u32 insn, prev;
    int frame, i;

    if (read_kernel_insn(fn, &insn) || (insn & 0xffc003ffU) != 0x02c00063U)
        return -EINVAL;
    frame = -sign_extend32((insn >> 10) & 0xfffU, 11);
    if (frame <= 0 || (frame & 15))
        return -EINVAL;
    *enter = *exit = 0;
    for (i = 1; i < 32; i++) {
        if (read_kernel_insn(fn + i * 4, &insn))
            return -EFAULT;
        if ((insn & 0xfc000000U) == 0x54000000U) {
            *enter = branch_target(fn + i * 4, insn);
            break;
        }
    }
    prev = 0;
    for (i = 1; i < 512; i++) {
        if (read_kernel_insn(fn + i * 4, &insn))
            return -EFAULT;
        if ((prev & 0xffc003ffU) == 0x02c00063U && sign_extend32((prev >> 10) & 0xfffU, 11) == frame && (insn & 0xfc000000U) == 0x50000000U) {
            *exit = branch_target(fn + i * 4, insn);
            break;
        }
        prev = insn;
    }
    if (!*enter || !*exit || *enter == *exit)
        return -EINVAL;
#ifdef CONFIG_KALLSYMS
    {
        char symbol[KSYM_SYMBOL_LEN];
        sprint_symbol(symbol, *enter);
        if (strncmp(symbol, "irqentry_enter+0x0/", strlen("irqentry_enter+0x0/")))
            return -EINVAL;
        sprint_symbol(symbol, *exit);
        if (strncmp(symbol, "irqentry_exit+0x0/", strlen("irqentry_exit+0x0/")))
            return -EINVAL;
    }
#endif
    return 0;
}

static int init_irqentry(unsigned long ri, unsigned long lbt, unsigned long lasx)
{
    unsigned long enter, exit, other_enter, other_exit;

    if (decode_irqentry(ri, &enter, &exit) || decode_irqentry(lbt, &other_enter, &other_exit) || enter != other_enter || exit != other_exit ||
        (g_patch_lasxdis && (decode_irqentry(lasx, &other_enter, &other_exit) || enter != other_enter || exit != other_exit))) {
        pr_err("lasx2lsx: unsupported irqentry call layout\n");
        return -EINVAL;
    }
    kernel_irqentry_enter = (void*)enter;
    kernel_irqentry_exit = (void*)exit;
    return 0;
}
#endif

/* Read sanitized ASCII CPU model name from IOCSR 0x20 if available */
static void read_iocsr_cpu_model(char out[16])
{
    int i, w = 0;

    memset(out, 0, 16);
    if (!cpu_has_iocsr)
        return;

    {
        u64 raw = iocsr_read64(LOONGARCH_IOCSR_CPUNAME);
        char tmp[9];
        memcpy(tmp, &raw, 8);
        tmp[8] = '\0';
        for (i = 0; i < 8 && tmp[i] != '\0'; i++) {
            if (isprint((unsigned char)tmp[i]) && !isspace((unsigned char)tmp[i]))
                out[w++] = tmp[i];
        }
        out[w] = '\0';
    }
}

/* Gate module initialization strictly to 3B6000M / 2K3000 in production,
 * or explicit sim_3a6000=1 verification mode on LASX-capable hosts.
 */
static int lasx_check_platform(char model_out[16])
{
    u32 prid = boot_cpu_data.processor_id;
    bool is_la364;

    read_iocsr_cpu_model(model_out);

    if (!cpu_has_lsx) {
        pr_err("lasx2lsx: CPU lacks LSX (128-bit SIMD) support\n");
        return -ENODEV;
    }
    if (!cpu_has_lbt) {
        pr_err("lasx2lsx: CPU lacks LBT extension required by jiscr1 runtime\n");
        return -ENODEV;
    }

    if (sim_3a6000) {
        if (!cpu_has_lasx) {
            pr_err("lasx2lsx: sim_3a6000=1 is only valid on LASX-capable verification hosts\n");
            return -EINVAL;
        }
        if (target_comm[0] == '\0' || (target_comm[0] == '*' && target_comm[1] == '\0')) {
            pr_err("lasx2lsx: sim_3a6000=1 requires a non-empty target_comm filter to protect host services\n");
            return -EINVAL;
        }
        g_patch_lasxdis = true;
        return 0;
    }

    /* Production mode: never initialize on CPUs that already have hardware LASX */
    if (cpu_has_lasx) {
        pr_info("lasx2lsx: CPU '%s' (PRID 0x%08x) natively supports LASX; module only initializes on 3B6000M / 2K3000\n", model_out[0] ? model_out : "unknown", prid);
        return -ENODEV;
    }

    is_la364 = ((prid & PRID_COMP_MASK) == PRID_COMP_LOONGSON) && ((prid & PRID_SERIES_MASK) == PRID_SERIES_LA364);

    if (!is_la364 || (model_out[0] != '\0' && !strstr(model_out, "3B6000M") && !strstr(model_out, "2K3000"))) {
        pr_info("lasx2lsx: unsupported CPU '%s' (PRID 0x%08x); module only initializes on 3B6000M / 2K3000\n", model_out[0] ? model_out : "unknown", prid);
        return -ENODEV;
    }

    g_patch_lasxdis = false;
    return 0;
}

/* Lockless, noinstr-safe process filter:
 *   - On 3B6000M / 2K3000 (!g_patch_lasxdis): empty target_comm matches ALL user tasks.
 *   - On 3A6000 verification (g_patch_lasxdis): empty/"*" target_comm is always rejected.
 *     Match the process leader so renamed audio/codec threads stay in simulation.
 */
static __always_inline bool should_intercept_current(void)
{
    const char* p = target_comm;
    const char* c = g_patch_lasxdis ? current->group_leader->comm : current->comm;
    char first = READ_ONCE(p[0]);
    int i;

    if (first == '\0')
        return !g_patch_lasxdis;

    if (unlikely(first == '*' && READ_ONCE(p[1]) == '\0'))
        return !g_patch_lasxdis;

    for (i = 0; i < TASK_COMM_LEN - 1; i++) {
        char ch = READ_ONCE(p[i]);
        if (ch == '\0')
            return true;
        if (c[i] != ch)
            return false;
    }
    return true;
}

static struct lasx_mm_ctx* lookup_mm_ctx_rcu(struct mm_struct* mm)
{
    struct lasx_mm_ctx* mctx;

    if (!mm)
        return NULL;
    hash_for_each_possible_rcu(lasx_mm_hash, mctx, hnode, (unsigned long)mm)
    {
        if (mctx->mm == mm)
            return mctx;
    }
    return NULL;
}

static void free_mm_ctx_tctx_list(struct lasx_mm_ctx* mctx)
{
    struct lasx_tctx_node *node, *tmp;
    unsigned long flags;

    spin_lock_irqsave(&mctx->lock, flags);
    list_for_each_entry_safe(node, tmp, &mctx->tctx_list, list)
    {
        list_del(&node->list);
        kfree(node);
    }
    spin_unlock_irqrestore(&mctx->lock, flags);
}

static void lasx_mn_release(struct mmu_notifier* mn, struct mm_struct* mm)
{
    struct lasx_mm_ctx* mctx = container_of(mn, struct lasx_mm_ctx, mn);
    unsigned long flags;
    bool unhashed = false;

    (void)mm;
    spin_lock_irqsave(&lasx_mm_hash_lock, flags);
    if (!hlist_unhashed(&mctx->hnode)) {
        hash_del_rcu(&mctx->hnode);
        unhashed = true;
    }
    spin_unlock_irqrestore(&lasx_mm_hash_lock, flags);

    if (unhashed) {
        free_mm_ctx_tctx_list(mctx);
        kfree_rcu(mctx, rcu);
    }
}

static const struct mmu_notifier_ops lasx_mn_ops = {
    .release = lasx_mn_release,
};

#define LASX_RT_HINT_VA 0x50000000UL

static bool probe_rt_signature(unsigned long va)
{
    u8 sig[16];

    if (!va || va >= TASK_SIZE || (va & 0x3fffUL) != 0)
        return false;
    if (copy_from_user(sig, (const void __user*)va, sizeof(sig)) != 0)
        return false;
    return memcmp(sig, lasx_rt_blob, sizeof(sig)) == 0;
}

/* Detect an existing lasx_rt image inherited across fork() regardless of VA */
static unsigned long find_inherited_rt_base(struct mm_struct* mm)
{
    unsigned long cands[4];
    int n_cands = 0;
    int i;

    /* 1. Fast path: default hint address */
    if (probe_rt_signature(LASX_RT_HINT_VA))
        return LASX_RT_HINT_VA;

    /* 2. Check ancestor mm_structs in lasx_mm_hash */
    rcu_read_lock();
    {
        struct task_struct* p = current;
        int depth;
        for (depth = 0; depth < 4 && p; depth++) {
            struct task_struct* parent = rcu_dereference(p->real_parent);
            struct lasx_mm_ctx* pmctx;
            if (!parent || parent == p)
                break;
            pmctx = lookup_mm_ctx_rcu(READ_ONCE(parent->mm));
            if (pmctx && pmctx->rt_base && n_cands < (int)ARRAY_SIZE(cands))
                cands[n_cands++] = pmctx->rt_base;
            p = parent;
        }
    }
    rcu_read_unlock();

    for (i = 0; i < n_cands; i++) {
        if (probe_rt_signature(cands[i]))
            return cands[i];
    }

    /* 3. General VMA scan for anonymous R-X VMA of size LASX_RT_TEXT_SIZE */
    n_cands = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    mmap_read_lock(mm);
#else
    down_read(&mm->mmap_sem);
#endif
    {
        struct vm_area_struct* vma;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
        VMA_ITERATOR(vmi, mm, 0);
        for_each_vma(vmi, vma)
#else
        for (vma = mm->mmap; vma; vma = vma->vm_next)
#endif
        {
            if (!vma->vm_file && (vma->vm_flags & VM_EXEC) && (vma->vm_end - vma->vm_start) == LASX_RT_TEXT_SIZE) {
                cands[n_cands++] = vma->vm_start;
                if (n_cands >= (int)ARRAY_SIZE(cands))
                    break;
            }
        }
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
    mmap_read_unlock(mm);
#else
    up_read(&mm->mmap_sem);
#endif

    for (i = 0; i < n_cands; i++) {
        if (probe_rt_signature(cands[i]))
            return cands[i];
    }

    return 0;
}

/* Map embedded lasx_rt_blob into current->mm on first LASX trap, or adopt
 * an existing lasx_rt mapping inherited across fork().
 */
static struct lasx_mm_ctx* get_or_create_mm_ctx(bool allow_inject)
{
    struct mm_struct* mm = current->mm;
    struct lasx_mm_ctx* mctx;
    unsigned long rt_base = 0, data_va;
    bool adopted = false;
    size_t off;
    int ret;
    unsigned long flags;

    if (!mm)
        return ERR_PTR(-EINVAL);

    rcu_read_lock();
    mctx = lookup_mm_ctx_rcu(mm);
    rcu_read_unlock();
    if (mctx)
        return mctx;

    mutex_lock(&lasx_inject_mutex);
    rcu_read_lock();
    mctx = lookup_mm_ctx_rcu(mm);
    rcu_read_unlock();
    if (mctx) {
        mutex_unlock(&lasx_inject_mutex);
        return mctx;
    }

    rt_base = find_inherited_rt_base(mm);
    if (rt_base != 0)
        adopted = true;

    if (!adopted && !allow_inject) {
        mutex_unlock(&lasx_inject_mutex);
        return ERR_PTR(-ENOENT);
    }

    if (!adopted) {
        /* 1. Map [0, LASX_RT_TOTAL_SIZE) as R-X (preferring LASX_RT_HINT_VA) */
        rt_base = vm_mmap(NULL, LASX_RT_HINT_VA, LASX_RT_TOTAL_SIZE, PROT_READ | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, 0);
        if (IS_ERR_VALUE(rt_base)) {
            mutex_unlock(&lasx_inject_mutex);
            return ERR_PTR((long)rt_base);
        }

        /* 2. Replace [LASX_RT_TEXT_SIZE, LASX_RT_TOTAL_SIZE) with RW data/bss */
        data_va = vm_mmap(NULL, rt_base + LASX_RT_TEXT_SIZE, LASX_RT_TOTAL_SIZE - LASX_RT_TEXT_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, 0);
        if (IS_ERR_VALUE(data_va)) {
            vm_munmap(rt_base, LASX_RT_TOTAL_SIZE);
            mutex_unlock(&lasx_inject_mutex);
            return ERR_PTR((long)data_va);
        }

        /* 3. Populate runtime image via FOLL_FORCE|FOLL_WRITE (flushes I-cache) */
        for (off = 0; off < LASX_RT_FILE_SIZE;) {
            size_t chunk = min_t(size_t, 0x4000UL, LASX_RT_FILE_SIZE - off);
            int written = access_process_vm(current, rt_base + off, (void*)(lasx_rt_blob + off), (int)chunk, FOLL_FORCE | FOLL_WRITE);
            if (written != (int)chunk) {
                vm_munmap(rt_base, LASX_RT_TOTAL_SIZE);
                mutex_unlock(&lasx_inject_mutex);
                return ERR_PTR(-EFAULT);
            }
            off += chunk;
        }
        __asm__ volatile("ibar 0" ::: "memory");
    }

    mctx = kzalloc(sizeof(*mctx), GFP_KERNEL);
    if (!mctx) {
        if (!adopted)
            vm_munmap(rt_base, LASX_RT_TOTAL_SIZE);
        mutex_unlock(&lasx_inject_mutex);
        return ERR_PTR(-ENOMEM);
    }

    mctx->mm = mm;
    mctx->rt_base = rt_base;
    spin_lock_init(&mctx->lock);
    INIT_LIST_HEAD(&mctx->tctx_list);
    mctx->mn.ops = &lasx_mn_ops;

    ret = mmu_notifier_register(&mctx->mn, mm);
    if (ret) {
        kfree(mctx);
        if (!adopted)
            vm_munmap(rt_base, LASX_RT_TOTAL_SIZE);
        mutex_unlock(&lasx_inject_mutex);
        return ERR_PTR(ret);
    }
    mctx->mn_registered = true;

    spin_lock_irqsave(&lasx_mm_hash_lock, flags);
    hash_add_rcu(lasx_mm_hash, &mctx->hnode, (unsigned long)mm);
    spin_unlock_irqrestore(&lasx_mm_hash_lock, flags);

    atomic_long_inc(&cnt_mm_injections);
    mutex_unlock(&lasx_inject_mutex);
    return mctx;
}

static bool is_tctx_owner_alive(struct mm_struct* mm, pid_t nr, u64 stime)
{
    struct pid* pid_struct;
    struct task_struct* task;
    bool alive = false;

    pid_struct = find_get_pid(nr);
    if (!pid_struct)
        return false;

    rcu_read_lock();
    task = pid_task(pid_struct, PIDTYPE_PID);
    if (task && !(task->flags & PF_EXITING) && READ_ONCE(task->mm) == mm && task->start_time == stime) {
        alive = true;
    }
    rcu_read_unlock();
    put_pid(pid_struct);
    return alive;
}

static int init_user_tctx(unsigned long uaddr, unsigned long rt_base, bool clear_td)
{
    struct lasx_trap_frame __user* utf = (struct lasx_trap_frame __user*)uaddr;

    if (clear_td) {
        if (clear_user((void __user*)(uaddr + LASX_TD_OFFSET), LASX_TD_CLEAR_SIZE) != 0)
            return -EFAULT;
    }

    if (put_user((u64)LASX_TCTX_MAGIC, &utf->magic) || put_user((u64)rt_base, &utf->rt_base) || put_user((u64)LASX_RT_TEXT_SIZE, &utf->rt_text_len) || put_user((u32)LASX_IN_RT_NONE, &utf->in_rt) ||
        put_user((u32)0, &utf->trap_count)) {
        return -EFAULT;
    }
    return 0;
}

/* Look up, recycle from exited thread, or allocate 64KB per-thread context */
static unsigned long get_or_create_tctx(struct lasx_mm_ctx* mctx)
{
    pid_t pid = task_pid_nr(current);
    u64 stime = current->start_time;
    struct lasx_tctx_node *node, *reclaim = NULL;
    unsigned long uaddr = 0;
    unsigned long flags;

    spin_lock_irqsave(&mctx->lock, flags);
    list_for_each_entry(node, &mctx->tctx_list, list)
    {
        if (node->pid == pid && node->start_time == stime) {
            uaddr = node->uaddr;
            break;
        }
    }
    spin_unlock_irqrestore(&mctx->lock, flags);

    if (uaddr)
        return uaddr;

    mutex_lock(&lasx_inject_mutex);

    /* Re-check under mutex and collect up to 8 candidate slots to test liveness */
    {
        struct lasx_tctx_node* cands[8];
        pid_t cand_pids[8];
        u64 cand_stimes[8];
        int n_cands = 0, i;

        spin_lock_irqsave(&mctx->lock, flags);
        list_for_each_entry(node, &mctx->tctx_list, list)
        {
            if (node->pid == pid && node->start_time == stime) {
                uaddr = node->uaddr;
                break;
            }
            if (n_cands < (int)ARRAY_SIZE(cands)) {
                cands[n_cands] = node;
                cand_pids[n_cands] = node->pid;
                cand_stimes[n_cands] = node->start_time;
                n_cands++;
            }
        }
        spin_unlock_irqrestore(&mctx->lock, flags);

        if (uaddr) {
            mutex_unlock(&lasx_inject_mutex);
            return uaddr;
        }

        for (i = 0; i < n_cands; i++) {
            if (!is_tctx_owner_alive(mctx->mm, cand_pids[i], cand_stimes[i])) {
                reclaim = cands[i];
                break;
            }
        }
    }

    if (reclaim) {
        if (init_user_tctx(reclaim->uaddr, mctx->rt_base, true) == 0) {
            spin_lock_irqsave(&mctx->lock, flags);
            reclaim->pid = pid;
            reclaim->start_time = stime;
            uaddr = reclaim->uaddr;
            spin_unlock_irqrestore(&mctx->lock, flags);

            atomic_long_inc(&cnt_tctx_reused);
            mutex_unlock(&lasx_inject_mutex);
            return uaddr;
        }
    }

    uaddr = vm_mmap(NULL, 0, LASX_TCTX_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0);
    if (IS_ERR_VALUE(uaddr)) {
        mutex_unlock(&lasx_inject_mutex);
        return uaddr;
    }

    if (init_user_tctx(uaddr, mctx->rt_base, false) != 0) {
        vm_munmap(uaddr, LASX_TCTX_SIZE);
        mutex_unlock(&lasx_inject_mutex);
        return (unsigned long)-EFAULT;
    }

    node = kmalloc(sizeof(*node), GFP_KERNEL);
    if (!node) {
        vm_munmap(uaddr, LASX_TCTX_SIZE);
        mutex_unlock(&lasx_inject_mutex);
        return (unsigned long)-ENOMEM;
    }
    node->pid = pid;
    node->start_time = stime;
    node->uaddr = uaddr;

    spin_lock_irqsave(&mctx->lock, flags);
    list_add(&node->list, &mctx->tctx_list);
    spin_unlock_irqrestore(&mctx->lock, flags);

    atomic_long_inc(&cnt_tctx_allocs);
    mutex_unlock(&lasx_inject_mutex);
    return uaddr;
}

/* Exact LoongArch LASX 256-bit opcode recognizer */
static bool is_lasx_insn(u32 instr)
{
    u32 top8 = instr >> 24;

    if (top8 >= 0x74 && top8 <= 0x77)
        return true;
    if (top8 == 0x0a)
        return true;
    if (top8 == 0x0c)
        return ((instr >> 23) & 1U) != 0;
    if (top8 == 0x0d) {
        u32 lo = (instr >> 20) & 7U;
        return (lo == 2U || lo == 6U);
    }
    if (top8 == 0x2c)
        return ((instr >> 23) & 1U) != 0;
    if (top8 == 0x32 || top8 == 0x33)
        return true;
    /* XVLDX (0x7090) and XVSTX (0x7098) */
    if ((instr >> 15) == 0x7090U || (instr >> 15) == 0x7098U)
        return true;
    return false;
}

static inline bool is_jiscr1_insn(u32 instr) { return (instr & 0xfc0003e0U) == 0x48000300U; }

static int redirect_to_lasx_rt(struct pt_regs* regs, struct lasx_mm_ctx* mctx, unsigned long tctx_uaddr, u32 trap_kind)
{
    struct lasx_trap_frame __user* utf = (struct lasx_trap_frame __user*)tctx_uaddr;
    u32 mflags = (u32)atomic_read(&g_mode_flags);

    if (put_user((u64)regs->csr_era, &utf->orig_pc) || put_user((u64)regs->regs[3], &utf->orig_sp) || put_user((u64)regs->regs[4], &utf->orig_a0) || put_user((u64)regs->regs[1], &utf->orig_ra) ||
        put_user(mflags, &utf->mode_flags) || put_user(trap_kind, &utf->trap_kind) || put_user((u32)LASX_IN_RT_ACTIVE, &utf->in_rt))
        return -EFAULT;

    regs->regs[4] = tctx_uaddr;
    regs->regs[3] = tctx_uaddr + LASX_STACK_TOP_OFF;
    regs->csr_era = mctx->rt_base;
    return 0;
}

enum lasx_trap_result {
    LASX_TRAP_HANDLED,
    LASX_TRAP_NATIVE,
    LASX_TRAP_RI,
};

/* Called with IRQs enabled, outside the atomic die-notifier chain. */
static enum lasx_trap_result lasx_handle_trap(struct pt_regs* regs, int trapnr)
{
    struct lasx_mm_ctx* mctx;
    unsigned long tctx_uaddr;
    u32 insn, in_rt = LASX_IN_RT_NONE;
    bool named = should_intercept_current();
    bool enabled = READ_ONCE(enable_runtime);

    if (irqs_disabled())
        return LASX_TRAP_RI;
    current->thread.trap_nr = trapnr;
    if (get_user(insn, (u32 __user*)regs->csr_era))
        return LASX_TRAP_RI;

    /* Native LBT instructions, including runtime register binding, need the
     * original hardware handler. Only external jiscr1 sites need a context. */
    if (trapnr == EXCCODE_BTDIS && !is_jiscr1_insn(insn))
        return LASX_TRAP_NATIVE;

    rcu_read_lock();
    mctx = lookup_mm_ctx_rcu(current->mm);
    rcu_read_unlock();

    if (!mctx && !named) {
        mctx = get_or_create_mm_ctx(false);
        if (IS_ERR(mctx)) {
            if (PTR_ERR(mctx) == -ENOENT)
                return trapnr == EXCCODE_INE ? LASX_TRAP_RI : LASX_TRAP_NATIVE;
            return LASX_TRAP_RI;
        }
    }

    if (trapnr == EXCCODE_INE && regs->regs[11] == LASX_HCALL_MAGIC && insn == LASX_HCALL_INSN) {
        unsigned long target_pc = regs->regs[4];
        u32 new_insn = (u32)regs->regs[5];
        int written = -EFAULT;

        if ((target_pc & 3UL) == 0 && target_pc < TASK_SIZE)
            written = access_process_vm(current, target_pc, &new_insn, sizeof(new_insn), FOLL_FORCE | FOLL_WRITE);
        regs->regs[4] = written == sizeof(new_insn) ? 0UL : (unsigned long)-EFAULT;
        regs->csr_era += 4;
        if (written == sizeof(new_insn))
            atomic_long_inc(&cnt_hcall_patches);
        return LASX_TRAP_HANDLED;
    }

    if (trapnr == EXCCODE_LASXDIS)
        atomic_long_inc(&cnt_lasx_intercepted);
    /* Existing jiscr1 patches still need SCR bindings after injection stops. */
    if (!enabled && trapnr != EXCCODE_BTDIS) {
        if (trapnr == EXCCODE_INE && is_lasx_insn(insn))
            atomic_long_inc(&cnt_lasx_intercepted);
        return LASX_TRAP_RI;
    }
    if (!is_lasx_insn(insn) && !is_jiscr1_insn(insn)) {
        atomic_long_inc(&cnt_sigill_passthrough);
        return LASX_TRAP_RI;
    }
    if (trapnr == EXCCODE_INE && is_lasx_insn(insn))
        atomic_long_inc(&cnt_lasx_intercepted);

    if (!mctx)
        mctx = get_or_create_mm_ctx(trapnr != EXCCODE_BTDIS && named);
    if (IS_ERR(mctx))
        return trapnr == EXCCODE_BTDIS && PTR_ERR(mctx) == -ENOENT ? LASX_TRAP_NATIVE : LASX_TRAP_RI;

    if (regs->csr_era >= mctx->rt_base && regs->csr_era < mctx->rt_base + LASX_RT_TOTAL_SIZE)
        return trapnr == EXCCODE_BTDIS ? LASX_TRAP_NATIVE : LASX_TRAP_RI;

    tctx_uaddr = get_or_create_tctx(mctx);
    if (IS_ERR_VALUE(tctx_uaddr))
        return LASX_TRAP_RI;

    if (trapnr != EXCCODE_BTDIS && get_user(in_rt, &((struct lasx_trap_frame __user*)tctx_uaddr)->in_rt) == 0) {
        if (in_rt == LASX_IN_RT_PASSTHRU) {
            put_user((u32)LASX_IN_RT_NONE, &((struct lasx_trap_frame __user*)tctx_uaddr)->in_rt);
            atomic_long_inc(&cnt_sigill_passthrough);
            return LASX_TRAP_RI;
        }
        if (in_rt == LASX_IN_RT_ACTIVE)
            return LASX_TRAP_RI;
    }

    if (redirect_to_lasx_rt(regs, mctx, tctx_uaddr, trapnr == EXCCODE_BTDIS ? LASX_TRAP_LBT_INIT : LASX_TRAP_LASX_INSN))
        return LASX_TRAP_RI;
    atomic_long_inc(trapnr == EXCCODE_BTDIS ? &cnt_lbt_inits : &cnt_rt_upcalls);
    return LASX_TRAP_HANDLED;
}

/* Modern vector stubs enter with IRQs off and context tracking still in user
 * mode. Establish the same boundary as the original C handlers before sleeping.
 * On 4.19 the vector stub has already established the sleepable context. */
static __always_inline enum lasx_trap_result lasx_user_trap(struct pt_regs* regs, int trapnr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    irqentry_state_t state = kernel_irqentry_enter(regs);
    enum lasx_trap_result result;
    unsigned long resume_pc;

    instrumentation_begin();
    local_irq_enable();
    result = lasx_handle_trap(regs, trapnr);
    resume_pc = regs->csr_era;
    local_irq_disable();
    instrumentation_end();
    kernel_irqentry_exit(regs, state);

    /* Exit may have delivered a pending signal. Do not pass that signal
     * handler's PC to the original handler for the old exception. Sigreturn
     * will retry the original instruction if it still needs native handling. */
    if (regs->csr_era != resume_pc)
        return LASX_TRAP_HANDLED;
    return result;
#else
    return lasx_handle_trap(regs, trapnr);
#endif
}

static asmlinkage void noinstr my_do_ri(struct pt_regs* regs)
{
    struct lasx_active_trap active;
    lasx_trap_enter(&active);
    if (!user_mode(regs) || lasx_user_trap(regs, EXCCODE_INE) != LASX_TRAP_HANDLED)
        orig_do_ri(regs);
    lasx_trap_exit(&active);
}

static asmlinkage void noinstr my_do_lasx(struct pt_regs* regs)
{
    enum lasx_trap_result result;

    struct lasx_active_trap active;
    lasx_trap_enter(&active);
    result = user_mode(regs) ? lasx_user_trap(regs, EXCCODE_LASXDIS) : LASX_TRAP_NATIVE;
    if (result == LASX_TRAP_NATIVE) {
        lasx_noinstr_atomic_inc(&cnt_lasx_passthrough);
        orig_do_lasx(regs);
    } else if (result == LASX_TRAP_RI) {
        orig_do_ri(regs);
    }
    lasx_trap_exit(&active);
}

static asmlinkage void noinstr my_do_lbt(struct pt_regs* regs)
{
    enum lasx_trap_result result;

    struct lasx_active_trap active;
    lasx_trap_enter(&active);
    result = user_mode(regs) ? lasx_user_trap(regs, EXCCODE_BTDIS) : LASX_TRAP_NATIVE;
    if (result == LASX_TRAP_NATIVE)
        orig_do_lbt(regs);
    else if (result == LASX_TRAP_RI)
        orig_do_ri(regs);
    lasx_trap_exit(&active);
}

struct cpu_verify_ctx {
    atomic_t err_count;
    unsigned long fn_ri;
    unsigned long fn_lasx;
    unsigned long fn_lbt;
};

static void verify_eentry_on_cpu(void* arg)
{
    struct cpu_verify_ctx* vctx = arg;
    unsigned long eentry = csr_read64(LOONGARCH_CSR_EENTRY);
    unsigned long fn_ri = 0, fn_lasx = 0, fn_lbt = 0;

    if (!eentry || decode_and_verify_vec_target(eentry, EXCCODE_INE, &fn_ri, NULL) != 0 || decode_and_verify_vec_target(eentry, EXCCODE_BTDIS, &fn_lbt, NULL) != 0 || fn_ri != vctx->fn_ri ||
        fn_lbt != vctx->fn_lbt) {
        atomic_inc(&vctx->err_count);
        return;
    }

    if (g_patch_lasxdis) {
        if (decode_and_verify_vec_target(eentry, EXCCODE_LASXDIS, &fn_lasx, NULL) != 0 || fn_lasx != vctx->fn_lasx) {
            atomic_inc(&vctx->err_count);
        }
    }
}

struct sm_patch_req {
    bool patch_lasx;
    u32 lasx_insns[4];
    u32 lbt_insns[4];
    u32 ri_insns[4];
};

static void write_local_eentry_vectors(const struct sm_patch_req* req)
{
    unsigned long eentry = csr_read64(LOONGARCH_CSR_EENTRY);
    u32* vec_lbt = (u32*)(eentry + (unsigned long)EXCCODE_BTDIS * VEC_SLOT_STRIDE);
    int i;

    u32* vec_ri = (u32*)(eentry + (unsigned long)EXCCODE_INE * VEC_SLOT_STRIDE);
    for (i = 0; i < 4; i++)
        WRITE_ONCE(vec_ri[vec_move_idx(EXCCODE_INE) + 1 + i], req->ri_insns[i]);
    for (i = 0; i < 4; i++)
        WRITE_ONCE(vec_lbt[vec_move_idx(EXCCODE_BTDIS) + 1 + i], req->lbt_insns[i]);

    if (req->patch_lasx) {
        u32* vec_lasx = (u32*)(eentry + (unsigned long)EXCCODE_LASXDIS * VEC_SLOT_STRIDE);
        for (i = 0; i < 4; i++)
            WRITE_ONCE(vec_lasx[vec_move_idx(EXCCODE_LASXDIS) + 1 + i], req->lasx_insns[i]);
    }

    __asm__ volatile("dbar 0\n\tibar 0" ::: "memory");
}

static int apply_vector_patch_sm(void* arg)
{
    write_local_eentry_vectors((const struct sm_patch_req*)arg);
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
static void lasx_sync_trap_returns(void* unused) { (void)unused; }
#endif

/* Re-apply per-CPU CSR_EENTRY patch when a CPU comes online (suspend/resume or hotplug) */
static int lasx_cpu_online(unsigned int cpu)
{
    unsigned long eentry = csr_read64(LOONGARCH_CSR_EENTRY);
    unsigned long fn_lbt = 0, fn_lasx = 0;
    unsigned long fn_ri = 0;
    unsigned long flags;
    struct sm_patch_req req;

    (void)cpu;
    if (!eentry)
        return 0;

    if (decode_and_verify_vec_target(eentry, EXCCODE_INE, &fn_ri, NULL) != 0 ||
        (fn_ri != (unsigned long)orig_do_ri && fn_ri != (unsigned long)my_do_ri))
        return 0;
    if (decode_and_verify_vec_target(eentry, EXCCODE_BTDIS, &fn_lbt, NULL) != 0)
        return 0;
    if (fn_lbt == (unsigned long)my_do_lbt && (!g_patch_lasxdis))
        return 0;
    if (g_patch_lasxdis && decode_and_verify_vec_target(eentry, EXCCODE_LASXDIS, &fn_lasx, NULL) != 0)
        return 0;

    req.patch_lasx = g_patch_lasxdis;
    memcpy(req.lasx_insns, patched_lasx_insns, sizeof(patched_lasx_insns));
    memcpy(req.lbt_insns, patched_lbt_insns, sizeof(patched_lbt_insns));
    memcpy(req.ri_insns, patched_ri_insns, sizeof(patched_ri_insns));

    local_irq_save(flags);
    write_local_eentry_vectors(&req);
    local_irq_restore(flags);
    return 0;
}

static int __init lasx2lsx_kmod_init(void)
{
    char cpu_model[16];
    unsigned long local_eentry;
    unsigned long fn_ri = 0, fn_lasx = 0, fn_lbt = 0;
    struct cpu_verify_ctx vctx;
    struct sm_patch_req req;
    int ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0) && defined(CONFIG_PREEMPT)
    pr_err("lasx2lsx: preemptible 4.19 kernels are not supported\n");
    return -EOPNOTSUPP;
#endif
    ret = lasx_check_platform(cpu_model);
    if (ret)
        return ret;

    local_eentry = csr_read64(LOONGARCH_CSR_EENTRY);
    if (!local_eentry) {
        pr_err("lasx2lsx: CSR_EENTRY is NULL\n");
        return -EINVAL;
    }

    ret = decode_and_verify_vec_target(local_eentry, EXCCODE_INE, &fn_ri, saved_ri_insns);
    if (ret)
        return ret;
    ret = decode_and_verify_vec_target(local_eentry, EXCCODE_BTDIS, &fn_lbt, saved_lbt_insns);
    if (ret)
        return ret;
    if (g_patch_lasxdis) {
        ret = decode_and_verify_vec_target(local_eentry, EXCCODE_LASXDIS, &fn_lasx, saved_lasx_insns);
        if (ret)
            return ret;
    }

    atomic_set(&vctx.err_count, 0);
    vctx.fn_ri = fn_ri;
    vctx.fn_lasx = fn_lasx;
    vctx.fn_lbt = fn_lbt;
    on_each_cpu(verify_eentry_on_cpu, &vctx, 1);
    if (atomic_read(&vctx.err_count) != 0) {
        pr_err("lasx2lsx: per-CPU CSR_EENTRY verification failed\n");
        return -EINVAL;
    }

    orig_do_ri = (loongarch_exc_fn_t)fn_ri;
    orig_do_lasx = (loongarch_exc_fn_t)fn_lasx;
    orig_do_lbt = (loongarch_exc_fn_t)fn_lbt;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    ret = init_irqentry(fn_ri, fn_lbt, fn_lasx);
    if (ret)
        return ret;
    ret = register_task_exit();
    if (ret)
        return ret;
#endif
    encode_t0_imm64((unsigned long)my_do_ri, patched_ri_insns);

    encode_t0_imm64((unsigned long)my_do_lasx, patched_lasx_insns);
    encode_t0_imm64((unsigned long)my_do_lbt, patched_lbt_insns);

    req.patch_lasx = g_patch_lasxdis;
    memcpy(req.lasx_insns, patched_lasx_insns, sizeof(patched_lasx_insns));
    memcpy(req.lbt_insns, patched_lbt_insns, sizeof(patched_lbt_insns));
    memcpy(req.ri_insns, patched_ri_insns, sizeof(patched_ri_insns));

    ret = stop_machine(apply_vector_patch_sm, &req, cpu_online_mask);
    if (ret) {
        pr_err("lasx2lsx: stop_machine vector patch failed: %d\n", ret);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
        unregister_task_exit();
#endif
        return ret;
    }

    ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "loongarch/lasx2lsx:online", lasx_cpu_online, NULL);
    if (ret > 0) {
        g_cpuhp_state = (enum cpuhp_state)ret;
        g_cpuhp_registered = true;
    }

    pr_info("lasx2lsx: initialized on %s (%u CPUs, mode=%s, target_comm='%s', rt=%luKB)\n", cpu_model[0] ? cpu_model : "LA364", num_online_cpus(), g_patch_lasxdis ? "3A6000-sim" : "3B6000M/2K3000",
            target_comm[0] ? target_comm : "<all>", LASX_RT_FILE_SIZE / 1024UL);
    return 0;
}

static void __exit lasx2lsx_kmod_exit(void)
{
    struct sm_patch_req req;
    struct lasx_mm_ctx* mctx;
    struct hlist_node* tmp;
    int bkt;

    if (g_cpuhp_registered) {
        cpuhp_remove_state_nocalls(g_cpuhp_state);
        g_cpuhp_registered = false;
    }

    req.patch_lasx = g_patch_lasxdis;
    memcpy(req.lasx_insns, saved_lasx_insns, sizeof(saved_lasx_insns));
    memcpy(req.lbt_insns, saved_lbt_insns, sizeof(saved_lbt_insns));
    memcpy(req.ri_insns, saved_ri_insns, sizeof(saved_ri_insns));

    stop_machine(apply_vector_patch_sm, &req, cpu_online_mask);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
    /* No new wrappers can start; wait for sleeping handlers and their returns. */
    wait_event(active_traps_wait, atomic_read(&active_traps) == 0);
    synchronize_sched();
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
    /* Wrappers leave with IRQs off, so no new entry can slip past the counter
     * after stop_machine restores the vectors. Their cores may have slept. */
    while (atomic_read(&active_traps))
        msleep(1);
    unregister_task_exit();
    /* A CPU may still be executing the IRQ-off epilogue after decrementing.
     * A synchronous IPI cannot run there until it has left the wrapper. */
    on_each_cpu(lasx_sync_trap_returns, NULL, 1);
#endif

    /* Unregister mmu_notifiers for any still-running processes */
    for (;;) {
        struct lasx_mm_ctx* victim = NULL;
        unsigned long flags;

        spin_lock_irqsave(&lasx_mm_hash_lock, flags);
        hash_for_each_safe(lasx_mm_hash, bkt, tmp, mctx, hnode)
        {
            hash_del_rcu(&mctx->hnode);
            victim = mctx;
            break;
        }
        spin_unlock_irqrestore(&lasx_mm_hash_lock, flags);

        if (!victim)
            break;

        if (victim->mn_registered && mmget_not_zero(victim->mm)) {
            mmu_notifier_unregister(&victim->mn, victim->mm);
            mmput(victim->mm);
        }
        free_mm_ctx_tctx_list(victim);
        kfree_rcu(victim, rcu);
    }
    rcu_barrier();

    pr_info("lasx2lsx: unloaded (intercepted=%ld, upcalls=%ld, lbt_inits=%ld, mm=%ld, tctx=%ld/%ld)\n", atomic_long_read(&cnt_lasx_intercepted), atomic_long_read(&cnt_rt_upcalls),
            atomic_long_read(&cnt_lbt_inits), atomic_long_read(&cnt_mm_injections), atomic_long_read(&cnt_tctx_allocs), atomic_long_read(&cnt_tctx_reused));
}

module_init(lasx2lsx_kmod_init);
module_exit(lasx2lsx_kmod_exit);
