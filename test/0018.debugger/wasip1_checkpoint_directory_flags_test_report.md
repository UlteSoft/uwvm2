# WASIp1 目录标志与 portable checkpoint：R11

已同步修复 uwvm2、uwvm2-ros。C++ 文件打开、字符串和输出使用 FastIO，ROS 的模式范围未修改。

原始 path_open 对终止的 `.`、`..` 组件复用了父目录的原生打开描述，并忽略请求标志。旧版每仓库 336 个案例出现 240 个标志错误、168 个共享句柄、168 个父目录被子目录修改影响的案例。现在重新打开独立目录并保留真实 mount owner；Windows NT 经 FastIO 使用空相对名称，POSIX 使用 `.`。

portable restore 保留不同根目录资源的独立打开描述；同一保存资源的 FD 别名仍共享资源。新建根目录时，从一个 PATH_OPEN 父目录的 inheriting rights 授权子目录的 base/inheriting rights，不再错误要求父目录自身持有这些子权限或 SET_FLAGS。恢复已有根目录则检查持有权限；更强同步标志仍检查相应父目录权限，不能拼接别名权限。

| 实际原生 OS | uwvm2 | uwvm2-ros |
|---|---:|---:|
| Linux | 5,377 | 5,377 |
| macOS | 4,513 | 4,513 |
| FreeBSD 15.1 / QEMU KVM | 4,513 | 4,513 |
| Windows 11 / QEMU KVM | 1,393 | 1,393 |

四 OS 原始 wasm32/wasm64 WASIp1 ABI 合计 **31,592** 项检查，覆盖 owned/borrowed 父目录、POSIX 父目录 0/NONBLOCK、六种 dot/parent/普通目录路径，请求 0/NONBLOCK/DSYNC/SYNC/DSYNC+SYNC/RSYNC/SYNC+RSYNC，原始 fdstat 查询/设置及失败时无 FD 发布、输出槽与父目录不变。POSIX 每仓库 336 个案例，Windows 168 个案例。修复后的标志错误、共享句柄、父目录被修改计数均为 0。

Windows 目录 NONBLOCK 明确返回 ENOTSUP；不支持的同步标志也会拒绝。跨 OS 导入无法精确表达的目录标志会失败，不会伪装成功。

Linux 真实 Core3 LLVM JIT 合计 **5,280** 项检查；两个仓库均跑 instruction/unwind 实际暂停策略，覆盖 6 类 fresh-process 保存/恢复及多环境事务：

- 普通文件、稀疏保留 FD、根目录别名、规范化路径、DSYNC、独立根目录标志。
- 实际暂停后先关闭目标唯一持有 SET_FLAGS 的子目录，再依靠父目录的继承权限重建。
- 修改恢复后子目录 NONBLOCK，其他根目录与 preopen 保持不变。
- 完整 portable 图重新编码与保存元数据逐字节一致。
- 更强同步请求被拒绝后，FD 图、权限、标志、游标、分配状态、argv/env 不变。
- 多环境原子准备/提交与失败回滚；实际继续执行后 Core3 guest 返回 7。

最终 **36,872** 项检查。旧版负向复现另计：原始 ABI 9,602 项、真实 JIT 596 项，不混入最终成功数。WAT 经 wasm-tools parse、all-features validate；票据、类型化 GC 捕获及继续执行均来自真实 JIT。

Linux 编译、测试与两个 VM 在原来的 64 GiB、swap=0 cgroup 内；guard 认证 boot/anchor、PID birth/pidfd、实际归属和任务退场。只处置本任务自己的后代。共享峰值触发过一次本任务 ROS 编译保护停止，保留失败记录后重试通过。macOS 编译峰值上界 1,660,649,472 字节，编译及测试均低于 2 GiB。VM 基础磁盘未修改。

纳入测试冻结时其他 agent 的 ASM 更新，保留并发编辑。本轮 16 个源码/测试/文档输入与实际测试摘要一致；四 OS ABI 的 1,794 个当前源码依赖路径一致。交付时其他并发路径变化 14 个，未覆盖它们。原始测试源、失败夹具、编译参数、依赖、二进制摘要和 nonce 串口日志均已保存。

本机和 Linux 空间不足、Windows NT dot-reopen 初次失败、夹具换行/FastIO 字符指针错误、快照切换失败编译，以及一次汇总脚本按仓库过滤错误均未计入成功。仅在摘要核验或完整归档后清理本任务旧产物；未改变全局磁盘 reserve 或处理其他任务进程。

必须在同一 cooperative stop 配套保存 Wasm 与 WASIp1 checkpoint。本轮恢复 WASIp1 状态，不提供原子 whole-instance Wasm+WASI restore。跨 OS 仍需重配同名目标 mnt 与必要 rebind；文件字节、外部 I/O、时钟/随机状态不迁移。

完整证据在 SSH Linux 持久存储，因本机磁盘满未复制大型归档：

- 完整结果：`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-directory-flags-20261006-r11-private-vm/evidence/test-results-complete.json.gz`；gzip SHA256 `13c71edfe3da279da34b4fd2ed26930a1ae7fc0bceb1b935230b6abf4879b68d`。
- 逐 payload 核验：`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-directory-flags-20261006-r11-private-vm/evidence/archive-qualification.json.gz`。
- 源码/证据归档：`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-directory-flags-20261006-r11-private-vm/evidence/source-evidence.tar.gz`；44,894 个 payload、40,493,559 字节，SHA256 `60618383d1e64041e84f9f97745e7e64528a69e3231ddc14af106bc0f3dc0400`。

可用 `ssh linux 'gzip -dc <完整结果路径>'` 读取原始 JSON。源码归档排除 native objects/可执行文件与 VM 磁盘，保留真实构建参数和摘要用于复建；LLVM SDK 的独立环境归档及摘要记录在完整结果和归档核验中。重启后必须重新认证 boot/anchor/cgroup，再恢复测试环境。
