# R56：调用者的 Source variables 标量值与类型（2026-10-08）

两仓库已同步修复：真正保存的 caller 帧中，frame-relative 的整数、布尔、float、double 原先在 Source variables 中 unavailable，而 Watch 能正确读取。现在 Source variables 复用真实选中帧的认证读取路径，并保留原始类型的 volatile 限定符；当前帧的同类类型丢失问题一并修复。

真实用例让 caller 与 callee 定义同名变量，且各自保持活跃：

| 变量 | caller | callee | Source variables / Watch |
| --- | --- | --- | --- |
| retained | 73 | -99 | volatile int |
| enabled | true | false | volatile _Bool 或 volatile bool（按实际 producer） |
| fraction | 1.25 | 9.5 | volatile float |
| wider | -2.5 | 7.25 | volatile double |

选中 saved caller 后，Source variables、Watch、hover、variables context 都读取其对应的真实物理帧。caller 的公开 line/column 仍为 0，不伪造源码位置或 native PC。所有标量均为只读叶节点，不发出 memoryReference。

## 实现与边界

第一轮 saved-frame 回调完成并释放运行时读取 guard 后，才进入现有 canonical selected-frame transaction。原来的 controller mutex 和 pause 仍被持有，每次读取都重新检查该 caller 的 incarnation、完整 stop/cohort 与 frame 选择。不会嵌套运行时读取租约，也不会借用 callee 的存储。

只精化已有三个允许精化的 unavailable 原因；原始缺失位置、失效范围、未知位及不支持类型不会被构造为值。canonical typed scalar 的 type_name 与位值一起复制，保留原始限定符。新增名字拼接使用 fast_io::concat_std，未引入新的宿主或 guest 文件 I/O。

## SSH linux 验证

- 两仓库分别重新编译完整 LLVM JIT 产品：runtime、host、main 三个 TU 和最终链接全部重新执行；具体源清单、编译器、链接器、依赖和 provider 哈希可核对。
- 四个 C-family front ends（C/C++/Objective-C/Objective-C++），Wasm32/64、DWARF4/5、O0/O1、instruction/unwind 两种栈策略；每仓库 64 个基础配置。
- 四个旧产品正向复现用例：Source variables unavailable，Watch 已知且正确；新产品四个短验证及 128 个矩阵会话全部通过。
- 共 1220 个新产品 caller 会话，58560 次选中帧标量 evaluate，4880 个当前/caller 帧快照检查。每个会话在真实 Wasm 单步前后都检查两个帧，立即排队的旧 caller frame、scope 和 Watch 均被拒绝，新帧重新得到正确值和类型。
- 每仓库 473 项 DAP 单测通过；另有 604 个真实 C-family primitive/float/composite、Rust、TinyGo、Zig 等回归会话通过，包含原有位置失效及 unmapped-source 控制。
- 两仓库连续测试分别为 614.127s (544 sessions) / 614.144s (544 sessions)，交替使用两种栈策略并覆盖 32 个原始 producer profiles。
- 共 54 条成功监督收据。实际测试均在原 64 GiB / swap 0 cgroup 的 E16–31 中执行；所有原始 guest OS wait、broker wait、DAP EOF 均成功，监督树按 birth/PIDFD 退役，OOM 计数未增长。
- 原始 Wasm / DWARF 没有重写，validator 与 DWARF verifier 结果保留。最初断点名断言和 O1 初始化前断点的失败用例单独保留，未计为最终通过。

## 资格范围与剩余工作

本轮是 R54 冻结完整源加五段 R56 自有修改；使用此前单独固定的 LLVM23 developer provider。同期其他 agent 修改的最新完整工作区、最新 ROS provider、uwvm-int full 不在此构建资格中。实现已按精确片段同步到两个本机工作区，不覆盖其他修改。

本轮没有新增 QEMU 执行；历史 16 架构 portable DATA 结果不能证明改动后的完整 VM/JIT/DAP 架构体验。standard Go、AssemblyScript 全 ABI、所有优化位置、native IDE UI、运行时 formatter、写变量/watchpoint/guest call 等仍有未完成项。88 项语言能力统计不升级，继续保留原有 implemented/partial/missing 状态。ASM 限定 guest Wasm 上下文的边界未扩大。

## 保留与占用

详细收据在 [debug_caller_scalars_qualification_20261008.json](debug_caller_scalars_qualification_20261008.json)。SSH canonical 证据：
/home/macromodel/Documents/uwvm3-implementation/retained-caller-scalars-r56-final/

压缩归档 14,983,320 bytes，SHA256 0188efa4ee9eb19719e54f3f35643d9b2c200180eb1c5766f26a652dd00fa964；12716 个归档成员均已逐项流式验证并 fsync。清理 204 个仅本轮自有、可重建的中间对象文件，共 708,665,824 bytes；完整最终 VM、原始 Wasm、源清单、依赖、命令和日志保留。

本轮 20 个自有工作目录合计约 0.999 GiB；清理后 SSH 主机可用空间约 33.23 GiB（当时观察值）。本机仅保留小型证据元数据。
