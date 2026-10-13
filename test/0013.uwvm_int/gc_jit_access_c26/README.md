本轮 C26 LLVM JIT 数值数组优化

新增 xmake 选项 `--gc-jit-access-profile=typed-array-access-private`（实验性，Linux x86_64 LLVM JIT），统一开启七项访问宏。已同步并原生构建测试 uwvm2、uwvm2-stable、uwvm2-ros；默认配置保持不变。

资格证明使用已经验证的原始 opcode/type，整个函数结束才封定。仅当前 activation 的检查后数组构造事实可消除数值访问锁；循环后的全局/表/引用字段发布、调用、尾调用、外部桥接、死分支发布也撤销整个函数资格。未知参数、debug/checkpoint、imports、lazy/tiered/OSR保留原检查。对象引用仍是 opaque token，不能当裸地址，没有跨GC保存地址。

P-core2 固定硬件计数器三轮中位数，每单位为同一Wasm的完整一次调用、排除JIT：

|测试|新版/旧版周期|新版/Wasmtime周期|
|---|---:|---:|
|i32-write|0.1723|0.8887|
|i32-write-read|0.1761|1.0749|
|i8-write-read-gc-pressure|0.8338|1.0749|
|i16-write-read-gc-pressure|0.8324|1.0988|
|i32-write-read-gc-pressure|0.8379|1.0037|
|f32-write-read-gc-pressure|0.8449|1.0063|

纯i32写入比Wasmtime少11.13%周期，读写混合仍多7.49%；GC压力i32/f32接近持平，i8/i16仍有差距。数据绑定冻结的private benchmark ELF及其完整编译宏。三个七宏普通产品构建被正确性、签名缓存和实际RX机器码验证，不冒充相同的测时ELF；查看build bindings区分配置。

22份硬件Hotspots/Microarchitecture全部零PERF丢样；独立解析事件类型cpu_core、目标TID/CPU2、RX字节与原始PERF。旧纯读写Lock Latency约62.5%，新为0。安全private RX无object flag XCHG，publication反例保留。三个普通产品的签名缓存命中RX均通过同样正/反例。周期采样可能skid，不把某IP样本比例作为单指令准确耗时。

汇编显示剩余UWVM逐操作token hash桶查找、owner/type/kind检查、root槽写入；Wasmtime更多使用类型化堆偏移与检查。实际RX success trace的llvm-mca：C26为52指令、11.5模型吞吐周期，Wasmtime为55指令、8.5周期。模型不能表达真实锁一致性、cache miss或分支执行，不代替PMU实测。[LLVM官方说明](https://llvm.org/docs/CommandGuide/llvm-mca.html)，[Intel硬件分析说明](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-1/general-exploration-analysis.html)。

本机Wasmtime HEAD 65abe37529a6bca836efcac1dcb920d5c7890580的crates/cranelift/src/func_environ/gc.rs用于源码分析；实测49.0.2共享库另有SHA/inode/映射绑定。不能称HEAD就是采样库commit。既有source-bound分析保留版本边界。

正确性：各仓库521项，共1563项，含两个期望校验拒绝和指定陷阱消息检查；84次新进程签名缓存miss/hit；36次debug读取/修改/类型范围拒绝；冻结C26a3的16项checkpoint capture/retire/continue和两参与者census。没有整个GC图checkpoint回滚或全架构测试已完成的结论。

未解决：reference-read内部计时14.94倍，reference-write10–12倍，DAG约2.07倍，exnref约1.29倍。数值数组胜项不代表整个GC胜出。DRC与追踪回收的循环垃圾处理和工作量不同。其余22类时钟结果及旧/新UWVM六项工作指标相等验证见报告。

失败保留并排除：MCA抽取器a1/a2进入cold分支，a3完整成功；checkpoint初版fixture API/路径及单CPU两参与者覆盖失败；先前codegen wrapper输出名冲突，child10项证明完整保留但不冒充正常外层receipt。fixed模板留下无关array.len单位文字，计算始终raw_count/reps；报告明确更正单位，原件不改写。

唯一64GiB cgroup、swap0、OOM0，维持32/40/48GiB准入及撤回门槛，无CPU空闲准入，无共享cache reclaim。原始PERF/SQL/ELF保留SSH磁盘并记录SHA；本地只带代码、fixtures、RX汇编、报告和紧凑receipt，控制文件占用。
