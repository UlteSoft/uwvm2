# 浮点环境边界、契约快路径与跨架构验证

## 范围与结论

这里的“关闭保护”仅指浮点环境的保存、建立和恢复。它不允许关闭 Wasm 内存边界、间接调用类型检查、trap、调用深度、重入或 JIT bridge token 检查。
浮点环境正确，也不等于整个数值后端、所有编译器/ABI 或整个沙箱已经得到安全证明。

默认路径继续保护纯 uwvm-int 和 LLVM JIT：公共执行入口建立 Wasm 环境并在退出时恢复宿主完整环境；不具备保持环境契约的 native function，以及 native global get/set 回调，在返回 Wasm 前恢复环境。
ROS 移植保留其 full-only 执行模型，没有重新引入 lazy、tiered 或其他被裁剪的功能。

## 何时可以使用快路径

原生目标默认**不启用** `UWVM_ASSUME_FIXED_WASM_FP_ENVIRONMENT`。只有嵌入者能同时保证以下条件时，才可以在整个构建中一致定义此宏：

1. 每个执行线程的每次入口已经使用 round-to-nearest/ties-to-even、渐进下溢、屏蔽浮点异常，以及后端要求的精度控制；不能只初始化主线程。x87/68881 正确舍入快路径要求 53 或 64 位有效精度，PC=24 不满足 f64 契约。
2. 覆盖所有实际使用的标量和向量执行单元，包括 JIT 根据 CPU 额外启用的单元。
3. native function/global/memory/metadata 回调、插件、编译路径及异步信号处理程序均保持这些控制位。异常展开必须符合运行时契约，不能靠 longjmp 等跳过必要的清理。
4. 宿主同意把运算产生的浮点异常状态标志视为 caller-saved；快路径不再承诺恢复进入 Wasm 前的异常标志。
5. 宿主、运行时、头文件使用方和模块构建保持宏一致，且编译使用正确的浮点语义，不能用 fast-math/FTZ 链接初始化破坏前提。
6. 数值后端本身已经符合 Wasm 语义。固定环境不会修复扩展精度双重舍入等后端问题。

满足这些条件后，统一添加编译定义 `-DUWVM_ASSUME_FIXED_WASM_FP_ENVIRONMENT` 并完整重编译。宏按“是否定义”生效，定义为 `0` **也会开启**；恢复默认保护应移除定义。
这是一份信任契约，不是运行时检测开关。不能因为模块没有浮点指令、只启用解释器、仅运行 WASI 或 native ABI 通常保持控制位，就推断契约成立。

Wasm32/Wasm64 宿主自动采用固定环境路径，因为合规的外层 WebAssembly 引擎提供规定的运算语义，内部没有可写的宿主浮点 CSR；这仍然依赖外层引擎正确实现语义。
较小范围的优化应优先使用已有的单个 native function `preserves_wasm_control` 契约，避免信任整个进程的回调集合。

即使启用快路径，活动标记的嵌套恢复仍然保留，JIT 重入和 token 边界也不删除。扩展精度目标可省去每次算术的控制字读取，但仍检查 midpoint/subnormal，并保留必要的正确舍入回退；不能把这个宏理解为关闭数值正确性检查。
原生回调本来就能访问宿主地址空间；FP guard 不能把恶意原生代码变成沙箱内代码。

## 架构适配

| 目标/配置 | 默认保护与本次处理 |
|---|---|
| x86_64、启用 SSE 的 i386 家族 | 高频 global 边界保存 x87 CW 和 MXCSR；先用 no-wait 状态检查/必要时 FNCLEX 清除延迟异常，再恢复 CW。使用原生 SSE2 数学时普通 opfunc 不增加检查；仍使用 x87 的构建另走精确舍入路径。 |
| 未启用 SSE 的 i386 家族 | 使用完整 libc fenv 回退，避免忽略 JIT/回调可能使用的 SSE 单元；不无条件执行 SSE 指令。扩展精度运算使用下述正确舍入路径。 |
| AArch64 LE/BE、Clang ARM64EC | 高频边界直接保存/恢复 FPCR。ARM64EC 即使定义 `__x86_64__`，也必须选择 AArch64 指令和 LLVM target 初始化。 |
| ARM32 hard-float | 完整 fenv 回退；测试 FPSCR.FZ、舍入和 subnormal。软浮点 ABI 不能被当作没有硬件 FP 的证明。 |
| RISC-V 硬件 FP | 高频边界 FRRM/FSRM，仅恢复 FRM，状态标志 caller-saved；依据 `__riscv_flen`，不是参数传递 ABI。 |
| RISC-V 无硬件 FP 构建 | fenv 回退；JIT 不额外启用 F/D/V，避免软件 libc 不维护硬件 FCSR。软浮点算术实现仍需单独验证。 |
| PPC32/PPC64，显式 AltiVec | 除 libc FPSCR 外保存 VSCR，入口清 VSCR.NJ，回调后恢复；向量所有 lane 掩码避免大小端假设。 |
| Linux generic PPC32/PPC64 | 根据 OS HWCAP 检测 AltiVec，用不跨向量 ABI 的隔离汇编保存 VSCR；即使宿主未用 `-maltivec`，也保护 JIT 向量运算。 |
| 非 Linux generic PPC | 没有可靠的运行时 VSCR 探测时，限制 JIT AltiVec/VSX；要启用这些扩展，应使用能保护它们的 AltiVec 构建。未测试所有相关 OS。 |
| MIPS MSA | 显式 MSA 构建补充 MSACSR，入口设 IEEE 默认值；非 MSA 构建限制 JIT MSA，避免遗漏 CSR 和 FR=0 ABI 问题。 |
| m68k/68881 | 补充标量 FPCR 保存/恢复，避免精度位遗漏及当前 QEMU 的多控制寄存器传输问题；扩展精度算术另使用正确舍入路径。 |
| LoongArch、Alpha、HPPA、SPARC、s390x、SH4 等 | 保留完整 libc fenv，不把未实现的专用汇编分支变成空保护。SH4 测试只使用实现提供的舍入模式。 |

PPC 的 libc fenv 还涉及 OS 异常使能状态，不能为了省调用仅用 `mffs/mtfsf` 替换。SH4 的 libc 也维护 FPSCR 相关软件状态。
参考实现：[glibc PPC fesetenv](https://github.com/bminor/glibc/blob/master/sysdeps/powerpc/fpu/fesetenv.c)、[glibc SH4 fesetenv](https://github.com/bminor/glibc/blob/master/sysdeps/sh/sh4/fpu/fesetenv.c)、[glibc m68k fesetenv](https://github.com/bminor/glibc/blob/master/sysdeps/m68k/fpu/fesetenv.c)。

## 验证范围（2026-09-14）

Linux 测试位于 `/tmp/uwvm-fp-portable.B2sYn3`。所有本任务重型编译/测试在 `uwvm-fp-review.slice` 中执行，限制合计内存 64 GiB、禁用 swap，并绑定 16–31 共 16 个 E-core。
工具为 Clang/LLVM 22.1.8、GCC 15、QEMU user 10.2.1；Apple AArch64 另以系统 Clang 原生运行独立测试。

独立测试矩阵为 17 个配置 × 4 项通用测试，68 项通过：

- Linux x86_64 native；i686 默认 x87 和 SSE2 两种配置。
- AArch64、ARM hard-float、PPC64 BE generic/AltiVec、PPC64 LE、PPC32 BE。
- RISC-V64、s390x、LoongArch64、Alpha、m68k、HPPA、SPARC64、SH4。

另外四个 PPC 配置通过 `wasm_fp_ppc_generic.cc`，包含真实 `vaddfp` subnormal 运算、hostile VSCR 入口和回调恢复；该文件在非 PPC 上不执行架构用例，不将这些空分支计入覆盖率。
MIPS64r6 N64 LE/BE 在 I6400 QEMU 上通过实际 MSA CSR 的 freestanding 探针，没有声称完成整个 MIPS C++ 运行时或 SIMD 算术验证。
ARM64EC、AArch64 BE、RISC-V32/64 的专用边界通过实际目标汇编编译检查；ARM64EC 生成 MRS/MSR FPCR，而非 x87。
RISC-V64 的硬件 FP + soft 参数 ABI，以及无 FPU ISA 的回退选择，分别做了真实目标编译检查。
Wasm32/64 的实际固定环境头文件无需 libc 即可编译、链接，并在外层引擎执行嵌套标记探针。

生产入口回归：主仓库 x86_64 纯解释器/组合构建及 AArch64 组合构建通过，按构建启用的模式覆盖 full/lazy/tiered 路径；ROS x86_64 纯解释器/组合构建均在 TLS 和 map 两种状态存储下通过，RISC-V64 组合构建通过 QEMU 回归。
ROS 的 AArch64、PPC64 BE、LoongArch64 纯解释器也通过生产入口回归。PPC64 ELFv1 使用 GNU linker，而不是不支持该 ABI 的 LLD；主仓库 AArch64 的既有未使用函数警告在测试命令中降为警告，未修改无关生产代码。

`version.h`/模块版本文件中的架构名称是识别信息，不等于此 ABI 的后端已经符合规范。下表补齐其余标签的审计状态，不能把它们写成“全部测试通过”：

| 标签或未覆盖变体 | 状态/需要的额外验证 |
|---|---|
| ARM64EC | 代码生成检查；缺 Windows ARM64EC 运行环境，未作整机/完整 JIT ABI 认证。 |
| AArch64 BE、ARM32 BE | 前者边界汇编通过；未运行对应大端 libc/完整运行时。 |
| i386/i486/i586 早期 CPU | i686 x87 与 LLVM i386 代码生成已测试；不等于每个旧 CPU、操作系统及无 FPU 软件库已认证。 |
| MIPS32/64 O32/N32/N64、R2、BE/LE | 仅上述 N64 MSA CSR 子集实测；其余 ABI 缺 C++ sysroot，不能推断通过。 |
| LoongArch32 | LoongArch64 的结果不能替代；缺对应 sysroot/runtime。 |
| RISC-V32、无 FPU/软浮点库 | RV32 仅汇编；软件 FP 和所有可选向量扩展组合未完整运行。 |
| PPC32 LE/SPE、RS/6000、AIX | Linux PPC BE/LE 结果不能替代 SPEFSCR 或 AIX fenv/ABI；需独立系统与工具链。 |
| s390 31-bit、System/370、z/Architecture 标签 | s390x 实测；其他 ABI 或非 IEEE 算术模式未认证。 |
| SPARC v8、其他 SuperH、ColdFire | SPARC64/SH4/68881 结果不覆盖这些变体。 |
| Blackfin、Convex、E2K、Xtensa LE/BE、IA64 | 无匹配的可执行 C++/libc 环境；默认 fenv 分支保留，但没有运行验证。 |
| PDP11/PDP10/PDP7、AVR、CUDA PTX、Unknown | 不能仅凭版本标签承诺 native Wasm 后端；非 IEEE float/double、位宽、OS/线程及执行模型均需适配。`precfloat.h` 已检查基础 IEEE 类型，检查通过仍不是运算舍入正确性的证明。 |

### 扩展精度双重舍入：已修复

`tools/ci/probes/wasm_fp_excess_precision.cpp` 是诊断探针，不纳入“环境 guard 通过”计数：

```
f64 bits(0x3ff0000000000000) + bits(0x3ca0000000000001)
正确结果：0x3ff0000000000001
未修复的 GCC 15 i686 x87 / m68k C++ 运算：0x3ff0000000000000
strict_float 与生产 uwvm-int 路径：0x3ff0000000000001
```

`double` 与 `_Float64` 都复现，尽管舍入模式已是 FE_TONEAREST。问题是扩展精度中间值先舍入、存储到 f64 后再次舍入。
不能靠关闭保护、只改类型名称，或简单固定 x87 精度位，就证明所有 underflow/运算情况正确。
本次增加 `compiler/shared/strict_float.h`，覆盖 f32/f64 的 add/sub/mul/div/sqrt、整数到浮点及 f64 demote。普通 numeric、convert、heavy/extra-heavy 合并运算、byref 路径均复用它；合并运算仍按每条 Wasm 指令分别舍入，不变成 FMA。主仓库的 SIMD 标量回退也使用该实现；ROS 不引入原本不存在的 SIMD/lazy/tiered 功能。

正确性回退使用 64 位有效精度、向零舍入执行扩展运算，读取硬件 inexact 标志并将其 jam 到最低有效位，随后用整数实现 ties-to-even 的最终 f32/f64 打包。有限 Wasm 输入的这些运算不会溢出或下溢扩展指数范围，因此 sticky 信息不会提前丢失。该方法依据 [Boldo/Melquiond 的 round-to-odd 结果](https://guillaume.melquiond.fr/doc/05-imacs17_1-article.pdf)，同时处理目标 subnormal、溢出、NaN 和 signed zero。

常见路径不每次切换控制字：在 RN、异常屏蔽、精度足够时，先执行并显式存储原生结果。f64 只接受正常指数范围内、扩展低 11 位不等于 midpoint 的结果（以及正确的零）；midpoint、目标 subnormal 和特殊值重新走正确性回退。也支持 53 位精度控制下的正常 f64 结果。f32 输入在 53/64 位中间有效精度下有足够保护位。控制字只读，FLDCW/FPCR 写入留在回退分支；回退函数禁止内联，避免在每个 opfunc 内复制控制切换和整数打包代码。SSE2、AArch64 等原生 IEEE 目标在编译期完全绕过这些检查。

LLVM JIT 在模块优化前将受影响目标的浮点算术/转换及 sqrt 改写成整数参数、整数返回值的版本化 bridge，覆盖标量和 fixed-vector 的每个 lane。如果便携 x87 构建运行在支持 SSE2 的宿主上，JIT 函数实际的 `target-features` 明确启用 SSE2 时，算术/sqrt/demote 保留原生 IR；不因为解释器的编译选项较保守，就给原本正确的 JIT 算术增加 helper 调用。整数转换仍保护，因为 32 位后端可能借助 x87 处理 i64。保留 opaque call 的副作用，防止优化跨越改变 FP 控制的回调。full、lazy、tiered 的模块优化入口均覆盖；读取对象缓存也重新绑定稳定符号，缓存指纹包含算法版本和 native/extended 模式，避免复用旧对象或跨模式复用。ROS 仅接入其 full 路径。

### 新增数值验证与模拟器边界

新增测试位于 Linux `/tmp/uwvm-strict-fp.lthTPP`；资源限制与前述环境测试相同。使用独立 MPFR oracle，按其 [IEEE 范围与 subnormalize 文档](https://www.mpfr.org/mpfr-current/mpfr.html) 传递 ternary 结果，避免 oracle 自己双重舍入。

- 每份 corpus 为 454,005 组：随机位模式、正常/非正常数、半途边界、整数转换半途两侧、NaN/inf/zero。
- i386 原生 32 位执行通过 CW=037f/027f/007f/077f/0b7f/0f7f 六种控制；m68k QEMU 通过 FPCR=0/80/10/20/30 五种控制。每种均逐位对照完整 corpus。
- LLVM 的 30 个标量/向量 IR 运算通过不启用时零改写、扩展精度路径清除原始 FP 运算、IR verifier、幂等及 MCJIT 执行检查。额外验证 SSE2 快路径保留 22 个原生算术/sqrt/demote，8 个整数转换仍受保护，且显式 `-sse2` 能关闭该快路径。LLVM 22 的 i386/m68k 目标对象与实际 bridge 链接，各在两种控制下通过完整 corpus，检查每个向量 lane；这不是声称运行了这些目标的完整 LLVM SDK。
- 主仓库实际 shared SIMD evaluator 在 i386/m68k 各通过 333,240 组算术、sqrt、demote，以及独立 i32 signed/unsigned 转换边界。没有把 SIMD 不存在的 i64 转换计入 SIMD 覆盖率。
- 新的 strict_float 单测在前述 17 个配置运行。PPC64LE 使用下述隔离修正的模拟器，其他配置使用原始 QEMU；Apple AArch64 原生通用 CI 套件通过。
- 定义固定环境契约宏后，i386 原生和 m68k QEMU 在合规默认环境下各通过完整 454,005 组。此模式不测试故意破坏契约的控制字作为受支持输入。

QEMU 10.2.1 的 PPC64LE `xscvuxdsp` 将 u64 先转换到 f64 再 round-to-f32；输入 `2^63 + 2^39 + 1` 因此错误得到 `5f000000`。其 [VSX_CVT_INT_TO_FP 实现](https://github.com/qemu/qemu/blob/v10.2.1/target/ppc/fpu_helper.c) 可见这两个步骤。仅在任务的 QEMU 源码副本中把 r2sp 分支改为整数直接转 f32，再精确扩展用于寄存器表示，同一个未修改的 PPC64LE 测试二进制通过；没有修改系统 QEMU，也没有给真实 PPC 路径增加软件规避。

另外，QEMU x87 的 PC=24 对 f64 load 提前舍入，同一二进制在真实 x86 的 32 位执行模式下通过全部 454,005 组，不能以该模拟器失败替代硬件结果。m68k 的 FPCR=40 则确实会影响 FMOVE 精度：若宿主在进入运行时之前已经通过 C++ FP 参数传递丢失位，运行时无法恢复原始值。该不合规宿主模式的直接 C++ helper 测试不列为通过；生产入口回归使用整数构造的 byte-ABI 参数，专门检查窄精度宿主入口及控制字恢复。默认 Wasm entry 必须先建立正确精度，不能删除边界或把固定环境契约用于 PC=24 的 f64 执行。

生产测试夹具使用运行时的 Wasm 浮点载体类型，兼容 GCC 中独立的 `_Float32/_Float64`，并用 static_assert 检查 global 确实注册。窄精度测试后显式恢复 m68k 原始 FPCR：QEMU 下 libc 的多控制寄存器恢复可能遗留 PC=24，调试器已确认它会在下一轮调用运行时之前破坏 C++ 参数。这是测试清理问题，不据此给 lazy 热路径增加保护。

本次数值补丁的隔离快照通过主仓库、ROS 的 x86_64 纯解释器/组合构建及 x87-only i386 生产入口回归；主仓库 m68k 的 full/lazy 也通过，包括 PC=24 宿主的 byte-ABI 入口、结果及控制字恢复。m68k 使用 GCC 15，运行时 O3、测试 O2，并链接 libatomic；测试构建使用 `FAST_IO_DISABLE_FLOATING_POINT` 绕开 fast_io 格式化表的既有编译问题，未关闭 Wasm 浮点算术，既有编译警告在测试命令中降为警告。这不等于完成 m68k 完整 CLI 或完整 LLVM SDK 的构建认证。

## 先前边界修复的汇编与性能

对比主仓库前一个修复 `8d398d5eb4d04cd09106e87d3804b5a2464e9e48`，Clang 22 / x86_64 / O3 的默认保护构建中，纯解释器 4,826 个、组合构建 9,653 个 uwvm-int opfunc 的机器码及符号重定位目标全部一致。比较包含常量池内容归一化，不把编译器局部符号编号改变误判为代码改变。
普通算术、local 和分派没有插入 FP 检查，原有尾分派保留；不能把回调边界的开销乘到每个 Wasm 指令上。

契约路径的独立 callback probe 是单条 `jmp *%rdi`；ROS 固定环境组合运行时对象不再引用 fegetenv/fesetenv/fesetround。
ROS 组合版本默认/契约模式的 3,706 个 opfunc 中，只有 64 个 native-global opfunc 改变，其他 3,642 个连同重定位均一致。

固定在 E-core 16、每轮 1,000 万次迭代、预热后各取 14 个样本，正反顺序对照并重复整组。下面为第二组中位数（ms），每次结果位模式均为 `46189a80`：

| 构建/负载 | 前一个主仓库修复 | 当前默认保护 | 契约快路径 |
|---|---:|---:|---:|
| 主仓库纯解释器，算术 | 29.388 | 29.381 | 29.383 |
| 主仓库纯解释器，native global | 93.053 | 93.047 | 63.667 |
| 主仓库组合构建，算术 | 29.383 | 29.385 | — |
| 主仓库组合构建，native global | 95.497 | 95.502 | — |
| ROS 纯解释器，native global | — | 93.048 | 63.665 |
| ROS 组合构建，native global | — | 105.311 | 71.009 |

主仓库默认保护相对前一个修复无可见回退；契约模式在这些 native-global 密集负载中节约约 31–33% 时间。
这不是通用加速保证：ROS 算术测试中，默认/契约模式约 31–35 ms，组合版本契约模式前两组分别慢约 7.5% 和 6.3%，第三组反而快约 2.2%，纯解释器方向也有变化。虽普通 opfunc 未变，但尚未隔离布局/地址等影响，不能把差异简单归因于噪声或声称所有负载更快。因此快路径不默认开启，部署前必须测真实工作负载。

另从 ROS 原始 `d5f0cc3e` 构建相同基线（只加入基准输入，不加入修复），第三组得到：

| ROS 负载（ms） | 原版未完整保护 | 当前默认保护 | 契约快路径 |
|---|---:|---:|---:|
| 纯解释器，算术 | 35.507 | 34.297 | 34.285 |
| 组合构建，算术 | 34.289 | 34.358 | 33.596 |
| 纯解释器，native global | 63.697 | 93.073 | 63.680 |
| 组合构建，native global | 71.045 | 105.329 | 71.032 |

ROS 原版与修复版的两种构建均只有 64 个 native-global opfunc 改变，另 3,642 个完全一致。相对于原版缺失保护的路径，空回调压力测试新增约 2.94/3.43 ns 每次回调；这项必要成本不能被“无热路径回退”掩盖。满足契约时可消除该成本。两个仓库的测试构建使用不同 combine profile，不能横向比较绝对耗时推断 ROS 本身更慢。

## 本次正确舍入修复的汇编与性能

基线为主仓库 `7d09288f`、ROS `0f7f5205`，只应用本次浮点补丁的独立快照用于构建，未混入工作区并行开发的 SIMD/memory/validator 改动。主仓库 x86_64 纯解释器 4,826 个、组合版本 9,653 个 opfunc，ROS 两种构建各 3,706 个 opfunc，与各自基线的机器码和重定位全部一致。原有尾分派保持不变。

E-core 16，1,000 万次迭代，各 14 个样本，中位数 ms；结果位模式均为 `46189a80`：

| 构建/负载 | 本次修复前 | 本次修复后 |
|---|---:|---:|
| 主仓库纯解释器，算术 | 29.402 | 29.399 |
| 主仓库纯解释器，native global | 93.123 | 94.232 |
| 主仓库组合构建，算术 | 29.400 | 29.398 |
| 主仓库组合构建，native global | 95.568 | 96.689 |
| ROS 纯解释器，算术 | 33.925 | 34.477 |
| ROS 纯解释器，native global | 93.115 | 93.119 |
| ROS 组合构建，算术 | 34.310 | 34.011 |
| ROS 组合构建，native global | 105.379 | 105.368 |

这些负载未出现大幅回退，但不是所有负载零开销的保证；表中仍报告最高约 1.6% 的实测差异，不因 opfunc 相同就删除布局/系统负载影响。

旧 x87 不能照搬现代目标的“指令完全不变”结论。真实 32 位执行模式、GCC 15/O3、E-core 16、100 万次依赖加法、7 个样本的中位数如下。未修复列通过 volatile 存储每步结果，仅作性能参照，并不具备正确性保证：

| 微基准（ms） | 未修复原生运算 | 每次走正确性回退 | 当前快路径 |
|---|---:|---:|---:|
| f32 add | 7.349 | 39.335 | 4.409 |
| f64 add | 7.347 | 65.618 | 8.686 |

当前 f64 微基准仍比有 bug 的原生版本慢约 18.2%（每次约 1.34 ns），但比无条件正确性回退减少约 86.8% 时间。罕见 midpoint/subnormal 密集负载及旧架构的 JIT helper 调用仍需付出正确舍入成本，不能宣传“完全没有性能损耗”。f32 的数字也不能外推成所有操作加速。实际 Clang/O3 i386 f64.add opfunc 为 208 字节，保留末尾的间接 jmp 分派；热分支没有 FLDCW 或额外 FP helper 调用，PIC 自身的 GOT 建立不计为数值回退。精确回退集中在少数共享函数，防止解释器代码体积膨胀。QEMU 的耗时不用于硬件性能结论。

满足固定环境契约后，同一微基准的另一组 7 样本中位数为 f32 4.408 ms、f64 7.347 ms；该组未修复 f64 参照为 7.346 ms。汇编确认循环去掉了 FNSTCW，仍保留指数/midpoint 判断和慢分支。这只表明该合规环境下的常见加法可以消除本组可见差距，不保证特殊值或其他负载零成本。

m68k 生产对象的普通 f64.add 及 delay-local 合并 opfunc 同样检查到显式 FPCR 读取、正常结果快分支和共享正确性回退，末尾仍为 `jmp %a0@`；未为了通过 QEMU 测试在热分支插入 FPCR 写入。

## WAVM 对照

实际测试的是本机 WAVM checkout `6f871e61c6e8fc54fa5317b5d61d128d681846f3` 的既有 `build-clang` 二进制，不是声称最新上游版本已同样测试。
该 checkout 中未找到对应 fenv/FPCR/MXCSR 执行边界，浮点 emitter 使用普通原生浮点 IR；可参看上游 [EmitNumeric.cpp](https://github.com/WAVM/WAVM/blob/master/Lib/LLVMJIT/EmitNumeric.cpp)。

对子进程用独立预加载构造器设置宿主环境，WAT 从参数接收位模式以避免常量折叠。默认环境三个用例通过；改为 FE_UPWARD 后 `1 + 2^-24` 的 f32 位模式不再等于 `1`；设 MXCSR FTZ/DAZ 后，`min_normal * 0.5` 和 `min_subnormal * 2` 均不再得到规定结果。
这证明在测试配置下缺少边界保护会产生数值错误，但不是沙箱逃逸的证明。若宿主严格满足上述固定环境契约，省去边界保存是合理优化；否则不能因另一引擎省略保护就认为它天然安全。

## 可重复执行

```
python3 tools/ci/check_wasm_fp_environment.py --cxx clang++
python3 tools/ci/check_wasm_fp_environment.py \
  --cxx /path/to/powerpc64-linux-gnu-g++-15 \
  --cxxflag=--sysroot=/path/to/sysroot \
  --runner 'qemu-ppc64 -L /path/to/sysroot/usr/powerpc64-linux-gnu'
```

脚本保存命令、编译日志和 JSON 结果。宿主/目标库搜索路径需由调用者配置。
完整 runtime 回归另使用 `uwvm_int_fp_environment`：主仓库覆盖 full/lazy/tiered，ROS 只覆盖其存在的 full 路径；均包含 hostile native function/global get/global set 与宿主入口环境。

独立数值 oracle 与 LLVM 降低测试可以这样重现（MPFR/GMP、LLVM 和目标 sysroot 需先配置好；生成端不能启用 fast-math）：

```sh
c++ -std=c++20 -O3 -Isrc -DUWVM_FP_ORACLE_GENERATE \
  tools/ci/probes/strict_float_oracle.cpp -lmpfr -lgmp -o fp-generate
./fp-generate 30000 > corpus.hex
# 用目标交叉编译器构建 consumer，再用对应 loader 或 QEMU 运行。
c++ -std=c++20 -O3 -Isrc tools/ci/probes/strict_float_oracle.cpp -o fp-consume
./fp-consume < corpus.hex
clang++ $(llvm-config --cxxflags) -std=c++20 -O3 -Isrc \
  test/0014.llvm_jit/strict_float_lowering.cc $(llvm-config --ldflags --libs --system-libs) -o fp-lowering
./fp-lowering
./fp-lowering --emit > strict.ll
llc -O3 -mtriple=i386-linux-gnu -mcpu=i386 -filetype=obj strict.ll -o strict.o
# 交叉编译 consumer 时增加 -DUWVM_FP_TEST_GENERATED 并链接 strict.o，
# 会通过真实目标机器码检查标量及全部向量 lane。
```

主仓库的 `tools/ci/probes/strict_float_simd_oracle.cpp` 另以 C++26、src 及三个 third-parties include 路径构建，读取相同 corpus。ROS 未恢复这个 SIMD evaluator，因此不复制该 SIMD 专属探针。
