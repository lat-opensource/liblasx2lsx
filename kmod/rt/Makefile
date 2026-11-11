# SPDX-License-Identifier: MIT
ifeq ($(shell uname -m),x86_64)
CROSS_COMPILE ?= loongarch64-unknown-linux-gnu-
CC := $(CROSS_COMPILE)gcc
WORLD_CFLAGS := -DCONFIG_LOONGARCH_NEW_WORLD -DFRECIPE -mfrecipe -Wno-array-bounds
else
CC ?= gcc
ifeq ($(shell readelf -l /bin/sh 2>/dev/null | grep -q 'ld-linux-loongarch-lp64d.so.1' && echo new || echo old),new)
WORLD_CFLAGS := -DCONFIG_LOONGARCH_NEW_WORLD -DFRECIPE -mfrecipe -Wno-array-bounds
else
WORLD_CFLAGS :=
endif
endif

PYTHON3 ?= python3
LIB_ROOT := ../..
BUILD_DIR := build

CFLAGS := -Wall -O2 -fPIE -fno-plt -fno-stack-protector -fno-ident \
          -fno-tree-loop-distribute-patterns \
          -ffunction-sections -fdata-sections \
          $(WORLD_CFLAGS) \
          -Wno-strict-aliasing -Wno-aggressive-loop-optimizations \
          -mlsx -mno-lasx \
          -I$(LIB_ROOT)/src/include -I..

ASFLAGS := -fPIE -mlsx -mno-lasx -I..

LDFLAGS := -nostdlib -pie -Wl,-T,lasx_rt.lds -Wl,--gc-sections \
           -Wl,-Bsymbolic -Wl,--no-undefined -Wl,--build-id=none \
           -Wl,--no-warn-rwx-segments \
           -Wl,--wrap=la_addi_d -Wl,--wrap=mmap -Wl,--wrap=mprotect

EMU_SRCS := \
    lasx_emu.c lasx_arith.c lasx_logic.c lasx_mem.c lasx_imm.c \
    lasx_muldiv.c lasx_extract.c lasx_bitmanip.c lasx_cmp.c \
    lasx_fbasic.c lasx_fma.c lasx_perm.c lasx_extadd.c lasx_fcmp.c \
    lasx_shift.c lasx_shift_n.c lasx_shift_ni.c lasx_shift_helpers.c \
    lasx_profile.c

INTERPRET_SRCS := \
    lasx_interpret.c lasx_patterns_xvmap.c \
    lasx_interpret_opt_gen_xvmap.c lasx_interpret_opt_gen_usedef.c \
    lasx_interpret_opt_gen.c

EMU_OBJS       := $(patsubst %.c,$(BUILD_DIR)/emu_%.o,$(EMU_SRCS))
INTERPRET_OBJS := $(patsubst %.c,$(BUILD_DIR)/int_%.o,$(INTERPRET_SRCS))
LAGOON_OBJ     := $(BUILD_DIR)/lagoon.o
RT_OBJS        := $(BUILD_DIR)/lasx_rt_entry.o $(BUILD_DIR)/lasx_rt_glue.o

ALL_OBJS := $(RT_OBJS) $(EMU_OBJS) $(INTERPRET_OBJS) $(LAGOON_OBJ)

OUT_BIN  := ../lasx_rt.bin
OUT_META := ../lasx_rt_meta.h
OUT_ELF  := $(BUILD_DIR)/lasx_rt.elf

.PHONY: all clean

all: $(OUT_BIN) $(OUT_META)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/lasx_rt_entry.o: lasx_rt_entry.S ../lasx_kproto.h | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

$(BUILD_DIR)/lasx_rt_glue.o: lasx_rt_glue.c ../lasx_kproto.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fno-builtin -c -o $@ $<

$(BUILD_DIR)/emu_%.o: $(LIB_ROOT)/src/emu/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/int_%.o: $(LIB_ROOT)/src/interpret/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/lagoon.o: $(LIB_ROOT)/src/lagoon.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUT_ELF): $(ALL_OBJS) lasx_rt.lds
	$(CC) $(CFLAGS) -o $@ $(ALL_OBJS) $(LDFLAGS) -lgcc

$(OUT_BIN): $(OUT_ELF) gen_rt_blob.py
	$(PYTHON3) gen_rt_blob.py $(OUT_ELF) $(OUT_BIN) $(OUT_META)

$(OUT_META): $(OUT_BIN)
	@test -f $@ || $(PYTHON3) gen_rt_blob.py $(OUT_ELF) $(OUT_BIN) $(OUT_META)

clean:
	rm -rf $(BUILD_DIR) $(OUT_BIN) $(OUT_META)
