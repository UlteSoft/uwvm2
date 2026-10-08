本轮在 uwvm2 和 uwvm2-ros 同步消除了 LLVM observer 检查点数据包的重复清零：私有共享 copier 已逐项写入完整 local 槽、填充和 saved flag，父函数现在只清零 operand/control 后缀。locals-only 捕获不再先清零整个数据包。legacy/resumable 路径仍清零完整 payload 与 flags；类型、初始化检查、观测 heap 配额和完整 LLVM verifier 要求保留。

新增回归入口为 `debug_checkpoint_observer_packet_ir.cc`。它调用实际生产 emitter，验证完整 LLVM module，并由 native MCJIT 或真实目标 Clang/QEMU 执行。C++ 文件、LLVM raw stream 适配、输出及参数解析采用 fast_io 的 native_file、print/println、parse_by_scan。生产修复没有新增 I/O。私有 copier 仍没有 Wasm provenance、DISubprogram、activation 或公开 safepoint；ASM 不得越过 Wasm 生成上下文。

| 本轮验证 | 结果与范围 |
| --- | --- |
| 新 observer native 回归 | 两仓库均通过；四次真实捕获，i32/i64/f32/f64/v128/raw reference carrier、sNaN 位、旧内存填充、locals-only、operand/control、宽到窄复用、无效 flag/null 候选、实际 local 更新与 sentinel |
| 跨架构 observer 组件 | 16 Linux profiles × 两仓库，共 32 项通过；真实 32/64 位、大小端 LLVM IR 生成、目标降低/链接、ELF 与非可执行 GNU_STACK 检查、native/QEMU 执行 |
| 同 fixture 的修复前后 | ROS 单个生产 header 差异的 baseline 与 candidate 均由 native MCJIT 通过；静态读取真实 IR，父函数 aggregate 清零 store 合计 540 → 64 字节，减少 88.15%；helper 自身必需的清零未计入这个指标 |
| legacy 回归 fixture 修复 | 在 emit 前选择真实 native TargetMachine、设置实际 DataLayout，最终交给同一 target 执行；ROS 通过，普通仓库因共享内存压力停止，后续重试被磁盘保护拒绝入场 |
| 新完整产品 | 普通仓库全新编译启动后在 4.36 秒被共享磁盘保留阈值停止；ROS 未启动。两仓库均未完成新完整产品资格验证 |

16 个 profiles 为 x86_64、aarch64、i686、riscv64、ppc64、ppc64le、ppc32、mips64、mips64el、mips32、mips32el、sparc64、loongarch64、s390x、armhf、armel。首轮 MIPS 链接失败来自 fixture 的 memset；改用 LLVM 全局初始填充值与整数 all-ones store 后全部通过，没有为测试添加 CRT helper。这里的 reference 是私有组件中的物理数据载体，没有真实 VM 引用、源码、activation 或 ASM 权限。32 项通过不等于完整目标 C++ emitter ABI、各架构完整 VM/JIT 或语言调试体验通过；检查点仍不支持 executable restore。

旧 legacy fixture 使用默认 DataLayout，当前生产 owner preflight 正确拒绝了它。本轮修复 fixture 以符合真实 native 模块要求，没有放宽生产检查。保留这一失败及内存、磁盘保护停止的全部日志；资源停止、未启动与逻辑失败分别记录。自动 IR 比较脚本也因磁盘阈值未启动，上表的计数来自实际已验证 IR 的只读静态审阅，不构成额外的执行测试。

所有实际编译、native MCJIT、目标降低和 native/QEMU 测试均在 SSH linux 原来的 64 GiB cgroup 中完成。memory.max=68719476736、swap.max=0、cpuset=0,2,4,6,16-31、测试 affinity=16-31。原 PIDFD、birth、boot、UID、exe、RSS、压力、文件、日志及磁盘保护保留。相关资格记录的 memory.events 前后均为 max=162、oom=0、oom_kill=0；没有终止其他 agent 的进程或覆盖其代码。

普通冻结生产源码 ID 为 b60923e445dc5d21647cf36e5c79bdccf8a88123c3648f5cdfabaa924e498d93，ROS 为 10b4db2d10378c9206f24faf4159fc4764d908f6e2812ccd4fe42164a7c2a435。单生产文件 baseline 分别为 2ae458a3a8bc890681d0cc30fb2a985f7534bb9722a11b09f759e901638b3fd2、eb01d6049ada14b8c7c2234006c8ed637718872d8ebb18a421009c79c2db5ab4。最新独立编译的 observer fixture SHA-256 为 2b512f60b7a191b5fa67337bf1fc387fc4171c310a8ba537cfc29acc65815246，legacy fixture 为 d723858d24f03f3999cec91b926efaab9c17a9a582b589dcb707b3df0d836578；测试版本单独固定，不混同初始冻结目录中的旧 fixture。

收尾核对发现并行修改已将两仓库的后缀 aggregate 清零改为 LLVM memset（当前 header SHA-256 为 4a48286bfae311bb12d6c85c7031fba756dcba9f12c2fdebcaaba81844e8ec60），本轮删除 locals/flags 重复清零的逻辑仍在。共享 copier 的后续分块修改也已保留。本轮 PASS 仅属于归档中的 7f95bc86bfcbadfe9551290600934eeafd769d8b278c6904deec2e272ef35646 header 及其冻结输入，未为这些后来修改重新资格化。新完整程序、真实 VM 状态捕获、ASM 边界、语言/DAP/run-to 回归及新版本长时回归均待原资源保护允许后重跑。之前 1839.18 秒持续回归仅属于旧冻结版本，不属于本轮新版本。

标准 Go JIT 冷启动仍未解决，本轮没有新 Go 通过结果；原优化及显式 debug policy 的 180 秒失败保留。AssemblyScript typed values、完整可执行检查点恢复、完整语言 native 体验和完整跨架构 VM/JIT 体验仍未完成。旧语言统计 14 implemented、38 partial、34 missing、1 separate_level、1 prohibited_by_scope（88 类）是此前审计结果，本轮清零优化没有新增用户调试能力，也没有重新做完整能力审计。

仅在逐字节核对可恢复备份后清理本任务旧临时对象、过期组件测试二进制及四个旧完整产品 ELF。旧产品仍保存在完整证据归档内；源码、guest/DWARF、SDK 和工具链保留。部分远端重复归档迁回本机，旧报告中的原始临时路径以本轮迁移记录为准。旧 snapshot 归档现位于本机 `/tmp/uwvm2-observer-zero-20261006/snapshot-copy-evidence-a1.tar.gz`（SHA-256 4ca8e306be39bc60152050bc3189fa02102eacc810606e0ea7e9fbea126447a4）；旧 sizeof 归档仍位于 `/tmp/uwvm2-language-sizeof-20261005/completion-evidence-a1.tar.gz`。

精确输入哈希、真实执行与保护停止记录、未完成项以及可恢复备份位置见 [本轮资格记录](debug_observer_packet_zero_qualification_20261006.json)。

新原始证据已备份至本机 `/tmp/uwvm2-observer-zero-20261006/observer-packet-evidence-a1.tar.gz`：189722071 字节、9118 个成员，SHA-256=8b215cff0e6b4c097099679875fee089629a2878668d0309722b2aa62d474ec8。本机和远端均逐成员核对；完整 SDK/toolchain 没有重复打包，其实际输入哈希及原位置保留。
