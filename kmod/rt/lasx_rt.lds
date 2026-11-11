/* SPDX-License-Identifier: MIT */
OUTPUT_FORMAT("elf64-loongarch")
ENTRY(lasx_rt_trap_entry)

SECTIONS
{
    . = 0x0;
    __rt_image_start = .;

    .text : ALIGN(16) {
        KEEP(*(.text.entry))
        *(.text .text.*)
    }

    .rodata : ALIGN(16) {
        *(.rodata .rodata.* .srodata .srodata.*)
    }

    .rela.dyn : ALIGN(8) {
        __rela_dyn_start = .;
        *(.rela.*)
        __rela_dyn_end = .;
    }

    .dynsym   : ALIGN(8) { *(.dynsym) }
    .dynstr   : ALIGN(8) { *(.dynstr) }
    .hash     : ALIGN(8) { *(.hash) }
    .gnu.hash : ALIGN(8) { *(.gnu.hash) }

    /* Separate R-X code/rodata from RW data/bss on a 16KB page boundary */
    . = ALIGN(0x4000);
    __data_start = .;

    .dynamic : ALIGN(16) { *(.dynamic) }
    .got     : ALIGN(16) { *(.got .got.plt) }
    .data    : ALIGN(16) { *(.data .data.* .sdata .sdata.* .data.rel.ro .data.rel.ro.*) }
    __file_end = .;

    .bss     : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.* .sbss .sbss.* COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }

    . = ALIGN(0x4000);
    __rt_image_end = .;

    /DISCARD/ : {
        *(.interp)
        *(.note*)
        *(.comment)
        *(.eh_frame*)
    }
}
