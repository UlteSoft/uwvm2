# Wasmtime 49.0.2 中间 IR、实际汇编与 GC 访问定位

本轮把分析绑定到了与实测库相同的 Wasmtime 49.0.2：直接调试 C API 执行的 JIT，导出同版本 CLI 的 CLIF/机器码，并新增引用读写和数值写后读测试。最明显的剩余差距在引用访问：C20 的类型化数值读取没有覆盖引用数组及其后续 struct 字段访问，热点仍在通用桥接和对象认证路径。本轮新增分析、测试和证据，没有新增生产 GC 算法修改，也没有达到 Wasmtime 的整体 GC 性能。

全部原生编译、执行、GDB、VTune、LLVM-MCA 均在 SSH Linux 原有的唯一 64 GiB cgroup 中完成，swap 上限为 0。性能目标是 i9-14900HX 的 P-core 2；保留既有 40/48 GiB 测试准入/终止及 32 GiB 构建准入，没有等待 CPU 空闲，没有创建第二个 64 GiB 组、修改内核采样率或回收共享缓存。最终检查 memory.events 的 max/oom/oom_kill 均为 0。

用户指示的 `/home/macromodel/Documents/src/wasmtime` 是干净的 46.0.0，HEAD `b4b23fe583173131a047141947a785b4c1c646ae`，不能当作实测 49.0.2 的源码。它保持未修改。本轮在 cgroup 临时目录下载 [官方 v49.0.2](https://github.com/bytecodealliance/wasmtime/releases/tag/v49.0.2) CLI 和同标签源码 `3c8a3e79aa3188a1a12b3aebbaa097abccceac92`。CLI 压缩包 SHA256 与官方 asset digest 一致；实际测量的 C API 库 SHA256 为 `e8b92b84cafbd553a8fe1c96ddff1673c3f252849b033b63b11b573ca4e5912e`。

**中间 IR 与执行字节。** 导出了 12 个独立 Wasm 的 O2 CLIF/汇编/序列化对象，另导出两个 f32 模块的 O0 结果，共 14 组。CLI 参数为 `-C collector=drc,cache=n,parallel-compilation=n,cranelift-debug-verifier=y --emit-clif ...`，优化级别另以 `-O opt-level=2` 或 `0` 设置。`emit_clif` 位于编译之后，输出的是编译器已处理的 IR；O0 是另一个优化设置，不能称为精确的“优化前 IR”。

f32 immutable/mutable 两份 O2 输出都包含同一个 124 字节读循环，与先前经过 VTune 核验的 C API 执行字节完全相同。GDB 在实际 C API 的 `vmovss` 前停下，验证了：

```text
length = 1024
GC 引用 = 32 位堆偏移 16
元素地址 = 堆基址 + 对象偏移 + 28 + index * 4
元素位模式 = 0x7f800001
```

单步读取后 xmm0 仍包含相同的 sNaN 位模式。长度字段为对象偏移 24；长度加载在 CLIF 中标为 readonly，但这份实际优化后的循环仍逐次加载长度，并保留元素范围、对象尺寸及溢出检查。不能把优势归结为“所有长度加载都已移出循环”。关键是布局、检查和真实加载直接出现在编译器 IR 中，数值读取没有逐元素运行时桥接和对象原子锁。对应源码为 [translate_array_len / array_get](https://github.com/bytecodealliance/wasmtime/blob/3c8a3e79aa3188a1a12b3aebbaa097abccceac92/crates/cranelift/src/func_environ/gc.rs#L901)。

**引用读取与写入。** 引用数组读取先直接加载堆引用，再检查对象头的近似根集合成员位。GDB 在实际指令 `test eax, 2` / `jne` 上验证：成员位已置位时跳过插入和增引用计数，计数保持 **1025 → 1025**。成员位未置位的路径才连接侵入链表、置位、增计数，并按阈值进入 GC。普通局部引用复制和已经在集合中的重复读取不必每次做引用计数增减。

同值 `array.set` 的实际机器码仍有计数写入：GDB 验证 **1025 → 1026 → 1025**，顺序为增加新值计数、写目标槽、减少旧值计数。这保证 `old == new` 时不会先降到零而误释放。这里使用普通 64 位堆加载/存储；这与 Wasmtime 的独占 Store / 非共享 GC heap 契约有关，不能直接删除 UWVM 中承担另一套并发契约的锁。源码为 [DRC read/write barriers](https://github.com/bytecodealliance/wasmtime/blob/3c8a3e79aa3188a1a12b3aebbaa097abccceac92/crates/cranelift/src/func_environ/gc/drc.rs#L495)。

**分配与回收算法。** Wasmtime 的数组构造仍调用 `gc_alloc_raw`，随后在 JIT 中初始化长度和元素；并非所有分配都已内联进 JIT。实际库的 `DrcHeap::alloc_raw` 内联了 FreeList bump 快路径：16 字节对齐、检查加法溢出及 `new_ptr <= bump_end`，成功就更新指针，耗尽才查按长度/地址组织的 BTree 索引。GDB 捕捉到初始空区域的两次入口，随后在有效区域中验证请求 4124 字节、对齐 4128 字节，`bump_ptr` 从 **16 → 4144**，边界为 65536。慢路径断点“0 次”仅覆盖这次有效快分配之后，不能据此声称冷启动没有慢路径。

对应 [FreeList](https://github.com/bytecodealliance/wasmtime/blob/3c8a3e79aa3188a1a12b3aebbaa097abccceac92/crates/wasmtime/src/runtime/vm/gc/enabled/free_list.rs#L214) / [alloc_raw](https://github.com/bytecodealliance/wasmtime/blob/3c8a3e79aa3188a1a12b3aebbaa097abccceac92/crates/wasmtime/src/runtime/vm/gc/enabled/drc.rs#L1066) 中，常见分配不会每次查整棵空闲块索引。对实际六条 bump 算术指令做了 LLVM 23 RaptorLake MCA；模型仅用于依赖/端口分析，不包含调用、分支概率、缓存和一致性，也不能把结果称为完整分配耗时。

DRC 在安全点通过精确栈图标记当前栈根，再修正近似根链表，对不再被引用的对象减计数并通过显式工作栈递归释放子对象，使用缓存的类型追踪信息。它不等同于 UWVM 的全图可达性标记/回收候选扫描。49.0.2 明确写明 [DRC 不能回收循环垃圾](https://github.com/bytecodealliance/wasmtime/blob/3c8a3e79aa3188a1a12b3aebbaa097abccceac92/crates/wasmtime/src/config.rs#L3589)，因此不能照搬后牺牲 UWVM 的循环回收能力，也不能用无循环读取测试证明回收器总体胜负。

**新增性能结果。** 每次 `_start` 有 131072 次主循环读/写，写用例另校验最终 1024 个槽位。三个交错轮次、每格五个内部 POSIX clock 样本、每样本 32 次 `_start`，共 36 格；排除 JIT、首次调用与两次预热批次，包含 VM 进入/退出及构造。C19/C20 六项工作指标一致。以下使用原有私有性能实验 ELF，不能归因给仅开启四项宏的三个仓库集成构建。

| 用例 | C19 ms/start | C20 ms/start | Wasmtime DRC ms/start | C20/Wasmtime |
|---|---:|---:|---:|---:|
| 引用数组读取 + struct.get | 2.1864 | 2.1687 | 0.1611 | 13.46× |
| 同值引用覆盖 + 读取 | 3.7433 | 3.6963 | 0.3706 | 9.97× |
| 两个引用交替覆盖 + 读取 | 3.6817 | 3.6220 | 0.3264 | 11.10× |
| 数值 i32 写后读 | 1.9965 | 1.8663 | 0.2247 | 8.30× |

C20 的数值写后读比 C19 减少约 6.5% 时间，三个引用用例只变化约 0.8–1.6%；当前优化尚未覆盖它们的主要路径。每个运行时都执行完全相同的 Wasm 和校验断言；这些结果定位访问开销，没有强制回收、循环垃圾或并行 mutator，不能解释成纯回收器速度比。

**新的硬件证据。** 对引用读取完成 C20 / Wasmtime 的硬件 Hotspots 和 detailed Microarch，共四份；均验证目标 PID/TID、user mode、P-core 2、原始时钟 ROI 超过 500 ms、RX 前后字节一致和零丢样。独立 `amplxe-perf report --stats` 核对了总采样记录数，再以 event-ID/attr 验证所有计入目标的记录来自 cpu_core，cpu_atom 和软件 DUMMY 的目标样本为零。为满足采样时间，两个 VM 使用分别校准的重复次数；不能直接用原始记录总数作速度比。

UWVM 的三个主要热点为 `array_get`、`llvm_jit_gc_aggregate_fixed_bridge<11,2>` 和 `llvm_jit_gc_struct_get32_bridge<false>`。实际 array_get 为 661 字节，通用包装为 102 字节，struct 桥接为 339 字节；不能把短包装当作全部 getter 成本。按 PIE 的实际加载基址绑定后，`array_get` 的 `xchg` 在前 100 个热点 PC 中有 172 条混合事件记录；它们不是锁等待次数。该 fixture 的数组可变，struct 字段不可变；struct 桥接静态代码中有可变字段锁分支，但本例没有证据表明执行了那条分支。Wasmtime 同一引用成员检查块与 CLI、GDB 和两份新硬件采样的 RX 字节相同。

这些 CSV 的 CPU Time/CPI/TMA 是采样估计，混合事件计数、skid 和多路复用都会影响细粒度百分比；不将父子 TMA 指标相加，也不将 Lock Latency 当成精确锁时间。原始 PERF 保留在 Linux 持久目录，紧凑包保存哈希、独立计数核验、CSV、映射、实际执行字节及反汇编。

**据此确定的实现顺序。** 首先补齐类型化 `array.get ref` 与 `struct.get` 的 JIT 访问，让类型布局、检查、引用结果及根发布出现在 IR 中；引用必须在下一安全点前正确进入 UWVM 根帧。其次补齐类型化数值 `array.set`，再按 UWVM 自己的追踪 GC 写入/根发布协议设计引用写入，不能盲目附加 DRC 引用计数。之后在可证明的安全点区间内复用对象认证，并以明确的独占执行/变更协议消除不必要的逐元素锁。当前改动入口是 `single_func_gc_emit.h`、`single_func_gc_typed_array_load.h`、`gc_object.h` 及其 typed borrow helper。

上述实现必须保持 null/bounds/trap 顺序、foreign owner/subtype 的认证与保活、异常清理、调试修改和 checkpoint 根发布。Opaque token 不能直接转成对象地址，借用指针不能越过调用/GC/调试回调；没有所有权证明时不能给认证桥接添加 readonly/noalias，也不能删除共享访问锁。新路径还需覆盖引用/空值/同值写入/子类型/跨模块与外部别名/循环图/强制回收/异常根/并行访问/调试修改/checkpoint，并核对分配量、存活量、回收量、根与边扫描量、collection 次数和暂停分位数。本轮的访问平均时间不替代这些 GC 指标。

**正确性与复查。** 12 个用例在官方 CLI O2 通过，两个 f32 O0 通过；四个新增用例在实测 C API 和三份集成 C20 full-JIT ELF 上通过，共 30 项执行校验。集成构建沿用上一轮已验证的三仓库快照，本轮没有重建生产源码，也没有把私有性能实验数据当作这些集成构建的性能。其他架构、QEMU、真实并行 GC、完整 checkpoint、复制型 Wasmtime GC、其他 VM 及 Advisor 不在本次新增验证内。

- `evidence/WASMTIME-CODEGEN49-NATIVE-REPORT-a1.json`：版本、环境、计时、限制与证据哈希。
- `evidence/codegen-matrix-a1.json`、`codegen/`：14 组 CLIF、汇编和序列化对象；绝对路径指向 Linux 原件。
- `fixtures/`：四个新增 Wasm/WAT；另外八个数值读取用例在上一级 `read-lock/`。
- `gdb/`：四项成功 GDB 观察、执行字节和断点脚本；计时输出仅用于证明正常结束。
- `evidence/profile-assembly-proof-c20-a2.json`、`profiles/`、`assembly/`：新 PMU、实际 ELF/RX 汇编、PIE 地址绑定和 MCA。
- `receipts/`、`control/`：执行记录和远端复查脚本。脚本依赖既有 boot/cgroup/resource guard，只用于 Linux 恢复后的受限环境，不能直接在本机运行。

保留了两个配置/调试初次失败：GC 功能开关遗漏导致 CLI 拒绝解析，补齐既有 feature flags 后通过；最初分配断点停在尚未建立有效 bump 区域的入口，修正为捕获初始化后的真实快分配。PIE 地址统计以 a2 为准，`wasmtime-reference-native-binding-a2.py` 已重新生成并核验同一结果。原始拒绝记录没有删除。

Linux 工作目录 `/tmp/uwvm-gc-perf-20261011-r74/source/wasmtime-codegen-analysis-a1`，持久副本 `/home/macromodel/Documents/uwvm-gc-perf-20261011-r74/recovery/source/wasmtime-codegen-analysis-a1`。本紧凑包和新增四个 fixtures 同步到 uwvm2、uwvm2-stable、uwvm2-ros，原有 C20 证据和源码不覆盖。文件完整性见本目录 `bundle-index.json`。
