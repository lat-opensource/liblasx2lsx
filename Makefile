# Always build LoongArch binaries by default
# On x86_64 host use cross-compile with new world config
# On native LoongArch build natively, auto-detect ABI world
ifeq ($(shell uname -m),x86_64)
# Force cross-compile on x86 - never build native
CC = loongarch64-unknown-linux-gnu-gcc
AS = loongarch64-unknown-linux-gnu-as
CFLAGS = -Wall -fPIC -O2 -ffunction-sections -fdata-sections -DCONFIG_LOONGARCH_NEW_WORLD -DFRECIPE -Wno-strict-aliasing -Wno-aggressive-loop-optimizations -Wno-array-bounds -mlsx -mfrecipe -ftls-model=initial-exec -Isrc/include
DBG_CFLAGS = -Wall -fPIC -O0 -g -ffunction-sections -fdata-sections -DCONFIG_LOONGARCH_NEW_WORLD -DFRECIPE -Wno-strict-aliasing -Wno-aggressive-loop-optimizations -Wno-array-bounds -mlsx -mfrecipe -ftls-model=initial-exec -Isrc/include
QEMU = qemu-loongarch64-static
QEMU_CPU = -cpu la464,lasx=off
HOST_X86 = 1
else
# Default native build on LoongArch
CC ?= gcc
CFLAGS ?= -Wall -fPIC -O2 -ffunction-sections -fdata-sections -Wno-strict-aliasing -Wno-aggressive-loop-optimizations -mlsx -ftls-model=initial-exec -Isrc/include
DBG_CFLAGS ?= -Wall -fPIC -O0 -g -ffunction-sections -fdata-sections -Wno-strict-aliasing -Wno-aggressive-loop-optimizations -mlsx -ftls-model=initial-exec -Isrc/include
HOST_X86 = 0

# Detect ABI world on native LoongArch via ELF interpreter
ifeq ($(shell readelf -l /bin/sh 2>/dev/null | grep -q 'ld-linux-loongarch-lp64d.so.1' && echo new || echo old),new)
    CFLAGS += -DCONFIG_LOONGARCH_NEW_WORLD -DFRECIPE -mfrecipe -Wno-array-bounds
    DBG_CFLAGS += -DCONFIG_LOONGARCH_NEW_WORLD -DFRECIPE -mfrecipe -Wno-array-bounds
endif
endif

LDFLAGS = -shared -Wl,--gc-sections -lm -ldl

# Source directories
SRC_DIR = src
EMU_DIR = $(SRC_DIR)/emu
INTERPRET_DIR = $(SRC_DIR)/interpret
INCLUDE_DIR = $(SRC_DIR)/include

# Emu sources
EMU_SRC = $(EMU_DIR)/sigill_hook.c $(EMU_DIR)/thread_data.c $(EMU_DIR)/lasx_emu.c \
	$(EMU_DIR)/lasx_arith.c $(EMU_DIR)/lasx_logic.c $(EMU_DIR)/lasx_mem.c \
	$(EMU_DIR)/lasx_imm.c $(EMU_DIR)/lasx_muldiv.c $(EMU_DIR)/lasx_extract.c \
	$(EMU_DIR)/lasx_bitmanip.c $(EMU_DIR)/lasx_cmp.c $(EMU_DIR)/lasx_fbasic.c \
	$(EMU_DIR)/lasx_fma.c $(EMU_DIR)/lasx_perm.c $(EMU_DIR)/lasx_extadd.c \
	$(EMU_DIR)/lasx_fcmp.c $(EMU_DIR)/lasx_shift.c $(EMU_DIR)/lasx_shift_n.c \
	$(EMU_DIR)/lasx_shift_ni.c $(EMU_DIR)/lasx_shift_helpers.c $(EMU_DIR)/perf_stats.c \
	$(EMU_DIR)/lasx_profile.c

# Interpret sources
INTERPRET_SRC = $(INTERPRET_DIR)/lasx_interpret.c $(INTERPRET_DIR)/lasx_patterns_xvmap.c \
	$(INTERPRET_DIR)/lasx_interpret_opt_gen_xvmap.c \
	$(INTERPRET_DIR)/lasx_interpret_opt_gen_usedef.c \
	$(INTERPRET_DIR)/lasx_interpret_opt_gen.c

# Lagoon assembler (prebuilt static library)
LAGOON_A = $(SRC_DIR)/liblagoon.a

# Enable parallel build
MAKEFLAGS += -j$(shell nproc)

# Header dependencies (auto-regenerate on .h change)
HDRS := $(wildcard $(INCLUDE_DIR)/*.h)

TARGET = liblasx2lsx.so
DBGTARGET = liblasx2lsx_dbg.so
PROFILETARGET = liblasx2lsx_profile.so
SRC = $(EMU_SRC) $(INTERPRET_SRC)
OBJ = $(SRC:.c=.o)
DBGOBJ = $(SRC:.c=_dbg.o)

# Files that call profile_* API, compile with -DLASX_PROFILE for profile build
PROFILE_SRC = $(EMU_DIR)/sigill_hook.c $(INTERPRET_DIR)/lasx_interpret.c \
              $(INTERPRET_DIR)/lasx_patterns_xvmap.c $(EMU_DIR)/lasx_profile.c
PROFILE_OBJ = $(patsubst %.c,%_profile.o,$(PROFILE_SRC))
RELEASE_OBJ = $(filter-out $(patsubst %_profile.o,%.o,$(PROFILE_OBJ)),$(OBJ))

DEPS := $(OBJ:.o=.d) $(DBGOBJ:_dbg.o=_dbg.d) $(PROFILE_OBJ:.o=.d)

-include $(DEPS)

.DEFAULT_GOAL := all

# Individual variant targets
release: $(TARGET)
debug: $(DBGTARGET)
profile: $(PROFILETARGET)

all: release debug profile

# Test configuration
TESTS = test_sigill test_xvadd test_xvfbasic test_xvfma test_xvimm test_xvcmp test_xvldst test_xvmul test_xvperm test_xvsh test_xvext test_xvbit test_xvbiti test_debug test_xvperm_full test_xvadd_b test_xvadd_b_perf test_xvadd_b_thread test_interpret_mt test_xvblock42 test_xvblock38
TEST_BINARIES = $(TESTS)

# test_xvfmadd_beq has custom build (includes run_loop.s)
test_xvfmadd_beq: tests/test_xvfmadd_beq.c tests/run_loop.s
	$(AS) -o tests/run_loop.o tests/run_loop.s
	$(CC) $(CFLAGS) -Wall -O2 -o $@ tests/test_xvfmadd_beq.c tests/run_loop.o -lm -lpthread

# Loop pattern tests
test_pattern1: tests/test_pattern1.c tests/run_loop1.s
	$(AS) -o tests/run_loop1.o tests/run_loop1.s
	$(CC) $(CFLAGS) -Wall -O2 -o $@ tests/test_pattern1.c tests/run_loop1.o -lm

test_pattern2: tests/test_pattern2.c tests/run_loop2.s
	$(AS) -o tests/run_loop2.o tests/run_loop2.s
	$(CC) $(CFLAGS) -Wall -O2 -o $@ tests/test_pattern2.c tests/run_loop2.o -lm

test_pattern3: tests/test_pattern3.c tests/run_loop3.s
	$(AS) -o tests/run_loop3.o tests/run_loop3.s
	$(CC) $(CFLAGS) -Wall -O2 -o $@ tests/test_pattern3.c tests/run_loop3.o -lm

test_pattern4: tests/test_pattern4.c tests/run_loop4.s
	$(AS) -o tests/run_loop4.o tests/run_loop4.s
	$(CC) $(CFLAGS) -Wall -O2 -o $@ tests/test_pattern4.c tests/run_loop4.o -lm

test_pattern5: tests/test_pattern5.c tests/run_loop5.s
	$(AS) -o tests/run_loop5.o tests/run_loop5.s
	$(CC) $(CFLAGS) -Wall -O2 -o $@ tests/test_pattern5.c tests/run_loop5.o -lm

test_xvmap_loop: tests/test_xvmap_loop.c tests/test_xvmap_loop.s
	$(AS) -o tests/test_xvmap_loop.o tests/test_xvmap_loop.s
	$(CC) $(CFLAGS) -Wall -O2 -o $@ tests/test_xvmap_loop.c tests/test_xvmap_loop.o -lm

# Batch xvmap loop tests (generated by tools/gen_loop_tests.py)
test_xvmap_%: tests/test_xvmap_%.c tests/test_xvmap_%.s
	$(AS) -o tests/test_xvmap_$*.o tests/test_xvmap_$*.s
	$(CC) $(CFLAGS) -Wall -O2 -o $@ tests/test_xvmap_$*.c tests/test_xvmap_$*.o -lm

# Special hand-written xvmap tests (19)
XVMAP_SPECIAL_TESTS = test_xvmap_xvld test_xvmap_xvst \
    test_xvmap_xvreplgr2vr_w test_xvmap_xvreplgr2vr_h test_xvmap_xvreplgr2vr_d \
    test_xvmap_xvldrepl_w test_xvmap_xvinsgr2vr_w test_xvmap_xvstelm_w test_xvmap_xvstelm_d \
    test_xvmap_xvpermi_q test_xvmap_xvpermi_d test_xvmap_xvperm_w test_xvmap_xvfcmp_cond_s \
    test_xvmap_xvldx test_xvmap_xvstx test_xvmap_xvpickve2gr_w test_xvmap_xvpickve2gr_wu \
    test_xvmap_vext2xv_hu_bu test_xvmap_vext2xv_wu_hu

$(XVMAP_SPECIAL_TESTS): %: tests/%.c tests/%.s
	$(AS) -o tests/$*.o tests/$*.s
	$(CC) $(CFLAGS) -Wall -O2 -o $@ tests/$*.c tests/$*.o -lm

# Build and run all xvmap unit loop tests (block vs blockloop comparison)
# Excludes test_xvmap_loop1 (has intentional internal branch)
XVMAP_LOOP_SOURCES = $(filter-out tests/test_xvmap_loop1.s,$(wildcard tests/test_xvmap_*.s))
XVMAP_LOOP_TESTS = $(patsubst tests/%.s,%,$(XVMAP_LOOP_SOURCES))

unittest-loop: liblasx2lsx.so $(XVMAP_LOOP_TESTS)
	@echo "Running xvmap loop tests..."
	@failed=0; passed=0; \
	for test in $(XVMAP_LOOP_TESTS); do \
		bin=$$(echo $$test | sed 's|tests/||'); \
		if [ "$(HOST_X86)" = "1" ]; then \
			out_block=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block $(QEMU) $(QEMU_CPU) ./$$bin 2>/dev/null); \
			out_bll=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block,loop $(QEMU) $(QEMU_CPU) ./$$bin 2>/dev/null); \
		else \
			out_block=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block ./$$bin 2>/dev/null); \
			out_bll=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block,loop ./$$bin 2>/dev/null); \
		fi; \
		if [ -z "$$out_bll" ]; then \
			echo "FAIL: $$bin (blockloop crash)"; failed=$$((failed+1)); \
		elif [ "$$out_block" = "$$out_bll" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: $$bin"; \
			echo "  block:     $$out_block"; \
			echo "  blockloop: $$out_bll"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "xvmap loop tests: $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

SCAN_LOOPS_OBJS = $(INTERPRET_DIR)/lasx_patterns_xvmap.o \
                   $(INTERPRET_DIR)/lasx_interpret.o \
                   $(INTERPRET_DIR)/lasx_interpret_opt_gen.o \
                   $(INTERPRET_DIR)/lasx_interpret_opt_gen_xvmap.o \
                   $(INTERPRET_DIR)/lasx_interpret_opt_gen_usedef.o \
                   $(EMU_DIR)/thread_data.o

scan_loops: tools/scan_loops.c $(SCAN_LOOPS_OBJS) $(LAGOON_A)
	$(CC) $(CFLAGS) -o $@ $^ -lm -ldl

JIT_COMPARE_OBJS = $(INTERPRET_DIR)/lasx_interpret_opt_gen.o \
                   $(INTERPRET_DIR)/lasx_interpret_opt_gen_usedef.o \
                   $(EMU_DIR)/thread_data.o

jit_compare: tools/jit_compare.c $(JIT_COMPARE_OBJS) $(LAGOON_A)
	$(CC) $(CFLAGS) -Isrc/include -o $@ $^ -lm -ldl

# Random test configuration
RANDOM_TESTS_DIR = random_test/src
RANDOM_RESULTS_DIR = random_test/result
RANDOM_TEST_DIR = random_test/test
RANDOM_DATA_FILE = random_test/random_data

# Generate test names from .c files (363 tests)
RANDOM_TESTS = $(patsubst %.c,%,$(notdir $(wildcard $(RANDOM_TESTS_DIR)/*.c)))

# test_xvand, test_xvextadd: intrinsics require -mlasx but emulator doesn't support all instructions
# test_xvfcmp, test_xvbit_full, test_xvmul_full: need native LASX
# test_xvfbasic_full, test_xvextract: use intrinsics, have emulator issues
TESTS_DEBUG = test_xvand test_xvextadd test_xvbit_full test_xvmul_full test_xvfcmp test_xvfbasic_full test_xvextract
TEST_DEBUG_BINARIES = $(TESTS_DEBUG)

# Build tests (compiled for target architecture)
$(TEST_BINARIES): %: tests/%.c
	$(CC) $(CFLAGS) -Wall -O2 -o $@ $< -lm -lpthread

# Build rule for random test binaries
$(RANDOM_TEST_DIR)/%: $(RANDOM_TESTS_DIR)/%.c | $(RANDOM_TEST_DIR)
	$(CC) $(CFLAGS) -Wall -O2 -o $@ $< -lm

$(RANDOM_TEST_DIR):
	mkdir -p $(RANDOM_TEST_DIR)

# Tests using intrinsics need -mlasx flag
test_xvperm_full: CFLAGS += -mlasx
test_interpret_mt: CFLAGS += -mlasx

$(TEST_DEBUG_BINARIES): %: tests/%.c
	$(CC) $(CFLAGS) -Wall -O2 -o $@ $< -lm

# Debug tests with intrinsics need -mlasx
test_xvfbasic_full: CFLAGS += -mlasx
test_xvextract: CFLAGS += -mlasx
test_xvand: CFLAGS += -mlasx
test_xvextadd: CFLAGS += -mlasx
test_xvbit_full: CFLAGS += -mlasx
test_xvmul_full: CFLAGS += -mlasx
test_xvfcmp: CFLAGS += -mlasx

# Generate list of all random test binaries
RANDOM_TEST_BINARIES = $(patsubst %.c,$(RANDOM_TEST_DIR)/%,$(notdir $(wildcard $(RANDOM_TESTS_DIR)/*.c)))

# Build random test binary generator
$(RANDOM_TEST_DIR)/gen_random: random_test/gen_random.c | $(RANDOM_TEST_DIR)
	$(CC) $(CFLAGS) -Wall -O2 -o $@ $<

# Build random test binaries (parallel via Make -j)
build-random-tests: $(RANDOM_TEST_DIR)/gen_random $(RANDOM_TEST_BINARIES)

# Run random tests with emulation (uses liblasx2lsx.so)
test: $(TARGET) build-random-tests
	@echo "Running random tests (block mode)..."
	@failed=0; passed=0; \
	for test in $(RANDOM_TESTS); do \
		case "$$test" in xvfrecipe.d|xvfrecipe.s|xvfrsqrte.d|xvfrsqrte.s) continue;; esac; \
		if [ "$(HOST_X86)" = "1" ]; then \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		else \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		fi; \
		expected=$$(cat $(RANDOM_RESULTS_DIR)/$$test.exe.result); \
		if [ "$$output" = "$$expected" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: $$test"; \
			echo "  Expected: $$expected"; \
			echo "  Got:      $$output"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "Random tests: $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

# Run random tests with emu-only mode (no optimization, explicit MODE=emu)
test-one: $(TARGET) build-random-tests
	@echo "Running random tests (emu-only mode)..."
	@failed=0; passed=0; \
	for test in $(RANDOM_TESTS); do \
		case "$$test" in xvfrecipe.d|xvfrecipe.s|xvfrsqrte.d|xvfrsqrte.s) continue;; esac; \
		if [ "$(HOST_X86)" = "1" ]; then \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=emu $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		else \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=emu "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		fi; \
		expected=$$(cat $(RANDOM_RESULTS_DIR)/$$test.exe.result); \
		if [ "$$output" = "$$expected" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: $$test"; \
			echo "  Expected: $$expected"; \
			echo "  Got:      $$output"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "Random tests (emu-only): $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

# Run 42-instruction block test: compare block mode vs usedef mode with random seeds
test-xvblock42: $(TARGET) test_xvblock42
	@echo "Running 42-instruction block test (block vs usedef)..."
	@failed=0; passed=0; \
	for seed in 0 12345 54321 99999 77777; do \
		if [ "$(HOST_X86)" = "1" ]; then \
			out_block=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block $(QEMU) $(QEMU_CPU) ./test_xvblock42 -s $$seed 2>/dev/null); \
			out_usedef=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=usedef $(QEMU) $(QEMU_CPU) ./test_xvblock42 -s $$seed 2>/dev/null); \
		else \
			out_block=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block ./test_xvblock42 -s $$seed 2>/dev/null); \
			out_usedef=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=usedef ./test_xvblock42 -s $$seed 2>/dev/null); \
		fi; \
		if [ -z "$$out_usedef" ]; then \
			echo "FAIL: seed=$$seed (usedef crash)"; failed=$$((failed+1)); \
		elif [ "$$out_block" = "$$out_usedef" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: seed=$$seed"; \
			echo "  block:"; echo "$$out_block"; \
			echo "  usedef:"; echo "$$out_usedef"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "test-xvblock42: $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

# Run 38-instruction block test: compare block mode vs usedef mode
test-xvblock38: $(TARGET) test_xvblock38
	@echo "Running 38-instruction block test (block vs usedef)..."
	@failed=0; passed=0; \
	for seed in 0 12345 54321 99999 77777; do \
		if [ "$(HOST_X86)" = "1" ]; then \
			out_block=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block $(QEMU) $(QEMU_CPU) ./test_xvblock38 -s $$seed 2>/dev/null); \
			out_usedef=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=usedef $(QEMU) $(QEMU_CPU) ./test_xvblock38 -s $$seed 2>/dev/null); \
		else \
			out_block=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block ./test_xvblock38 -s $$seed 2>/dev/null); \
			out_usedef=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=usedef ./test_xvblock38 -s $$seed 2>/dev/null); \
		fi; \
		if [ -z "$$out_usedef" ]; then \
			echo "FAIL: seed=$$seed (usedef crash)"; failed=$$((failed+1)); \
		elif [ "$$out_block" = "$$out_usedef" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: seed=$$seed"; \
			echo "  block:"; echo "$$out_block"; \
			echo "  usedef:"; echo "$$out_usedef"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "test-xvblock38: $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

# Run random tests without emulation (native/QEMU with LASX)
test-native: build-random-tests
	@echo "Running random tests (native mode)..."
	@failed=0; passed=0; \
	for test in $(RANDOM_TESTS); do \
		case "$$test" in xvfrecipe.d|xvfrecipe.s|xvfrsqrte.d|xvfrsqrte.s) continue;; esac; \
		if [ "$(HOST_X86)" = "1" ]; then \
			output=$$($(QEMU) "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		else \
			output=$$("$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		fi; \
		expected=$$(cat $(RANDOM_RESULTS_DIR)/$$test.exe.result); \
		if [ "$$output" = "$$expected" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: $$test"; \
			echo "  Expected: $$expected"; \
			echo "  Got:      $$output"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "Random tests: $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

# Run random tests with xvmap loop JIT (block + loop mode)
test-loop: $(TARGET) build-random-tests
	@echo "Running random tests (block+loop mode)..."
	@failed=0; passed=0; \
	for test in $(RANDOM_TESTS); do \
		case "$$test" in xvfrecipe.d|xvfrecipe.s|xvfrsqrte.d|xvfrsqrte.s) continue;; esac; \
		if [ "$(HOST_X86)" = "1" ]; then \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block,loop $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		else \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block,loop "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		fi; \
		expected=$$(cat $(RANDOM_RESULTS_DIR)/$$test.exe.result); \
		if [ "$$output" = "$$expected" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: $$test"; \
			echo "  Expected: $$expected"; \
			echo "  Got:      $$output"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "Random tests (blockloop): $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

# Run random tests with explicit block mode (no loop detection)
test-block: $(TARGET) build-random-tests
	@echo "Running random tests (block mode)..."
	@failed=0; passed=0; \
	for test in $(RANDOM_TESTS); do \
		case "$$test" in xvfrecipe.d|xvfrecipe.s|xvfrsqrte.d|xvfrsqrte.s) continue;; esac; \
		if [ "$(HOST_X86)" = "1" ]; then \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		else \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=block "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		fi; \
		expected=$$(cat $(RANDOM_RESULTS_DIR)/$$test.exe.result); \
		if [ "$$output" = "$$expected" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: $$test"; \
			echo "  Expected: $$expected"; \
			echo "  Got:      $$output"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "Random tests (block): $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

# Run random tests with usedef mode (batch use/def optimization)
test-usedef: $(TARGET) build-random-tests
	@echo "Running random tests (usedef mode)..."
	@failed=0; passed=0; \
	for test in $(RANDOM_TESTS); do \
		case "$$test" in xvfrecipe.d|xvfrecipe.s|xvfrsqrte.d|xvfrsqrte.s) continue;; esac; \
		if [ "$(HOST_X86)" = "1" ]; then \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=usedef $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		else \
			output=$$(LD_PRELOAD=./liblasx2lsx.so LIBLASX2LSX_INTERPRET_MODE=usedef "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		fi; \
		expected=$$(cat $(RANDOM_RESULTS_DIR)/$$test.exe.result); \
		if [ "$$output" = "$$expected" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: $$test"; \
			echo "  Expected: $$expected"; \
			echo "  Got:      $$output"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "Random tests (usedef): $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

# Pattern rules for source files in subdirectories
$(EMU_DIR)/%.o: $(EMU_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(EMU_DIR)/%_dbg.o: $(EMU_DIR)/%.c
	$(CC) $(DBG_CFLAGS) -MMD -DDEBUG -c -o $@ $<

$(INTERPRET_DIR)/%.o: $(INTERPRET_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(INTERPRET_DIR)/%_dbg.o: $(INTERPRET_DIR)/%.c
	$(CC) $(DBG_CFLAGS) -MMD -DDEBUG -c -o $@ $<

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(SRC_DIR)/%_dbg.o: $(SRC_DIR)/%.c
	$(CC) $(DBG_CFLAGS) -MMD -DDEBUG -c -o $@ $<

$(EMU_DIR)/%_profile.o: $(EMU_DIR)/%.c
	$(CC) $(CFLAGS) -DLASX_PROFILE -MMD -c -o $@ $<

$(INTERPRET_DIR)/%_profile.o: $(INTERPRET_DIR)/%.c
	$(CC) $(CFLAGS) -DLASX_PROFILE -MMD -c -o $@ $<

$(TARGET): $(OBJ) $(LAGOON_A)
	$(CC) -o $@ $(OBJ) $(LAGOON_A) $(LDFLAGS)

$(DBGTARGET): $(DBGOBJ) $(LAGOON_A)
	$(CC) -o $@ $(DBGOBJ) $(LAGOON_A) $(LDFLAGS)

$(PROFILETARGET): $(RELEASE_OBJ) $(PROFILE_OBJ) $(LAGOON_A)
	$(CC) -o $@ $(RELEASE_OBJ) $(PROFILE_OBJ) $(LAGOON_A) $(LDFLAGS) -lpthread

# Build lagoon static library
$(LAGOON_A): $(SRC_DIR)/lagoon.o
	$(AR) rcs $@ $<

$(SRC_DIR)/lagoon.o: $(SRC_DIR)/lagoon.c $(HDRS)
	$(CC) $(CFLAGS) -mlsx -MMD -c -o $@ $<

# .deb packaging
DEB_VERSION = 1.0.0
ifeq ($(HOST_X86),1)
DEB_ARCH = loongarch64
else
DEB_ARCH = $(shell dpkg --print-architecture 2>/dev/null || uname -m)
endif

DEB_PKG = $(CURDIR)/liblasx2lsx_$(DEB_VERSION)_$(DEB_ARCH).deb

deb: $(TARGET)
	rm -rf debian/pkg
	mkdir -p debian/pkg/DEBIAN debian/pkg/usr/lib debian/pkg/usr/include debian/pkg/usr/share/doc/$(TARGET:.so=)
	cp $(TARGET) debian/pkg/usr/lib/$(TARGET).$(DEB_VERSION)
	ln -sf $(TARGET).$(DEB_VERSION) debian/pkg/usr/lib/$(TARGET).1
	ln -sf $(TARGET).1 debian/pkg/usr/lib/$(TARGET)
	cp $(INCLUDE_DIR)/lasx_emu.h debian/pkg/usr/include/
	cp LICENSE debian/pkg/usr/share/doc/$(TARGET:.so=)/copyright
	cp debian/changelog debian/pkg/usr/share/doc/$(TARGET:.so=)/changelog
	gzip -9 -n debian/pkg/usr/share/doc/$(TARGET:.so=)/changelog
	echo "Package: $(TARGET:.so=)" > debian/pkg/DEBIAN/control
	echo "Version: $(DEB_VERSION)" >> debian/pkg/DEBIAN/control
	echo "Architecture: $(DEB_ARCH)" >> debian/pkg/DEBIAN/control
	grep -E '^(Maintainer|Depends|Description|Section|Priority)' debian/control >> debian/pkg/DEBIAN/control
	dpkg-deb --build -Zxz debian/pkg $(DEB_PKG)
	rm -rf debian/pkg
	@echo "Package: $(DEB_PKG)"

# Cross-compilation for LoongArch (use: make cross CROSS_COMPILE=loongarch64-unknown-linux-gnu-)
cross: CC = $(CROSS_COMPILE)gcc
cross: all

clean:
	rm -f $(TARGET) $(DBGTARGET) $(PROFILETARGET) $(TEST_BINARIES) $(RANDOM_TEST_DIR)/*.c.tmp $(RANDOM_TEST_DIR)/*
	rmdir $(RANDOM_TEST_DIR) 2>/dev/null || true
	rm -f $(EMU_DIR)/*.o $(EMU_DIR)/*.d
	rm -f $(INTERPRET_DIR)/*.o $(INTERPRET_DIR)/*.d
	rm -f $(SRC_DIR)/*.o $(SRC_DIR)/*.d
	rm -f *.o *.d

clean-all: clean
	rm -f $(RANDOM_TEST_DIR)

# Run single random test with emulation (USAGE: make test-single TEST=xvadd.b [SEED=xxx])
test-single: $(TARGET) build-random-tests
	@test -f "$(RANDOM_TESTS_DIR)/$(TEST).c" || { echo "Error: $(RANDOM_TESTS_DIR)/$(TEST).c not found"; exit 1; }
	@if [ -n "$(SEED)" ]; then \
		$(RANDOM_TEST_DIR)/gen_random -f "$(RANDOM_TEST_DIR)/data_seed_$(SEED)" -n 8 -s $(SEED) -p; \
		echo "Input data:"; xxd -g 8 "$(RANDOM_TEST_DIR)/data_seed_$(SEED)"; \
		if [ "$(HOST_X86)" = "1" ]; then \
			LD_PRELOAD=./liblasx2lsx.so $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$(TEST)" -f "$(RANDOM_TEST_DIR)/data_seed_$(SEED)"; \
		else \
			LD_PRELOAD=./liblasx2lsx.so "$(RANDOM_TEST_DIR)/$(TEST)" -f "$(RANDOM_TEST_DIR)/data_seed_$(SEED)"; \
		fi; \
	else \
		echo "Input data:"; xxd -g 8 "$(RANDOM_DATA_FILE)"; \
		if [ "$(HOST_X86)" = "1" ]; then \
			LD_PRELOAD=./liblasx2lsx.so $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$(TEST)" -f "$(RANDOM_DATA_FILE)"; \
		else \
			LD_PRELOAD=./liblasx2lsx.so "$(RANDOM_TEST_DIR)/$(TEST)" -f "$(RANDOM_DATA_FILE)"; \
		fi; \
	fi

# Run single random test without emulation (native/QEMU with LASX) (USAGE: make test-single-native TEST=xvadd.b [SEED=xxx])
test-single-native: build-random-tests
	@test -f "$(RANDOM_TESTS_DIR)/$(TEST).c" || { echo "Error: $(RANDOM_TESTS_DIR)/$(TEST).c not found"; exit 1; }
	@if [ -n "$(SEED)" ]; then \
		$(RANDOM_TEST_DIR)/gen_random -f "$(RANDOM_TEST_DIR)/data_seed_$(SEED)" -n 8 -s $(SEED) -p; \
		echo "Input data:"; xxd -g 8 "$(RANDOM_TEST_DIR)/data_seed_$(SEED)"; \
		if [ "$(HOST_X86)" = "1" ]; then \
			$(QEMU) "$(RANDOM_TEST_DIR)/$(TEST)" -f "$(RANDOM_TEST_DIR)/data_seed_$(SEED)"; \
		else \
			"$(RANDOM_TEST_DIR)/$(TEST)" -f "$(RANDOM_TEST_DIR)/data_seed_$(SEED)"; \
		fi; \
	else \
		echo "Input data:"; xxd -g 8 "$(RANDOM_DATA_FILE)"; \
		if [ "$(HOST_X86)" = "1" ]; then \
			$(QEMU) "$(RANDOM_TEST_DIR)/$(TEST)" -f "$(RANDOM_DATA_FILE)"; \
		else \
			"$(RANDOM_TEST_DIR)/$(TEST)" -f "$(RANDOM_DATA_FILE)"; \
		fi; \
	fi

# Run random tests comparing native vs emulation (USAGE: make test-random-compare SEED=xxx)
test-random-compare: $(TARGET) build-random-tests
	@if [ -z "$(SEED)" ]; then \
		echo "Error: SEED is required"; \
		exit 1; \
	fi
	@echo "Generating random data with SEED=$(SEED)..."
	@$(RANDOM_TEST_DIR)/gen_random -f "$(RANDOM_TEST_DIR)/data_compare_$(SEED)" -n 8 -s $(SEED)
	@echo "Input data:"; xxd -g 8 "$(RANDOM_TEST_DIR)/data_compare_$(SEED)"
	@echo "Running random tests (comparing native vs emulation)..."
	@failed=0; passed=0; \
	for test in $(RANDOM_TESTS); do \
		case "$$test" in xvfrecipe.d|xvfrecipe.s|xvfrsqrte.d|xvfrsqrte.s|xvflogb.d|xvflogb.s) continue;; esac; \
		if [ "$(HOST_X86)" = "1" ]; then \
			native_output=$$($(QEMU) "$(RANDOM_TEST_DIR)/$$test" -f "$(RANDOM_TEST_DIR)/data_compare_$(SEED)" 2>&1); \
			emu_output=$$(LD_PRELOAD=./liblasx2lsx.so $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$$test" -f "$(RANDOM_TEST_DIR)/data_compare_$(SEED)" 2>&1); \
		else \
			native_output=$$("$(RANDOM_TEST_DIR)/$$test" -f "$(RANDOM_TEST_DIR)/data_compare_$(SEED)" 2>&1); \
			emu_output=$$(LD_PRELOAD=./liblasx2lsx.so "$(RANDOM_TEST_DIR)/$$test" -f "$(RANDOM_TEST_DIR)/data_compare_$(SEED)" 2>&1); \
		fi; \
		if [ "$$native_output" = "$$emu_output" ]; then \
			passed=$$((passed+1)); \
		else \
			echo "FAIL: $$test"; \
			echo ">>> Native:   $$native_output"; \
			echo ">>> Emu:    $$emu_output"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "Random tests compare: $$passed passed, $$failed failed"; \
	[ $$failed -eq 0 ]

# Boundary test data
BOUNDARY_DATA = random_test/boundary_data
gen_boundary: random_test/gen_boundary_data.c
	$(CC) $(CFLAGS) -Wall -O2 -o random_test/gen_boundary_data random_test/gen_boundary_data.c
boundary-data: gen_boundary
	@echo "Generating boundary test data..."
	@mkdir -p random_test
	./random_test/gen_boundary_data -f $(BOUNDARY_DATA) -t all
	@echo "Generated boundary test data with 12 test cases"

# Run tests with boundary data
test-boundary: $(TARGET) build-random-tests boundary-data
	@echo "Running boundary tests..."
	@failed=0; passed=0; data_idx=0; \
	while read -r line; do \
		data_idx=$$((data_idx+1)); \
		for test in $(RANDOM_TESTS); do \
			case "$$test" in xvfrecipe.d|xvfrecipe.s|xvfrsqrte.d|xvfrsqrte.s|xvflogb.*) continue;; esac; \
			if [ "$(HOST_X86)" = "1" ]; then \
				output=$$(LD_PRELOAD=./liblasx2lsx.so $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$$test" -f $(BOUNDARY_DATA) 2>&1); \
			else \
				output=$$(LD_PRELOAD=./liblasx2lsx.so "$(RANDOM_TEST_DIR)/$$test" -f $(BOUNDARY_DATA) 2>&1); \
			fi; \
			if [ -n "$$output" ]; then \
				passed=$$((passed+1)); \
			else \
				echo "FAIL: $$test (data $$data_idx)"; \
				failed=$$((failed+1)); \
			fi; \
		done; \
	done < <(xxd -p -r $(BOUNDARY_DATA) | fold -w16 | head -$$(($(stat -c%s $(BOUNDARY_DATA))/16)))
	@echo "Boundary tests: $$passed passed, $$failed failed"
	@[ $$failed -eq 0 ]

# Verbose test mode (show all test names)
test-verbose: $(TARGET) build-random-tests
	@echo "Running random tests (verbose mode)..."
	@failed=0; passed=0; \
	for test in $(RANDOM_TESTS); do \
		case "$$test" in xvfrecipe.d|xvfrecipe.s|xvfrsqrte.d|xvfrsqrte.s) continue;; esac; \
		printf "Testing %s ... " "$$test"; \
		if [ "$(HOST_X86)" = "1" ]; then \
			output=$$(LD_PRELOAD=./liblasx2lsx.so $(QEMU) $(QEMU_CPU) "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		else \
			output=$$(LD_PRELOAD=./liblasx2lsx.so "$(RANDOM_TEST_DIR)/$$test" -f $(RANDOM_DATA_FILE) 2>&1); \
		fi; \
		expected=$$(cat $(RANDOM_RESULTS_DIR)/$$test.exe.result 2>/dev/null || echo ""); \
		if [ "$$output" = "$$expected" ]; then \
			echo "$$(tput setaf 2)PASSED$$(tput sgr0)"; \
			passed=$$((passed+1)); \
		else \
			echo "$$(tput setaf 1)FAILED$$(tput sgr0)"; \
			failed=$$((failed+1)); \
		fi; \
	done; \
	echo "Random tests: $$passed passed, $$failed failed"; \
	if [ $$failed -gt 0 ]; then exit 1; fi

# Run coverage verification
verify-coverage:
	@python3 tools/verify_coverage.py

# Run usedef batch tests from JIT log files
test-usedef-batch: $(TARGET)
	@python3 tools/gen_usedef_tests.py --log-dir tests/usedef --all

# ============================================================================
# Code coverage (gcov/lcov)
# ============================================================================
COVERAGE_DIR = coverage
GCOV_TOOL = loongarch64-unknown-linux-gnu-gcov
LCOV = lcov
GENHTML = genhtml
COV_CFLAGS = -fprofile-arcs -ftest-coverage -O0 -g
COV_LDFLAGS = -fprofile-arcs

clean-coverage:
	rm -rf $(COVERAGE_DIR)
	find . -name '*.gcda' -delete
	find . -name '*.gcno' -delete

coverage-report:
	mkdir -p $(COVERAGE_DIR)
	rm -f src/lagoon.gcda
	$(LCOV) --gcov-tool $(GCOV_TOOL) --capture --no-external \
		--directory . --output $(COVERAGE_DIR)/coverage.info \
		--rc branch_coverage=1
	$(LCOV) --remove $(COVERAGE_DIR)/coverage.info \
		'*/random_test/*' '*/tests/*' '*/tools/*' '*/include/*' \
		-o $(COVERAGE_DIR)/coverage_filtered.info \
		--rc branch_coverage=1 \
		--ignore-errors unused
	$(GENHTML) --legend --title "liblasx2lsx LASX Emulator Coverage" \
		$(COVERAGE_DIR)/coverage_filtered.info \
		-o $(COVERAGE_DIR)/html
	@echo "Coverage report: file://$(PWD)/$(COVERAGE_DIR)/html/index.html"

coverage: clean-coverage
	@echo "=== Rebuilding library with coverage instrumentation ==="
	rm -f $(OBJ)
	$(MAKE) $(TARGET) CFLAGS="$(CFLAGS) $(COV_CFLAGS)" LDFLAGS="$(LDFLAGS) $(COV_LDFLAGS)"
	@echo "=== Building random test binaries ==="
	$(MAKE) build-random-tests
	@echo "=== Running test suite ==="
	$(MAKE) test
	@echo "=== Running usedef batch tests ==="
	$(MAKE) test-usedef-batch
	@echo "=== Generating coverage report ==="
	$(MAKE) coverage-report

.PHONY: all release debug profile cross clean clean-all test test-native test-loop test-block build-random-tests test-single test-single-native test-random-compare test-boundary test-verbose verify-coverage gen_boundary boundary-data unittest-loop test-usedef-batch coverage clean-coverage coverage-report
