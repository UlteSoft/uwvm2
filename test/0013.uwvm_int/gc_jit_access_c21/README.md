**C21：将可变数值 GC 数组写入翻译成 LLVM IR，三仓库已同步并完成原生验证。**

本轮新增 `i8/i16/i32/f32 array.set` 路径。原通用桥接执行对象认证、布局查询、元素打包、状态码和写入；新路径先调用现有专门的 authenticated borrow，再由 JIT 生成无符号边界检查、类型化地址计算、原始位写入和锁释放。纯写入耗时降低 28.3–42.3%，i32 写入固定硬件计数器的 instructions 降低 59.3%、cycles 降低 25.5%。实际发生回收的混合压力测试中，四种类型 cycles 降低 2.3–6.1%，仍未达到 Wasmtime。所有数字仅指下列固定 Wasm 和绑定构建。

启用已有 LLVM JIT 配置时增加 `xmake f --gc-jit-access-profile=typed-array-access`；默认为 `default`，原 `typed-array-load` 仍可使用。该 profile 的五个宏必须在所有翻译单元一致并完整重编译，禁止只替换一个 TU。快路径目前限定 x86_64 Linux Clang/int128；其他平台保留原受检查路径，本轮没有跨架构性能证明。三份生产快照使用实际仓库全部当前源码、相同五宏、三个原生 TU 手工编译；它们不包含私有计时钩子。`bindings/` 保留全部源文件清单、构建 argv、ELF SHA 和五个修改文件，Linux 持久目录保留完整源压缩包和 ELF。

数值访问比较使用同一字节 Wasm、相同启动次数、三个平衡轮次、每 cell 五次内部 POSIX 计时；排除 JIT 和两个预热 batch，保留 VM 调用与构造成本。22 个工作负载 × 3 VM × 3 轮 = 198 cells。下面是每次 start 的中位耗时，ratio 小于 1 表示 C21 更快。

| Wasm 工作负载 | C20 ms/start | C21 ms/start | Wasmtime ms/start | C21/C20 | C21/Wasmtime |
| --- | ---: | ---: | ---: | ---: | ---: |
| i8-write | 1.263097 | 0.728757 | 0.108640 | 0.5770 | 6.7080 |
| i8-write-read | 2.047669 | 1.655891 | 0.196996 | 0.8087 | 8.4057 |
| i16-write | 1.234838 | 0.766371 | 0.129871 | 0.6206 | 5.9010 |
| i16-write-read | 2.018905 | 1.669918 | 0.213916 | 0.8271 | 7.8064 |
| i32-write | 1.299243 | 0.905454 | 0.132903 | 0.6969 | 6.8129 |
| i32-write-read | 2.003781 | 1.754083 | 0.230682 | 0.8754 | 7.6039 |
| f32-write | 1.306702 | 0.937457 | 0.125867 | 0.7174 | 7.4480 |
| f32-write-read | 2.052557 | 1.791008 | 0.253803 | 0.8726 | 7.0567 |

固定计数使用 VTune 随附 amplxe-perf 的成组 `cpu_core/cycles/u,instructions/u,ref-cycles/u`，无缩放，running 比例至少 99.99%，精确目标 TID/CPU2。READY 出现在 JIT 与预热之后，enable ACK→GO→DONE→disable ACK；内部 ROI 至少 500ms。少量用户态协议序列化进入计数，阻塞与内核时间不进入。i32-write C21/C20 cycles=0.744785、instructions=0.406581；尾调用保留项分别 1.000128/1.000049，函数引用初始化保留项 1.010428/0.999993。原先 3–5% 的部分墙钟差异不能单独归因为算法回退。参考数组未进入新路径，读与写仍明显落后；当前参考读取测试约 14.5× Wasmtime。

另写四个 GC 压力模块：131072 次“写入存活数组→分配并丢弃 struct→再次读取同一数组”，每次 start 有 131073 次分配，结尾检查 1024 个槽位。每个 UWVM 计时 sample 验证实际 collections/reclaimed>0、精确根启用、GC 准入为 0，且 C20/C21 的六项工作计数完全相同。36 个内部计时 cells 和 18 个固定计数 cells 的结果如下；Wasmtime 该协议不提供对应回收计数，不声称它的回收次数相同。

| 元素类型，写→分配→读 | C20 ms/start | C21 ms/start | Wasmtime ms/start | C21/C20 时间 | C21/Wasmtime 时间 | C21/C20 cycles | C21/C20 instructions |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| i8 | 7.42933 | 7.29367 | 5.36810 | 0.9817 | 1.3587 | 0.9520 | 0.8964 |
| i16 | 7.65300 | 7.15824 | 5.15502 | 0.9354 | 1.3886 | 0.9389 | 0.8939 |
| i32 | 7.50692 | 7.55067 | 5.79501 | 1.0058 | 1.3030 | 0.9770 | 0.8949 |
| f32 | 7.05335 | 7.41742 | 5.86810 | 1.0516 | 1.2640 | 0.9729 | 0.8949 |

f32 的墙钟中位耗时增加约 5.2%，i32 增加约 0.6%，这两项仍记录在报告中；成组计数的 cycles/instructions 均下降。两种观测口径并不等价，当前不能宣称每项墙钟都改善。i32 压力下 C21 约 37.41M cycles/start，Wasmtime 28.47M，约 1.314×；C21 的 instructions 更少仍需更多 cycles，单靠缩短指令数不能证明追平。

三生产仓库另用正常 CLI、guest WASI MONOTONIC 时钟完成 96 个访问计时 cells，无私有钩子、LLVM 缓存关闭；i32-write 三仓库约 0.839–0.842ms，Wasmtime CLI49 约 0.128ms。**WASI 导入触发现有 GC gate5，故这些 96 cells 只能证明访问/codegen 性能，不能证明自动回收性能。** 不带导入的四个生产 Wasm 各仓库另测，共 12 cells，每个都明确完成 12 次回收、reclaimed>0、roots_requested=1、disabled=0。原普通 churn fixture 含未使用异常 tag，会触发 gate9；真正回收验证删除了该无用 tag。异常与自动回收组合准入未在本轮解决，绝不把仅有 allocation churn 的测试冒充回收证明。

VTune 2026.4 硬件 Hotspots 与 Microarchitecture Exploration 各对 C20、C21、Wasmtime 采集两种负载，共 12 份合格采集。独立解码核对原始 PERF、core PMU 的 cycles/instructions 事件、精确目标 PID/TID、用户态 CPU2 和 RAW 时间 ROI；目标 atom/software 样本为 0，PERF lost 为 0，JIT RX 页前后 SHA 相同，另用 perf stats 独立交叉核对。`codegen/hardware-assembly-proof*.json` 关闭早期不可修改记录中的 pending 审计。每种 VM 的采集重复数校准到约 1.2s，不能把样本总数当速度比。某些派生 uarch 百分比为空或缺失，不能把显示的零值解释成“没有 stall”；本文主要采用原始事件、热点归属和另行取得的未缩放固定计数。

实际 RX 汇编证实：C20 调用 `llvm_jit_gc_aggregate_fixed_bridge<14,3>` 再进入 `array_set`；C21 调用专门 borrow 后直接生成 `xchg byte [header+0x20]`、typed store、release store。packed i32 为 dword store；旧 carrier 分支对完整 16 bytes 零扩展并写入，不留下高字节。Wasmtime 相同 Wasm 的实际 loop 直接执行 null/length/bounds/堆范围检查、静态 4-byte scale 和 dword store，没有每次 store 的运行时桥接和对象锁。未声称 Wasmtime 所有长度检查已移出循环，测到的 loop 仍存在检查。

Wasmtime C API/DSO 与官方 CLI 都是 49.0.2；分析源码固定为 `3c8a3e79aa3188a1a12b3aebbaa097abccceac92`，不是本机其他 dev HEAD。`func_environ/gc.rs` 的 translate_array_set→array_elem_addr→write_field_at_addr 使用静态类型布局，translate_array_len 是只读 load；源码摘录及原文件 SHA 在 `bindings/wasmtime49/`。四种新模块均保存对应 CLIF/cwasm/汇编，并以运行时 RX 页证明测到的实际 JIT 路径。

两个 llvm-mca 输入来自实际 x86-64 RX/store block：UWVM 9 指令、100 次模拟 322 cycles、RThroughput 2.0；Wasmtime 28 指令、520 cycles、RThroughput 4.5。**块范围不同**：UWVM 块排除了认证、边界与锁，Wasmtime 块包含边界检查；这是局部调度辅助，不能由此声称 UWVM VM 更快。MCA 不模拟回收、缓存一致性、真实分支和并发。

纯访问中 C20 的热点 array_set/aggregate bridge 消失；C21 主要落在 JIT 与 authenticated borrow。压力负载中的原生热点转为 allocate_general_slab、publish、collect_exclusive_aggregate_domain、reserve_numeric_slot_locked 和 struct_new_small。两条剩余优化路线有实测依据：在证明对象生命周期/foreign owner/共享语义的前提下减少认证与逐对象锁；把已证明类型与根信息推进分配及 publication IR。未经该证明直接删除锁或把 opaque token 当原始指针，会违反现有契约。Wasmtime DRC 不回收循环，不能通过删除 UWVM 循环回收能力换取不等价的“胜出”。

本轮原生正确性累计 1816 个执行 cells（含预期陷阱与两个既有非法模块的拒绝），并非 1816 种独立算法：Wasmtime oracle 100、私有 379+36、三生产主测试 1137、三生产 foreign/分立 feature 108、新 foreign store 36、私有实际回收配对 8、生产实际回收 12。新 100 个模块检查截断、符号/无符号、NaN 原始位、长度 1/2/257、alias、new/default/fixed、子类型、copy/fill、EH 后继续、null/empty/length/UINTMAX。foreign 测试检查跨模块 owner 类型布局，存活数组跨真实 collection 另行证明。debugger 四类型各三次 restart，私有 12 加三生产 36，共 48；验证查看、写原始位、错误类型/越界拒绝、继续执行 Wasm 断言。debug/checkpoint 模式仍走原受检查回退；本轮没有 checkpoint 恢复、parallel GC、QEMU 或跨架构 native 验证。20 个分立特性模块是代表性测试，线程模块仅检查原子语义，不是多线程 GC 压力测试。旧 carrier 分支获得汇编级证明，但 mixed-profile foreign legacy binary 未做动态证明。

所有编译、原生运行、VTune 与 MCA 均在 SSH Linux 的唯一 64GiB cgroup，swap.max=0，CPU2 P-core；入口与每个运行收据保存启动 boot/cgroup/affinity 和内存 guard。准入测试<40GiB、构建<32GiB，运行撤销阈值 48GiB、host available 8GiB，不等待 CPU 空闲。只清理已备份的本任务中间对象、迁移已关闭自有源码、对已持久化自有文件建议释放 page cache；没有重建第二个 64GiB 组。用户新批准的一次共享 12GiB reclaim 没有执行，资源记录保留执行状态。memory.events 的 max/oom/oom_kill 均为 0。

`C21-NATIVE-REPORT.json` 是机器可读最终索引；`evidence/` 保留完整接受收据和对应原始 stdout/stderr 的 base64 字节、测试结果和计数；`profiles/` 保留采集/验证/RX/固定计数数据，完整 PERF 与 ELF 留在 Linux 的持久目录避免三仓库重复大文件；`controls/` 保留与 boot、cgroup、绝对路径绑定的原生控制脚本，重启后必须重新核对环境，不能直接在 Mac 运行。旧 279 个测试与保留工作负载可按 Wasm SHA 在相邻 C20 bundle 找到，新增及 foreign/feature fixture 的可移植路径在 fixture-manifest.json。早期失效 EH fixture、worker whitelist、带 tag 的 GC 准入、旧 bridge 绑定尝试不计入合格结果。

Intel 硬件采集方法参考 [Hardware Event-Based Sampling](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/hw-event-based-sampling-collection.html) 与 [Microarchitecture Exploration](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-1/general-exploration-analysis.html)；本轮版本与实际采集参数以冻结 spec 为准。
