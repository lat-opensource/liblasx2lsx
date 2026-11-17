/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

static const int input[8] __attribute__((aligned(32))) = {1, 2, 3, 4, 5, 6, 7, 8};
static int emulated;
static int simulated;
static char native_path[PATH_MAX];
static sigjmp_buf ill_jmp;
static volatile sig_atomic_t ill_hits;
extern const uint32_t lasx_fork_fresh_pc;

__attribute__((noinline, noclone)) static void inherited_vec(int* out) { __asm__ volatile("xvld $xr0,%0,0\n\txvst $xr0,%1,0" ::"r"(input), "r"(out) : "memory", "$xr0"); }

__attribute__((noinline, noclone)) static void fresh_vec(int* out)
{
    __asm__ volatile(".globl lasx_fork_fresh_pc\n"
                     "lasx_fork_fresh_pc:\n\t"
                     "xvld $xr1,%0,0\n\txvaddi.wu $xr1,$xr1,3\n\txvst $xr1,%1,0" ::"r"(input),
                     "r"(out)
                     : "memory", "$xr1");
}

static int fresh_is_patched(void) { return (*(const volatile uint32_t*)&lasx_fork_fresh_pc & 0xfc0003e0U) == 0x48000300U; }

static int check_vec(void (*fn)(int*), int add)
{
    int out[8] __attribute__((aligned(32)));
    fn(out);
    for (int i = 0; i < 8; i++)
        if (out[i] != input[i] + add)
            return 1;
    return 0;
}

static void user_sigill(int sig)
{
    (void)sig;
    ill_hits++;
    siglongjmp(ill_jmp, 1);
}

static void* fork_worker(void* arg)
{
    int raw_first = (int)(intptr_t)arg;
    if (pthread_setname_np(pthread_self(), "codec-worker"))
        return (void*)1;
    /* A direct clone avoids libc's child-side memset before the first LASX. */
    pid_t pid = raw_first ? syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0) : fork();
    if (pid < 0)
        return (void*)1;
    if (pid == 0) {
        if (!raw_first && check_vec(inherited_vec, 0))
            _exit(2);
        if (check_vec(fresh_vec, 3))
            _exit(3);
        if (emulated && !fresh_is_patched())
            _exit(4); /* Native LASX can compute correctly despite a missed trap. */
        if (check_vec(inherited_vec, 0))
            _exit(5);

        struct sigaction sa = {.sa_handler = user_sigill};
        sigemptyset(&sa.sa_mask);
        if (sigaction(SIGILL, &sa, NULL))
            _exit(6);
        if (sigsetjmp(ill_jmp, 1) == 0) {
            register unsigned long a7 __asm__("$a7") = 0;
            __asm__ volatile(".word 0" : "+r"(a7)::"memory");
            _exit(7);
        }
        if (ill_hits != 1 || check_vec(fresh_vec, 3))
            _exit(8);

        if (simulated) {
            execl(native_path, native_path, "--native-child", (char*)NULL);
            _exit(127);
        }
        _exit(0);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) != pid || status != 0) {
        fprintf(stderr, "[FAIL] renamed-worker %s child status=%#x\n", raw_first ? "new LASX first" : "inherited JIT first", status);
        return (void*)1;
    }
    return NULL;
}

/* Run explicitly as root with --disable-runtime; the child has not used LBT. */
static int check_disabled_runtime(void)
{
    int fd = open("/sys/module/lasx2lsx/parameters/enable_runtime", O_WRONLY);
    int sync[2], status = 0;
    char byte;
    if (fd < 0 || pipe(sync))
        return 1;
    if (check_vec(inherited_vec, 0))
        return 1;
    pid_t pid = syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0);
    if (pid == 0) {
        if (read(sync[0], &byte, 1) != 1)
            _exit(2);
        _exit(check_vec(inherited_vec, 0));
    }
    int failed = pid < 0;
    if (!failed) {
        failed = write(fd, "N", 1) != 1;
        if (write(sync[1], "x", 1) != 1)
            failed = 1;
        if (waitpid(pid, &status, 0) != pid || status != 0)
            failed = 1;
    }
    if (write(fd, "Y", 1) != 1)
        failed = 1;
    close(fd);
    close(sync[0]);
    close(sync[1]);
    printf("[%s] existing jiscr1 child binding with enable_runtime=N (status=%#x)\n", failed ? "FAIL" : "PASS", status);
    return failed;
}

static int check_fatal_sigill(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        register unsigned long a7 __asm__("$a7") = 0;
        __asm__ volatile(".word 0" : "+r"(a7)::"memory");
        _exit(1);
    }
    int status;
    return pid < 0 || waitpid(pid, &status, 0) != pid || !WIFSIGNALED(status) || WTERMSIG(status) != SIGILL;
}

int main(int argc, char** argv)
{
    struct rlimit lim = {0, 0};
    setrlimit(RLIMIT_CORE, &lim);
    if (argc > 1 && strcmp(argv[1], "--disable-runtime") == 0)
        return check_disabled_runtime();
    if (argc > 1 && strcmp(argv[1], "--native-child") == 0) {
        if (check_vec(fresh_vec, 3) || fresh_is_patched()) {
            fprintf(stderr, "[FAIL] exec retained the old simulation scope\n");
            return 1;
        }
        /* A normal LBT user outside the filter must not acquire a runtime. */
        register uint64_t val __asm__("$a0") = 0x12345678;
        __asm__ volatile(".word 0x00000882\n\t.word 0x00000c44" : "+r"(val)::"memory");
        return val != 0x12345678;
    }

    emulated = access("/sys/module/lasx2lsx/parameters/enable_runtime", F_OK) == 0;
    FILE* f = fopen("/sys/module/lasx2lsx/parameters/sim_3a6000", "r");
    if (f) {
        simulated = fgetc(f) == 'Y';
        fclose(f);
    }
    char temp_dir[] = "/tmp/lasx-fork-XXXXXX";
    if (simulated) {
        char self[PATH_MAX];
        ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (n < 0 || !mkdtemp(temp_dir))
            return 1;
        self[n] = '\0';
        snprintf(native_path, sizeof(native_path), "%s/outside", temp_dir);
        if (symlink(self, native_path)) {
            rmdir(temp_dir);
            return 1;
        }
    }

    int failed = check_vec(inherited_vec, 0);
    for (intptr_t path = 0; !failed && path < 2; path++) {
        pthread_t th;
        void* result = NULL;
        if (pthread_create(&th, NULL, fork_worker, (void*)path)) {
            failed = 1;
            break;
        }
        if (pthread_join(th, &result) || result)
            failed = 1;
    }
    if (!failed)
        failed = check_fatal_sigill();
    if (simulated) {
        unlink(native_path);
        rmdir(temp_dir);
    }
    if (!failed)
        printf("[PASS] renamed-worker fork: inherited/new LASX, handled/fatal SIGILL and exec scope\n");
    return failed;
}
