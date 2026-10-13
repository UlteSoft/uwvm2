# WASIp1 多环境 checkpoint 恢复验证

2026-10-05，`uwvm2` 与 `uwvm2-ros` 同步修复并验证。测试对应保存了哈希的固定源码快照；并发工作树后来发生的改动单独记录，不代表已经被本轮测试覆盖。

## 实现与修复

新增 `llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api`。一次接受 1..16 个不同的真实 WASIp1 环境，在同一个真实的完整 cooperative stop 中准备所有资源，全部成功后才统一发布。任何环境准备失败，都保留所有目标环境的原状态；错误诊断中的 `request=INDEX` 指出失败请求。

- 原生 capsule 组拒绝不同 recording label 的混用；portable 组同时检查每项请求与 snapshot 的标签，以及整组标签一致性。标签只是比较数据，不能授权恢复，也不能证明 Wasm 与 WASIp1 曾同时捕获。
- 组级 FD 额度计入绑定和预留空槽位，总上限 65,536；参数、环境变量、mount 和相对路径合计文本上限 1 MiB。
- 单环境及多环境恢复都先构造私有替换资源，再检查同一执行租约和 closed host gate；检查成功后才提交。提交完成后的取消请求不会把已经完成的恢复报告成失败。
- portable 恢复继续使用目标环境当前的 mnt、FD 和权限。未跟踪文件需要显式绑定现有目标 FD；无法准确恢复的游标或 flags 会明确拒绝。
- 文件打开、读写、seek、字符串和诊断输出沿用 fast_io。没有用伪造 SDK、mock 执行器或源 OS 的 native handle 冒充目标 OS 的恢复。

接口说明和 C++ 示例见 [wasip1_checkpoint.md](../../src/uwvm2/uwvm/debugger/wasip1_checkpoint.md#atomic-native-environment-groups)。原生组恢复入口继续提供同进程 managed 文件内容恢复；portable 组只恢复状态，保留目标文件内容。

## 测试结果

两个仓库均使用实际 LLVM full runtime、host API 和真实 guest；分别验证 `instruction` 与 `unwind` 两种调用栈策略。每项组测试都建立两个独立初始化的 WASIp1 环境，取得真实 before-park capture、完整暂停及 typed GC 观察证明，恢复后继续执行 guest 的 `fd_read`。

| 目标系统 | 两仓库原生运行 | 从其余三系统导入 | 结果 |
| --- | ---: | ---: | --- |
| Linux | 4 | 12 | 全部通过 |
| macOS | 4 | 12 | 全部通过 |
| FreeBSD 15.1 | 4 | 12 | 全部通过 |
| Windows 11 / NT 10.0.26100 | 4 | 12 | 全部通过 |

共 **16 轮原生 + 48 轮有向跨系统恢复**，每轮 **220 项检查**，合计 **14,080 项组测试检查**。迁移输入由源 OS 的原生程序在真实暂停中生成，经过文件哈希检查后交给目标 OS 的新进程加载；没有把同一 OS 的重跑计算成跨系统恢复。

检查包括：整组原子提交、第二个目标失败时的回滚、空请求、缺失 metadata、export 混入 import、不同标签、重复真实环境、第二个目标 FD 额度不足、错误 Wasm 指纹、缺失 FD 绑定、匿名文件游标不匹配、整组文本超限、参数及环境变量恢复、FD alias 保留、portable 恢复不覆盖目标文件内容、原生 managed 组单独恢复内容，以及恢复后继续运行和旧暂停票据失效。

此外，两仓库的 Linux 单环境 checkpoint/portable 回归合计 **888 项检查**通过；完整 CLI、broker、DAP 回归各 **232 项**通过。两个仓库各自关闭 WASIp1、仅启用 uwvm-int 的实际 runtime 编译检查共 **4 项**通过。这四项是编译检查，不计入 native guest 运行数。

## 执行约束和证据

Linux 编译、测试以及两种 QEMU 客体执行均使用原有 cgroup：

```text
/sys/fs/cgroup/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope
memory.max = 68719476736
memory.swap.max = 0
cpuset = 0,2,4,6,16-31
```

bootstrap 在 SIGSTOP 状态下迁入此 cgroup，确认身份与归属后才恢复执行。pidfd、进程 birth、归属和 RSS watcher 只管理本任务创建的进程；所有成功阶段的 OOM 事件计数均未变化，进程均完成回收。共享 cgroup 内存不足时等待，没有改用其他 cgroup。

macOS 在本机原生运行，固定程序禁止 fork，并检查实际进程 birth、RSS 和回收结果。16 轮 macOS 运行中，guest 与控制器 RSS 的合计峰值上界最高 **81,051,648 字节**，低于要求的 **2 GiB**。

FreeBSD、Windows 使用 KVM 的 2 GiB 私有 QEMU 客体和只读 backing image；客体报告真实 OS 版本、输入哈希、每项执行状态和唯一 nonce。通过记录包括正常 QEMU 退出及 backing image 的设备、inode、大小和时间戳不变。Linux 的 KVM ACL 发生变化后，仅向本任务的 VM 提供 KVM FD，没有修改主机 ACL、账户组或其他 VM。

当前 runtime 需要 native AsmParser；缺少此组件的 macOS、FreeBSD、Windows SDK 已使用各自对应、经过校验的 LLVM `23.1.1-uwvm-ros.11` 源码、配置和编译命令构建真实 archive。原 SDK archives 未替换，新增组件的源码、依赖、编译命令、object/archive 哈希保存在证据中。之前的编译、KVM 启动及缺少关闭 JIT 宏的测试配置失败记录均保留。

机器可读结果见 [wasip1_checkpoint_group_test_results.json](wasip1_checkpoint_group_test_results.json)。包含 64 项运行的日志哈希、输入 metadata 哈希、编译及链接输入、SDK 来源、cgroup/RSS/回收证明和失败尝试。重复文档用 `contents_ref` 指向 `document_contents_by_file_sha256`；该键是原始文档文件的 SHA-256。

结果文件 SHA-256：

```text
f8a6db194b11f3f0527ae18e5fe0d84c970c0494696a7e2016f0a501f15f29b0
```

固定快照的实际编译依赖与并发工作树比较：uwvm2 的 1,614 个仓库内依赖中 15 个后来变化，uwvm2-ros 的 1,594 个中 14 个后来变化，无缺失文件。本轮修改的五个实现文件及组测试文件在两仓库中均与测试输入一致。完整差异在结果的 `live_source_audit` 中，不将这些后续变更算作已测试。

## 当前边界

组恢复现在通过原生管理 API 提供；console 与 DAP 的现有 import 命令仍一次恢复一个环境。portable 快照保存每个环境内部的 alias；跨环境共享现有资源可通过显式 FD 绑定保留，独立 mounted-file 快照尚不编码跨环境的 open-file identity。

跨 OS 数据不包含文件内容、已完成的外部 I/O、socket 缓冲或其他外部世界状态，目标 mnt 需要重新配置。整个 Wasm 实例恢复以及 Wasm/WASIp1 的原子联合回滚仍未实现。应在同一暂停中同时捕获 Wasm 和 WASIp1，现有提醒继续保留；这不等于已经可以执行联合回滚。
