# Native debug code generation: v72 diagnostic (2026-10-12)

本轮生产修改仍为已在两产品通过完整资格的 v70；此处是一个已撤回的主产品隔离实验。保留当前 debug-local-ssa IR、全部快照及校验，只把 Linux x86_64 的原生 CodeGenOptLevel 从 Less 提高到 Default，并把实验对象策略设为44。没有改变默认代码、命令或减少跟踪。

仅 SSH Linux 原64 GiB/swap0 cgroup xmake -j32，构建 95.225s；主产品6次真实 state/native/trace资格，18个未采样交错计时，以及2次专门的逐指令/trace检查通过。本机只整理源码/hash/报告，没有编译或运行测试。实验未进行 VTune/Advisor 采集，硬件结论仍引用 v70 的12个有效 VTune窗口和2个Advisor Survey。

| 相同 Wasm，各3次 | v70 Less ms | 实验 Default ms | 中位数下降 | min–max 重叠 |
| --- | ---: | ---: | ---: | --- |
| memory64 | 2106.411402 | 2058.196664 | 2.289% | True |
| typed_call_ref | 7010.713052 | 6988.264738 | 0.320% | True |
| call_ref_fanout_256 | 2249.358695 | 2220.404864 | 1.287% | False |

共享主机、核心非独占；原始全部18个样本、范围和启动时间保留，中位数不构成显著性或等价性检验。typed_call_ref仅约0.32%的中位数变化且范围重叠，不足以采用默认策略；memory64/fanout的三组小幅差异也没有完整生产资格。

专门小输入的四次 Wasm位置转换，实际 ni次数在两个版本均为36、27、31、39。其余每条opcode（包括没有独立机器指令的constant、nop和end）的trace保持完整。这个小输入不能代表所有生产JIT函数。

实验结束后恢复两处源码以及21个原有源码／build／.xmake文件的bytes和mtime，并核验所有156个已交付范围文件及主产品built/frozen v70 hash完全匹配。原始候选、备份和日志保留Linux，无新增perf/lockup告警、OOM或cgroup max事件；没有声称历史freeze原因已查明。ROS的v70保持原有资格，未把主产品诊断视为ROS或全平台新资格。

v70加本诊断合计1442个不同的未采样计时样本；另有功能资格观测，不计入吞吐样本。主矩阵依旧19/20个Core3特性／变体／profile通过，deterministic profile仍未解决；仍未达到Wasmtime性能。

下一步主要问题是完整逐指令状态维护、native frame/workspace的进入与退出成本；优化级别调整没有消除这些工作。Wasmtime的增量槽是GC roots，任何借鉴必须同步真实GC、native寄存器修改、restart和函数替换后的状态。UWVM ordinary代码生成/GC与Wasmtime也有差异，不能把所有跨引擎差距算作调试开销。

[完整诊断数据](debug_native_selection_v72_diagnostic_report.json)；[已采纳v70及硬件证据](README.debug-performance-core3-wasmtime-20261011.md)。
