# R51 — Zig 参数、稀疏 DWARF 对象与连接竞态

本轮修复同步到 uwvm2 / uwvm2-ros：Source variables 在通过 canonical stopped reader 细化参数时保留实际停止的线程 cohort；非 scalar 的裸 DW_OP_piece 根对象进入现有复制对象读取路径；DAP 的直接整数和 Boolean 求值显示真实数值与 producer 类型。运行时仍重新验证停点、frame、epoch、capture 和完整 cohort，显示元数据不能提供地址、指针跟随或写入权限。C++ 改动没有新增 I/O，既有 fast_io 路径保留。

另外修复 Unix broker 的 accept-to-SO_PASSCRED 竞态：令牌可能在 accept 返回后、accepted socket 开启凭据接收前到达。监听 socket 在 bind/listen 前开启 SO_PASSCRED，使 accepted socket 预先继承。UID/GID、每包 SCM_CREDENTIALS、PIDFD、能力令牌及 guest PID 排除检查均保留。确定性测试强制该窗口，在两仓库旧 server 上失败，在新 server 上通过；早期仅在 accept 前排队的测试不能复现，原记录也保留。

R50 特定 ReleaseSafe 断点失败的原因现已查明：旧驱动以普通 run 启动，再请求 pause，设置断点时目标源码行可能已经执行。本轮使用 -Rdbg，第一条经过认证的 status 必须返回 prepared; no Wasm instruction executed，然后设置源码断点。原 ReleaseSafe Wasm 字节不改写即可停在真实的行 14、function 1、offset 24。这不能证明多位置源码断点已经实现。

测试矩阵及边界：

| 真实编译配置 | 本轮已验证 | 仍保留的不可用情况 |
| --- | --- | --- |
| Zig Debug × Wasm32/64 | 源码断点；seed/enabled/disabled 参数；三种 evaluate context；75 项有限 Boolean 求值；对象及二维数组；旧引用退休 | 完整语言类型、comptime/CTFE、重载等 |
| Zig ReleaseSafe × Wasm32/64 | 执行前断点；真实 Wasm 单步；稀疏 DW_OP_piece 根对象及子对象；seed=3 和数组末项 15；旧引用退休 | 当前 PC 尚未激活的 location；Boolean 及数组空 pieces；无源码映射的真实 Wasm 指令 |

真实 Debug 示例：

```text
Source variables: seed i32=3, enabled bool=true, disabled bool=false
watch/hover/variables seed              -> 3     (i32)
watch/hover/variables enabled           -> true  (bool)
!packet.enabled                        -> false (bool)
packet.grid[1][2]                       -> 15
```

ReleaseSafe 的 packet 在首个断点处正确显示 no location active。随后通过真实 Wasm 单步到 location 生效的位置，根对象可展开；编译器给出的 seed 和常量数组 piece 可读，缺失的位显示 unavailable。中间一个无源码映射的 Wasm 指令保持 Wasm frame，不伪造语言 frame。优化参数和完整 Boolean 求值不能继承 Debug 的资格。

测试结果：

- 两个仓库各通过 440 项 DAP 单测，包括真实 Linux 的排队凭据窗口测试。旧 server 两次确定性失败记录保留。
- 新完整 VM 上共 972 个 Zig 会话通过，其中 uwvm2 持续 601.08 秒 / 476 个会话，ROS 持续 601.04 秒 / 480 个会话。两个调用栈策略和四个真实编译 profile 均覆盖。
- 36450 项 Debug Boolean 求值、4374 项 Debug 参数显示，以及 14580 项数值控制/优化 sparse piece 求值通过。
- C、C++、Objective-C、Rust、TinyGo 回归共 38 个真实会话再次通过。
- 所有合格会话的原 guest OS wait、broker wait 和 DAP 实际 EOF 返回码均为 0；全部保留 guard 的 OOM 事件增量为 0。失败或清理为非零的原记录分别保留，不计入上述通过数。
- 新证据归档 10895756 字节，SHA256: a5d72f7ab31993cbae8100c42c365bc90a201e2ab9cf6cbdd59ed39ba00db3f6；完整 SDK 和 VM 二进制不重复打包。

两个完整产品的三个 TU 均在 Linux 原 64 GiB/swap 0 cgroup 中重新编译并链接；实际依赖及输入 SHA 在构建前后相同。测试使用 R50 冻结源码加本轮两处 controller 改动，以及单独固定的历史 LLVM23 provider。manifest source_id 与继承的内嵌 build-source 标签分别记录，产品身份由 binary SHA 固定。当前整个 dirty workspace、其他 agent 的终止事件/原生 continuation 修改、最新 ROS 外部 LLVM provider、uwvm-int full 和完整跨架构 VM/JIT/DAP 均不由本记录资格化。ROS mode 配置未改动。

88 项原统计状态保留：14 implemented、39 partial、33 missing、1 separate_level、1 prohibited_by_scope。四个 launch profile 的有限测试通过和完整优化语言语义的完成是两个分别记录的条件。all_required_Zig_full_predicates_passed、optimized_full_predicates_qualified 与完整 native parity 仍为 false。

证据见 debug_zig_locations_qualification_20261008.json。原失败与成功的 guard、构建实际依赖、固定输入、源代码切片、原 guest OS wait、broker wait 和 DAP 实际 EOF 证据均保留。压缩协议逐成员复核 SHA 并 fsync；归档留在 Linux，SDK 和完整 VM 二进制不重复打包。

本轮仅在 SHA 清单、归档逐成员核验和 fsync 完成后移除 6 个自有中间 .o 文件，释放 354038456 字节；完整 VM 产品、源码快照、SDK 和其他 agent 文件保留。

Linux 归档：/home/macromodel/Documents/uwvm3-implementation/retained-zig-locations-r51-final/zig-locations-r51-evidence.tar.xz。本机 controller 与 DAP adapter 保留其他 agent 的已有改动，其文件整体与冻结测试切片不同；本轮 source_evaluation_display 函数逐字与已测切片一致。
