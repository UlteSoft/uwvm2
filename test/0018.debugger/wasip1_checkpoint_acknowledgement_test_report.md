# WASIp1 checkpoint 回执修复与跨平台测试（R19）

日期：2026-10-06。`uwvm2` 与 `uwvm2-ros` 已同步修改。

## 修复内容

普通 DAP `saveCheckpoint`、`restoreCheckpoint`、`dropCheckpoint` 原先只在收到附加文本时校验 checkpoint 元数据，因此仅含 `status=ok` 的报头会被误当作成功。带完整元数据但 `applied=0` 的 `ok` 回执也会被接受。修复前在原 64GiB cgroup 中对两仓库各复现 72 种情况，共 144 次。

现在成功回执必须同时满足：`applied=1`、匹配请求的槽位、资源计数总和不超过 65536、`external-io-rollback=false`，并包含原生格式化器的完整提醒：

> checkpoint Wasm and WASIp1 together at the same cooperative stop; WASIp1-only restore does not restore Wasm state.

缺失、截断、错配或矛盾的回执返回 DAP 协议错误；旧停止状态缓存保持失效，适配器不会重试。协议错误不能证明原生操作没有执行，重试前应检查状态。提前拒绝的非 `ok`、`applied=0` 回执仍允许没有 checkpoint 后缀。若原生操作已提交后其他控制端恢复运行，仍返回 `applied=true` 与联合 checkpoint 提醒，同时标记 `stopCurrent=false`。

## 本轮验证范围与结果

新增 C++ fixture 使用 FastIO 的字符串、输出与格式化器，覆盖 8 个槽位、3 种成功资源计数和 2 种拒绝状态，每个平台每仓库产生 40 份原生报文。原生报文在四个平台逐字段、逐字节一致。Python 用例走实际 DAP request/response framing 和适配器代码，broker 为测试替身。

| 原生报文平台 | 两仓库原生格式化检查 | 两仓库 DAP 场景 | DAP 执行位置 |
|---|---:|---:|---|
| Linux | 80 | 1680 | 原 64GiB Linux cgroup |
| Windows 11 | 80 | 1680 | 原生 C++ 在真实 KVM 客机；实际报文回放至 Linux DAP |
| FreeBSD 15.1 | 80 | 1680 | 原生 C++ 在真实 KVM 客机；实际报文回放至 Linux DAP |
| macOS | 80 | 1680 | 原生 C++ 与 DAP 均在本机 |

另有 Linux 既有 DAP 用例每仓库 28 项，共 56 项。总计 **7096 次检查通过**；公共协议场景按仓库、平台重复计数，不代表 7096 个不同功能。无跳过的原生报文用例。

每仓库每份平台报文执行 840 个 DAP 场景：336 个缺失/错误元数据回执、48 个 `ok/applied=0` 矛盾回执、120 个合法资源边界/提交后恢复运行场景、216 个拒绝状态场景、120 个实际原生报文解析场景。验证每个修改请求只发送一次，错误不刷新为可信停止状态，成功回执保留对应槽位和联合 checkpoint 提醒。

本轮未重建完整 CLI、LLVM JIT runtime 或 C++ named modules；未执行真实 checkpoint 保存/恢复，也未重新测试文件资源恢复、全 Wasm 状态恢复或整个 DBG 功能。此报告仅覆盖本次回执契约修复。

## 资源约束与隔离

复用 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17` 及原有 `memory.max=64GiB`、`memory.swap.max=0` 的 cgroup；所有 Linux 测试和两台 VM 都由原 guard 准入，套件通过本任务 flock 串行运行。两台 VM 各 2GiB 客机内存、2 vCPU。包含 QEMU 与本任务控制进程的实际聚合 RSS 上界：Windows 2225995776 字节；FreeBSD 1381978112 字节。macOS 编译最大保守聚合 RSS 上界 1023426560 字节，低于 2GiB；测试进程禁止 fork。

复用一份 Windows SDK、一份 FreeBSD SDK、既有 Windows 基础盘和已验证的只读 FreeBSD clean base，不复制工具链或大基础镜像。两份本轮临时 overlay、ISO、Windows vars 在成功验证后删除，保留原生日志、nonce、二进制、依赖 SHA256 和回执。原始盘前后 stat 相同。硬容量仍为 8GiB，当前已用 3825733632 字节，剩余 4658782208 字节；磁盘映像实际分配 4339654656 字节。

所有正式测试 guard 均确认已绑定子进程通过 pidfd 退休；cgroup `memory.events` 前后相同，OOM 为 0。没有接管、终止其他 agent 的进程或覆盖其他 agent 的修改。

首次测试调度因 SSH 字符串转义问题未切到新入口，旧组件路径的 KVM 探针退出 1；该次失败未计入通过数量，失败回执保留。随后修复测试调度，正式基线、编译、原生测试和回放均通过。

## 可复核证据

本目录的 `wasip1_checkpoint_acknowledgement_test_results.json` 记录源文件摘要、依赖摘要、实际命令、编译与执行日志、VM 原生串口 nonce 与 guard 结果。远端 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-checkpoint-ack-20261006-r19/evidence.tar.gz` 保存修复前/后冻结源码、二进制、日志、SDK 清单和 guard；不包含大 SDK、基础盘或临时 overlay。归档清单逐文件校验，交付回执记录归档摘要。
