# WASIp1 checkpoint 导出失败清理与跨平台测试（R23）

日期：2026-10-06。`uwvm2` 与 `uwvm2-ros` 同步修复。

## 修复

原先单个和整组 portable 导出在写入或关闭失败后留下已独占创建的目标文件；同一路径的重试因此被拒绝。部分写入失败还会依赖析构关闭，无法检查关闭状态。

两个导出接口现在共用 FastIO `write_exclusive_file`：独占创建后记录新 regular file 的 device/inode，写入和关闭都需成功才能返回。失败时对仍持有的文件执行检查关闭，随后通过 FastIO no-follow status 核对当前路径身份，匹配时尝试 unlink。清理失败不会掩盖原始异常；已有目标不会获得本次导出的所有权。文件名已指向另一份文件时保留替换文件。所有产品 I/O 使用 FastIO，无生产测试钩子。

这是尽力清理，不是原子发布。status 与 unlink 之间没有跨进程排他锁，调用者必须保证 checkpoint 路径不被其他写入者同时 rename/rebind。身份未知、清理权限不足等情况仍可能留文件；需要检查后再重试。断电持久化仍需独立同步策略。文件格式及导入权限规则未变；没有加入宿主文件内容。

## 实测

新增 `test/0013.debugger/wasip1_portable_export_cleanup.cc` 在真实 native_file 上读写、关闭、status、重命名及删除。测试限定 token substitution 只用于 portable helper：部分字节已实际写入后注入 EIO，真正关闭完成后注入 EIO，或清理 unlink 注入 EACCES。另一个用例把原文件移到 `.kept` 后在原路径创建另一份文件，验证身份变化时内容不被清理。最后验证原始 EIO、清理失败后的文件保留、成功清理后的原路径重试、已有文件独占保护、正常 single/group 往返和非法输入不创建文件。

修复前在原 cgroup 跑两个仓库，每个 995 项，共 **1990 项基线观察**，复现旧文件残留与写入错误后未检查关闭。基线通过表示旧行为被成功复现。

| 平台 | 新 helper 检查（两个仓库） | 本轮额外回归（两个仓库） | 环境 |
|---|---:|---:|---|
| Linux | 1990 | codec 4254、路径 560、旧 close 130、DAP 56 | 原 64 GiB swap=0 cgroup |
| Windows 11 | 1990 | — | 真正 KVM 客机，原生 NT 文件后端 |
| FreeBSD 15.1 | 1990 | — | 真正 KVM 客机，原生 POSIX 文件后端 |
| macOS | 1990 | — | 本机，聚合 RSS 上界受 2 GiB 限制 |

合计 **12960 次检查通过**，包含仓库、平台和重复运行的重复场景，不等于独立功能数量。Windows nonce `ef2ab4802bfa32c98e9924dba9a7a276`，FreeBSD nonce `561b7bdaaac1696d83c44d5805c2b718`；两个客机每仓库均报告精确 995 PASS、退出 0。KVM 实际启用，QEMU 退出 0，基础镜像前后 stat 相同，私有 overlay、ISO 和 Windows vars 验证后已删除。

两个仓库还通过包含真实 LLVM 头文件的完整 controller header 语法编译，依赖 SHA256 固定；这两次编译没有计为运行测试。此轮没有重新链接运行完整 CLI/LLVM JIT，没有测试活跃 Wasm/WASI 环境恢复、named modules、真实磁盘满或硬件 I/O 失败。新测试证明四个 OS 的原生文件 helper 及注入的错误处理；没有把替身视为真实运行时。

## 资源与证据

复用 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17` 的 8 GiB 私有 ext4 卷、现有 SDK、基础镜像和 64 GiB swap=0 cgroup。Linux 测试和两个 VM 都经过 guard 与任务 flock。VM 每台客机 2 GiB、2 vCPU；本任务聚合 RSS 上界 Windows 2226556928 字节、FreeBSD 1236611072 字节。本机 macOS 保守聚合编译 RSS 上界 1005273088 字节，测试禁止 fork。所有通过的 guard 都记录 memory.events 前后相同，已绑定子进程均通过 pidfd 退休。未接管或终止其他 agent 的进程，未回滚其源码。

归档前私有卷占用 4390195200 字节、剩余 4094320640 字节；后端镜像实际分配 5239472128 字节。归档后占用另记交付回执。本机冻结源使用 APFS clone，归档从 Linux 流式校验，不另存大副本。验证后仅退休本轮自己的冻结备份与上传包。

两次未通过的测试工具启动保留且排除计数：一次启动早于新 dispatcher 上传而进入旧默认 suite，因 KVM 权限被拒绝；一次新夹具因 FastIO 错误类型、byte 指针和字符串输出类型不正确而编译被拒绝。均发生在产品修改前，修正并重新运行基线后才应用产品修改。

远端 `/home/macromodel/Documents/uwvm3-implementation/wasip1-recovery-20261006-r17/rounds/wasip1-export-cleanup-20261006-r23/evidence.tar.gz` 保存冻结前后源码、二进制、真实 OS 日志、VM nonce、编译依赖、SDK 清单和 guard，不复制 SDK 或基础盘。逐文件读回并校验 SHA256，交付端再次流式读回完整归档。对应 results JSON 和 delivery receipt 位于本报告同目录。
