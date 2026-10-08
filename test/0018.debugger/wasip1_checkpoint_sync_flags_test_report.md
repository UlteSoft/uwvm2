# WASIp1 checkpoint 同步标志修复与测试（R9，2026-10-06）

两个仓库均已修复 `fd_fdstat_get` 的 POSIX 同步标志判定；覆盖 wasm32、wasm64 的 owned file、borrowed observer 和 directory 路径。Linux `O_SYNC` 包含 `O_DSYNC`，旧代码用任意位判断，把真实 DSYNC-only 的状态 2 误报为 26（DSYNC|RSYNC|SYNC）。portable checkpoint 因而可能申请不存在的 FD_SYNC 权限，或把文件重新打开成更强的同步模式。

现在要求完整 native 掩码匹配，并避免零掩码生成标志。真实 native 别名仍按 host 可观察状态报告。此次修改不改变 checkpoint 格式，也不增加 ROS 的运行模式。

## 验证结果

下表是每个仓库的最终检查数。

| 平台 | 检查数 | 实际覆盖 |
|---|---:|---|
| Linux | 433 | 原始 WASI wasm32/wasm64：12 组 flags 组合、文件/observer/目录、FastIO native flags 对照 |
| macOS 本机 | 340 | 同样的真实 WASI 路径；缺少 RSYNC 时明确 ENOTSUP |
| FreeBSD QEMU | 340 | 真实 FreeBSD 原始 WASI 程序；缺少 RSYNC 时明确 ENOTSUP |
| Windows QEMU | 144 | 支持的 flags、observer、目录；28 个不支持的同步请求明确 ENOTSUP |

总计 **7226 项最终检查通过**：四 OS 2514，Linux 完整 Core3 LLVM JIT portable 保存/恢复 2740，环境组 1916，DAP 56。另有 **1150 项负向基线检查**：原始 WASI 866；真实 Core3 JIT 284。旧实现的 JIT 基线显式复用已通过依赖/对象哈希核验的 R8 immutable runtime/host 对象，其有问题的 fdstat 头文件与本轮修复前相同。修复后的运行时使用本轮完整冻结源码重新编译。

两个仓库各使用 instruction/unwind 两种策略。除新增 DSYNC-only 场景，还回归 standard、sparse-reserved、root-alias、normalized-path，以及完整环境组。新增场景通过原始 guest path_open 打开真实文件；在真实 cooperative stop/host gate 下保存，随后在新进程、仅保留 PATH_OPEN+FD_DATASYNC 且去掉 FD_SYNC 的目标挂载上恢复。

验证恢复后的 native flags 仍只有 DSYNC，游标与共享 FD 别名保留，重新捕获后 metadata 字节完全一致，目标 pathname 文件内容不被复制或截断。伪造更强的 SYNC/RSYNC 请求仍被 capability_denied 拒绝，失败后的完整状态不变，原生 guest 正常恢复执行并返回 7。

测试程序的 POSIX native 检查已添加平台条件；Windows 的 DSYNC runtime case 明确跳过。两个 Linux 程序重新编译后，其二进制 SHA 与上述通过的程序完全一致，原始通过记录因此仍对应最终 Linux 二进制。其他 OS 覆盖的是原始 WASI 函数；本轮没有重新跑全套跨 OS JIT 事务矩阵，也不宣称覆盖全部 Wasm 3.0 调试特性。

## 资源限制与并行修改

Linux 编译、测试、交叉编译及 QEMU 都在原来的 64 GiB、swap 0 cgroup 中；macOS 检查受 2 GiB 监控。最终 own RSS 上界峰值 4293849088 B，cgroup memory.current 峰值 66996133888 B；macOS 编译/运行监控上界 1645805568 B。最终监控均成功、所有绑定的 own pidfd 已退休；共保存 6 个失败监控记录，包含测试准备错误和共享 cgroup 63 GiB 预留阈值触发后的安全终止。

初始测试修正了 close-list 成员名和 bool 输出匹配。JIT/Windows 的共享内存预留停止均保留失败记录，随后完成受控重试。另有一个无测试子进程的 waiting controller 在身份核验后退休，以调整本任务调度。未信号控制任何其他 agent。最终 12 个 own 源码路径与冻结源码一致，另外 20 个已变化的其他源码路径保留原样，结果明确针对冻结输入。

Windows 使用 private qcow2、2 GiB keep-size 预分配与 cache=none；helper 仅访问本任务目录，32 MiB 上限，不修改 filesystem reserve policy。失败/成功 private VM disk 在 own 进程退休后均已无损压缩并释放预分配。两个原始 guest bases 未改变。macOS 磁盘被其他并行工作占满时，只清理了已经有验证备份的本任务完成 source staging；可从归档对应 repo 快照重建 macos-inputs 路径。

## 使用与限制

- Wasm checkpoint 必须同时 checkpoint WASIp1，并在同一个 cooperative stop 下完成。
- 目标 OS 必须重新配置 mnt 和足够的 parent/child rights；pathname 文件内容与外部 I/O 不在这份 portable 状态里。
- 目标 OS 不支持的 native flags 仍会失败，不会悄悄丢弃或升级标志。Windows 本轮明确不支持这些同步模式。
- 已持久化的旧 flags=26 无法安全自动降成 2，因为它也可能来自真实 SYNC 文件。需要在修正后的实现上，从原 live 环境重新保存 DSYNC-only checkpoint。
- native binaries/objects 和 VM overlays 不放入 source archive；源码、实际命令、依赖/产物哈希、日志、metadata、真实 VM nonce/status 与 cgroup/macOS 监控记录均保留。原始 SDK 和 guest bases 保存在 Linux 的已记录路径。

## 可恢复材料

主 source archive：`/Users/liyinan/Documents/MacroModel/builds/wasip1-sync-flags-20261006-r9/test-inputs-evidence-1791231027767555883.tar.gz`  
SHA-256：`aad3313d84e64fd5a291089aeeba7dc0a5b8b0592c582634dfef8dd5c0f3991d`  
全部 **19293** 个 payload（含 tar hardlinks）均已逐个 SHA 验证。

Linux durable 副本：`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-sync-flags-20261006-r9-private-vm/evidence/source-evidence.tar.gz`  
SDK archive：`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-checkpoint-reboot-20261005-r3/test-environment-1791210330404465482.tar.gz`  
SDK SHA-256：`3cb41246b39b4a9e15aefd7b30c33cbd0feda8466753edeff9f71f6273b4e49f`

完整结果保存在同目录 `test-results-complete.json`。归档内 core results 与交付结果的关系记录在 delivery_metadata；归档后形成的 source qualification、placement 和 staging retirement 记录另随 delivery receipt 提供。下次重启后须先重新核验 boot/cgroup anchor，不能直接复用历史 guard 的身份断言。
