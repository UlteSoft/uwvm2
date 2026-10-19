# UWVM -Rdbg 与 Wasmtime 调试性能定位（v97，2026-10-13）

当前差距来自普通执行生成代码、逐调用帧维护，以及调试状态同步三部分。新的 typed-reference gate 复用只减少少量工作，没有达到 Wasmtime 性能，也没有在调用密集测试中证明稳定加速。

仅 SSH Linux 64 GiB、无 swap cgroup 内构建、运行和采样；本机没有编译或运行。固定 Raptor Lake E-core CPU21，同一 Wasm、同一重复次数和 checksum，关闭磁盘 JIT cache。240 次新无采样测试；计时覆盖 kernel，排除编译和启动。主仓库前后各10次，stable/ROS各5次；普通模式、Wasmtime原生GDB、真实LLDB Wasm插件各5次。保留全部样本和离群值，共享主机没有宣称独占。

| 输入 | UWVM普通 ms | UWVM -Rdbg ms | Wasmtime普通 ms | Wasmtime原生GDB ms | Wasmtime Wasm插件 ms | 两种Wasm调试时间比 |
|---|---:|---:|---:|---:|---:|---:|
| memory64 | 58.997 | 1897.268 | 200.512 | 203.939 | 938.206 | 2.02× |
| typed_call_ref | 1008.420 | 3680.163 | 61.625 | 64.222 | 271.043 | 13.58× |
| gc_struct | 965.796 | 3975.067 | 40.828 | 40.918 | 260.086 | 15.28× |
| call_ref_fanout_256 | 368.809 | 1427.797 | 39.193 | 37.420 | 92.560 | 15.43× |

## 原因与证据

1. **普通执行已经存在巨大差距。** typed_call_ref 普通模式约16.36倍，GC约23.65倍。typed中UWVM开调试相对自身约3.65倍，Wasmtime Wasm调试相对自身约4.40倍。这组数据不能支持“UWVM调试器额外倍率一定高于Wasmtime”的说法；应同时优化普通调用/GC生成代码，不能把整个运行时差距都归给调试器。
2. **调用帧进入、退出是明确热点。** 加倍到10000次kernel重复的typed硬件采样中，small_enter桥约54B sample-weighted retired events；shadow/workspace/activation/debug-activation四类leave合计22.5B。入口实际执行activation admission、父帧校验和工作区绑定。稳定深度已复用分配容量，不能解释成每调用malloc。采样粒度1.5B events/sample，这些不是精确指令次数或耗时百分比。
3. **JIT生成代码和状态维护过重。** 实际MCJIT ELF的typed静态指令9474→9344；Wasmtime guest ELF为1312。C++桥在JIT对象之外，静态大小不能当成动态次数。typed硬件summary retired约64.343B→64.192B，仅减少0.234%；CPI约0.229、E-core retiring88.9%、ICache0.0，证据指向执行了大量额外工作，不能声称主要是缓存miss。
4. **GC不仅是调试开销。** 新采样命名热点包括struct_get32桥18B、struct_set6B、debug_host_scope析构7.5B、wrapper4.5B、suspend_host4.5B retired events；unknown JIT 69B保留未归因，不跨进程拼接JIT地址。普通GC性能也有明显差距。

Wasmtime只读源码commit `b4b23fe583173131a047141947a785b4c1c646ae` 的 `func_environ.rs:1329` 增量同步dirty operand槽，`:1426`只写被赋值local，`:2296`的call_ref直接读取typed function reference入口/vmctx。当前UWVM同模块typed快速路径已绕过通用C++ resolver，不能继续说“每个动态call_ref都进resolver”；外部/imported/lazy等仍保留回退。Wasmtime源码`:1342-1353`明确说debug instrumentation副本**没有单独GC rooting**，依靠当前非移动GC和主程序值的root，不能套用“debug槽本身是GC roots”的旧结论。

补充代码生成策略：实际日志显示UWVM调试为 `session:debug / debug-local-ssa / less`，普通为 `aot:max / passbuilder-tuned / aggressive`。`uwvm_runtime.default.cpp:12735`、`:13326`确认调试IR只进行选定numeric local promotion和vector legalization，然后返回，不走完整普通PassBuilder流水线；Wasmtime guest运行仍设置 `opt-level=2`。因此debug/ordinary比值混合优化策略变化和状态维护，尚无受控消融实验给出各自精确耗时。当前UWVM同模块typed shortcut仅用于Observe/debug，普通动态call_ref仍有resolver回退；不能把普通16.36倍差距全部归因于任何单一项。安全扩展调试IR优化也是后续重点。

## 此次已验证的有限优化

编译器内部witness只复用同一SSA的不可变carrier/type/ABI/table身份检查，要求真实CFG支配关系；任一证明不符回退原路径。观察后仍执行fresh Acquire+Volatile入口读取，绝不缓存当前代码地址。native MCJIT测试验证观察回调中0→new、new→new、new→0发布变化，以及diamond/bypass/SSA/ABI/plan/table/type错误回退。Observe ABI62→63，resumable17、packet schema6、native ABI2和disk cache5保持原协议。

fanout主仓库中位时间减少1.235%，stable0.486%，ROS0.261%；typed主仓库增加1.046%，stable近乎不变，ROS增加0.285%。保留这些结果，不宣称稳定typed提速。三个Linux仓库构建、36项组件检查和57组正式PTY回归通过；每仓库22个GC trap、17个subtype案例及66真实深栈帧检查通过。原有Core3 deterministic_profile限制仍存在，不能宣称全部Core3通过。主仓库复用本轮完全相同source hash的R2构建和R1组件结果，stable/ROS新构建；240个性能样本均新测。

## 方法边界与后续方向

完成10个UWVM硬件窗口（8个微架构summary、2个driverless热点），采样不混入无采样吞吐计时。只解释实际E-core指标；未执行P-core的派生100%bad speculation行被排除。实际MCJIT原始ELF和Wasmtime ELF/CLIF均保留；无命中原生GDB通常接近普通执行，但提供的状态层级与Wasm插件不同。

额外Wasmtime硬件采集尝试没有建立有效的start/end窗口：先遇到fixture metadata/name不同及异步collector/LLDB响应，最后目标退出而未按预期停点。失败原件全部保存并排除，**没有声称得到新的Wasmtime硬件计数或跨运行时CPI比较**。240次真实Wasm插件吞吐比较和生成汇编捕获有效。

下一步优先优化真实activation进入/退出和重复不可变plan join，扩展dirty locals/operand增量存储及冷读者解码，并检查普通调用和GC生成代码。保留运行时publication、epoch、生命周期、parent状态、tail/EH、GC roots和完整准确停点；直接删除这些检查不是有效优化。

RAM中的新原始数据、三个native二进制、93个合格source哈希及输入已逐成员SHA验证并持久化到Linux `perf-20261010/debug-call-ref-witness-v97-r5-raw-evidence.tar.gz`。原磁盘25/18GiB完整流程guard未降低；采用独立RAM产物流程保留25GiB RAM文件系统余量，cgroup61GiB主动停线，主盘4GiB余量、128MiB受限源码/clean rebuild写入；持久化是单独320MiB有界导出。旧数据未清理，OOM计数0。本机只应用局部source overlay并保留独立实验，完整本机实验组合未编译；macOS/Windows/QEMU未在此轮复测。

原始全样本、命令、hash、资格和失败记录见 `debug_call_ref_witness_v97_performance_report.json`。官方方法参考：[Wasmtime guest debugging](https://docs.wasmtime.dev/examples-debugging-guest.html)、[native debugging](https://docs.wasmtime.dev/examples-debugging.html)、[VTune CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-0/command-line-interface.html)。
