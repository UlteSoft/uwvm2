**C22：把本地数值 GC 数组认证放进 LLVM IR，三个仓库已同步并完成原生验证。**

在已有 LLVM JIT 配置上启用 `xmake f --gc-jit-access-profile=typed-array-access-inline`。这是 Linux x86_64 Clang/int128 的实验 profile；default、typed-array-load、typed-array-access 保留。六宏须在每个 TU 一致，并完整重编译。三个生产构建使用各仓库完整当前源码、三个 TU、LLVM verifier 和相同六宏，没有私有计时钩子。缓存源标识取自完整冻结源码清单 SHA，不使用 unsafe provenance/signature escape hatch。

新路径每次函数激活获取一次稳定的 owner/table 索引域；每次访问对 opaque token 进行与原算法相同的 xorshift17/9 哈希，从 acquire 发布的本地桶头走最多八个可信节点，再检查 owner/type/kind。精确本地对象直接进入已有 typed load/store；foreign、子类型、错误引用和深碰撞保留原认证桥接。guest token 从未作为地址解引用。GC 不替换索引表的分配，故索引域可跨 safepoint；对象指针只供紧随其后的单次访问，不能跨调用、debugger 或 safepoint。mutable 数值对象的 acquire/release 锁保留；边界检查先于持锁区，持锁区没有桥接、callback 或 trap。

198 个访问内部计时 cells：同一 Wasm、三 VM、三个平衡轮次、五次 POSIX 样本，排除 JIT 和两个预热 batch，包含 VM 入口和构造。C21/C22 六项工作指标相同。下表为每次 start 中位数，ratio<1 表示 C22 更快。

| Wasm | C21 ms/start | C22 ms/start | Wasmtime ms/start | C22/C21 | C22/Wasmtime |
| --- | ---: | ---: | ---: | ---: | ---: |
| i8-write | 0.766499 | 0.752516 | 0.112219 | 0.9818 | 6.7058 |
| i8-write-read | 1.663461 | 1.265795 | 0.197851 | 0.7609 | 6.3977 |
| i16-write | 0.756642 | 0.756355 | 0.127174 | 0.9996 | 5.9474 |
| i16-write-read | 1.609295 | 1.220367 | 0.214120 | 0.7583 | 5.6994 |
| i32-write | 0.931686 | 0.921534 | 0.135987 | 0.9891 | 6.7767 |
| i32-write-read | 1.794562 | 1.405406 | 0.230405 | 0.7831 | 6.0997 |
| f32-write | 0.914177 | 0.899525 | 0.122560 | 0.9840 | 7.3394 |
| f32-write-read | 1.762519 | 1.428316 | 0.241258 | 0.8104 | 5.9203 |

四种写后读取耗时下降19–24%，纯写入基本持平。44个另行成组未缩放固定计数 cells 使用 VTune 随附 amplxe-perf 的 core2 user cycles/instructions/ref-cycles：READY→enable ACK→GO→DONE→disable ACK，全部有效 ROI≥0.5s，相同 workload/reps，running≥99.99%。少量用户态协议序列化进入计数，内核和阻塞时间不进入。

i32-write-read 的 C22/C21 cycles=0.787667、instructions=0.469657；i32-write 分别为0.992913/0.494190。纯写入少了一半指令，却几乎没有同比减少 cycles，不能由指令数下降推断等比例加速。tailcall cycles/instructions=0.999867/0.999902，func-reference initialization=0.987759/1.000007，原快项保留。首轮短 i8 pilot 的变慢未在平衡计时复现，pilot 不进入最终比率。

另有36个真实 GC 压力计时 cells：131072次写存活数组→分配并丢弃 struct→读数组，末尾检查1024槽位。每个 UWVM sample 验证 collections/reclaimed>0、根启用、disabled=0，六项工作量相同；Wasmtime 协议没有对应回收次数，不能声称它们的回收次数相同。

| GC 压力 | C22/C21 时间 | C22/Wasmtime 时间 | C22/C21 cycles | C22/C21 instructions |
| --- | ---: | ---: | ---: | ---: |
| i8-write-read-gc-pressure | 0.9283 | 1.2569 | 0.9464 | 0.9068 |
| i16-write-read-gc-pressure | 0.9621 | 1.2727 | 0.9592 | 0.9096 |
| i32-write-read-gc-pressure | 0.9642 | 1.2383 | 0.9491 | 0.9069 |
| f32-write-read-gc-pressure | 0.9301 | 1.1666 | 0.9595 | 0.9069 |

真实 GC 压力下时间改善3.6–7.2%、cycles改善4.1–5.4%，仍比 Wasmtime 慢16.7–27.3%；i32 cycles约1.241× Wasmtime。普通数值读写仍约5.7–7.3×，reference read约14.5×，本轮尚未全面追平。

VTune2026.4 硬件 Hotspots 和 detailed Microarchitecture Exploration 对 i32-write-read、i32-write-read-gc-pressure 的旧/新/Wasmtime 各采集一次，共12份。独立解码原始 PERF、core PMU cycles/instructions、目标 PID/TID/user/CPU2/RAW ROI，再用 perf stats 核对记录总数；目标 atom/software样本=0、lost=0，actual RX前后SHA相同。每个 VM 按约1.2s校准不同reps，采样总量不能当作速度比；空或缺失的派生 uarch 百分比不能解释成“没有stall”。访问 HS 中旧 borrow 有可见热点；新版1642个目标样本中1639个在JIT，uarch为549/549。真实压力热点仍在 collection、publication、slab allocation。

实际 RX 汇编与 native ELF/PIE 地址已绑定：索引域 getter 在入口调用一次，LLVM 已把固定 token 的哈希移出循环；循环仍重新 acquire 读取桶头、检查身份/布局，再执行 `xchg byte [object+0x20]`、元素访问及 release store。八层检查被 LLVM 展开，冷分支保留完整 borrow call。GDB 普通桶头/非桶头均为domain=1,borrow=0；深碰撞为1/4；collection-domain为1/0。三个生产构建另在签名缓存命中的 fresh process 中验证 getter=1、borrow=0，真实48次GC与guest rawbits断言通过。“没有借用桥接调用”的动态结论限定这些具体cells。

Wasmtime49.0.2 C API/DSO 与匹配 CLI，分析源码固定为3c8a3e79aa3188a1a12b3aebbaa097abccceac92。它以静态数组类型布局及readonly length load生成堆/长度/边界访问，没有UWVM的opaque membership链和逐对象读写锁。匹配 write-read、GC-pressure Wasm 的 CLIF/cwasm/汇编在codegen中；本机dev源码也保持这个布局方向，性能数字只绑定49.0.2。剩余访问差距有实际汇编依据：membership检查、锁、packed/legacy stride分支、根和寄存器spill仍在热路径。真实压力仍需优化 allocation/publication/collection；单换memcpy不能消除这些路径，删除锁需要另外的线程及生命周期证明。

llvm-mca 使用实际 RX 的 exact bucket-head 单次write路径：UWVM43条/100次1152模型cycles/RThroughput10.0，Wasmtime31条/510/RThroughput4.7。条件分支保留，静态模型不执行实际控制流，也不模拟锁coherence。范围排除碰撞、getter、分配/GC、完整loop backedge，两种VM契约不同，这不是VM速度比。更小的两指令access-only模型另存，不能替代完整路径分析。

正确性1944个执行cells（含预期trap和既有非法模块拒绝）：私有379、新32模块×旧/新/Wasmtime96、私有foreign/features36、新foreign store12、真实GC配对8；三生产411×3=1233、foreign/features108、新foreign store36、真实GC12、签名cache miss/hit24。它们不是1944种独立GC算法。新32覆盖i8/i16/i32/f32、桶头/非桶头/深碰撞、真实GC前后、嵌套activation、首次认证位于未执行分支、reference PHI、精确类型/子类型混合；raw NaN、alias、null/越界/EH继续等保留测试通过。debugger共48次restart：查看数组、写raw bits、错误类型/范围拒绝、继续guest断言；GDB实际路径另7cells。24个签名缓存cells=12真实miss+12hit，源/ABI语义fingerprint隔离新IR，签名验证保持开启。

三生产另完成96个正常CLI guest WASI MONOTONIC访问计时cells，无benchmark hooks；这是另一协议/构建，不能直接与私有时间作优化前后比。另以三个实际生产审计确认 WASI 导入触发 GC gate5（disabled=1、collections=0），故这些96cells仅证明访问性能。真正回收验证独立使用无导入/无tag Wasm，三生产每种型完成12次collections；cache collection-domain是较长200000分配fixture，完成48次。gate5和exception人口gate9没有在本轮解决。20个分立Wasm特性是代表性正确性检查，threads只测原子语义；本轮没有 parallelGC、checkpoint恢复、QEMU、非x86架构资格或每种Wasm3特性的完整性能矩阵。debug/checkpoint/lazy/tiered/pendingnumeric继续受检查回退。

所有编译、运行、VTune及MCA在SSH Linux唯一64GiB cgroup，swap=0。测试<40GiB、构建<32GiB准入；运行48GiB或host available<8GiB撤销；CPU2 P-core，不等CPU空闲。memory.events max/oom/oom_kill=0。批准的一次共享12GiB file-cache reclaim现已执行且耗尽授权（swappiness=0，无部分结果重试）；旧C21报告“尚未执行”是历史记录，本轮resources收据才是当前状态。不能把共享用量整个下降归因于reclaim，其他进程也同时释放内存。仅清理已持久化ELF和完整source之后关闭的自有.o。完整PERF/ELF/源码压缩包保留Linux持久目录，避免三仓库重复大文件。

两个失效fixed窗口（Wasmtime<0.5s、外部采集器scope未证）及选错debug-wrapper的GDB尝试单独置rejected，不计入结论；修正后的直接native symbol GDB已通过。早期不可修改hardware聚合中的pending字段由codegen/hardware-assembly-proof两个最终索引关闭。

C22-NATIVE-REPORT.json为最终机器索引。evidence有接受收据及原始stdout/stderr字节；profiles保留ROI/RX/验证/固定计数；specs/controls绑定本次boot、cgroup和绝对路径，重启必须重新检查。旧279与C21新增100模块可按SHA在相邻C20/C21 bundle查找。bindings保留三份源码清单、真实构建argv/ELF SHA和8个改动文件；private33个实验宏与production6宏明确区分。旧C20/C21数据保持原样。

硬件采集方法参考[Intel Hardware Event-Based Sampling](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/hw-event-based-sampling-collection.html)和[Microarchitecture Exploration](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-1/general-exploration-analysis.html)。具体版本、参数、目标和事件以冻结spec及原始记录为准。
