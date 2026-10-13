# WASIp1 checkpoint 根目录资源恢复修复（R7）

2026-10-06；同步修改本机 `uwvm2` 与 `uwvm2-ros`。最终 18 个修改路径与已测试快照 SHA256 一致。ROS 模式范围保持原有设置。

## 修复的真实问题

同一配置 mount 下，guest 两次调用原始 WASI `path_open(".")`，分别使用 follow=1、follow=0。
三个根目录 FD 共享真正配置的目录入口，但各有独立 WASI 引用计数资源；描述符别名才共享一个资源。

旧恢复逻辑比较 WASI RC 地址，把合法的 dot-open 资源误判为同名冲突。
真实 LLVM JIT、guest、原始 cooperative host gate 下的修复前复现：

```text
root_alias_before capture=captured restore=missing_mount root_resources=3
root_alias_before PASS checks=18 guest=7
```

保存前置流程另有 51 项检查；这 69 项不计入最终修复通过数。
旧代码在根目录 flags 相同的恢复分支还会直接复用目标 WASI RC，合并独立资源并遗漏保存的 follow 元数据。

最终实现：

1. 比较真正拥有的配置根目录入口，识别 mount 来源。两个独立配置保持不同来源，即使 guest 名和本机路径相同。
2. 每个保存的根目录 resource 创建独立 WASI RC；aliases 按 `resource_index` 共享相应 RC。保留各自 `checkpoint_follow`，暂存恢复不改目标 live resource 元数据。
3. flags 不同而重新打开原生根句柄时，在 `dir_stack_t` 保留原配置入口的拥有式引用。原始 guest `path_open` 复制目录链，后续 dot open 自动继承真实来源；目标关闭后引用仍保持目录能力有效。
4. 来源身份仅属进程内所有权，不序列化，portable version 1 不变。C++ 文件/目录 I/O、相关字符串及诊断输出使用 FastIO。

源码和测试：

- [目录链所有权](../../src/uwvm2/imported/wasi/wasip1/fd_manager/fd.h)
- [根目录来源辅助函数](../../src/uwvm2/runtime/lib/uwvm_runtime_wasip1_mount_identity.h)
- [portable restore](../../src/uwvm2/runtime/lib/uwvm_runtime_wasip1_portable_environment.h)
- [原始 WASI 目录测试](../0013.debugger/wasip1_mount_identity.cc)
- [真实 JIT checkpoint 测试](../0017.runtime/debug_wasip1_portable_runtime.cc)
- [guest WAT](../0017.runtime/fixtures/debug_wasip1_root_alias.wat)

## 最终测试结果

| 验证范围 | uwvm2 | uwvm2-ros | 合计 |
| --- | ---: | ---: | ---: |
| Linux 原生目录所有权与原始 WASI 调用 | 34 | 34 | 68 |
| macOS 原生目录所有权与原始 WASI 调用 | 34 | 34 | 68 |
| Windows 实际 KVM/QEMU guest | 34 | 34 | 68 |
| FreeBSD 实际 KVM/QEMU guest | 34 | 34 | 68 |
| Linux genuine LLVM-full 单环境事务 | 824 | 824 | 1,648 |
| Linux genuine LLVM-full 检查点组 | 958 | 958 | 1,916 |
| DAP WASIp1 协议回归 | 28 | 28 | 56 |
| **最终通过** | **1,946** | **1,946** | **3,892** |

单环境在新进程保存/恢复。每策略：普通 51+83；稀疏保留 FD 52+89；根目录资源 52+85。
每仓库覆盖 instruction/unwind 两种策略。新场景的 FD151/171/172 恢复后保持三个独立资源、
同一 mount 来源和各自 follow；再次捕获的完整 portable metadata 与载入文件逐字节相同。
最终 runtime/host-api 对象均重新编译；组测试复用前核验对象与所有依赖 SHA。

34 项原生测试使用 FastIO 真目录，直接调用原始 `path_open`/`fd_close`：
覆盖不同配置同路径的来源隔离、dot open、资源/别名、saved follow、独立句柄来源、
恢复后 guest 再 dot open、目标退役后的来源所有权及实际创建/写文件。该组不靠模拟 native debug ticket。

Windows/FreeBSD 使用实际 SDK、实际 guest，具有随机 nonce、精确 PASS 行、测试/VM 退出 0、
实际 KVM QMP 证据、基盘前后属性一致。使用私有 overlay/ISO、无网卡。
macOS 在本机 Apple SDK 编译，测试执行禁止 fork；编译的保守聚合 RSS 上界分别
**1,586,708,480 B**、**1,596,309,504 B**，均低于 2 GiB。

Linux 编译、测试、交叉编译和 QEMU 均在原 64 GiB cgroup，swap 0。
最终 guards 全部通过、实际 root 退出 0、全部绑定后代 pidfd 已退役；
各阶段 `memory.events` 不变。未调整共享上限或其他参与者。

| 阶段 | 本任务聚合 RSS 上界（B） | 共享 memory.current 峰值（B） |
| --- | ---: | ---: |
| runtime | 4286631936 | 67645321216 |
| group | 2580250624 | 65265590272 |
| components | 772669440 | 58418257920 |
| vm-windows-component | 2239295488 | 65872367616 |
| vm-freebsd-component | 1231216640 | 62239899648 |

## 测试资格与并行修改

本轮确认四 OS 目录所有权/原始 WASI 调用，以及 Linux 实际 LLVM JIT 单环境/组事务。
完整跨 OS capsule/事务迁移矩阵没有重跑，整个 Wasm 状态恢复和所有调试层也不属于本轮完整资格。

保存 Wasm checkpoint 仍需同时保存 WASIp1 checkpoint；portable 恢复前须在目标 OS 配置对应 `mnt`。
保存的是 FD/权限/位置等环境状态，文件内容不包含在内，本轮未改变此约定。

快照后记录到其他参与者改动了 2 个冻结路径；本轮 18 个修改路径均未漂移。
其他修改未混入本轮测试资格，详见 JSON 的 working-tree-drift 证据。

失败/被替代尝试均保留且不计数：首次基线触及全局内存保留量而退役自己的进程；
中间实现把来源放在 WASI RC，随后为 guest 后续打开传播移到目录链并重新测试；
一次输出目录创建失败导致空入口退出，修正后先冻结再调度最终测试；
收集器拒绝自有旧结果压缩回执末尾的字面量反斜杠-n，修正分隔符并保留原字节/SHA。
保留失败 guards、退出及退役证据。

## 可恢复证据

[完整 JSON](wasip1_checkpoint_root_alias_test_results.json) 包含 64 份原始证明、118 份日志、
最终收集 guard，随后追加归档证明。包括 flags、所有实际依赖 SHA、genuine SDK、产物 SHA、
VM 串口/QMP、macOS RSS。macOS 依赖由本机驱动核验；Linux 收集器认证拷回的报告，
没有声称能在 Linux 核验本机 Apple SDK 路径。

仅无损压缩此前自有 R6 已完成的可选产物，先核验退役和 `/proc` 使用。
此前 R5/R6 持久 raw JSON 无损 gzip 后核验原始回执 SHA，并保留还原回执。
本机两仓库已有 JSON 原字节保持。未清理其他参与者的进程或文件。

Linux 持久源码/证据归档：

```text
/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-root-alias-20261006-r7/test-inputs-evidence-1791222052238732774.tar.gz
```

- 归档大小：30435463 B；19,231 文件。
- 归档 SHA256：`60ccfb97284d3d8136254c673f3359d44dbfcc35754e7783ff7adafd717171e7`。
- 规范结果 SHA256：`5af1ae72484e8ef46116319c254ab12f9eeb1ce28fc9476db243faddf65e0a7b`。
- genuine SDK 归档：`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-checkpoint-reboot-20261005-r3/test-environment-1791210330404465482.tar.gz`。
- SDK 归档 SHA256：`3cb41246b39b4a9e15aefd7b30c33cbd0feda8466753edeff9f71f6273b4e49f`。

归档含修复前、中间、最终完整冻结源码、输入、脚本、日志和规范结果；相同文件用 tar hardlink 去重。
原生产物及私有 VM 磁盘依记录参数重建，原始 guest 基盘位置见 VM 回执。
按回执原始 `/tmp` 根目录还原；重启后先重新认证 boot 与原 cgroup anchor 再运行 guard。

最终报告、追加归档证明的 JSON、delivery receipt 同时保存于上述归档目录。
持久 JSON 为 `final-results.json.gz`；`gunzip -c` 后核验 receipt 的 `results_sha256`。
持久写入与压缩还原 SHA 在交付验证阶段再次核验。
