# WASIp1 checkpoint controller 修复与测试（R22）

日期：2026-10-06。两个仓库同步修复，**19096 次修复后检查观测通过**。重复场景按仓库和平台分别累计；两次完整 controller 语法检查另记。

## 修复

旧代码在保存时先替换旧槽位，再调用可能分配内存的 detached 元数据复制 API。复制失败被忽略，旧 checkpoint 已丢失，仍回复 ok/applied=1。恢复先读取元数据，提交后再读取一次；第二次复制失败会把回复 epoch 留为 0。删除则在写 epoch 之前返回，成功回复总是 epoch=0。恢复/删除的首次复制失败还被一律误报 entry not found。

现在保存先验证新 capsule 的模块和元数据，再发布槽位；失败保留旧 owner。恢复和删除先验证并保存资源计数和 recorded epoch，再修改状态。恢复提交后不再分配复制元数据。allocation failed 按原状态报告，失败不替换或删除槽位。capture 失败不再查询无关旧槽位元数据。DAP 对 save/restore/drop 的成功回包要求非零 recorded epoch；明确拒绝可携带 epoch=0。完整成功回复即使随后 resumed 仍保留 applied=true，避免重试。

原生 capture 源码明确拒绝 epoch=0，因此这个协议约束符合真实 capsule 契约。Wasm 与 WASIp1 必须在同一 cooperative stop 一起 checkpoint 的提醒继续保留。wasip1-only restore 不恢复 Wasm 内存、全局、栈等状态。

## 证据与测试范围

`checkpoint_controller_fragment.py` 从冻结 `controller.h` 的唯一边界提取完整 checkpoint 分支，逐字节纳入 C++ 夹具，并记录完整文件和分支 SHA256。夹具只把 runtime 名称映射到私有测试 namespace 的 API-result doubles；真实 VM native API、pause/host 权限和 capsule registry 没有被伪造或绕过。真实 `wasip1_state::view/print` 通过 FastIO strings/print 生成 72 个回包。完整 controller 还在 Linux 对真实公开 runtime 类型通过 syntax-only 检查，含 UWVM_USE_LLVM_JIT；这没有链接完整 JIT。

两仓库各 1920 项原生断言：八个槽位、三种操作、epoch 1/7/UINT64_MAX；复制 allocation_failed/invalid_owner、capture/restore 状态映射、错误模块、空槽位、strict 传递、只修改选中槽位、保留另外七个 owner、旧/新 owner 退休、验证发生在发布之前、恢复提交后没有第二次 fallible copy。失效结果由受控 API-return 注入；没有制造真实内存耗尽。

每仓库每平台的 72 个原生回包进入真实 Adapter.handle 和 DAP framing，broker 为 transport double，240 个新协议场景覆盖 recorded epoch、成功后 resume、zero-epoch 拒绝、明确拒绝的 zero-epoch。Windows/FreeBSD 解析在 Linux cgroup 执行，Python 未在客机运行。

| 原生平台 | controller 断言（两仓库） | 新 DAP 场景（两仓库） | 执行位置 |
|---|---:|---:|---|
| Linux | 3840 | 480 | 原 64GiB cgroup |
| macOS | 3840 | 480 | 本机，聚合限制 2GiB |
| Windows 11 | 3840 | 480 | 真正 KVM 客机执行原生程序；Linux 解析串口回包 |
| FreeBSD 15.1 | 3840 | 480 | 真正 KVM 客机执行原生程序；Linux 解析串口回包 |

另有既有 checkpoint formatter 80 项、旧 checkpoint DAP 1680 场景、既有 WASIp1 DAP 56 项通过。合计 19096 次观测；修复前两仓库各 1920 原生断言和 24 协议复现，3888 次单独记录。

Windows nonce `316f4578f1c07154a4f4684ffbbb627b`；FreeBSD nonce `56981b4e2e7e9ec70b761153f3944395`。两仓库客机测试精确 1920 PASS、退出 0；QEMU 退出 0，KVM 实际启用，基础盘 stat 前后相同。只从本次 nonce 的原生串口区间提取回包，并校验 SHA256。

本轮不证明真实 live Wasm/WASIp1 checkpoint 恢复、完整 CLI/JIT 链接、全状态恢复或跨 OS 环境导入。scope：Exact product checkpoint controller branch with private-namespace API-result doubles and authentic product formatter compiled/executed natively on Linux, macOS, Windows and FreeBSD. Real DAP adapter with transport doubles. Full product controller header typechecked against actual LLVM runtime declarations on Linux. No full CLI/JIT link or live Wasm/WASIp1 restore; allocation failures are injected API results, not real OS OOM.

## 并发修改后的交付复验

首次交付检查时，另一个 agent 修改了两仓库 `dap_adapter.py` 的 Wasm root/member 查询与 cached stop 校验。完整 WASIp1 edit 方法逐字节未变。本轮保留全部并发修改，将最新适配器摘要固定在 integration-inputs 中，在原 Linux cgroup 对四 OS 原生回包执行 1920 个新协议场景，并执行旧 checkpoint 1680 场景和既有 WASIp1 56 个测试，**额外 3656 次回归全部通过**，单独计数。没有重跑已验证且未改动的原生程序。后来另一个 agent 又只修改了语言级 `validate_source_evaluation_expression` 函数。核对函数签名和模块其余所有字节，确认整个 WASIp1 路径与已通过 3656 项回归的版本相同；该后续语言修改不属于本轮测试或修复。最终 own_source 对应交付文件，integration_recheck 对应实际协议受测版本，own_source_frozen 对应首次四 OS 测试快照；三版本与差异均保存。

报告交付还遇到本机 ENOSPC。流式逐文件核验完整 Linux 归档，并比对本地冻结文件后，仅删除 R22 自有本地快照和已验证的压缩上传副本。产品源码、原生结果及其他 agent 文件保留。恢复记录在交付回执中。

## 限制、失败尝试与占用

复用 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17`、原 64GiB swap=0 cgroup、共享 SDK 与基础盘。原生夹具/协议/交叉编译使用现有 2GiB owned-process guard；完整 controller 首次检查超过这一预算，被 guard 只终止本任务进程，未发生 cgroup OOM。随后通过现有 runtime-tests 的 4.5GiB guard 重试，两仓库完成。未更改 guard 或共享 cgroup 限制。失败尝试日志和回执保留。

最初 baseline 夹具误将旧 capture-failure 路径的元数据复制次数预期为 0，测试在这一断言失败。修正测试对旧行为的预期后，未修改产品的 baseline 全部通过，之后才实施修复。失败尝试的原始输入摘要、完整归档与输出保存。这个失败不计入通过数。

macOS 编译最大保守聚合 RSS 上界 1020575744 字节，小于 2GiB。Windows/FreeBSD VM 各 2GiB/2 vCPU，包含本任务控制进程的 RSS 上界分别 2227294208 / 1351766016 字节。全部合格 guard 确认 owned pidfd 退休及 cgroup memory.events 未增加；没有接管其他 agent 进程或回滚其他源码。

私有卷硬容量 8GiB，归档前占用 4226396160 字节，剩余 4258119680 字节，后端实际分配 5211615232 字节。合格后删除本次 overlay、ISO、Windows vars。没有复制 SDK/基础盘。本机已核验 R21 完整远端归档及本地输入摘要，仅清理其自有旧快照。归档后占用见交付回执。

`wasip1_checkpoint_controller_test_results.json` 与交付回执包含源码、命令、依赖、二进制、原生回包、平台/资源证据。远端 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-checkpoint-controller-20261006-r22/evidence.tar.gz` 逐文件读回核验；不包含大 SDK、基础盘或临时 overlay。
