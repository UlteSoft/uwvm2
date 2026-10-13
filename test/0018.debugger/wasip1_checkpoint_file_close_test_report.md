# WASIp1 portable checkpoint 文件关闭错误修复与跨平台测试（R20）

日期：2026-10-06。`uwvm2` 与 `uwvm2-ros` 同步修复。

## 问题与修复

`save_file`、`save_group_file` 原先在写完字节后直接返回 true，文件随后通过 FastIO 析构关闭。FastIO 析构会丢弃关闭状态，因此关闭阶段的原生 I/O 错误无法阻止成功回执。`load_file`、`load_group_file` 原先在关闭前 decode 并替换输出，最后关闭错误也不会被处理。

四个接口现在显式调用 FastIO `file.close()`。导出完成检查后才返回成功；导入在完成所有可能失败的文件 I/O 后才 decode 并发布输出。关闭错误作为 `fast_io::error` 向上抛出。既有单个/整组 controller 源码均在文件 helper 调用外捕获该错误并设置 `native_operation_failed`，且 `mutation_applied` 只在 helper 成功之后设置。导入关闭失败不会到达环境恢复调用。

独占创建行为保留：写入或关闭报错时已经创建的文件可能仍然存在，甚至包含完整且有效的元数据。检查文件后再决定下一步；同一路径的再次导出不能覆盖它。此修复检查最终关闭结果；断电持久化需要独立的同步策略。

## 如何复现与验证

新增 `test/0013.debugger/wasip1_portable_file_close.cc`。它使用真实 FastIO native_file 完成打开、status、读、写及关闭，只在测试翻译单元中把 portable helper 的四个文件所有者替换为派生探针，成功原生 close 后按开关注入一个 FastIO EIO 错误。所有前置头文件先加载，token substitution 仅影响四个文件声明；产品接口没有添加测试钩子。探针记录检查关闭、未检查析构关闭、错误次数，并确认原生所有者已失效，避免重复关闭。

修复前在原 64GiB cgroup 对两仓库运行：每仓库 63 项基线检查，共 126 项；复现四接口各仓库均忽略最终关闭错误（8 个具体场景），两个读取接口各仓库均已替换输出（4 个具体场景）。基线通过表示成功复现旧行为。

修复后验证正常单个/整组文件往返、完整元数据和不透明 argv 字节、最终关闭错误传播、导入保留之前的输出、导出报错后文件仍可检查、重试不能覆盖已有文件、非法文件名/非法图不创建所有者，以及错误后不进行第二次关闭。

| 平台 | 新原生文件关闭检查（两仓库） | 其他本轮回归（两仓库） | 执行环境 |
|---|---:|---:|---|
| Linux | 126 | 编解码 4254；文件名准入 560；DAP 56 | 原 64GiB cgroup |
| Windows 11 | 126 | — | 真实 KVM 客机，使用原生 NT 文件后端 |
| FreeBSD 15.1 | 126 | — | 真实 KVM 客机，使用原生 POSIX 文件后端 |
| macOS | 126 | — | 本机原生 POSIX 文件后端，限制 2GiB |

合计 **5374 次检查通过**；按仓库和平台累计包含重复场景。Windows、FreeBSD 串口含本次独立 nonce、输入二进制 SHA256、两个进程的退出 0 与精确 63 项 PASS；两台 VM 退出 0，KVM 实际启用，基础盘前后 stat 相同。

本轮为文件 helper 的原生 I/O 与关闭错误注入回归。没有制造磁盘满、NFS 故障或实际硬件关闭错误；没有重建完整 CLI、LLVM JIT runtime 或 named modules，也没有运行真实 Wasm/WASIp1 环境 checkpoint 恢复。controller 的错误映射依据源码检查，不计为运行时集成测试。

## 资源与文件夹占用

复用 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17`、原有 64GiB swap=0 cgroup、SDK 与只读基础镜像；Linux 编译和测试均经过原 guard 准入。测试套件通过本任务 flock 串行执行。两台 VM 各 2GiB 客机内存、2 vCPU；包含 QEMU 与本任务控制进程的实际聚合 RSS 上界：Windows 2225197056 字节，FreeBSD 1443348480 字节。macOS 最大保守聚合编译 RSS 上界 1004306432 字节，低于 2GiB；测试进程禁止 fork。

本轮的 VM overlay、ISO、Windows vars 已在验证成功后删除，仅保留日志、二进制、冻结输入与回执。没有复制 SDK 或大基础镜像。任务私有卷硬容量 8GiB；归档前占用 3934183424 字节，剩余 4550332416 字节，后端镜像实际分配 4537958400 字节。最后交付记录另存归档后的占用。

所有正式测试 guard 都确认已绑定子进程通过 pidfd 退休；cgroup memory.events 前后相同，OOM 为 0。其他 agent 的进程和源码修改均未接管、终止或回滚。

首次新夹具编译因 FastIO string_view 的显式构造要求被拒绝，修正测试代码后重新执行基线；该次失败保留源码、编译日志与 guard，未计入通过总数。

## 证据

同目录 `wasip1_checkpoint_file_close_test_results.json` 记录冻结源文件、编译依赖、实际命令和原生输出。远端 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-file-close-20261006-r20/evidence.tar.gz` 保存修复前/后源码、二进制、日志、SDK 清单、VM nonce 与 guard，不包含大 SDK、基础盘或临时 overlay。归档逐文件读回校验，交付回执记录摘要及验证结果。
