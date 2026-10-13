# WASIp1 完整 Linux CLI/JIT 集成回归（R25）

日期：2026-10-07。两库同步修复，并使用当前冻结源码重建完整产品。ROS 保持 full uwvm-int / full LLVM JIT，没有引入基础、lazy 或 tiered 模式。产品改动不增加 I/O 路径；原生夹具继续使用 FastIO string / print / scan 和文件接口。

## 修复

普通 DATA 查询 API `llvm_jit_debug_query_wasip1_state_host_api` 缺少 `portable_export_group` / `portable_import_group` 分支，合法组请求在真正的停止点仍返回默认的“requires current cooperative stop”，并产生 switch 警告。两库现在明确返回 `invalid_request`；组事务仍使用专用 capture/restore 接口。新增原生回归先取得真实 Core3 Wasm before-park 捕获和完整协作暂停，再验证两种组操作语法合法、明确拒绝且没有提交或 capsule metadata。没有构造假的暂停状态、伪造 FD 或绕过 canonical owner。

CLI 测试原来没有检查 quit 后的实际退出码，超时强杀也可能计为通过。现在每个 Console / broker 收尾都记录退出码和 forced 状态，只有自然 exit=0 通过；构造期等待 prompt 失败也清理已启动进程。用真实子进程验证正常退出、非零退出、信号退出及超时强杀，后面三类必须被拒绝。信号控制只作用于测试自己创建的子进程。

本任务 Linux 监控器的目录 allocated_bytes 原来重复计算同 inode 的多个硬链接。现在按 `(st_dev, st_ino)` 统计物理文件块；逐路径 logical_bytes 限制保持不变。真实硬链接、独立拷贝和外部符号链接检查，与 Linux 原生 `du -B1`（扣除目录自身块）一致。没有提高目录限额、硬卷容量或剩余空间保护。

## 实际执行结果

| 仓库 | CLI / DAP 观察 | 正常收尾会话 | 原生 checkpoint | portable 保存 | portable 恢复 |
|---|---:|---:|---:|---:|---:|
| uwvm2 | 314 | 14，全部 exit=0 | 154 × 两策略 | 51 × 两策略 | 83 × 两策略 |
| uwvm2-ros | 314 | 14，全部 exit=0 | 154 × 两策略 | 51 × 两策略 | 83 × 两策略 |

共 **628 项 CLI/DAP 观察、1152 项原生运行检查**，合计 1780；是重复仓库、策略和断言的累计数，不等于独立功能数。instruction/unwind 均通过。另外：Linux shutdown 负向控制 8 项、本机 macOS shutdown 负向控制 8 项（聚合 RSS 上界最大 99139584 字节，小于 2 GiB）、硬链接统计 8 项。四次真实 full interpreter/full LLVM 后端 smoke 通过；ROS 的 `-Rjit`、`-Rtiered`、`-Rcm` 三种已删除模式都以 126 退出。

完整 CLI/DAP 覆盖 argv/env 修改、分页、权限缩减、managed 二进制文件创建/dup/close、关闭复用后的 capsule 恢复、严格模式拒绝且不提交、portable 独占导出、已有文件不覆盖、损坏 digest / FIFO / reserved FD 拒绝、新进程重配挂载后的恢复，以及两环境整组导入中后项失败时不部分提交。真实 Wasm 恢复后继续读取 argv/env 和 FD 数据，返回值及 entry/return/errno trace 均校验。C++ 夹具进一步检查内容、偏移、flags、aliases、FD 配额及真实 canonical 捕获。

**Wasm checkpoint 必须同时保存 WASIp1 checkpoint，在同一个协作停止点完成。** 本轮原生夹具检查 Wasm-only 提醒 metadata，并检查同一 proof 下的联合 capture。WASIp1 restore 仍只恢复其环境/资源；未实现整台 Wasm 实例与 WASIp1 的联合原子 restore/replay。Portable 数据不携带外部文件内容，跨进程/OS 的目标环境仍需重新配置 mnt。

## 工具链、失败及范围

旧跨 OS SDK 原路径在清理后缺失。找到其他任务已恢复的 Linux ros.11 SDK，逐项只读核验源码、generated headers、全部链接 archive；用它重建本轮三条实际产品翻译单元及原生夹具。没有复用其他任务的 VM 二进制、对象、进程，也没有修改其 SDK。借用的 SDK 原始修补资格和全部 SHA 在 `borrowed-sdk-qualified.json`，与当前仓库编译依赖分别记录。普通 LLVM 23.0 被 ROS 的精确版本检查拒绝；未绕过版本限制或改版本号。

保留所有失败，不计入通过：初始普通 SDK / 未配置 OpenSSL 的诊断构建；4.5 GiB ROS 编译保护线停止；完整 main 的 peer assembly_finish switch 警告导致严格构建拒绝；普通 uwvm2 的 6 GiB 编译保护线停止；硬链接重复统计造成的目录 admission 拒绝；初始 ordinary smoke 将 -Rint shortcut 与 -Rcm 混用而收到参数冲突。修正 smoke 用 `-Rcc int -Rcm full`，没有改产品参数校验。失败日志与 guard history 均保存。

变更 runtime 和 native fixtures 使用 `-Werror=switch` 编译通过；main 保留其原有 peer 警告，不声称整个项目零 warning。没有覆盖或修复其他任务正在处理的 ASM 代码。本轮也未验证 language/ASM 的完整体验。

**本轮完整 CLI/LLVM-JIT 运行范围仅为 Linux x86_64。** Windows、FreeBSD、macOS 的 ros.11 target SDK 尚未恢复，未重建执行它们的完整 VM；macOS 此次只运行测试判定的负向控制。上一轮 R24 的四 OS native 文件 helper 验证保留其原冻结范围，不能替代本轮完整 VM 结论。

## 资源与证据

全部 Linux 编译、validator、CLI、broker、原生夹具和统计回归在原 64 GiB、swap=0 cgroup 内运行。只为普通 uwvm2 这条完整构建提供独立、精确 argv 限定的 8 GiB owned-RSS guard；其他阶段维持既有额度。共享 cgroup admission 54 GiB、停止线 60 GiB、目录 6/7 GiB、8 GiB 私有 ext4 卷、512 MiB 卷余量及宿主盘余量保持不变。所有成功/失败的已启动 guard，memory.events 前后相同，绑定子进程均经 PIDFD 退休；其他任务进程未被纳入所有权或终止。

每库大产物先逐项压缩、完整读回验证 SHA，再只退休本轮的三条 object 和三个 binary，分别保存在 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-live-integration-20261007-r25/uwvm2-products.tar.gz` / `uwvm2-ros-products.tar.gz`。编译命令、依赖 SHA、WAT/Wasm、原生日志、CLI 原始会话、broker transport、失败及源码补丁均保留。源码固定于 inputs/post-inputs；最终 API 文档作为 local-evidence/docs sidecar 保存，不改变已编译依赖。其他任务在本轮冻结后继续修改的工作树文件另列 SHA，不默默计入本轮执行结论。

归档前私有卷使用 5777154048 字节，可用 2707361792 字节。最终占用、完整归档 SHA 和双方读回结果见 delivery receipt。
