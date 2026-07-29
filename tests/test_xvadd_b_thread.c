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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>

static __attribute__((noinline)) void asm_test(void *mem)
{
    __asm__ volatile(
        "xvld $xr0, %0, 0\n"
        "xvld $xr1, %0, 32\n"
        "xvadd.b $xr2, $xr0, $xr1\n"
        "xvst $xr2, %0, 64\n"
        :
        : "r"(mem)
        : "memory", "$xr0", "$xr1", "$xr2");
}

static __attribute__((noinline)) void asm_test_fresh(void *mem)
{
    __asm__ volatile(
        "xvld $xr0, %0, 0\n"
        "xvld $xr1, %0, 32\n"
        "xvadd.b $xr2, $xr0, $xr1\n"
        "xvst $xr2, %0, 64\n"
        :
        : "r"(mem)
        : "memory", "$xr0", "$xr1", "$xr2");
}

// 每个线程独立运行测试，使用本地内存（栈上分配）
void *test_xvadd_b(void *arg) {
    int thread_id = *(int *)arg;
    printf("Thread %d: begin\n", thread_id);

    // 内存布局：源A(32B)，源B(32B)，结果(32B)
    unsigned char mem[96] __attribute__((aligned(32)));
    memset(mem, 0, sizeof(mem));

    // 初始化源数据（确定性的值）
    uint8_t *a = (uint8_t *)mem;
    uint8_t *b = (uint8_t *)(mem + 32);
    for (int i = 0; i < 32; i++) {
        a[i] = i * 3 + 1;      // 1,4,7,...
        b[i] = i * 5 + 2;      // 2,7,12,...
    }

    // Thread 2 starts a fresh block with SCR1 cleared, reproducing the case
    // where block translation returned before initializing the signal context.
    if (thread_id == 2) {
        __asm__ volatile("movgr2scr $scr1, $zero" ::: "memory");
        asm_test_fresh(mem);
        asm_test_fresh(mem);
        asm_test_fresh(mem);
    } else {
        asm_test(mem);
        asm_test(mem);
        asm_test(mem);
    }

    // 验证结果
    uint8_t *c = (uint8_t *)(mem + 64);
    int errors = 0;
    for (int i = 0; i < 32; i++) {
        uint8_t expected = a[i] + b[i];
        if (c[i] != expected) {
            printf("Thread %d: mismatch at index %d: got %u, expected %u\n",
                   thread_id, i, c[i], expected);
            errors++;
        }
    }

    if (errors == 0)
        printf("Thread %d: xvadd.b test PASSED\n", thread_id);
    else
        printf("Thread %d: xvadd.b test FAILED (%d errors)\n", thread_id, errors);

    return NULL;
}

int main() {
    pthread_t t1, t2;
    int id1 = 1, id2 = 2;

    int self = 0;
    test_xvadd_b(&self);

    // 线程一执行测试
    pthread_create(&t1, NULL, test_xvadd_b, &id1);
    pthread_join(t1, NULL);

    // 线程二执行测试
    pthread_create(&t2, NULL, test_xvadd_b, &id2);
    pthread_join(t2, NULL);

    return 0;
}
