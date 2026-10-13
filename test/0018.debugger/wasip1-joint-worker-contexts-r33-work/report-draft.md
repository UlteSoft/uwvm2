# WASIp1 joint worker contexts — R33

两个仓库的四 OS 原生矩阵通过：**64 次运行、22,328 个计数断言**。
另有 **8 次 Linux 完整实例/预加载回归、24 次真实宿主回调拒绝回归、
2 次内存绑定单元运行/20 个断言**。

| 仓库 | 原生 OS | WASIp1 运行 | 计数断言 |
|---|---|---:|---:|
| uwvm2 | linux | 8 | 2796 |
| uwvm2 | windows | 8 | 2776 |
| uwvm2 | freebsd | 8 | 2796 |
| uwvm2 | macos | 8 | 2796 |
| uwvm2-ros | linux | 8 | 2796 |
| uwvm2-ros | windows | 8 | 2776 |
| uwvm2-ros | freebsd | 8 | 2796 |
| uwvm2-ros | macos | 8 | 2796 |

## 实现与修复

同一真实 `fast_io::native_thread` 私有准备工作线程同时持有 GC 根、调试
activation/shadow TLS，以及最终私有世界的 WASIp1 环境和新模块内存绑定。
所有帧的 owner/dense/cache/environment/memory 关系在 OS 线程启动前校验。
工作线程保持叶帧上下文，逐一嵌套检查调用者帧、显式空内存遮蔽和 LIFO 恢复；
协调器确认整个队列持有上下文后释放。线程先恢复 WASIp1 TLS，再退出根和调试
作用域并 join，之后才允许销毁候选世界。原生储存计入既有准备预算；额度不足
在任何 OS 工作线程启动前拒绝。未包含 WASIp1 的请求不会伪造成功计数。

新增跨模块 fixture 实际执行 main → provider，两模块拥有不同 WASIp1 环境
和内存；捕获真实两帧和 GC 引用。每轮主成功计数为一个共同工作线程、两次帧
上下文访问。整个矩阵主成功结果共验证 64 个共同工作线程、80 次帧上下文访问。
之前独立检查完整调度缓存的两工作线程仍保留，共 128 个/256 次模块访问。
这些是有限私有准备的诊断数据，不是执行恢复凭据。

新用例暴露了 observation 模式的桥接错误：typed shadow 已完整记录两帧，
但通用桥接包装把真实 defined-Wasm 导入误标为 foreign-host island，导致
`incomplete_logical_frames`。两个仓库的编译器/运行时同步加入 observation
桥接，通过实际保留的 source、publication、profile、cache 和 ABI 成员关系
识别真实 Wasm 叶目标，再交给原有调度器绑定其 WASIp1 上下文。真实宿主目标
继续进入 foreign scope；六种普通/尾调用形式的真实 native callback reentry
在两种栈策略下全部拒绝 thread capture，原程序仍正确返回且只发生一次调用。
捕获完整性检查和禁止 ASM 越出 guest 上下文的边界保留。
回调测试还同步修正了旧嵌入启动方式：显式保留不可变 Wasm 输入镜像，
并在 guest 进入前执行 CLI 延迟的 active element 初始化；没有放宽运行时拒绝规则。
本轮 24 次回归验证新 observation fixture；旧 `debug_checkpoint_host_effects_runtime.cc`
中的 resumable/pre-call 专项未在本轮重新编译运行，不能据此声明这些专项通过。

## 验证与资源

Linux、所有交叉编译及 Windows/FreeBSD QEMU 原生执行均在原 SSH Linux
64 GiB cgroup，swap=0。QEMU 使用只读原始基盘及本任务临时 overlay，真实退出
后验证基盘未变并清理。macOS 在本机 arm64 运行原始未剥离二进制，禁止 fork；
暂存二进制使用本任务新建且逐个退役的 1 GiB RAM 磁盘，整个容量计入 2 GiB
内存上限。最大记录上界 1423376384 字节。uwvm2 使用 2 GiB 物理磁盘余量；
本机余量随后下降，ROS 在创建 RAM 设备前按原条件拒绝，未启动原生测试。
ROS 的 RAM 暂存配置改为 512 MiB 物理磁盘余量，物理原生元数据仍小于
8 MiB，每项原生运行前重新检查。两个配置均完整计入 RAM 盘容量、控制器
和真实程序峰值，用户指定的 2 GiB 内存上限未改变；不沿用需要写入大型实体二进制的旧磁盘配置。
每次卸载前已有完整读回的可恢复编译产物。解码阶段在 RAM 磁盘退役后运行。

冻结源码 8783 文件，uwvm2 manifest `b22ea661b18400b72dd73a7c4a80f717a56449fd3575f95b8830641edd3dd15d`。
ROS 使用从该输入派生的单文件不可变补丁，manifest
`15fa0a34d3bf1c27e6744969a79e2e04f69846882ab1137a4114f2f6f789ecde`：保持 ROS 既有桥接接口，调用
`get_llvm_jit_generated_bridge_scope_depth()`；uwvm2 使用其既有
`get_llvm_jit_generated_wasm_bridge_entry_depth()`。原始 ROS 编译失败 guard
仍保留，所有 ROS 原生测试和恢复归档认证的是修正后的补丁输入。
同时发生的其他 agent 修改按路径 SHA 对照记录；此报告只认证该冻结集。
第一失败集、两次诊断及原始失败 guard 均保留；失败产物经完整读回后可恢复
归档，不会把归档成功说成测试通过。C++ I/O、解析及原生线程仍使用 FastIO。

Linux 本任务热区为独立 8 GiB 文件系统，测试准入 6 GiB、停止 7 GiB；
最终分配 6073122816 字节，卷剩余 2362675200
字节。8 组本轮编译产物以 7 个归档完整读回保存在 `/Users/liyinan/Documents/MacroModel/wasip1-r33-owned-cold-evidence`，共 328830082 字节，
本轮区域检查上限 314 MiB；所有本地该任务冷区合计 2143013758 字节/2 GiB。
使用 512 MiB 窗口上限的流式 Zstd。两个 macOS 仓库的原始产物按成员交错
归档并无损压缩，保留每个二进制、签名、对象及元数据的原始字节；完整读回两遍后才
退役对应原始副本。原区域 256 MiB 无法容纳 6 个现有归档及新增 macOS
产物，因此按实际压缩结果调整到 314 MiB；所有旧区域及合计 2 GiB
上限不变，原始编译/原生运行资格哈希仍完整保留。最终无损压缩独立使用 Linux
原 cgroup 内 4 GiB 的归档进程 RSS 上限；所有原生测试的限制及本机
512 MiB 解码窗口/2 GiB 内存上限不变。
R31/R32 原始源码归档已逐项完整读回，原样只读副本保留在已有本机工作目录；
仅释放 Linux 上相同的重复归档，没有新增本机归档副本或改变冷区上限。
最初等待器的 1,200 秒窗口先于长矩阵结束而超时，原生测试并未失败；
只把等待窗口对齐已有 guard 的 3,600 秒期限，资源限制未改。
Windows、FreeBSD 首次 guest 启动前分别因热区占用 6,635,630,592 和
6,563,225,600 字节超过 6 GiB 准入而拒绝，均未运行 VM。修正调度顺序：两个原始合格编译对象在链接完成后完整
归档/读回再退役，最终产物归档从认证缓存流式加入完全相同的原始对象字节，
随后才退役缓存。两仓库 guest 均在原 6 GiB 准入/7 GiB 停止限制下运行；
没有把清理阶段的阈值用于测试或放宽限制。实际拒绝和修复证据均保留。

## 仍未完成

**完整 Wasm 世界的原子恢复/发布和恢复后的 guest replay 尚未完成。**
还需要旧世界真实执行工作线程退役、join，新执行工作线程和活根安装，以及
source/store/engine/generation/WASIp1 cache 的共同发布与端到端执行验证。
`prepared_and_discarded` 不表示已恢复。本轮不改变 WASIp1 可移植格式，也不
声称新增 OS 迁移方向或回滚外部文件内容、网络、输出。保存 Wasm 检查点时，
仍明确提醒必须在同一个合作停止点同时保存 WASIp1 才能包含 FD/环境状态。
