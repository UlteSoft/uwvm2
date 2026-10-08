# WASIp1 portable checkpoint：sparse 保留空槽修复与测试

2026-10-06。uwvm2 与 uwvm2-ros 同步修改。本轮不增加或恢复 ROS 已删除的基础运行 mode。

## 缺陷与真实复现

真实 LLVM-full guest 使用原始 debug_jit 初始化，执行原始 WASI fd_renumber(0, INT32_MAX)，把没有输入权限的保留 FD0 移入 sparse FD 表。修复前，native capsule capture 返回 captured，而 portable capture 返回 invalid_portable_snapshot；恢复并继续 guest 后正常返回 7。复现使用实际 runtime/host API、cooperative stop 和 Wasm validator，不伪造 ticket 或 capsule。

旧 portable schema 把所有 reserved descriptor 限制在 opens_size 内，恢复代码也对每个 reserved descriptor 直接使用 opens.index_unchecked。原始 FD manager 支持把保留空槽移入 renumber_map，因此合法环境不能 portable 保存。

## 修改

两个仓库同步更新：

- wasip1_portable_checkpoint.h：保留 FD 与活跃 FD 使用相同的 INT32_MAX 范围；保留已有 FD 唯一性、dense 前缀完整性、rights 与总 scan-cell 配额检查。
- uwvm_runtime_wasip1_portable_environment.h：dense 保留槽放入 opens，sparse 保留槽放入 renumber_map；保存 null-resource 构造状态和两类 rights。
- wasip1_portable_checkpoint.cc：增加 sparse 保留槽的两种空资源状态、实际文件保存/加载、两环境 group framing、INT32_MAX/越界/重复/冲突/dense 缺口与 65536/65537 scan-cell 边界测试。
- debug_wasip1_portable_runtime.cc 与 debug_wasip1_sparse_reserved.wat：新增实际 guest 把 FD0 移到高位后，跨进程保存/恢复、目标空槽保护、共享 native 游标与别名、完整元数据再保存测试。
- wasip1_checkpoint.md：记录高位保留槽的规则。

目标已经占据的 reserved cell 仍须按原 FD 号、rights 和 null-resource 状态原样保留；import 不能把它变成活跃文件或输入能力。closed/free-list 仍只索引 dense 前缀。65536 上限按实际 scan cells 计算，高位 FD 只计一项。

portable wire 仍是 version 1，新增状态使用原有 reserved record 字段；旧版本读者会拒绝自己不支持的 sparse reserved 状态，普通 dense 快照格式不变。所有新增 C++ 文件 I/O、路径字符串、输出与读取使用 FastIO。

## 验证结果

| 本轮检查 | uwvm2 | uwvm2-ros | 合计 |
|---|---:|---:|---:|
| Linux portable codec / 原生文件 | 2127 | 2127 | 4254 |
| 本机 macOS portable codec / 原生文件 | 2127 | 2127 | 4254 |
| Windows 实际 KVM QEMU 客体 codec / 原生文件 | 2126 | 2126 | 4252 |
| FreeBSD 实际 KVM QEMU 客体 codec / 原生文件 | 2127 | 2127 | 4254 |
| Linux 真实 LLVM-full 单环境普通+sparse save/restore | 550 | 550 | 1100 |
| Linux 真实 LLVM-full 双环境事务 | 958 | 958 | 1916 |
| DAP wasip1 协议回归 | 28 | 28 | 56 |

本轮修复回归合计 20,086 项检查；修复前真实缺陷复现的 18 项检查另记。每仓库均测试 instruction 与 unwind 两种 debug 策略。单环境普通布局每策略 save 51、restore 83；sparse 保留槽每策略 save 52、restore 89。双环境每策略 479。

实际 sparse restore 验证：目标 renumber_map 包含 INT32_MAX 空槽、没有输入能力、close_pos 仍是 SIZE_MAX，活跃文件别名仍共享 native RC 和 cursor；游标恢复到 128，guest 通过另一个别名移动到 129 后观察到共享偏移，最终正常返回 7。目标文件仍保留 TARGET-CONTENT-UNCHANGED，源文件内容不写入 checkpoint。

对目标已有 sparse reserved cell 移动 FD 号或改变 null-resource 构造状态的导入返回 capability_denied；拒绝后再次 capture，完整 wire 与之前逐字节一致。成功恢复、达到 65536 scan-cell 边界以及恢复原布局后仍可再 capture。65537 scan-cell 被原子拒绝。

Linux 与 macOS 两仓库实际落盘的两种 sparse reserved 元数据，共四组文件比较全部逐字节一致；这是 wire 格式一致性检查，未据此声称跨 OS native capsule 整体恢复通过。

macOS 编译/测试的保守进程内存上界最大 1,052,917,760 字节，小于 2 GiB。所有成功 Linux/QEMU guard 的原共享 cgroup memory.events 前后保持一致，且本任务认证的子进程全部 pidfd retired。一次并发内存增长触发共享 1 GiB reserve 的 ROS 编译中止另行保留，未归因于源码问题；随后完整重编译成功。最终真实单环境允许最多 4.5 GiB 自有进程 RSS、双环境最多 3 GiB，外层原 64 GiB cgroup/swap=0 与 63 GiB 全局余量保护保持生效。


## 测试范围

四 OS 本轮验证共同 portable codec、实际 native 文件保存/加载及拒绝路径。完整真实 WASIp1 capsule 的 save/restore 与双环境事务在 Linux LLVM-full 上验证。本轮没有重跑完整跨 OS LLVM capsule 恢复矩阵，codec 结果不代表整机 Wasm 状态恢复已重新认证。

Wasm checkpoint 仍须同时保存 WASIp1 checkpoint；portable 保存环境状态和 FD 元数据，不带文件内容。目标机器仍须重新配置 mount 与必要的显式 FD rebind。

## 并发修改与测试输入

Linux/Windows QEMU/FreeBSD QEMU 都在原 64 GiB cgroup 内运行，swap=0。本机 macOS 编译/测试的进程内存保守上界低于 2 GiB。仅管理本任务认证的子进程，其他 agent 的进程与文件不被清理。

本轮使用冻结源码及真实 SDK，保存完整 compiler dependency hashes、对象/二进制 hash、命令、guest nonce、serial output、KVM enabled、base image 前后状态、exit status、pidfd retirement 与内存观察。

第一次 ROS runtime 编译遇到并发发布的不完整快照：runtime.default.cpp 已引用 uwvm_runtime_debug_native_stack.h，但初始枚举漏收该新增文件。补齐这个 peer 依赖，不修改它的实现；保留初始 manifest 与编译失败日志。已通过的 uwvm2 测试仅在依赖/产物 hash 再核对后复用，ROS 用完整依赖重新编译。

证据采集时，本次 12 个修改文件的工作树 hash 全部等于已测试输入。另有 18 个已枚举路径被其他并发修改改变，以及 2 个新源文件；这些不计入本次修复，也不声称已测试。before/post 快照之间另有 12 个 peer 变化，完整列在 JSON。

## 可恢复证据

详细结果 JSON：wasip1_checkpoint_sparse_reserved_test_results.json。

SSH Linux 持久化目录：

/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-sparse-reserved-20261006-r6

before/post 源码、fixture、测试脚本、编译依赖与日志归档：test-inputs-evidence-1791219544871183055.tar.gz
- bytes：26287338
- SHA-256：deafa768c11fbcb9092a043a31f365c8a884b5581c5dd8342c849e9865be1531
- 共 12819 项文件；归档中 canonical test-results-complete.json 的 SHA-256 是 0cb1cb6abd0b601ef65ac2af04eab8379ae13d9373c6768613ceed9e6ecc3b5c。
- 真正 LLVM SDK 依赖归档：/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-checkpoint-reboot-20261005-r3/test-environment-1791210330404465482.tar.gz
- SDK 依赖归档 SHA-256：3cb41246b39b4a9e15aefd7b30c33cbd0feda8466753edeff9f71f6273b4e49f

还保存最终 test-report.md、final-results.json 与 delivery-receipt.json，并在独立 cgroup guard 内核对其 hash。归档排除可重建的二进制/对象及私有 VM overlay/ISO/UEFI vars；这些的已测 hash、真实构建命令、输入与 immutable guest base 路径仍在证据中。

上一轮 R5 的 optional 编译产物与私有 VM 文件经逐项压缩验证后释放 1,112,126,024 字节；临时压缩包用于便利恢复，R5 持久化源码/SDK 归档仍保留重建依据。没有删除其他 agent 的产物或改动 guest base。

重启恢复时先恢复被引用的 SDK 与本轮源码归档，保留 tar hardlinks，并按保存命令重建。guard 内的 boot ID/cgroup anchor 是本轮历史身份，重启后须重新认证原测试 cgroup 后再运行。

