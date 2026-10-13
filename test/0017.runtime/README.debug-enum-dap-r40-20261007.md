# 枚举 DAP 与 Objective-C 数组默认下界 — R40，2026-10-07

两个仓库已同步完成有限整数枚举的 DAP 显示修复。原 VM 已返回 value=3 (warm)，但旧 adapter 将含此枚举的整个对象退回完整协议文本，variablesReference=0。现在保留 3 (warm)、-1 (negative) 等只读叶子，包含它们的对象及旁边的数组继续提供复制树、named/indexed 分类及分页。未命名值 13 保留原数值。枚举名称不解码为表达式、地址、路径或命令；自指针只显示 guest bits，保持叶子。

实际 C/C++ Wasm32/64 × DWARF4/5 × instruction/unwind 短会话及持续会话通过。Source variables、Watch、Hover、Variables evaluation 使用同一真实源码停点与复制成员树。C++ 还实际覆盖一字节负枚举 -3 (negative) 和八字节无符号最大值 18446744073709551615 (maximum)。这些是 Clang 编译结果，经原 wasm-tools validator，Wasm/DWARF 字节未改写。

本轮另修复 source_dwarf_index.h 漏掉 Objective-C / Objective-C++ 数组默认下界的问题。[DWARF5 表 7.17](https://dwarfstd.org/doc/DWARF5.pdf) 将两者的省略下界定义为 0；源码现加入 0x0010 / 0x0011。两仓库新编译的真实 LLVM metadata component 均从全部 16 个原 C/C++/Objective-C/Objective-C++ 32/64 位、DWARF4/5 Wasm 中验证两维界限 2×3、零下界及最后元素选择器。

这项 C++ 修复尚未在新完整 VM 中验证。原 R35 完整 VM 的 Objective-C 数组会话失败记录仍保留：grid 显示 unavailable (array has an unavailable bound)，枚举叶子已正确显示。该失败没有变为 PASS。当前磁盘保留阈值未放宽，本轮没有再次构建完整 VM，因此不能宣称 Objective-C/Objective-C++ 最新 VM/DAP 数组、最新工作区或最新 ROS LLVM provider 已完成资格化。

## 使用

使用 -g 编译 Wasm，在 source_enum_probe 的 OBJECT_ENUM_DAP_STOP 行设置断点；获得本次 stackTrace 返回的 frameId 后：

    {"command":"evaluate","arguments":{"expression":"object","context":"watch","frameId":123}}
    {"command":"evaluate","arguments":{"expression":"object.shade","context":"hover","frameId":123}}
    {"command":"evaluate","arguments":{"expression":"object.negative_shade","context":"watch","frameId":123}}

123 只示意真实 frame 引用。object 可展开 value、shade、negative_shade、unnamed_shade、grid、next；C++ 另有 small、wide。对应枚举求值分别显示 3 (warm)、-1 (negative)、13，引用为 0。object / grid 的正引用用于 variables 请求；单步、继续、替换或真实 stop/code generation 变化后，旧引用不能使用。初始化时 supportsVariableType=true 才显示新增 type。

## 解析范围与限制

适配器只接受 canonical 十进制整数 + 完整、canonical 转义枚举名称。整数宽度为 1/2/4/8 bytes，检查有符号最小值及无符号最大值范围；名称按原 formatter 字节计数最多 4096 bytes，保留最多四倍 ASCII 转义宽度。非法宽度、非法转义、原始 control/non-ASCII、对象范围错误或真实状态变化不能发布前缀。未知/未命名的裸数值继续沿用原兼容规则，不凭 typedef 名称猜测浮点/整数 ABI。

名称截断、未知 scalar annotation、variant-part/active-variant/omission 等尚未支持格式保留整段不透明 packet。Rust 普通枚举、payload/niche variants 没有本轮真实 producer/DAP 资格。现有 64 KiB packet、1024 nodes、depth 32、每停点 8192 nodes / 512 KiB、真实 status 前后检查均保留。未新增 setter、evaluateName 或 memoryReference。

所有新增 C++ I/O 使用 fast_io：native_file_loader 读文件，print/println/perrln 输出，parse_by_scan 检查地址宽度参数。ASM 调试范围继续限制为 Wasm 产生的上下文；没有扩大到 VM 或 host helper。

## 实际验证

全部编译、validator、功能 Python 回归、VM 和 guest 执行只在 SSH Linux 原 64 GiB / swap=0 cgroup 内运行。复用原 keeper、boot、birth/PIDFD、CPU 集和 OOM/磁盘/输出保护。构建仅限小型 producer 与 LLVM component；未复制新的完整 VM。

| 仓库 | DAP 单元 | 双策略短会话 | 持续会话 | 持续秒数 | 持续枚举求值 | 持续树行核对 |
|---|---:|---:|---:|---:|---:|---:|
| uwvm2 | 127 | 16 | 106 | 600.279 | 1266 | 6352 |
| uwvm2-ros | 127 | 16 | 109 | 603.046 | 1299 | 6528 |

最终枚举共 247 个真实会话、2949 次显式枚举求值、988 次对象树呈现、14800 行复制树核对。每次 guest 对相同 probe 有限执行 4096 次独立自检，结果 41，覆盖枚举、数组和自指针；有限重复使异步 pause 后仍有后续真实断点可命中。重复会话不算独立功能。

两仓库各一个原 TinyGo 数组/字符串实际回归会话另通过（90 次 compiler builtin 比较及原 guest 自检 42306）。每个最终枚举会话均要求 guest 自然 exit 0、原 Popen OS wait、adapter/broker 退出、真实 DAP EOF；排队真实单步后 stop-id 增加，旧 frame、Source scope 和对象引用拒绝。单元 127 项包括本轮 9 项新的 detached packet DATA 控制，不能替代实际 Wasm 证据。

失败均保留：本轮 14 个 guard 失败，以及 2 个未启动子任务的 pin 闭包拒绝。包含不支持 Wasm target 的 compiler、依赖记录换行、测试旧预期、过快 guest pause、原 VM Objective-C 数组缺陷、component include/RTTI 配置和 ROS raw 日志删除/统计竞争。最终成功结果独立记录，不覆盖旧失败。后者仅在完整归档内容、EOF、原 guest wait 和已完成状态再次验证后允许统计已退休的 own raw，不放宽任何预算，其他文件消失仍失败。

每个成功枚举会话的原 stdio packet 先生成 protocol.tar.xz，逐成员 hash/size 验证并 fsync 后才退休 own 原日志；解压可恢复所有请求、响应和事件。主证据保留全部早期失败/成功 cuts、最终源文件、producer objects/Wasm、native component 产品、guard/PIDFD/OOM、nested raw archive 及资格记录。本轮主 archive 2540024 bytes、3549 个成员，全部逐成员流式复核并 fsync。原 R35 全 VM 和 R34 TinyGo producer 以既有完整 archive 引用；不重复打包完整 VM。

## 未完成

Objective-C/Objective-C++ 修复后的新完整 VM 与 DAP 会话、Rust enum/payload/niche variant、其他语言 pretty-printer/动态或优化布局、Go runtime/goroutine、Zig/AssemblyScript 原生语言体验、实际 IDE/general fast attach、完整 QEMU VM/JIT/DAP 和最新 ROS provider 仍待继续。aggregate 清单仍是 partial；本轮不宣称全部语言 native 等价。

机器记录：[debug_enum_dap_qualification_20261007.json](debug_enum_dap_qualification_20261007.json)。

主 archive 验证后，仅退休 33 个仍重复展开的 own stdio raw 文件，1100396 logical bytes；它们均按主 archive 成员再次核对 hash/size。源文件、VM/component 产品、Wasm、所有 receipts、broker wait 和 nested protocol archive 全保留。当前实际 free bytes 为 9299017728，仍高于原 9288400896 保留阈值。


R41 后续：Objective-C / Objective-C++ 默认数组下界修复已在 R35 frozen full source + 修复的新完整 VM 上通过实际 Wasm32/64、DWARF4/5 DAP 和长测。当前全部工作区及最新 ROS provider 仍待资格化。参见 README.debug-objc-full-dap-r41-20261007.md 和 debug_objc_full_dap_qualification_20261007.json。
