# WASIp1 portable restore 资源别名修复（R14）

已同步修复 UWVM2 与 UWVM2-ROS：两个独立的 checkpoint 资源不能绑定到同一个目标 owner。旧实现会成功导入并把资源数减少一个，使原本独立的文件开始共享游标。现在返回 `unsupported_resource`，不发布替换状态。

本轮四个 OS 测试的是原生匿名文件、资源身份检查和游标行为；真实 Core3 LLVM-full 停点、调试构造、checkpoint/import、组恢复与 resume 在 Linux 上验证。断言次数不代表 Wasm 特性数量。

## 源码与行为

`uwvm_runtime_wasip1_portable_environment.h` 原先直接把外部/stdio 目标的实际 RC 放入 replacements。不同 resource_index 可以填入相同 RC，后续 FD 构造就把两个独立资源合并了。仅比较目标 FD 数字也不够：fd-dup 产生的两个 FD 可以指向同一 RC。

新增 `wasip1_resource_identity::conflict`，在所有替换资源准备好后、任何 FD 表和文本发布前，检查实际保留的 RC 是否重复。检查只在每个环境的资源行之间进行；多个 guest binding 原本共用同一资源行仍正常共享。比较只用于内部准备，不序列化指针，不提供停点或 capture 权限。复杂度 O(n log n)，受原 65,536 行限制。

实际目标仍须满足原权限、类型、cursor/flags 和 mount 条件。已挂载文件通过原 path_open 独立重开；匿名目标须事先具有匹配状态。文件内容不进入 portable wire。

两仓库各六个同步文件：

- src/uwvm2/runtime/lib/uwvm_runtime_wasip1_resource_identity.h
- src/uwvm2/runtime/lib/uwvm_runtime_wasip1_native_file.h
- src/uwvm2/runtime/lib/uwvm_runtime_wasip1_portable_environment.h
- test/0013.debugger/wasip1_resource_identity.cc
- test/0017.runtime/debug_wasip1_portable_runtime.cc
- src/uwvm2/uwvm/debugger/wasip1_checkpoint.md

C++ 文件、打印和命令解析使用 FastIO。ROS 的 mode 配置沿用现有实现，没有新增 basic mode。

## 测试

| OS | UWVM2 | ROS | 执行 |
|---|---:|---:|---|
| Linux | 16 项检查 | 16 项检查 | 原 64 GiB cgroup 原生进程 |
| Windows | 16 项检查 | 16 项检查 | 原 cgroup 内 QEMU/KVM，真实 Windows guest |
| FreeBSD | 16 项检查 | 16 项检查 | 原 cgroup 内 QEMU/KVM，真实 FreeBSD guest |
| macOS | 16 项检查 | 16 项检查 | 本机原生进程；编译/运行内存上界最大 1035468800 bytes，低于 2 GiB |

原生检查覆盖空集合、单一 owner、相同内容的独立文件、相邻/非相邻重复 owner、不同 FD cell 共享 owner、首个冲突位置、空 target、检查前后 flags/cursor 不变，以及别名共享游标和独立文件游标隔离。四 OS 合计 128 项。

Linux 旧实现对照：两仓库 × instruction/unwind × save/restore，共 8 个真实停点进程、632 项断言。实际构造两份 A NUL B 匿名文件与第一份文件的 FD 别名；导出真实 snapshot；把两份独立资源绑定到第一份目标。旧导入返回 restored，重新导出后资源数确实减少一个。用真实 native capsule 恢复原布局，guest 返回 7。

Linux 修复版：两仓库 × 两种策略 × 九场景 × save/restore，共 72 个进程、5,320 项断言。九场景为 standard、sparse-reserved、root-alias、normalized-path、dsync、root-flags、root-choice、fd-slot、resource-alias。

新增 resource-alias 场景验证：

1. 相同目标 FD 的冲突被拒绝，返回结果没有 capsule/portable 输出。
2. 不同目标 FD、相同实际 RC 的冲突也被拒绝。
3. 两次拒绝后完整 portable wire 与基线逐字节一致，包括 FD、rights、flags、cursor、argv/env 和 free list。
4. 两个独立目标正确恢复，原有共享 binding 保留；再次导出完整 wire 不变。
5. seek 第一份到 7 时，其原始别名也到 7，第二份仍为 0；恢复游标并清理测试状态后 guest 正常返回 7。

两仓库的两环境组恢复在 instruction/unwind 下均通过：4 个进程、1,916 项断言。正向 Linux 合计 76 个进程、7,236 项；加四 OS 原生检查，正向合计 7,364 项。负例单独计数。

最终 feature guard 全部通过，actual PID 经 pidfd 确认退出，测试期间 parent memory events 未变化。Linux memory.max=68719476736、memory.swap.max=0、cpuset=0,2,4,6,16-31。Windows/FreeBSD 有精确 guest nonce、退出码 0、KVM enabled 和未变化的 base image。

## 接口示例

假设资源 2、3 是两份独立匿名文件，目标 FD 3、4 分别满足它们的权限、游标和 flags，PATHHEX 为目标系统上的 checkpoint 文件名：

```text
set wasip1 import 0 PATHHEX 2=3 3=4
```

`2=3 3=3` 会被拒绝。若目标 FD 4 是 FD 3 的 fd-dup，`2=3 3=4` 也会被拒绝；用两次 `set wasip1 file` 构造独立匿名目标，再使用返回的 FD 数字。资源编号以实际 checkpoint 及诊断为准，例子不固定分配编号。

## 证据与并发环境

持久 SSH Linux 证据：

`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-resource-alias-20261006-r14/source-and-test-evidence.tar.gz`

大小 272596978 bytes；全部 17155 个 payload 校验；SHA-256：

`9e4d520fead34e1a0d301fea621f0ec760d045c6aee7ba691c294234eee2336c`

包含实际冻结源码、补丁/备份、源码依赖与 SDK/链接输入 SHA、测试对象和程序、Wasm、wire、日志、guard 历史及其确切脚本版本、VM 证明和逐项校验的 macOS 输入归档。源码使用独立冻结字节；两仓库的 12 个本轮文件与测试快照相符。并发 agent 的三个 runtime 依赖更新已纳入冻结源码，因此重编译了 runtime。

保留失败历史但不计入通过数：共享 cgroup 超过 63 GiB 保护线时，只停止本任务进程；编译/链接的较小 RSS 额度也曾触发保护。成功完成的对象以实际 argv、源码依赖和 SHA 校验后复用。编译器专用 jemalloc 配置和库 SHA 已记录，guest 进程保持原运行环境。未对其他 agent 发信号。

前几轮已结束产物先归档逐项验证，再回收临时副本；62,020 个 payload 的归档为同目录 `completed-prior-assets.tar.gz`。完成的运行对象/程序复制到同目录 completed-runtime，逐字节校验后保留原路径的链接，释放 tmpfs 占用。macOS 已完成私有源码与程序回收前验证了全部归档 payload；实际依赖验证后才回收。

Windows 私有 overlay 在确认 guest 退出、base 未变化且 gzip 解压字节完全匹配后回收其约 2 GiB 预分配，压缩副本 `completed-windows-disk.qcow2.gz` 保留。全局磁盘保留策略和 cgroup 限额未修改。归档的初次文件大小/空间限制中止、元数据收集先于最新 guard 历史送达的中止也保留；最终归档与收集均通过。

本轮保护的是每个环境已记录的资源别名关系。外部文件内容、kernel buffers、inode/time 和 stdio 已发生的 I/O 不回滚；Windows 目录 NONBLOCK 仍明确拒绝。本轮未实现 whole-instance Wasm restore 或原子联合 Wasm/WASIp1 restore。

**提醒：Wasm 与 WASIp1 checkpoint 必须在同一个真实 cooperative stop 同时采集。Wasm-only checkpoint 不包含 WASI FD 和环境状态。**
