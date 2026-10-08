# R50 — Zig CU producer 识别与真实 Debug DAP 验证

> R51 后续纠正与补充：本报告保留 R50 的原失败记录。特定 ReleaseSafe 断点遗漏已定位为 run/pause 启动竞态；Debug 参数及稀疏对象读取已在新完整产品上补测。多位置断点与完整优化语义仍未资格化。见 [R51 报告](README.debug-zig-locations-r51-20261008.md)。

Zig 0.17.0 的本轮真实 LLVM Wasm 产物使用 `DW_LANG_C99`，`DW_AT_producer` 为 `zig 0.17.0`。原 VM 在真实断点帧中把 `!packet.enabled` 返回为 `int=0`。修复保留原 DWARF 语言号，在有界、所属 CU 的 producer 元数据中识别 Zig，并将标记从当前选中的物理或 inline scope 传给数值表达式解析器。类型名、文件后缀和其他帧不能选择其语言。跨 CU origin 链不能继承这个标记。新增 C++ 字符串复制使用 `fast_io::string` 和 `concat_fast_io`。

已同步到本机 uwvm2 / uwvm2-ros。五个相关头文件与完整构建快照相同；controller 的测试快照仅叠加本轮语言传递的单行改动，本机其他 agent 的终止事件改动保留。两个 VM 的三个 TU 均重新编译并链接，使用单独固定的历史 LLVM23 provider。当前整个 dirty workspace、最新 ROS 外部 LLVM provider、uwvm-int full、完整原生语言体验没有因此获得资格。

所有编译器、validator、VM、DAP、QEMU 和组件测试均在 Linux 原 64 GiB cgroup 中执行。保持原 boot/init birth、UID、CPU、PIDFD、内存/输出/磁盘准入与清理限制，所有保留 guard 的 OOM 增量为 0。共享 cgroup 的结果不用于性能比较。

测试结果：

- 官方 Zig 压缩包 SHA、19,505 个 SDK 文件校验；两个仓库各自新生成 Debug/ReleaseSafe × Wasm32/64 四组产物，validator 与 DWARF verify 通过，原产物字节未改写。
- 两个仓库分别通过 434 项 DAP 单测、51 项 producer DATA 检查、721,385 项 primitive 回归和其余组件回归；另各自通过冷 DWARF 索引与 address-class 索引回归。
- 700 个真实 Zig Debug DAP 会话，包括 uwvm2 连续 600.17 秒的 344 个会话、ROS 连续 601.35 秒的 348 个会话，以及初始 instruction/unwind 各两组会话。
- 52,500 项真实 Boolean 求值，覆盖 watch/hover/variables、比较、`and/or`、短路、类型拒绝、对象/数组、有限 `@as` 控制。每次真实单步后检查旧 frame/scope/子对象引用拒绝。全部原 guest OS wait、broker wait、adapter 实际 EOF 返回码为 0。
- 新完整 VM 上的 Rust、TinyGo、C/C++/Objective-C 家族回归共 38 个真实 DAP 会话通过。
- 16 架构 × 两仓库共 32 个实际 QEMU 目标，producer 检查均与固定原生 x86_64 输出一致：每目标 51 项，共 1,632 项。这只资格化该组件，不资格化完整跨架构 VM/JIT/DAP。

示例结果来自真实 source frame：

```text
!packet.enabled                         -> bool false
packet.enabled and !packet.disabled      -> bool true
false and (packet.seed / 0 > 0)          -> bool false
packet.grid[1][2]                        -> 15
@as(i64, packet.seed)                    -> 3
```

必须继续完成的具体问题：

- ReleaseSafe 原样本及保留内存对象、noinline 的样本均出现“断点已验证但运行到正常退出未命中”。当前 break-source 只选择一个函数/偏移，多位置源行断点仍未资格化；不能把 Debug 的通过结果推广到优化实例。
- 本轮 Zig Debug 的 formal arguments 在部分位置仍报告 unavailable；可读取 aggregate 不能代替参数读取资格。
- 优化的 DW_OP_piece、消除成员、内联/常量传播实例，以及完整 Zig 类型、强制转换、CTFE/重载等仍未完成。
- 所有 88 项原状态保留：14 implemented、39 partial、33 missing、1 separate_level、1 prohibited_by_scope。完整原生语言体验仍为 false。

默认新 DAP 脚本要求全部四个 profile。只有显式 `--debug-only` 才选择本轮已通过的 Debug 子集，结果继续记录 `all_required_profiles_passed=false`。Wasm64 使用实际产物所需的 memory64 与 table64 开关。

证据位于本目录 qualification.json 与 zig-producer-r50-evidence.tar.xz。归档保存失败记录、输入/输出 SHA、真实协议与原进程退出证据，不重复保存完整 SDK 和 VM 二进制。协议均在原 EOF/OS wait 完成后压缩，并逐成员复核 SHA。所测试的两个完整产品及原 SDK 仍保留在各自原目录。
