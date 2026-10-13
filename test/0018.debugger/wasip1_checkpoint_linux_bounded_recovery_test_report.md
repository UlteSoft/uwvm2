Linux 测试环境已恢复，并为 WASIp1 checkpoint 任务建立了可以持续复用的、有容量上限的工作目录。2026-10-06 14:31（Asia/Shanghai）采样。此次没有修改产品源码；此前 R16 修复涉及的 12 个文件仍与两仓库原测试输入的 SHA-256 一致。

复用了另一个 agent 已恢复的 `uwvm-debug-tests64g-20261006-r2` 容器。最初观察到的旧容器在上传期间被外部停止，旧锚点 44190/70295 已失效，尚未启动测试工作负载；重新核验并使用以下当前身份：

- Boot ID：`7458ba8f-e0eb-4fff-be38-97fc93aab47a`。
- 容器 ID：`d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03`；锚点 PID `45005`、birth `74453`。
- Cgroup：`/sys/fs/cgroup/system.slice/docker-d3b12e6d323a0f28c2408989fff41a1ca1a991e19945919bc870337dd0afae03.scope`。
- `memory.max = 68719476736`（64 GiB），`memory.swap.max = 0`。
- CPU 集合 `0,2,4,6,16-31`；本任务测试进程使用 16–31。

工作目录为 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17`。其单独的 ext4 测试卷容量为 **8 GiB**，对应镜像 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17.ext4`，UUID `ae4d0a7f-dff9-493d-a216-82058227ed0e`。镜像按实际使用分配空间，8 GiB 是容量上限。采样时镜像实际占用 **117,260,288 字节（111.8 MiB）**，目录文件约 67.1 MiB，其中包含源码、日志、测试程序和经过逐文件校验的证据归档；文件系统元数据另计在镜像实际占用中。

运行入口 `guard.py` 串行调度本任务的测试，启动前核验本次 boot、锚点 birth、实际 cgroup 和资源限制。实际测试进程先停止，再进入该 cgroup，确认后恢复运行。只跟踪本任务创建的进程，通过 pidfd 清退；其他 agent 的进程仅观察，不接管或发信号。

- 宿主持久盘可用空间低于 **32 GiB** 时拒绝启动或中止本任务；采样时约 **122.2 GiB** 可用。
- 目录超过 6 GiB 时拒绝新任务，运行期间超过 7 GiB 时中止；硬容量仍为 8 GiB。临时目录上限监控为 2 GiB，并保留本卷 512 MiB 空间。
- `TMPDIR/TMP/TEMP` 指向测试卷内的 `tmp`，避免使用 `/tmp` 的 tmpfs 存放编译产物。采样时 `/tmp` 约 45.9 GiB 可用。
- 单个文件由内核 `RLIMIT_FSIZE` 限制为最多 1 GiB，禁用 core dump。目录、内存和剩余空间监控是采样阈值，文件系统总容量和单文件大小限制由内核执行。
- 本轮组件测试自身 RSS 停止阈值 2 GiB，实际累计采样峰值 **652.8 MiB**。共享 cgroup 启动阈值 54 GiB，运行停止阈值 60 GiB；宿主可用内存至少保留 8 GiB / 4 GiB（启动 / 运行）。

重新编译并运行了两仓库的 `wasip1_rebinding_validation.cc`，**各 164 项，共 328 项通过**。这包含 debugger 命令解析、rebinding 参数边界以及 FastIO 原生文件读写。测试使用此前已固定并校验的 R16 输入，相关本机修复文件仍匹配；没有声称本轮重新验证了其他 agent 的全部最新改动、完整 CLI 或四个操作系统的 checkpoint 恢复。

新 cgroup 起初没有放行 KVM。恢复配置仅给该 cgroup 的字符设备 `/dev/kvm`（10:232）增加读写许可，保留原有设备过滤程序作为逐字节一致的其他请求处理路径。原规则和新规则的 4,704 项设备判定校验通过，配置回执包含原程序及哈希。此许可属于运行时配置，容器重启后需要重新核验并应用。

在同一 cgroup 内，实际打开 KVM 接口，核验 API 版本 12，并由 QEMU 10.2.1 创建 128 MiB 虚拟机，QMP `query-kvm` 返回 `enabled=true, present=true`，正常退出。使用已有 `qboot.rom`；该核验**没有启动 Windows 或 FreeBSD 来宾 OS**。另以 64 KiB 的临时单文件硬限制尝试写入 1 MiB，内核拒绝继续写入，文件实际为 65,536 字节，验证后删除。

组件、文件限制、重复目录清退和归档四组最终守卫均通过，所有本任务 pidfd 均确认退出；组件测试期间共享 cgroup 的 memory.events 未变。恢复过程中的失败尝试保留在 `guard-history-*.json` 中；部分测试失败的 stderr 在任务记录中，守卫历史本身只记录退出状态。失败包括迁移助手写权限、先前测试目录的独占文件冲突、KVM 未放行及默认 BIOS 文件缺失，已修正后重测通过。

逐文件核验后清退了本任务的重复上传目录，释放约 48.5 MiB 文件分配空间。没有复制 LLVM 工具链、VM 基础镜像或创建本轮 VM 磁盘。后续继续复用此目录；活动入口记录在 `/home/macromodel/Documents/uwvm3-implementation/wasip1-active-environment.json`。

原 Linux 测试目录已清理，旧完整归档和 LLVM ros.11、Windows 交叉 SDK、FreeBSD 测试 sysroot 在原路径不存在，尚未恢复它们。Windows 和 FreeBSD 原始 VM 镜像仍在 Documents/qemu。此次从本机恢复了 R16 选定源码、原始结果和报告；不能将它们等同于恢复旧完整归档。后续需要这些 SDK 时应只恢复一份共享副本，按需使用。

当前恢复证据：`/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/evidence/recovery-evidence.tar.gz`，15,383,302 字节，2,352 个文件，SHA-256 `e51076f1d5d1d0e9973a12669b695c22d68a8c0cd6b1e2b91a30e7a16cbf0744`。Linux 和本机均重新读取全部归档文件并核对 SHA-256。原始恢复结果与本报告一起保存至两个仓库；最终交付文件另有校验回执。

当前复测入口：

```sh
ssh linux 'python3 /home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/guard.py components'
```

再次重启后，先通过 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17-remount.py` 验证镜像 UUID 并重新挂载（不会重新格式化），再重新核验新的 boot、锚点 PID/birth 和 cgroup，刷新守卫身份及 KVM 配置；旧守卫会拒绝使用失效身份。
