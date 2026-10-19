# Debug performance: v70 host admission / packet inlining (2026-10-12)

只读参考本机 Wasmtime 的增量 state slots、GC rooting 和批量 patchable breakpoints，继续缩减 UWVM 每次真实调用的宿主开销。本轮把 cache.find 的同步 key 参数改为 const reference，内联原有 shadow observer_site、完整 packet validation、enter_impl 和 compiler_enter_observer。函数体中的所有 identity、plan/control block、flag、reference/root、parent、continuation、quota 和 failure 条件保持；没有复用旧 payload、减少快照内容或加入无插桩断点后端。observe policy43 拒绝旧 v42，resumable17、ABI2、site8 prefix 和 JIT producer 不变。宏 push/pop 保持头文件独立及原有作用域。

先核验 v69 的 152 个范围文件，冻结 v69 为旧版；两产品分别在 SSH Linux 原 64 GiB / swap=0 cgroup xmake -j32 构建。81 个长输入预筛全部独立 checksum 通过，记录九组交错结果后才进入完整资格。所有编译、程序运行、调试、sanitizer 和 profiling 只在该 cgroup；本机仅源码、hash、AST 和小报告整理。计时目标全部 TID 固定 CPU21，CPU9 只作 hybrid PMU companion。主机／核心非独占，cgroup peak 是共享累计值，不归为本任务峰值。

主矩阵保留 1412 个未采样样本：Core3 418、live-reference/vector 66、calls/callchain 180、长输入 81、短 fanout 250、FFT 39、控制／缓存 378。相同 Wasm hash，新旧交错，逐样本独立 checksum；guest CLOCK_MONOTONIC kernel 排除 JIT／启动／attach／停点，冷准备及 process time 另存。所有原始样本和 min–max 保留；中位数不是显著性、性能等价或普遍无回退证明。

| 长输入，主产品各 9 次 | v69 debug ms | v70 debug ms | 中位数下降 | Wasmtime Wasm/LLDB ms | v70 / Wasmtime |
| --- | ---: | ---: | ---: | ---: | ---: |
| typed_call_ref | 7233.919943 | 6853.738474 | 5.256% | 271.273320 | 25.27 |
| memory64 | 2028.823271 | 2028.795571 | 0.001% | 941.414093 | 2.16 |
| call_ref_fanout_256 | 2350.991287 | 2241.456688 | 4.659% | 89.065514 | 25.17 |

预筛 typed_call_ref 新版有一个约 7.33s 的慢样本，完整保留，新旧 min–max 重叠；256-target 范围不重叠。memory64 持平。因此这是调用路径的局部收益，不能宣称普遍达到 Wasmtime。

| 匹配的 summary 硬件窗口 | v69 Instructions Retired | v70 Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| memory64 | 40,650,555,041 | 40,647,659,231 | 0.007124% |
| typed_call_ref | 119,030,870,351 | 113,425,494,592 | 4.709178% |
| call_ref_fanout_256 | 29,405,006,533 | 28,283,439,853 | 3.814203% |

这些窗口包含 table/local/operand、loop、entry/leave、校验和停点门控，整个退休指令差不能等同于某个 helper 的精确指令成本。VTune 2026.4 共 10 个有效采集：typed call_ref 旧新 2 个 driverless/no-stack 硬件 Hotspots，memory64／typed call_ref／256-target 旧新 6 个 Microarchitecture summary 计数，typed call_ref 旧新 2 个 detailed uarch。detailed 在全部未采样计时后串行采集，是样本加权估计，与 summary 计数分开；不是所有 Core3 特性的硬件采样。实际 event、PID/TID、cap=0、affinity、CPI、MUX、hash、boot 和参数保留。真实 pre-clock 到 profile_end breakpoint 的窗口含可能的 hint-bucket 碰撞，不能称为严格 none 配置。

按 Intel 手册，显式50ms 乘一次基数30M，实际 event period1.5B 逐行核验，最低样本标准不变；不以配置文本代替真实 Events Per Sample。默认10ms 行为保持。吞吐来自未采样 kernel，不使用 profiler elapsed。2 个 Advisor 2026.0 Survey 有效，没有声称完整 JIT loop／Trip Counts／FLOPs；Unknown JIT 不虚构成精确函数或指令归因。[Intel sampling interval](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2024-1/sampling-interval.html)。

另在全部 UWVM 采集和计时退出后，采集相同 Wasm 的真实 Wasmtime Wasm/LLDB pre-clock/profile_end summary 计数2次，共12个有效 VTune 窗口。Wasmtime kernel 较短，使用既有可配置 minimum_kernel_ms=100，仅 summary counting；没有修改 Hotspots 最低样本标准，也未增加 Wasmtime Hotspots/detailed。实际50ms、counter、MUX和caps保存，不从 Unknown 推导函数名。

| 相同 Wasm 的计数窗口 | UWVM v70 retired | Wasmtime Wasm/LLDB retired | UWVM / Wasmtime retired | Wasmtime MUX |
| --- | ---: | ---: | ---: | ---: |
| memory64 | 40,647,659,231 | 18,426,263,985 | 2.206 | 0.997701 |
| typed_call_ref | 113,425,494,592 | 5,264,015,491 | 21.547 | 0.993807 |

这是完整目标窗口而非某条 JIT 指令，包含各引擎不同的 runtime IO／trap／边界工作；不把跨引擎比值全部当成调试器开销或确定性的因果证明。

| typed call_ref detailed，目标 E-core | v69 | v70 |
| --- | ---: | ---: |
| Retiring | 68.4% | 71.5% |
| Front-End Bound | 4.8% | 4.1% |
| Bad Speculation | 0.0% | 0.0% |
| Back-End Bound | 28.6% | 27.9% |
| Store Bound | 5.9% | 6.8% |

这些派生百分比不是某个函数的精确 cycle 占比或独立因果证明。Hotspots 的真实函数行按旧新保存；某函数没有稀疏样本并不代表它没执行。宿主 native bridge/helper 反汇编使用实际冻结二进制，并检查非零指令和无 missing-symbol 警告；这是宿主静态代码证据，不是完整生产 JIT 反汇编，也不等同于动态工作量。

| 短动态目标，各 5 次 | 主产品 v69 → v70 ms | ROS v69 → v70 ms |
| --- | ---: | ---: |
| 1 | 188.093142 → 179.634992 | 181.817597 → 174.190500 |
| 8 | 205.259057 → 194.680585 | 201.574660 → 189.993749 |
| 32 | 214.658934 → 200.141502 | 213.285502 → 199.069850 |
| 64 | 220.596272 → 207.824099 | 216.872593 → 205.074494 |
| 256 | 236.154980 → 222.862911 | 232.283880 → 219.008765 |

| 控制／缓存，none 各 3 次 | 主产品 v69 → v70 ms | ROS v69 → v70 ms | Wasmtime Wasm/LLDB ms | Native GDB / ordinary |
| --- | ---: | ---: | ---: | ---: |
| branches | 4.771450 → 4.786859 | 4.770658 → 4.768064 | 3.225690 | 0.987 |
| dispatch | 4.152369 → 4.145337 | 4.274937 → 4.248544 | 3.020701 | 0.999 |
| scan32k | 20.997112 → 20.980449 | 21.014490 → 21.021623 | 10.319590 | 0.997 |
| scan256k | 21.007904 → 21.011485 | 21.263482 → 21.022595 | 9.924975 | 1.001 |
| scan8m | 21.010944 → 20.998096 | 21.059085 → 21.009348 | 9.991591 | 1.018 |
| chase8m | 30.185398 → 29.729108 | 33.857939 → 35.122227 | 27.650649 | 1.078 |

none／cold／disabled 和 ordinary 分开保留。以下列出全部负向中位数，包含很小且范围重叠的变化；不选择性删除。原生 GDB、Wasmtime DWARF/GDB、Wasmtime Wasm/LLDB 实际分别测量；引擎 ordinary 代码生成／GC 差异不能全部称为 debugger 开销。

- uwvm2 branches none: 慢 0.323%，min–max 重叠=True。
- uwvm2 dispatch disabled: 慢 0.029%，min–max 重叠=True。
- uwvm2 scan32k disabled: 慢 0.032%，min–max 重叠=True。
- uwvm2-ros scan32k none: 慢 0.034%，min–max 重叠=True。
- uwvm2-ros scan32k cold: 慢 0.363%，min–max 重叠=True。
- uwvm2 scan256k none: 慢 0.017%，min–max 重叠=True。
- uwvm2 scan256k disabled: 慢 0.017%，min–max 重叠=True。
- uwvm2-ros scan256k disabled: 慢 0.012%，min–max 重叠=True。
- uwvm2 scan8m cold: 慢 0.050%，min–max 重叠=True。
- uwvm2-ros scan8m disabled: 慢 0.116%，min–max 重叠=True。
- uwvm2 chase8m cold: 慢 0.520%，min–max 重叠=True。
- uwvm2-ros chase8m none: 慢 3.734%，min–max 重叠=True。

| 同一真实 FFT，各 3 次 | 中位数 ms | min–max ms |
| --- | ---: | ---: |
| uwvm2/before/ordinary | 0.073958 | 0.071679–0.074608 |
| uwvm2/before/debug | 0.891432 | 0.891235–0.895799 |
| uwvm2/after/ordinary | 0.073916 | 0.073753–0.074024 |
| uwvm2/after/debug | 0.895681 | 0.895483–0.900559 |
| uwvm2-ros/before/ordinary | 0.074332 | 0.074269–0.074422 |
| uwvm2-ros/before/debug | 0.893800 | 0.890215–0.894794 |
| uwvm2-ros/after/ordinary | 0.074700 | 0.074484–0.074730 |
| uwvm2-ros/after/debug | 0.893060 | 0.891013–0.894267 |
| wasmtime/current/ordinary | 0.058756 | 0.058599–0.059150 |
| wasmtime/current/gdb-o2 | 0.058220 | 0.058108–0.058872 |
| wasmtime/current/guest-lldb | 0.388345 | 0.386595–0.397456 |
| native/current/ordinary | 0.050919 | 0.047704–0.055096 |
| native/current/gdb | 0.049662 | 0.048941–0.054931 |

| FFT 进程到结果，含冷准备 | 中位数秒 |
| --- | ---: |
| uwvm2/before/ordinary | 0.857762 |
| uwvm2/before/debug | 34.475358 |
| uwvm2/after/ordinary | 0.857326 |
| uwvm2/after/debug | 34.519994 |
| uwvm2-ros/before/ordinary | 0.858944 |
| uwvm2-ros/before/debug | 34.532405 |
| uwvm2-ros/after/ordinary | 0.859901 |
| uwvm2-ros/after/debug | 34.488751 |

FFT 为同一 2048 点 forward/inverse，独立 xorshift32 种子及逆变换恢复 oracle，绝对误差 ≤1e-12。短 kernel 与数十秒 cold JIT／PTY 准备分开，本轮没有解决冷编译成本；FFT和小控制的原始范围不能用长调用收益替代。

另外使用同一个冻结 v70 binary 的既有显式 -Rllvm-full-policy pb-o2，做6次真实 Wasm/native/trace资格和12个未采样诊断，合计1424个未采样计时。没有修改运行源码或默认策略。

| 显式策略诊断，主产品各3次 | 当前 debug-local-ssa / less ms | pb-o2 ms | pb-o2 中位数变化 |
| --- | ---: | ---: | ---: |
| memory64 | 2022.643065 | 2307.196212 | +14.068% |
| typed_call_ref | 6800.464616 | 6698.252558 | -1.503% |

pb-o2不运行当前专用的 numeric local SSA admission，并改变整套 IR pipeline；因此不是单独比较 Less 与 Default 的实验。memory64明显回退、typed仅小幅改善，不采用为默认。六次功能资格和三组计时也不能替代完整生产资格。

冷分配单元首次把 site1 错误地写在空存储绑定之前，被原有 site0 校验拒绝；保留原失败输入和结果。修正测试发布顺序后，两产品通过，未放宽运行时条件。

两产品另有2个独立头文件的冷分配失败单元通过：实际让下一次 owned value expansion 的 new 抛 bad_alloc，验证返回空视图、quota failure 保持且无法复活；正常扩展保留完整42数值。宏作用域让独立头文件按真实 compiler exceptions 支持捕获失败，与原生产 TU 行为一致，未改变 runtime 冻结二进制。

两产品共 14 个组件、14 个独立 ASan/UBSan executable（非完整 runtime sanitizer）、38 组既有真实 PTY、dynamic cross-module call_ref 与 copied-reference 各6次 restart、132 个深层 frame 观测、44 个 GC terminal traps、14 个 sampling guard case 通过。覆盖 native SI/NI/register、完整 trace、shutdown、mutable demand、PHI／悬挂 caller、scalar/sNaN/v128、replace/restart、引用根和状态恢复；组件 DATA 不替代实际 capture/continuation 认证。

Core3 20 个独立特性／变体／profile 输入包括 memory64、table64、multi-memory、typed call_ref、direct/indirect/ref tail calls、GC allocation/struct/array/i31/recursive、nondefaultable locals、exception payload/rethrow_ref、extended const、relaxed madd/swizzle、annotations 和 deterministic profile。19 个运行资格通过；deterministic profile 的显式 selector 和 canonical-NaN 正 probe 仍失败，原始错误保留，coverage_complete=false。没有本机 macOS、Windows/QEMU 或 C++ module 构建。

Wasmtime 对照通过实际 LLDB Wasm plugin / qWasmCallStack 验证，区别于原生 DWARF/GDB。只读参考的 instrumentation slots 是 GC roots，dirty stack/local 增量更新；断点在静止的唯一 code memory 中批量 unpublish/patch/publish。[Wasmtime guest 调试](https://docs.wasmtime.dev/examples-debugging-guest.html)，[参考源码](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/cranelift/src/func_environ.rs)。UWVM 仍有每指令状态写入、父 packet、frame/workspace 和 native admission/retirement 成本；下一步需要在真实 GC／native mutation／restart 后仍准确的增量槽和更轻的 native frame 协议。没有实现无插桩断点或 ITT JIT method registration，parity_achieved=false。

Wasmtime补采的首次 preflight 因旧 harness 清单格式断言失败，未启动目标／采集器；原失败 guard/log 保留。r2只适配清单字段与显式50ms，真实停点／qWasmCallStack／checksum／capability／affinity 检查保持，两个计数窗口有效。

本轮 stage_alert_free=true，无新 perf/lockup 告警；历史 v67 IRQ 告警和 kernel 自动下调 sample rate40000 保留，whole_boot_alert_free=false。第一条新告警停止余下采集，不自动重试，没有修改全局 sysctl／driver／watchdog。cgroup max/OOM/OOM-kill 计数为0，不宣称旧 freeze 原因已查明。自有阶段全部退出后核验 built/frozen/reported 二进制 hash、两产品范围源码和报告一致；小证据取回本机，原始 profiler DB 留 Linux。数据：[debug_core3_v70_performance_report.json](debug_core3_v70_performance_report.json)。

---

## v69 历史记录

# Debug performance: v69 warm native entry admission (2026-10-12)

本轮继续只读参考 Wasmtime 的增量 state slots 和批量 patchable breakpoints，把 UWVM 已有入口 plan cache 的命中路径从完整冷查询中分离。真实 TLS activation、incarnation、module、epoch、publication、generation、raw/typed native entry、同一 plan/control block 全部照常比较；命中后仍执行原有 producer、profile、parent packet、continuation、quota、reference/root 检查。未命中时进入原来的完整 canonical admission，没有缓存运行值或引用根。observe policy 42 拒绝旧 v40/v41 对象，resumable17、ABI2、原 JIT entry ABI 和 site8 prefix 保持。两产品分别编译、测量，保留既有产品差别。

v68/v68b 的简单入口融合没有获得收益，已完全撤回。原始 prefix/site-width 编译拒绝、过时 allocation arity 导致的 JIT generation 拒绝、修复后 81 个无收益交错样本，以及仅三个 clock sample 的无效采样全部保留。该无效采样没有新 kernel 告警，不能用于比较。撤回范围 hash 与 v67 基线逐文件核验。

仅 SSH Linux 原 64 GiB / swap=0 cgroup 编译及运行，两产品 xmake -j32 通过。CPU0–31 构建，实际计时和采样目标 TID 固定 CPU21；CPU9 为 hybrid PMU companion。主机／核心非独占，cgroup peak 是共享累计值，不能归为本任务峰值或旧卡死已解决的证据。本机只读写源码、hash、AST 和小报告，没有本机编译或工作负载。

主要及控制矩阵保留 1412 个未采样样本，另有 36 个独立进程 ROS FFT 回退复测，共 1448 个。相同 Wasm hash、独立 checksum、新旧交错；guest CLOCK_MONOTONIC kernel 时间排除 JIT／启动／attach／停点，process/cold 准备另存。所有样本、范围和失败均保留；中位数不代表统计显著性、普遍无回退或性能等价。

| 长输入（主产品，各 9 次） | v67 debug ms | v69 debug ms | 中位数下降 | Wasmtime Wasm/LLDB ms | v69 / Wasmtime |
| --- | ---: | ---: | ---: | ---: | ---: |
| typed_call_ref | 7617.194297 | 7260.981667 | 4.676% | 271.875941 | 26.71 |
| memory64 | 2028.199508 | 2028.845677 | -0.032% | 939.609125 | 2.16 |
| call_ref_fanout_256 | 2411.629876 | 2335.899684 | 3.140% | 89.152954 | 26.20 |

| 匹配的 summary 硬件窗口 | v67 Instructions Retired | v69 Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| memory64 | 40,653,263,638 | 40,650,201,828 | 0.007532% |
| typed_call_ref | 126,134,754,225 | 119,030,163,464 | 5.632540% |
| call_ref_fanout_256 | 30,825,426,471 | 29,404,577,753 | 4.609340% |

长 typed call_ref 和 256-target 的计时及退休指令方向一致。整个窗口的旧新退休指令差各折合约 142 / dynamic call，但窗口还包含 table/local/operand、循环、校验和门控，不能把该数值说成仅 resolver 或某个函数的精确开销。memory64 基本持平。这是局部入口优化，尚未达到 Wasmtime／GDB 的普遍效率。

生产冻结二进制的宿主 native bridge 反汇编确认：v67 入口调用完整 checkpoint_actual_frame，v69 命中路径使用现有 cache.find，失败分支进入 checkpoint_enter_actual_activation_uncached。cache.find 仍是 outlined 调用，完整冷检查保留。入口 bridge 静态指令数 137 → 206，所导出的全部 helper 静态指令数 515 → 715；增大的静态代码与减少的实际热路径工作量分开记录。这里不是 JIT 函数的完整反汇编，也不以静态数量代替动态性能。首次 demangle/filter 导出退出0但缺少所有 symbol、零指令，明确拒绝；修复导出复用相同冻结 binary，未修改代码或重新采样。

VTune 2026.4 共 10 个有效采集：typed call_ref 旧新各一次 driverless/no-stack 硬件 Hotspots，memory64／typed call_ref／256-target 旧新共 6 个 Microarchitecture summary 计数，加上 typed call_ref 旧新 2 个 detailed uarch 采样。detailed 是样本加权估计，与 summary 计数分开；不是所有 Core3 输入的硬件采样。目标由真实 pre-clock stop 到 profile_end breakpoint，含可能的保守 hint-bucket 碰撞，不能当成严格 none 配置。实际 event periods、PID/TID、caps=0、affinity、clock、retired、CPI、MUX、hash 和 boot 保留，Unknown JIT 不虚构为精确指令归因。2 个 Advisor 2026.0 Survey 有效，没有声称完整 JIT loop／Trip Counts／FLOPs。吞吐取未采样 kernel，采样 elapsed 不替代它。

按 Intel 手册，sampling-interval 会乘一次 custom SAV。此次保留基数 30M，显式 50ms 得到实际 1.5B period；逐行核验真实 Events Per Sample，保留最低样本数标准。此前 v68b 的双重乘数产生 7.5B、只有三个 clock sample，已拒绝。默认 10ms 的原有基数／行为不变。[Intel sampling interval 文档](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2024-1/sampling-interval.html)。

本轮没有新的 perf/lockup 告警，stage_alert_free=true；v67 的 IRQ 告警和 kernel 自动下调至 40000 的历史仍保留，whole_boot_alert_free=false。没有重置全局 sysctl、driver 或 watchdog；第一条新告警将停止余下采样，不自动重试。没有声称旧 freeze 原因已查明。

| typed call_ref detailed，目标 E-core | v67 | v69 |
| --- | ---: | ---: |
| Retiring | 65.6% | 66.8% |
| Front-End Bound | 4.6% | 4.2% |
| Bad Speculation | 1.2% | 0.8% |
| Back-End Bound | 28.5% | 28.2% |
| Store Bound | 11.9% | 5.9% |

这些是该配置下 VTune 的采样派生指标，不是某个函数的准确 cycle 比例或单项变更的独立因果证明。实际 Hotspots 行按旧新分开保存；新版仍可见 observer workspace、safepoint、packet validation、activation entry 和 call_ref resolver。缺少某函数的稀疏样本不表示它未执行。

| 短动态目标（各 5 次） | 主产品 v67 → v69 ms | ROS v67 → v69 ms |
| --- | ---: | ---: |
| 1 | 195.319305 → 187.939577 | 190.546194 → 182.084096 |
| 8 | 213.030292 → 204.958417 | 207.961521 → 201.284382 |
| 32 | 221.333155 → 214.133841 | 220.976252 → 212.944669 |
| 64 | 231.809715 → 221.256915 | 225.603432 → 217.574624 |
| 256 | 244.452512 → 236.215229 | 240.543398 → 233.288376 |

| 控制／缓存（none 各 3 次） | 主产品 v67 → v69 ms | ROS v67 → v69 ms | Wasmtime Wasm/LLDB ms | Native GDB / ordinary |
| --- | ---: | ---: | ---: | ---: |
| branches | 4.774839 → 4.773955 | 4.779235 → 4.779737 | 3.209742 | 0.987 |
| dispatch | 4.141136 → 4.151604 | 4.134131 → 4.134190 | 3.045670 | 0.956 |
| scan32k | 21.003968 → 20.999016 | 21.003165 → 21.024212 | 9.913779 | 0.989 |
| scan256k | 20.987015 → 21.000613 | 20.998174 → 21.003145 | 9.903036 | 0.987 |
| scan8m | 21.058038 → 21.181586 | 21.107528 → 21.056780 | 9.963780 | 1.015 |
| chase8m | 29.383916 → 27.625834 | 27.607101 → 27.662810 | 27.335491 | 1.001 |

none／cold／disabled 和普通模式分别保留。控制／缓存大多持平，也有小回退：主产品 scan8m none 中位数慢约 0.587%，ROS dispatch disabled 慢约 0.363%，其三次样本范围没有重叠；不把这些删去。chase8m 主产品 none 中位数下降约 5.98%，但范围重叠，不能归因为此次入口优化。实际原生 GDB、Wasmtime DWARF/GDB 和 Wasm/LLDB 分别测量。引擎普通代码生成／GC 差异不能全部算成 debugger 开销。

| 同一真实 FFT，初始各 3 次 | 中位数 ms | min–max ms |
| --- | ---: | ---: |
| uwvm2/before/ordinary | 0.074216 | 0.074109–0.075943 |
| uwvm2/before/debug | 0.889659 | 0.888523–0.891340 |
| uwvm2/after/ordinary | 0.074340 | 0.074059–0.077790 |
| uwvm2/after/debug | 0.894179 | 0.893838–0.920051 |
| uwvm2-ros/before/ordinary | 0.074583 | 0.074420–0.074770 |
| uwvm2-ros/before/debug | 0.894026 | 0.892855–0.896211 |
| uwvm2-ros/after/ordinary | 0.074481 | 0.074373–0.130051 |
| uwvm2-ros/after/debug | 0.987439 | 0.894164–0.989767 |
| wasmtime/current/ordinary | 0.060111 | 0.059524–0.061045 |
| wasmtime/current/gdb-o2 | 0.059871 | 0.058851–0.068817 |
| wasmtime/current/guest-lldb | 0.391463 | 0.387670–0.395105 |
| native/current/ordinary | 0.049803 | 0.048057–0.051148 |
| native/current/gdb | 0.049055 | 0.048998–0.049799 |

| FFT 进程到结果（含冷准备） | 中位数秒 |
| --- | ---: |
| uwvm2/before/ordinary | 0.860213 |
| uwvm2/before/debug | 35.250435 |
| uwvm2/after/ordinary | 0.867569 |
| uwvm2/after/debug | 35.555919 |
| uwvm2-ros/before/ordinary | 0.878208 |
| uwvm2-ros/before/debug | 36.477243 |
| uwvm2-ros/after/ordinary | 0.881954 |
| uwvm2-ros/after/debug | 36.157172 |

初始 ROS FFT kernel 中位数 0.894026 → 0.987439ms，约慢 10.45%；原始三次完整保留。该 signal 触发独立进程、新旧交错各 9 次复测，同时保留 ordinary 对照、相同 frozen binary/Wasm hash 和独立 inverse oracle，没有再次硬件采样。

| ROS FFT 追加各 9 次 | 中位数 ms | min–max ms |
| --- | ---: | ---: |
| before/ordinary | 0.074605 | 0.074267–0.080734 |
| before/debug | 0.893447 | 0.889926–0.960336 |
| after/ordinary | 0.074731 | 0.074318–0.080738 |
| after/debug | 0.892115 | 0.889032–0.914974 |

追加 debug 中位数下降 0.149%，min–max 重叠=True。追加结果与最初结果分开保存，不能据此证明普遍无回退或确定共享主机／代码布局是唯一原因。FFT 是同一 2048 点 forward/inverse、独立 xorshift32 种子和逆变换恢复 checksum，绝对误差 ≤1e-12。短 kernel 和约 35–36 秒 cold JIT／PTY 准备必须分开；本轮没有解决冷编译成本。

回归包括两产品 14 个组件、14 个独立 ASan/UBSan executable（非完整 runtime sanitizer 构建）、38 组既有真实 PTY、dynamic cross-module call_ref 和 copied-reference 各 6 次 restart、132 个深层 frame 观测、44 个 GC terminal traps、14 个 sampling guard case。覆盖 shutdown、mutable demand、完整 trace、scalar/sNaN/v128、PHI／悬挂 caller、native SI/NI/registers、replace/restart、引用根和状态恢复；组件 DATA 不能替代真实 capture/continuation 认证。

Core3 保留 20 个独立特性／变体／profile 输入：memory64、table64、multi-memory、typed call_ref、direct/indirect/ref tail calls、GC allocation/struct/array/i31/recursive、nondefaultable locals、exception payload/rethrow_ref、extended const、relaxed madd/swizzle、annotations 和 deterministic profile。19 个通过运行资格，deterministic profile 显式 selector 和 canonical-NaN 正 probe 仍失败；coverage_complete=false，不能称为所有特性已经全部通过。没有本机 macOS、Windows/QEMU 或 C++ module 构建。

Wasmtime 的真实 Wasm guest 调试通过 LLDB Wasm plugin / qWasmCallStack 验证，与原生 DWARF/GDB 分开比较。其增量 state slots 由 GC rooting 保持引用准确，patchable breakpoints 在唯一、静止的代码内存中批量 unpublish/patch/publish；UWVM 不能简单复用旧 payload 或跳过实际 native/GC 校验。[Wasmtime guest 调试文档](https://docs.wasmtime.dev/examples-debugging-guest.html)，[只读参考版本的状态槽实现](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/cranelift/src/func_environ.rs)。

下一步架构重点仍是减小 native frame/packet/workspace 的重复处理，并让引用槽在 native mutation、GC、parent continuation 和 restart 后保持同步；目前尚未实现无插桩的软件断点后端，也未实现 Wasmtime 的 ITT JIT method registration。Unknown JIT 归因仍有限。长 call_ref 仍约为 Wasmtime Wasm guest 26 倍，因此 parity_achieved=false，不能宣称已达到目标。

所有自有运行阶段退出后核验 frozen/built/reported binary hashes、两产品范围文件、kernel alerts 和 cgroup max/OOM events。报告打包首次因 root 审计文件权限失败，原失败 guard 和部分 archive 保留；以 root 在同一 cgroup 只读重打包，没有重测或改动已冻结源码。小证据取回本机，大 profiler DB 和原始源包留在 Linux。数据：[debug_core3_v69_performance_report.json](debug_core3_v69_performance_report.json)。

---

## v67 历史记录

# Debug performance: v67 fused x86 demand-byte gate (2026-10-12)

本轮继续只读参考 Wasmtime 的 patchable breakpoint 和增量 state slots。v66 的基址复用没有证实减少退休指令，负面结果保留。v67 改为缩短已有合格非 GC gate：同一个真实原子需求 byte 的 acquire load／icmp／branch，由 LLVM compiler-owned asm-goto 的 `cmpb + jcc` 执行。Linux x86_64 以外保持原路径，GC/extern 和精确 GC access 保留原 LLVM acquire load、无条件完整 packet 与 post-packet callback gate。

先在同一 SSH Linux cgroup 用实际 Clang 23 工具链编译小型诊断：普通 acquire gate 是 `movzbl + test + jcc`，asm-goto 是 `cmpb + jcc`。这是该诊断的 IR／汇编证据，不是生产 JIT 函数完整反汇编或性能证明。生产变化另行真实 JIT 状态测试、相同 kernel 计时和硬件计数。

具体边界：需求地址仍来自 process-lifetime 4096 个真实一字节原子对象，桶和 `byte_address` 的范围／lock-free／sizeof 检查不变。CMP 只读该 byte，无 RMW、无 packed-word 混合别名、无缓存需求值。x86 普通进程内存 byte 读取具有原子性，后续读写不越过该读；side-effecting LLVM `~{memory}` clobber 也阻止编译器跨越内存操作。host 发布仍使用独立的 C++ release stores。[Intel SDM §8.1.1、§8.2](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)，[LLVM 间接约束、memory clobber 和 label constraints](https://llvm.org/docs/LangRef.html#inline-assembler-expressions)。

CallBr 的 default successor 是 resume，唯一 indirect successor 是 capture；`*m` 输入显式 `elementtype(i8)`，`!i` 对应 compiler-owned capture label，clobber memory/flags，nounwind。该私有机器指令没有 guest source/numeric metadata 或 register result，不产生 capture/root/continuation 权限。完整 cold packet、host authentication、真实 stop、native JIT-only 边界、shutdown、trace、step、parent、PHI、引用根和恢复检查保持。observe policy 40 拒绝旧 v39 对象；resumable17、ABI、wire/cache 格式不变。保留两产品既有差别。非 Linux 分支的 unused-parameter 在项目 Wextra/Werror 下通过 `maybe_unused` 解决；全部测量结束后两产品重新 xmake -j32，要求与已经完成全部运行资格的冻结 binary 逐字节一致，不用未测的新二进制替换结果。

仅 SSH Linux 64 GiB / swap=0 cgroup 编译和运行，两产品 xmake -j32 通过；先核验 v66 冻结二进制与 148 个既有范围文件。本机仅源码编辑、hash、AST 和小报告整理。计时实际 target TID 固定 CPU21，硬件 CPU9 是 hybrid PMU companion；主机／核心非独占，共享累计 cgroup peak 不能归为本任务峰值或旧卡死已修复证明。

本轮 1556 个未采样样本，Core3 418、live-reference SIMD 66、calls/callchain 180、长 memory64／typed call_ref／256-target 81、短 fanout 250、真正 FFT 39、控制／缓存 378，以及追查 chase8m 回退的 144 个无采样样本。新旧交错、相同 Wasm hash、逐样本独立 checksum。CLOCK_MONOTONIC guest kernel 时长排除 JIT／启动／attach／停点；process/cold 准备另存。全部 raw samples、min–max 和失败保留，中位数不能代表统计显著性、普遍无回退或性能等价。

| 长输入（主产品，各 9 次） | v66 debug ms | v67 debug ms | 中位数下降 | Wasmtime Wasm/LLDB ms | v67 / Wasmtime |
| --- | ---: | ---: | ---: | ---: | ---: |
| typed_call_ref | 7587.781138 | 7582.930240 | 0.064% | 270.979928 | 27.98 |
| memory64 | 2261.377496 | 2023.617087 | 10.514% | 936.206218 | 2.16 |
| call_ref_fanout_256 | 2411.937141 | 2402.304977 | 0.399% | 91.732038 | 26.19 |

| 同一硬件窗口 | v66 Instructions Retired | v67 Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| memory64 | 46,055,863,891 | 40,652,787,354 | 11.731571% |
| typed_call_ref | 127,186,209,791 | 126,136,266,517 | 0.825517% |
| call_ref_fanout_256 | 31,075,737,810 | 30,825,064,099 | 0.806654% |

VTune 2026.4：6 次 driverless/no-stack 硬件 Hotspots，6 次 Microarchitecture summary 硬件计数，加上 memory64／typed call_ref 旧新各一次、4 次 detailed 尝试中 3 次资格通过；真实 event、clock、retired、CPI、MUX、PID/TID/caps/hash/boot 留存。summary 与 detailed 分开保存；3 次通过的 detailed 有实际非零硬件 event 的 function attribution 报告，Unknown 名称不会被虚构成 JIT 指令。detailed 在全部未采样计时完成后串行采集，不代表每个 Core3 输入都进行了 detailed 采样。v66 的显式 no-stack 工具修复持续生效，旧／新使用相同参数。目标由真实 pre-clock stop 跑到 profile_end breakpoint，含可能的保守 hint-bucket 碰撞，不能当成严格 none 配置。Unknown JIT 样本没有精确指令归因，空闲 P-core 派生百分比不代表目标负载。2 次 Advisor 2026.0 Survey 有效，没有声称完整 JIT loop／Trip Counts／FLOPs。采样 elapsed 不替代无采样吞吐。[Intel 官方 driverless 方法](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html)。

**最后一次 typed call_ref after detailed 被采样安全监控拒绝**：kernel 提示 `perf: interrupt took too long (4938 > 4912), lowering kernel.perf_event_max_sample_rate to 40000`。采集立即停止，没有再次采样，没有手动改变全局 sysctl 或 driver。该尝试的原始日志／receipt 保留，passed=false，不用于比较；typed call_ref 没有完整 detailed 旧新对照。之前 12 个主要 VTune 结果、memory64 的 detailed 旧新、typed call_ref before detailed、2 个 Advisor Survey 和全部未采样计时各自保留真实资格。stage_alert_free=false，不宣称旧 freeze 原因已解决。后续仅报告核验和无采样构建。

| 控制／缓存（none 各 3 次） | 主产品 v66 → v67 ms | ROS v66 → v67 ms | Wasmtime Wasm/LLDB ms | Native GDB / ordinary |
| --- | ---: | ---: | ---: | ---: |
| branches | 5.117691 → 4.780710 | 5.094742 → 4.766210 | 3.212071 | 0.973 |
| dispatch | 4.361320 → 4.177825 | 4.399220 → 4.144230 | 3.070443 | 0.993 |
| scan32k | 24.152437 → 21.006115 | 24.162032 → 21.026407 | 9.926074 | 1.001 |
| scan256k | 24.147157 → 21.056415 | 24.139201 → 21.019573 | 9.899580 | 0.991 |
| scan8m | 24.174017 → 21.317900 | 24.149292 → 21.007998 | 9.898132 | 1.010 |
| chase8m | 36.443707 → 34.922674 | 33.654240 → 33.589682 | 28.673420 | 1.108 |

最初每组 3 次的 chase8m 中，cold 配置主产品中位数变慢 11.00%，ROS 变慢 32.47%；原始样本全部保留。它是本轮待解释的回退信号，不能从平均收益中隐去。因此在全部采集和两份无采样构建退出后，再交错运行各 9 次 ordinary／none／cold／disabled，共 144 个样本，复用同一个 Wasm hash 和独立 checksum，不重启任何硬件 collector。

| chase8m 复测（各 9 次） | v66 ms | v67 ms | 中位数下降 | min–max 重叠 |
| --- | ---: | ---: | ---: | --- |
| uwvm2 / none | 31.664128 | 30.124512 | 4.862% | True |
| uwvm2 / cold | 31.226190 | 31.599193 | -1.195% | True |
| uwvm2 / disabled | 28.969519 | 30.540010 | -5.421% | True |
| uwvm2-ros / none | 29.973999 | 30.380392 | -1.356% | True |
| uwvm2-ros / cold | 33.591997 | 30.576692 | 8.976% | True |
| uwvm2-ros / disabled | 31.486617 | 30.944859 | 1.721% | True |

后续复测和先前数据分开保存。共享 host／E-core cluster、地址与代码布局、缓存状态可能影响该依赖访问负载；当前数据不能锁定某一个原因，也不能证明所有模式没有回退。FFT 的进程到结果时间含 cold JIT／PTY，主产品 v66 34.267s → v67 35.750s，ROS 34.318s → 35.509s；热 kernel 加速不能声称 cold 启动也加速。

VTune 只读 report export 首次因 CSV 原生 CR/LF parser 失败，修复读取方式后复用已有数据、补齐导出，原失败 guard/log 保留。此修复没有重新进行硬件采样。

typed call_ref 的已知 runtime Hotspots 包括 checkpoint_actual_frame、shadow_ledger 的 enter／leave／validate_compiler_packet、observer workspace enter／leave、resolver 和 canonical type projection。前后实际采样行均保存；它们提示下一步应减少反复帧／packet／workspace 处理，而不是继续只缩短 gate。采样加权 event count 不是精确函数 cycle 比例，Unknown JIT 仍保持 Unknown。

控制／缓存包括 data-dependent nested branches、br_table、32 KiB／256 KiB／8 MiB scan、8 MiB dependent chase。none／cold／disabled 和普通模式分别保存，实际 Wasmtime Wasm/LLDB、DWARF/GDB 与原生 GDB分别运行。每次测试先在同一 cgroup 用独立 unsigned32-bit oracle 计算正确 checksum，包括 scan 每次更新真实数组和 chase 的依赖链；所有引擎逐样本核验，源/WAT/Wasm/native hash 固定。

| 同一真实 FFT（各 3 次） | 中位数 ms | min–max ms |
| --- | ---: | ---: |
| uwvm2/before/ordinary | 0.076306 | 0.074187–0.077877 |
| uwvm2/before/debug | 1.015732 | 0.985876–1.037528 |
| uwvm2/after/ordinary | 0.075768 | 0.075684–0.077852 |
| uwvm2/after/debug | 0.893632 | 0.890494–0.936683 |
| uwvm2-ros/before/ordinary | 0.074966 | 0.074528–0.077671 |
| uwvm2-ros/before/debug | 0.984772 | 0.984461–0.988372 |
| uwvm2-ros/after/ordinary | 0.074705 | 0.074674–0.080408 |
| uwvm2-ros/after/debug | 0.892823 | 0.891828–0.902526 |
| wasmtime/current/ordinary | 0.059051 | 0.058886–0.063694 |
| wasmtime/current/gdb-o2 | 0.063066 | 0.059062–0.065355 |
| wasmtime/current/guest-lldb | 0.393476 | 0.386637–0.417595 |
| native/current/ordinary | 0.047961 | 0.047547–0.050282 |
| native/current/gdb | 0.050424 | 0.048910–0.050542 |

| FFT 进程到结果（含冷准备） | 中位数秒 |
| --- | ---: |
| uwvm2/before/ordinary | 0.884337 |
| uwvm2/before/debug | 34.267398 |
| uwvm2/after/ordinary | 0.889590 |
| uwvm2/after/debug | 35.749615 |
| uwvm2-ros/before/ordinary | 0.868861 |
| uwvm2-ros/before/debug | 34.317668 |
| uwvm2-ros/after/ordinary | 0.868773 |
| uwvm2-ros/after/debug | 35.509445 |

FFT 是同一 2048 点 forward/inverse 输入，独立 xorshift32 种子和 inverse 恢复 oracle，绝对误差 ≤1e-12；原生用同源码构建。传统矩阵 float 递推不是 FFT。短 kernel 变化不能精确归因，冷准备成本另列。普通引擎代码生成／GC 差别不能全部归为 debugger 开销。

回归：两产品 14 个组件、14 个独立 ASan/UBSan executable（非完整 runtime sanitizer 构建）、38 组既有真实 PTY，dynamic cross-module call_ref 和 copied-reference 各 6 次重启、132 个深层帧观测、44 个 GC terminal traps、12 个 sampling guard case。含 mutable demand、shutdown force-all、精确 scalar/sNaN/v128、PHI／悬挂 caller、native SI/NI／registers、trace、replace/restart。组件 DATA 不能替代真实 capture/continuation 认证。

Core3 的 20 个独立特性／变体／profile 输入中，19 个运行资格通过；deterministic_profile 的显式 selector 和 canonical-NaN 正 probe 仍未解决，失败保留，coverage_complete=false。本轮没有 macOS、Windows/QEMU 或 C++ module 构建。**尚未达到 Wasmtime/GDB 的普遍执行效率**，parity_achieved=false。Wasmtime 的实际 Wasm guest 模式使用插桩与 GC-rooted state slots，和原生 DWARF/GDB 分开比较；不能通过省略状态／引用根／native认证来制造性能相当。[Wasmtime guest 调试文档](https://docs.wasmtime.dev/examples-debugging-guest.html)。

所有自有阶段退出后审计 kernel alerts、cgroup max/OOM events、自有作业、冻结／构建／报告 binary hashes 与两产品同步范围。小证据和原始 transcript/hash 保存，大 profiler DB 留在 Linux。数据：[debug_core3_v67_performance_report.json](debug_core3_v67_performance_report.json)。

---

## v66 历史记录

# Debug performance: v66 shared private observation base (2026-10-12)

只读参考 Wasmtime 65abe37529a6bca836efcac1dcb920d5c7890580 的逐 opcode 可修改断点和增量状态槽：未命中路径应减少实际机器指令。本轮 Linux x86_64 中，已有合格 observer gates 共用一个私有 function-entry SSA 基址，每条 Wasm 指令仍读取它自己的原子需求 byte，保留 acquire 次序与完整的请求／冷采集路径。没有缓存需求位、当前寄存器、packet、引用或可执行目标。

编译器自己的空 tied identity 阻止 LLVM 把每个 GEP 折回独立的绝对地址；它放在实际函数入口第一个合法插入点，支配后续 Wasm 分支，LLVM tracking handle 跟踪变更。地址来自 process-lifetime 的 4096 个真实一字节原子对象；桶和边界不变。该 identity 没有 source location / numeric metadata，不进入 Wasm operand/local 或 native register projection。未选中 observer、未知引用和其他后端保持原路径；GC/extern 完整 packet 仍在 gate 前无条件更新。断点、trace、step、shutdown、parent/root/native/restore 认证保持。observe policy 39 拒绝旧 v38 对象；resumable policy 17、ABI、缓存格式不变。两产品只应用相同范围补丁，保留原差别。

修复采样工具检查缺口：Microarchitecture 的 summary 和 detailed 请求现在都显式给出 enable-stack-collection=false，测试必须确认 false 选项存在。以前“没有显式 true”不足以证明实际命令关闭栈采集。本轮所有旧／新采集之前完成工具同步，使用同样的配置。

仅在 SSH Linux 64 GiB / swap=0 cgroup 编译／运行。主产品、ROS xmake -j32 通过，先校验冻结 v65 和 142 个既有范围文件；本机只读写源码、hash 和小报告。CPU0–31 构建，CPU21 计时及真实 target TID；CPU9 仅用于 hybrid PMU companion。主机和核心非独占，cgroup peak 是共享累计值，不能归为本任务峰值或旧卡死问题的解决证明。

1412 个未采样样本：Core3 418、live-reference SIMD 66、calls/callchain 180、长 memory64／call_ref／256-target 81、短 fanout 250、真正 FFT 39，加上控制／缓存 378。相同 Wasm hash、独立 checksum、新旧交错；guest CLOCK_MONOTONIC 的 kernel 时长与 startup / process 时长分开保留。min–max、全部样本和失败均保留；中位数不是统计显著性、等价或普遍无回退证明。

| 长输入（主产品，各 9 次） | v65 debug ms | v66 debug ms | 中位数下降 | Wasmtime Wasm/LLDB ms | v66 / Wasmtime |
| --- | ---: | ---: | ---: | ---: | ---: |
| typed_call_ref | 7722.806903 | 7784.429530 | -0.798% | 276.535402 | 28.15 |
| memory64 | 2368.762882 | 2324.964072 | 1.849% | 964.272282 | 2.41 |
| call_ref_fanout_256 | 2456.221305 | 2451.700498 | 0.184% | 90.449775 | 27.11 |

| 匹配硬件窗口 | v65 Instructions Retired | v66 Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| memory64 | 46,056,624,015 | 46,057,542,844 | -0.001995% |
| typed_call_ref | 127,037,763,696 | 127,190,494,633 | -0.120225% |
| call_ref_fanout_256 | 31,025,453,686 | 31,077,203,191 | -0.166797% |

**基址复用没有证实减少退休指令或普遍提速的预期。** 三个硬件窗口的退休指令均略增；memory64 无采样中位数下降 1.849%，typed call_ref 增加 0.798%，256-target 基本持平。控制／缓存和 FFT 大多变化很小；冷准备仍约 33 秒。这里保留该候选的完整资格与负面结果，作为下一轮比较基线，不能据此宣布达到 Wasmtime。

VTune 2026.4 完成 6 次 driverless／无 stack 硬件 Hotspots 和 6 次 Microarchitecture summary 硬件计数，均有真实 event、clock、retired、CPI、MUX、PID/TID/caps/hash/boot。summary 不是完整 detailed uarch sampling。采集从真实 pre-clock stop 到 profile_end 断点，含可能的保守 hint-bucket 碰撞，不能等同严格 none 配置。Unknown JIT 行未归为精确指令 hotspot，空闲 P-core 指标不代表目标负载。2 次 Advisor 2026.0 Survey 有效；没有声称完整 JIT loop、Trip Counts 或 FLOPs。采样时长不替代无采样吞吐。[Intel 官方 driverless 方法](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html)。

| 短动态目标（各 5 次） | 主产品 v65 → v66 ms | ROS v65 → v66 ms |
| --- | ---: | ---: |
| 1 | 195.550338 → 195.645823 | 194.050705 → 193.044641 |
| 8 | 212.155669 → 211.748633 | 211.384105 → 211.708402 |
| 32 | 222.897991 → 220.635285 | 224.319374 → 224.201998 |
| 64 | 230.847138 → 230.066858 | 228.930610 → 230.268157 |
| 256 | 243.932135 → 244.820867 | 245.426346 → 246.776493 |

新增 378 个控制／缓存未采样样本，各模式／版本 3 次：nested branches、br_table、32 KiB／256 KiB／8 MiB scan、8 MiB dependent chase。保留 none／cold／disabled 与普通模式、实际 Wasmtime Wasm/LLDB 及 DWARF/GDB、原生 GDB。测试执行前，在同一 cgroup 以独立 unsigned 32-bit scalar oracle 计算 checksum，包括逐次更新 scan 内容和真实 LCG chase 依赖；所有引擎逐样本验证。源/WAT/Wasm/native hash 固定。新脚本在首次执行前补齐 actual native 标签与 self-hash，没有丢弃测试失败或测量。

| 控制／缓存（none 中位数） | 主产品 v65 → v66 ms | ROS v65 → v66 ms | Wasmtime Wasm/LLDB ms | Native GDB / ordinary |
| --- | ---: | ---: | ---: | ---: |
| branches | 5.081694 → 5.077213 | 5.072838 → 5.077166 | 3.272731 | 0.982 |
| dispatch | 4.376009 → 4.361914 | 4.381886 → 4.377478 | 3.088700 | 0.994 |
| scan32k | 24.130140 → 24.150138 | 24.135245 → 24.129844 | 9.890197 | 0.994 |
| scan256k | 24.127555 → 24.128411 | 24.127814 → 24.129685 | 9.906868 | 0.998 |
| scan8m | 24.162341 → 24.216382 | 24.158690 → 24.122992 | 9.923456 | 0.974 |
| chase8m | 29.917326 → 29.875072 | 30.108646 → 29.908739 | 28.275537 | 1.083 |

| 同一真实 FFT（各 3 次） | 中位数 ms | min–max ms |
| --- | ---: | ---: |
| uwvm2/before/ordinary | 0.074453 | 0.074167–0.078509 |
| uwvm2/before/debug | 0.986006 | 0.984135–0.990396 |
| uwvm2/after/ordinary | 0.074132 | 0.074048–0.074211 |
| uwvm2/after/debug | 0.987391 | 0.985649–0.988232 |
| uwvm2-ros/before/ordinary | 0.074589 | 0.074346–0.075200 |
| uwvm2-ros/before/debug | 0.987500 | 0.984271–0.990948 |
| uwvm2-ros/after/ordinary | 0.074639 | 0.074423–0.074707 |
| uwvm2-ros/after/debug | 0.984847 | 0.982837–1.049117 |
| wasmtime/current/ordinary | 0.059897 | 0.058673–0.060919 |
| wasmtime/current/gdb-o2 | 0.059831 | 0.058629–0.060959 |
| wasmtime/current/guest-lldb | 0.392558 | 0.385800–0.397591 |
| native/current/ordinary | 0.047957 | 0.047619–0.049691 |
| native/current/gdb | 0.049483 | 0.049291–0.049685 |

| FFT 进程到结果（含冷准备，各 3 次） | 中位数秒 |
| --- | ---: |
| uwvm2/before/ordinary | 0.859478 |
| uwvm2/before/debug | 32.723196 |
| uwvm2/after/ordinary | 0.858925 |
| uwvm2/after/debug | 33.239040 |
| uwvm2-ros/before/ordinary | 0.861538 |
| uwvm2-ros/before/debug | 32.886529 |
| uwvm2-ros/after/ordinary | 0.858498 |
| uwvm2-ros/after/debug | 33.263976 |

FFT source/Wasm hash 固定，独立 xorshift32 种子和 inverse 恢复 oracle，绝对误差 ≤1e-12；原生从同源码构建。传统矩阵 float 递推不是 FFT。短 kernel 的小变化不能精确归因；冷编译成本未解决，不能用 kernel 结果掩盖。实际 Wasmtime Wasm/LLDB、Wasmtime DWARF/GDB、原生 GNU GDB 分开比较。普通引擎代码生成和 GC 差别不能全算成 debugger 开销。

回归：两产品共 14 个组件、14 个独立 ASan/UBSan executable（非完整 runtime sanitizer 构建）、38 组既有真实 PTY、dynamic cross-module call_ref 与 copied-reference 各 6 次重启、132 个深层帧观测、44 个 GC terminal traps、12 个 sampling guard case。包含精确 scalar/sNaN/v128、PHI／悬挂 caller、native JIT-only SI/NI／registers、trace、shutdown、replace/restart 的状态检查。组件 DATA 不提供真实 capture/continuation 权限。

Core3 20 个独立特性／变体／profile 输入中，19 个运行资格通过；deterministic_profile 缺少 UWVM 显式 selector 和 canonical-NaN 正 probe，失败保留，coverage_complete=false。本轮未运行 macOS、Windows/QEMU 或 C++ module 构建。**尚未达到 Wasmtime/GDB 的普遍执行效率**，parity_achieved=false。[Wasmtime 实际 Wasm guest 调试方法](https://docs.wasmtime.dev/examples-debugging-guest.html)。

Wasmtime 槽同时是 GC roots，不可直接复用旧引用状态。后续方向包括降低完整状态／native witness 的机器指令成本、批量安全 breakpoint patch/publish、ITT JIT 方法注册和冷编译。没有新增无插桩后端，没有通过省略跟踪、根更新或权限认证制造性能相当的结果。

所有自有阶段结束后审计 kernel alerts、cgroup OOM/max events、自有作业退出、冻结／已构建／报告二进制和两产品同步范围。小报告与原始 transcript/hash 保留，大 profiler 数据留在 Linux。数据：[debug_core3_v66_performance_report.json](debug_core3_v66_performance_report.json)。

---

## v65 历史记录

# Debug performance: v65 copied call_ref resolution (2026-10-12)

只读参考 Wasmtime 65abe37529a6bca836efcac1dcb920d5c7890580：typed `call_ref` 从已经验证的函数引用加载 code/vmctx，不重新锁原 table。UWVM 的桥已经拿到完整的私有 JIT funcref 副本，却在解析时再取一次 table mutex。本轮只去掉这次重复锁；真正 table/global 的读取、修改和 call_indirect 仍保留锁。

真实 generated bridge / LLVM FP scope 检查保持，debug 还要求当前非零 execution depth 和真实 lease 借用。完整引用 kind、canonical function/import membership、type projection、当前 raw/typed acquire entry 读取全部保持。替换仅在 ALL 真实参与者停住且目标无活动帧时提交；解析与读取间不调用 guest/callback、不 park、不编译，原代次代码按生命周期保留。完整 parent packet/reference/flags 校验、GC roots、native/restore 权限不变；没有 JIT IR/ABI/cache-format 改动，observe policy 38、resumable policy 17 保持。源代码同步主产品与 ROS，已有产品差别保留。

新增真实 PTY 测试：先取旧 funcref 到 local、再把 table 改成另一个目标；停住替换旧函数，继续执行时 copied reference 使用当前新代次，而新的 table 引用仍调用另一个目标；动态 typed tail、当前 locals、过期代次拒绝、完整 trace、三次重启都检查。先用冻结 v64 验证测试，再对两产品新版验证。初次 typed elem 写成普通 funcref 被官方 validator 拒绝；第二次 trace count=256 超过 128 上限被 CLI 拒绝。只改测试脚本，两个失败日志及 transcript 原样保留，新旧产品二进制未因修正脚本改变。

全部编译/运行仅在 SSH Linux 64 GiB / swap=0 cgroup；两套 xmake -j32 通过。本机仅处理源码和小报告。构建 CPU0–31，计时/target TID CPU21，hybrid PMU companion CPU9；不声称主机或核心独占。先审计 v64 的 138 个范围文件及冻结二进制。1034 个未采样样本：Core3 418、live-reference SIMD 66、calls/callchain 180、长特性/256-target 81、短 fanout 250、真实 FFT 39。相同 Wasm hash、独立 checksum、旧/新交替；guest CLOCK_MONOTONIC 排除 JIT/startup/attach/停点，raw 全样本/min–max/中位数与全进程时间分开保留。小差异可能来自共享主机和代码布局，中位数不证明显著性、等价或普遍无回退。

| 长输入（主产品，旧/新/实际插件各 9 次） | v64 debug ms | v65 debug ms | 中位数下降 | Wasmtime Wasm/LLDB ms | v65 / Wasmtime |
| --- | ---: | ---: | ---: | ---: | ---: |
| typed_call_ref | 7786.565158 | 7556.105493 | 2.960% | 271.033112 | 27.88 |
| memory64 | 2300.974055 | 2301.266464 | -0.013% | 936.078696 | 2.46 |
| call_ref_fanout_256 | 2492.627341 | 2451.102279 | 1.666% | 90.293102 | 27.15 |

| 匹配硬件窗口 | v64 Instructions Retired | v65 Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| memory64 | 46,056,328,921 | 46,056,051,750 | 0.000602% |
| typed_call_ref | 130,438,618,622 | 127,035,319,872 | 2.609119% |
| call_ref_fanout_256 | 31,706,287,840 | 31,025,661,684 | 2.146660% |

12 次 VTune 2026.4：6 次 driverless/无 stack 硬件 Hotspots，6 次 Microarchitecture summary 硬件计数；summary 不是完整 detailed uarch sampling。真实 event/period、CPI/clock/MUX、E-core 指标、PID/TID/caps/hash/boot 留在 JSON。空闲 P-core 派生比例和 Unknown JIT 样本不当作精确 hotspot 归因，采样耗时不当作吞吐。采集窗口使用 profile_end 断点，含可能的保守 hint 检查，不能直接称为 none 配置。2 次 Advisor 2026.0 Survey 的有效性/native 行保留，未宣称完整 JIT loop、Trip Counts 或 FLOPs。[Intel 官方 driverless 方法](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html)。

| 短动态目标（各模式/版本 5 次） | 主产品 v64 → v65 debug ms | ROS v64 → v65 debug ms |
| --- | ---: | ---: |
| 1 | 202.298558 → 195.262224 | 197.522318 → 194.595854 |
| 8 | 218.474077 → 212.522893 | 215.154583 → 213.157327 |
| 32 | 228.916093 → 225.264968 | 229.919249 → 222.547503 |
| 64 | 233.348067 → 229.890509 | 233.698960 → 229.103490 |
| 256 | 249.869366 → 247.405770 | 250.398902 → 245.820426 |

| 真正 FFT（同一 Wasm/独立逆变换 oracle，各 3 次） | 中位数 ms | min–max ms |
| --- | ---: | ---: |
| uwvm2/before/ordinary | 0.074302 | 0.074159–0.075093 |
| uwvm2/before/debug | 0.986101 | 0.985327–0.990293 |
| uwvm2/after/ordinary | 0.075936 | 0.074059–0.077925 |
| uwvm2/after/debug | 0.985227 | 0.984158–0.985441 |
| uwvm2-ros/before/ordinary | 0.076354 | 0.074427–0.080315 |
| uwvm2-ros/before/debug | 0.989748 | 0.989707–1.040864 |
| uwvm2-ros/after/ordinary | 0.074636 | 0.074340–0.076065 |
| uwvm2-ros/after/debug | 1.010488 | 0.984449–1.010978 |
| wasmtime/current/ordinary | 0.062766 | 0.062744–0.065150 |
| wasmtime/current/gdb-o2 | 0.064484 | 0.061892–0.065284 |
| wasmtime/current/guest-lldb | 0.412818 | 0.406349–0.424378 |
| native/current/ordinary | 0.052977 | 0.051525–0.053191 |
| native/current/gdb | 0.053582 | 0.051878–0.068002 |

| FFT 进程到结果（含冷准备，各 3 次） | 中位数秒 |
| --- | ---: |
| uwvm2/before/ordinary | 0.855132 |
| uwvm2/before/debug | 34.032693 |
| uwvm2/after/ordinary | 0.857239 |
| uwvm2/after/debug | 32.832818 |
| uwvm2-ros/before/ordinary | 0.892494 |
| uwvm2-ros/before/debug | 34.224659 |
| uwvm2-ros/after/ordinary | 0.882339 |
| uwvm2-ros/after/debug | 33.967221 |

FFT 的浮点结果用独立 xorshift32 初始值与逆变换恢复关系验证，绝对误差 ≤1e-12；原生从同 source hash 构建。传统矩阵 float 是递推，不是 FFT。FFT 内核短，微小百分比变化不足以精确归因。冷编译/准备成本尚未解决，不能用 kernel 时间掩盖全进程开销。

本轮回归：10 个组件、10 个 ASan/UBSan executable、完整 38 组既有 PTY、两产品动态 cross-module call_ref（6 次重启）、两产品新增 copied-reference（6 次重启）及旧版测试资格（3 次重启）、132 个深层帧、44 个 GC terminal traps、12 个采样 guard case。失败后停采、并发拒绝、无 kernel 监控时拒绝采集均验证。本轮未测试 macOS、Windows/QEMU 或 C++ module 构建。

Core3 20 个独立特性输入仍有 deterministic_profile 的显式 selector/canonical NaN 正 probe 缺口，19 个运行资格通过；整张矩阵保留失败，coverage_complete=false。**未达到 Wasmtime/GDB 的普遍执行效率**，parity_achieved=false。普通代码生成/GC 的引擎差别也不能全部算成 debugger 开销；实际 Wasmtime Wasm/LLDB、Wasmtime DWARF/GDB、native GDB 分开比较。[Wasmtime 官方 guest 调试方法](https://docs.wasmtime.dev/examples-debugging-guest.html)。

Wasmtime 的增量槽同时是 GC roots，不可把旧引用状态直接复用；批量 breakpoint patch/publish 和 ITT 方法注册值得继续借鉴。本轮未新增无需插桩的断点后端或 ITT JIT 注册。剩余主要方向是 per-opcode 状态写入/解码、父包认证、帧进入/退栈和冷编译成本，不能省略跟踪来制造相当的结果。

小报告/原始 transcript/hash 保留；大 profiler database 留在 Linux。所有阶段结束后审计本次自有作业、采样 kernel alerts、cgroup events、冻结/已构建/report 二进制以及两产品同步范围。共享 cgroup 累计 peak 不称作本任务峰值，不宣称先前主机卡死原因已解决。

数据：[debug_core3_v65_performance_report.json](debug_core3_v65_performance_report.json)。

---

## v64 历史记录

# Debug performance: v64 original entry-row projection (2026-10-12)

v63 的 256 槽缓存已减少多目标的反复冷认证，但加入 module XOR 也给单目标 50,000,000 次调用增加了约五千万条指令。本轮保留固定 256 槽（支持的 64 位目标上为 20 KiB，每个 execution scope 分配一次），恢复原来的 function-only 投影。新 &255 分区细化原 &63 分区：原先不同槽的两个函数编号不会因为扩大容量而撞槽，也不在 warm lookup 增加 XOR。

module ID 仍属于完整 key。跨模块同编号若冲突，必须 miss 并走冷路径，不能借用另一模块的 plan。全部 record/publication/epoch/module/function/generation/raw+typed entry、live sealed plan 地址及 shared control block 匹配保持。完整 parent packet/reference/flags 校验、GC roots、原生/restore 权限和替换/重启失效不变；没有 JIT IR/ABI/cache 格式修改，observe policy 38、resumable policy 17 保持。主产品与 ROS 同范围同步，已有产品差别保留。

全部构建/执行在 SSH Linux 64 GiB / swap=0 cgroup 内，两产品 xmake -j32 通过；本机仅处理源码和小报告。构建 CPU0–31，计时/target TID CPU21，PMU companion CPU9；不宣称主机或核心独占。v63 的完整结果、FFT 修正和采集前拒绝记录保留在下面历史记录。本轮 1124 个未采样样本：Core3 418、live-reference SIMD 66、calls/callchain pilot 180、长特性/256 目标 81、短 fanout 250、FFT 39、额外短 fanout 配对复查 90。相同 Wasm hash、独立 checksum、旧/新交替，guest CLOCK_MONOTONIC 排除 JIT/startup/attach/停点，完整范围/raw 值与进程时间另存。中位数不是显著性或普遍无回退证明。

| 长输入（主产品，旧/新/实际插件各 9 次） | v63 debug ms | v64 debug ms | Wasmtime Wasm/LLDB ms | v64 / Wasmtime |
| --- | ---: | ---: | ---: | ---: |
| typed_call_ref | 7893.375395 | 7847.560209 | 271.296875 | 28.93 |
| memory64 | 2303.800346 | 2306.403086 | 941.224941 | 2.45 |
| call_ref_fanout_256 | 2482.570473 | 2472.588640 | 88.178945 | 28.04 |

| 匹配硬件窗口 | v63 Instructions Retired | v64 Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| memory64 | 46,056,097,993 | 46,056,278,057 | -0.000391% |
| typed_call_ref | 130,537,941,375 | 130,436,760,136 | 0.077511% |
| call_ref_fanout_256 | 31,726,027,223 | 31,705,280,065 | 0.065395% |

12 次 VTune 2026.4 采集为 6 次 driverless/无 stack 硬件 Hotspots 和 6 次 Microarchitecture summary 硬件计数。summary 不是完整 detailed uarch sampling。真实 event/period、CPI/clock/MUX、E-core 比例、PID/TID/caps/hash/boot 留在 JSON/receipt；不使用 profiler elapsed time 当吞吐，也不把空闲 P-core 或 Unknown JIT 样本当作精确 hotspot 归因。窗口有 profile_end 断点及可能的保守 hint 检查，不能直接等同于 none 配置。2 次 Advisor 2026.0 Survey 的有效性和 native 行另存，不据此宣称 Trip Counts/FLOPs 或完整 JIT loop 分析。[Intel 官方 driverless 方法](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html)。

| 目标数（各 5 次） | 主产品 v63 → v64 debug ms | ROS v63 → v64 debug ms |
| --- | ---: | ---: |
| 1 | 208.964560 → 210.915701 | 207.497884 → 206.947680 |
| 8 | 223.834257 → 221.669301 | 223.677001 → 224.606662 |
| 32 | 228.998960 → 230.378542 | 233.444482 → 232.470825 |
| 64 | 235.600061 → 234.291673 | 233.545713 → 234.620748 |
| 256 | 254.523317 → 263.953143 | 250.762233 → 263.673601 |

首批 5 次短版 256-target 新版中位数增加 3.70%（主产品）/5.15%（ROS）；新旧 min–max 重叠，普通模式也有波动。没有隐藏或舍弃这些有效数据。为检查该问题，另做每产品 9 组旧/新交替配对，加 ordinary 与真实 Wasmtime Wasm plugin 对照；原数据与复查数据分开保留。复查仍不构成普遍无回退证明，regression_ruled_out=false。

| 256-target 复查（每模式/版本 9 次） | v63 ms | v64 ms | 中位数下降 |
| --- | ---: | ---: | ---: |
| uwvm2 / uwvm-none | 250.995025 | 250.489409 | 0.201% |
| uwvm2 / uwvm-ordinary | 37.540203 | 37.087094 | 1.207% |
| uwvm2-ros / uwvm-none | 250.900520 | 251.304837 | -0.161% |
| uwvm2-ros / uwvm-ordinary | 37.159345 | 36.961940 | 0.531% |

| 真正 FFT（同一 Wasm/独立种子与逆变换 oracle，各 3 次） | 中位数 ms | min–max ms |
| --- | ---: | ---: |
| uwvm2/before/ordinary | 0.074141 | 0.074131–0.078023 |
| uwvm2/before/debug | 0.986281 | 0.985854–1.011055 |
| uwvm2/after/ordinary | 0.074101 | 0.073985–0.080146 |
| uwvm2/after/debug | 0.989643 | 0.987216–0.991892 |
| uwvm2-ros/before/ordinary | 0.074543 | 0.074393–0.075297 |
| uwvm2-ros/before/debug | 0.986432 | 0.985468–0.988895 |
| uwvm2-ros/after/ordinary | 0.074633 | 0.074533–0.078163 |
| uwvm2-ros/after/debug | 0.988933 | 0.987275–1.011828 |
| wasmtime/current/ordinary | 0.060214 | 0.059365–0.061823 |
| wasmtime/current/gdb-o2 | 0.060720 | 0.056815–0.061925 |
| wasmtime/current/guest-lldb | 0.407903 | 0.405511–0.411775 |
| native/current/ordinary | 0.048487 | 0.048092–0.050521 |
| native/current/gdb | 0.049776 | 0.049539–0.051812 |

| FFT 全进程到结果（包含冷准备，各 3 次） | 中位数秒 |
| --- | ---: |
| uwvm2/before/ordinary | 0.856555 |
| uwvm2/before/debug | 32.742379 |
| uwvm2/after/ordinary | 0.872090 |
| uwvm2/after/debug | 32.863283 |
| uwvm2-ros/before/ordinary | 0.867655 |
| uwvm2-ros/before/debug | 33.841959 |
| uwvm2-ros/after/ordinary | 0.867664 |
| uwvm2-ros/after/debug | 33.416653 |

FFT 的浮点结果用独立 xorshift32 初始两项与逆变换恢复关系验证，绝对误差 ≤1e-12；原生从同 source hash 构建。FFT 内核很短，微小百分比差异不支持精确推断。冷编译/准备明显长于内核时间，尚未解决；这里完整保留 ready/process-to-result 数据，不能把仅测内核运行误写成整体性能相当。传统矩阵 `float` 是递推，不是 FFT。

正确性：2 个修改组件、2 个 ASan/UBSan executable、每产品 6 组重点 PTY（合计 12）、2 组动态 call_ref PTY / 6 次重启、132 个深层帧、44 个 GC terminal traps、12 个 sampling guard cases 通过。warm callee 在同次执行退栈后替换结果 42 → 44，旧代次和活动目标替换仍拒绝。本轮未重跑全部 v63 的 38 组 PTY 和完整普通矩阵；其历史资格单独保留。没有本机、Windows/QEMU 或 C++ module 构建/执行。

Core3 二十个输入仍有一个 deterministic_profile 缺口，19 个运行资格通过；显式 selector 和 canonical NaN 正 probe 未完成，整张矩阵保留失败，coverage_complete=false。**尚未达到 Wasmtime/GDB 的普遍执行效率**，parity_achieved=false；不同引擎 ordinary 编译/GC 的差别也不能全部称为调试器开销。

只读参考 Wasmtime 65abe37529a6bca836efcac1dcb920d5c7890580 的增量 GC-rooted 状态槽、批量 breakpoint patch/publish 和 ITT agent，未修改其源码。[真实 Wasm/LLDB 插件](https://docs.wasmtime.dev/examples-debugging-guest.html) 与原生 DWARF/GDB 分开测量。下一步仍需减少 per-opcode 状态写入、parent packet 解码和帧进入/退栈，并定位冷编译开销；不会通过省略最新引用状态或校验来制造性能相当。

同步清单遗漏的两份旧 cache DATA unit 已补齐，v63 正确审计 136 个文件后才允许本轮构建；只取消了无子进程的自有等待编排。最终 source/receipt/host 审计和小归档见 JSON，原始 profiler database 留在 Linux。共享 cgroup 累计 peak 不称为本任务峰值，历史 kernel warnings 保留，未宣称已修复之前主机卡死的原因。

数据：[debug_core3_v64_performance_report.json](debug_core3_v64_performance_report.json)。

---

## v63 历史记录

# Debug performance: v63 bounded entry cache (2026-10-12)

针对 v62 在主产品 256 目标输入中的 1.49% 观测回退，本轮将已认证的 entry-plan 元数据缓存从 64 槽增到 256 槽，并将 module ID 混入索引。固定存储在支持的 64 位目标上为 20 KiB，每个已准入 execution scope 分配一次；不按指令分配。同模块 256 个连续函数编号可以各占一槽，但 caller 仍可能与 target 冲突，跨模块映射也不是无冲突映射。unit 检查完整目标周期、跨模块相同函数编号、碰撞和固定容量。

这次仅修改 entry-cache header 与 DATA unit，主产品与 ROS 同范围同步。全部 key 字段、live sealed plan 地址及 shared control block 匹配、冷路径完整认证、替换/重启失效和调用者 packet/reference/flags 验证保持。cache DATA 不授予执行或 restore 权限，未改变 GC roots、JIT IR、ABI/cache 格式；observe policy 38、resumable policy 17 保持。独立的 32 槽 function-identity cache 尚未修改，仍可能在大 fanout 下 miss。

全部编译、Wasm/native/PTY 和 profiler 执行在 SSH Linux 64 GiB / swap=0 cgroup 内完成，两产品 xmake -j32 通过。本机只有源代码阅读/编辑和小报告聚合。计时/target TID 固定 CPU21；CPU9 是 hybrid PMU companion，不把空闲 P-core 百分比当作 workload 结论；不宣称主机或 CPU 独占。

| 目标数（每版/产品 5 次） | 主产品 v62 → v63 debug ms（下降比例） | ROS v62 → v63 debug ms（下降比例） |
| --- | ---: | ---: |
| 1 | 203.116492 → 200.200476 (1.436%) | 198.010250 → 199.137234 (-0.569%) |
| 8 | 216.432183 → 216.207743 (0.104%) | 215.834417 → 215.193072 (0.297%) |
| 32 | 227.816567 → 226.608933 (0.530%) | 229.205698 → 227.542112 (0.726%) |
| 64 | 235.229639 → 235.340708 (-0.047%) | 235.287788 → 233.446605 (0.783%) |
| 256 | 313.578448 → 250.853962 (20.003%) | 307.206369 → 248.802080 (19.011%) |

新增 10,000,000 次调用的 256-target 长输入：前版 3 次未采样 ms 为 [3107.497128, 3103.488242, 3125.678987]，新版为 [2468.308416, 2462.224407, 2462.024785]，中位数 3107.497128 → 2462.224407 ms。独立闭式 checksum 与小规模迭代交叉检查、官方 parse/validate/print 和 Wasm hash 保留。匹配硬件退休指令下降 16.602%。硬件窗口另有 profile_end 断点及保守 hint 检查，不能直接等同于 none 吞吐配置。

| 长特性（主产品，各版本/Wasmtime 9 次） | v62 debug ms | v63 debug ms | Wasmtime Wasm/LLDB ms | v63 / Wasmtime |
| --- | ---: | ---: | ---: | ---: |
| typed_call_ref | 7811.254073 | 7872.121883 | 271.063248 | 29.04 |
| gc_struct | 6714.892948 | 6715.133062 | 253.504890 | 26.49 |
| memory64 | 2301.405841 | 2302.713916 | 936.106925 | 2.46 |

| 匹配硬件窗口 | v62 Instructions Retired | v63 Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| memory64 | 46,056,356,384 | 46,056,447,518 | -0.000198% |
| typed_call_ref | 130,495,951,794 | 130,549,860,901 | -0.041311% |
| gc_struct | 99,439,072,417 | 99,433,682,076 | 0.005421% |
| simd_integer_mix | 48,200,240,257 | 48,200,514,787 | -0.000570% |
| call_ref_fanout_256 | 38,041,454,144 | 31,725,648,607 | 16.602429% |

本轮 2586 个未采样计时样本保留完整 raw/range：Core3 1254、live-reference SIMD 198、控制流/浮点递推/cache/calls 矩阵 540、调用 pilot 180、长特性 81、long SIMD 6、fanout 250、原始 long fanout 6、修正 gate 的 long fanout 6、独立 FFT 65。guest CLOCK_MONOTONIC kernel 时间排除 JIT、startup、attach 与资格停点；startup/process 时间另存。前后交替、同 hash 与独立 checksum。代码布局、共享核心/缓存和机器负载会影响小幅差异，不将中位数当作显著性、普遍无回退或跨引擎因果证明；ordinary/native GNU GDB、Wasmtime native GDB 与实际 Wasm/LLDB plugin 分开记录。

本轮额外用同一个 radix-2 FFT Wasm 文件分别测试两版 UWVM、普通/原生 GDB 和真实 Wasmtime Wasm/LLDB。原生 FFT 从同一个 source hash 编译；独立 xorshift32 种子和逆变换恢复关系校验 checksum（绝对误差 ≤1e-12），不是只比较多个实现输出恰好相同。此前 v62 的矩阵描述误把 `float` 递推写成 FFT；那个矩阵不是 FFT，本轮实际 FFT 单独列出。短 FFT 计时不能支持微小百分比的精确结论。完整 first-prompt / process-to-result 时间保留，冷编译/准备成本没有被 guest 内核计时覆盖；本轮可见调试首提示符约 33 秒而内核运行仅约 1 毫秒，这项差距没有宣称解决。

| FFT 模式（各 5 次） | 中位数 ms | min–max ms |
| --- | ---: | ---: |
| uwvm2/before/ordinary | 0.074316 | 0.074168–0.074514 |
| uwvm2/before/debug | 0.987431 | 0.985769–0.999728 |
| uwvm2/after/ordinary | 0.074242 | 0.074161–0.075976 |
| uwvm2/after/debug | 0.987444 | 0.985131–0.989388 |
| uwvm2-ros/before/ordinary | 0.074400 | 0.074369–0.074826 |
| uwvm2-ros/before/debug | 0.987539 | 0.984740–0.990035 |
| uwvm2-ros/after/ordinary | 0.074563 | 0.074405–0.076504 |
| uwvm2-ros/after/debug | 0.987766 | 0.985261–0.992619 |
| wasmtime/current/ordinary | 0.058874 | 0.058407–0.059981 |
| wasmtime/current/gdb-o2 | 0.058836 | 0.055092–0.059551 |
| wasmtime/current/guest-lldb | 0.392193 | 0.389333–0.397348 |
| native/current/ordinary | 0.049002 | 0.047865–0.051243 |
| native/current/gdb | 0.049801 | 0.049641–0.056638 |

20 次 VTune 2026.4 采集为 10 次 driverless/无 stack 的硬件 Hotspots 与 10 次 Microarchitecture summary 硬件计数，后者不是完整 detailed uarch sampling。CLI、事件/period、退休指令/clock/CPI/MUX、E-core 指标、PID/TID/affinity/capabilities/hash/boot 保留；不拿 profiler elapsed time 计算吞吐。Unknown JIT 未能精确归因到指令。2 次 Advisor 2026.0 Survey 的有效性及 native 行记录另存，不作为吞吐、Trip Counts/FLOPs 或 JIT loop 归因。[Intel 官方 driverless 方法](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html)。

正确性：这次修改的 2 个组件与 2 个 ASan/UBSan executable、38 组 PTY、2 组动态 call_ref PTY / 6 次重启、132 个深层帧、44 个 terminal GC traps、12 个 sampling guard cases 通过。warm callee 退栈后同次执行替换为新函数，结果 42 → 44；旧代次和活动帧替换仍拒绝。没有宣称本轮重跑全部未修改的组件或 macOS/Windows-QEMU/C++ module 编译。

Core3 20 个特性输入通过官方格式检查，19 个运行资格通过。deterministic_profile 的 canonical NaN 正 probe 和显式 UWVM selector 仍未完成，矩阵保留失败，coverage_complete=false。**尚未达到 Wasmtime/GDB 的普遍执行效率**，parity_achieved=false。

只读核对 Wasmtime revision 65abe37529a6bca836efcac1dcb920d5c7890580。其增量状态槽带 GC rooting、断点批量 patch/发布设计值得继续采用；完整引用同步和停点状态不能通过复用旧 payload 省略。[Wasmtime 实际 Wasm 调试插件](https://docs.wasmtime.dev/examples-debugging-guest.html) 使用 code instrumentation，与直接 native DWARF/GDB 区分。下一步主要成本仍是 per-opcode 状态写入、parent packet 认证和帧进入/退栈。原始 profiler 数据库留在 Linux；host 审计保留历史 kernel warning，共享 cgroup peak 不称为本任务峰值，也没有宣称修复此前主机卡死原因。

新增 long fanout 最初缺少 `profile_end` 导出，被采集前的强校验拒绝，VTune 未启动。原始失败 script/log/receipt 与未采样 6 次计时保留；新副本只在输出之后加入 gate，官方格式检查及额外 6 次计时通过后重采集。有效硬件 20 次之外另有 1 次采集前拒绝，不将它伪装成成功采样。

数据：[debug_core3_v63_performance_report.json](debug_core3_v63_performance_report.json)。

---

## v62 历史记录

# Debug performance: v62 warm entry authentication (2026-10-12)

本轮将已认证的 warm entry 命中移到重复的 engine/context/profile 所有权检查之前。实际 TLS incarnation/epoch、module/function 范围、record/publication/generation、两个 native entry，以及 live sealed plan 的地址与共享 control block 均仍逐次匹配。缓存只在同一已准入执行 scope 内有效；执行 lease 固定发布对象，已发布代码禁止重新配置 profile。函数替换改变 key，重启销毁 scope。miss、无缓存、分配失败、冷 observation/restore 仍执行完整 source/profile/generation 认证。调用者 awaiting packet、完整引用/flags 捕获、GC 和原生帧权限未改变。

相同范围的修改已同步两套产品，并保留主产品/ROS 原有 bridge-scope helper 差别。SSH Linux 64 GiB / swap=0 cgroup 内两边 xmake -j32 通过，构建 CPU0–31；计时与所有 target TID 固定 CPU21，PMU companion 为 CPU9。本机仅阅读/编辑源码和聚合小报告，没有本机编译或执行。主机/CPU 没有独占声明。没有 JIT IR、ABI 或 cache 格式修改，observe policy 38、resumable policy 17 保持。

只读参考 Wasmtime 65abe37529a6bca836efcac1dcb920d5c7890580 的 state slots、function references、breakpoint edit 和 VTune agent。其 local.set/dirty operand 增量写入使用 GC-rooted slots，断点/单步配置批量 patch 后重新发布代码；UWVM 不能直接复用未同步的旧引用快照。这次实现的是已认证元数据的重复检查优化，尚未实现相同的状态槽/patchpoint 架构。[实际 Wasmtime Wasm/LLDB 插件](https://docs.wasmtime.dev/examples-debugging-guest.html) 与 native DWARF/GDB 分别测量。

本轮 2509 次未采样计时：Core3 1254、live-reference SIMD 198、控制流/FFT/cache/calls 矩阵 540、调用 pilot 180、长特性 81、long SIMD 6、fanout 250。WASI CLOCK_MONOTONIC kernel ns 排除 JIT、启动、attach 与资格停点；startup/process 时间另存。前后交替，相同 Wasm hash、独立 checksum。完整分布保留，不能把小样本中位数当作显著性/等效或普遍无回退证明。

| 长输入（主产品，各版本/Wasmtime 9 次） | v61b debug ms | v62 debug ms | Wasmtime Wasm/LLDB ms | v62 / Wasmtime |
| --- | ---: | ---: | ---: | ---: |
| typed_call_ref | 7855.718921 | 7807.248490 | 271.065095 | 28.80 |
| gc_struct | 6715.886713 | 6715.632766 | 253.569367 | 26.48 |
| memory64 | 2310.085495 | 2307.850098 | 938.266399 | 2.46 |

长 call_ref 中位数观测下降 0.617%；其匹配硬件退休指令下降 0.535%。控制输入的完整值与范围也保留。原生/GDB 与引擎 ordinary 结果分别列在 JSON；不同引擎普通编译和 GC 性能的差别不能全部描述成 debugger overhead。**仍未达到 Wasmtime/GDB 的执行效率。**

| 匹配硬件窗口 | v61b Instructions Retired | v62 Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| memory64 | 46,055,568,305 | 46,056,657,777 | -0.002366% |
| typed_call_ref | 131,198,368,754 | 130,495,906,104 | 0.535420% |
| gc_struct | 99,433,296,643 | 99,434,530,665 | -0.001241% |
| simd_integer_mix | 48,199,696,752 | 48,199,107,888 | 0.001222% |

16 次 VTune 2026.4 采集为 8 次 driverless/无 stack 的硬件 Hotspots 和 8 次 Microarchitecture summary 硬件计数；summary 不等于完整 detailed uarch sampling。真实 event rows、period、CPU mask、PID/TID/capability/hash/boot、CPI、MUX、E-core 相对份额保留。窗口包含启用的 profile_end 停点及保守 hint bucket 检查，不能等同于无断点吞吐；没有用 profiler elapsed time 算吞吐。Hotspots 的稀疏样本不能支持精确函数排序，Unknown JIT 仍不能逐条归因，空闲 P-core 比例也不是 workload 结论。[Intel driverless profiling 方法](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html) 与实际 CLI/receipt 一并保留。

Advisor Survey 在长 call_ref 上前后有效性与 native function 行记录另存；不将 instrumented Survey time 当作吞吐，也没有 Trip Counts/FLOPs 或完整 JIT loop 归因。

| 动态目标数（每版/产品 5 次） | 主产品 v61b → v62 debug ms | ROS v61b → v62 debug ms |
| --- | ---: | ---: |
| 1 | 202.000161 → 200.010040 | 213.655015 → 210.395279 |
| 8 | 219.354972 → 215.667216 | 224.307260 → 223.046889 |
| 32 | 244.905165 → 227.800913 | 233.439608 → 231.546393 |
| 64 | 245.230996 → 243.654538 | 237.341394 → 235.336019 |
| 256 | 310.010059 → 314.627264 | 306.658838 → 307.871170 |

1/8/32/64/256 目标均有独立 checksum 和实际 Wasmtime plugin 停点/值/单步/trace 资格，分布重叠与可能回退不隐藏。完整 raw 值见 JSON。

正确性：30 个组件、38 组真实 PTY、2 组动态 call_ref PTY / 6 次重启、132 个深层帧、44 个 GC terminal traps、12 个 ASan/UBSan executable、12 个 sampling guard cases 通过。单次执行中 warm callee 退栈后替换函数，结果 42 → 44，并拒绝旧代次/活动帧替换；完整引用、16 bytes/tag/padding、flag 0/1/2 与 legacy 32/64 little/big 布局仍检查。没有 macOS/Windows-QEMU/C++ module 构建或执行。

20 个 Core3 输入覆盖十类变更并通过官方格式检查，19 个运行资格通过。deterministic_profile 的 canonical NaN 正 probe 和 UWVM 显式 selector 仍未解决，因此 coverage_complete=false，整张 Core3 matrix 保留失败；普通模式 NaN 不能据此直接判成 Core3 违规。

两次编排问题单独保留：本机打包的 whole-file equality 假设被主/ROS 原有 helper 差别拒绝，未发生错误 remote apply；改为逐产品相同 scoped delta。fanout runner 提前读未生成的 collector phase 文件后退出，未启动子任务；保留原 script/log，新 runner 等待全部 19 个 collector phases 正常结束后再执行，没有丢弃既有计时样本。

最终 host/source/receipt 审计和小证据归档见 JSON；原始 VTune/Advisor database 留在 SSH Linux。共享 cgroup 累计 peak 不能称为本任务峰值，历史 perf warnings 保留，先前主机卡死原因没有被宣称解决。待优化仍是完整状态写入、当前 caller packet 认证和帧进入/退栈，而不是显示符号或单次索引查询。

数据：[debug_core3_v62_performance_report.json](debug_core3_v62_performance_report.json)。

---

## v61 历史记录

# Debug performance: v61b dynamic function identity (2026-10-12)

本轮减少动态 call_ref 的重复函数归属查询：已准入的 LLVM/native-TLS 执行使用 32 槽、1 KiB 的索引缓存，每次命中仍核验当前 registry 行的 runtime_func；调用目标 builder 同时返回已经核验的 provider 身份。缓存没有可执行地址、函数指针或 code owner。raw/typed entry 继续从当前发布槽读取；没有移除锁、签名检查、帧认证、packet 校验或完整引用捕获。冷编译和 map-backed TLS 保留完整范围查找。

两份仓库在 SSH Linux 64 GiB / swap=0 cgroup 内以 xmake -j32 构建通过；允许 CPU0–31 构建，全部计时与目标线程固定 CPU21，硬件 PMU companion 为 CPU9。没有本机编译或 workload 执行，主机与 CPU 并未声明独占。生成的 IR / ABI / cache 格式没有改变，因此 observe policy 38、resumable policy 17 均保持。

源代码参考 Wasmtime 65abe37529a6bca836efcac1dcb920d5c7890580 的 function reference、guest state slot 和 VTune agent；这三个文件保持只读。Wasmtime call_ref 直接从 VM-held record 取得 code/context，UWVM 当前继续核验其不同的引用表示，先削减重复发现。Wasmtime instrumentation slots 是 GC roots，不能据此直接复用 UWVM 的旧引用载荷。[Wasmtime guest debugging](https://docs.wasmtime.dev/examples-debugging-guest.html) 是真正 Wasm/LLDB 插件基线，单独测量 native DWARF/GDB 和 GNU GDB。

本轮保留 2509 次未采样计时：两产品 Core3 1254、live-reference SIMD 198、传统控制流/FFT/cache/calls 矩阵 540、调用/调用链 pilot 180、长特性 81、long SIMD 6、目标 fanout 250。JIT、启动、attach 和停点不计入 guest CLOCK_MONOTONIC kernel ns；启动和进程时间另存。前后版本逐次交替，使用相同 Wasm hash 和独立校验和。共享主机与控制输入的计时变化不能支持把整个小差异都归因于这一缓存，更不能证明普遍 parity。

| 长输入（主产品，各版本/Wasmtime 9 次） | v60 debug ms | v61b debug ms | Wasmtime Wasm/LLDB ms | v61b / Wasmtime |
| --- | ---: | ---: | ---: | ---: |
| typed_call_ref | 8010.897272 | 7871.188421 | 271.181575 | 29.03 |
| gc_struct | 6792.816405 | 6806.576016 | 258.351117 | 26.35 |
| memory64 | 2346.407065 | 2309.618581 | 940.741061 | 2.46 |

长 call_ref 未采样中位数观测下降 1.74%；memory64 控制也下降 1.57%，gc_struct 上升 0.20%。所以不能把前者全部描述成缓存带来的吞吐收益。匹配硬件计数中，call_ref 退休指令下降 1.39%，而三个控制的计数基本不变。仍未达到 Wasmtime/GDB 的执行效率。

| 匹配硬件窗口 | v60 Instructions Retired | v61b Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| memory64 | 46,056,649,313 | 46,056,531,110 | 0.000257% |
| typed_call_ref | 133,040,100,682 | 131,188,536,526 | 1.391734% |
| gc_struct | 99,433,765,439 | 99,434,611,780 | -0.000851% |
| simd_integer_mix | 48,200,286,459 | 48,200,362,146 | -0.000157% |

16 次 VTune 2026.4 采集分别是 8 次 driverless、无 stack 的硬件 Hotspots 和 8 次 Microarchitecture summary 硬件计数。actual event rows、period、PID/TID/affinity/capability/hash/boot、CPI、MUX 和 E-core 份额保留在 JSON/原始 CSV。summary 不是完整 detailed uarch sampling。启用的 profile_end 停点及保守 hint bucket 检查包含在这一匹配计数配置内；不可将它等同于无断点吞吐，也未用 profiler elapsed time 计算吞吐。空闲 P-core 派生比例不是 workload 结论。

Hotspots 前后均有 checkpoint_actual_frame、call_ref resolver、shadow_ledger enter、activation leave 等实际 native 行；JIT 仍有 Unknown module，不能逐条命名归因。实际 300M 事件粒度较粗，native 行排序/clock 差异不能证明精确热点占比。call_ref 的 CPI 为 0.247610 → 0.246852；完整 E-core top-down 相对份额另列，不能从比例推断绝对 store 成本。

本轮 Advisor 2026.0 在长 call_ref 上获得有效 Survey/functions 行，前后分别 38 / 34 个函数行，collector attach、detach、正常退出及校验和均通过。它提供 native 函数观测，不能把未知 JIT loop 行说成完整 JIT 指令归因；没有做 Trip Counts/FLOPs。本轮有效性与 v60 SIMD 空 Survey 的失败是不同输入，历史失败仍保留。

新增 1/8/32/64/256 目标的独立输入，每个目标返回不同值；typed table.get → local → call_ref 的取模校验和由闭式公式与小规模独立循环核对，官方 parse/validate/print 均通过。每个产品每组前后 ordinary/debug 和实际 Wasmtime guest 各 5 次，连同 actual stop/value/step/trace 资格。256 目标超过两个缓存的局部性，结果不能只挑选单目标热路径：

| 动态目标数 | 主产品 v60 → v61b debug ms | ROS v60 → v61b debug ms |
| --- | ---: | ---: |
| 1 | 205.079435 → 202.421570 | 212.719359 → 200.732747 |
| 8 | 220.415919 → 216.162724 | 238.089457 → 226.316563 |
| 32 | 240.744343 → 237.836860 | 244.848837 → 237.695052 |
| 64 | 244.976567 → 243.778445 | 246.612558 → 243.849376 |
| 256 | 308.316855 → 312.679707 | 307.570255 → 306.941348 |

主产品 256 目标的中位数上升 1.42%，ROS 下降 0.20%；各 5 次的分布有重叠，不能宣称已证明普遍无回退或必然提速。全部原始值与范围保留；高 fanout 的缓存开销需要继续检查。

正确性：30 个组件、38 组既有真实 PTY、2 组新增动态 call_ref PTY、6 次新用例重启、132 个深层帧观测、44 个 GC terminal trap、12 个 sanitizer executable、12 个 sampling guard checks 通过。新 PTY 在同一执行中先得到 42，在 warm lookup 的 callee 退栈后替换函数，再得到 44，同时核对 live locals/reference、旧代次和活动帧替换拒绝；两产品均通过。emit/MCJIT 组件继续逐槽检查全部 16 bytes、sNaN/v128/reference 上半部/tag、raw/public、flag 0/1/2 与改变后的值。

20 个 Core3 输入覆盖十类变更并通过官方格式验证，19 个通过运行资格；deterministic profile 的正 canonical NaN probe 在前后 ordinary/debug 均失败，显式 UWVM profile selector 也缺失。因此 coverage_complete=false，整张 Core3 matrix 仍为失败；普通模式的 NaN 行为不能据此直接判成 Core3 违规。没有在本机、Windows/QEMU 或 C++ module 配置执行测试。

首次候选 ROS 构建使用主运行时独有 TLS 字段，编译失败，未用于性能结论；修正版使用共有的 execution depth 并限制 native TLS，两产品在新版本路径重建。新增 PTY 的 import 显示身份在首次执行前核对为实际 owned import slot main:0，provider:0 的实际执行及 42→44 结果独立核对；测试 harness hash 写入 receipt。未采样 SIMD preflight 不需要 CAP_PERFMON，因此只停止了尚无子进程的本任务等待编排，重启为只给 profiler guards 授权的版本。未信号其他任务。

最终审计：没有本任务遗留运行中的编排，工作目录 binary 与冻结的合格 binary 一致，本轮无新 kernel sampling alert / OOM / memory.max event。共享 cgroup 累计 peak 22.40 GiB，不是本任务专属峰值。旧 boot 内核 perf 降采样警告仍保留，先前主机卡死原因没有被宣称已解决。3904 份小型证据与实际 PTY/插件 transcript 压缩归档已校验 hash；完整 profiler database 留在 SSH Linux。

数据：[debug_core3_v61_performance_report.json](debug_core3_v61_performance_report.json)。待优化的主要结构仍是每条指令的完整状态写入、帧进入/调用者认证与 observer JIT 插桩，而不是剩余的一次索引查询。高性能停点不会自动消除这些成本；本轮没有删除记录来制造普通执行速度。

---

## v60 历史记录
2026-10-12 当前为 observation policy 38 / resumable policy 17，完成 v60 原始引用快照的向量复制优化。两套产品均在 SSH Linux 64 GiB、swap=0 cgroup 内完成 `xmake -j32`、回归、计时及采集；本机只读取源码、编辑和整理小报告。完整数据见 [debug_core3_v60_performance_report.json](debug_core3_v60_performance_report.json)。

现有 Linux x86-64 小型完整 observer local prefix 中，真实 carrier 为 16 字节的 raw reference 使用两个 i64 的整数向量读写，减少低 codegen 优化级别下 i128 标量展开。先根据初始化 flag 选择 local 或完整不可变零存储，再读取；不再重复对已清零的值作 select。每次仍刷新 flag 和全部 payload/tag/padding，flag 0/2 不读取未初始化 local；没有缓存旧引用、改变 GC roots、帧身份或恢复权限。public nullness、numeric、大 prefix、未知类型及其他平台保留原路径。

最初 CPU20 上另有独立任务的 VTune 进程与本轮编译重叠。本轮保留该核心的 807 个早期未采样样本，但性能结论使用迁移后的 CPU21 系列；只停止本任务的空闲编排进程，没有给其他任务发信号。核对空闲后固定 E-core CPU21 并不等于独占核心，也不能据此解释全部历史波动。

下面是相同 Wasm、独立 checksum、前后版本交替的未采样 guest CLOCK_MONOTONIC 内核中位数，排除 JIT、启动、连接和资格停点。Core 3 每组 3 次，live-reference SIMD 每组 9 次。Wasmtime 49.0.2 接入真实 LLDB 34 Wasm 插件，并核对 qWasmCallStack 包；native DWARF/GDB、引擎 ordinary 执行及完整 min/median/max 在 JSON 中另列。小样本中位数不证明显著性、等效或全部用例无回退。

| 产品 / 内核 | v59 debug ms | v60 debug ms | Wasmtime Wasm/LLDB ms | 耗时下降 |
| --- | ---: | ---: | ---: | ---: |
| uwvm2 gc_struct | 1.373596 | 1.362212 | 0.070791 | 0.83% |
| uwvm2 gc_array | 1.647948 | 1.656090 | 0.086427 | -0.49% |
| uwvm2 typed_call_ref | 1.626590 | 1.636407 | 0.061278 | -0.60% |
| uwvm2 gc_recursive | 3.297800 | 3.279532 | 0.773484 | 0.55% |
| uwvm2 live-reference SIMD | 0.582985 | 0.577498 | 0.065423 | 0.94% |
| ROS gc_struct | 1.395566 | 1.389516 | 0.069023 | 0.43% |
| ROS gc_array | 1.675798 | 1.703931 | 0.087400 | -1.68% |
| ROS typed_call_ref | 1.660293 | 1.653043 | 0.061130 | 0.44% |
| ROS gc_recursive | 2.861210 | 2.757695 | 0.764292 | 3.62% |
| ROS live-reference SIMD | 0.603416 | 0.608012 | 0.067608 | -0.76% |

性能仍未整体达到 Wasmtime Wasm 调试或原生 GDB，JSON 保持 parity_achieved=false。ordinary 引擎代码生成也不同，跨引擎差距不能全部归因于调试器。没有加入通用无插桩断点后端，也没有声称所有调用或控制内核都变快。

| 无断点调用 pilot（9 次） | v59 ms | v60 ms | native GNU GDB ms |
| --- | ---: | ---: | ---: |
| uwvm2 calls | 28.567427 | 28.506823 | 0.390258 |
| uwvm2 callchain | 63.263058 | 63.446710 | 0.425109 |
| ROS calls | 28.542290 | 28.554423 | — |
| ROS callchain | 63.579877 | 63.644310 | — |

两套产品保留 2,315 个新核心未采样样本：1,254 个 Core 3、110 个纯 SIMD、198 个 live-reference SIMD、540 个传统计算/缓存/控制流/调用 none/cold/disabled、180 个调用 pilot、6 个主长输入计时和 27 个额外长特性计时。另保留迁移前 807 个 CPU20 样本，总计 3,122 个；没有删掉失败或离群值。

28 个组件、38 组真实 PTY 交互、132 次深层帧读取、44 个 GC terminal trap、10 个 ASan/UBSan/LSan 可执行检查、12 项采集保护检查通过；还包含 52 个 SIMD 实际停点和 24 个操作数 policy 用例。现有混合 local 组件实际调用生产 emitter、LLVM promotion 和 MCJIT，逐槽检查全部 16 字节：reference 上半部/tag、sNaN、v128、raw/public、真正未初始化存储、flag 0/1/2、重新赋值和再次缺失。

新增 8 组前后版本、两个产品、llc O0/O1 的 native lowering 证据使用同一个实际八次捕获组件，保留 IR、反汇编、对象/可执行 hash 和逐字节检查结果。whole-component 静态指令数只能证明这个组件的展开，不能替代 guest 动态退休计数或吞吐。

辅助脚本的两次失败保留：llc 无法解析 LLVM 打印出的 +snan(...)，随后临时 bitcode 输出副本仍以 argc<=2 拒绝新的第三个参数。修复只在临时副本增加可选 bitcode 输出并接受对应参数；原组件、生产源码和八次 native 字节检查均不改动。新 llc 输入为 bitcode，没有将 signalling NaN 改写为其他浮点数。

每个已列 Core 3 类别有独立输入；20 个通过官方 parse/validate/print，19 个通过实际运行、断点、locals/operands、nw → ni → nw 和 trace 资格。deterministic profile 的正 canonical NaN probe 在前后 ordinary/debug 均失败，且 UWVM 没有显式 profile selector。整个 Core 3 矩阵仍失败，coverage_complete=false；普通模式的 NaN 行为不能据此直接判定为 Core 3 违规。没有在本机执行测试，也未测 Windows/QEMU 或 C++ module 构建。

VTune 2026.4 在 4 个匹配长输入上完成前后硬件 Hotspots 和 Microarchitecture summary，共 16 份硬件采集。目标 TIDs 固定 E-core CPU21、capabilities 为零；P-core CPU9 仅满足 hybrid PMU 校验，空闲 P-core 派生指标不能当作工作负载结果。执行范围由真实 pre-clock _start:0 与 post-result profile_end:0 停点圈定。采集包含启用的 post-result 断点，可能存在同一 hint bucket 的保守检查；这些计数描述这一匹配配置，不能等同于无断点吞吐或把 profiler elapsed time 当作吞吐。

| 匹配长输入 | v59 Instructions Retired | v60 Instructions Retired | 退休指令下降 |
| --- | ---: | ---: | ---: |
| live-reference SIMD | 52,800,758,943 | 48,201,079,387 | 8.71% |
| memory64 | 46,056,645,491 | 46,055,523,752 | 0.00% |
| typed_call_ref | 133,939,517,153 | 133,049,908,176 | 0.66% |
| gc_struct | 106,293,837,083 | 99,441,847,272 | 6.45% |

主 long SIMD 未采样中位数为 2809.688529 → 2797.601039 ms，仅下降 0.43%（各 3 次）。退休指令下降没有转化为相同比例的吞吐改善，不能把计数收益描述成整体提速。实际 CPI、E-core Retiring / Front-End / Back-End / Store Bound 的前后值在 JSON 的 microarchitecture_core_summaries 中另列；份额是相对指标，缺少逐条命名 JIT 证据时不能推断绝对 store 成本或唯一热点。

三个额外长特性输入的未采样中位数（主产品，各 3 次）：

| 长输入 | v59 debug ms | v60 debug ms | Wasmtime Wasm/LLDB ms | v60 / Wasmtime |
| --- | ---: | ---: | ---: | ---: |
| gc_struct | 6757.446640 | 6728.614532 | 253.640658 | 26.53 |
| typed_call_ref | 8026.316447 | 8059.536451 | 271.377505 | 29.70 |
| memory64 | 2302.721738 | 2303.992895 | 936.910779 | 2.46 |

主 long SIMD 为 5,000 万次迭代、checksum 32389035495000，GC 分配与读取在每内核循环外。额外 gc_struct、typed_call_ref、memory64 控制各有单独 WAT、官方验证、opcode witness、hash 和独立 oracle，并做 3 次 UWVM 前后与实际 Wasmtime Wasm/LLDB 未采样系列。两类输入和全部长计时范围均保留。

额外采集编排首次使用 typed_call_ref 作为 guard job 名，guard 的字母数字/连字符规则在启动目标之前拒绝了下划线。失败 phase 保留，仅规范化 job 名；复用已完成的 4 份 memory64 采集，在新 phase 中执行剩余 8 份，没有重跑完整矩阵、放宽 guard 或改变生产代码。

Hotspots 是 driverless、无 stack 的实际 clock/retired 硬件采样；实际事件 period/count 和非零 function 行来自原始报告。退休指令与 top-down 份额来自独立 Microarchitecture summary 硬件计数，并未把稀疏 Hotspots 外推成精确总计数或逐条 JIT 归因。份额变化不能证明绝对成本变化。JIT 仍可能显示未知模块，尚无足够的命名指令归因。[Intel driverless CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html)。

typed_call_ref 的前后 Hotspots 实际包含 checkpoint_actual_frame、动态目标 resolver、activation_leave、shadow_ledger enter / packet validation 的非零 native 行，JSON 的 named_typed_call_hotspots 保留真实字段、事件 period 和样本数。它们指出尚未被引用复制改动消除的帧认证、目标解析、进入/退出和 packet 校验路径；300M period 的 clock 样本较粗，不能宣称已精确排序或逐条定位未知 JIT。

Advisor before: passed=false；attach/detach/normal exit 分别记录，实际函数行 0、survey 行 0。空或默认 JIT loop 归因不能视作有效 Survey，没有做 Trip Counts/FLOP。
Advisor after: passed=false；attach/detach/normal exit 分别记录，实际函数行 0、survey 行 0。空或默认 JIT loop 归因不能视作有效 Survey，没有做 Trip Counts/FLOP。

最终 host audit: collector 窗口新增告警 0，阶段新增受监控告警 1，全 boot 保留告警 3；本任务剩余进程 0。cgroup memory.events 原文保留；cgroup 累计 peak 为 19.13 GiB，不能把共享 cgroup peak 归因于单个任务。此前主机卡死根因未确定。原始 profiler 数据库留在 Linux，只回传小报告。

只读 Wasmtime revision 65abe37529a6bca836efcac1dcb920d5c7890580 的稳定 state slots / dirty operand / assigned local 思路指导了此次减少快照生成指令；其代码明确将 instrumentation slots 作为 GC roots，GC 会更新槽内的 canonical references。UWVM 尚未具有同样的引用槽契约，不能直接跳过旧引用刷新。剩余性能差距包括 frame/caller 管理、活跃引用完整 packet 和 trapping/访存操作。Wasmtime Wasm 插件也使用插桩，原生 GDB 是不同能力基线。[Wasmtime guest debugging](https://docs.wasmtime.dev/examples-debugging-guest.html)、[状态槽源码](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/cranelift/src/func_environ.rs)。

以下 policy 37 / v59 及更早内容保留为历史结果；当前以 policy 38 / v60 报告为准。

2026-10-12 当前为 observation policy 37 / resumable policy 17，完成 v59 小型混合 local 快照优化。uwvm2 和 uwvm2-ros 均在 SSH Linux 64 GiB、swap=0 cgroup 中构建（`xmake -j32`）、执行和采集。本机只读取源码、编辑、核对 hash 和整理小报告。完整结果见 [debug_core3_v59_performance_report.json](debug_core3_v59_performance_report.json)。

本阶段把不超过 8 个声明、完整 observer local prefix 中的 numeric 与已知 funcref/externref 改为直接复制，省去地址表及私有 copier 调用。numeric local 可以继续由 LLVM 提升到 SSA；reference local 保留初始化标记控制的存储读取。每次仍刷新全部 flag 和每槽 16 字节，不缓存旧引用，不改变 GC root 或恢复权限。真实 x86-64 reference carrier 是 16 字节，按实际 ABI extent 校验；缺失或非法 flag 在读取前选择私有不可变零存储。大 prefix、未知类型、legacy 和其他平台保留原 copier。

下面是同一输入、固定 CPU20、前后版本交替的 guest CLOCK_MONOTONIC 内核时间。Core 3 每组 3 次；live-reference SIMD 单列补充的 9 次系列。排除 JIT、启动、调试器连接和资格停点时间，保留全部原始范围、process/startup 时间和 checksum。Wasmtime 49.0.2 使用实际 LLDB 34 Wasm 插件，并以 `qWasmCallStack` 包日志验证；native DWARF/GDB 和普通执行在 JSON 中另列。

| 产品 / 内核 | v58 debug ms | v59 debug ms | Wasmtime Wasm/LLDB ms | 耗时下降 |
| --- | ---: | ---: | ---: | ---: |
| uwvm2 gc_struct | 1.825805 | 1.364738 | 0.070829 | 25.25% |
| uwvm2 gc_array | 2.064052 | 1.654262 | 0.084187 | 19.85% |
| uwvm2 typed_call_ref | 1.684949 | 1.618296 | 0.059955 | 3.96% |
| uwvm2 gc_recursive | 3.767230 | 3.307123 | 0.799242 | 12.21% |
| uwvm2 live-reference SIMD (9 次) | 0.685119 | 0.579821 | 0.066047 | 15.37% |
| ROS gc_struct | 1.831652 | 1.398242 | 0.071123 | 23.66% |
| ROS gc_array | 2.174221 | 1.675159 | 0.086828 | 22.95% |
| ROS typed_call_ref | 1.746854 | 1.660758 | 0.060155 | 4.93% |
| ROS gc_recursive | 3.886919 | 3.016616 | 0.769460 | 22.39% |
| ROS live-reference SIMD (9 次) | 0.695079 | 0.592432 | 0.065987 | 14.77% |

性能仍未整体达到 Wasmtime Wasm 调试或原生 GDB：live-reference SIMD 仍慢约 9 倍，GC struct/array 约 19–20 倍，typed call_ref 约 27 倍；ordinary 引擎代码生成本身也不同，不能把跨引擎差距都算成调试器开销。纯 numeric/SIMD 控制基本中性。初始 live-reference SIMD 5 次系列保留 3.444896 ms 的 after 离群值；另跑 9 次后，主产品 before 范围 0.675791–0.703451 ms、after 0.574579–0.585361 ms，两套系列都保留，没有剔除样本。小样本中位数不证明显著性或等效。

调用控制先做 9 次独立 none 系列，再做 3 次完整矩阵及额外 9 次 none/cold/disabled 系列。初始独立 pilot 接近中性；后两组存在明显双峰和宽范围，有的显示收益、有的显示回退。全部数据保留，不认定调用链整体变快，也不声称已证明所有用例无回退。仅凭这些波动不能推断主机频率、调度、限速或调试器中的具体原因。

两套产品共保留 2,578 个本阶段未采样样本，包括 1,254 个 Core 3、110 个纯 SIMD、110 个 live-reference SIMD、540 个传统计算/缓存/调用、180 个 pilot、6 个长输入资格计时，以及补充 54 个 live-reference SIMD 和 324 个调用控制。

28 个组件、38 组真实 PTY 交互、132 次深层帧读取、44 个 GC terminal trap、10 个 ASan/UBSan/LSan 可执行检查、12 项采集保护检查通过；另含 52 个 SIMD 实际停点及 24 个操作数 policy 用例。新增组件实际调用生产快照复制代码、LLVM promotion 和 MCJIT，检查 sNaN、v128、reference 高位 payload/tag、raw/public 表示、真正未初始化存储、flag 0/1/2、重新赋值与再次缺失，并核对每槽全部 16 字节。

首次候选错误地要求 reference 为 64 位，新组件立即拒绝，未启动吞吐或硬件采集；按真实 16 字节 ABI 修复后双方重新构建。ROS 旧 register pane 驱动在输入 startw 后只等 200 ms 静默，尚未收到 restart 结果便断言和结束进程；改为核对实际清空的命令行及光标恢复，并设置 60 秒 deadline，双方在新目录重跑 19 组交互通过。原失败记录都保留，不改写为成功。

每个已列 Core 3 类别有独立测试输入：20 个通过官方 parse/validate/print，19 个通过实际运行、断点、locals/operands、nw → ni → nw 和 trace 资格。deterministic profile 的 canonical NaN 在前后 ordinary/debug 都失败，且没有显式 profile selector；因此整体 Core 3 矩阵仍为失败，coverage_complete=false。未测试 macOS、Windows/QEMU 或 C++ module 构建。

VTune 2026.4 硬件 Hotspots 与 Microarchitecture summary 在同一长 live-reference SIMD 输入上分别采集前后版本：5,000 万次迭代，checksum 32389035495000。GC struct 分配及读取在每个内核循环外，循环内没有分配、调用或向量访存。真实 pre-clock _start:0 到 post-result profile_end:0 的停点圈定采集范围；采样时间不作为吞吐样本。

| 同一长 live-reference SIMD 输入 | before | after |
| --- | ---: | ---: |
| Instructions Retired | 59,456,629,343 | 52,801,483,273 |
| CPI | 0.225415 | 0.217840 |
| E-core Retiring | 88.6% | 91.8% |
| E-core Front-End Bound | 8.9% | 1.1% |
| E-core Back-End Bound | 2.4% | 6.9% |
| MUX Reliability | 0.999158 | 0.999261 |

退休指令数减少 11.19%。微架构百分比是相对份额，不能从 Back-End Bound 份额上升推断绝对成本上升。Hotspots 实际 event period 300,000,000，before/after retired 样本 197/176、clock 样本 29/26；JIT 仍显示未知模块，稀疏采样没有逐条命名指令归因。退休计数来自独立 summary，原始 event 行和结果数据库保留。

四份 VTune 采集通过实际 PID/TID、checksum、hash、正常退出及每次 kernel alert 检查。目标 TID 固定 E-core CPU20、capabilities 为零；collector 的 P-core CPU8 只用于 hybrid PMU 校验，P-core 计数为零。遵循 [Intel driverless CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html)。

Advisor 2026.0 before/after Survey 均成功 attach、圈定执行、核对 checksum、detach 和正常退出，但没有非零 measured self-time 行，均保留 passed=false；不能声称完成有效的 JIT loop/function 归因，没有做 Trip Counts/FLOP。继续需要实际 JIT metadata 集成，不能以拉长采集或空报告替代。[Intel collection controls](https://www.intel.com/content/www/us/en/docs/advisor/user-guide/2024-0/collection-controls.html)。

最终 audit：本阶段没有新增受监控 kernel alert，各 collector 窗口没有告警，所有本任务进程已结束，cgroup OOM/max 计数为零，继承峰值约 18.18 GiB。全 boot 仍保留更早的两条 perf 降采样告警（2026-10-12 01:51:08.865947 和 02:14:58.524256 +08:00），不能声称此前主机卡死根因已经解决。原始 profiler 数据库留在 SSH Linux，只回传小报告。

只读 Wasmtime revision 65abe37529a6bca836efcac1dcb920d5c7890580：Wasm guest debug 使用稳定 state slots、dirty operand/assigned local 更新及可修补 PreserveAll 调用；FrameTable 提供实际帧信息。Wasm 插件自身也有插桩。v59 只消除小混合 local 的 copier 并允许 numeric SSA，没有实现 UWVM 的 dirty-reference 重用或通用无插桩断点。剩余成本包括 per-call frame/caller 管理、活跃引用完整 packet、访存/陷阱观察；扩大 dirty slot 优化前必须另行证明 GC roots、native/debugger mutation 和 cold restore 正确性。[Wasmtime guest debugging](https://docs.wasmtime.dev/examples-debugging-guest.html)、[状态槽源码](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/cranelift/src/func_environ.rs)。

以下 policy 36 / v58 及更早结果保留为历史对照；当前以 policy 37 / v59 报告为准。

2026-10-12 当前为 observation policy 36 / resumable policy 17，完成 v57 深度 workspace 复用和 v58 SIMD 按需观察阶段。两套产品本机及 SSH Linux 源码已同步；所有构建、测试、Wasm 执行及采集仅在 SSH Linux 64 GiB、swap=0 cgroup 完成。本机只读取源码、编辑和整理小报告。双方完成 `xmake -j32`，补充的非 Linux 标注重建产物与完成回归及计时的冻结文件逐字节相同。完整数据见 [debug_core3_v58_performance_report.json](debug_core3_v58_performance_report.json)。

性能仍未整体达到 Wasmtime Wasm 调试或原生 GDB。SIMD 收益明显，乘加的本轮中位数低于 Wasmtime Wasm/LLDB，但 swizzle 和整数混合内核仍慢约 2.4 倍和 1.6 倍；这不构成所有用例性能相等的证明。

workspace 按真实调用深度复用存储，避免每次叶函数返回都 swap / 销毁记录。退休时清除活跃 extent 和 prefix，下一次 resize 生成完整零字节及未初始化标记；超出 16 KiB 的缓冲立即释放，最多保留 32 份 inactive payload（不含分配器及 metadata capacity）。80 层深度、64 层繁忙调用者、不同 prefix / 大小、非顶部或过期标签、溢出和移动后状态均有检查。保留的容量不能产生根、帧身份或恢复权限。

234 个明确非分配、非陷阱 SIMD 算子接入已有的实际 typed-site 观察需求门控。没有活跃 GC/extern 引用时，无请求路径省去完整 packet 和回调；有活跃引用时，仍先发布完整 packet，再跳过无请求回调。向量访存、保留 / 未知 / 截断 / 长编码、异常和未知引用保持保守路径。单步、trace、pause、catch 及受管理 shutdown 仍请求完整观察。改变观察策略号为 36，避免复用旧生成代码；resumable 策略仍为 17。

| 产品 / 内核 | v54c debug ms | v58 debug ms | Wasmtime Wasm/LLDB ms | 提速倍数 |
| --- | ---: | ---: | ---: | ---: |
| uwvm2 relaxed_madd | 1.416825 | 0.073951 | 0.078536 | 19.16 |
| uwvm2 relaxed_swizzle | 1.427531 | 0.094787 | 0.040123 | 15.06 |
| uwvm2 simd_integer_mix | 2.713894 | 0.113738 | 0.071460 | 23.86 |
| ROS relaxed_madd | 1.370563 | 0.073696 | 0.078303 | 18.60 |
| ROS relaxed_swizzle | 1.400961 | 0.097424 | 0.040330 | 14.38 |
| ROS simd_integer_mix | 2.675374 | 0.117091 | 0.071507 | 22.85 |

Core 3 计时每组 3 次，新增整数 SIMD 混合内核每组 5 次，前后版本交替、固定 CPU20、同一 Wasm 和独立 scalar checksum。WASI CLOCK_MONOTONIC 测量内核，排除 JIT、启动、连接和资格停止；保留 process 时间及全部范围。Wasmtime 49.0.2 使用真实 LLDB 34 Wasm 插件，包日志核对 `qWasmCallStack`；native DWARF/GDB 和普通执行另列，不能把引擎代码生成差距全部算作调试开销。小样本中位数不证明显著性或等效。

| 产品 / 调用内核 | before ms | after ms | native GNU GDB ms |
| --- | ---: | ---: | ---: |
| uwvm2 calls | 29.186591 | 28.724533 | 0.386982 |
| uwvm2 callchain | 64.379037 | 63.364077 | 0.421599 |
| ROS calls | 28.762402 | 28.415744 | — |
| ROS callchain | 64.541625 | 63.902258 | — |

直接调用及调用链的收益较小，类型化调用、GC、memory64 和多内存仍有明显差距。两套产品共保留 2,264 个本轮未采样样本：1,254 个 Core 3、110 个新 SIMD、540 个传统计算/缓存/调用样本，以及两个隔离阶段各 180 个 pilot 样本。pilot 各重复 9 次，v58 与 v57 的非 SIMD 调用控制变化约 0–0.5%，并未改善所有用例。

26 个组件、38 组真实 PTY 交互、132 次深层帧读取、52 个新增 SIMD 实际停点用例、额外 24 个操作数 policy 用例、44 个 GC terminal trap、10 个 ASan/UBSan/LSan 可执行检查及 12 项采集保护检查通过。分类组件对实际 parser 的 SIMD enum 和全部 32,768 个两字节 subopcode 值检查白名单，排除向量访存和保留洞。额外操作数回归检查所有 20 个 relaxed SIMD 算子的确定值、分支、循环和多返回值调用。旧脚本仍等待 `guest exited: 0`，尽管所有观察正确且 VM 正常退出，原失败记录保留；改为核对当前 `Program exited normally.` 后双方在新目录复跑通过。

每个 Core 3 类别都有独立输入，20 个输入通过官方 parse/validate/print；19 个通过实际运行、断点、locals/operands、`nw → ni → nw` 和 trace 资格。确定性 profile canonical NaN 在前后版本 ordinary/debug 都失败，而且没有显式 profile selector，因此整个 Core 3 矩阵继续保留失败和 `coverage_complete=false`。本阶段没有测试 macOS、Windows/QEMU 或 C++ module 构建。

VTune 硬件 Hotspots 与独立 Microarchitecture summary 使用同一份带初始化 GC struct local 的 SIMD 文件：每内核 10,000 次、重复 5,000 次，共 5,000 万次迭代，checksum 32389035495000。GC 分配及 field 读取各在每个内核循环外发生一次；循环内没有访存、调用或分配。它与上表 pure SIMD 短输入不同，不能将 profiler 耗时当作上表吞吐。

| 同一带 GC SIMD 输入 | before | after |
| --- | ---: | ---: |
| Instructions Retired | 131,218,826,831 | 59,456,784,852 |
| CPI | 0.270360 | 0.226591 |
| E-core Retiring | 76.0% | 88.2% |
| E-core Front-End Bound | 5.1% | 9.2% |
| E-core Back-End Bound | 18.8% | 2.4% |
| MUX Reliability | 0.999316 | 0.999269 |

退休指令减少 54.69%。Hotspots 中 before 可见 safepoint / controller 回调；after 的非零 clock 样本全部落在未知 JIT 模块，不能将未采样到的命名函数解释为零成本，也不能声称已逐指令归因 JIT 代码。百分比是相对份额，Front-End Bound 份额升高不证明绝对成本升高。采样报告实际 period 为 300,000,000；before / after clock 样本 80 / 28、retired 样本 437 / 198，完整 raw 数据保留远端。summary 退休计数用于上述精确计数对比，稀疏 Hotspots 不替代它。

两次 Hotspots、两次 summary 均通过真实 pre-clock `_start:0` / post-result `profile_end:0` 停点、checksum、实际 PID / TID / capability、正常退出及 immutable hash 检查。目标所有 TID 位于 E-core CPU20、capabilities 为零；采集器额外包含已核对 P-core CPU8 以满足 hybrid PMU 校验，P-core 计数为零，忽略其空闲派生指标。遵循 [Intel driverless CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html)。

Advisor 2026.0 使用 Survey、no-stacks、30 秒上限和真实目标 attach，pre-clock 到 post-result 范围后对真实 `e000/hs000` 目录 detach，guest 正常退出。before 有实际 loop/function 行；after 默认循环报告和单独离线函数导出均为空，保持未通过，原失败收据没有重写。两次采集的 attach、checksum、detach 和退出成功并不证明有效归属分析。私有 JIT 的名称/循环元数据缺失是限制，没有通过延长采样或虚构行来补足，也没有声称做过 Trip Counts/FLOP。[Intel Advisor collection controls](https://www.intel.com/content/www/us/en/docs/advisor/user-guide/2024-0/collection-controls.html)。

最终全 boot 审计保留两条本轮 perf 降采样告警，均早于硬件采集：2026-10-12 01:51:08.865947 和 02:14:58.524256 +08:00。它们与所有本轮 profiler guard 窗口没有交集；不能宣称整轮主机无告警、推断告警来源或确认此前卡死根因已解决。所有本任务构建/测试/采集进程已结束，cgroup OOM/max 计数为零，峰值约 18.18 GiB。整轮无告警的审计断言保持失败；各独立采集窗口检查通过。

只读 Wasmtime 源码 revision `65abe37529a6bca836efcac1dcb920d5c7890580` 中，guest debug 使用稳定 state slots、仅刷新 dirty operands / assigned locals、FrameTable 及 PreserveAll 可修补调用；开启断点时编辑私有 StoreCode、重新发布可执行权限。Wasmtime 的 Wasm 插件也使用插桩，native GDB 是不同能力基线。UWVM 本轮扩展按需 SIMD 观察及存储复用，没有新增通用无插桩断点后端。下一项剩余热点是实际 frame / caller 管理及活跃引用所需的完整状态更新；参考 Wasmtime 的 dirty slots 时必须另行证明 GC root 和 cold capture 正确性。[Wasmtime guest 调试](https://docs.wasmtime.dev/examples-debugging-guest.html)、[状态槽与断点源码](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/cranelift/src/func_environ.rs)。

以下 policy 34 / v54c 及更早结果保留为历史对照；当前阶段以上面的 policy 36 / v58 报告为准。

2026-10-11 当前实现为 observation policy 34，resumable policy 仍为 17。uwvm2 和 uwvm2-ros 均已同步本机和 SSH Linux；本机只读源码、编辑及整理结果，没有编译或执行 Wasm。两套产品在 64 GiB、swap=0 cgroup 内完成 `xmake -j32`。随后补上的非 Linux 后端 `maybe_unused` 标注又完成双方构建，产物 SHA-256 与已回归、计时和采集的可执行文件完全相同。

2026-10-12 完成 v54c 不可变元数据复用阶段，完整数据在 [debug_core3_v54_performance_report.json](debug_core3_v54_performance_report.json)。性能仍未达到 Wasmtime Wasm 调试器，也未达到原生 GDB 无命中执行的水平。以下是这一阶段的结果，后面的 policy 34 / v52 和 policy 30 内容保留为历史对照。

进入调用时同步借用已经校验的执行 scope 元数据，复用相同计划地址和 shared_ptr 控制块，避免每次复制强引用。最多保留 32 份、每份不超过 16 KiB 的不可变元数据载荷（不含分配器开销）。每次仍检查当前 publication、profile、入口、epoch、generation 及完整的等待调用者 packet；返回时清除真实身份、值、引用、位置和 native storage。缓存不能产生根、代码租约或恢复权限，generic/resumable 路径维持原有 move 和立即释放行为。生成策略仍为 observe 34 / resumable 17，没有修改 JIT 生成协议。

两套产品的 Linux 64 GiB、swap=0 cgroup 构建及回归已通过：24 个组件、36 组真实 PTY 交互、132 次深层帧读取、44 个 GC terminal trap 用例、6 个 ASan/UBSan/LSan 可执行测试、12 个采样保护检查。每个 Core 3 类别都有独立输入：20 个官方 parse/validate/print 输入、19 个运行资格通过；确定性 profile canonical NaN 探针在前后版本的 ordinary/debug 模式仍失败，整个 Core 3 矩阵保留失败状态。两套产品共保存 1,794 个未采样计时样本。撤回试验后两套产品再次构建成功，SHA-256 与已完成回归及采样的冻结版本逐字节一致。macOS、Windows/QEMU 和 C++ module 构建本阶段未测试。

| 用例 | UWVM before ms | UWVM after ms | 参照 ms |
| --- | ---: | ---: | ---: |
| uwvm2 直接调用 | 30.931757 | 28.954508 | native GDB 0.386933 |
| ROS 直接调用 | 30.545578 | 29.010830 | — |
| uwvm2 调用链 | 65.960471 | 64.291665 | native GDB 0.421609 |
| ROS 调用链 | 64.094723 | 64.575835 | — |
| uwvm2 typed_call_ref | 1.756992 | 1.688790 | Wasmtime Wasm/LLDB 0.059631 |
| uwvm2 nondefaultable_local | 1.788725 | 1.694761 | Wasmtime Wasm/LLDB 0.057663 |
| uwvm2 gc_struct | 1.818449 | 1.818117 | Wasmtime Wasm/LLDB 0.068977 |
| uwvm2 memory64 | 0.128701 | 0.129006 | Wasmtime Wasm/LLDB 0.063403 |

直接调用的主产品 / ROS 中位数分别降低约 6.39% / 5.02%；调用链没有一致收益，部分用例变慢。固定 CPU20、相同 Wasm、独立 checksum、前后交替三轮；中位数不是显著性或相等性能证明。native GDB 是另一种能力基线，各引擎的 ordinary 性能分别保留，不能把所有跨引擎差距归因于调试器。

同一 3,000 万次直接调用输入，VTune driverless Hotspots 和独立 Microarchitecture summary 硬件计数已通过真实 pre-clock / post-result 断点资格。退休指令 49,502,177,756 → 49,949,923,289，增加约 0.90%；CPI 0.248724 → 0.235433。这项优化没有减少执行指令数量。目标所有 TID 仅在 E-core CPU20，采集范围另含核对过的 P-core CPU8 来满足 hybrid PMU 校验，P-core 计数为零；忽略其派生百分比。主要命名热点仍为 actual-frame 校验、activation 进入/退出、workspace 管理和 caller packet。未知模块 JIT 样本没有逐指令归属。

Linux Advisor 2026.0 已做前后 Survey：实际附加、真实结果断点、checksum、detach、正常退出均通过，loop/function 原始 CSV 保留。Advisor Survey 是辅助分析，未做 Trip Counts/FLOP，不能当作硬件计数或未采样耗时。函数报告中 shared_ptr 赋值热点减弱，frame 校验及 workspace 仍热；仅约 0.03–0.04 秒的可矢量化 workspace 退出循环不能解释整体差距。[Intel Advisor attach / detach](https://www.intel.com/content/www/us/en/docs/advisor/user-guide/2024-2/target-pid.html)、[JIT Trip Counts 选项](https://www.intel.com/content/www/us/en/docs/advisor/user-guide/2024-0/profile-jit.html)。

Wasmtime 只读源码的稳定 state slot、dirty operand/local 更新、FrameTable/栈形元数据和 PreserveAll 可修补断点仍是架构参照。UWVM 当前改动仅降低了元数据强引用和部分退休成本，没有实现通用无插桩断点或冷 native-frame 重建；真实停止状态、GC root、native continuation 的验证仍须保留。[Wasmtime guest 调试](https://docs.wasmtime.dev/examples-debugging-guest.html)、[状态槽与断点源码](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/cranelift/src/func_environ.rs)。

旧 boot 的第四次计数在离线报告生成阶段被正常 systemd 重启打断，原 passed=false 收据保持不变；新 boot 重新核对 cgroup 后以新目录完成前后计数。采集窗口无新 perf/NMI/OOM 告警，不能据此确认原卡死问题已解决。v53 entry/workspace ABI 合并没有一致收益，已精确撤回；v56 强制内联触发 Clang 23 编译进程信号异常，未进入运行资格检查，已精确撤回，诊断文件保留 SSH Linux。完整 raw VTune/Advisor data/db 仅保留远端，本机只读取小报告；没有本机编译或 Wasm 执行。


仍未达到 Wasmtime Wasm 调试或原生 GDB 的执行效率。当前改善主要集中于引用和 GC 热路径：不命中断点时省去逐指令解码与控制器调用，活跃 GC/extern 值仍先更新完整类型化状态，再决定是否进入观察回调。非 GC 函数引用需要真实固定模块记录及链接签名匹配；GC 字段、数组元素及长度访问只在明确的非分配指令白名单内省去回调。分配、批量操作、未知前缀／类型、异常引用保持完整路径。真实停止、根扫描和恢复权限仍分别验证。

20 个独立 Core 3 输入通过官方 parse/validate/print；19 个通过普通执行和实际断点、locals/operands、`nw → ni → nw`、trace 检查。确定性 profile 的 canonical NaN 检查在前后版本普通／调试模式均失败，且 UWVM 缺少显式 profile 选择器，因此完整矩阵保持失败、`coverage_complete=false`。两产品保留 1,254 个 Core 3 及 540 个计算／缓存／调用内核计时样本。每组重复三次，前后版本交替、固定 CPU20、同一 Wasm 与独立 checksum；中位数不证明统计显著性或性能相等。

新增及复跑的 36 组真实 PTY 交互、132 次深层帧观察、2 个缓存策略身份组件、44 次 GC 空引用／越界终止停止、12 项采集保护检查均通过。GC 回归覆盖全部 9 类字段／元素／长度访问，检查实际 GC opcode、当前字段值、未初始化局部、ignore/condition/disable/delete/restart，以及终止后的状态保留和拒绝执行权限。之前 v50b 的 22 个组件及 ASan/UBSan/LSan 引用单元检查保留为前阶段证据，没有冒称在 v52 重跑。模块构建、macOS、Windows/QEMU 本阶段未测试。

| uwvm2 用例 | policy 30 debug ms | policy 34 debug ms | Wasmtime Wasm/LLDB ms | 提速倍数 |
| --- | ---: | ---: | ---: | ---: |
| annotations | 0.057633 | 0.058159 | 0.029686 | 0.99 |
| exception_payload | 141.862314 | 145.417785 | 195.712207 | 0.98 |
| exception_rethrow_ref | 222.864062 | 226.761526 | 382.944146 | 0.98 |
| extended_const | 0.059374 | 0.058644 | 0.030701 | 1.01 |
| gc_allocation | 15.904425 | 2.876889 | 0.136608 | 5.53 |
| gc_array | 16.430077 | 2.073743 | 0.086682 | 7.92 |
| gc_i31 | 1.037937 | 0.977886 | 0.032099 | 1.06 |
| gc_recursive | 16.291581 | 3.171996 | 0.772210 | 5.14 |
| gc_struct | 14.537641 | 1.828863 | 0.069024 | 7.95 |
| memory64 | 0.124438 | 0.127944 | 0.064574 | 0.97 |
| multi_memory | 0.709840 | 0.755983 | 0.081459 | 0.94 |
| nondefaultable_local | 11.711476 | 1.789413 | 0.057142 | 6.54 |
| relaxed_madd | 1.305063 | 1.384018 | 0.078534 | 0.94 |
| relaxed_swizzle | 1.309136 | 1.404578 | 0.039102 | 0.93 |
| table64 | 1.819066 | 1.878471 | 0.065252 | 0.97 |
| tail_direct | 1.236582 | 1.287608 | 0.036741 | 0.96 |
| tail_indirect | 1.620727 | 1.657799 | 0.055121 | 0.98 |
| tail_ref | 2.016524 | 1.309328 | 0.502026 | 1.54 |
| typed_call_ref | 12.397678 | 1.762515 | 0.059914 | 7.03 |

ROS 类型化引用调用为 12.323342 → 1.799975 ms，GC struct 为 14.398385 → 1.823258 ms，GC array 为 16.288744 → 2.131286 ms。不同引擎的普通执行基线差异也保留在 [当前完整报告](debug_core3_v52_performance_report.json)，不能把全部跨引擎差距解释为调试开销。

数值 calls 主产品为 29.929468 → 30.805376 ms，callchain 为 63.360766 → 64.311949 ms，未改善；ROS 为 29.205959 → 30.329033 ms、62.109225 → 64.184490 ms。原生 calls 的 GNU GDB 中位数约 0.387 ms、普通执行约 0.404 ms；UWVM 相应用例普通执行约 0.440 ms、debug 约 30.805 ms。该原生参照使用独立 native 程序，能力和代码生成器并不相同。三次小样本不能证明回退显著性，但必须保留这些未改善结果。

硬件采集遵循 Intel driverless CLI，Hotspots 使用无栈的双事件采样，Microarchitecture 当前使用独立的 summary 硬件计数。目标所有 TID 均限制 CPU20，采集器仅额外包含已核对的 P-core CPU8 来满足 hybrid PMU 校验，P-core 样本／计数为零。目标的 inheritable/permitted/effective/ambient capabilities 均为零。真实 pre-clock 断点后开始采集，结果输出后的 `profile_end` 断点处停止，排除编译、启动和退出回收；profiled 时间不作吞吐样本。

同一 memory64 输入、40,000 次内核调用：UWVM policy 33 退休指令 92,118,750,327，Wasmtime 36,847,374,203；CPI 分别 0.204825／0.208107，Retiring 97.7%／96.1%。差距主要来自执行指令更多。此组是 policy 33 对照，不能标成 policy 34。GC struct 的同一输入、1,260 次调用：policy 33 39,421,995,523 → policy 34 30,254,913,389 条退休指令，减少 23.25%；CPI 0.309531 → 0.309079，MUX reliability 均约 0.999。无命中回调减少与指令数下降相符。新 Hotspots 仍能看到 GC host-scope 进入／退出和对象访问，类型化调用仍有 activation/frame 身份及目标解析开销。大量 JIT 样本没有命名归属，不能声称已逐条归因未知 JIT 指令。

Wasmtime 真实 Wasm 调试使用 LLDB Wasm 插件；native DWARF/GDB 另作参照。[Wasmtime 官方 guest 调试](https://docs.wasmtime.dev/examples-debugging-guest.html)。只读本机源码中，`func_environ.rs` 的 state slot、栈形元数据及可修补断点说明了减少热路径工作的方法；本机源码 revision 与测试用 release 49.0.2 不同，性能结论以真实会话和采集数据为准。Launch Application 的 `status` 在当前 VTune 硬件采集返回成功却无状态确认，原失败收据保留；附加流程省去无法通过断点资格检查的 `--profile=vtune`，在真实 Wasm 停止点验证 PC、插件包、checksum 和目标身份后采集。其 private StoreCode 没有命名 kernel 采样行，因此整段计数合格、命名 JIT hotspot 归属不合格；没有用启动参数冒充命中证明。

21:17 的 detailed Microarchitecture 内存采集触发 perf 降采样告警，保护逻辑已中止并拒绝其结果。22:11:07 主机又有 perf 降采样告警，当时在 ROS 未采样计时阶段，不与本任务任何 perfmon 守护窗口重叠。当前已结束所有采集，cgroup OOM/max 事件均为零；不能由这些记录确认原卡死根因。当前各合格采集的独立监控窗口没有新告警，这不代表整次 boot 无告警。短窗口、多路复用及稀疏 Hotspots 样本限制精度。[Intel driverless 说明](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/profiling-hardware-without-sampling-drivers.html)、[Linux perf 降采样控制](https://docs.kernel.org/admin-guide/sysctl/kernel.html#perf-cpu-time-max-percent)。

当前代码、构建、回归、未采样计时和独立硬件窗口的 SHA-256／收据路径全部在当前 JSON。完整 raw VTune data/db 仅保留 SSH Linux，已递归 fsync；本机只取小报告。下一步需针对调用帧身份、host-scope 和仍然完整更新的状态槽设计冷路径读取；不能为追求性能省去根验证、真实状态或 native continuation 校验。

以下为 policy 30 的历史记录及输入生成说明；当前结果和限制以上述 policy 34 报告为准。

2026-10-11 已在 SSH Linux 的 64 GiB、swap=0 cgroup 中完成 policy 30 的两套产品构建、回归和配对计时。本机没有编译或执行 Wasm。`xmake -j32` 构建成功，完成 20 个组件测试、30 组真实 PTY 调试交互、132 次深层帧读取及 ASan/UBSan/LSan 检查。模块构建、macOS、Windows/QEMU 本阶段未测试。

当前仍未达到 Wasmtime Wasm 调试性能。20 个 Core 3 输入均通过官方 wasm-tools parse/validate/print；19 个通过普通执行及真实断点、locals/operands、`nw → ni → nw` 和 trace 检查。UWVM 没有显式确定性 profile 选择器，canonical NaN 探针失败，因此整个 Core 3 矩阵保持失败，不能称为完全覆盖。两套产品共保留 1,254 个 Core 3 和 540 个传统计算/缓存/调用内核计时样本。每组重复三次、固定 CPU20、前后交替，使用同一 Wasm 和独立 checksum；下面是中位数，不是统计显著性或性能相等证明。

policy 30 将通过完整验证的函数元数据缓存在当前执行 scope 中。缓存同时绑定真实记录、publication、epoch、module/function/generation、原始及 typed 入口，命中还需要相同的元数据地址与 shared_ptr 控制块。只在生成代码的进入路径复用；实际停止后的捕获仍重新验证源码及发布身份。scope 结束清除 TLS 借用，更新、重置及 generation 改变不能继承命中。另将 observer workspace 释放及 activation 退出合成一次 ABI 调用，保持原先释放次序。恢复策略仍为 17。

uwvm2 的数值调用内核从 33.247797 ms 降到 29.671004 ms，调用链从 71.187604 ms 降到 61.810296 ms。ROS 对应为 33.204925 → 29.117856 ms、70.291913 → 61.630013 ms。GC struct 的主产品中位数降低约 40.94%，但 ROS 同一用例仅降低约 0.39%；不能将主产品结果推广为两套产品的普遍提升。类型化引用调用仍比 Wasmtime Wasm/LLDB 慢约 213 倍，剩余工作尚未完成。

| uwvm2 用例 | policy 29 debug ms | policy 30 debug ms | Wasmtime Wasm/LLDB ms | UWVM ordinary ms | Wasmtime ordinary ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| annotations | 0.060304 | 0.057954 | 0.030740 | 0.000179 | 0.010341 |
| exception_payload | 143.719346 | 141.876540 | 189.654680 | 91.080128 | 2.282025 |
| exception_rethrow_ref | 245.240727 | 235.910508 | 602.881242 | 139.549592 | 7.253521 |
| extended_const | 0.060764 | 0.060686 | 0.031415 | 0.000298 | 0.010199 |
| gc_allocation | 16.958458 | 16.748606 | 0.148265 | 1.113225 | 0.088456 |
| gc_array | 18.226380 | 18.341363 | 0.086501 | 0.255590 | 0.047213 |
| gc_i31 | 1.040148 | 1.054465 | 0.032513 | 0.000885 | 0.010022 |
| gc_recursive | 16.544097 | 16.849629 | 0.812199 | 1.052763 | 0.749837 |
| gc_struct | 26.020442 | 15.367275 | 0.073567 | 0.205600 | 0.025550 |
| memory64 | 0.126525 | 0.127231 | 0.064268 | 0.015510 | 0.027615 |
| multi_memory | 0.711376 | 0.712328 | 0.082094 | 0.063090 | 0.027873 |
| nondefaultable_local | 11.943717 | 11.679387 | 0.057007 | 0.252928 | 0.019172 |
| relaxed_madd | 1.847642 | 1.308861 | 0.087158 | 0.015009 | 0.021135 |
| relaxed_swizzle | 1.411954 | 1.320458 | 0.042086 | 0.002749 | 0.010318 |
| table64 | 1.956465 | 1.825336 | 0.065625 | 0.017376 | 0.026509 |
| tail_direct | 1.350338 | 1.238063 | 0.037264 | 0.000275 | 0.015870 |
| tail_indirect | 1.767537 | 1.609654 | 0.056526 | 0.024745 | 0.048856 |
| tail_ref | 2.188023 | 2.001706 | 0.516954 | 0.000178 | 0.482730 |
| typed_call_ref | 12.605033 | 12.673183 | 0.059613 | 0.244603 | 0.019684 |

不同引擎有自己的普通执行基线：普通优化器可能消去常量或无副作用的计算，不能把全部跨引擎差距解释为调试开销。异常 payload 用例尤其如此：UWVM ordinary 约 90.71 ms、debug 约 141.88 ms，而 Wasmtime ordinary 约 2.34 ms、Wasm/LLDB 约 189.65 ms。原生 GNU GDB 路径独立记录，不能当作 Wasm 插件的等价能力参照。完整两套产品的中位数、原始样本哈希和回归状态在 `debug_core3_v48_performance_report.json`；找回的原始 JSON 位于 `build/debug-snapshot-perf-evidence/reboot-20261011/`。

19:41:50 的上一启动内核日志记录了 bad kernel stack frame pointer，展开栈中包含 NVIDIA timer callback，随后 `perf_event_nmi_handler` 报 2.254 ms，perf 将最大采样率降到 10000。没有找到 OOM kill；已经完成的守护收据也没有 OOM，但中断收据不能证明中断时的内存状态。pstore 为空，没有可据以确定根因的 panic 转储。记录中的 uwvm 进程名只表明当时的被采样上下文，不能据此确定 uwvm 或 NVIDIA 谁造成卡死。

Linux 官方文档明确指出，过长的 perf NMI 可能堆积并阻止其他工作执行。cgroup 限制用户态资源，不能隔离内核 PMU/NMI 路径。[Linux perf CPU 时间说明](https://docs.kernel.org/admin-guide/sysctl/kernel.html#perf-cpu-time-max-percent)。目前根因未确认，整机卡死不能称为已修复。

最后一批类型化引用 Hotspots 的 `receipt.json`、summary、function report 和 transcript 在重启后是零字节，按中断处理；没有将守护 exit0 代替完整采样证据。其后详细 Microarchitecture 矩阵未取得结果。本次重启后没有启动 PMU 采集，也没有改系统 sysctl、内核或显卡驱动。

旧配置只固定目标线程 CPU，没有设置 VTune 采样范围；Intel 文档说明默认采集所有 CPU。新脚本明确设置 `-cpu-mask 20`，禁止栈采集，采用 10 ms 间隔及 30 秒上限，以共享文件锁拒绝并发采样。它从当前 boot 的实际 journal cursor 检查新增 NMI、perf、栈指针、lockup、RCU 及 OOM 告警，告警或日志不可读即拒绝/终止自身采集。只处理当前调用拥有的子进程；这种保护不能恢复已完全卡死的内核。[Intel 降低采集开销](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-1/minimizing-collection-overhead.html)、[Intel CLI knobs](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/knob.html)、[Intel CPU mask](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-1/cpu-mask.html)。

收据以临时文件写入、fsync、原子替换及目录 fsync 发布；最终 raw report 文件也在 passed 收据前同步。重启后实际 journal/互斥锁、六类告警回放、日志读取失败及原子发布失败均通过 cgroup 内检查，测试没有启动 PMU。后续两套产品各重新资格检查 19 个 Core 3 执行输入，无新增失败、OOM 或内核采样告警；确定性 profile 仍是已知缺口。新增告警回放文件明确标为 synthetic，不是主机真实报错。

源码阅读使用本机 Wasmtime commit `65abe37529a6bca836efcac1dcb920d5c7890580`（2026-10-07），冻结执行参照是 v49.0.2；两者不是同一构建来源。Wasmtime Wasm 调试使用 LLDB 的 Wasm 插件与 GDB remote protocol，其稳定状态槽、dirty local/operand 更新、FrameTable 及可替换 PreserveAll 断点是后续优化参照。UWVM 当前已实现数值槽冷解码和执行 scope 元数据复用，但引用槽解码、工作区与 activation 热路径仍需完整的硬件定位和进一步优化。[Wasmtime guest debugging](https://docs.wasmtime.dev/examples-debugging-guest.html)、[状态槽和断点生成](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/cranelift/src/func_environ.rs)、[调试补丁发布](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/wasmtime/src/runtime/debug.rs)。

Wasmtime 的 EngineCode profiling 名称登记与 Store 私有执行副本的地址可能不同，这是本机源码推断，尚需冻结版本的实际样本验证。硬件捕获有效性与 JIT 函数归属分别记录；没有真实 kernel 样本行就不能称为有名 Wasm 热点。[代码所有权](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/wasmtime/src/runtime/code.rs)、[VTune agent](https://github.com/bytecodealliance/wasmtime/blob/65abe37529a6bca836efcac1dcb920d5c7890580/crates/wasmtime/src/profiling_agent/vtune.rs)。

测试目录为 `fixtures/debug_core3_performance`。类别按 [Core 3 官方变更表](https://webassembly.github.io/spec/core/appendix/changes.html#release-3-0) 核对；每个文件是完整独立模块，校验值由标量算术公式计算。

| 类别 | 独立输入 |
| --- | --- |
| 扩展常量表达式 | `extended_const.wat` |
| 尾调用 | `tail_direct.wat`、`tail_indirect.wat`、`tail_ref.wat` |
| 异常处理 | `exception_payload.wat`、`exception_rethrow_ref.wat` |
| 多内存 | `multi_memory.wat` |
| 64 位地址空间 | `memory64.wat`、`table64.wat` |
| 类型化引用 | `typed_call_ref.wat`、`nondefaultable_local.wat` |
| GC | `gc_struct.wat`、`gc_allocation.wat`、`gc_array.wat`、`gc_i31.wat`、`gc_recursive.wat` |
| Relaxed SIMD | `relaxed_swizzle.wat`、`relaxed_madd.wat` |
| 确定性 profile | `deterministic_profile.wat` |
| 文本注解 | `annotations.wat` |

memory64 的计时与输出使用单独的 WASI32 内存，因此同时依赖 multi-memory。扩展常量初始化发生在内核时钟之前；其内核测量全局读取，完整进程时间也记录初始化成本。注解输入验证文本到自定义 section 的传输，其计时不能解释为“注解指令”的执行成本。确定性 profile 输入检查 canonical NaN 和 relaxed swizzle 的确定性选择：Wasmtime 显式开启相应选项；本次阅读未找到 UWVM 的等价 profile 开关，脚本保留这个能力缺口，有限探针即使通过也不能证明完整 profile 一致性。

`make_debug_core3_feature_workloads.py` 的默认模式只写 WAT。`--compile` 会先检查实际 Linux cgroup 成员资格、内存、swap 和已核对的 CPU 集，然后运行 wasm-tools parse/validate/print，逐个保存失败信息和超时之前的日志。`run_debug_core3_feature_performance.py` 再执行独立 checksum 校验、kernel 断点、locals/operand 读取、`nw → ni → nw`、完整 trace；指定 baseline 时，前版本也需要通过普通执行及实际调试状态检查。可见停止与 trace 的资格检查不计入吞吐样本。

吞吐样本分别包括 UWVM ordinary/no-hit/cold/disabled、Wasmtime ordinary/native GDB/Wasm LLDB。前后版本按重复轮次交替，同一输入保留所有样本、命令、日志、输入与可执行文件哈希。内核时间由 WASI CLOCK_MONOTONIC 获取；JIT、启动与连接时间单独记录。每个引擎都有自己的普通执行基线，不能把不同代码生成器的差异全部归因于调试器。脚本记录各组样本数、范围及矩阵完整性；5% median band 仅记录为 `median_band_matched`，在单独完成方差评估前，`parity_achieved` 始终为 false。

以下命令必须由原有 SSH Linux 64 GiB、swap=0 守护脚本启动。设置 `UWVM_TEST_CPUSET` 为核对过的实际 CPU 集；串行计时子进程固定 CPU20。`--source-root` 必须指向与当前可执行文件构建收据对应的仓库。以下路径与可执行文件是参数示意，输出目录必须全新。

```sh
python3 test/0017.runtime/make_debug_core3_feature_workloads.py \
  --out /path/to/new-core3-inputs --iterations 10000 \
  --compile --wasm-tools /path/to/wasm-tools

python3 test/0017.runtime/run_debug_core3_feature_performance.py \
  --source-root /path/to/uwvm2 --uwvm /path/to/candidate-uwvm \
  --baseline /path/to/frozen-before-uwvm \
  --wasmtime /path/to/frozen-wasmtime --lldb /path/to/wasm-lldb \
  --fixtures /path/to/new-core3-inputs --out /path/to/new-core3-results \
  --cpu 20 --repeats 5
```

硬件采样使用独立的长内核输入，可用 `--kernel-repeats` 延长真实 guest 执行，校验值累加每次结果。先根据两边普通计时选择重复量，确保较快的 Wasmtime 内核也超过 1 秒。Hotspots 与 Microarchitecture 分开采集，采样期间不运行构建或其他自有基准。

比较采样窗口时，用 `--profile-gates` 生成长输入，并对 UWVM 采样启用 `--post-kernel-gate`。这会在结果输出后调用独立的 `profile_end`，两边都在那里设置真实断点。UWVM 从 `_start:0` 停止点开始采集，到该断点结束；Wasmtime 通过真正的 Wasm/LLDB 插件在 `start` 停止点核对 PC，恢复硬件采集，再于 `profile_end` 暂停采集，正常退出后完成报告。退出及运行时回收发生在暂停期间，避免把 UWVM 保留控制台与 Wasmtime 立即回收的差异混进硬件对照。该门控调用在内核时钟之后，不更改计算或 checksum；普通短输入保持原样。两边必须使用同一份门控 Wasm，并保留哈希、插件包日志、目标 PID、实际 cgroup 与所有 TID 的 affinity。

内核低于 1 秒不作合格采样，PMU 模式必须有有效的退休指令计数。profiled wall time 不能替代普通吞吐测量。以下 `--kernel-repeats` 是示例，必须依据真实 pilot 计时调整。

```sh
python3 test/0017.runtime/make_debug_core3_feature_workloads.py \
  --out /path/to/new-long-core3-inputs --iterations 10000 \
  --kernel-repeats 10000 --profile-gates \
  --compile --wasm-tools /path/to/wasm-tools

python3 test/0017.runtime/run_debug_core3_vtune.py \
  --uwvm /path/to/candidate-uwvm --wasm /path/to/long-core3-case.wasm \
  --out /path/to/new-hotspots --expected-cgroup /actual/verified/cgroup \
  --cpu 20 --expected-checksum INTEGER_FROM_MANIFEST \
  --feature FEATURE_FROM_MANIFEST --post-kernel-gate --kind hotspots \
  --hybrid-pmu-companion 8 --driverless-hotspots --drop-target-capabilities

python3 test/0017.runtime/run_debug_core3_vtune.py \
  --uwvm /path/to/candidate-uwvm --wasm /path/to/long-core3-case.wasm \
  --out /path/to/new-microarch --expected-cgroup /actual/verified/cgroup \
  --cpu 20 --expected-checksum INTEGER_FROM_MANIFEST \
  --feature FEATURE_FROM_MANIFEST --post-kernel-gate --kind uarch-exploration \
  --hybrid-pmu-companion 8 --pmu-mode summary --drop-target-capabilities

python3 test/0017.runtime/run_debug_core3_wasmtime_attach_vtune.py \
  --source-root /path/to/uwvm2 --wasmtime /path/to/frozen-wasmtime \
  --lldb /path/to/wasm-lldb --fixtures /path/to/new-long-core3-inputs \
  --case CASE_FROM_MANIFEST --out /path/to/new-wasmtime-hotspots \
  --expected-cgroup /actual/verified/cgroup --cpu 20 --hybrid-pmu-companion 8 \
  --kind hotspots --driverless-hotspots --drop-target-capabilities
```

采用 Intel 官方 CLI：`-collect hotspots -knob sampling-mode=hw -knob enable-stack-collection=false -knob sampling-interval=10 -duration 30 -cpu-mask 20`、`-collect uarch-exploration -knob pmu-collection-mode=detailed -knob sampling-interval=10 -duration 30 -cpu-mask 20`、`-target-pid`、`-command stop -r`、`-report summary/hotspots`。当前 Microarchitecture 默认使用 summary 整段硬件计数；不能冒充按函数 detailed 采样。当前主机不再自动尝试 detailed。[Intel collect 文档](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/collect.html)、[Intel PMU 模式文档](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-0/knob.html)。编译阶段在恢复后的 cgroup 内采用允许的全核心并行度，并记录内存事件；两套产品分别构建，性能计时与采样保持串行。

早期实验的 Wasmtime Launch Application 流程额外使用 `-start-paused`、`-command resume -r` 和 `-command pause -r`；不在启动模式下调用可能终止目标的 stop 来代替正常退出。暂停后继续至真实 guest 退出，保持 teardown 无 CPU 采样。[Intel 暂停采集文档](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/pausing-collection-from-the-command-line.html)、[Intel 采集时段控制](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-1/managing-analysis-duration-from-the-command-line.html)。

恢复硬件执行前，须重新核对主机稳定性和当前 boot/cgroup，并使用新输出目录。之前的零字节采样记录保留为中断，不自动恢复旧协调进程。当前保护逻辑及局部硬件窗口已验证；完整 Core 3 硬件归属和 Wasmtime 性能目标尚未完成。


Advisor Survey 复现命令同样必须由 SSH Linux 64 GiB、swap=0 守护器启动并固定 CPU20；输入必须有真实 `_start` 和 `profile_end`，校验值来自独立 oracle。实际附加的子进程仅授权当前父进程及其后代作为 ptracer，不更改全局 ptrace/sysctl；目标 capabilities 为零。命令采用 Intel 官方 Survey attach 和 detach，原始 loop/function 报告中必须存在非零测量行，函数的空 ID 也保留。采集和未采样计时分开进行。

```sh
python3 test/0017.runtime/run_debug_advisor_survey.py \
  --uwvm /path/to/frozen-uwvm --wasm /path/to/profile-gated-calls.wasm \
  --out /path/to/new-advisor-result --expected-cgroup /actual/verified/cgroup \
  --cpu 20 --expected-checksum INTEGER_FROM_MANIFEST
```


## v75: authenticated dynamic typed `call_ref` IR (2026-10-12)

已同步 main、stable、ROS 的实际 LLVM IR 快路径：对同模块 defined function reference 核验真实 record 范围、stride、canonical subtype interval 和 LLVM carrier ABI，再逐次 acquire-load 当前发布的 typed entry。类型投影移到编译期 admission bitmap；有效快路径绕过 C++ resolver。空值、零入口、跨模块/import/raw、类型不匹配、lazy/tiered/resumable/staged 和其他平台继续完整旧解析／陷阱路径。真实帧、初始化标志、locals、operands、GC roots、EH、tail 和动态替换协议保持。observe policy47 拒绝旧43；resumable17 / ABI2 保持。

| 长内核，各五个样本，中位数 ms | main 前 → 后 | stable 前 → 后 | ROS 前 → 后 | Wasmtime真实Wasm/LLDB |
| --- | ---: | ---: | ---: | ---: |
| typed_call_ref | 6957.901 → 5423.212 | 6894.797 → 5230.379 | 6816.999 → 5302.023 | 282.639 |
| call_ref_fanout_256 | 2209.671 → 1815.044 | 2267.306 → 1849.723 | 2286.183 → 1881.150 | 99.336 |
| memory64 | 2074.761 → 2086.273 | 2027.736 → 2045.671 | 2029.859 → 2030.491 | 950.359 |

调用引用耗时分别下降22.057%、24.140%、22.224%；fanout下降17.859%、18.418%、17.717%。这些组前后范围不重叠；memory64 的小幅慢中位数范围重叠，控制输入基本持平。主产品仍比真实 Wasmtime Wasm 调试慢19.19倍／18.27倍，未达到性能目标。135个样本分别保留：main30个已冻结 screen 样本经生产二进制 SHA 一致后复用；stable/ROS60个与 Wasmtime ordinary/native GDB/真实 Wasm plugin45个为新测。使用 WASI CLOCK_MONOTONIC kernel，排除编译、启动、attach 与人为检查；process/startup另存。完整原始样本、范围与限制见 [v75报告](debug_typed_call_ref_v75_performance_report.json)。

全部运行／编译仅 SSH Linux 原64GiB、swap0 cgroup，xmake-j32，计时固定CPU21 E-core。三个产品各8个实际组件、19个PTY组、17个子类型／空引用案例、22个GC陷阱以及66帧真实leaf/busy/suspended caller状态检查通过；动态引用、table复制与同一调用中的替换分别重启三次。每产品20个Core3输入仍有原有 deterministic_profile 缺口（五条失败记录，优化前后均有），因此19/20，不称完整矩阵通过。Windows/QEMU、macOS、完整sanitizer和C++module本轮未测。

新增stable使用独立baseline；恢复了其53个缺失的已锁定LLVM源码文件，逐字节核对ROS同版本manifest。首次stable增量构建仅0.125s并冻结了旧baseline，已明确作废；随后强制xmake-r-j32重建为不同SHA，并重跑全部实际程序资格和长输入。不能使用旧的stable after结果作为优化证据。

9个有效VTune50ms硬件窗口（Hotspots、uarch summary／detailed）及2个Advisor Survey保留实际事件、能力、cgroup、hash和命令。typed_call_ref退休指令113423987628 → 87757286088，fanout28283816564 → 22600514593；CPI略变差，收益来自减少执行工作，不能称CPI改善或精确逐opcode成本。未知JIT样本不虚构函数归属；Advisor是软件Survey。采样结束于14:47:02，主机14:53:49/50出现较晚的perf interrupt告警并降至32000，此后未再采样；不归因未知进程、不声称全主机稳定，也未改sysctl／驱动／watchdog。

已在真实full-debug MCJIT notifyObjectLoaded处保存原始ELF，再对同一Wasm抓取冻结Wasmtime49.0.2的实际ELF／CLIF。UWVM新快路径跳过保留的冷resolver；Wasmtime读取code/vmctx并间接调用，断点位置实际是五字节NOP。按ELF STT_FUNC精确边界（排除relocations和padding），短apply：UWVM1347bytes/298静态指令，Wasmtime guest149bytes/37；kernel：UWVM14413 → 14555bytes、3080 → 3111静态指令，Wasmtime guest535bytes/128。新增guard和冷fallback使静态代码更大但动态执行更少，不能把这些静态数字称为每条Wasm的退休指令。原capture计数误把relocation行计为instruction，已作废，只用exact-symbol-audit。只读Wasmtime源码commit b4b23fe（workspace46.0.0）不是实测49.0.2的构建源码。

当前还需缩减activation/shadow-ledger登记、workspace初始化及完整locals复制。共享工作区新增的另一组数组实验改动已保留：本轮测试绑定到冻结v75源码；local reconciliation确认call_ref区段与helper/policy/tests不变，未把其他实验当成已验证。


后续v76入口实验未合入：真实fused控制上下文在prepare之后才绑定，导致初始packet继续用私有copier。尝试提前绑定后，19个PTY组、state/native/full trace均通过，短apply静态计数由1347bytes/298instructions/18calls降到1312/287/17；但两个候选分别令256-target fanout慢3.949%和4.075%，前后范围不重叠，虽typed_call_ref快2.869%／2.517%仍全部撤回。第二版已限制Linux x86_64、完整纯numeric locals<=2，仍有回退。60个原始未采样样本、源码／二进制hash、真实汇编、23／25个源码和build/config字节恢复检查保留在v75报告。没有归因未经验证的寄存器／缓存机制，没有继续硬件采样，也没有把诊断policy48合入；三个仓库保留已验证policy47。复用的main screen不包含逐目标capability字段；新stable/ROS计时、实际Wasmtime server及硬件目标才有明确Cap0收据。


后续v77工作区实验同样未合入：尝试在真实sealed plan／activation核验后，只重置site与全部local metadata，复用将由现有IR完整覆盖的payload；普通DATA入口继续清零全部可见字节。六组存储／shadow／reference组件、六组实际state/native/full-trace资格及专用SI/NI检查通过，xmake-j32实际编译成功；30个交替长样本中typed_call_ref中位数慢0.667%（范围不重叠），256-target分派慢0.983%（范围重叠），memory64快0.156%（范围重叠，基本持平），未显示值得合入的收益。因此在扩展生产资格前拒绝候选，24个source/build/config文件逐字节恢复，真实build二进制重新匹配冻结v75。未使用硬件／Advisor采样，cgroup的max／OOM／oom_kill保持0。全部原始样本、候选hash和恢复收据加入同一v75报告；没有把这版试验称为三个仓库生产验证通过。


## v79: LLVM IR canonical numeric locals in mixed prefixes (2026-10-12)
三个仓库采用相同的已验证优化：仅 Linux x86_64 的真实 observe_values、1..8 个数值／引用混合 locals，在实际执行 local.set／tee 和 self-tail reset 时更新自有数值调试槽。非零且未变化的策略 epoch 下避免再次复制数值槽；引用和 v128 仍每次完整刷新。首次捕获、pause／resume 后 epoch 变化、native 编辑、其他布局／平台、resumable 及非自有目标保持原完整 copier。observe policy50 拒绝旧47，resume17／ABI2 不变。设计参考当前只读 Wasmtime 源码的直接 local 更新及 operand 增量同步，实测二进制单独固定为49.0.2。

| 长内核，各五个样本，中位数 ms | main 前 → 后 | stable 前 → 后 | ROS 前 → 后 | Wasmtime真实Wasm/LLDB |
| --- | ---: | ---: | ---: | ---: |
| memory64 | 2023.347 → 2022.957 | 2023.685 → 2023.033 | 2023.958 → 2023.635 | 936.475 |
| typed_call_ref | 5290.753 → 5144.052 | 5160.502 → 4920.244 | 5130.080 → 4925.024 | 273.193 |
| gc_struct | 6708.965 → 4934.640 | 6718.699 → 5141.163 | 6726.929 → 5148.466 | 254.083 |
| call_ref_fanout_256 | 1815.187 → 1767.260 | 1799.336 → 1732.942 | 1783.640 → 1735.594 | 91.266 |

GC struct 耗时下降26.447%、23.480%、23.465%，前后样本范围均不重叠；typed call_ref 的中位数下降2.773%、4.656%、3.997%，其中stable范围重叠；fanout下降2.640%、3.690%、2.694%，其中ROS范围重叠。memory64变化均小于0.04%，范围重叠，基本持平。五样本范围分离只是描述性证据，没有宣称统计显著性。主产品typed／fanout仍比真实Wasmtime guest慢18.83／19.36倍，未达到性能目标。

180个原始长计时保留：三个产品120个前后交替样本，Wasmtime ordinary、native GDB、真实Wasm/LLDB共60个新样本。main40个screen样本仅在强制生产构建SHA完全一致后复用。WASI单调时钟排除编译、启动、attach与人为状态检查；process/startup另存。GDB模式是native/DWARF无命中，不代表Wasm状态插件。全部编译／运行仅SSH Linux原64GiB、swap0 cgroup，xmake-j32，计时CPU21；共享主机不声称独占。

三仓库各8个实际组件、19个PTY组、17个typed／subtype／null案例、22个GC陷阱及66帧实际leaf／busy／suspended状态检查通过；动态和复制引用替换各重复三次。完整20个Core3输入仍保留优化前后的deterministic_profile缺口，每产品19/20，不声称完整矩阵通过。ROS预算组件缺少旧测试源码，明确使用main同一DATA测试源编译ROS自身头文件。新组件实际执行MCJIT生成的生产helper，验证数值bit pattern、初始化flag、引用搬移、full／sparse epoch、外部native编辑、self-tail和新activation。

拒绝了两个较宽候选：v78 dirty刷新令memory控制慢1.917%；v79第一版所有小numeric前缀直接维护令memory慢7.748%。分别27／28个source/build/config文件逐字节恢复后才选定混合前缀版本。生产脚本第二次因ROS旧测试源缺失终止，84个mutable文件精确恢复；修正共享DATA路径后的第三次完成全部资格和计时。原始失败与恢复收据一并保留，未当成产品回归通过。

本轮未恢复VTune／Advisor硬件采样，之前v75硬件窗口仅作历史分析依据，不作新版本退休指令／CPI结论；未进行本机编译、Windows/QEMU、完整sanitizer或C++module测试。只同步本轮已资格化源码及局部emit字段／include，保留本机其他guarded-array实验；完整混合working tree不在冻结构建测试范围。完整180样本、源码／binary哈希、范围、实际插件连接命令、资格及限制见 [v79报告](debug_canonical_numeric_locals_v79_performance_report.json)。


## v81: LLVM IR whole-cell operand stores (2026-10-12)

真实 Linux x86_64 observe_values 的增量 operand 槽，使用 bitcast／零扩展后的 i128 完整写入，替代清零 byte array 再写窄值。所有引用仍每次捕获刷新，FULL epoch／CFG／resume／native 编辑刷新、真实帧、GC、初始化和动态替换协议保持。observe52 拒绝旧50，resume17／schema6／ABI2 不变。其他平台及 ordinary／resumable 保持原路径。

| GC 长内核，中位数 ms | 前 | 后 | 耗时下降 |
| --- | ---: | ---: | ---: |
| uwvm2 | 4950.701 | 4912.671 | 0.768% |
| uwvm2-stable | 5124.342 | 5056.063 | 1.332% |
| uwvm2-ros | 5116.035 | 5082.626 | 0.653% |

GC 配对改善主仓库约1%，15组中14组更快；所有完整样本范围重叠，不宣称统计显著性。call_ref／memory 基本持平；fanout 补测接近持平，主仓库配对中位数略慢，不能把混合时段中位数差异当成稳定加速。当前 main call_ref 5130.482ms，Wasmtime真实Wasm/LLDB 271.194ms，仍约18.92倍，未达到性能目标。

同一300槽位、609次捕获的实际MCJIT DATA探针，生成IR从279526降到189521（约32.2%）；字节、初始化、NaN／vector、重复引用、pop／CFG和哨兵一致。其epoch失配冷路径按保留前缀累积，不是线性规模；原旧线性阈值已由基准实测纠正。这不是动态ASM退休指令减少比例。实际同一小模块的UWVM生成代码指令行10173→10116；Wasmtime ordinary717／guest1312，仅为静态代码体积证据，不按不同运行时的整个模块体积推算吞吐。

新VTune：CPU21(E-core)，CPU9只为混合PMU验证，目标全部线程固定CPU21且权限为零；50ms、无stack、≤30秒、串行journal监视。typed前后退休指令约84958914473→84960661012，基本不变；GC73964886168→73714880222，下降0.338%。只是一组硬件计数，不替代220个未采样计时。Summary里未运行的P-core百分比不属于目标负载。GC热点初次只有8个clock样本而拒绝；保持阈值／间隔并只加倍原repeat、独立oracle后取得98个retired和17个clock样本；这一较长输入不进入吞吐样本。有效窗口没有kernel告警，不声称整个共享主机稳定。依据[Intel CLI文档](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-0/command-line-interface.html)。Advisor本轮未复跑。

三仓库各9组件、19PTY组、17typed案例、22GC陷阱及66个实际帧检查通过；动态／复制引用替换各三次。20项Core3仍保留前后的deterministic_profile缺口（19/20）。220原始计时含main补测40、其他产品与新Wasmtime ordinary／native GDB／真实Wasm插件样本，版本／源码／输入／二进制hash完整冻结。两个fused-entry候选因分派慢4.409%／2.878%拒绝，分别29／30个mutable文件精确恢复；全部失败、校验、采样和恢复收据保留。

只在SSH Linux64GiB／swap0 cgroup编译与运行，xmake-j32；没有本机、Windows/QEMU、sanitizer或C++module测试。本机同步四个已验证路径，保留同时发生的数组实验，完整混合working tree不在冻结构建范围。旧v79／v75报告不变。完整限制与原始数据见 [v81报告](debug_canonical_slot_stores_v81_performance_report.json)。


## v84: authenticated observer entry and dynamic call demand (2026-10-12)

函数入口把真实 activation admission 的核验结果同步传给原有 LIFO workspace bind，减少重复加入；仅实际 Linux x86_64 little-endian64 observer entry 使用 SysV 两寄存器私有返回值。完整缓存身份和 shared-owner alias 校验保留，真实 ledger admission／key equality 内联。动态 call_ref 在原有完整 typed-reference 核验及无观察需求时省略可选回调，完整 before-call packet 与所有引用仍发布；暂停后实际调用重新读取 entry，不能复用暂停前的观察地址。源码点在 semantic／LLVM 两个栈 pop 之前生成。ordinary／resumable 及其他目标保持原路径。observe55 拒绝旧52；resume17／schema6／ABI2 不变。

| typed_call_ref，中位数 ms | 前 | 后 | 耗时下降 |
| --- | ---: | ---: | ---: |
| uwvm2 | 5123.687 | 4040.081 | 21.149% |
| uwvm2-stable | 4936.221 | 4180.621 | 15.307% |
| uwvm2-ros | 4909.146 | 4182.131 | 14.809% |

主仓库 typed／256目标fanout各15个交替配对，其余产品每个输入5对；初始220个未采样计时包含新Wasmtime普通、GDB native DWARF与实际Wasm/LLDB插件三个独立模式。只有production程序SHA与候选一致才复用80个main样本。memory64／main和ROS GC完整范围重叠，不宣称这些负载稳定加速。stable GC最初5对慢约0.96%，另加10对单独复测；所有结果保留。

VTune typed_call_ref：退休指令 84960838657 → 72096085488，下降 15.142%；CPI 0.247628 → 0.230019。一组硬件计数，仅实际_start停点至profile_end，排除编译／启动／销毁并包含输出；不替代未采样计时。

VTune gc_struct：退休指令 73717295395 → 73718837522，下降 -0.002%；CPI 0.278277 → 0.278492。一组硬件计数，仅实际_start停点至profile_end，排除编译／启动／销毁并包含输出；不替代未采样计时。

实际生成UWVM小模块的静态指令行10116→10227，Wasmtime ordinary717／guest1312；整模块大小不是动态指令量。当前main typed_call_ref 4040.081ms，Wasmtime实际Wasm插件 272.397ms，仍约 14.83 倍，尚未达到性能目标。硬件热点保留未知JIT符号；不能从粗粒度采样或整模块静态汇编行数推算每条Wasm的动态指令量。每个热点窗口保持至少10个clock及retired样本，只加倍原repeat并使用独立oracle，不进入吞吐样本。CPU21固定目标、CPU9仅混合PMU验证、目标capabilities零；50ms、无stack、≤30秒、串行kernel journal监视，没有采样告警，不声称共享主机全局稳定。依据[Intel CLI文档](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-0/command-line-interface.html)。Advisor本轮未复跑。

三个仓库各12组件／4种layout校验、19PTY组、17typed案例、22GC陷阱、66个实际帧通过，动态／复制引用各三次替换。20项Core3仍是19/20：deterministic_profile基准和候选均失败，未计为通过。只在SSH Linux64GiB／swap0 cgroup构建运行，xmake-j32；本机没有运行编译或测试。本机三个仓库以范围补丁同步，反向恢复校验准确保留既有数组实验与ROS差异，整个混合working tree不属于冻结构建验证范围。失败试验和恢复记录保留。完整数据见 [v84报告](debug_typed_observer_entry_v84_performance_report.json)。

Stable GC共15对合并中位数：5055.324→5073.857ms，下降-0.367%；单独10对复测范围见原始收据。总计240个未采样样本，均校验真实checksum。剩余热点主要包含真实observer entry admission与shadow-ledger维护，GC get/set／host scope及未知JIT行；粗粒度采样不支持精确的小函数百分比。

| 256目标分派，中位数 ms | 前 | 后 | 耗时下降 |
| --- | ---: | ---: | ---: |
| uwvm2 | 1777.244 | 1530.363 | 13.891% |
| uwvm2-stable | 1745.234 | 1538.230 | 11.861% |
| uwvm2-ros | 1746.376 | 1537.375 | 11.968% |

稳定版GC的新增10对单独复测中位数5051.186→5063.889ms（候选慢0.251%），完整范围重叠；15对合并中位数候选慢0.367%。两组均保留，不把小幅回退写成持平或提速。


## v90: bounded numeric copier IR inlining (2026-10-12)

实际 Linux x86_64 observer 的纯数值、最多4个局部变量的私有复制函数使用 LLVM InlineFunction 内联原有 IR；在克隆前建立独立 continuation，避免使用已被删除的 call iterator。大副本和引用副本继续共享，完整字节／padding、执行标志、sNaN 位型、引用刷新、所有 safepoint、真实 admission／owner／parent／tail／GC 校验不删减。warm admission lambda 内联，保留原 catch 和 cold path。observe56 拒绝旧55；resume17／schema6／ABI2 不变。

| 负载 | 仓库 | 前 ms | 后 ms | 耗时下降 | 配对数 |
| --- | --- | ---: | ---: | ---: | ---: |
| memory64 | uwvm2 | 2023.676 | 2023.809 | -0.007% | 10 |
| typed_call_ref | uwvm2 | 4027.426 | 3874.016 | 3.809% | 10 |
| gc_struct | uwvm2 | 4881.572 | 4963.984 | -1.688% | 10 |
| call_ref_fanout_256 | uwvm2 | 1525.944 | 1513.656 | 0.805% | 10 |
| memory64 | uwvm2-stable | 2023.467 | 2024.476 | -0.050% | 5 |
| typed_call_ref | uwvm2-stable | 4164.182 | 3885.276 | 6.698% | 5 |
| gc_struct | uwvm2-stable | 5077.719 | 4931.667 | 2.876% | 5 |
| call_ref_fanout_256 | uwvm2-stable | 1535.853 | 1520.861 | 0.976% | 5 |
| memory64 | uwvm2-ros | 2023.429 | 2023.776 | -0.017% | 5 |
| typed_call_ref | uwvm2-ros | 4160.233 | 3886.470 | 6.580% | 5 |
| gc_struct | uwvm2-ros | 5074.561 | 4928.517 | 2.878% | 5 |
| call_ref_fanout_256 | uwvm2-ros | 1540.810 | 1525.151 | 1.016% | 5 |

220个新的未采样计时包含main每个输入10对、其他两个仓库各5对，以及Wasmtime49.0.2普通／GDB native／实际Wasm-LLDB插件三个独立模式。主仓库gc_struct退化1.688%，不能声称所有负载加速；退化、范围重叠、单独配对中位数和全部原始样本都在报告中保留。GC退休指令基本不变，clockticks与CPI上升，目前没有足够证据确定回退原因。

VTune typed_call_ref 退休指令 72097077824 → 69796767939，下降 3.191%；CPI 0.228970 → 0.228651。一组硬件计数不替代吞吐计时；热点独立double-repeat输入达到最低clock／retired样本数后才采用，未知JIT符号仍标未知。

VTune gc_struct 退休指令 73713401864 → 73713449380，下降 -0.000%；CPI 0.272311 → 0.276488。一组硬件计数不替代吞吐计时；热点独立double-repeat输入达到最低clock／retired样本数后才采用，未知JIT符号仍标未知。

main typed_call_ref 3874.016ms；Wasmtime实际Wasm插件 271.110ms，仍约14.29倍，未达到目标。对照实际MCJIT原始对象和Wasmtime cwasm／CLIF，静态指令行不能推算动态吞吐。固定CPU21、CPU9仅PMU验证、目标capabilities零、50ms／无stack／≤30s、构建和采样串行；没有本轮采样kernel告警，不声称共享主机全局稳定。按[Intel CLI文档](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-0/command-line-interface.html)使用。Advisor未运行。

三个仓库各4组件（真实MCJIT与4种layout verifier）、19PTY组、17typed案例、22GC陷阱、66个实际暂停帧，以及动态／复制引用的替换检查通过。Core3输入仍19/20，已存在deterministic_profile失败没有计为通过。仅SSH Linux64GiB／swap0 cgroup编译执行，xmake-j32；本机只同步源码与报告，原有数组实验和ROS差异保留，混合本机working tree未构建。槽位证明候选的失败／回退与所有精炼过程完整保留。数据见 [v90报告](debug_observer_followup_v90_performance_report.json)。


## v91: exact live extents and inline workspace reuse (2026-10-12)

LLVM observer IR 调用的私有工作区入口将物理构造字节与当前有效范围分开：复用热路径内联，增长路径单独保留原有分配失败回滚。每次仍清零完整有效范围，包括 site／flags／carrier／padding；增长时旧前缀也全部清零。退栈清除有效范围与 prefix，保留原有32记录／16KiB容量限制。所有当前借用只接受有效范围，缩小后的物理尾部不能扩大 packet，已退栈的旧地址不可见。真实 activation／publication／parent／类型／GC 检查不删减。guest IR／entry ABI／observe56／resume17／schema6／ABI2 不变。

| 负载 | 仓库 | 前 ms | 后 ms | 耗时下降 | 配对数 |
| --- | --- | ---: | ---: | ---: | ---: |
| memory64 | uwvm2 | 2024.335 | 2023.733 | 0.030% | 10 |
| typed_call_ref | uwvm2 | 3862.906 | 3728.219 | 3.487% | 10 |
| gc_struct | uwvm2 | 4944.289 | 4862.937 | 1.645% | 10 |
| call_ref_fanout_256 | uwvm2 | 1510.125 | 1483.647 | 1.753% | 10 |
| memory64 | uwvm2-stable | 2027.253 | 2026.863 | 0.019% | 5 |
| typed_call_ref | uwvm2-stable | 3882.320 | 3773.023 | 2.815% | 5 |
| gc_struct | uwvm2-stable | 4928.590 | 4924.690 | 0.079% | 5 |
| call_ref_fanout_256 | uwvm2-stable | 1524.347 | 1489.460 | 2.289% | 5 |
| memory64 | uwvm2-ros | 2027.110 | 2028.830 | -0.085% | 5 |
| typed_call_ref | uwvm2-ros | 3887.176 | 3765.030 | 3.142% | 5 |
| gc_struct | uwvm2-ros | 4919.733 | 4924.411 | -0.095% | 5 |
| call_ref_fanout_256 | uwvm2-ros | 1524.685 | 1489.710 | 2.294% | 5 |

220次新的未采样计时包含main每输入10对、stable和ROS各5对，以及Wasmtime49.0.2普通／GDB native／实际Wasm-LLDB插件三个模式。退化、范围重叠、配对中位数和原始样本均保留；不声称所有输入改善。上一轮v90的GC回退记录原样保留。

VTune typed_call_ref 退休指令 69800235064 → 67647918013，下降 3.084%；CPI 0.227670 → 0.226863。一组计数不替代吞吐计时；硬件窗口包括结果输出，独立double-repeat热点保留未知JIT行。

VTune gc_struct 退休指令 73717449860 → 73718220489，下降 -0.001%；CPI 0.275807 → 0.272441。一组计数不替代吞吐计时；硬件窗口包括结果输出，独立double-repeat热点保留未知JIT行。

main typed_call_ref 3728.219ms；Wasmtime实际Wasm插件 271.206ms，仍约13.75倍，未达到目标。参考本机Wasmtime revision65abe375的stack-slot／dirty operands／local.set和typed call_ref设计，并导出实际MCJIT对象与Wasmtime CLIF／汇编；静态指令数不能直接推算动态吞吐。

三个仓库各7原生组件、19PTY组、17typed案例、22GC陷阱、66实际暂停帧和动态／复制引用替换检查通过；Core3仍19/20，旧deterministic_profile失败没有计为通过。仅SSH Linux64GiB／swap0 cgroup构建执行，xmake-j32；本机只同步两个源码／测试文件及报告，原有数组实验和ROS差异保留，混合本机working tree未构建。固定CPU21，CPU9仅PMU验证；目标capabilities零，50ms／无stack／≤30s采样串行，无本轮kernel采样告警或cgroup OOM。按[Intel CLI文档](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-0/command-line-interface.html)使用；Advisor未运行，不声称共享主机全局稳定。见 [v91报告](debug_workspace_extent_v91_performance_report.json)。

2026-10-13交付前发现Linux已重启，原64GiB／swap0容器恢复后重新校验源码及可执行文件哈希。计时、硬件计数、原cgroup峰值均来自记录中的上一boot；本次恢复不计作重新执行测试。


## v92: observation-only checked scalar GC leaf lowering (2026-10-13)

LLVM翻译层仅在不可变observe_values profile且无managed-page宏时，选择原checked scalar struct.get32直接目标；减少外层foreign-host island暂停／恢复及uncaught-exceptions工作。普通／未绑定／resumable／managed-page仍走原选择。对象／类型／字段边界／可变字段锁／外来对象强租约／原始整数与f32位／状态陷阱／完整packet及GC roots规则不变。没有runtime开关、readnone承诺或未检查对象地址。observe57隔离旧缓存，resume17／schema6／ABI2／cache5不变。

| 负载 | 仓库 | 前 ms | 后 ms | 耗时下降 | 配对数 |
| --- | --- | ---: | ---: | ---: | ---: |
| memory64 | uwvm2 | 2023.156 | 2023.406 | -0.012% | 10 |
| typed_call_ref | uwvm2 | 3900.337 | 3952.531 | -1.338% | 10 |
| gc_struct | uwvm2 | 5129.761 | 4106.933 | 19.939% | 10 |
| call_ref_fanout_256 | uwvm2 | 1586.135 | 1599.927 | -0.870% | 10 |
| memory64 | uwvm2-stable | 2023.098 | 2023.099 | -0.000% | 5 |
| typed_call_ref | uwvm2-stable | 3760.782 | 3751.679 | 0.242% | 5 |
| gc_struct | uwvm2-stable | 5329.644 | 4192.180 | 21.342% | 5 |
| call_ref_fanout_256 | uwvm2-stable | 1592.573 | 1564.932 | 1.736% | 5 |
| memory64 | uwvm2-ros | 2022.656 | 2023.256 | -0.030% | 5 |
| typed_call_ref | uwvm2-ros | 3777.733 | 3806.290 | -0.756% | 5 |
| gc_struct | uwvm2-ros | 5282.578 | 4209.176 | 20.320% | 5 |
| call_ref_fanout_256 | uwvm2-ros | 1575.328 | 1556.605 | 1.188% | 5 |

220次新的未采样计时：main每输入10对、stable及ROS各5对，另含Wasmtime49.0.2 ordinary／native GDB／实际Wasm-LLDB插件各5次。比较本轮同boot运行的冻结v91与新候选；退化、范围重叠、配对中位数和原始样本全部保留。

本轮硬件验证未完成：VTune baseline summary通过，candidate在内核`perf: interrupt took too long (3135 > 3131)`告警后被保护脚本拒绝。失败根记录及含告警的子receipt原样保留；根记录空的kernel_alerts数组不代表无告警。没有将失败计数纳入合格对照，后续GC硬件计数及hotspots未运行，不再尝试PMU采集。此前完成的220次未采样计时独立保留；告警来源无法归因，共享主机不做整体稳定声明。

原生cancel／fi交互首次失败，旧基线与候选各3次原脚本对照也失败；后续稳定暂停复现及原脚本再次通过。原记录全部保留，此轮不声称该间歇性问题已经修复。

main typed_call_ref 3952.531ms；Wasmtime实际Wasm插件280.576ms，仍约14.09倍，未达到目标。导出本轮GC fixture的实际MCJIT对象、Wasmtime CLIF／汇编；静态代码不能代替动态计数。

三个仓库在v92c-production分别完成全新构建及10组件检查；v92d复用哈希完全相同的冻结ELF和组件记录，重新执行全部19PTY组、17typed案例、22GC陷阱、66实际暂停帧及动态／复制引用替换，均通过；Core3仍19/20，旧deterministic_profile失败原样保留。仅SSH Linux64GiB／swap0构建执行，xmake-j32；本机每仓库只同步4个源码／测试文件和报告。GC翻译文件仅应用两个经过验证的hunk，完整反向重建验证原有实验字节保留；混合本机树未构建。初始const／链接／demangling夹具失败及恢复记录保留，未计为通过。固定CPU21、目标capabilities零，CPU9仅PMU验证；采集配置50ms／无stack／≤30s串行；本轮有kernel采样告警并停止，cgroup OOM仍为零。按[Intel CLI文档](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-0/command-line-interface.html)使用；Advisor未运行，共享主机不做全局稳定声明。见[v92报告](debug_gc_scalar_internal_leaf_v92_performance_report.json)。

## v93: bounded mixed entry captures; preserve pure GP layout

Only SSH Linux, the existing 64 GiB cgroup, and fresh unprofiled paired runs. No local build or runtime tests.

| Repository | Kernel | Before ms | After ms | Median time reduction |
|---|---|---:|---:|---:|
| uwvm2 | memory64 | 2142.916 | 2118.460 | 1.141% |
| uwvm2 | typed_call_ref | 3856.829 | 3880.195 | -0.606% |
| uwvm2 | gc_struct | 3916.140 | 3901.546 | 0.373% |
| uwvm2 | call_ref_fanout_256 | 1536.818 | 1495.373 | 2.697% |
| uwvm2-stable | memory64 | 2089.197 | 2091.723 | -0.121% |
| uwvm2-stable | typed_call_ref | 3964.678 | 3958.666 | 0.152% |
| uwvm2-stable | gc_struct | 3946.399 | 3895.529 | 1.289% |
| uwvm2-stable | call_ref_fanout_256 | 1540.609 | 1471.707 | 4.472% |
| uwvm2-ros | memory64 | 2083.690 | 2083.803 | -0.005% |
| uwvm2-ros | typed_call_ref | 4035.224 | 4105.728 | -1.747% |
| uwvm2-ros | gc_struct | 3937.414 | 3891.211 | 1.173% |
| uwvm2-ros | call_ref_fanout_256 | 1541.711 | 1523.096 | 1.207% |

The accepted change keeps complete values, flags, reference checks, real activation tracking and breakpoint/step/trace behavior. Pure GP entries keep their prior tiny copier because broad entry SSA caused a repeatable multi-target regression. Three failed performance variants are retained in the report. Small mixed/FP/vector entries remove the address table; initial flags are compiler-declaration constants only at the real initial producer. Actual GP arguments keep original cells across entry calls. Small availability prefixes are written once with exact bounds.

Fresh main kernel-clock timing remains 13.865 times Wasmtime real Wasm/LLDB guest debugging for typed_call_ref. No parity claim. Ranges, regressions and all 220 samples are retained. Main and ROS typed_call_ref medians regressed with overlapping ranges; this is not a uniform throughput improvement.

Hardware qualification passed: True. Hardware-window clocks and static assembly instruction rows are not throughput samples. Failed sampling receipts and alerts remain authoritative.

Three repositories were freshly built and checked. Each ran 17 component fixtures and 19 actual PTY groups; actual 66-frame checks, reference replacement/copying, traps, typed subtype tests and Core3 checks are in the report. Existing deterministic_profile and intermittent native cancellation limitations remain. This does not qualify unrelated local experimental source or macOS/Windows/QEMU.

Report: `debug_observer_entry_inline_v93_performance_report.json`. Observe policy58, resumable17, native ABI2, schema6 and cache format5.

## v94: reuse authenticated entry record; inline retained block checks

Only SSH Linux and the existing 64 GiB cgroup. Fresh, alternating kernel-clock timing excludes JIT compilation/startup and uses CPU21. No local build or runtime tests.

| Repository | Kernel | Before ms | After ms | Median time reduction |
|---|---|---:|---:|---:|
| uwvm2 | memory64 | 2025.676 | 2026.123 | -0.022% |
| uwvm2 | typed_call_ref | 3747.490 | 3706.406 | 1.096% |
| uwvm2 | gc_struct | 3883.509 | 3866.907 | 0.427% |
| uwvm2 | call_ref_fanout_256 | 1438.874 | 1447.641 | -0.609% |
| uwvm2-stable | memory64 | 2022.826 | 2022.481 | 0.017% |
| uwvm2-stable | typed_call_ref | 3783.710 | 3678.412 | 2.783% |
| uwvm2-stable | gc_struct | 3891.864 | 3868.103 | 0.611% |
| uwvm2-stable | call_ref_fanout_256 | 1469.894 | 1471.772 | -0.128% |
| uwvm2-ros | memory64 | 2022.531 | 2023.416 | -0.044% |
| uwvm2-ros | typed_call_ref | 3776.976 | 3699.048 | 2.063% |
| uwvm2-ros | gc_struct | 3890.770 | 3868.695 | 0.567% |
| uwvm2-ros | call_ref_fanout_256 | 1469.978 | 1472.862 | -0.196% |

The existing LLVM JIT fused entry now shares one synchronous authenticated module/publication/generation lookup with the complete warm plan-cache join. Every native activation remains real and fresh. Shadow parent, tail, quota, ownership and generation checks remain; misses retain the canonical source lookup. Already-constructed segmented blocks pass the original capacity check inline, while actual growth preserves the original budget, allocation failure and stable address behavior.

Typed call_ref remains 13.659 times Wasmtime real Wasm/LLDB guest debugging. No parity claim. All 220 fresh unprofiled samples, ranges and losses are retained; main has ten pairs and stable/ROS have five per kernel. Wasmtime ordinary, native GDB and Wasm guest-plugin runs are distinct modes. This shared-host experiment does not prove a universal speedup.

Cross-runtime timings use the same Wasm inputs and kernel-clock timing, but UWVM records debug-local-ssa/less while Wasmtime uses CLI opt-level=2. The full ratio cannot be assigned to one resolver or frame-maintenance mechanism.

All three repositories built with xmake -j32. Each ran 8 component fixtures and 19 real PTY groups, reference replacement/copying, 22 GC traps, 17 typed subtype cases and the Core3 feature matrix. Each actual paused-chain check reads 66 frames including 64 exact suspended recursive callers. Existing deterministic_profile and intermittent native cancellation limitations remain.

Hardware sampling uses the official Intel VTune CLI with 50 ms driverless hardware hotspots and paired microarchitecture summary windows; target capabilities are zero. Hardware-window clocks and static assembly instruction rows are not throughput samples. Fresh kernel checks and all raw receipts are preserved.

Report: `debug_observer_admission_v94_performance_report.json`. Runtime bookkeeping optimization; generated IR/ABI, observe policy58, resumable17, schema6, native ABI2 and disk cache format5 are unchanged. No macOS/Windows/QEMU or unrelated local-source qualification.

## v95: scalar cleanup and complete unreachable-path pruning

Only SSH Linux in the existing 64 GiB cgroup. CPU21 fresh alternating kernel-clock timing excludes JIT/startup; no local compiler or runtime tests.

| Repository | Kernel | Before ms | After ms | Median time reduction |
|---|---|---:|---:|---:|
| uwvm2 | memory64 | 2023.826 | 1896.392 | 6.297% |
| uwvm2 | typed_call_ref | 3715.085 | 3672.502 | 1.146% |
| uwvm2 | gc_struct | 3936.633 | 3953.623 | -0.432% |
| uwvm2 | call_ref_fanout_256 | 1462.295 | 1480.463 | -1.242% |
| uwvm2-stable | memory64 | 2024.053 | 1896.197 | 6.317% |
| uwvm2-stable | typed_call_ref | 3695.529 | 3666.839 | 0.776% |
| uwvm2-stable | gc_struct | 3947.121 | 3997.280 | -1.271% |
| uwvm2-stable | call_ref_fanout_256 | 1487.882 | 1488.081 | -0.013% |
| uwvm2-ros | memory64 | 2023.740 | 1896.085 | 6.308% |
| uwvm2-ros | typed_call_ref | 3681.069 | 3663.121 | 0.488% |
| uwvm2-ros | gc_struct | 3961.635 | 4041.983 | -2.028% |
| uwvm2-ros | call_ref_fanout_256 | 1487.724 | 1484.703 | 0.203% |

LLVM EarlyCSE and InstCombine now follow bounded numeric-local SSA for actual Linux x86_64 PC-relative observer functions. Constant terminator folding and complete unreachable-block pruning follow combining. No inlining, loop motion or general CFG merging pipeline is added. Live safepoints, cancellation Invokes, complete snapshots, native/root/restore admission and atomic/volatile effects remain. Observe60 invalidates prior generated caches; resumable17/schema6/native ABI2/disk cache5 remain.

The scalar-only candidate passed 18 of 19 main PTY groups but deterministically failed typed-call restart. Actual IR showed 23 safepoints/23 polls becoming 22/23: a constant-dead call was removed while its poison Invoke remained. That candidate was rejected and all72 source/product/configuration preimages restored exactly. The final pass removes the complete dead path; existing cancellation/bitmap checks remain. Actual native component execution covers this dead-call/Invoke shape, and the original four-mode PTY test is unchanged.

Typed call_ref remains 13.550 times Wasmtime real Wasm/LLDB guest debugging. No parity claim. All220 fresh unprofiled samples, ranges and negative results remain; main has ten pairs and stable/ROS five per kernel. Host sharing and differing optimization policies (UWVM debug-local-ssa/less plus cleanup versus Wasmtime opt-level2) limit causal attribution.

All three repositories built with xmake-j32. Main/stable reuse the successful identical final-source builds and all10 components from the preceding disk-declined stage, with exact whole-binary identities; ROS reuses its successful identical-source components and builds freshly. All repositories run new final runtime tests: 10 components, 19 real PTY groups, reference replacement/copying,22 GC traps,17 subtype cases and the Core3 matrix per repository. Actual paused chains read66 frames with64 exact suspended recursive callers. Existing deterministic_profile and intermittent native cancellation limitations remain.

Official Intel VTune CLI hardware collection uses50 ms driverless hotspots and paired microarchitecture summary windows. Target capabilities are zero. Hardware window clocks, static instruction rows and fixture counts are distinct from unprofiled throughput. Actual UWVM JIT objects and Wasmtime CLIF/ELF comparisons are retained.

Report: debug_scalar_cleanup_v95_performance_report.json. LLVM20 API compatibility branches are included but LLVM20 SDK compilation is not claimed; final qualification is LLVM23. No macOS/Windows/QEMU or unrelated local experimental-source qualification.

## v96: bounded native observer workspace capacity

Only SSH Linux in the existing64GiB cgroup. All three repositories have xmake-j32 builds and11 passing components each for this exact native-capacity source. Main/stable builds/components are from r7; ROS builds/components and all57 PTY groups/actual paused-frame reads are from r8. The r9 continuation verifies identical source and binary SHA and reuses those original raw receipts without relabeling them as fresh tests. No local build or runtime tests. CPU21 alternating unprofiled kernel clocks exclude startup/JIT.

| Repository | Kernel | Before ms | After ms | Median time reduction |
|---|---|---:|---:|---:|
| uwvm2 | memory64 | 1896.035 | 1896.001 | 0.002% |
| uwvm2 | typed_call_ref | 3669.571 | 3588.660 | 2.205% |
| uwvm2 | gc_struct | 3877.710 | 3877.425 | 0.007% |
| uwvm2 | call_ref_fanout_256 | 1464.560 | 1442.231 | 1.525% |
| uwvm2-stable | memory64 | 1895.979 | 1895.857 | 0.006% |
| uwvm2-stable | typed_call_ref | 3661.296 | 3592.154 | 1.888% |
| uwvm2-stable | gc_struct | 3912.336 | 3913.810 | -0.038% |
| uwvm2-stable | call_ref_fanout_256 | 1468.608 | 1449.475 | 1.303% |
| uwvm2-ros | memory64 | 1896.055 | 1896.753 | -0.037% |
| uwvm2-ros | typed_call_ref | 3659.814 | 3590.596 | 1.891% |
| uwvm2-ros | gc_struct | 3967.724 | 3960.694 | 0.177% |
| uwvm2-ros | call_ref_fanout_256 | 1466.988 | 1453.175 | 0.942% |

For final payloads up to64bytes, entry IR selects a native bridge whose warm path clears a constant72-byte physical reserve. Only the requested payload plus8-byte site is published as active storage, so padding grants no larger packet or frame capability. No JIT clearing instructions or deferred initialization gap are added. Growth beyond64bytes restores the original bridge. All activation/publication, full snapshot, initialization, root/restore and cancellation checks remain. Observe62 expires older caches; other schemas remain.

Native tests execute full scalar/vector/reference snapshots, sNaNs, absent references and padding; small entry selection, growth to the generic bridge, repeated selection, wrong-callee rejection, exact active extent and retirement. The earlier inline-clear candidate was rejected after all3 fanout medians regressed5.35-5.49%; all94 preimages were restored. Sparse E-core frontend pressure increased, while ICache Misses decreased; code-size/layout is a hypothesis rather than a demonstrated cache cause. Raw failure, throughput, hardware and generated-code evidence is preserved losslessly.

Typed call_ref remains 13.218 times Wasmtime real Wasm/LLDB guest debugging. No parity claim. All240 unprofiled samples remain, including184 complete measurements before the disk-reserve pause and56 new measurements after resume with identical source/input/binary SHA: main ten before/after pairs and stable/ROS five pairs per four kernels; five fresh samples each for main ordinary UWVM and Wasmtime ordinary/nativeGDB/realWasmLLDB guest modes. Shared-host activity and differing policies limit causal attribution.

All57 real PTY groups pass, with dynamic/copied references,22 GC traps and17 subtype cases per repository. Actual paused chains read66 frames with64 exact suspended recursive callers. Core3 retains the existing deterministic_profile baseline/candidate failure; intermittent native cancellation remains a known limitation.

Intel VTune CLI uses50ms driverless hardware hotspots and paired microarchitecture windows, target zero capabilities and CPU21. CPU9 is hybrid event validation only. Actual UWVM JIT objects and Wasmtime CLIF/ELF are captured separately. Profile clocks, sparse sample estimates and static instruction counts are not repeated throughput measurements.

Report: debug_entry_clear_v96_performance_report.json. Linux LLVM23 only; no macOS/Windows/QEMU or unrelated local experimental-source qualification.
