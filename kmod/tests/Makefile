# SPDX-License-Identifier: GPL-2.0-only OR MIT
ifeq ($(shell uname -m),x86_64)
CC := loongarch64-unknown-linux-gnu-gcc
else
CC ?= gcc
endif

CFLAGS := -O2 -Wall -Wextra -mlasx

TARGETS := test_lasx_raw_ri test_lasx_suite test_lasx_static

.PHONY: all clean

all: $(TARGETS)

test_lasx_raw_ri: test_lasx_raw_ri.c
	$(CC) $(CFLAGS) -fno-tree-vectorize -nostdlib -fno-stack-protector -static -fno-pie -no-pie -o $@ $<

test_lasx_suite: test_lasx_suite.c ../lasx_kproto.h
	$(CC) $(CFLAGS) -pthread -o $@ test_lasx_suite.c -lm

test_lasx_static: test_lasx_static.c
	$(CC) $(CFLAGS) -static -o $@ $<

clean:
	rm -f $(TARGETS)
