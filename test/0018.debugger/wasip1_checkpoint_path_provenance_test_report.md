# WASIp1 checkpoint 有效相对路径来源修复（R8）

2026-10-06；同步修改本机 `uwvm2` 与 `uwvm2-ros`。本轮 10 个源码/测试/文档路径与测试快照 SHA256 一致，ROS 模式范围保持原有设置。

## 修复的问题

原始 WASI `path_open` 接受 `./state.bin`、`nested//./state.bin` 等有效相对路径。
旧 `record_checkpoint_path` 拒绝其中的 dot/空组件，使成功打开的文件丢失 mount 来源，
导出的 portable checkpoint 把它作为 external FD，恢复时无法按 mount 相对路径重新打开。

修复前两个仓库各完成 141 项真实原始 WASI 调用检查，共 282 项：
每仓库 24 次有效打开丢失来源，覆盖原始 wasm32/wasm64 入口与 follow=0/1。
这些是问题复现检查，不计入最终通过数。

修复后将内部重复分隔符和 `.` 组件规范化：

```text
原始 guest 路径：nested//./state.bin
checkpoint 路径：nested/state.bin
checkpoint mount：原配置的逻辑 mount 名
```

保留 follow、mount 来源和原有 4096 字节原始路径/目录链预检。
不折叠 `..`，避免跨越 symlink 产生不同含义；绝对路径、末尾分隔符、反斜杠、
冒号、嵌入 NUL、规范化后为空的普通文件路径保持不可重新打开的分类。
Unicode 组件字节保持原样，portable 格式版本不变。
C++ 字符串、文件/目录 I/O 及相关诊断使用 FastIO。

源码和用例：

- [路径来源记录](../../src/uwvm2/imported/wasi/wasip1/fd_manager/fd.h)
- [四 OS 原始 WASI 路径用例](../0013.debugger/wasip1_path_provenance.cc)
- [真实 JIT 保存/恢复](../0017.runtime/debug_wasip1_portable_runtime.cc)
- [guest fixture](../0017.runtime/fixtures/debug_wasip1_normalized_path.wat)
- [checkpoint 文档](../../src/uwvm2/uwvm/debugger/wasip1_checkpoint.md)

## 最终测试结果

| 验证范围 | uwvm2 | uwvm2-ros | 合计 |
| --- | ---: | ---: | ---: |
| Linux 原生路径来源/原始 WASI | 184 | 184 | 368 |
| macOS 原生路径来源/原始 WASI | 184 | 184 | 368 |
| Windows 实际 KVM/QEMU guest | 184 | 184 | 368 |
| FreeBSD 实际 KVM/QEMU guest | 184 | 184 | 368 |
| Linux genuine LLVM-full 单环境事务 | 1,094 | 1,094 | 2,188 |
| Linux genuine LLVM-full 检查点组 | 958 | 958 | 1,916 |
| DAP WASIp1 协议回归 | 28 | 28 | 56 |
| **最终通过** | **2,816** | **2,816** | **5,632** |

原生用例直接调用原始 wasm32/wasm64 `path_open`/`fd_close`，使用 FastIO 创建和读取真实文件。
7 种有效写法 × 2 种 ABI × 2 个 follow 值共 28 次真实打开；另检查拒绝分类、4096/4097
字节边界及 Unicode 元数据。wasm64 是测试显式启用的已有可选 WASI 入口。

Linux 使用 genuine `23.1.1-uwvm-ros.11` LLVM 库和实际运行时/host API。
单环境每策略：普通 51+83；稀疏保留 FD 52+89；根目录资源 52+85；规范化路径 51+84。
每仓库均覆盖 instruction/unwind；保存、恢复在独立新进程执行。
新用例确认 canonical 路径、真实 FD 权限/位置，目标文件内容保持原值，
恢复后再次捕获的完整 portable metadata 与载入 metadata 逐字节相同。
组回归每策略 479 项，检查捕获/恢复、失败事务和多模块环境。

uwvm2 运行时/host API 成功编译后，新增测试程序的两处条件表达式得到裸 UTF-8 指针，
导致 FastIO 拼接/比较编译失败；改为显式 `u8string_view` 后重跑。
复用这两个已成功编译对象前，逐一核验对象 SHA、全部实际依赖 SHA 和编译参数；
ROS 对象在最终阶段重新编译，组链接也核验全部对象/依赖。失败链接没有计入通过数。

Windows/FreeBSD 使用真实 SDK 和真实 guest；随机 nonce、精确 PASS、各测试退出 0、
QEMU 退出 0、QMP KVM enabled 以及基盘前后属性一致均已验证。虚拟机使用私有增量盘、无网卡。
Windows 最终采用自有预分配磁盘文件和 `cache=none`；guest 内存仍为 2 GiB。
磁盘准备只分配新的私有文件，未改变文件系统保留策略；分配前后保留至少 20/18 GiB
实际磁盘空闲。通过后已确认所有 VM pidfd 退役，并释放其 2 GiB 预分配块，记录最终文件 SHA。

macOS 使用本机 Apple SDK；编译聚合 RSS 保守上界分别为 **1,586,741,248 B**、**1,614,692,352 B**，
均低于 2 GiB。测试执行限制和监控回执保留。

Linux 编译、交叉编译、实际测试和 QEMU 均在原 64 GiB cgroup，swap 0。
最终所需 guards 均通过、实际 root 退出 0、绑定后代 pidfd 全部退役，阶段内 `memory.events` 不变。

| 阶段 | 本任务聚合 RSS 上界（B） | 共享 memory.current 峰值（B） |
| --- | ---: | ---: |
| runtime | 4202840064 | 65925890048 |
| group | 2580029440 | 66184204288 |
| components | 780517376 | 60304576512 |
| vm-windows-component | 2240753664 | 65466228736 |
| vm-freebsd-component | 1376243712 | 65823170560 |

## 失败尝试与并行修改

两次早期基线用例编译错误已修正，原 harness/manifest 和失败 guard 保留；
重试日志是当前日志，不声称保留了被重试覆盖的首次编译日志。
真实 JIT 编译的前两次尝试以及 Windows 的前三次启动触及共享 63 GiB 保留阈值，
只退役本任务经过 pidfd 身份确认的进程；失败不计数。
一个 Windows 等待任务在未发出任何测试子进程时被身份确认并取消，随后调整进入阈值重跑。
所有硬上限和全局停止阈值保持一致。

失败 Windows 私有增量盘的写入也计入 tmpfs/cgroup 内存。三份失败增量盘等自有产物
无损压缩后保留 SHA/回执和串口日志，释放 874,514,432 B 原产物，压缩归档 230,540,469 B。
最终将 Windows 私有磁盘放到磁盘文件，并成功完成测试。

只对本任务拥有的不可变快照，在同一仓库相对路径、大小和 SHA 完全相同时建立 hardlink；
避免把不同 include 名称混为一个 inode。此前已完成的可选产物也按原字节/SHA 保留。
未管理其他参与者的进程或文件；`/proc` 权限不足的条目在迁移回执中明确列出。
另 24 个冻结路径的并行变动保持现状；本轮 10 个修改路径全部匹配最终测试快照。
当前整仓的这些其他变动未重新获得本轮测试资格。

## 使用边界与可恢复证据

保存 Wasm checkpoint 时仍需同时保存 WASIp1 checkpoint；目标 OS 恢复前要配置对应 `mnt`。
保存 FD、权限、位置等环境状态，**不包含文件内容**。

本轮资格覆盖四 OS 路径来源与原始 WASI 调用、Linux 真实 JIT 单环境/组恢复。
完整跨 OS capsule 事务矩阵和整个 Wasm 状态恢复没有在本轮重跑；其他调试层也不属于本轮完整资格。

[完整 JSON](wasip1_checkpoint_path_provenance_test_results.json) 保存 70 份证明引用、
102 份日志、收集 guard，再追加完整归档、归档 guard 和最终存放回执。

本机持久源码/证据归档：

`/Users/liyinan/Documents/MacroModel/builds/wasip1-path-provenance-20261006-r8/test-inputs-evidence-1791226223230092902.tar.gz`

- 归档：28,817,566 B；SHA256 `8b2bfef316de038c3a95dbf06618100223c708c948a1326169eae875abcc112f`。
- 原始 canonical JSON：SHA256 `d75b377f49fa36014e6266922ec229ecc58d559cf6939ba9b107ff653226b224`。
- 19,258 个归档条目含修复前/最终/早期 harness 快照；全部字节与归档清单核对。
- SDK 持久归档：`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-checkpoint-reboot-20261005-r3/test-environment-1791210330404465482.tar.gz`；SHA256 `3cb41246b39b4a9e15aefd7b30c33cbd0feda8466753edeff9f71f6273b4e49f`。
- 原 Linux `/tmp` 路径为临时副本，当前持久位置由 `archive-placement.json` 记录。

本机因并行写入发生磁盘不足，失败的证据副本已换成原字节一致的 hardlink。
此前四份自有可选产物归档最终完整迁至 Linux 自有持久目录：
`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-path-provenance-20261006-r8-private-vm/previous-products`。逐项校验字节数、SHA 和所有者，并保留至少 20 GiB 实际磁盘空闲；
未改变文件系统保留策略。JSON 中最终迁移回执覆盖 canonical 收集时的历史存放位置。

原生可执行文件、对象、VM 私有增量盘不是源码归档内容；实际产物 SHA、SDK、
全部依赖、命令和原始 guest 基盘位置保留，可按记录重建。
此前可选产物迁移回执另提供按原路径恢复归档的方法。
