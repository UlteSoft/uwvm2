# WASIp1 私有调用上下文与 trace 绑定验证（R29）

普通版与 ROS 版同步修复。本轮的实际结果及资格范围在所有原生执行和归档回执通过后填写。

## 实现

候选世界现在构造与真实运行时 `g_wasip1_runtime_module_context_cache` 相同类型、最终大小的 FastIO vector，保留每个新 dense module 的 import visibility、私有环境指针和新 memory[0]。环境指针仅借用同一候选世界的不可移动 owner；环境共享保持原拓扑，多个模块的 memory[0] 独立保留，环境的 `wasip1_memory` 在冷准备中仍为 null。缓存大小在分配前计入 native payload budget，全部准备成功后才返回 `wasip1_dispatch_prepared`、`prepared_wasip1_dispatch_contexts` 和 `prepared_wasip1_trace_bindings`。

默认 WASIp1 快速路径现同时检查真正的模块缓存。旧 initializer 的 override 配置为空不再足以选择原全局环境：缓存中的环境与全局默认环境不一致、或为 null 时，必须走模块选择路径。未构建缓存时保留原初始化 fallback；已有 override 配置仍拒绝默认快速路径。单独的 FastIO vector 回归覆盖 default/private/null/missing contexts 和原 memory binding 清理，10 项检查。

实际 trace 测试对真正 managed 文件的 native handle 建立 FastIO `io_dup` 外部 trace binding。即使 guest FD 表全部为 managed，strict 仍拒绝该外部 trace handle；binding-only 通过 FastIO 复制既有句柄进入候选，候选销毁后原句柄、配置、共享游标保持不变。新的 trace binding 数量只在完整成功后返回；失败、omitted WASIp1、quota 拒绝均不返回成功的 dispatch/trace 元数据。生产路径没有打开 trace path；外部 trace 内容及 I/O 副作用不进入回滚。

## 实际测试

| 仓库 | Linux | Windows | FreeBSD | macOS |
|---|---:|---:|---:|---:|
| uwvm2 | 1804 / 6 次通过 | 1784 / 6 次通过 | 1804 / 6 次通过 | 1804 / 6 次通过 |
| uwvm2-ros | 1804 / 6 次通过 | 1784 / 6 次通过 | 1804 / 6 次通过 | 1804 / 6 次通过 |

合计 48 次原生执行、14392 项累计断言，均为真实原生执行。另有 8 次 Linux 完整实例/preload 对象图回归、2 次默认上下文快速路径单元回归（20 项断言）。本机 macOS 保守聚合内存最大上界 349,110,272 bytes（332.94 MiB），低于 2 GiB。

原 checkpoint 和 builtin alias 各执行 instruction/unwind，POSIX 每次 210 项、Windows 每次 205 项；差别仍为五项 POSIX 专用断言。三实际模块、两环境的 fixture 每次 482 项，包含一组共享环境及三个新 memory 绑定和最终缓存检查。断言累计不代表独立功能数。Linux 两库另各执行 10 项 default-context 快速路径回归以及 complete-instance/preload 的 instruction/unwind，共 8 次对象图回归。

Windows、FreeBSD 使用真实 QEMU/KVM，guest 各 2 GiB RAM；Linux 编译、校验、测试、VM 和归档均在恢复后的同一 64 GiB/swap=0 cgroup 内执行。macOS 通过原始 arm64 Mach-O 在本机执行，测试与 controller 聚合内存保守上界限制 2 GiB，并验证 PID/birth/UID/PGID、no-fork profile、实际 wait4 峰值和回收。没有架构矩阵要求或跨 OS wire 迁移重新资格声明。

## 冻结源码与并发范围

冻结 8767 个文件，最终 manifest SHA256 `194ff28bd7ea121fa338698e9cff6ba427c52fb7522e2fca9ae066ff1433b9be`，source archive SHA256 `c994dd6c8d89318e9b2578353cb2981bb634ca325bdc2cc07533bbe2501db6db`。首次夹具错误把 char native file 直接传给 char8_t `io_dup` 构造器，导致测试器编译失败；生产 runtime/host API 和 10 项单元已通过。修复显式 `u8native_io_observer` 后仅更新两份 fixture，保留其余冻结输入，按依赖 SHA 复用未变对象，不将随后并发工作区改动混入该结果。初始源码切片、失败日志和守护回执保留。

本轮相关 16 条仓库路径中，交付时有 8 条仍与冻结切片相同。不同项记录在 workspace_scope.json 中，保留当前并发工作区内容，不用已测试旧文件覆盖它们。四 OS 测试对冻结切片负责；这一比较不会把最新完整工作区重新资格化。

## 空间与恢复证据

约 735 MiB 的六份 R26 本任务历史压缩产物逐项传到本机只读恢复区，整体 SHA 和全部 tar regular payload 通过后才在 cgroup 内退休 Linux 原件。退休前检查了当前 cgroup 打开的文件，未采用或信号任何其他进程；历史 source、SDK archives、日志和资格回执保留。本机历史冷区采用 1 GiB 检查限额，本轮验证该区时内存上界低于 2 GiB。Linux 原只读冷区 2 GiB 检查限额与活动 ext4 8 GiB 硬容量均未提高；新的压缩产物在冷区不足时留在活动卷，仍受 6/7 GiB 准入/停止阈值、512 MiB 卷余量和 32 GiB 主机余量约束。

收尾检查：活动目录按唯一 inode 分配计 5.555 GiB，活动卷剩余 2.314 GiB；Linux 历史冷区 1.930 GiB / 2 GiB。macOS 四份运输可执行重复文件共 920.10 MiB，在远端产品归档全量读取与最终资格通过后删除；源码、原生日志和可恢复归档保留。

收尾守护首次复制时仍限制为 report 入口，在启动存储检查前拒绝了 storage 命令。修正 storage/delivery 的精确入口后恢复收尾，初次 finish-progress 和原始守护文件保留；该拒绝未启动原生测试、未改变资格结果或资源限制。

## 尚未完成的实际联合恢复

本轮缓存为私有准备数据，不交换全局缓存、不发布 source/engines/generation、不启动恢复后的线程。`prepared_and_discarded` 不能称作 restored 或 replay。完整联合恢复仍需对接实际旧 worker retirement/join、新 worker startup enrollment、运行中的 root 安装，以及包含 Wasm/WASIp1/模块上下文的不可失败 commit，并验证真实恢复后的执行。共享环境的每次调用 memory 选择还必须与恢复线程的实际调用上下文衔接。

保存 Wasm checkpoint 时，仍必须在同一协作停止点同时 checkpoint WASIp1。Portable WASIp1 只包含可迁移状态/元数据，外部文件内容、trace I/O 副作用和内核句柄不会跨 OS 回滚，目标系统需要重新配置 mnt/rebindings。
