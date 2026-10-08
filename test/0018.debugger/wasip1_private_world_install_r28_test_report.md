# 候选 Wasm 世界的 WASIp1 私有安装与四系统验证（R28）

普通版 uwvm2 与 uwvm2-ros 同步实现；ROS 模式范围保持 full uwvm-int / full LLVM JIT。本报告对应全部资格回执通过后的冻结源码结果。

## 实现范围

联合准备在真正 coherent manager 的同一协作停止点，继续持有 execution lease、关闭的 host admission、独占 GC 和 publication guard。WASIp1 候选 FD 表、argv/env 所有者现在转入新世界内不可移动的私有环境，随后才准备实际 LLVM engine 和 frame/root。原环境地址不再作为候选 owner 保留。

每个实际新 dense module ID 对应自己的环境索引与新 memory[0]，多个模块共享同一环境时不会互相覆盖一个全局内存指针。安装后按真正 capsule 回读权限、资源身份、FD 别名、free-list 顺序、字符串 views、managed 文件字节、flags 和游标。文件操作使用 FastIO；Windows 同步 pread 后恢复新文件游标。

已消费的 mount / preopen 初始化配置不会重新执行，避免恢复时把原本关闭的 FD 再创建出来。strict 策略拒绝外部 FD、活动外部 trace file 和没有候选生命周期适配的自定义回调。默认运行时 proc_exit 回调可保留。binding-only 可通过 FastIO 复制已有 trace handle，保留既有外部绑定；外部内容、kernel offset 和 I/O 副作用仍不进入回滚。trace file 分支经源码核对，本轮没有单独声称实际 trace file 执行覆盖。

结果新增 `wasip1_installed_privately`、`prepared_wasip1_modules`、`prepared_wasip1_memories`、`prepared_wasip1_shared_modules`。只有资源、环境、engine、frame/root 整体准备成功后才返回成功安装指标；早期 quota、后期 frame quota、回调拒绝等失败不返回成功指标，RAII 释放候选，原世界可继续运行。

`prepared_and_discarded` 表示候选安装、核验并销毁，不能当成已恢复。保存 Wasm checkpoint 时必须在同一协作停止点同时保存 WASIp1 checkpoint。

## 实际执行结果

| 系统 | uwvm2 原生运行 / 累计断言 | uwvm2-ros 原生运行 / 累计断言 |
| --- | ---: | ---: |
| Linux | 6 / 1750 | 6 / 1750 |
| Windows | 6 / 1730 | 6 / 1730 |
| FreeBSD | 6 / 1750 | 6 / 1750 |
| macOS | 6 / 1750 | 6 / 1750 |

合计 **48 次通过资格核验的原生程序运行、13,960 项累计断言**；另有 **8 次 Linux 完整对象图回归**。交叉编译回执保留 `actual_native_execution:false`，原生运行由独立执行回执证明。
本机 macOS 全部原生用例的最大聚合内存保守上界为 345,522,176 bytes（329.52 MiB），低于 2 GiB。

原 checkpoint 和 builtin alias fixture 各执行 instruction / unwind，POSIX 每次 197 项，Windows 每次 192 项；差别来自 5 项 POSIX 专用断言。三模块 / 两环境 fixture 每次 481 项，验证共享拓扑、新 memory 绑定、原生 managed group 恢复及 portable metadata 的同 OS 回绑；真正 guest 最后读取 91。断言累计不是独立功能数。

Linux 另进行两库 complete-instance / complete-preload 的 instruction / unwind 共 8 次完整对象图回归，验证双线程 census、2 个 positive episode、2 frames / 12 root carriers，以及 GC 别名、稀疏 memory/table、passive segment/drop、异常和 preload import 关系。

macOS 使用原始未 strip 的资格 arm64 二进制。本轮不需要 RAM disk；监控每个原生测试与 controller 的聚合内存，保守上界包含 128 MiB controller 余量，并以 wait4 获取实际进程峰值。继承的 no-fork profile 有单独受监控的负测试；仅按 PID/birth/UID/PGID 认证自有进程并回收。Windows / FreeBSD 使用实际 QEMU/KVM、各 2 GiB guest RAM，在恢复后的 Linux 64 GiB / swap=0 cgroup 内执行，底盘只读且前后 stat 一致，不修改 KVM ACL。

## 源码与证据

冻结 8640 个文件，manifest SHA256 `4a46be13165404991f29fc8e32843e05fe88e84d13bf910336e8ef950ec9d371`，source archive SHA256 `01e359df5dc0c18dd83e1986c84a862abf8c7fe1ee01da45817e79cd3056fc04`。逐项输入、依赖、日志、产物与执行回执由最终 cgroup 内资格生产器验证。

本轮未修改 portable wire，也没有重新声称完成 12 个跨 OS 迁移方向；R26 的迁移验证是独立历史证据。portable WASIp1 仍只保存外部资源元数据，需要目标系统重新配置 mnt 或提供 rebindings，不包含外部文件内容。

收尾时检查本轮涉及的 18 个编译路径（两库各 9 个），10 个与冻结文件一致，8 个保留了后续并发改动，涉及 `uwvm_runtime.default.cpp`, `uwvm_runtime.h`, `uwvm_runtime_checkpoint_world_preparation_api.h`, `uwvm_runtime_checkpoint_world_resources.h`。这些改动包含私有 root scope 和 native retirement/reset 路径，属于后续集成；本轮四系统结果不能替代对后续整个工作区的资格验证。新 WASIp1 安装头文件、WASIp1 capsule 访问关系和两个测试夹具在两库保持一致。详细哈希见 [源码范围记录](wasip1_private_world_install_r28_workspace_scope.json)。未执行 C++ module 模式构建；该模式的默认 exit 依赖 import 只作源码同步，不声称构建通过。

## 测试器修复与占用

初次冻结包漏带已有 vendor 头文件，未进入新代码编译；补齐后修复了默认 exit 回调声明依赖和权限枚举比较。多模块测试器曾漏传 artifact root 而退出 64。补参后发现 preload 的 active data segment 尚未实际应用：夹具现通过真正 initializer 完成全部模块实例化再进行完整 census，没有放宽 census 条件。失败日志与原始失败产物归档保留。macOS 还保留了传输尚未完成时的 preflight 拒绝记录（该次没有执行原生测试），以及第一项已 PASS 但 RSS 监控器未签发资格的记录。监控器现仅对 kernel task 已消失、BSD identity 仍短暂存在的真正 ESRCH 状态执行有界重试，每次重新核对 PID/birth/UID/PGID，最终仍以 wait4 核对实际峰值；重跑六项后才签发通过回执。

测试继续使用同 UUID 的 8 GiB 专用活动卷，6 / 7 GiB 准入与停止阈值，512 MiB 卷余量和 32 GiB 主机余量。已验证历史产物以及本轮资格产物在整体 SHA 和全部 payload 回读后，在冷区剩余配额允许时移入只读冷区，原路径以 symlink 保持可读。达到冷区限额后的压缩尾部产物保留在活动卷，仍受活动卷限额约束。冷区使用 2 GiB 检查限额；活动卷是硬文件系统容量，冷区限额通过自有 guard 检查。仅退休已验证的本任务原始产物和传输重复件，未处理其他任务的文件或进程。

收尾实测：活动目录按唯一 dev/inode 计占用 5.46 GiB，活动卷可用 2.41 GiB，主机文件系统可用 60.73 GiB，卷空闲 inode 17,691；只读冷证据 1.93 GiB，低于 2 GiB 检查限额。本机已在远端归档与最终资格通过后核对哈希并退休 964,581,520 bytes 的原始 Mach-O 传输重复件，保留原生执行日志、输入和回执。详见 [收尾状态](wasip1_private_world_install_r28_post_reboot_state.json)。

## 尚未完成

私有环境安装已实现。完整联合 restore/replay 还需要将旧 worker retirement/join 与此事务衔接、安装 live GC roots、登记新 startup workers，并在不可失败的 commit 中将完整 Wasm/WASIp1 世界与新的模块环境绑定发布到实际 dispatch，随后验证真正恢复后的执行。本轮接口没有发布或启动新世界，不能将原世界继续运行称为完整恢复。
