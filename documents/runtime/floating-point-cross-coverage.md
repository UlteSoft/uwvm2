# 浮点跨架构补测与维护约束（2026-09-15）

本次是在[原有修复说明](floating-point-change-rationale.md)之后执行的新一轮补测，包含代码修复，不是原有“仅补注释”提交的测试记录。主仓库和 ROS 分别使用独立源码快照；ROS 不恢复已经删除的 SIMD、lazy 或 tiered 功能。

## 1. 新发现的问题与不能退回的写法

### RV32 的 nearest：解释器与生成对象是两个问题

GCC 15.3 / RV32 glibc 的 nearbyintf 在已测 O0/O3 配置下可以原样返回 sNaN。搬运操作应当保留 sNaN，但 Wasm nearest 是算术操作，结果必须属于允许的 quiet arithmetic NaN 集合。因此 numeric.h 的 RISC-V 输入 NaN 处理也必须包含 nearest，不能只覆盖 ceil/floor/trunc。此前解释器 rounding 回归在 evaluator、普通及融合 byref/tail 路径累计报告 40 个失败；整数桥接路径通过，不能据此认为其他路径也正确。

RV32 没有 FCVT.L.D。LLVM 22 对 f64 roundeven 可能发出 C23 libm roundeven 调用，musl SDK 没有这个符号；exception-ignoring constrained f32 也可能发出 roundevenf。不能用 nearbyint/rint 替换：它们服从当前 FRM，而 nearest 的语义固定为 ties-to-even。

strict_float_jit.h 中的处理只针对 **module 的 RV32 triple**，并且在关闭其他旧架构 bridge 的 early return 之前执行：

- 有 +f 的 f32 保留原生 roundeven；将 fpexcept.ignore constrained 形式转成等价的普通 intrinsic。
- 有 +d 的 f64 对绝对值执行 (x + 2^52) - 2^52，两个指令均明确编码 rne。仅在 |x| < 2^52 时选用该结果；更大有限值已经是整数，inf 保留，NaN 在整数位上 quiet，最后恢复原符号，包括负零。
- LLVM 22 在本次实验中把显式 round.tonearest constrained fadd/fsub 仍生成为 dynamic rounding。默认 FRM 测试通过，但非默认 FRM 回归失败。因此这里使用固定 RNE 的两条 inline asm，不能未经重测改回普通或 constrained 加减。
- 输出约束必须 early-clobber：第二条减法还要读取 bias，输出不能与 bias 寄存器重合。
- 没有对应 F/D 特性，或函数特性未知时，使用完全整数的 f32/f64 ties-even 实现。**先**钳制移位数再执行 shift；不能先产生越界 shift/poison，最后才 select 一个看似安全的结果。
- fixed-vector 的每个 lane 都处理；不擅自处理 scalable vector。strict/maytrap constrained 契约不在此处消除。
- 移除已替换 intrinsic，重复 lowering 不重复插入。缓存 fingerprint 新增 llvm-wasm-rv32-nearest，防止无 git revision 的构建误用旧对象。

这里没有按“支持现代 IEEE NaN”推导 libc 或 LLVM 的每一种 rounding lowering 都正确；也没有把所有普通浮点运算都改成 helper。

### Clang MIPS N32 的 FE_DFL_ENV 不是普通内存指针

N32 使用 32 位指针，但传入寄存器的该 sentinel 需要符号扩展。LLVM 22 的常量序列在本次 BE/LE 测试中产生 0x00000000ffffffff；libc 比较的是 0xffffffffffffffff，比较失败后会解引用 -1，触发 SIGBUS。

set_default_wasm_fp_environment() 仅对 Clang N32 将 FE_DFL_ENV 放入 volatile 指针对象后调用 fesetenv；32 位指针 load 使用带符号扩展的 LW。volatile 防止优化/LTO 把它折回有问题的常量序列。其他 ABI 继续直接调用 libc。这是一处公开入口边界的 load，不是在每条 Wasm 算术前增加 fenv 工作，更不能把它当作 sNaN 浮点返回 ABI 的解决方案。

回归不能自己先直接调用同一条有缺陷的 FE_DFL_ENV 常量路径，否则会在进入被测 guard 前崩溃。环境测试共享生产 helper；fixed-contract 的 strict_float 测试由测试侧宿主建立环境，生产 fixed 分支仍不引入环境切换。

### MIPS musttail 是后端能力与符号可见性的共同约束

Clang 接受 musttail 语法，并不表示 MIPS 后端默认允许尾调用。LLVM 22 的 MIPS 后端默认关闭 tail-call optimization；不打开 -mllvm -mips-tail-calls，实际代码生成会报无法消除尾调用。打开后，直接调用可被 ELF interpose 的符号仍可能不满足条件。

因此 Linux Clang MIPS 构建启用该后端选项，解释器内部 opfunc 的 hot/cold 宏添加 hidden visibility。**不**给整个程序添加全局 hidden，不改变公开 runtime/provider API，也不删除 musttail 改成随指令数增长的宿主递归栈。LLVM 实现参见 [MipsSEISelLowering.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/llvm/lib/Target/Mips/MipsSEISelLowering.cpp) 和 [MipsISelLowering.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/llvm/lib/Target/Mips/MipsISelLowering.cpp)。

uwvm_int_fp_tail_chain.cc 执行 262145 条真实间接 FP neg opfunc，检查最终 IP、sNaN 的非符号位和宿主栈深度。它验证指令分派的有界栈，不等同于完整 runtime 的 callback/lazy/typed-call 测试，也不是 QEMU 性能基准。

### RV32 libc 回退不能编造 syscall 编号

RV32 SDK 不提供旧的 __NR_lseek / __NR_clock_settime。它们对应 llseek/time64 接口，不是只换编号就有相同参数 ABI。fast_io 仅在相应旧 syscall 宏确实存在时使用原 raw 分支，否则交给 libc 的 lseek/clock_settime 处理 off_t、time64 和 ABI。

linux_rv32_libc_fallback.cc 在临时文件上 seek 到 8 GiB + 7，再读取/复位位置，不实际写入 8 GiB。clock setter 只取 volatile 函数指针以保证编译实例化，**不调用、不改系统时间**。这证明缺失 syscall 宏的分支可构建和 seek 宽度正确，不宣称验证了设置系统时钟的行为。

### 未解决：LLVM 22 MIPS32 R6 的高寄存器压力代码生成

Clang/LLVM 22.1.8 在不同实验配置下，主仓两个高压力 SIMD 回归的 O3 构建均出现过 Unsupported instruction / MCInst 0；失败处包括 FP spill/reload 与 GPR/FPR 搬运，汇编输出中也出现 ldc1 $f7, 200($sp)。这是仍未解决的工具链代码生成限制，不能声称该目标的所有 LLVM 路径已经安全验证。

实验性的 -mno-odd-spreg 使 NaN 回归通过，但 bit-transport 回归仍失败；保存汇编后还观察到 cmp.lt.s/mfc1 使用被该选项禁止的 $f1。FPXX、FP64、关闭 SLP 和外部汇编器等替代实验也未形成完整通过的配置。因此 **没有提交该限制，也没有修改 JIT target features 或为它增加 cache key**，避免把不完整的 workaround 固化成生产策略。GCC 的该目标回归通过，但它不是 Clang/JIT 的替代验证。

未优化 IR 经 llc 可以构建并执行同一 bit-transport 测试；优化后的高压力 IR 仍可触发后端失败。不能拿前者代替 O3 验证，也不能为“全绿”整体降级优化或删掉失败测试。相关但未证明同根因的 LLVM 上游报告见 [MIPS MCInst 0 问题](https://github.com/llvm/llvm-project/issues/181442)。本次保留了编译命令、预处理源码、bitcode/汇编及失败日志，供后续缩减复现和检查修复版编译器。

## 2. 本次执行覆盖（不能与历史记录相加后声称全架构覆盖）

环境：GCC 15.3.0 Bootlin 2026.08、Clang/LLVM 22.1.8、QEMU user 10.2.1。所有 Linux 测试位于 /tmp，统一 systemd slice 限制 MemoryMax=64G、MemorySwapMax=0、AllowedCPUs=16-31；启动命令同时使用 taskset -c 16-31。QEMU 的 guest LD_LIBRARY_PATH 显式指向目标 SDK，不能继承宿主 LLVM 路径，尤其 N32 的 /lib32 loader。

| 层次 | 已执行内容 | 结果/边界 |
|---|---|---|
| opfunc / 环境 | RV32 glibc、RV32 musl、AArch64 BE、ARMv5 soft-float、MIPS32 R6 LE、MIPS N32 BE/LE；每个目标 GCC/Clang、O0/O3 | ROS 336/336；主仓最终配置已收回 389 项通过、1 项 R6 Clang O3 SIMD NaN 编译失败；最后 2 项 R6 SIMD bit-transport 复跑结果未收回，不算 PASS |
| RV32 libc 回退 | 两仓、两种 libc、GCC/Clang、O0/O3 | 16/16 编译并执行通过；clock setter 未调用 |
| LLVM native rounding 对象 | RV32 两 libc 的原生及整数 fallback、AArch64 BE、ARMv5 soft、MIPS32 R6；ordinary/constrained × none/O2/O3 | 每仓 42/42 修复后对象执行通过 |
| LLVM legacy/NaN2008 对象 | MIPS O32/N32/N64 × BE/LE × legacy/NaN2008；另 SPARC64、m68k；none/O2/O3 | 每仓 42/42 修复后对象执行通过 |
| 完整 runtime s390x | 主仓解释器/JIT × eager/lazy；ROS 解释器/JIT eager | 主仓 4、ROS 2 种生产入口配置均 0 failures |
| macOS AArch64 | 两仓 check_wasm_fp_environment.py | 各 6/6 测试入口通过；非本机架构的条件分支跳过不算该架构覆盖 |

strict_float.cc 还独立补跑普通环境与 presence-defined fixed 契约（宏定义为 0 也启用）：7 SDK × GCC/Clang × O0/O3 × 两仓 × 两种契约，共 112/112 通过。该轮 R6 Clang 的 8 项仍带实验性寄存器限制，不能冒充撤掉限制后全部重新执行；其他 104 项不受此配置差异影响。这与上表普通 strict_float 测试有重叠，不把两者相加冒充互不重复的测试总数。

最终收尾时 SSH Linux 连接超时：已确认不带实验性寄存器限制的 ROS R6 24/24、两仓 R6 native rounding 对象各 6/6 通过；主仓 R6 NaN O3 构建失败，bit-transport O0/O3 两项尚未收回结果。最后两项不能写成通过或失败，也不能据此声称全矩阵完成。原始产物位于测试机 /tmp/uwvm-fp-coverage.iTbe7Q；release-r6-main、release-r6-ros、release-native-r6-* 和 release-rv32-* 是收尾复跑，verified-* 是前一轮，不能不区分配置直接汇总。

未修复基线作为独立负对照：RV32 glibc 舍入基线执行后报告语义失败；musl 基线是确认缺少 roundeven 符号的**链接失败**，不是执行负对照。MIPS legacy 及 SPARC64/m68k 基线执行失败，NaN2008 基线允许通过。某次 extra SDK 缺少宿主 libopcodes 搜索路径是工具环境失败，补齐该 SDK 的宿主库路径后重新执行，不把第一次失败抹成通过。

s390x 运行的是实际 uwvm_runtime.default.cpp 和生产入口回归，不只是 helper；主仓因已有未使用平台 helper 的 -Werror 诊断，使用 -Wno-error=unused-function，不关闭数值断言。该结果不代表 RV32/MIPS 上完整 LLVM runtime、缓存命中、所有 module 构建也已运行。

### 汇编与性能边界

RV32D 的两 lane f64 舍入 O3 fixture 反汇编从纯整数版本 300 行减少到显式 RNE 版本 143 行；确认 fadd.d/fsub.d 带 rne，没有 FRM CSR 切换，也没有 roundeven/roundevenf 外部符号。其他 ceil/floor 等外部符号仍可能存在。行数只说明代码形态，不能换算成真实硬件提速比例。

在本次 native rounding fixture 中，AArch64 BE 和 ARMv5 soft 的 before 与未跑优化 pipeline 的修复后 none 对象逐字节相同，表明 RV32 专项 lowering 不改变这两个目标的该 fixture。N32 Clang O0 长链样本的 f32/f64 栈距离为 92/100 字节，位检查通过；全部长链测试仍需按各目标单独记录。

fixed-environment 仅省环境保护；不省内存边界、trap、重入、安全 bridge 或数值语义修复。不能因为没有 native import 就推导所有保护都能关掉。

## 3. 可复现入口

安装下列精确 SDK 目录，自行校验发布的 SHA256；配置生成器不会下载或更改系统。官方归档为 https://toolchains.bootlin.com/downloads/releases/toolchains/ 下各架构的 tarballs 目录。

| SDK 目录 | SHA256（tar.xz） |
|---|---|
| riscv32-ilp32d--glibc--stable-2026.08-1 | 716cdf72fbac22707547691c656c50f8c0011d5251668d0aa1bb72ec48083192 |
| riscv32-ilp32d--musl--stable-2026.08-1 | 0ac6a31fcd10c67a8345c7200b20cb2ef26636b28effcb863fb9e8b398d9ff17 |
| mips64-n32--glibc--stable-2026.08-1 | 7efb02627d7312581aca68fd72fb5c67bff76515bd98f7b4d1d0dd7c88fb6ef0 |
| mips64el-n32--glibc--stable-2026.08-1 | 22f31fc2b32634d5f959d5cc449bb2000fc70420767cd7d01a64f1ad54bca515 |
| mips32r6el--glibc--stable-2026.08-1 | dfb0f561343a659b3b0be8f1a68ef290d7b377ae91752111566cca57b43920ae |
| aarch64be--glibc--stable-2026.08-1 | 82aef57aec7bc4d0da48b5a045bef2b4d48b2d9c32f4876563009e43829476a7 |
| armv5-eabi--glibc--stable-2026.08-1 | 80165643bfcbeace9f0fd783399d1f61dcfa7b0b97d3af8f55f9ae855b4cbcec |

在已经配置资源上限、LLVM 工具及宿主库路径的 shell 内：

LLVM 依赖切换后的说明：下面带 `--llvm-config` 的命令属于历史独立后端实验，
用于指定外部比较工具链，不是当前 ROS 的构建入口。ROS 的正式依赖只来自
`third-parties/llvm`，不构建或调用 llvm-config；固定版本的当前结果及直接
CMake 依赖合同见 [ROS LLVM 验证记录](../toolchain/ros-bundled-llvm23.md)。
不能把外部 SDK 实验的通过算成当前 vendor／完整 ROS CLI 已通过。

```sh
python3 test/0013.uwvm_int/fp_cross_bootlin_config.py --sdk-root /path/to/sdks --qemu-dir /path/to/qemu --clang clang++ --output /tmp/fp-config.json
python3 test/0013.uwvm_int/run_fp_cross_matrix.py --config /tmp/fp-config.json --build-dir /tmp/fp-matrix --jobs 6
python3 test/0013.uwvm_int/fp_cross_bootlin_config.py --sdk-root /path/to/sdks --qemu-dir /path/to/qemu --native-rounding --output /tmp/fp-native.json
python3 test/0014.llvm_jit/run_native_rounding_cross.py --target-config /tmp/fp-native.json --llvm-config /path/to/llvm-config --build-dir /tmp/fp-native
```

ROS 生成 opfunc 配置时加 --ros。run_fp_cross_matrix.py 的每项 JSON 保留 build/run 命令、返回码及日志；缺少工具、超时、编译失败均失败退出，不自动降级为通过。复跑 RV32 libc 测试时，只保留配置中的 riscv32 目标，并将 suites 设为 ["libc"]。legacy fixture 使用 run_legacy_nan_cross.py，参数说明见 --help；N32 runner 使用 6058 退出 syscall，不能因指针为 32 位而用 O32 的 4001。

## 4. 仍不能宣称验证完成的范围

version.h/version.cpp 的架构识别名称不等于存在可运行的对应 SDK、libc、LLVM JIT 和回归环境。本轮没有穷尽 ARM32 BE、MIPS soft-float/NaN2008 的完整 libc ABI 组合，也没有在这些新增 SDK 上运行完整 JIT/cache/module 全矩阵。非 Linux 的新增执行证据仅有 macOS AArch64；其他 OS、LoongArch32、ARM64EC 等不能借用 Linux/QEMU 结果声称通过。

QEMU 证明所选 ISA/ABI 下的执行结果，不证明真实芯片的异常实现、内核组合或硬件吞吐。后续换编译器/SDK 或删除 workaround 时，应先保留负对照、O0/O3、全部 lane 和 hostile FRM，再复查实际对象、tail jump 和缓存兼容性。
