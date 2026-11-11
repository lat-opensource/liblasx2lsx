/* SPDX-License-Identifier: GPL-2.0-only */
    .section .rodata, "a", @progbits
    .balign 16
    .globl lasx_rt_blob
    .type lasx_rt_blob, @object
lasx_rt_blob:
    .incbin "lasx_rt.bin"
.Llasx_rt_blob_end:
    .size lasx_rt_blob, .Llasx_rt_blob_end - lasx_rt_blob
