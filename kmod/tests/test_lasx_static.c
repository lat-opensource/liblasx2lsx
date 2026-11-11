/* SPDX-License-Identifier: GPL-2.0 OR MIT */
/*
 * Statically linked (-static) LASX binary test.
 * Has NO dynamic linker (ld.so) at all — neither LD_PRELOAD nor /etc/ld.so.preload
 * can intercept this binary, yet lasx2lsx.ko handles it transparently.
 */
#include <lasxintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

__attribute__((noinline, noclone)) static void static_lasx_compute(const int32_t* a, const int32_t* b, int32_t* out)
{
    __m256i va = __lasx_xvld(a, 0);
    __m256i vb = __lasx_xvld(b, 0);
    __m256i vc = __lasx_xvadd_w(va, vb);
    __m256i vp = __lasx_xvpermi_q(vc, vc, 0x01);
    __lasx_xvst(vp, out, 0);
}

int main(void)
{
    int32_t a[8] __attribute__((aligned(32))) = {1, 2, 3, 4, 10, 20, 30, 40};
    int32_t b[8] __attribute__((aligned(32))) = {5, 6, 7, 8, 50, 60, 70, 80};
    int32_t out[8] __attribute__((aligned(32))) = {0};
    int32_t exp[8] = {60, 80, 100, 120, 6, 8, 10, 12};

    for (int i = 0; i < 100000; i++)
        static_lasx_compute(a, b, out);

    if (memcmp(out, exp, sizeof(exp)) != 0) {
        printf("[FAIL] test_lasx_static output mismatch!\n");
        return 1;
    }

    printf("[PASS] Test 8 (Static -static ELF binary, no ld.so): 100,000 LASX iters exact match!\n");
    return 0;
}
