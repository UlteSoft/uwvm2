# R39：完整 runtime 发布记录的私有预分配

两个仓库已同步新增最终 full-runtime 记录准备步骤。在旧线程退休前，给每个候选模块预先分配 typed/raw/resume 入口、函数代际、safe-point 对应表、空 compiler metadata 槽位和 retained-generation 容量。模块对应必须匹配真实非移动 parser/source、dense 顺序、staged engine、编译器 plan 与完整函数范围。真正 execution drain 和 OS/TLS join 后，管理器复查同一份记录，才继续报告 prepared_world_ready_closed。

maximum_private_full_publication_functions 默认 65536，按所有模块的 defined 函数总数限制；容器内存由原 native payload 预算计费。超额报告追加的 full_publication_record_preparation_declined（旧枚举值保留），测试验证原 pause/source epoch/initializer serial 不变。预留 retained-generation 容量时不创建空 owner，避免现有退出清理遍历空元素。ROS 不含 tiered_loop_reentries 字段，新增按类型检测的兼容检查；初版编译失败日志保留，修正版本重新完整编译。

| 最终验证 | 结果 |
|---|---:|
| 两仓库 Linux 测试/runtime 及 Windows/FreeBSD/macOS runtime 前端 | 16 项通过 |
| 独立 Wasm 组装／validator 检查 | 8 项通过 |
| 原生阶段新组装／validator 检查 | 28 项通过 |
| 两仓库 Linux LLVM full 原生 | 24 个全新用例通过，复用 0 |
| 本轮 Windows/FreeBSD/macOS 原生 | 尚未执行 |

24 个原生用例分别覆盖 core 普通／间接调用、WASIp1 普通／间接调用、完整单实例和 preload，并运行 instruction/unwind 两种策略。新配额拒绝、正常准备计数、真正物理 join 后复查，以及前几轮 private dispatch/indirect/native endpoint/CFI/root/环境归属检查均实际执行。每个仓库重新构建 runtime 和 host API，使用真实 LLVM 链接与 JIT。

编译和测试均在原 SSH Linux 64 GiB cgroup；产物只写入本轮拥有的 tmpfs，受 768 MiB 文件、6 GiB RSS 和共享 cgroup 60 GiB 停止线监控。所有本轮进程已回收、RAM 产物已删除，父 cgroup memory.events 未变化。日志、命令、源码/hash 和 guard 证明保存。持久四 OS suite 原磁盘/inode 门槛保持，本轮没有把前端编译称为其他 OS 原生测试；本机 macOS 2 GiB 限制保留。

本机磁盘在并行工作期间再次写满。本任务的六份 R33 旧归档迁移到 Linux：在原 cgroup 独立读回校验 342 个成员、3,759,922,400 字节，7 个校验进程全部回收，再按原 SHA/device/inode/UID/size/mtime 身份删除本机副本，实际释放 264,507,392 字节。归档、原资格证明及新迁移记录均保留；未删除其他 agent 的文件。此只读归档校验使用独立 archive lock，不改变运行中的 UWVM suite lock 或原存储政策。

测试使用冻结的 38 个源文件叠加未变 R34 依赖，未覆盖其他 agent 正在修改的整个 checkout。既有 ROS validator/bridge-depth 接口差异保留。

**整体 world 发布、恢复线程启动和 guest replay 仍未接通。** 新记录保持 engine/context 为空、ready=false、epoch=0、LIVE native seal 未发放；实际 code/CFI、typed target 数组和有效 body/bitmap 仍由真实 staged owner 持有。后续共同提交必须移动同一分配，保留 LLVM 已嵌入的地址，并真实提交 source/initializer serial/GC/WASIp1/native 归属，再完成所有新线程的 closed startup lease、根、ledger 和 pause participant。Wasm 与 WASIp1 必须在同一实际停止点一起 checkpoint；外部文件内容和 I/O 不会回滚。
