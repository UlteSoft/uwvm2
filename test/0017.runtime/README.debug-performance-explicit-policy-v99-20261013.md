# -Rdbg 显式 LLVM 优化参数实测（v99，2026-10-13）

现有参数已经支持覆盖调试默认值，无需新建src/wasm、src/asm和三层会话模式。优先级为显式full policy、显式通用LLVM policy、调试会话默认值、普通默认值。裸`-Rdbg`在本次Linuxx86_64上使用numeric-localSSA、EarlyCSE、InstCombine和常量死路径清理，加nativeCodeGenLess，并非严格O0；显式`debug`才是None/noIRpipeline。

```sh
uwvm -Rdbg -Rllvm-full-policy debug app.wasm # O0
uwvm -Rdbg -Rllvm-full-policy pb-o1 app.wasm
uwvm -Rdbg -Rllvm-full-policy pb-o2 app.wasm
uwvm -Rdbg -Rllvm-full-policy pb-o3 app.wasm
```

这些级别都继续启用调试器；现有src/Wasm/ASM布局和操作保留。优化参数在进程启动、JIT编译之前选择。本轮没有增加运行中的优化切换命令，没有修改源码、默认值或旧报告，也没有加入已撤回的三种调试层预设。

仅SSH Linux64GiB/no-swap cgroup测试，目标固定CPU21 E-core，capabilities全部0；本机没有编译或运行。三个当前v97二进制不变，仅改变显式启动参数。45组短资格检查覆盖5个级别×3种输入×3仓库，实际日志标签验证优先级；pb-o2额外57组PTY回归覆盖完整SIMD/GC/locals/operands/内存修改、源码/ASM/寄存器显示、取消、快速断点、符号和重启。4个新VTune微架构summary窗口与180个新无采样吞吐样本分别运行，无命中断点的kernel计时排除启动和编译。主仓库每级别每输入5次；stable/ROS每输入默认和O2各5次。共享主机，保留全部样本和离群值。

| 输入 | 默认 ms | O0 ms | O1 ms | O2 ms | O3 ms |
|---|---:|---:|---:|---:|---:|
| memory64 | 1899.689 | 3638.357 | 2274.618 | 2271.776 | 2270.954 |
| typed_call_ref | 3608.950 | 4449.931 | 3745.992 | 3671.651 | 3679.183 |
| gc_struct | 3879.515 | 5590.622 | 4231.967 | 3743.263 | 3735.065 |
| call_ref_fanout_256 | 1427.101 | 1856.423 | 1536.357 | 1535.557 | 1535.765 |

**选择：保留当前默认策略。** O2在memory64慢19.587%、typed_call_ref慢1.737%、fanout慢7.600%，GC快3.512%；stable/ROS方向一致，GC改善4.219%/4.533%。O3在GC略好，但其他负载仍退化；57组扩展回归只验证O2，不能说O3已做同等完整回归。GC密集负载可以显式试用pb-o2，不把全局默认盲目改成O2/O3。

硬件计数支持memory64的回退：退休指令38.445B→46.251B、周期7.763B→9.287B，CPI0.201935→0.200805，实际E-core retiring约99%。主要是动态指令数增加，不能未经汇编/消融验证归因于某一个LLVM pass或spill。GC指令55.052B→58.651B，周期15.609B→15.316B、CPI0.283532→0.261140；更多指令但平均周期降低，获得小幅收益。一对硬件窗口不能证明具体微架构因果。未执行P-core的派生指标排除；没有新的Wasmtime硬件/吞吐比较。本轮结果说明单纯改优化级别不能消除之前的调用帧和观察/GC桥开销。

当前native_provenance仍是syntheticWasm-offset行表和numeric-register信息，不是完整C/C++/Rust nativeDWARF；源码变量依赖原始WasmDWARF与保留状态。不能因为只显示src/ASM就删除Wasm状态。LLVM允许优化与调试信息共存，但这不等于所有源码变量或每个跨层状态都自动可恢复。

更早的v98通用SimplifyCFG试验在组件的结构/EH约束处失败，未进入production build，源文件和build/config全部精确回退；保留失败native字节和诊断记录。它不证明显式O2生产调试损坏，本轮真实O2回归已通过。首次资格脚本在进程退出后取affinity导致收据错误，修正后重新完成45组；复制测试/采样脚本的初次相对import路径错误也保存，原仓库测试不变。

所有新RAM原始数据逐成员SHA验证后保存在Linux `perf-20261010/debug-policy-v99-raw-evidence.tar.gz`。保留25GiB RAM文件系统余量、61GiB主动停线、主盘4GiB余量和有界输出；原磁盘完整流程guard未降低，旧测试数据未清理，OOM计数0。93份Linux源码与三个active binary仍与v97逐字节一致。macOS/Windows/QEMU本轮未复测。

完整命令、全部样本、日志和收据见`debug_policy_v99_performance_report.json`。参考：[LLVM调试信息](https://llvm.org/docs/SourceLevelDebugging.html)、[Wasmtime guest debugging](https://docs.wasmtime.dev/examples-debugging-guest.html)、[VTune CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-0/command-line-interface.html)。
