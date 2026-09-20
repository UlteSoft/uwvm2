# 浮点舍入、原生 NaN 与指令生成补充审计（2026-09-15）

这些修复与前序 ABI/环境修复的关系、代码位置及维护约束见 [维护注释索引](floating-point-change-rationale.md)。

这是 [浮点环境与位模式审计](floating-point-environment.md) 的后续补丁。环境控制正确，不代表宿主舍入 builtin、NaN 编码和 LLVM lowering 自动满足 Wasm。

## 复现的问题与修复

- GCC 15 的 SSE2 `ceil/floor/trunc` 可能原样返回 signaling NaN。uwvm-int 普通/融合指令和 i386 JIT bridge 都受影响。没有 SSE4.1 的 SSE2 构建改用整数位舍入，同时处理 signed zero、subnormal、ties-to-even 和 NaN；现代原生舍入路径不变。
- HPPA、SH4 和 legacy-NaN MIPS 使用不同的 signaling-bit 约定。仅设置 Wasm quiet bit 不够：宿主可改变 canonical 输入的 payload。算术结果在受影响目标上规范化为 Wasm canonical NaN；HPPA 的窄化转换还会把小 payload NaN 变成 infinity，因此必须在转换前按整数位分类。
- 68881、SPARC 的 invalid 算术可产生 all-ones 默认 NaN。Wasm 在没有非 canonical NaN 输入时要求 canonical 结果，已覆盖标量、融合算术和主仓库 SIMD。
- PPC32/PPC64 的 f32→f64 promotion 可保留 signaling NaN；NaN 输入显式产生 canonical f64，有限值保留原生转换。
- RISC-V 原生 LLVM `ceil/floor/trunc` 同样会保留 signaling NaN，不能只修解释器。公共 lowering 根据生成目标只规范化舍入结果，覆盖 scalar/vector、普通/constrained intrinsic；普通算术不改走 bridge。
- 原生 NaN 规范化与扩展精度 lowering 的顺序：先让整数 bridge 处理需要正确舍入的算术，再规范化残留原生指令，避免在 m68k 上重复处理 vector 结果。
- LLVM 对象缓存增加 `llvm-wasm-native-nan=canonical-native-rounding-v1`，隔离旧的数值语义。

纯搬运、reinterpret、abs、neg、copysign 不经过算术 NaN 规范化。ROS 同步公共标量/JIT 修复，没有重新引入 SIMD、lazy 或 tiered 功能。

规则依据：[WebAssembly 数值语义](https://webassembly.github.io/spec/core/exec/numerics.html)。NaN 的 quiet 与 canonical 条件不能混为一谈；符号操作的位精确保留也不同于算术指令允许的 NaN 结果集合。

## 验证

Linux `/tmp/uwvm-fp-next.iYf7DZ`，Clang/LLVM 22、GCC 15、QEMU user；本任务统一使用 64 GiB 上限、无 swap、CPU 16–31（16 个 E-core）。i686 用原生 32 位加载器，其余交叉目标用 QEMU。

- 两仓分别通过 `uwvm_int_fp_rounding`、`uwvm_int_fp_nan_encoding` 的 24 组配置，共 96 次执行。覆盖 Clang/GCC x86_64、i686 x87/SSE2/SSE4.1、O0/O3、AArch64、ARMhf、PPC32/PPC64 BE/LE/AltiVec、RISC-V64、s390x、LoongArch64、Alpha、m68k O0/O3、HPPA、SPARC64、SH4。
- 舍入用例每组比较 758,784 次，含所有 binade 边界、相邻值、正负 ties 和确定性随机位模式。期望值由独立 `modf/fmod` oracle 计算，不调用被测舍入函数或 bridge；整数转换 oracle 直接解码二进制有理数，不依赖宿主 FP→integer 转换。
- 主仓库 `uwvm_int_simd_nan_encoding` 的 24 组配置通过，包含混合有限 lane、NaN 输入、无 NaN 输入的 invalid 算术及升/降精度。
- i386 LLVM：无 SSE/SSE2/SSE4.1 × none/O2/O3 × x87/SSE2 bridge，共 18 组通过；补充混合多返回值、直接/间接调用和整数/饱和转换边界。
- MIPS O32/N64、大小端，以及 SPARC64、m68k：6 个目标 × none/O2/O3，共 18 组生成对象通过；测试检查 lowering 幂等性和 IR verifier，并保留未修复对照的失败日志。m68k 使用生产整数 bridge，不能以与宿主 libc ABI 不符的原生 FP libcall 替代。
- 原生 LLVM 舍入：x86_64、AArch64、ARMhf、PPC32/PPC64 BE/LE、RISC-V64、s390x、LoongArch64，共 9 个目标 × 普通/constrained × none/O2/O3 = 54 组通过；包含 scalar/vector 的 ceil、floor、trunc、nearbyint、roundeven。RISC-V 对照版本失败，修复后通过。运行器使用生产 FP 环境 guard，包括 PPC VSCR.NJ。

生产 raw-byte 入口回归在原有 14 项位模式操作外增加了 4 项舍入：主仓库 x86_64 full/lazy × interpreter/LLVM，以及 ROS x86_64、RISC-V64 full-only × interpreter/LLVM 全部通过。RISC-V64 这里实际执行了完整 JIT，不只是交叉生成对象。三项新增标量/SIMD 单元另通过 x86_64 UBSan 检查。

独立测试可通过相应 xmake test target 运行。交叉 LLVM 回归入口：

```sh
python3 test/0014.llvm_jit/run_fp_bits_cross.py --help
python3 test/0014.llvm_jit/run_legacy_nan_cross.py --help
python3 test/0014.llvm_jit/run_native_rounding_cross.py --help
```

脚本接收工具路径、SDK/sysroot 与 QEMU 路径，保留生成 IR、对象和日志。原生舍入脚本中的 `--sdk-root`/`--extra-sdk-root` 使用 `usr/bin/<triple>-g++-15` 与 `usr/<triple>` 布局。

## 性能与边界

- 同配置 x86_64 生产对象对比：主仓库 9,653 个、ROS 3,706 个解释器 opfunc，代码字节及重定位全部相同；包括 scalar FP、融合算术、整数、local、provider global。
- 最终生产 runtime 微基准（每组 before/after 各 4 次交错运行、每次 7 个样本、每样本 1,000 万轮）：主仓库 arithmetic/provider 中位耗时变化 −0.027%/−0.012%，ROS 为 −0.270%/+0.015%，结果位均为 `46189a80`。所测运行未显示性能退化；RISC-V 反汇编保留原生 `fcvt` 舍入，未新增通用算术 bridge。
- SSE2 有限值舍入微基准：绑定 CPU 16，3 轮 before/after 交错、每轮每操作 7 个样本、每样本 1,000 万次。f32 ceil/floor/trunc/nearest 中位耗时分别变化 −8.70%/−3.69%/−31.52%/−19.33%；f64 为 −3.98%/−3.12%/−17.55%/−15.10%。这只证明所测 helper 微基准，不是整体 Wasm 吞吐或所有架构的性能保证。
- 新保护只针对已复现的数值语义问题；没有关闭 fenv、内存边界、trap、调用深度、重入或 bridge token 检查。RISC-V/legacy NaN 的必要处理可能增加对应指令成本，不将其描述为零开销。
- 生成对象测试不等于在每个目标安装并执行了完整 LLVM JIT。没有完成 MIPS N32、所有 NaN2008/软件库组合、LoongArch32、所有 RV32、ARM64EC 或全部非 Linux OS 的完整运行时验证，也不宣称这些结果证明整个沙箱安全。
