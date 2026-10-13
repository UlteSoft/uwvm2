# WASIp1 跨平台 full CLI / JIT checkpoint 回归（R26，本轮完成）

日期：2026-10-07。两个仓库同步修复，ROS 仅保留 full uwvm-int 和 full LLVM JIT。全部 Linux 编译、执行和 Windows/FreeBSD QEMU 测试进入同一个 64 GiB、swap=0 cgroup；macOS 原生执行按本任务父子进程聚合上界限制为 2 GiB。

## 修复

Windows COFF 宏与 LLVM 枚举冲突：对象图使用 COFF 隔离头内的常量 facade，补齐四个绝对重定位常量，同时恢复调用方宏。实际 Windows 头和 POSIX 冲突宏回归各检查 12 项。Windows LLVM SDK 的四个 BLAKE3 汇编 object 原先混入 ELF，专用构建工具修正 ASM target；64 个实际 ros.11 archive 的 1673 个 object 已验证为 COFF x86_64。

Windows `-Rdbg` 首次状态查询的真实退出码为 `0xC00000FD`。PE unwind 表确认 console 与 controller 的叠加栈帧超过默认 1 MiB 栈。将有界行编辑器改为 RAII heap 持有后，console 栈帧从 885,992 降到 217,560 字节，编辑容量不变；两个完整产品实际 CLI 各 98 项通过。

macOS 实际 JIT 对象中的 `ltmp0` 段锚点被 LLVM 分类为函数，引发额外别名拒绝，导致完整 `-Rdbg` SIGTRAP。现在仅接受原始 local N_SECT、descriptor=0、合法 linker-private 名称、文本段 offset=0 且对应唯一已证明函数起点的段锚点，同段第二个锚点仍拒绝。记录保留为 DATA，不生成执行或读取宿主代码权限。原生真实对象回归通过 21 项，包括真实额外函数别名和五种字节变异拒绝；两个完整 macOS 产品已真实通过 dbg、checkpoint 和恢复。

macOS/FreeBSD PTY 工具新增 poll 与 waitid(WNOWAIT) 观察自己的子进程，保持 FastIO process 的回收责任。macOS 真实非法 Wasm 子进程以 127 退出时，工具约 2.3 秒报告失败，父进程按预期退出 1，prompt deadline 未耗尽；健康完整 CLI 两库均通过。

只读 CDFS 文件曾因 FastIO 默认 read 请求 FILE_WRITE_ATTRIBUTES 返回 AccessDenied。两库新增显式 `no_write_attributes`，Wasm 映射加载和 portable 单体、组导入使用该标记。默认 read 的时间戳写权限保留。Linux/macOS 新同源 helper 各通过 5 项，两个 Windows 完整 CDFS 消费者现已真实通过：普通版 858、ROS 861，共 1719 项。每库包含 12 项真实 NT 属性/时间戳、只读映射、单体/组导入断言和两种 NT 路径 smoke，完整 CLI 各 98 项。输入不复制到可写磁盘。

新增 C++ 文件、诊断及路径转换使用 FastIO。原有指针宽度、重定位、对象归属与越界拒绝条件保留；asm dbg 仍不得访问 VM 宿主上下文。

## 已完成的完整产品执行

| OS | 普通版 | ROS | 实际公开 CLI |
|---|---:|---:|---:|
| Linux | 1056 | 1056 | 每库 CLI/DAP 314 |
| Windows，只读修复后 final cut | 858 | 861 | 每库 98 |
| FreeBSD，原冻结 cut | 1006 | 1009 | 每库 84 |
| macOS，修复后 final cut | 1177 | 1180 | 每库 84 |

计数是断言、仓库、策略的累计观察，不是独立功能数，也不将不同 source cut 合并为一次通过。Linux 和 FreeBSD 的 native checkpoint 每种策略 154 项，Windows 为 149；差异是五项 POSIX-only append/readback 断言。Portable 本机保存/恢复各 51/83 项。macOS 每库额外完成三个来源 OS、两种策略各 83 项恢复和只读导入 5 项；聚合上界均低于 2 GiB。

完整 CLI 涵盖断点停止、argv/env 修改、managed 二进制文件创建、FD dup/close/复用、别名和 checkpoint 恢复、strict 外部资源拒绝且不提交、portable 发布/导入，以及实际 Wasm 继续执行、syscall entry/return/errno trace。ROS 删除的三种模式均按预期以 126 拒绝。QEMU 的成功回执验证退出 0 和原底盘 stat 不变。

## 跨 OS 恢复

Linux、Windows、FreeBSD 的六个有向路径已经真实完成。三个 OS→macOS 均通过，macOS→Linux 和 macOS→FreeBSD 各 332 项通过；macOS→Windows 的 332 项也已通过，使用 final cut 的新 Windows 目标并直接从只读 CDFS 导入；四 OS 的 12 个有向路径全部完成。每个方向检查两个仓库、instruction/unwind 两种策略各 83 项，来源执行回执、元数据、目标 fixture Wasm SHA 全部校验。旧目标静态产品用于兼容性恢复的范围明确记录，不能冒充新源码完整产品重建。

修复后 final source manifest SHA256：`0e00f189ecefd584e8979d59698fe86f312459004a1e150c28479c6b07ecb9ad`。Mac 原生回执、完整编译归档、实际对象拒绝回归和来源资格记录都保留；详细机器结果见同目录 JSON。

## 状态与资源边界

**保存 Wasm checkpoint 时必须在同一协作停止点同时保存 WASIp1 checkpoint。** 实际 CLI 已验证此提醒。WASIp1-only restore 不恢复 Wasm 内存或执行位置；整台 Wasm/WASIp1 联合原子 restore/replay 尚未提供。Portable 不携带外部文件内容，目标必须重新配置 mnt 或提供资源 rebindings。这个能力边界不能因测试通过而消失。

本轮所列未完成验证已全部完成。FreeBSD final cut 的新 helper 在两库各通过 5 项只读导入、12 项 COFF、1 项提前退出和 84 项健康完整 CLI，共 204 项补充观察；它们驱动的是已资格验证的旧完整 FreeBSD VM，不冒充新 core 重建。每库另外重复通过两种策略共 166 项 macOS 来源恢复。Linux 同源 helper 两库合计 34 项真实通过。所有失败回执保留其原范围。额外 FreeBSD 退出检测第一次因测试器多传参数而退出 2，已纠正，不作为运行时缺陷或通过。

目录采用 6/7 GiB 准入/停止线、独立 8 GiB 卷、512 MiB 卷余量和 inode 余量。普通版 Darwin 编译曾被自有 8 GiB RSS 守卫终止；该唯一 Linux 编译路由改为 12 GiB 自有预算和 48 GiB 共享准入，仍受 64 GiB cgroup 与 60 GiB 共享停止阈值约束。一次归档 housekeeping 曾短暂越过目录 7 GiB 采样停止线，独立 8 GiB 容量和卷余量没有耗尽；随后清理已验证产物并加入压缩写入预算，不能声称该采样线从未越过。

SDK 库、编译中间件和完整产品在逐项归档回读后才退休。结束运行的自有失败 VM 私有镜像允许删除，其失败诊断、源快照和原底盘保留；被删除的私有镜像不再声称可恢复。历史 Mach-O 宏失败产品归档迁到本机冷存储，回读所有 payload 并记录新位置；公共 SDK 头和其他任务进程保留。没有终止或删除其他 agent 的资源。

收尾工作区审阅发现 100 个路径相对冻结 cut 已有并发更新；这些 peer 修改全部保留。本任务 24 个源码路径仍与 final manifest 匹配。完整运行结果对应冻结 cut，不代表未经重建的并发最新工作区。

最终收尾验证已在原 cgroup 内通过，复核两个 final Windows native qualification、两个 macOS 完整原生结果及所有 2 GiB 上界、macOS→Linux/Windows、FreeBSD 新 helper、Linux 新 helper、24 个本任务路径与全部 9205 个冻结文件。主要结果对应各表记录的 source cut；历史 Windows 1691 项仍保留在 JSON 的原回执中，本轮修复后 1719 项另有回执，未混算为独立功能。

本轮源代码修复已同步两个仓库，测试报告与机器结果相同。代码审阅 patch 限定为本任务 24 个路径的精确修改，保留并发 peer 修改。结束运行的临时 guest overlay/ISO/VAR 和归档后的原始重复产物已清理；恢复时必须按 SDK/library/PCH retirement 回执还原所需文件，不假定它们仍有原始副本。最终证据已以独立元数据 bundle 保存并逐项回读，完整源码、SDK 和产品归档由索引引用。

证据基础 bundle 已回读 2642 个文件，57,435,718 字节，SHA256 cdf7b18489e2020e7908a941c2b2521742857d9568dcb9c1bd3ea96280734ddc。最终报告的历史进度字段整理作为单独 delivery amendment 保存；它不改变源码、测试结果或基础 bundle。
