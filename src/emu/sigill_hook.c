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

#define __USE_MISC
#define __USE_GNU
#define _GNU_SOURCE
#include <dlfcn.h>
#include "debug.h"
#include "lasx_emu.h"
#include "lasx_emu_private.h"
#include "perf_stats.h"
#include "thread_data.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <ucontext.h>
#include <string.h>
#include <unistd.h>
#include "lasx_interpret.h"
#include <larchintrin.h>

int use_lsx_intrinsics;
#ifdef LASX_PROFILE
int lasx_profile_mode;
#endif
int lasx_interpret_mode;
int lasx_interpret_frag_opt;
// 保存原始的 SIGILL 处理函数
static struct sigaction original_sigill_action;

__thread bool is_lasx = 0;
// 自定义 SIGILL 处理函数
uint64_t tp_offset;
void sigill_handler(int sig, siginfo_t *info, void *context) {
  (void)sig;
  (void)info;

  ucontext_t *ucontext = (ucontext_t *)context;

  // 获取当前触发异常的指令地址
#if defined(__loongarch__)
  unsigned long pc_val = ucontext->uc_mcontext.__pc;
  unsigned int instr = *(unsigned int *)pc_val;
  
  if ((instr & 0x48000300) == 0x48000300) {
      //重新执行jiscr1
      is_lasx = true;
      return;
  }

  if (!tp_offset && getpid() == syscall(SYS_gettid)) {
      tp_offset = (uint64_t)thread_data_get() - (uint64_t)(UC_GPR(ucontext, 2));
  }

  if (lasx_emu_create_interpret_fragment(ucontext)) { is_lasx = true; return; }

  if (lasx_emu_create_interpret_block(ucontext)) { is_lasx = true; return; }

  if(lasx_emu_create_interpret(ucontext, instr)) {
      is_lasx = true;
      return;
  }
  int ret = do_lasx_emu(ucontext, instr);

  if (ret) {
    is_lasx = true;
    ucontext->uc_mcontext.__pc += 4;
  } else {
    // 打印指令地址和指令码
    tdlog("\n[SIGILL HOOK] 捕获到非法指令异常！\n");
    tdlog("  指令地址: %p\n", (void *)pc_val);
    tdlog("  指令码: 0x%08x\n", instr);
    tdlog("  跳过该指令继续执行...\n\n");
    signal(SIGILL, SIG_DFL); // 恢复默认
    return;                  // 返回后重新执行非法指令，进程终止
  }
#else
  // On non-LoongArch host compilation, just exit - this library isn't useful
  // here anyway
  (void)ucontext;
  tdlog("\n[SIGILL HOOK] This library should only be used on LoongArch "
        "systems.\n\n");
  signal(SIGILL, SIG_DFL);
  return;
#endif
}

// 保存目标程序注册的动作
static struct sigaction target_act = {0};
static int target_registered = 0;

// 原始 libc 函数指针
typedef sighandler_t (*signal_t)(int, sighandler_t);
typedef int (*sigaction_t)(int, const struct sigaction *, struct sigaction *);
static signal_t original_signal = NULL;
static sigaction_t original_sigaction = NULL;

// 真实的 sigaction（绕过劫持，直接系统调用）
static int real_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    return syscall(SYS_rt_sigaction, signum, act, oldact, _NSIG / 8);
}

// Wrapper 函数：被内核真正调用
static void wrapper_handler(int sig, siginfo_t *info, void *context) {
    if (sig == SIGILL) {
        sigill_handler(sig, info, context);
    }

    if (is_lasx) {
        is_lasx = false;
        return;
    }
    if (target_registered) {
        if (target_act.sa_flags & SA_SIGINFO) {
            if (target_act.sa_sigaction) {
                target_act.sa_sigaction(sig, info, context);
            }
        } else {
            if (target_act.sa_handler && target_act.sa_handler != SIG_DFL && target_act.sa_handler != SIG_IGN) {
                target_act.sa_handler(sig);
            }
        }
    }
}

// 安装 wrapper（SA_SIGINFO 风格）
static void install_wrapper(void) {
    struct sigaction sa = {
        .sa_sigaction = wrapper_handler,
        .sa_flags = SA_SIGINFO,
    };
    sigemptyset(&sa.sa_mask);
    if (target_registered) {
        sa.sa_flags |= (target_act.sa_flags & ~SA_SIGINFO);
        // 简化：不合并 mask，因为多数情况下无影响
    }
    real_sigaction(SIGILL, &sa, NULL);
}

// 劫持 signal
sighandler_t signal(int signum, sighandler_t handler) {
    if (signum == SIGILL) {
        if (handler == SIG_ERR) return SIG_ERR;

        sighandler_t old = SIG_DFL;
        if (target_registered) {
            old = (target_act.sa_flags & SA_SIGINFO) ?
                  (sighandler_t)target_act.sa_sigaction : target_act.sa_handler;
        } else {
            old = (sighandler_t)sigill_handler;
        }

        memset(&target_act, 0, sizeof(target_act));
        target_act.sa_handler = handler;
        target_act.sa_flags = 0;
        sigemptyset(&target_act.sa_mask);
        target_registered = 1;

        install_wrapper();
        return old;
    }
    return original_signal(signum, handler);
}

// 劫持 sigaction
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (signum == SIGILL && act != NULL) {
        if (oldact) {
            if (target_registered) {
                memcpy(oldact, &target_act, sizeof(struct sigaction));
            } else {
                memset(oldact, 0, sizeof(struct sigaction));
                oldact->sa_sigaction = sigill_handler;
                oldact->sa_flags = SA_SIGINFO;
                sigemptyset(&oldact->sa_mask);
            }
        }

        memcpy(&target_act, act, sizeof(struct sigaction));
        target_registered = 1;

        install_wrapper();
        return 0;
    }

    return original_sigaction(signum, act, oldact);
}


// 析构函数：程序退出时打印最终统计
__attribute__((destructor)) void cleanup_sigill_handler(void) {
  // 只有在 LD_PRELOAD 环境下才清理
  if (getenv("LD_PRELOAD") == NULL) {
    return;
  }

  // 打印最终性能统计（仅在启用时）
  if (perf_stats_enabled()) {
    perf_stats_print();
    perf_stats_cleanup();
  }
}

#ifdef LASX_PROFILE
#include "lasx_profile.h"
void profile_handler(int sig, siginfo_t *info, void *context)
{
    lasx_profile_dump_periodic();
}
#endif

// 构造函数：程序启动前自动执行，注册信号处理函数
__attribute__((constructor)) void register_sigill_handler(void) {
  struct sigaction sa;

  // 只有在 LD_PRELOAD 环境下才注册（避免重复注册）
  if (getenv("LD_PRELOAD") == NULL) {
    return;
  }

  use_lsx_intrinsics = 1;
  
  const char* env = getenv("DISABLE_LSX_INTRINSICS");
  if (env && atoi(env) != 0) {
      use_lsx_intrinsics = 0;
  }
  const bool has_lsx = __cpucfg(2) & 0b01000000;
  if (!has_lsx) {
    use_lsx_intrinsics = 0;
  }
  // 初始化性能统计
  perf_stats_init();

  const char *v = NULL;

#ifdef LASX_PROFILE
  v = getenv("LIBLASX2LSX_PROFILE");
  if (v) {
    char buf[128];
    strncpy(buf, v, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *token = strtok(buf, ",|;");
    while (token) {
      while (*token == ' ') token++;
      if (strncmp(token, "thread", 6) == 0)
        lasx_profile_mode |= LASX_PROFILE_THREAD;
      else if (strncmp(token, "signal", 6) == 0)
        lasx_profile_mode |= LASX_PROFILE_SIGNAL;
      else if (strncmp(token, "jit", 3) == 0)
        lasx_profile_mode |= LASX_PROFILE_JIT;
      token = strtok(NULL, ",|;");
    }
  } else {
      tdlog("interpret disabled.\n");
  }
#endif

  v = getenv("LIBLASX2LSX_INTERPRET_MODE");
  lasx_interpret_mode = 0;
  lasx_interpret_frag_opt = 0;
  if (v) {
    char buf[128];
    strncpy(buf, v, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *token = strtok(buf, ",|;");
    while (token) {
      while (*token == ' ') token++;
      if (strncmp(token, "block", 5) == 0)
        lasx_interpret_mode |= LASX_MODE_BLOCK;
      else if (strncmp(token, "loop", 4) == 0)
        lasx_interpret_mode |= LASX_MODE_LOOP;
      else if (strncmp(token, "usedef", 6) == 0)
        lasx_interpret_mode |= LASX_MODE_USEDEF;
      else if (strncmp(token, "frag", 4) == 0)
        lasx_interpret_frag_opt = 1;
      else if (strncmp(token, "blockloop", 9) == 0) {
        /* backward compat: blockloop = block + loop */
        lasx_interpret_mode |= LASX_MODE_BLOCK | LASX_MODE_LOOP;
      } else if (strncmp(token, "one", 3) == 0 || strncmp(token, "emu", 3) == 0) {
        lasx_interpret_mode = 0;
        lasx_interpret_frag_opt = 0;
      }
      token = strtok(NULL, ",|;");
    }
    /* block and usedef are mutually exclusive: usedef takes priority */
    if ((lasx_interpret_mode & LASX_MODE_BLOCK) && (lasx_interpret_mode & LASX_MODE_USEDEF)) {
      lasx_interpret_mode &= ~LASX_MODE_BLOCK;
    }
  } else {
    lasx_interpret_mode = LASX_MODE_USEDEF | LASX_MODE_LOOP;
    lasx_interpret_frag_opt = 1;
    tdlog("default interpret mode : usedef,loop,frag\n");
  }

  v = getenv("LIBLASX2LSX_INTERPRET");
  if (v && strncmp(v, "off", 3) == 0) {
      tdlog("interpret disabled.\n");
  } else {
      lasx_init_interpret();
  }

  // printf("[LD_PRELOAD] libsigill_hook.so 已加载，正在注册 SIGILL
  // 处理函数...\n");

  // 设置 sigaction
  sa.sa_sigaction = sigill_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_SIGINFO; // 使用 sa_sigaction 而不是 sa_handler

  // 保存原来的处理函数，如果有的话
  if (real_sigaction(SIGILL, &sa, &original_sigill_action) != 0) {
    perror("[SIGILL HOOK] sigaction 注册失败");
    return;
  }

  original_signal = (signal_t) dlsym(RTLD_NEXT, "signal");
  original_sigaction = (sigaction_t) dlsym(RTLD_NEXT, "sigaction");

#ifdef LASX_PROFILE
  if (lasx_profile_mode & LASX_PROFILE_SIGNAL) {
    struct sigaction sa2;
    sa2.sa_sigaction = profile_handler;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = SA_SIGINFO;
    if (sigaction(SIGUSR1, &sa2, NULL) != 0) {
      perror("[PROFILE] sigaction 注册失败");
      return;
    }
  }
#endif

  //    printf("[LD_PRELOAD] SIGILL 处理函数注册完成！\n");
}
