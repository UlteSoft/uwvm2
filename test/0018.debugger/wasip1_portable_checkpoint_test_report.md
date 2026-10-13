# WASIp1 便携 checkpoint：检查、修复与实际验证

2026-10-05。相关实现、回归与接口说明已同步到 `uwvm2` 和 `../uwvm2-ros`，保留其他 agent 的工作。**本轮冻结源码的两仓库 × 四 OS 完整原生验证、以及全部 48 个有向跨 OS 恢复用例均已通过。并发工作树的其他后续改动另有记录，不能把这些改动计入本轮通过结果。**

[机器可读证据](wasip1_portable_checkpoint_test_results.json) 记录源码、SDK、编译输入、可执行文件、日志、VM 和本任务进程回收证明的 SHA-256。历史结果与最新版结果分别标注。

## 已实现的范围

- CLI `set wasip1 export MODULE HEX_PATH`、`set wasip1 import MODULE HEX_PATH [RESOURCE=TARGET_FD,...]`；DAP `exportPortableCheckpoint`、`importPortableCheckpoint`。
- 保存 argv/env 字节、guest FD 编号与别名、base/inheriting rights、预留空槽、关闭槽分配次序、guest mount 名、规范相对路径、游标和 WASI flags。文件内容、原生 handle/pointer、源 OS 的资源 host 路径、进程 epoch 不进入文件。
- 目标通过自己的 mnt 配置重新打开文件，恢复不使用 CREATE/TRUNC。缺失文件导致拒绝；游标可在目标 EOF 之后，目标内容不变。
- stdio 绑定目标原始通道；匿名/未跟踪的文件、pipe/socket 等要求显式目标 guest FD，校验其真实权限、类型和 flags。
- C++ 文件操作、字符串、输出、整数及二进制解析使用 FastIO，包括有界 `parse_by_scan`。独占创建 checkpoint，拒绝覆盖；加载校验普通文件类型/大小并有界读取。固定宽度 little-endian 格式带版本、保留位和 SHA-256 trailer。
- 导入经过真实 LLVM-full cooperative stop、canonical capture、完整 cohort、host/GC/publication 排除以及当前 source/provider/profile 验证。所有资源打开、校验和分配完成后，才发布 FD 表与 argv/env。
- Wasm 单独 checkpoint 和 WASIp1 操作提醒必须在同一 stop 同时保存 Wasm 与 WASIp1。便携 WASIp1 元数据仍不实现整实例 Wasm 恢复。

详细接口和边界见 [设计说明](../../src/uwvm2/uwvm/debugger/wasip1_checkpoint.md)。不能表示的目标 flags、缺失 mnt、权限不足、非规范目录路径和未匹配外部资源会被拒绝；内容和外部 I/O 效果另行管理。

## 本轮确认并修复的问题

1. **低权限别名遮蔽原始 FD。** 真实 ROS `-Rdbg` 中复制 stdout/preopen，再把副本 rights 降为零，旧实现误选副本而拒绝导入。现逐个选择单个有足够权限的真实 FD；不同别名的权限不拼接。真实公开 CLI 与 native 恢复均已通过此回归。
2. **跨 OS guest 路径解释。** 便携恢复始终启用 WASI UTF-8 检查，不继承目标普通路径的 raw-byte opt-out；坏字节返回实际 `EILSEQ`，避免 Windows 转码选择另一文件。真实 native 两种策略均已通过。
3. **checkpoint 文件名未检查 UTF-8。** 原 cgroup 的原生 codec 实际接受非法文件名并触发新回归失败。现先检查 4096-byte 长度上限和 NUL-free RFC 3629 UTF-8，再分配自有字符串、打开文件。Windows 本机 drive/backslash 语法保留；合法希腊文/中文文件名实际 round-trip 通过四 OS。
4. **DAP portable ACK 自相矛盾。** 两个新回归实际失败：header total 与 resources 不一致，以及 status=ok/applied=0 被接受。现拒绝两者，并在 transport 前拒绝坏 UTF-8 pathHex；拒绝 ACK 后使旧 stop 引用失效。两仓库各 23 个协议测试通过。
5. **path_open 的权限派生错误及遗漏。** 原 cgroup 的 wasm32/64 原生回归实际失败：父目录仅保留 PATH_OPEN、inheriting 仍允许写时，错误地拒绝子文件。现子 base/inheriting 都由父 inheriting 限制；父 base 单独授权 PATH_OPEN、CREAT、TRUNC 和同步选项。CREAT 要求 PATH_CREATE_FILE，TRUNC 要求 PATH_FILESTAT_SET_SIZE；DSYNC 要求 FD_DATASYNC 或 FD_SYNC，RSYNC/SYNC 要求 FD_SYNC。Windows 拒绝无法准确记录的 SYNC，不再成功后静默清除。portable mount 选择采用相同规则，并保持单 FD 授权。这些规则遵循 [官方 WASIp1 rights 说明](https://github.com/WebAssembly/WASI/blob/a2b96e81c0586125cc4dc79a5be0b78d9a059925/legacy/preview1/docs.md#rights)。四 OS 实际执行均已通过操作权限、拒绝无副作用、写入不隐式截断的回归。
6. **native fixture 编译错误。** 新增 FD query 与后续 `fds` 重名，真实编译报错；已改为 `fd_query`。新增父目录权限回归通过真实授权 query/reduce 接口调整 rights；没有伪造 stop、注入 native FD 或绕过 validator。

7. **FreeBSD ELF 宏与 LLVM C++ 常量冲突。** 两仓库实际完整运行库编译均出现 9 个错误。现把 `ODK_GP_GROUP`、`ODK_IDENT`、`ODK_PAGESIZE`、`VERSYM_VERSION`、`VERSYM_HIDDEN` 纳入已有 push/undef/pop 保护，保留其他 agent 增加的 relocation aliases。修复后真实 FreeBSD 完整运行库、CLI 和 native fixtures 均编译、链接、运行通过；其他三 OS 也重新完成相应编译资格验证。

## 实际通过结果

下面计数均为**每仓库**的检查记录，不是 syscall/Wasm 特性数量。两种策略是 `instruction` 与 `unwind`；每个用例运行真实 native 程序，原始 Wasm 经过 parser/validator。

| OS | codec | wasm32/64 文件打开 | 匿名文件提供器 | 公开 CLI | 同进程 checkpoint／每策略 | portable save / restore／每策略 |
|---|---:|---:|---:|---:|---:|---:|
| Linux，原 cgroup | 865 | 107 | 143（既有 OS 验证） | 232，含 broker/DAP | 129 | 25 / 47 |
| macOS，本机 | 865 | 105 | 143 | 84 | 129 | 25 / 47 |
| FreeBSD 15.1，QEMU/KVM | 865 | 105 | 143 | 84 | 129 | 25 / 47 |
| Windows 11，QEMU/KVM | 864 | 99 | 140 | 98 | 124 | 25 / 47 |

Linux 两仓库的公开 CLI/broker/DAP 各 **232 项零失败**；最新 DAP 协议测试各 **23 项通过**。Windows 的 98 项包含原有 NT 输入/继承回归；POSIX/WASI flags 的可用性导致其他检查计数有差异。

原生 portable 回归实际覆盖：低权限 alias 不遮蔽原始 target；只保留 PATH_OPEN 的真实父目录；FD 3/91 的共享游标；stdio/preopen renumber；argv/env 和 allocator closed/reserved slots；重新配置目标 mount；目标文件比原文件短、游标仍恢复到 128；目标内容不变；无隐式 CREATE/TRUNC；坏 UTF-8、缺失文件、不同 Wasm/interface、权限/配额不足、失效 ticket/未跟踪 host activity 均拒绝。拒绝时 target text、FD map 和原 cursor 不提交。恢复后真实 guest 继续调用 `fd_tell`/`fd_seek`，并继续使用 Core3 GC 对象返回预期结果。

全部跨 OS 用例通过。每个非对角格包含 **两仓库 × 两策略 = 4 个实际恢复用例**，每个用例检查 47 项：

| 来源 → 目标 | Linux | Windows | FreeBSD | macOS |
|---|---|---|---|---|
| Linux | 本机 fresh-process 已通过 | 4/4 | 4/4 | 4/4 |
| Windows | 4/4 | 本机 fresh-process 已通过 | 4/4 | 4/4 |
| FreeBSD | 4/4 | 4/4 | 本机 fresh-process 已通过 | 4/4 |
| macOS | 4/4 | 4/4 | 4/4 | 本机 fresh-process 已通过 |

总计 **12 个有向方向 × 两仓库 × 两策略 = 48/48**。源 metadata 是对应 OS 的真实 native 导出；目标是独立 native process，不能用目标自行构造 metadata 代替跨 OS 迁移。Windows/FreeBSD 记录实际 QEMU 正常退出、完整 serial 日志、KVM 和 readonly base 前后 identity；源、目标、编译输入及日志的 hash 均可查。

四 OS 使用真实 LLVM `23.1.1-uwvm-ros.11` SDK；相关 RuntimeDyld 和版本对象由实际源码重建，保留之前 SDK，记录每个 archive 的 hash。SDK 构建资格与完整运行库/CLI 运行资格分别校验，不能互相代替。

## 资源限制与可重复验证

所有非 macOS 的编译、原生测试和 QEMU 均在原 SSH Linux cgroup：64 GiB、swap 0、原 cpuset。本任务重活串行，保护保留 1 GiB 共享余量，只以 PIDFD/出生时间/uid/cgroup 认证并回收自己的子进程，其他 agent 的进程和文件未被清理。

macOS 使用本机真实 arm64 程序，按本任务 2 GiB 进程 RSS 上限监控，独立 native fixture 的最大聚合 RSS 上界为 **121307136 bytes**；公开 CLI 使用同一 OS 的进程树监控。磁盘压力导致的若干传输失败不计作 native 通过。完整 native suite 先用原始完整二进制通过；最后迁移补测用相同 C++ 源码、SDK、runtime/host objects 重新编译链接的私有符号测试程序（约 65 MiB），保留原始程序及独立编译/link 参数、depfile、日志、hash 证明。单纯 strip 的大副本因传输失败未执行，未被计为通过。

VM 的可写磁盘是本任务私有 overlay。已结束的私有 ISO/overlay/input 副本在校验每个归档 member 与输入 hash 后归档并回收；原始只读 VM base 不变，执行字节可从归档恢复。中断、失败和传输尝试保留为历史记录，不计入 48 项结果。

原 SSH 输入和证据位于 `/tmp/uwvm-wasip1-portable-1791152168630483000`：

- `admit.py complete-native REPO`：runtime → CLI → fixtures → 公开 CLI/broker/DAP。
- `admit.py cross-OS full REPO`：目标 SDK 和真实完整运行库的编译资格；`admit.py vm-OS full REPO`：Windows/FreeBSD 真实 native suite。
- `admit.py migration REPO`：三种外 OS metadata 在 Linux 的 native 恢复。
- `admit.py vm-OS migration REPO`：Windows/FreeBSD 三来源 × 两策略的 native 恢复。
- 本机 `mac_portable.py REPO`：macOS 完整 suite；`mac_portable.py REPO migration`：补充迁移恢复，均使用 2 GiB 监控。
- `collect_portable_evidence.py`：重新核对编译资格、日志、source hashes、8 个仓库/OS 完整验证和 48 个有向方向，不足时状态保持 incomplete。

## 源码冻结与并发修改

这些执行结果对应机器证据中的不可变源码清单，包含本轮修复和当时已同步的调试器变更。收尾时，两个本机仓库的本轮 14 个自有实现/测试/文档路径均与测试清单一致；随后只更新本报告和文档的验证结论。

同时对 Linux 实际编译依赖进行了本机工作树审计：uwvm2 的 1709 个仓库依赖中有 **34 个**、ros 的 1683 个依赖中有 **33 个**被其他并发工作继续修改，包括 runtime/native provenance、language/ASM debugger、console 和 WASIp1 group-restore 框架。这些改动被保留，完整差异与两侧 hash 在 JSON 的 `live_working_tree_dependency_audit` 中。本轮 48 项结果**不声称**执行过这些后续变更；不能把本报告当作任意当前脏工作树的全量资格证明。

## 明确的接口边界

便携导入恢复的是 WASIp1 **逻辑环境**。文件内容、真实 stdin 消耗/output、网络包/socket buffers、inode/time、clock/random/poll replay 都是外部状态，不进入 metadata。未跟踪外部资源必须映射现有、匹配类型/flags/cursor/权限的目标 guest FD；metadata 不能创建 native handle 或增加 host capability。

必须在同一个真实 cooperative stop 成对保存 Wasm 与 WASIp1，所有相关接口继续显示提醒。整实例 Wasm restore 和原子 Wasm+WASI restore dispatcher 不属于这份便携 WASIp1 修复的实现/验证结果；不能把 WASIp1-only 恢复说成全部内存、栈与运行世界已经恢复。ASM 调试仍限制在生成的 Wasm 上下文内。
