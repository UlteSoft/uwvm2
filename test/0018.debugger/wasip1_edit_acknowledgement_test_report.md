# WASIp1 编辑回包确认修复与跨平台测试（R21）

日期：2026-10-06。`uwvm2` 与 `uwvm2-ros` 同步修复，5584 次修复后检查通过。

## 问题与修复

`uwvm/wasip1Edit` 对 checkpoint 回包已要求成功时 `applied=1`，但 replace/insert/remove argument、set/remove environment、reduce rights、create/duplicate/close descriptor 九种编辑仍接受 `status=ok applied=0`，返回 DAP success=true、available=true，即把未确认执行的编辑作为可用结果。

修复前冻结输入在原 Linux 64GiB cgroup 中对两仓库复现，每仓库九种操作，共 18 个场景。它们都在未确认提交的情况下被接受，随后查询 status。

现在在共用 header 校验阶段要求 `status=ok` 与 `applied=1` 一致；任何非 ok 状态必须 `applied=0`。所有编辑使用同一规则，删除三个 checkpoint 分支中重复的 applied 校验，保留其完整元数据、资源数量、slot、原子组声明和 Wasm/WASIp1 配对提醒校验。

矛盾回包成为 DAP 错误；已失效的缓存 stop 引用继续保持失效，没有自动重试或信任后续 status。明确的拒绝状态仍可返回 nonapplied/available=false，便于用户查看原因。完整成功回包在提交后宿主恢复运行时仍返回 applied=true，stopCurrent=false，避免误导再次执行。此改动加强回包验证，未改变宿主编辑或 checkpoint 恢复算法。

## 测试覆盖

新增 `test/0013.debugger/wasip1_edit_acknowledgement.cc`，调用产品 `wasip1_state::print`，通过 FastIO strings/print 在每个实际 OS 生成 36 个原生回包：九种操作乘四种 status/applied 组合。逐字节验证格式，使用 UINT64_MAX epoch 和 INT32_MAX guest FD 边界。该夹具只使用 detached DATA，不构造 VM 管理权限或执行实际编辑。

新增 `test/0018.debugger/test_dap_wasip1_edit_acknowledgement.py`，使用真实 Adapter.handle 输出和 DAP framing，broker 为 transport double。每次完整套件执行 435 个场景：九种编辑与全部 18 个状态及两种 applied 值的组合、成功后 resume、错误 stop/module、epoch/total 越界、未知状态、额外数据、FD 元数据缺失或越界、36 个来自原生 formatter 的回包。拒绝场景检查只发送一次编辑、没有 retry、没有随后的 status，并保持缓存失效。

| 原生回包平台 | 原生编辑格式检查（两仓库） | 编辑 DAP 场景（两仓库） | 执行位置 |
|---|---:|---:|---|
| Linux | 72 | 870 | 原 64GiB cgroup |
| macOS | 72 | 870 | 本机，聚合内存限制 2GiB |
| Windows 11 | 72 | 870 | 原生格式检查在真实 KVM 客机；回包解析在 Linux cgroup |
| FreeBSD 15.1 | 72 | 870 | 原生格式检查在真实 KVM 客机；回包解析在 Linux cgroup |

此外在 Linux cgroup 对两仓库重新编译既有 checkpoint 原生 formatter，80 项检查通过；用本轮回包重新执行 save/restore/drop、八个 slot、元数据缺失/错误、配对提醒缺失、矛盾 applied、资源边界和拒绝/提交后 resume，1680 场景通过。既有 WASIp1 DAP 套件另有 56 个测试通过。合计 **5584 次检查观测**，按仓库和平台累计，包含重复场景；18 项旧问题复现单独记录。

Windows nonce `3ed5e3111dcd0b54361d78c006e61993`；FreeBSD nonce `ca1b3802ed3770918bbdcd4a7fc40321`。串口包含本次输入二进制 SHA256、两个原生程序的精确 36 项 PASS 和退出 0；两台 QEMU 退出 0，KVM 实际启用，基础盘前后 stat 相同。回包只从本次 nonce 界定的真实串口区间提取，验证其摘要后交给修复后的适配器。

本轮验证 DAP 回包协议和四 OS 原生 formatter 兼容性。Python 没有在 Windows/FreeBSD 客机运行；没有重建完整 CLI、LLVM JIT runtime 或 named modules，也没有执行实际 Wasm/WASIp1 checkpoint 恢复或实际 guest FD 增删。435 个协议场景不是 435 次真实环境修改。

## 资源与占用

复用 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17`、原有 64GiB swap=0 cgroup、共享 SDK 与只读基础镜像。Linux 编译、原生测试和回包解析都经过原 guard 准入，并通过本任务 flock 串行执行。两台 VM 各 2GiB、2 vCPU；QEMU 与本任务控制进程的聚合 RSS 上界分别为 Windows 2225270784 字节、FreeBSD 1363832832 字节。macOS 最大保守聚合编译 RSS 上界 1021083648 字节，小于 2GiB；原生和 Python 测试均使用禁止 fork 的 sandbox。

VM overlay、ISO 和 Windows vars 在验证成功后删除；没有复制 SDK 或基础镜像。私有卷硬容量 8GiB，归档前占用 4000452608 字节，剩余 4484063232 字节，后端镜像实际分配 4647116800 字节。归档后最新占用见交付回执。

全部正式 guard 确认自己的子进程通过 pidfd 退休，cgroup memory.events 前后相同，没有 OOM。其他 agent 的进程和源码未接管、终止或回滚。

本机保存 post 快照时磁盘满而中断，未计入测试通过数。重新核对 Linux 上 R19/R20 的完整归档 SHA256，并校验自己的旧本地冻结源码摘要后，仅删除这些已有远端完整备份的本任务临时副本。继续保存 R21 快照，没有回滚两仓库已写入的修复，当前自有文件摘要与受测快照一致。恢复过程和脚本保存在证据中。

## 证据

同目录 `wasip1_edit_acknowledgement_test_results.json` 保存编译命令、冻结输入、依赖与二进制摘要、原生回包、平台执行与资源回执。远端 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-edit-ack-20261006-r21/evidence.tar.gz` 保存实际测试证据，逐文件读回校验；不包含大 SDK、基础镜像或临时 overlay。交付回执记录归档摘要及最终占用。
