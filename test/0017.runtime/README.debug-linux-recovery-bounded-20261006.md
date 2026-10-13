# Linux 重启恢复与测试目录限额（2026-10-06）

复用已恢复的共享 cgroup：64 GiB memory.max、swap 0、CPU 集合
0,2,4,6,16-31；测试 worker 使用 16-31。恢复后所有实际组件测试
都由出生身份/PIDFD 监督器送入这个 cgroup，没有新建第二个 64 GiB 测试池。

本任务的源码、构建产物和完整证据放在
/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006。
目录挂载于 root 持有的 16 GiB 稀疏 ext4 镜像，镜像大小限制文件系统容量；
源代码和 SDK 不再放在 /run/user/1000 或 /tmp 的 tmpfs。
已核对 loop 的 backing_file、挂载选项、文件系统容量和 identity。
当前镜像实际已分配 285,536,256 字节
（约 272.3 MiB），并非预占 16 GiB RAM。

| 限制 | 本轮执行方式 |
| --- | --- |
| 本任务目录硬上限 | 16 GiB ext4 镜像，nodev/nosuid |
| 主磁盘空闲保留 | 每次监督采样至少 25 GiB |
| 实际写入文件系统保留 | 至少 1 GiB、4096 个空闲 inode |
| 小型 DATA 任务内存 | 自有进程树合计 RSS 不超过 1 GiB |
| 同一源码切片所有测试输出 | 合计不超过 64 MiB；单文件不超过 8 MiB |
| 内存入场条件 | 保留原 6 GiB DATA 入场条件；距共享上限 1 GiB 时停止本任务 |

完整 VM/JIT/SDK/producer 的原 9 GiB 入场条件没有改动；上述 DATA
监督器只接纳固定的组件 runner，不能用于这些完整构建或安装任务。
保留既有历史证据，不清理其他任务的文件或进程。

恢复脚本在 uwvm2 和 uwvm2-ros 中同步保存：
tools/debug/restore_linux_test_environment.py。
Linux 持久入口位于镜像挂载目录外，重启后仍可访问：

```sh
ssh linux 'python3 /home/macromodel/Documents/uwvm3-implementation/restore_language_test_environment_20261006.py --check-only'
```

去掉 --check-only 可启动所选共享 keeper 并重新挂载已有镜像。
脚本校验 64 GiB/swap/CPU 配置；已存在的内存和 CPU 限制不被改写。
它拒绝遮盖非空目录或错误的镜像/挂载，不删除数据，也不运行测试。
本轮检查和重复恢复均通过。重启后的实际测试监督器仍须重新绑定
新的启动 ID、keeper PID/birth 和写入文件系统 identity，不能复用旧 pins。

首次测试入组被拒绝：cgroup.procs 的所有者与 root 的停机入组 helper
不匹配。自有 bootstrap 已通过 PIDFD 回收并 reaped(-9)，未执行组件；
该失败和 cleanup_discovery_error 如实保留。
随后恢复原 root 控制文件所有权，helper 明确指定 root 和不可变镜像 ID，
使用新的 control 目录重试成功。

新启动下两仓库原生回归均通过，各 24 阶段（11 次编译、10 次当前组件执行、
3 个旧 header 预期失败），各 1,295,317 条 Boolean cast 检查，
247,159 条 category 检查、DAP 202 正例/60 负例及各 11,802 条字符属性检查。
scalar、conditional、Zig coercion/type/preflight/negative 和桥接回归通过。
成功 guard 总计 287.125 秒；
自有 RSS 峰值 690,069,504 字节，
输出总量峰值 21,088,725 字节。
guard 前后 OOM/max 事件均保持零；校验 7,396 个输入哈希。

完整新证据归档 18,435,472 字节，6,538 个成员，
SHA256 63180f195dfd80cd6cd08504b06072b234117468c78df55acb9a626d04fc5dd3；
Linux 持久磁盘和本机各一份，两个副本均逐成员校验。
归档包含冻结源码、实际 ELF/日志/回执、恢复脚本和失败入场记录。
Linux：/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/bool-cast-reboot-a1/boolean-cast-reboot-evidence-a1.tar.xz
本机：/Users/liyinan/.codex/artifacts/uwvm2-bool-cast-reboot-evidence-20261006-a1/boolean-cast-reboot-evidence-a1.tar.xz

跨架构 SDK 被清理，当前未重新安装，本启动未执行 QEMU 矩阵。
此前 /run/user/1000 下完整原始证据已丢失，无法追回旧 ppc64 失败原因；
旧的 8/32 观察结果只保留为历史阶段记录。
本轮资格仅覆盖有限复制数值/声明类型/DAP 组件和环境恢复，
未重新验证完整当前 VM/JIT、真实 stopped frame、语言 producer、
modules build 或完整各语言原生调试体验。
