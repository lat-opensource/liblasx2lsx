# AGENTS.md - liblasx2lsx

提供 LASX（LoongArch SIMD）指令模拟的 C 库，通过 SIGILL 信号捕获和 LD_PRELOAD 实现。

## 项目结构

源代码按功能组织在 `src/` 目录下：

```
src/
├── include/          # 共享头文件
│   ├── debug.h
│   ├── loongarch-extcontext.h
│   ├── perf_idx.h
│   ├── perf_stats.h
│   ├── thread_data.h
│   ├── lasx_emu_private.h
│   ├── lasx_emu.h
│   ├── lasx_arith.h
│   ├── lasx_logic.h
│   ├── lasx_mem.h
│   ├── lasx_imm.h
│   ├── lasx_muldiv.h
│   ├── lasx_extract.h
│   ├── lasx_bitmanip.h
│   ├── lasx_cmp.h
│   ├── lasx_fbasic.h
│   ├── lasx_fma.h
│   ├── lasx_fcmp.h
│   ├── lasx_perm.h
│   ├── lasx_extadd.h
│   ├── lasx_shift.h
│   ├── lagoon.h
│   └── lasx_interpret.h
├── emu/              # 模拟核心
│   ├── sigill_hook.c      # SIGILL 信号处理器
│   ├── thread_data.c      # pthread 线程本地存储
│   ├── lasx_emu.c         # 核心模拟入口和指令分发
│   ├── lasx_arith.c       # 算术运算
│   ├── lasx_logic.c       # 逻辑运算
│   ├── lasx_mem.c         # 内存操作
│   ├── lasx_imm.c         # 立即数操作
│   ├── lasx_muldiv.c      # 乘除法
│   ├── lasx_extract.c     # 提取操作
│   ├── lasx_bitmanip.c    # 位操作
│   ├── lasx_cmp.c         # 比较操作
│   ├── lasx_fbasic.c      # 基础浮点
│   ├── lasx_fma.c         # 融合乘加
│   ├── lasx_fcmp.c        # 浮点比较
│   ├── lasx_perm.c        # 置换操作
│   ├── lasx_extadd.c      # 扩展加法
│   ├── lasx_shift.c       # 移位操作
│   ├── lasx_shift_n.c     # 窄化移位
│   ├── lasx_shift_ni.c    # 窄化立即数移位
│   └── perf_stats.c       # 性能统计
├── interpret/        # 解释层
│   ├── lasx_interpret.c
│   ├── lasx_arith_interpret.c
│   ├── lasx_logic_interpret.c
│   ├── lasx_mem_interpret.c
│   ├── lasx_imm_interpret.c
│   ├── lasx_muldiv_interpret.c
│   ├── lasx_extract_interpret.c
│   ├── lasx_bitmanip_interpret.c
│   ├── lasx_cmp_interpret.c
│   ├── lasx_fbasic_interpret.c
│   ├── lasx_fma_interpret.c
│   ├── lasx_fcmp_interpret.c
│   ├── lasx_perm_interpret.c
│   ├── lasx_extadd_interpret.c
│   └── lasx_shift_interpret.c
└── lagoon.c          # LoongArch 汇编器/JIT

tools/                # 工具脚本
├── add_lsx_intrinsics.py
├── check_lasx_opcodes.py
├── check_rmw.py
├── convert_to_intrinsics.py
├── find_missing_instructions.py
├── list_missing.py
├── verify_coverage.py
├── zl.py
├── find_missing.sh
└── run_parallel_tests.sh

tests/                # 测试程序
random_test/          # 随机测试框架
```

## 编译命令

```bash
make all          # 编译全部：liblasx2lsx.so + 测试 + 调试版
make              # 等同于 make all
make cross        # 交叉编译为 LoongArch（设置 CROSS_COMPILE 前缀）
make clean        # 清理编译产物
```

**自动行为：**
- x86_64 主机：自动使用 loongarch64-unknown-linux-gnu-gcc 交叉编译
- LoongArch 主机：默认使用 gcc 本地编译

**关键编译标志：**
- `-DCONFIG_LOONGARCH_NEW_WORLD` - 新版内核（x86_64交叉编译默认启用，LoongArch 本地自动检测）

## 使用方法

库通过 LD_PRELOAD 加载：

```bash
LD_PRELOAD=./liblasx2lsx.so your_program
```

## 关键约束（重要！）

### 1. Switch结构与OP前缀对应关系

**核心规则：`shift = 32 - OP编号`**

每个 `case OPxx_XXX:` 必须放在对应的 switch 语句中：

| OP前缀 | Switch位移 | 正确写法 |
|--------|-----------|---------|
| OP14 | `instr >> 18` | `switch (instr >> 18) { case OP14_...` |
| OP15 | `instr >> 17` | `switch (instr >> 17) { case OP15_...` |
| OP17 | `instr >> 15` | `switch (instr >> 15) { case OP17_...` |
| OP22 | `instr >> 10` | `switch (instr >> 10) { case OP22_...` |
| OP24 | `instr >> 8` | `switch (instr >> 8) { case OP24_...` |

**违反规则的后果：**
- 指令无法正确区分，导致错误的指令被错误处理
- 即使覆盖率显示100%，实际运行也会出错

### 2. 代码格式化

使用 clang-format 统一格式：

```bash
clang-format -i src/emu/lasx_emu.c
```

配置见 `.clang-format` 文件。

**不要用 Python 脚本手动转换格式**，容易出错。

### 3. 覆盖率验证

添加新指令后，验证 switch 结构正确性：

```python
import re
with open('src/include/lasx_emu_private.h') as f:
    priv = f.read()
opcode_defs = {m.group(1): int(m.group(2)) for m in re.finditer(r'#define (OP(\d+)_\w+)', priv)}

with open('src/emu/lasx_emu.c') as f:
    c = f.read()

# 验证各switch
for m in re.finditer(r'switch \(instr >> (\d+)\)', c):
    shift = int(m.group(1))
    expected_op = 32 - shift
    block = c[m.start():m.end()+500]
    cases = re.findall(r'case (OP\d+_\w+):', block)
    wrong = [x for x in cases if x in opcode_defs and opcode_defs[x] != expected_op]
    print(f"shift={shift} OP{expected_op}: {len(cases)}条, 错误{len(wrong)}")
```

### 4. 已知警告

编译时 `lasx_fbasic.c` 的 strict-aliasing 警告是原代码问题，不影响功能，无需修复。

## 测试规则

### 测试Targets

```bash
make test              # 运行正常测试（使用模拟库）
make test-native      # 无模拟运行，验证测试代码正确性
make test-debug       # 运行已知问题的测试（调试用）
make test-debug-native # 无模拟运行debug测试
```

### 测试目录结构

- `tests/` - 所有测试源文件
- 测试文件命名：`test_*.c`

### 添加新测试步骤

1. **创建测试文件**：在 `tests/` 目录下创建 `test_xxx.c`

2. **编写测试代码**：
   - 使用 LoongArch 内联汇编调用 LASX 指令
   - 内存布局：源A(32B) + 源B(32B) + 结果(32B)
   - 使用 `xvld` 加载，`xvst` 存储

3. **寄存器使用**：
   - `$xr0`, `$xr1`, `$xr2` 等向量寄存器
   - 指令格式：`xvld $xrN, offset(base)`
   - 指令格式：`xvadd.$xrN, $xrM, $xrK`

4. **添加到 Makefile**：
   - 正常测试：添加到 `TESTS` 变量
   - 已知问题测试：添加到 `TESTS_DEBUG` 变量

### 常见问题排查

#### 1. test-native 失败但 test 通过

**可能原因**：
- 测试预期值计算错误（测试代码bug）
- QEMU LASX 实现与硬件行为不同
- 指令语义理解错误

**排查步骤**：
```bash
# 1. 先用 test-native 验证是测试代码问题还是模拟器问题
make test-native

# 2. 如果 test-native 通过，说明模拟器实现有问题
# 3. 如果 test-native 也失败，检查预期值计算

# 4. 手动验证预期值
# 例如：xvandn.v 语义为 rd = rk & ~rj，不是 a & ~b
```

#### 2. 常见指令语义差异

| 指令 | 正确语义 | 常见错误 |
|------|---------|---------|
| `xvandn.v` | `rd = rk & ~rj` | `rd = rj & ~rk` |
| `xvorn.v` | `rd = rk \| ~rj` | `rd = rj \| ~rk` |
| `xvclo.h` | 输出 8-bit 计数/16-bit | 按 16-bit 全宽输出 |
| `xvclz.w` | 输出 8-bit 计数/32-bit | 按 32-bit 全宽输出 |

#### 3. 编译失败

- **binutils 版本**：某些指令（如 xvexth, vext2xv, xvextrins）需要较新版本的 binutils
- **QEMU LASX bug**：某些指令在 QEMU 原生 LASX 模式下会触发 SIGILL

### 测试验证流程

1. **先运行 test-native**：
   - 确认测试代码正确
   - 如果失败，修复测试预期值

2. **再运行 test**：
   - 确认模拟器实现正确
   - 如果失败，检查模拟器代码

3. **test-debug**：
   - 用于调试已知问题的测试
   - 不会导致主测试套件失败

### 测试输出格式

```c
// 推荐输出格式
printf("instruction_name: %s\n", errors ? "FAILED" : "PASSED");

// 详细错误信息
if (errors) {
    printf("Mismatches at: index, got value, expected value\n");
}
```

### 已知问题测试（加入 TESTS_DEBUG）

以下情况应加入 `TESTS_DEBUG` 而非 `TESTS`：

1. QEMU 原生 LASX 下 SIGILL 的测试
2. QEMU LASX 实现与硬件语义不同的指令
3. binutils 版本不支持的指令

示例：
```
# test_xvimm: QEMU native LASX SIGILL
# test_xvand: xvandn/xvorn 寄存器顺序不同
# test_xvcmp: xvclo/xvclz h/w/d 输出格式问题
```

## 使用 Intrinsics 编写测试

### 简介

当 binutils 版本不支持某些指令（如 xvshuf.f, xvfcmp, xvpermr, xvreplve 等）时，可以使用 GCC intrinsics 绕过汇编器限制。Intrinsics 由 GCC 直接生成指令，不经过汇编器。

### intrinsics 文件结构

```c
#include <lasxintrin.h>

int main() {
    unsigned char mem[96] __attribute__((aligned(32)));
    
    // 加载数据
    __m256i a = __lasx_xvld(mem, 0);
    __m256i b = __lasx_xvld(mem, 32);
    
    // 执行指令
    __m256i r = __lasx_xvadd_w(a, b);
    
    // 存储结果
    __lasx_xvst(r, mem, 64);
    
    return 0;
}
```

### 常用 intrinsics

| 类别 | Intrinsic | 说明 |
|-----|-----------|------|
| 加载存储 | `__lasx_xvld(addr, offset)` | 加载 32 字节 |
| | `__lasx_xvst(value, addr, offset)` | 存储 32 字节 |
| 整型运算 | `__lasx_xvadd_w(a, b)` | 加法 (word) |
| | `__lasx_xvmul_h(a, b)` | 乘法 (half) |
| | `__lasx_xvsadd_b(a, b)` | 饱和加法 (byte) |
| 位操作 | `__lasx_xvand_v(a, b)` | AND |
| | `__lasx_xvor_v(a, b)` | OR |
| | `__lasx_xvandn_v(a, b)` | AND NOT |
| 浮点 | `__lasx_xvfcmp_clt_s(a, b)` | 浮点小于比较 |
| | `__lasx_xvfmadd_s(a, b, c)` | 融合乘加 |
| | `__lasx_xvfrsqrt_s(a)` | 倒数平方根 |
| 置换 | `__lasx_xvshuf_w(a, b, sel)` | 元素混洗 |
| | `__lasx_xvpermi_w(a, b, imm)` | 立即数置换 |
| | `__lasx_xvreplve_b(a, idx)` | 元素复制 |
| 扩展 | `__lasx_xvexth_h_b(a)` | 符号扩展 (byte→half) |
| | `__lasx_vext2xv_w_b(a)` | GPR 扩展到向量 |
| | `__lasx_xvextrins_b(a, b, imm)` | 位提取插入 |

### 编译配置

在 Makefile 中添加 `-mlasx` 标志：

```makefile
# 需要 intrinsics 的测试
test_xvperm_full: CFLAGS += -mlasx
test_xvfcmp: CFLAGS += -mlasx
test_xvand: CFLAGS += -mlasx
test_xvextadd: CFLAGS += -mlasx
test_xvbit_full: CFLAGS += -mlasx
test_xvmul_full: CFLAGS += -mlasx
test_xvextract: CFLAGS += -mlasx
test_xvfbasic_full: CFLAGS += -mlasx
```

### Intrinsics vs 内联汇编

| 特性 | Intrinsics | 内联汇编 |
|-----|------------|---------|
| 汇编器依赖 | 无 (GCC 直接生成) | 需要 binutils 支持 |
| 指令覆盖 | 全部 LASX 指令 | 受限于 binutils 版本 |
| 可移植性 | 更好 | 需要版本检查 |
| 可读性 | C 函数调用风格 | 需熟悉汇编语法 |

### 已知问题

1. **xvfrstp 需要 3 个参数**：
   ```c
   // 错误: __lasx_xvfrstp_b(a, b)
   // 正确:
   __m256i c = __lasx_xvrepli_b(0);
   __m256i r = __lasx_xvfrstp_b(a, b, c);
   ```

2. **浮点类型转换**：
   ```c
   // 加载浮点向量
   __m256 v = *(__m256*)mem;  // 或使用 intrinsics
   __m256d vd = *(__m256d*)mem;
   
   // 存储时需转换
   __lasx_xvst((__m256i)r, mem, 64);
   ```

3. **Emulator 限制**：
   - 使用 intrinsics 的测试在 `make test` (QEMU LASX=off) 下运行
   - 如需 native LASX，加入 `TESTS_DEBUG`

### 查找可用 Intrinsics

```bash
# 查看所有 intrinsics
grep "__lasx_xv" /usr/lib/gcc/loongarch64-unknown-linux-gnu/15.1.0/include/lasxintrin.h | head -50

# 或参考官方文档
# https://gcc.gnu.org/onlinedocs/gcc-15.1.0/gcc/LoongArch-ASX-Vector-Intrinsics.html
```

## 测试配置

### 测试Targets

```bash
make test                     # 运行363个随机测试（使用模拟库，与预期结果对比）
make test-native              # 运行363个随机测试（QEMU原生LASX，与预期结果对比）
make test-random-compare SEED=xxx  # 使用指定SEED生成随机数据，对比native和emu结果
make test-single TEST=xvadd.b         # 测试单条指令（使用默认random_data）
make test-single TEST=xvadd.b SEED=12345  # 测试单条指令（使用指定SEED生成随机数据）
make test-single-native TEST=xvadd.b SEED=12345  # 无模拟测试单条指令
```

### 测试目录结构

- `tests/` - 所有测试源文件 (22个)
- `random_test/` - 随机测试框架
  - `src/` - 363个指令测试源文件
  - `result/` - 预期结果文件
  - `random_data` - 测试数据 (8个uint64_t)

### 当前配置

- **TESTS (15个)** - 通过测试：
  - test_sigill, test_xvadd, test_xvfbasic, test_xvfma, test_xvimm
  - test_xvcmp, test_xvldst, test_xvmul, test_xvperm, test_xvsh
  - test_xvext, test_xvbit, test_xvbiti, test_debug, test_xvperm_full

- **TESTS_DEBUG (7个)** - 已知问题：
  - test_xvand, test_xvextadd, test_xvbit_full, test_xvmul_full
  - test_xvfcmp, test_xvfbasic_full, test_xvextract

### 测试Targets

```bash
make test                     # 运行363个随机测试（使用模拟库，与预期结果对比）
make test-native              # 运行363个随机测试（QEMU原生LASX，与预期结果对比）
make test-random-compare SEED=xxx  # 使用指定SEED生成随机数据，对比native和emu结果
make test-single TEST=xvadd.b         # 测试单条指令（使用默认random_data）
make test-single TEST=xvadd.b SEED=12345  # 测试单条指令（使用指定SEED生成随机数据）
make test-single-native TEST=xvadd.b SEED=12345  # 无模拟测试单条指令
```

### 测试目录结构

- `random_test/` - 随机测试框架
  - `src/` - 363个指令测试源文件 (*.c)
  - `result/` - 预期结果 (*.exe.result)
  - `test/` - 编译后的测试二进制文件
  - `random_data` - 默认测试数据 (64字节 = 8个uint64_t)

#### 测试结果

| 模式 | 通过 | 失败 | 说明 |
|------|------|------|------|
| test-native | 703 | 0 | QEMU 原生 LASX |
| test | 703 | 0 | 模拟器模式 |

### 已知模拟器问题（已排除测试）

以下指令因 QEMU 实现差异在 `test-random-compare` 中被排除：

| 指令 | 问题描述 |
|------|---------|
| `xvfrecipe.d/s` | 精度差异 |
| `xvfrsqrte.d/s` | 精度差异 |
| `xvflogb.d/s` | 行为差异 |

#### 并行测试脚本

```bash
./run_parallel_tests.sh [并行任务数] [测试数量]
# 例如:
./run_parallel_tests.sh 4 20    # 4个并行任务，运行20个随机seed测试
```

失败时输出：
```
- SEED=xxxxx
    Failed instructions:
      make test-single TEST=xxx SEED=xxxxx
      make test-single-native TEST=xxx SEED=xxxxx
```

可使用指定 SEED 快速复现和调试。
| `xvshuf4i.b/h/w/d` | 立即数变体 shuffle 逻辑错误 |

这些是模拟器实现问题，需要修复。

#### 测试文件格式

每个随机测试文件结构相同：

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static void asm_test(void *mem) {
    __asm__ volatile(
        "xvld $xr0, %0, 0\n"
        "xvld $xr1, %0, 32\n"
        "xvadd.b $xr2, $xr1, $xr0\n"
        "xvst $xr2, %0, 64\n"
        ::"r"(mem):"memory","$xr0","$xr1","$xr2");
}

uint64_t data[32][4];

int main(int argc, char *argv[]) {
    const char *filename = NULL;
    // 解析 -f 参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            filename = argv[++i];
        }
    }
    if (!filename) {
        fprintf(stderr, "Usage: %s -f <filename>\n", argv[0]);
        return 1;
    }
    
    FILE *in = fopen(filename, "rb");
    uint64_t input[8];
    fread(input, sizeof(uint64_t), 8, in);
    
    // 加载数据到向量寄存器
    data[0][0] = input[0]; data[0][1] = input[1];
    data[0][2] = input[2]; data[0][3] = input[3];
    data[1][0] = input[4]; data[1][1] = input[5];
    data[1][2] = input[6]; data[1][3] = input[7];
    
    asm_test(data);
    
    // 输出结果
    printf("0x%016lx 0x%016lx 0x%016lx 0x%016lx\n",
           data[2][0], data[2][1], data[2][2], data[2][3]);
    return 0;
}
```

#### 使用随机数据测试

生成新的测试数据：

```bash
# 编译数据生成器
gcc -o gen_random random_test/gen_random.c

# 生成8个uint64_t的随机数据
./gen_random -f random_test/data_new -n 8
```

运行单条指令测试（以 xvadd.b 为例）：

```bash
# 编译测试文件
loongarch64-unknown-linux-gnu-gcc -o random_test/test/xvadd.b random_test/src/xvadd.b.c

# 使用模拟器运行
LD_PRELOAD=./liblasx2lsx.so qemu-loongarch64-static -cpu la464,lasx=off \
    ./random_test/test/xvadd.b -f random_test/random_data

# 与预期结果对比
./random_test/test/xvadd.b -f random_test/random_data > output.txt
diff output.txt random_test/result/xvadd.b.exe.result
```

#### 添加新的随机测试

1. 在 `random_test/src/` 创建测试文件（如 `xvxxx.y.z.c`）
2. 编译：`loongarch64-unknown-linux-gnu-gcc -o random_test/test/xvxxx.y.z random_test/src/xvxxx.y.z.c`
3. 在 QEMU 原生模式下生成预期结果：
   ```bash
   qemu-loongarch64-static ./random_test/test/xvxxx.y.z -f random_test/random_data > random_test/result/xvxxx.y.z.exe.result
   ```

### 已知Emulator问题

1. **xvexth.s/d** - test_xvfbasic_full: 模拟器结果错误
2. **xvextract** - test_xvextract: QEMU SIGILL
3. **xvand/xvorn 顺序** - test_xvand: 寄存器顺序不同

这些不是代码bug，是QEMU模拟限制。

## 向量指令模拟经验

### 重要：持续更新本文档

每当发现新的问题、模式或解决方案时，必须更新 AGENTS.md。这是项目的知识库，需要持续积累。

### 向量布局理解

LASX 向量寄存器为 256 位（32 字节），元素按小端序排列：

```
D[0] = W[0](低32位) + W[1](高32位) = B[0-3] + B[4-7]
D[1] = W[2](低32位) + W[3](高32位) = B[8-11] + B[12-15]
D[2] = W[4](低32位) + W[5](高32位) = B[16-19] + B[20-23]
D[3] = W[6](低32位) + W[7](高32位) = B[24-27] + B[28-31]
```

**关键点**：
- 读 `vreg_read_64(reg, i)` 返回 D[i]，包含 W[2*i] 和 W[2*i+1]
- 取 W[2*i]（偶数）：`val & 0xffffffff`
- 取 W[2*i+1]（奇数）：`(val >> 32) & 0xffffffff`

### 向后扫描与循环优化

当触发 SIGILL 的指令在循环中间时，通过向后扫描找到循环起始：

**问题场景**：
```
循环: [xvld] [xvfmadd] [addi] [xvld] [xvfmadd] [addi] [bnez]
        0        1        2       3        4        5      6
                                ↑
                             触发点 (pc_offset=3)
```

**解决方案**：
1. **向后扫描**：`detect_loop_range()` 新增 `pc_offset` 参数，从触发点向前扫描找循环起始
2. **翻译整个循环**：从循环起始开始翻译，生成 JIT 代码
3. **修改循环起始为 jiscr1**：将循环第一条指令替换为 jiscr1，指向翻译后的代码
4. **当前指令正常翻译**：继续翻译当前及后续指令
5. **循环执行时**：下次循环执行到第一条指令时，jiscr1 会跳转到翻译后的代码

**状态管理**：
```c
static unsigned int *detected_loop_start = NULL;  // 已检测循环的起始位置
static unsigned int *detected_loop_end = NULL;    // 已检测循环的结束位置
void *detected_loop_jit_entry = NULL;             // 翻译后的 JIT 代码入口
```

**避免重复优化**：
```c
static int is_loop_already_optimized(unsigned int *loop_start)
{
    unsigned int instr = *loop_start;
    if ((instr & 0xfc000000) == 0x48000000) {  // jiscr1 指令
        return 1;
    }
    return 0;
}
```

**退出机制**：
翻译后的循环以 `jiscr0` 指令结束，偏移量为循环长度：
```c
int loop_len = info->len > 0 ? info->len : -info->len;
int exit_offset = (loop_len - 1) * 4 + 4;
la_jiscr0(as, exit_offset);
```

**执行流程**：
1. p[3] 触发 SIGILL
2. `check_lasx_loop_pattern()` 向后扫描找到循环起始 p[0]
3. 检查 p[0] 不是 jiscr1（未优化）
4. 翻译整个循环，生成 JIT 代码
5. 记录 `detected_loop_start = p[0]`, `detected_loop_end = p[7]`
6. 返回 -7（负值表示需要修改循环起始）
7. 修改 p[0] 为 jiscr1（指向 JIT 代码）
8. 继续 while 循环，翻译 p[3]（单条指令 JIT）
9. 翻译 p[4], p[5], p[6]
10. n += 7，跳出 while 循环

下次执行：
1. p[0] 的 jiscr1 触发，进入翻译后的循环代码
2. 循环执行完毕，jiscr0 跳转到 p[7]
3. 继续执行后续代码

### 常见错误模式

#### 1. 错误的索引计算

**错误**：
```c
// _d_w: 源 W[0-7] → 目标 D[0-3]
for (int i = 0; i < 4; i++) {
    vreg_read_64(xj, i * 2);  // 错误！D[0],D[2],D[4],D[6] - 超出范围
    vreg_write_64(xd, i, ...);  // 错误！写到了错误位置
}
```

**正确**：
```c
for (int i = 0; i < 4; i++) {
    vreg_read_64(xj, i);      // 正确！D[0],D[1],D[2],D[3]
    vreg_write_64(xd, i, ...); // 正确！
}
```

#### 2. 覆盖问题

**错误**：
```c
for (int i = 0; i < 8; i += 2) {
    // word_idx = 0,0,1,1,2,2,3,3
    int word_idx = i / 4;
    // 写入 idx = word_idx/2 = 0,0,0,0,1,1,1,1 - 严重覆盖！
    vreg_write_64(xd, word_idx / 2, ...);
}
```

**正确**：按目标元素数量循环
```c
for (int i = 0; i < 4; i++) {  // _d_w 输出 4 个 D 元素
    vreg_read_64(xj, i);
    vreg_write_64(xd, i, ...);
}
```

#### 3. 使用错误的写粒度

**问题**：对于 `_h_b`（16 个 16 位元素）使用 `vreg_write_64`

**后果**：每次写入 64 位会覆盖相邻元素

**正确**：
| 指令模式 | 元素数 | 元素宽度 | 应使用函数 |
|---------|-------|---------|-----------|
| `_h_b` / `_h_bu` | 16 | 16-bit | `vreg_write_16` |
| `_w_h` / `_w_hu` | 8 | 32-bit | `vreg_write_32` |
| `_d_w` / `_d_wu` | 4 | 64-bit | `vreg_write_64` |
| `_q_d` / `_q_du` | 2 | 128-bit | `vreg_write_64` + `vreg_write_128` |

### XVADDWEV/WOD 指令语义

```
XVADDWEV.H.B: for i in range(16) XR[vd].H[i] = SignExtend(XR[vj].B[2i], 16)+SignExtend(XR[vk].B[2i], 16)
XVADDWEV.W.H: for i in range(8)  XR[vd].W[i] = SignExtend(XR[vj].H[2i], 32)+SignExtend(XR[vk].H[2i], 32)
XVADDWEV.D.W: for i in range(4)  XR[vd].D[i] = SignExtend(XR[vj].W[2i], 64)+SignExtend(XR[vk].W[2i], 64)
XVADDWEV.Q.D: for i in range(2)  XR[vd].Q[i] = SignExtend(XR[vj].D[2i],128)+SignExtend(XR[vk].D[2i],128)

XVADDWOD.H.B: for i in range(16) XR[vd].H[i] = SignExtend(XR[vj].B[2i+1],16)+SignExtend(XR[vk].B[2i+1],16)
XVADDWOD.W.H: for i in range(8)  XR[vd].W[i] = SignExtend(XR[vj].H[2i+1],32)+SignExtend(XR[vk].H[2i+1],32)
XVADDWOD.D.W: for i in range(4)  XR[vd].D[i] = SignExtend(XR[vj].W[2i+1],64)+SignExtend(XR[vk].W[2i+1],64)
XVADDWOD.Q.D: for i in range(2)  XR[vd].Q[i] = SignExtend(XR[vj].D[2i+1],128)+SignExtend(XR[vk].D[2i+1],128)
```

**带 U 后缀变体（第一个操作数零扩展）：**

```
XVADDWEV.H.BU.B: for i in range(16) XR[vd].H[i] = ZeroExtend(XR[vj].B[2i], 16)+SignExtend(XR[vk].B[2i], 16)
XVADDWEV.W.HU.H: for i in range(8)  XR[vd].W[i] = ZeroExtend(XR[vj].H[2i], 32)+SignExtend(XR[vk].H[2i], 32)
XVADDWEV.D.WU.W: for i in range(4)  XR[vd].D[i] = ZeroExtend(XR[vj].W[2i], 64)+SignExtend(XR[vk].W[2i], 64)
XVADDWEV.Q.DU.D: for i in range(2)  XR[vd].Q[i] = ZeroExtend(XR[vj].D[2i],128)+SignExtend(XR[vk].D[2i],128)

XVADDWOD.H.BU.B: for i in range(16) XR[vd].H[i] = ZeroExtend(XR[vj].B[2i+1], 16)+SignExtend(XR[vk].B[2i+1], 16)
XVADDWOD.W.HU.H: for i in range(8)  XR[vd].W[i] = ZeroExtend(XR[vj].H[2i+1], 32)+SignExtend(XR[vk].H[2i+1], 32)
XVADDWOD.D.WU.W: for i in range(4)  XR[vd].D[i] = ZeroExtend(XR[vj].W[2i+1], 64)+SignExtend(XR[vk].W[2i+1], 64)
XVADDWOD.Q.DU.D: for i in range(2)  XR[vd].Q[i] = ZeroExtend(XR[vj].D[2i+1],128)+SignExtend(XR[vk].D[2i+1],128)
```

- **wev**（even）：取偶数位置元素（0,2,4,...）
- **od**（odd）：取奇数位置元素（1,3,5,...）
- **U 后缀**：第一个操作数（vj）零扩展，第二个操作数（vk）符号扩展

### 实现检查清单

添加新指令时检查：

1. **循环次数**：输出多少元素就循环多少次
2. **读写索引**：
   - 读：`vreg_read_64(xj, i)` - 直接用 i
   - 写：`vreg_write_X(xd, i, ...)` - 直接用 i
3. **写粒度**：根据元素宽度选择正确的 vreg_write_ 函数
4. **元素提取**：
   - 偶数位置：`val & 0xffffffff`（D[i] 的低 32 位）
   - 奇数位置：`(val >> 32) & 0xffffffff`（D[i] 的高 32 位）

### 饱和算术指令注意事项

饱和算术指令（如 xvsadd, xvssub）需要注意溢出时的符号处理：

#### xvsadd.w 示例

```c
// 错误示例：使用 uint32_t 会导致符号丢失
int64_t res = (int64_t)a + (int64_t)b;
if (res > INT32_MAX) res = INT32_MAX;
if (res < INT32_MIN) res = INT32_MIN;
uint32_t c = (int32_t)res;  // 错误！负数会变成大正数

// 正确做法：直接存回位字段
uint64_t dst = vreg_read_64(uc, td, xd, dword_idx);
dst &= ~(0xffffffffULL << shift);
dst |= ((uint64_t)(int32_t)res << shift);  // 正确：先转 int32_t 再转 uint64_t
vreg_write_64(uc, td, xd, dword_idx, dst);
```

**关键点**：
- 不要用 `uint32_t c = (int32_t)res` 这样的中间变量
- 直接将 `(int32_t)res` 转换为 `uint64_t` 后写入位字段
- 或者使用 `vreg_write_32` 直接写入 32 位值

### 元素交错读写模式

对于 XVADDWEV/XVADDWOD 这类指令，源操作数是交错排列的：

#### XVADDWEV.H.B（偶数位置相加）

```
XR[vj].B[0], XR[vk].B[0] → XR[vd].H[0]
XR[vj].B[2], XR[vk].B[2] → XR[vd].H[1]
...
XR[vj].B[30], XR[vk].B[30] → XR[vd].H[15]
```

**实现模式**：
```c
// 遍历输出元素（16个half）
for (int i = 0; i < 16; i++) {
    // 计算源字节位置：偶数索引
    int byte_idx = i * 2;
    int dword_idx = byte_idx / 8;
    int shift = (byte_idx % 8) * 8;

    // 读取源数据
    uint64_t src_j = vreg_read_64(uc, td, xj, dword_idx);
    uint64_t src_k = vreg_read_64(uc, td, xk, dword_idx);

    // 提取字节并扩展
    int8_t a = (int8_t)((src_j >> shift) & 0xff);
    int8_t b = (int8_t)((src_k >> shift) & 0xff);

    // 计算并写入
    int16_t res = (int16_t)a + (int16_t)b;
    vreg_write_16(uc, td, xd, i, (uint16_t)res);
}
```

#### XVADDWOD.H.B（奇数位置相加）

```
XR[vj].B[1], XR[vk].B[1] → XR[vd].H[0]
XR[vj].B[3], XR[vk].B[3] → XR[vd].H[1]
...
XR[vj].B[31], XR[vk].B[31] → XR[vd].H[15]
```

**实现模式**：
```c
// 遍历输出元素（8个half，因为只有奇数位置）
for (int i = 0; i < 8; i++) {
    // 计算源字节位置：奇数索引
    int byte_idx = i * 2 + 1;
    int dword_idx = byte_idx / 8;
    int shift = (byte_idx % 8) * 8;

    // 读取和计算
    uint64_t src_j = vreg_read_64(uc, td, xj, dword_idx);
    uint64_t src_k = vreg_read_64(uc, td, xk, dword_idx);
    int8_t a = (int8_t)((src_j >> shift) & 0xff);
    int8_t b = (int8_t)((src_k >> shift) & 0xff);
    int16_t res = (int16_t)a + (int16_t)b;

    // 写入第 i 个 half
    vreg_write_16(uc, td, xd, i, (uint16_t)res);
}
```

### 常见错误模式汇总

| 错误类型 | 错误代码 | 正确代码 |
|---------|---------|---------|
| 错误的写粒度 | `vreg_write_64(..., i/2, ...)` | `vreg_write_16(..., i, ...)` |
| 错误的输出索引 | `vreg_write_64(..., word_idx/2, ...)` | `vreg_write_16(..., i, ...)` |
| 错误的循环方式 | `for (i=0; i<16; i+=2)` 每次处理2个 | `for (i=0; i<16; i++)` 每次处理1个 |
| 饱和运算符号丢失 | `uint32_t c = (int32_t)res;` | `dst \|= ((uint64_t)(int32_t)res << shift);` |
| 错误的源索引计算 | `i*2` 而非 `i*2+1` (for odd) | 根据 wev/od 选择正确偏移 |

### 测试失败排查

当测试失败时，按以下步骤排查：

1. **test-native 和 test 都失败，且错误值相同**
   - 原因：测试代码 bug（预期值计算错误或内存布局问题）
   - 解决：检查测试代码的预期值计算逻辑

2. **test-native 通过，test 失败**
   - 原因：模拟器实现 bug
   - 解决：检查指令实现代码

3. **test 通过，test-native 失败**
   - 原因：测试预期值基于硬件行为，与 QEMU 模拟器不同
   - 解决：将测试移至 TESTS_DEBUG

#### xvsadd.w 测试失败案例

测试 `xvsadd.w` 在 native 和 QEMU 模式下都失败，错误值完全相同：
```
xvsadd.w[0]: got 1682319370, expected -2147483648
```

这表明测试代码本身有问题（可能为预期值计算错误或大小端问题），而非模拟器 bug。

### 实际修复案例

#### lasx_extadd.c 修复记录

本次修复了以下函数：

| 函数 | 问题 | 修复方式 |
|------|------|---------|
| `xvsadd.w` | `uint32_t c = (int32_t)res` 导致符号丢失 | 改为 `dst \|= ((uint64_t)(int32_t)res << shift)` |
| `xvaddwev_h_b` | 使用 `vreg_write_64` 写 16-bit | 改用 `vreg_write_16` |
| `xvsubwev_h_b` | 同上 | 改用 `vreg_write_16` |
| `xvaddwod_h_b` | 同上 | 改用 `vreg_write_16` |
| `xvsubwod_h_b` | 同上 | 改用 `vreg_write_16` |
| `xvsubwod_h_bu` | 同上 | 改用 `vreg_write_16` |
| `xvsubwod_w_hu` | 使用 `vreg_write_64` 写 32-bit | 改用 `vreg_write_32` |

### Read-Modify-Write Bug（关键！）

这是最常见且最严重的 bug。当目标寄存器 xd 与源寄存器 xj 或 xk 相同时，会导致错误结果。

#### 问题本质

循环内每次读取 xd 后写入，当 xd==xj 或 xd==xk 时，后续迭代会读到已被修改的值：

```c
// 错误写法（有严重 bug）
for (int i = 0; i < N; i++) {
    uint64_t src_j = vreg_read_64(uc, td, xj, idx);
    uint64_t src_k = vreg_read_64(uc, td, xk, idx);
    // ... 计算 ...
    uint64_t dst = vreg_read_64(uc, td, xd, idx);  // xd==xj 时读到已修改的值！
    // ... 修改 ...
    vreg_write_64(uc, td, xd, idx, dst);
}
```

#### 正确模式：按 Slot 原子处理

正确做法：每个 slot 一次性读完 xj/xk，计算完再写 xd：

```c
// 正确写法
for (int slot = 0; slot < 4; slot++) {
    uint64_t src_j = vreg_read_64(uc, td, xj, slot);
    uint64_t src_k = vreg_read_64(uc, td, xk, slot);
    uint64_t dst = 0;  // 初始化为 0 或从 xd 读取初始值
    for (int i = 0; i < ELEMENTS_PER_SLOT; i++) {
        // 从 src_j/src_k 读取，不是从 xd
        // ... 处理 ...
    }
    vreg_write_64(uc, td, xd, slot, dst);
}
```

#### 受影响的文件

| 文件 | 修复的函数 |
|------|-----------|
| `lasx_arith.c` | xvadd.b/h, xvsub.b/h |
| `lasx_imm.c` | xvaddi.bu/hu/wu, xvsubi.bu/hu/wu |
| `lasx_extract.c` | vext2xv 全部 12 个变体 |
| `lasx_bitmanip.c` | xvbitclr_b/h/w/d, xvbitset_b/h/w/d, xvbitrev_b/h/w/d, xvbitclri_b/h/w/d, xvbitseti_b/h/w/d, xvbitrevi_b/h/w/d, xvsigncov_b/h/w/d, xvfrstp_b/h, xvbitseli_b, xvandi/xvori/xvxori/xvnori, xvsat 全部变体 |
| `lasx_cmp.c` | xvmax.b/h/w/d, xvmin.b/h/w/d, xvmax.bu/hu/wu/du, xvmin.bu/hu/wu/du, xvclo.b/h/w, xvclz.b/h/w, xvneg.b/h/w/d, xvmskltz.b/h/w/d, xvmskgez.b, xvmsknz.b, xvseqi.b/h/w/d, xvslei.b/h/w/d, xvslti.b/h/w/d, xvslei.bu/hu/wu/du, xvslti.bu/hu/wu/du, xvmaxi.b/h/w/d, xvmini.b/h/w/d, xvmaxi.bu/hu/wu/du, xvmini.bu/hu/wu/du |

#### 修复检查清单

每次实现新指令后，检查是否符合正确模式：

1. **是否在循环内读取 xd？** 
   - 如果是，且 xd 可能等于 xj 或 xk，必须重构

2. **是否按 slot 读取源操作数？**
   - 应该先 `vreg_read_64(xj, slot)` 和 `vreg_read_64(xk, slot)`
   - 然后在内部循环处理各元素
   - 最后一次性 `vreg_write_64(xd, slot, dst)`

3. **立即数操作（imm 变体）是否正确？**
   - 虽然不需要读 xk，但仍然不能循环内读 xd
   - 应该初始化 `dst = 0`，然后在循环内累积结果

### vext2xv 实现要点

vext2xv 指令从向量寄存器 XR[xj] 扩展到向量寄存器 XR[xd]，不是从 GPR 扩展：

| 指令 | 源操作数 | 元素数 | 扩展类型 |
|------|---------|-------|---------|
| `vext2xv_h_b` | XR[xj] | 16 | byte→half 符号扩展 |
| `vext2xv_w_h` | XR[xj] | 8 | half→word 符号扩展 |
| `vext2xv_d_w` | XR[xj] | 4 | word→dword 符号扩展 |
| `vext2xv_h_bu` | XR[xj] | 16 | byte→half 零扩展 |
| `vext2xv_w_hu` | XR[xj] | 8 | half→word 零扩展 |
| `vext2xv_d_wu` | XR[xj] | 4 | word→dword 零扩展 |

**常见错误**：从 GPR 读取而非 XR[xj]

### XVSRLN/XVSSRAN 窄化移位实现

窄化移位指令将较大元素尺寸（word→half, half→byte）的数据右移后存入较小元素尺寸。

#### XVSRLN.B.H 伪码

```asm
for i in range(8) :
  VR[vd].B[i] = SRLN(VR[vj].H[i], VR[vk].H[i]%16)
  VR[vd].B[i+16] = SRLN(VR[vj].H[i+8], VR[vk].H[i+8]%16)
for i in range(8, 16, 1) :
  VR[vd].B[i] = 0
  VR[vd].B[i+16] = 0
```

**关键点**：
- 输入：xj.H[i] (16-bit)，xk.H[i] (移位量)
- 输出：xd.B[i] (8-bit)
- **非连续存储**：B[0-7]在D0，B[16-23]在D2（不是D1）
- 高半部分需清零：B[8-15]→D1，B[24-31]→D3

#### 正确实现模式

```c
void do_lasx_emu_xvsrln_b_h(ucontext_t* uc, unsigned int instr)
{
    thread_data_t* td = thread_data_get();

    int xd = (instr >> 0) & 0x1f;
    int xj = (instr >> 5) & 0x1f;
    int xk = (instr >> 10) & 0x1f;

    for (int i = 0; i < 8; i++) {
        uint64_t src_j0 = vreg_read_64(uc, td, xj, i / 4);
        uint16_t a0 = (src_j0 >> ((i % 4) * 16)) & 0xffff;
        uint64_t src_k0 = vreg_read_64(uc, td, xk, i / 4);
        uint8_t shamt0 = (src_k0 >> ((i % 4) * 16)) & 0xf;
        vreg_write_8(uc, td, xd, i, a0 >> shamt0);

        uint64_t src_j1 = vreg_read_64(uc, td, xj, i / 4 + 2);
        uint16_t a1 = (src_j1 >> ((i % 4) * 16)) & 0xffff;
        uint64_t src_k1 = vreg_read_64(uc, td, xk, i / 4 + 2);
        uint8_t shamt1 = (src_k1 >> ((i % 4) * 16)) & 0xf;
        vreg_write_8(uc, td, xd, i + 16, a1 >> shamt1);
    }
    vreg_write_64(uc, td, xd, 1, 0);
    vreg_write_64(uc, td, xd, 3, 0);
}
```

**经验总结**：

1. **使用 vreg_write_8**：对于单字节写入，使用 vreg_write_8(idx=0-31) 自动处理 slot 计算

2. **源操作数索引**：
   - H[0-7] 存储在 slot [0,1,2,3]，每 4 个 half 占一个 slot
   - H[0-3] → slot 0, H[4-7] → slot 1, H[8-11] → slot 2, H[12-15] → slot 3
   - 公式：`slot = h_idx / 4`，`bit_shift = (h_idx % 4) * 16`

3. **目标存储位置**：
   - B[0-7] → D0 (slot 0)
   - B[8-15] → D1 (slot 1) - 需清零
   - B[16-23] → D2 (slot 2)
   - B[24-31] → D3 (slot 3) - 需清零

4. **高半部分清零**：使用 vreg_write_64 一次性清零整个 slot

## 内核按需加载方案 (`kmod/` & `liblasx2lsx-dkms`)

除 `LD_PRELOAD=./liblasx2lsx.so` 用户态方案外，`kmod/` 目录实现了无需 `LD_PRELOAD`、无需 `/etc/ld.so.preload`、对信号/TLS/线程/环境变量零侵入的内核按需注入方案（`lasx2lsx.ko`）：

### 目录结构

```
kmod/
├── lasx_kproto.h       # 内核与独立 PIE 运行时共享的 64KB 线程上下文协议头
├── lasx2lsx_kmod.c     # 内核模块（DIE_RI 拦截、按需映射、LBT $scr1/$scr2 延迟绑定、W^X COW 补丁）
├── lasx_rt_blob.S      # 通过 .incbin 嵌入预构建的 lasx_rt.bin
├── Kbuild / Makefile   # Out-of-tree Kbuild 构建规则
├── dkms.conf.in        # DKMS 配置模板
├── lasx2lsx.service    # systemd 启动单元（仅在 3B6000M / 2K3000 自动加载）
├── rt/                 # 无 libc 依赖的独立 PIE 运行时构建目录（复用 src/emu 与 src/interpret）
└── tests/              # 裸机（-nostdlib）、静态链接（-static）、多线程与 W^X 专项测试集
```

### 平台初始化约束（生产环境仅支持 3B6000M / 2K3000）

- **生产模式（默认 `sim_3a6000=0`）**：模块初始化时严格校验处理器型号，**仅在龙芯 3B6000M / 2K3000（LA364 系列，支持 LSX + LBT 且硬件无 LASX）上允许加载**；在 3A6000 等原生支持 LASX 的平台上默认返回 `-ENODEV` 拒绝初始化，避免误伤原生性能。
- **3A6000 验证模式（仅测试用途）**：需显式传入 `sim_3a6000=1 target_comm=<进程名前缀>`（禁止空前缀或 `*`），额外拦截 `EXCCODE_LASXDIS`（`do_lasx`）以模拟 3B6000M / 2K3000 的保留指令例外。

### 构建与验证命令

```bash
make kmod-rt      # 构建独立 PIE 运行时镜像 (kmod/lasx_rt.bin + kmod/lasx_rt_meta.h)
make deb-dkms     # 生成 liblasx2lsx-dkms_1.0.0_all.deb（可在 x86_64 交叉构建或在 LoongArch 本地构建）
make kmod         # 在 LoongArch 目标机针对当前内核构建 kmod/lasx2lsx.ko
make kmod-tests   # 构建 kmod/tests/ 下的按需模拟验证测试集

# 在 3B6000M / 2K3000 生产环境加载：
sudo modprobe lasx2lsx

# 在 3A6000 上模拟验证（必须指定 target_comm）：
sudo modprobe lasx2lsx sim_3a6000=1 target_comm=test_lasx
env -i ./kmod/tests/test_lasx_raw_ri
env -i ./kmod/tests/test_lasx_suite
env -i ./kmod/tests/test_lasx_static
```
