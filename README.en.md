[中文](README.md)

# liblasx2lsx

LASX (LoongArch SIMD Extension, 256-bit vector instruction) software emulation and binary translation suite, offering two operating modes:

1. **On-Demand Kernel Module (`lasx2lsx.ko` / `liblasx2lsx-dkms`, recommended)**: Intercepts reserved-instruction traps in the kernel and injects a freestanding PIE runtime on demand — **requiring neither `LD_PRELOAD` nor `/etc/ld.so.preload`**, imposing zero overhead on non-LASX processes, and supporting env-sanitizing launchers, statically linked (`-static`) binaries, runtimes with custom `SIGILL` handlers, and strict `W^X` code pages.
2. **Userspace Preload Library (`liblasx2lsx.so`)**: Runs purely in userspace without kernel privileges via `LD_PRELOAD` + `SIGILL` signal interception.

## Use Case

Some LoongArch platforms (e.g. **2K3000**, **3B6000M**) support only LSX (128-bit vector instructions), not LASX (256-bit). Programs compiled with LASX optimization enabled crash with SIGILL on these platforms.

This library intercepts SIGILL at runtime via LD_PRELOAD and emulates LASX instructions in software, allowing the program to run unmodified.

- If your program already implements runtime detection of LASX support and selects appropriate code paths, this library is unnecessary
- This library addresses: **code compiled assuming LASX is available, running on hardware that lacks it**

## How It Works

```
Application (compiled with LASX instructions)
    │
    ▼
┌─ CPU Execution ─────────────────────────────┐
│ LASX instr → hardware unsupported → SIGILL   │
└──────────────────────────────────────────────┘
    │
    ▼
┌─ liblasx2lsx (LD_PRELOAD) ─────────────────┐
│ sigill_handler()                            │
│   ├─ Read instruction + register state      │
│   ├─ JIT translate → LSX instruction seq    │
│   │   (split 256-bit into 2 × 128-bit)      │
│   └─ Pure C emulation (fallback)            │
│   │                                         │
│  Subsequent: replace original instr with    │
│  jiscr1 → jump to JIT code, no more SIGILL  │
└──────────────────────────────────────────────┘
```

Three JIT optimization tiers:

| Strategy | Description |
|----------|-------------|
| **emu** | Pure C emulation, instruction by instruction |
| **block** | Batch translate consecutive LASX instructions |
| **usedef** | Use/def analysis for register allocation (default) |
| **loop** | Loop detection + full loop JIT (default) |

## Build Requirements

```bash
gcc -mlsx                           # Building the library needs -mlsx (LSX 128-bit)
```

If you only need to use the library (no building), grab a pre-built `.so` — no toolchain needed.

## Building

```bash
make          # Build userspace libraries: liblasx2lsx.so + debug + profile
make deb      # Build userspace package liblasx2lsx_1.0.0_loongarch64.deb
make deb-dkms # Build on-demand DKMS package liblasx2lsx-dkms_1.0.0_all.deb
make kmod     # Build kmod/lasx2lsx.ko directly against the running LoongArch kernel
make clean    # Clean build artifacts
make debug    # Debug build only (liblasx2lsx_dbg.so)
make profile  # Profile build (liblasx2lsx_profile.so)
```

LoongArch hosts use native `gcc`.

## Usage

### On real LoongArch hardware

```bash
# 1. Build the library
make

# 2. Run your program
LD_PRELOAD=./liblasx2lsx.so ./your_loongarch_binary

# 3. Multi-threaded programs work fine
LD_PRELOAD=./liblasx2lsx.so ./your_multithreaded_program
```

### Optimization Control

Set `LIBLASX2LSX_INTERPRET_MODE` environment variable:

```bash
# Default optimization (recommended)
LD_PRELOAD=./liblasx2lsx.so ./your_binary

# Pure emulation, no JIT (debugging)
LIBLASX2LSX_INTERPRET_MODE=emu LD_PRELOAD=./liblasx2lsx.so ./your_binary

# Block mode only (no use/def)
LIBLASX2LSX_INTERPRET_MODE=block LD_PRELOAD=./liblasx2lsx.so ./your_binary
```

Mode reference:

| Mode | Description |
|------|-------------|
| `usedef` | Use/def batch translation optimization |
| `loop` | Loop detection and JIT |
| `frag` | xvmap fragment optimization |
| `block` | Traditional block translation (mutually exclusive with usedef) |
| `emu` / `one` | Pure emulation, all optimization off |

### Disable emulation entirely

```bash
LIBLASX2LSX_INTERPRET=off LD_PRELOAD=./liblasx2lsx.so ./your_binary
```

### How LD_PRELOAD Works

`LD_PRELOAD` is a Linux dynamic linker environment variable. The specified shared library loads before all others. This library uses it as follows:

1. Constructor `register_sigill_handler()` runs automatically at process start
2. Registers a SIGILL signal handler
3. When the program hits a LASX instruction → hardware SIGILL → library intercepts → software emulation
4. After first emulation, replaces the original instruction in-place with `jiscr1`, so subsequent executions jump directly to JIT code without triggering SIGILL

The entire process is transparent to the target program — no code modification or relinking required.

### On-Demand Kernel Module (`LD_PRELOAD`-Free, DKMS)

For system-wide transparent compatibility (e.g. desktop launchers that strip `LD_PRELOAD`, wrapper scripts, `-static` binaries, or Go/JVM/browser runtimes with custom `SIGILL` handlers), install `liblasx2lsx-dkms`:

- **Strict Platform Gating**: By default, the module initializes **strictly on Loongson 3B6000M / 2K3000** (LSX + LBT, no hardware LASX). On CPUs with native LASX (such as 3A6000), module initialization refuses to load (`-ENODEV`) so native performance is never affected.
- **Zero Global Overhead**: Processes that never execute LASX instructions receive zero extra mappings.
- **Multi-Thread & `fork()` Safe**: Uses first-use LBT `EXCCODE_BTDIS` traps to lazily bind `$scr1`/`$scr2` per thread, automatically recycling 64KB context slots from exited threads.

#### Install & Enable (3B6000M / 2K3000)

```bash
# Build and install the DKMS package (builds for the running kernel and auto-loads at boot)
make deb-dkms
sudo dpkg -i liblasx2lsx-dkms_1.0.0_all.deb

# Run any LASX binary directly without LD_PRELOAD:
./your_lasx_binary
```

Runtime JIT tuning and observability via sysfs:

```bash
# Inspect or switch JIT mode (default: usedef,loop,frag; options: block / one / off)
cat /sys/module/lasx2lsx/parameters/interpret_mode
echo block | sudo tee /sys/module/lasx2lsx/parameters/interpret_mode

# Inspect trap and injection counters
head /sys/module/lasx2lsx/parameters/stat_*
```

#### Simulation & Verification on 3A6000

3A6000 is supported strictly for verification by passing `sim_3a6000=1` together with a mandatory `target_comm` prefix filter (intercepting `do_lasx` to simulate 3B6000M reserved-instruction traps):

```bash
make kmod kmod-tests
sudo insmod kmod/lasx2lsx.ko sim_3a6000=1 target_comm=test_lasx
env -i ./kmod/tests/test_lasx_raw_ri
env -i ./kmod/tests/test_lasx_suite
env -i ./kmod/tests/test_lasx_static
sudo rmmod lasx2lsx
```

## Testing

```bash
make test                        # Default optimization (703 random tests)
make test-one                    # Pure emulation mode
make test-native                 # QEMU native LASX comparison
make test-single TEST=xvadd.b    # Single instruction test
make test-random-compare SEED=N  # Compare native vs emu with seed
```

## Project Structure

```
src/
├── include/          # Header files
├── emu/              # SIGILL handler + instruction emulation + thread data
├── interpret/        # JIT translation layer (batch translation + optimization)
└── lagoon.c          # LoongArch assembler (instruction encoding emission)

kmod/                 # LD_PRELOAD-free on-demand kernel module & DKMS packaging
├── lasx2lsx_kmod.c   # Kernel module (trap hook, lazy injection, LBT init, W^X COW patch)
├── rt/               # Freestanding PIE runtime glue and build scripts
└── tests/            # Freestanding (-nostdlib), static (-static), MT, and W^X tests

tools/                # Utility scripts
tests/                # Unit tests
random_test/          # Random test framework (703 instructions)
doc/
├── development.md    # Developer docs: architecture, debugging, improvements
└── ...               # Other technical docs
```

## Developer Reference

Architecture details, debugging guide, and known bug patterns:

- [doc/development.md](doc/development.md) — Development documentation

## License

- Userspace library and JIT core (`src/`): MIT
- Kernel module (`kmod/`): GPL-2.0-only (shared protocol header and runtime glue dual-licensed under `GPL-2.0-only OR MIT`)
