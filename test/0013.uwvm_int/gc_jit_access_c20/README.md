# GC JIT 数组访问优化与原生验证（C20，2026-10-12）

本目录保存同步到 `uwvm2`、`uwvm2-stable`、`uwvm2-ros` 的 C18–C20 增量优化测试和证据。默认配置不变；选择 `gc-jit-access-profile=typed-array-load` 启用实验配置。三个仓库的最终合并源码已分别在 SSH Linux 的同一个 64 GiB cgroup 内完成 LLVM 原生构建和 323 项检查，共 969 项通过。

**尚未达到 Wasmtime 的整体 GC 性能。** 独立 C20 性能构建继承了此前的其他实验选项；这里同步的四宏配置单独通过了正确性验证，不能将该性能构建的数字直接当作三个最终仓库构建的成绩。

## 修改与边界

- `array.len` 使用标量状态/结果 ABI。成功创建数组后，编译器记录不可变的逻辑长度，并维护引用局部变量的长度与有效位；未知引用、自尾调用重新开始等情况失效。保存的是长度，不是对象地址。
- i8、i16、i32、f32 数组读取使用认证后的单次原生借用。精确本地类型省去重复布局查询；JIT 生成边界检查、地址计算、相应宽度加载和扩展/位转换。foreign owner、subtype、null、越界仍保留认证和错误处理。
- 可变数组仍执行 acquire 读取锁和 release 解锁。借用区间内没有调用、trap 或 GC 安全点；原生对象指针不能跨安全点或回调缓存。
- 新的类型化读取仅在已经验证的 x86_64 Linux Clang 原生 ABI 上启用。其他架构、i64/f64/v128/引用元素读取及调试、checkpoint、lazy/tiered/pending 等上下文保留适用的旧路径。没有完成其他架构或操作系统的性能验证。

配置项会为所有翻译单元一致添加以下四个宏，避免对象布局与生成代码的 ABI 不一致：

```text
UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS=1
UWVM_EXPERIMENTAL_SCALAR_ARRAY_LENGTH=1
UWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES=1
UWVM_EXPERIMENTAL_TYPED_ARRAY_LOAD=1
```

在现有 Linux x86_64 LLVM 构建配置中选择 `--gc-jit-access-profile=typed-array-load`。Xmake 验证使用三个仓库中新增的原样配置块，在原生最小项目中核验选中配置的四个宏和默认配置的零个新增宏；没有重新执行完整仓库的 Xmake 依赖配置流程。最终三个 ELF 则均实际编译了完整的三个翻译单元，并开启 LLVM IR verifier。

## 性能结果与解释

下表来自独立 C20 性能 ELF；数字是耗时比例，越小越好。132 个内部 POSIX 计时单元采用四轮交错顺序，每单元五个样本；JIT、首次调用和两批预热不计入，VM 调用切换仍计入。

| 用例 | C20 / C19 | C20 / Wasmtime DRC |
|---|---:|---:|
| f32 重复初始化与读取 | 0.943 | 2.766 |
| GC 函数引用数组 | 1.009 | 1.601 |
| 标量对象 DAG | 1.003 | 1.971 |
| v128 阶段压力 | 0.997 | 0.611 |
| 引用阶段压力 | 1.020 | 0.124 |
| 混合对象数与字节压力 | 1.003 | 0.957 |
| exceptions / exnref | 0.998 | 1.335 |
| function references | 1.006 | 2.268 |
| reference types | 1.005 | 0.809 |
| tail call | 1.000 | 0.820 |
| 只读数组长度循环 | 0.957 | 0.038 |

数组长度循环被优化器消除，不能将最后一行解释成回收器速度。Wasmtime DRC 不回收循环垃圾；分配、访问、存活根扫描、释放、峰值空间和暂停分布需要分别比较，不能仅用一个访问循环宣称 GC 算法全面胜出。

固定硬件计数补测采用目标 TID、P-core 2、user mode 的 instructions/cycles/ref-cycles 分组、未缩放计数，要求运行比例至少 99.99%。在 f32 综合用例中，C20 的指令数减少 **25.9%**，周期数仅减少 **0.6%**，与计时减少 5.7% 一起说明指令削减尚未消除主要延迟。相对 Wasmtime，C20 仍为约 1.58 倍指令和 2.96 倍周期。

新增八个 `read-lock/` 用例覆盖 i8/i16/i32/f32 的不可变与可变数组：每对只改变声明的可变性，一个 1024 元素数组做 131072 次带校验和的索引读取，循环内没有 Wasm 调用、写入或 GC 安全点。72 个交错计时单元中，不可变读取相对 C19 减少约 **27.7%–37.6%** 耗时。C20 的可变 f32 耗时为不可变的 **2.55 倍**，Wasmtime 为 **1.02 倍**；实际汇编和 PMU 均指向仍存在的逐次认证与读取锁，详见 [ASSEMBLY.md](ASSEMBLY.md)。

该计时批次曾出现可变 i16 增加 2.45% 耗时；新增三轮、18 个固定计数单元没有复现：相对 C19 指令减少 **32.0%**、周期减少 **12.0%**。引用阶段用例的约 2% 计时增加也未在固定指令/周期中同幅复现。共享 CPU package 活动和频率仍影响周期与计时；这些结果不是所有负载永不回退的证明。

## 正确性和采样覆盖

- 独立 C20 构建：1279 个语义检查单元通过，包括重复引擎检查、预期非法 Wasm 和预期 trap；并非 1279 个不同模块。覆盖数值数组宽度/符号扩展/NaN 位、构造、别名写入、copy/fill/data、subtype/foreign owner、null/越界、分配压力与活跃根、函数引用、异常和独立特性模块。
- 六次调试器重启验证查看、修改、错误写入拒绝、目标替换、旧句柄拒绝等回退路径；不代表已经完成完整 GC checkpoint 或修改后强制回收的测试。
- 最终三个合并构建：每个 279 项主矩阵、16 项 foreign provider/consumer、20 项独立特性模块、八项新增读取校验，共 323 项。保留三个仓库各自的 CLI：主仓库 `-Rcc jit -Rcm full`，stable/ros 的完整 LLVM 后端为 `-Raot -Rllvm-full-policy pb-o3`。
- **98 份**合格硬件采样：90 份主要矩阵，加八份可变性对照，均完成原始 PERF 记录核验和独立 sample-total 交叉检查，目标 user ROI 的采样全部来自 `cpu_core`，没有目标 `cpu_atom`/软件计数器采样和 PERF 丢失。另有 **54 个**固定计数单元。
- 20 个独立特性模块中，18 个具有运行时硬件采样，custom annotation / extended const 为一致性检查。atomics 用例不是并行 GC 测试；这也不是 Wasm 3.0 所有操作码与组合的完整验证。

内存限制只有一个 `memory.max=68719476736`、`memory.swap.max=0` 的 cgroup；所有原生构建、计时、正确性执行、VTune 和 LLVM-MCA 均在该组内完成，没有按 CPU 空闲率等待。内存/磁盘阈值、PMU 归属、丢样和采样密度仍须检查。内核将采样上限从 40000 降到 32000 时，受影响采样被拒绝并保留；后续只调整本任务采集参数，没有修改系统采样上限。

## 证据与复现位置

- [总报告](evidence/C20-NATIVE-REPORT.json)：源代码、ELF、Wasm、采样、计时、三仓库构建和局限的哈希绑定。
- [主硬件矩阵](evidence/hardware-summary-c20.json)、[原始 PMU 归属](evidence/raw-PMU-domain-verification-c20.json)、[新增对照采样](evidence/read-lock-hardware-summary-c20.json)。
- [主固定计数](evidence/C20-FIXED-COUNTER-REPORT.json)、[i16 固定计数](evidence/READ-LOCK-I16-FIXED-COUNTER-REPORT-C20.json)、[交错计时](evidence/alternating-long-clock-c20.json)。
- [三仓库主矩阵](evidence/repo-sync-c20-v4-all-three-qualified.json)、[foreign/特性检查](evidence/repo-sync-c20-v4-foreign-isolated-features-qualification.json)、[新增读取检查](evidence/repo-sync-c20-v4-read-lock-qualification.json)。
- `cases.json`、`read-lock-cases.json` 记录本目录内的相对文件路径和原始哈希；`bundle-index.json` 绑定导出的紧凑证据和实际汇编。完整结果包含原始 Linux 路径以保留来源。

完整源快照、测试 ELF、采样 raw/RX、守卫执行回执和控制脚本保存在 SSH Linux：

```text
/home/macromodel/Documents/uwvm-gc-perf-20261011-r74/recovery/
```

本轮只清理了本任务约 100 MB 编译中间文件，并将已关闭、已验证的约 855 MB VTune 文件迁到持久磁盘，原路径通过链接继续可读；没有回收共享缓存或删除其他任务文件。本目录不复制巨大的 raw 采样、ELF 或完整源码快照。

## 尚待解决

剩余优化必须针对重复对象认证和 mutable 读取锁；删除锁前，需要实现并证明 UWVM 的原生/foreign/debug 并发访问约束，不能直接套用 Wasmtime 的 Store 约束。认证合并或安全点区间内复用需要显式失效与活跃根证明，不能将原生借用随意跨调用缓存。自动异常对象的保留问题、GC 暂停分布、真正并行 GC、更多引擎以及 QEMU/其他系统和架构验证仍未完成。
