C23 已把 LLVM JIT 数组访问进一步专门化，并同步到 uwvm2、uwvm2-stable、uwvm2-ros。认证成功的本仓库精确类型 i32/f32 数组直接采用编译期已知的 4 字节元素布局，避免每次读取表示标志、动态步长选择和状态分派。i32/f32 纯写入的同工作量硬件 cycles 分别下降 23.84% / 23.78%，instructions 下降约 16.21%。GC 压力改善小得多，尚未达到 Wasmtime 的整体 GC 性能。

这份目录是三个仓库完全一致的证据包。`C23-NATIVE-REPORT.json` 汇总结果；`MANIFEST.json` 校验所有其他文件。正式源码改动只有 `single_func_gc_inline_array_auth.h`、`single_func_gc_typed_array_load.h`、`single_func_gc_typed_array_store.h` 和 `llvm_jit_cache/environment.h`；各仓库四个文件的精确 SHA 和已测 Linux 源码快照绑定在 `bindings/`。其他本地工作区改动不属于本轮优化。三仓库冻结快照都编译了全部 3 个 TU，并各通过 491 个检查。

实际开发依据是本机 Wasmtime 的 `emit_array_size_info`、`array_elem_addr`、`translate_array_get/set`：使用编译期数组布局生成元素地址。设计参考 HEAD 与测量的 Wasmtime 49.0.2 源码、DSO 分别绑定，不能把开发分支描述为测量版本。UWVM2 的外国 store、子类型和旧 carrier 表示继续走认证桥接与实际布局；只有有界索引认证确认 owner、类型和 kind 都精确匹配的本地对象获得静态布局。两个分支完成操作后才合并值，LLVM 因此能把快分支生成成 `mov dword ptr [rdx + 4*rcx], r15d`。

初版曾专门化全部宽度，i8/i16 写读的 cycles 增加约 10.10% / 8.22%，尽管 instructions 更少。实际 RX 汇编显示活跃区间改变和循环状态溢出，因此拒绝该版本，只保留 32 位特化。最终 i8 写读在重定位常量归一化后整段汇编完全相同，instructions 相同；i8/i16 保持原有合并布局路径。i16 写读某次 cycles 下降 8.2% 但 instructions 相同，不归因于这次源码优化。

所有构建、Wasm、GDB、VTune、llvm-mca 均在 SSH Linux 的同一个 64 GiB cgroup 内执行，swap.max=0；唯一性及 memory.events 有记录。控制程序绑定 CPU20，计时和硬件 ROI 绑定 P-core2，正确性和 GDB 用 CPU6，构建用 CPU4/8。保持 32 GiB 构建准入、40 GiB 测试准入、48 GiB 撤退门槛；没有 CPU 空闲准入。JIT 和两轮预热排除在计时外，构造、VM 调用边界及测量 Wasm 本身的工作包含在 ROI 中。

下表时间是私有、相同 33 个实验宏的 C22/C23/Wasmtime DRC 矩阵：22 个工作负载 × 3 VM × 3 轮平衡顺序、每次 5 个内部 POSIX 时钟样本。cycles / instructions 来自相同 Wasm 和相同重复次数的用户态目标线程固定计数器，不缩放，PMU running 至少 99.99%。时间和 cycles 会受频率与同机共享资源影响，不能互换。


| Wasm | C22 ms/start | C23 ms/start | Wasmtime ms/start | C23/C22 时间 | C23/C22 cycles | C23/C22 instructions |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| i8-write | 0.732442 | 0.731023 | 0.103191 | 0.9981 | 0.9999 | 1.0000 |
| i8-write-read | 1.185649 | 1.187747 | 0.185512 | 1.0018 | 0.9994 | 1.0000 |
| i16-write | 0.712390 | 0.688819 | 0.121906 | 0.9669 | 未采集 | 未采集 |
| i16-write-read | 1.206001 | 1.211769 | 0.203743 | 1.0048 | 0.9178 | 1.0000 |
| i32-write | 0.881708 | 0.655061 | 0.128572 | 0.7429 | 0.7616 | 0.8379 |
| i32-write-read | 1.451698 | 1.307067 | 0.234901 | 0.9004 | 0.9834 | 0.8574 |
| f32-write | 0.950722 | 0.718901 | 0.124695 | 0.7562 | 0.7622 | 0.8379 |
| f32-write-read | 1.446767 | 1.407969 | 0.247959 | 0.9732 | 未采集 | 未采集 |

| GC 压力 | C23/C22 时间 | C23/Wasmtime 时间 | C23/C22 cycles | C23/C22 instructions |
| --- | ---: | ---: | ---: | ---: |
| i8-write-read-gc-pressure | 0.9751 | 1.2069 | 0.9957 | 1.0000 |
| i16-write-read-gc-pressure | 1.0013 | 1.2289 | 0.9949 | 0.9999 |
| i32-write-read-gc-pressure | 0.9683 | 1.0742 | 0.9834 | 0.9891 |
| f32-write-read-gc-pressure | 0.9713 | 1.0870 | 0.9838 | 0.9891 |

GC 压力包含实际自动回收，旧、新 UWVM 的六项工作指标相同。i32 C23 的同工作量周期数为 34,739,889/start，Wasmtime 为 28,541,579/start，仍相差 21.72%；C23/C22 cycles 改善 1.66%。该 workload 的时间差约 7.42%，不把时间与周期差混为一谈。单独的 i32 写读访问同工作量 cycles 仍约为 Wasmtime 的 6.15 倍；引用数组 read 时间仍约 14.78 倍。这些是后续优化对象，不能宣称整体 GC 已超越。

18 份 VTune 硬件 Hotspots / Microarchitecture Exploration，分别覆盖 i32 写入、写读、实际 GC 压力，每项旧、新、Wasmtime 各两种分析。原始 PERF attr 的 PMU type、event ID、RAW ROI、PID/TID、user PCs、实际 RX 前后校验都已匹配：目标为 cpu_core 的 cycles/instructions，cpu_atom 和 software 目标样本为 0，PERF 丢失为 0。`profiles/*/validation-a3/` 存放计数域验证和 CSV；`codegen/` 存放对应实际执行内存生成的汇编，`bindings/vtune-raw-disk-manifest.json` 给出原始 PERF 文件的持久化路径和 SHA。不同 profile 单独校准重复次数，采样总数不能直接相除推算加速。

汇编还显示 UWVM2 每次访问仍走有界对象成员查找、owner/type/kind 检查和 `xchg` 原子读取/写入锁，Wasmtime 对应普通 GC 数组路径无需这些逐对象锁。GC 压力 Hotspots 的 allocate_general_slab、publish、collect_exclusive_aggregate_domain、retire_slab_allocation_locked 等运行时工作仍显著。这里没有删除同步来换取跑分。精确根与对象布局生命周期仍需要跨 safepoint 保证，任何进一步跨操作缓存对象地址或移除锁都要证明线程和回收协议正确。

llvm-mca 使用硬件采样对应的实际 RX 汇编，选取 head-success 循环路径，保持所选指令和分支但重写目标标号；不纳入碰撞链、桥接、异常和分配。Alder Lake P-core 模型下，C22/C23/Wasmtime 分别为 41/35/32 条所选指令、58/51/31 uOps、block throughput 9.7/8.5/5.2。Raptor Cove 近似调度模型不模拟真实锁一致性、缓存失效、分支行为或整体 VM，因此不是 native 性能结论。`codegen/static32-mca-a2/` 绑定每条指令的原始地址、RX SHA 和运行收据。

验证共 2,424 次执行检查（771 私有矩阵，1,653 三仓库生产快照），包含既有 279 项、100 项 typed-store、32 项索引/回收域、80 项不同类型/可变性/长度的边界与原始位测试，外国 store、子类型、null/bounds trap、浮点 NaN 原始位、实际存活对象回收与 signed-cache miss/hit。48 次调试重启验证查看、合法修改、非法修改拒绝与后续 Wasm 读写，7 组 GDB 验证 head/non-head/deep-fallback/回收和三个仓库的真实缓存命中快路径。检查次数不是独立模块数量。

20 项隔离特性包含 MVP、mutable-global、sign-extension、nontrapping conversion、multi-value、reference types、typed function references、table64、tailcall、GC、exnref exception、memory64、multi-memory、bulk-memory、threads atomic semantics、branch hints、custom annotation、extended const、固定 SIMD 和 relaxed SIMD。这轮没有完成形式化的全 Wasm3 一致性测试、所有特性性能矩阵、多线程并行 GC、checkpoint 恢复、QEMU 或其他架构性能验证，不能从单线程原子指令测试推导并发 GC 正确性。

正常三仓库 6 宏 CLI 的 96 次 guest WASI MONOTONIC 内部计时单独保留。三个额外日志审计确认导入的 GC 准入 gate5 会关闭自动回收，所以这部分仅证明普通数组访问，不能证明 collector 吞吐。另有实际回收 fixture 每次约 12 次 collection 和 cache 压力每次 48 次 collection，日志断言 reclaimed>0、roots_requested=1、disabled=0。生产 6 宏和私有 33 宏采用不同协议，不能合成优化前后比例。

优化通过现有 `--gc-jit-access-profile=typed-array-access-inline` 显式启用；全部 TU 必须一致使用 PACKED_NUMERIC_ARRAYS、SCALAR_ARRAY_LENGTH、ARRAY_LENGTH_VALUES、TYPED_ARRAY_LOAD、TYPED_ARRAY_STORE、INLINE_ARRAY_AUTH 六项实验宏。默认和其他访问 profile 没有启用该新快路径。缓存指纹升级为 `...static-packed32-v3`，签名缓存重新生成，并在新进程 relocation 后验证真实快路径。发布前仍须执行目标发行配置的完整构建和回归；这里仅为 Linux x86_64 LLVM/int128 优化提供 native 证据。

`controls/` 和 `specs/` 保留复现脚本、原始绝对路径、命令行、源和 ELF 绑定；原始测试路径为 `/tmp/uwvm-gc-perf-20261011-r74`，持久化恢复目录为 `/home/macromodel/Documents/uwvm-gc-perf-20261011-r74/recovery`。恢复完整已绑定源码/ELF/fixtures 后必须先使用 `controls/entry/enter.sh` 进入原有唯一 cgroup，并根据当前 boot 更新准入；不能在 cgroup 外直接运行。accepted-native-receipts/logs.jsonl 保存收据和其 SHA、原始输出的 base64；验收的预期 trap 可产生非零退出。若一些沿用的 scope 文本/打印标签仍写 C21，汇总的实际 ELF SHA、variant 与命令绑定明确对应 C22 versus C23-static32。rejected 目录记录初版全宽特化及资源撤退，均不计入最终通过数。

`/tmp` 为 tmpfs，其 shmem 占用不能简单当成可回收的磁盘 page cache。这轮仅对本任务已关闭文件给出缓存 advice，并把 SHA 验证已有持久化副本的历史 VTune raw 常规文件移出 tmpfs，共约 2.10 GB；保留索引、派生结果和 symlink。新共享 cgroup 12 GiB 回收提案没有执行。最终私有和三生产构建自有中间对象清理有收据，ELF、源快照和本轮原始采样保留。没有创建额外 64 GiB cgroup，没有更改门槛，也没有对其他任务文件执行清理。
