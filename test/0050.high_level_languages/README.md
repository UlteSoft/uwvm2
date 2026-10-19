# 高级语言到 Wasm：适配与实测

The [F#/VB/Rust/Go producer continuation](results/native-budget/language-four-producers-20261019/README.md)
adds real Debug/Release native/WASI constructions and exact native/Wasmtime
checks. Its public build/run tutorials retain compiler boundaries and distinguish
producer correctness from current UWVM DBG, ROS and native/VTune qualification.

The latest [C/C++ continuation](results/native-budget/language-c-family-20261019/README.md)
passes eight actual native/WASI compilations, sixteen ordinary and sixteen
Wasm DBG workflows, and eight source scalar updates across the three LLVM
products and the ROS no-adapter product. Four explicit-serial methodology
regressions also pass. The [large-module checkpoint](results/native-budget/language-debug-followup-20261019/README.md)
retains every ended success/failure and native/Wasmtime gate preflight.
The corrected large DBG matrix and subsequent native/VTune cohort have separate
active paths in CONTINUATION.json. Older named-cohort pending/hand-off notices
below retain their historical scope.

The [2026-10-19 interpreter continuation](results/native-budget/language-runtime-control-edges-20261019/README.md) records a control-end merge fix in all three repositories. Its frozen products pass 93 language expectations (54 normal semantics, 30 invalid-input rejections and nine known producer/Host boundaries), plus seven regression modules per repository. The linked record tracks selected full CLI builds separately from managed debugging, native ROS and performance qualification.


本套件覆盖 Java、C#、Python、AssemblyScript、真正的 JavaScript，以及 MoonBit 的线性内存和 WasmGC 两种后端。结果按具体编译器、产物和运行模式记录；相同语言不同实现之间的耗时不能用于评价语言快慢。

构建、guest 执行、调试和性能采集均在 `ssh linux` 的原有容器 `uwvm-debug-core3-v48-20261011` 中完成。其内存限制为 64 GiB、swap 为零、cpuset 为 0–31。普通性能执行固定 E-core CPU21；硬件采集的 CPU9 仅用于混合 PMU 校验。原始命令、源内容身份、可执行文件和 Wasm SHA、实际 cgroup、退出状态与输出均随报告保存。

## 适配内容

1. 修正主模块同时具有 start initializer 和 `_start` command 时的执行顺序。初始化先执行，随后执行不同的 command；相同入口执行一次。预加载模块、显式入口 ABI 和初始化期间的 `proc_exit` 有独立回归。
2. 修正 WASI Preview 1 的 `args_get` / `environ_get`，包括本项目的 wasm64 版本：只写 `sizes_get` 报告数量的指针。每个字符串仍写 NUL。原先额外的表尾写入会破坏 TeaVM 紧邻分配的数组头，导致其 GC sweep 无法前进。
3. LLVM 并行对象生成改用 lazy bitcode，只展开本分区函数体。保留 object-local personality，完成元数据 materialization，再验证和生成对象。大型 Mono 调试模块的整份 IR 不再在每个 worker 中展开。
4. 默认 session:debug 按最大函数与总优化 IR 检查本机生成预算：任一函数超过 100000 条，或分区/模块合计超过 1000000 条时，并行分区和串行 Rct0 路径均使用 None/FastISel。SSA、调试 safepoint 和两秒暂停/单步期限不变。显式 full/general 策略与普通执行的资格条件保持不变；缓存记录串行/并行修订号及阈值。
5. 使用隔离的 TeaVM producer 补丁修复 DWARF 行地址、end sequence、name 排序、局部变量索引和直接值表达式。原始产物与各补丁阶段分别保存；元数据补丁的 noncustom sections 必须逐字节一致。
6. 修正 `fd_readdir` 及 wasm64 版本在剩余空间不足一个目录项头时的分页：写入真实目录项的截断前缀并填满缓冲区，避免 wasi-libc 将短页误判为 EOF、漏掉 CPython 的 `encodings` 目录。旧行为反例、新行为和越界拒绝共 30 条 Linux 命令均有独立记录，见 [目录分页验证](wasi-readdir/RESULTS.md)。

### NULL 与 Java 参数

不需要 Java 风格的 args/envs 开关。WASI 传递精确数量的指针；C/C++ guest libc 自行分配额外的零槽，满足 `argv[argc] == NULL` 和环境表末尾 NULL。兼容性回归直接检查 guest 的 `main`、`environ`、`getenv`、空值和 UTF-8。

TeaVM 的 argv provider 按精确数量创建 Java 数组并跳过程序名。其原始 `System.getenv` 是 stub；可选 producer 补丁 `--wasi-environ` 使用标准 `environ_sizes_get/get`，按首个 `=` 分割 UTF-8，并提供只读 Map。开关属于隔离的构建工具，VM 无需新增 Java ABI 模式。

依据：[C++ main 约定](https://eel.is/c++draft/basic.start.main)、[wasi-libc 的 argv 分配](https://github.com/WebAssembly/wasi-libc/blob/main/libc-bottom-half/sources/__main_void.c)、[环境表初始化](https://github.com/WebAssembly/wasi-libc/blob/main/libc-bottom-half/sources/__wasilibc_initialize_environ.c)。

## 路线与调试边界

| 路线 | ABI / 实现 | 本套件检查的调试能力 |
| --- | --- | --- |
| Java / TeaVM 0.13.1 | WASI Preview 1，TeaVM 线性堆与 GC | Wasm 调试；修补 producer 后的 Java 源码断点、步进、函数名、真实 int locals / print 和值更新 |
| C# / .NET SDK 8.0.408，Mono WASI 8.0.15 | 托管 IL 在 Wasm 内的 Mono 中执行 | Wasm 调试；保留 Mono native DWARF 与 portable PDB。C# 托管源码/变量需要额外的 managed debugger bridge |
| Python / CPython 3.14.8，WASI SDK 24 | CPython 解释器与冻结 Python bytecode 静态链接进 WASI core Wasm；另有标准 CLI + 显式标准库目录路线 | 保留 CPython C DWARF；Wasm 调试资格按独立 O2/Oz 产物分别记录。Python 源码/词法变量需要额外的 debugger bridge |
| AssemblyScript 0.28.20 | 静态编译为 WASI core Wasm | Wasm 调试、producer source map 源码断点与步进；没有 DWARF 词法变量 |
| JavaScript / Javy 9.1.0 | QuickJS 与 JS bytecode 静态链接进 WASI core Wasm | Wasm 指令、函数索引、栈帧与内存；没有 JavaScript 源码映射和词法作用域 |
| MoonBit 0.10.15+943464bfa | `wasm` / `wasm-gc`，小型 WASI ASCII 输出适配 | 两后端 Wasm 调试；GC 后端 source map 断点与步进；没有词法变量 DWARF |

AssemblyScript 的语法接近 TypeScript，使用静态类型编译为 Wasm；它不是完整 JavaScript 的编译器。本套件的 JavaScript 路线确实使用动态 JS 和 QuickJS。`asm.js` 则是另一种较早的 JavaScript 子集约定。

保留的边界测试包括 TeaVM 原始隐式整数除零捕获缺陷、需要浏览器/JS host imports 的 TeaVM WasmGC 产物，以及 MoonBit 原始 `spectest.print_char` 输出。预期拒绝这些依赖的测试通过，只说明诊断符合预期，不能计作相应语言路线已经可用。Java 对象/数组的完整托管布局也未由 int 值测试认证。

Python 的 producer、native/Wasmtime 46 项参考与原产品三仓 33 项普通检查已有实际记录，见 [Python 资格说明](python/qualification.md) 和 [普通运行摘要](results/native-budget/python-ordinary-summary.json)。目录分页修复后的独立标准 CLI 已在 native、Wasmtime 和三仓新产品上各通过 16 项，共 **80/80**，包括标准库导入、argv/env、异常、循环 GC 和 Python 自身的 trace/locals，见 [新版标准库结果](python/stock-stdlib-r2-results.md)。[初轮失败与诊断](python/stock-stdlib-results.md) 单独保留。完整 Oz 模块在三仓的 WasmDBG 已实际通过并完成独立核验；新产品性能仍在执行资格流程。Python 自身的 trace 测试不等于 UWVM 的托管源码调试桥，标准库结果也不涵盖 native threads、完整 asyncio 或 rclpy。

ROS SDK 与平台适配由另一个任务同步开发；实际 CPython 扩展桥在 Source64 双 ROS 运行时上的默认拒绝验证见 [Python ROS 证据](python/ros-peer-evidence-source64-r2/README.md)。真实 TeaVM Java 模块在加入六个 WASI 修复及两阶段入口修复后的独立基线上，也已验证正常运行和默认拒绝，见 [Java ROS 证据](java/evidence/2026-10-18/ROS-SDK-DEFAULT-DENY.md)。同一七文件修复基线上的 [C# Mono 产物与两项默认拒绝](csharp/ros-peer-evidence/seven-file-baseline-r3/README.md)、[MoonBit 两后端 24 项实际运行](moonbit/ros-peer-evidence/seven-file-baseline-r3/README.md) 另有原始记录。[MoonBit 132 项 Node 参考模型结果](moonbit/ros-peer-evidence/model-reference-r1/README.md) 单独记录，不计入 UWVM 或 ROS 投递测试。这些独立运行时的身份与本套件通用产品不同，均不代表 native ROS 消息投递通过。

## 运行与证据

各语言目录包含固定版本、下载校验、构建命令、源码和独立参考结果。`engines/recover_languages.py` 与 `csharp/recover.sh` 用于恢复工具及产物。TeaVM 的隔离补丁构建见 `java/build_dwarf_fixed.py` 与 `java/build_perf_timed.py`。

```sh
ssh linux
docker exec -it uwvm-debug-core3-v48-20261011 chroot /host /bin/bash
TASK_ARENA=/tmp/high-level-20261017
TASK_SUITE="$TASK_ARENA/uwvm2-ros/test/0050.high_level_languages"
python3 "$TASK_SUITE/prepare_cases.py" --arena "$TASK_ARENA" --out "$TASK_ARENA/manifest.json"
python3 "$TASK_SUITE/run_matrix.py" --arena "$TASK_ARENA" --manifest "$TASK_ARENA/manifest.json" --out "$TASK_ARENA/new-matrix" --samples 1
```

功能矩阵实际验证精确语义输出、错误输入/退出码，以及真实 PTY 中的 entry、stop ID、bt、Wasm locals、单步、断点、restart、退出和终端恢复。源码与变量验证另有严格 runner，不能用 Wasm 调试成功代替托管语言调试成功。

前一批 native-budget 三仓产品均已通过每仓 20 项普通运行预期结果与 9 项 WasmDBG 工作流，共 60 与 27 项。普通结果包括每仓 12 项正常语义、5 项 C# 无效输入拒绝、3 项已知 producer/host 边界诊断。主仓两个大型 DBG 结果复用已完成的隔离资格，逐项核验相同 binary/source/Wasm/helper SHA 与完整 argv，不能复用旧产品结果。Java args/env 另有 18 项三仓检查，包含 3 项 Unicode WasmDBG。完整计数与身份见 [功能摘要](results/native-budget/functional-summary.json)；并发功能测试的耗时不进入性能表。

`fd_readdir` 修复后的新产品已另行完成三仓各 31 项普通预期结果，共 **93 项**，独立完整字节核验见 [新版普通矩阵证据](results/fd-readdir-r4/README.md)。其中 54 项正常语义、15 项 C# 无效输入、15 项 Python 无效输入和 9 项已知边界分开计数；旧批次结果不代替新二进制的资格。新产品另已通过 [18 项 Java args/env 与 12 项源码工作流](csharp/fd-readdir-r4/SOURCE.md)，其中三项 Unicode DBG 额外核对完整 continue 输出；源码工作流的完整预期行按会话检查，Java 变量范围为 int。新产品的 27 项常规 WasmDBG 工作流和独立 Python FullOz 的三仓工作流均已实际通过并退出，共 **30 项**；[统一独立资格报告](results/fd-readdir-r4/debug-evidence-r2/final/readdir-debug-evidence-qualification-r2.json) 核验完整原始输出、命令、身份与进程退休，135 个输入前后 SHA 不变、errors 为空。[增量证据说明](results/fd-readdir-r4/debug-evidence-r2/README.md) 与[前组原始证据](results/fd-readdir-r4/debug-evidence-r1/README.md) 保留来源。进程并发期间的耗时不用于性能比值。

正式性能首批 r2 在预热阶段被外层进程 RSS 观察守卫停止，尚无 measured sample。原始 FAIL、原始输出及进程退休核对保留于 [失败批次证据](engines/r5-failed-bench-r2-evidence-r1/README.md)，不进入性能比值；后续批次使用新的目录与冻结身份。ROS 任务另外增加的两处精确 client namespace 判断已做[独立静态核对](csharp/final-production-peer-integration-audit-r3/README.md)，保留该改动，且不将旧产物资格移用于尚未构建的 Source65。 后续 RMW namespace 与 loader 集成另已做[当前源码窄范围核对](csharp/final-production-peer-integration-audit-r4/README.md)，自有实现块保持不变；该静态结果不授予新集成源码运行资格。

C3 对无 VmRSS 的退出窗口使用单独的、有固定一秒期限的观察器：只接纳已持有 pidfd、身份与 cgroup 一致且符合单线程退出条件的进程，未知 RSS 记录为 null，仍保留资源上限和最终退休要求。[首次 native 生命周期回归](engines/r5-exit-window-evidence-r3-r1/README.md) 实际通过 8 项，捕到真实 PF_EXITING 窗口，并检查超限、错误退出、超时和未知状态拒绝；这不证明前一批失败的具体内核原因，也不属于 guest 性能结果。

C3 普通性能批次的 49 个请求已结束：41 个组合各执行五个测量样本，共 205 个；三仓 UWVM 与 Wasmtime 的 28 个必需组合均无执行失败，另 8 个可选组合的执行失败按原样保留。但最终环境核验发现 `perf_event_max_sample_rate` 从 50000 降为 40000，kernel journal 记录了内核自动 lowering；这些记录不能归因到具体 consumer，也不能量化对样本的影响。外层最终收据 `887a04a1…` 为 **FAIL**，没有运行 offline summary，也没有正式 derived 或性能比值。原始样本与失败收据见 [r3 失败取证](engines/r5-bench-r3-failure-forensics-r1/README.md)；[工具源码只读审查](java/perf-sysctl-source-review-r1/facts.json) 仅说明已审 bench 路径未发现显式 sysctl 写入或采样启动，不能证明整个窗口无其他活动。

Linux 现已交还 ROS 任务，等待 RMW endpoint、metadata 和 SDK 实现闭合及联合排程。后续仍需完成干净新 cohort 的普通比较与正式 42 项硬件采集；不会自动重跑或修改 sysctl。已完成的 93 项功能、30 项 WasmDBG 和 80 项标准库资格各自独立保留；既有 N3 资格不代替新增 ROS 实现或 Source65 的验收。

下一批的[环境监测与准备工具源码](engines/rate-drift-cohort-assembly-r1/README.md)另行保存，尚未执行或取得新资格。候选 launcher 在预检、每 0.5 秒观察和最终清退时核对同一份新环境基线；漂移或读取失败均停止并保存失败。环境工具只记录实际值，需检查真实收据后才能选定基线；[私有文件与子进程回归源码](engines/rate-drift-guard-native-r1/README.md)不会修改系统设置，也不代替完整采集验证。

普通性能比较使用同一份计时 Wasm、禁用引擎磁盘缓存、一个预热和随机交错的独立进程样本。guest 内计时和进程 wall/user/sys 分开记录。WasmEdge AOT 的离线编译、原 Wasm SHA 和 native derivative SHA 单独保存。失败或零分辨率计时不产生比值；操作系统页缓存没有被清空。

VTune 的普通执行采集使用独立的 stdin gated Wasm：初始化和预热后输出 READY，收到 `G` 执行计时内核，输出 DONE 后等待 `D`。collector 在 READY 后启动，在 DONE 后停止并完成数据整理，再允许语义输出和退出。测量窗口包括内核计时调用与 DONE 标记 I/O，排除冷 JIT、语言初始化及退出。目标使用 `-Rct 0`，全部 TID 固定 CPU21，五类 capabilities 为零、no-new-privs 为一；采集串行、无栈、有 30 秒上限与 kernel journal 监视。实际内核须在 1–20 秒，clock/retired Hotspots 每种事件至少十个目标 PMU 样本。

Microarchitecture Exploration 的 summary 是聚合 counting，仅用于流程预检；最终进程归因使用 detailed sampling 并核验实际报告。可选 `capture_jit_symbols.py` 在 READY 后只读注册 ELF，供同一 PID 的真实采样 IP 匹配 Wasm 函数范围；它不提供 C#/JS 托管方法或源码行归因。降权目标通过任务自有的公开 hardlink 路径读取相同二进制、Wasm 和 SDK 依赖副本，原 SDK 与 home 权限不变。

`bench_engines.py`、`run_vtune_ordinary.py`、`run_vtune_matrix.py` 保存所有成功和失败样本；旧 `run_vtune.py` 的 DBG 采样范围单独标记。`record_environment.py` 记录实际动态依赖 SHA、boot 与 PMU 配置。源码读取参考的本机 Wasmtime HEAD 与被测 release 二进制版本不同，性能归因须以实际测量为准。

本轮中途 SSH Linux 重启清空了 tmpfs。之后恢复了同一容器，使用 `/home/macromodel/Documents/uwvm-hll-20261017` 持久目录和 `/tmp/high-level-20261017` 别名。重启前历史报告与恢复后的新产品资格分别保留，产物是否复现均检查 SHA；中断实验不会改写为成功。

## Default-on language adapters and ROS interpreter sizes

All fifteen independent xmake language switches now default to enabled. Exact
language lists and none still select minimal builds. The [new Linux matrix](results/native-budget/language-defaults-on-size-20261019/README.md)
records selected Debug/Release checks and original/stripped ROS interpreter
ELF sizes with precise backend and source boundaries.


## Native ROS2 replay on the default-on products

The [new native replay](results/native-budget/language-defaults-native-20261019/README.md)
passes sixteen MoonBit deliveries on both frozen default-on Release/Debug ROS
interpreter products. Four retained producer profiles run twice per Runtime;
all48 actors exit naturally. Native writer/GID/full-value checks, permission
denial and checked retirement are verified separately from performance and
full SDK compatibility.


## MoonBit complete records and JointState

The [new selected Linux matrix](results/native-budget/language-moonbit-records-jointstate-20261019/README.md)
records nine source-policy cases, fourteen real Wasm codec test executions,
sixteen JointState builds/layouts and sixteen default-disabled Runtime outcomes.
The first numeric-receiver compilation failure is retained. Positive native
JointState delivery and broader SDK/debugging/performance qualification remain
separate work.


## Complete MoonBit codecs inside all three UWVM products

The [new standalone codec matrix](results/native-budget/language-moonbit-codec-uwvm-20261019/README.md)
executes the same complete fixture and ROS1/ROS2 JointState codec methods in
all six frozen Release/Debug interpreter products. It records 72 positive
outcomes,168 completed methods and24 expected SIGILL unreachable failures
from deliberately corrupt golden bytes. All16 applications are built for
wasm/wasm-gc and Debug/Release. This is actual UWVM computation; native
JointState middleware delivery remains a separate requirement.


## Complete Java records and JointState in UWVM

The [new Java linear WASI matrix](results/native-budget/language-java-records-jointstate-20261019/README.md)
fixes the SDK float32 writer's unavailable class-library call and compiles
twelve complete-record/JointState applications with SIMPLE/FULL optimization.
The six frozen three-repository Runtime products complete 168 codec methods
and 564 checks in twelve runs; twelve corrupt-golden controls fail as expected.
The ROS products additionally pass eight default-disabled ROS1/ROS2 JointState
outcomes. Positive subscription builds have no native delivery credit.


## Complete C# records and JointState in UWVM

The [actual C#/Mono WASI matrix](results/native-budget/language-csharp-records-jointstate-20261019/README.md)
adds twelve complete-record/JointState builds and 24 final Wasms with Debug/Release
producer configurations. Twenty-four codec executions across the six frozen
Runtime products complete360 methods and 1368 checks; 24 corrupt-golden controls
fail at the actual managed assertion. The ROS products pass16 default-disabled
ROS1/ROS2 JointState outcomes. Actual P/Invoke prototypes, original-object links,
final binary imports and identical noncustom sections across baseline/retained-
DWARF pairs are verified. Positive native JointState delivery remains separate.


## Language debugger switches preserve Wasm execution

All fifteen adapters default to enabled; keeping them enabled is recommended.
The [language debugger option guide](../../docs/debugger/language-options.md)
explains individual switches/macros, explicit-list precedence and rebuilds.
Disabling an adapter only trims language-specific debugger support: legitimate
Wasm from that language still executes with its normal feature/import/resource
requirements. Host adapter switches do not generate Guest debug metadata or
replace a missing managed debugger bridge.


## Complete CPython records and JointState in UWVM

The [actual Python/WASI matrix](results/native-budget/language-python-records-jointstate-20261019/README.md)
adds twelve builds and 24 Wasm variants. Twenty-four positive executions in
the six frozen Runtime products complete 408 methods and 1944 checks, including
full ROS1/ROS2 JointState golden values. Twenty-four corrupt-golden controls
reach the actual Python assertion; sixteen default-disabled JointState outcomes
pass in the ROS products. Frozen source/bytecode, real static bridge links,
complete binary imports and identical noncustom sections are verified. The
record preserves the original scratch accounting limitation and failed cohorts.
Positive native JointState delivery, Python snapshot DBG and performance parity
remain separate qualification.

## 2026-10-19 LLVM debugger and native checkpoint

The [new verified data](results/native-budget/language-debug-native-vtune-20261019/README.md)
records fifteen same-source Java timed measurements, ten actual hardware
Hotspots/detailed-microarchitecture collections, four actual stripped sizes and
sixteen effective source-position/value workflows. It preserves four corrected
legacy source-map path rejections separately. The larger forty-case Wasm debug
matrix remains in progress; fifteen configuration switches do not establish
fifteen complete managed-language adapters or native performance parity.

The [Zig/TinyGo/Kotlin producer matrix](results/native-budget/language-zig-tinygo-kotlin-20261019/README.md)
retains seventy exact native/Wasmtime program outcomes, original compiler and
runtime failures, bounded stdlib preparation controls and the narrow compiler
WASI exit adapter. See the [Zig](zig/README.md), [Go/TinyGo](go/README.md) and
[Kotlin](kotlin/README.md) tutorials for the tested basic build/run routes. The
separately queued UWVM and DBG consumers do not yet add result credit.


The [Go authority and TinyGo/Kotlin startup record](results/native-budget/language-go-authority-startup-20261019/README.md)
adds actual ROS-all LLVM startup checks. TinyGo and Kotlin complete under default
authority. Go completes after explicit monotonic-clock and realtime-clock grants;
traces and individual omission controls retain the default denial and show the
required resources. Full consumer DBG and native/VTune results remain separate.

## Completed C#/Python native and Wasmtime measurements

The [verified C#/Python cohort](results/native-budget/language-csharp-python-native-20261019/README.md)
contains thirty formal measurements and twenty actual hardware collections.
For these fixed kernels, the three UWVM LLVM products execute faster than the
corresponding Wasmtime 42.0.1 cases, while native C# remains about 10.7 times
faster and native Python about 1.8 times faster than UWVM. Native CoreCLR and
Mono WASI use different execution strategies. Fresh-process initialization is
reported separately and remains substantially slower in UWVM.
All ten detailed microarchitecture runs carry VTune short-execution warnings:
actual sample identity is verified, but their percentages are not accepted as
bottleneck evidence. The raw warnings and all failed Python DBG retries remain.

The prepared [C/Go/TinyGo/Rust/Zig gated fixtures](gated-kernel/README.md)
and [longer C# fixture](csharp-long-kernel/README.md) provide source, independent
integer reference implementations, strict argument handling and build/run
instructions. Their queued Linux build/semantic/authority and native/VTune
matrices have no qualification credit until actual completion. All fifteen
debugger options remain enabled by default; keeping all enabled is recommended.
Disabling language-specific debugger support does not block valid Wasm execution.

## Independently verified closed Zig/F#/Rust consumers

The [closed consumer packet](results/native-budget/language-zig-fsharp-rust-closed-20261019/README.md)
records 65 naturally ended actors: 64 passed and one original Rust source
breakpoint failure is retained. It includes all four frozen Zig products
(Debug/Release ordinary output and Wasm DBG, plus Debug C-ABI scalar source
values 40 to 42), and the initial ROS/all Debug F# and Rust consumers. An
independent archive-only verifier rechecks all 217 roles, full XZ integrity,
complete output and process retirement. Generic Wasm DBG does not establish
managed source/object debugging. No performance or native ROS delivery result
is added by this packet.

## Linked Rust/std debug metadata limits

The [Rust linked-DWARF component matrix](results/native-budget/language-rust-linked-dwarf-budget-20261019/README.md)
passes twelve current-header Debug/Release × all/none combinations and three
frozen-source budget-only controls. All six acceptance/refusal/reparse checks
pass in each combination. The common DIE limit is now 131,072; the current
8 MiB copied-string budget is preserved. An independent archive verifier
checks 155 roles and full XZ integrity. These parser-only results give no
actual Runtime source-value or performance credit; complete CLI rebuilds and
Rust source requalification retain separate exact products.
