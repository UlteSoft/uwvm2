# WASIp1 checkpoint 跨 OS 实现与验证

2026-10-05。本次修复同步到 `uwvm2` 与 `../uwvm2-ros`。验证使用固定源码快照，包含冻结时已有的并发修改；之后的其他 agent 修改保留在本机，不冒充已测试的内容。源码、SDK、二进制、日志及进程退出证据见 [机器可读结果](wasip1_checkpoint_os_test_results.json)。

完整设计与接口：[wasip1_checkpoint.md](../../src/uwvm2/uwvm/debugger/wasip1_checkpoint.md)。早期 Linux 实现结果见 [2026-10-04 报告](wasip1_checkpoint_test_report.md)，其中 116 项是当时的 fixture 计数。

## 验证结果

以下计数均为**每个仓库**的断言或检查记录；checkpoint 分别运行 `instruction`、`unwind`，不是 WASI syscall 数或 Wasm 3.0 特性覆盖率。

| 原生 OS | 文件提供器 | checkpoint / 每种策略 | 公开 CLI | 其他原生检查 |
|---|---:|---:|---|---|
| Linux x86_64 | 143 | 129 | CLI/broker/DAP 170；portable CLI 68 | 颜色 27；实际线程退出；full LLVM/full interpreter 普通执行；ROS 删除模式拒绝 |
| Windows 11 x86_64，QEMU | 140 | 124 | 84，含 NT 文件继承回归 16 | 颜色 27；实际线程退出 |
| FreeBSD 15.1 x86_64，QEMU | 143 | 129 | 70，含真实 PTY 设置恢复 | 颜色 27；隔离 14；实际线程退出 |
| macOS，本机 arm64 | 143 | 129 | 完整 PTY CLI 138；portable CLI 70 | 颜色 27；隔离 12；每仓库实际线程退出重复 12 次 |

两仓库、两种策略分别执行，未以另一个 OS 的通过结果代替。WAT 先经真实 wasm-tools parse 与 all-features validate。完整 fixture 恢复后让真实 guest 调用 WASI `fd_write`、`fd_read`，验证二进制 A/NUL/B、文件长度、共享游标与 alias EOF；CLI 另外让真实 guest 读取恢复后的 argv/env 并核对调用的 entry/return/errno 跟踪。

## 可恢复内容与接口

保存、恢复 argv/env、guest FD 映射、rights、别名、预留空槽和关闭槽分配次序，以及受管匿名文件的内容、长度、游标、可恢复 flags。恢复先校验并准备全部候选状态，再提交；每个受管资源创建一个新 backing，其所有别名指向同一个资源。

在 LLVM full 的真实 cooperative stop 中：

```text
set wasip1 file 0 410042
info wasip1 fds 0
# 用构造回复的 affected=N，以及实际查询的 rights：
set wasip1 fd-dup 0 N 0x60006e 0
set wasip1 checkpoint 0 0
# 执行、修改或关闭 FD 后，在新的真实 cooperative stop：
set wasip1 restore 0 0 bindings
unset wasip1 checkpoint 0 0
unset wasip1 fd 0 N 0x60006e 0
```

`410042` 是 A、NUL、B，`-` 可构造空文件。N 是 guest FD；接口不接受 host FD、native handle 或 host 路径。可追加 `if-stop STOP_ID`，过期比较不会提交。SLOT 为 0..7。

`bindings` 恢复受管状态，保留原始 owned 外部资源绑定；`strict` 发现外部资源时先拒绝，argv/env、FD 表与受管内容不提交。普通 stdio/preopen 通常会导致 strict 拒绝。借用的 observer 不获得历史恢复权限。

**checkpoint Wasm 时必须同时 checkpoint WASIp1，并处于同一个真实 cooperative stop。** Wasm-only capture 会输出提醒并返回 `wasip1_checkpoint_required`。联合 native API `llvm_jit_checkpoint_capture_instance_with_wasip1_host_api` 在一个真实完整暂停、host closure、GC exclusion、publication 作用域内捕获两个域；失败不发布半份组合。CLI/DAP checkpoint 回复和 help 同样保留提醒。

单独恢复 WASIp1 不恢复 Wasm memories/globals/栈。这项改动没有增加原子联合 Wasm/WASI restore dispatcher。capsule 是同进程 live artifact；单独序列化 Wasm graph 不会包含可重建的 WASI 环境。

## OS 修复

- Linux 清除 FastIO `io_temp` 的初始 O_APPEND，保证定位写不变成追加写。
- macOS/FreeBSD 用 FastIO 固定临时目录、OS entropy、目录相对独占 0600 创建及立即 unlink；payload 和 guest FD 发布前文件已匿名。路径字符串、文件操作、输出与解析使用 FastIO。
- POSIX 只保存 WASI 可见的 flags，恢复保留新句柄的非 WASI bits，避免把 Darwin 的不可恢复内核 bookkeeping bit 当成可写 flags。错误使用目标 OS 的 `ENOTSUP`。
- Windows 从 FastIO Win32 `io_temp` 转移真实 owned handle 到原有 NT/WASI wrapper；同步 NT pread 会改变共享游标，捕获在成功及失败后均恢复原游标。原有 Windows WASI 不支持的 flags 仍返回 ENOTSUP。
- 修复 FastIO 的 NT throwing/nothrow 文件创建：`OBJECT_ATTRIBUTES.SecurityDescriptor` 原先错误接收 Win32 `SECURITY_ATTRIBUTES`，继承打开报 `STATUS_UNKNOWN_REVISION`。现在使用默认安全描述符，继承由 `OBJ_INHERIT` 控制；回归检查实际 GetHandleInformation、独立 reader 的继承标志与含 NUL 的内容。字段含义见 [Microsoft OBJECT_ATTRIBUTES](https://learn.microsoft.com/en-us/windows/win32/api/ntdef/ns-ntdef-_object_attributes) 与 [SECURITY_ATTRIBUTES](https://learn.microsoft.com/en-us/windows/win32/api/wtypesbase/ns-wtypesbase-security_attributes)。
- Windows full JIT 对后端产生的 `_Unwind_Resume` 做 engine 内的真实静态 unwinder 绑定；完整物化和 uwvm2 的既有 lazy 路径在 finalize 后检查 engine error，避免未完成的外部 relocation 被发布。ROS 没有新增 lazy/basic 模式。
- Windows 诊断字符串改用有界 scatter，保留输出字节并控制模板实例化规模；真实目标 LLVM SDK 的 ASM 工具链也使用正确 target，LLVMSupport 的成员实际验证为 COFF。
- 线程结束必须是原生线程实际退出。Linux、Windows、FreeBSD 使用原有真实 join/wait；macOS 用保留的 Mach thread right、kernel-authenticated dead-name notification 与原始 pthread_join。测试明确区分 body return 和仍阻塞的 TLS destructor。
- BSD 控制台保护识别真实 PTY slave、master/别名和 regular-file hard link，未知身份拒绝；输出在 truncate 之前检查。终端设置在正常退出时恢复。ASM 管理范围仍限定于生成的 Wasm 上下文。

## 边界与证据

外部路径内容、目录变化、已经输出的数据、stdin 消费、pipe/socket 缓冲、网络、时钟、随机数、内核 inode/time/stat 元数据不回滚；没有宣称整个 OS 或确定性 WASI replay。后续完整 VFS、虚拟 stdio、事件记录/重放需要专门 provider。

所有非 macOS 编译、测试和 QEMU 均在 SSH Linux 原指定 64 GiB/shared cgroup 中执行，独立记录本任务的 PID/birth/UID/cgroup/affinity/pidfd。没有接管、杀死其他 agent 进程或改变共享限额。Windows 完整编译另设 owned RSS 6 GiB；其余 Linux guard 为 16 GiB。

macOS 原生执行在本机，使用 2 GiB RSS 观察/停止限制及实际 wait4 峰值；这是 watchdog 和退出后的计量，不冒充 macOS 内核 cgroup hard cap。原生 checkpoint 和提供器还使用 no-fork sandbox；CLI 使用真实 PTY 与子进程内存统计。

本轮 macOS 为 26.6.2（25G83）。checkpoint 四次运行的最大进程加 supervisor 峰值上界为 259,129,344 bytes；完整 CLI 两仓库的进程树峰值分别为 302,759,936 / 313,114,624 bytes。24 次物理线程退出测试均记录 Mach names 11 → 11。

Windows VM nonce `e4c5c499be6a25a11aa1e09e03bb33c2`，FreeBSD VM nonce `43145dce7242f687b0a2739d32942a81`。两者 QEMU exit 0、原始 backing 的 identity/size/mtime/ctime 不变，全部本任务已绑定 pidfd 退出。仅验证这些条件后，清理本任务私有 overlay、输入 ISO 和复制的 vars；原始 VM 保留。

之前 Windows 编译/SDK/未解析 unwinder/NT 创建失败的记录保留，未计入本轮通过。旧快照 Windows 通过结果也单独保留；本报告使用最终固定快照的各 OS 结果。

最终源码快照：

- uwvm2: `sha256:75f4c140f897dac0cc610ae1abe11229399c0c20d5a0d3f5291f5f37106e791b`
- uwvm2-ros: `sha256:3307d55a2c638db004415f95217143c4898cfbb1afca1ab5ae8b0c983bce855e`

