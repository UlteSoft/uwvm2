# R57：捕获数值的限定符与 typedef 类型（2026-10-08）

两仓库同步修复：编译器原始 DWARF 带有 const 的 direct copied numeric local，原先在 Source variables、Watch、hover、variables context 中只展示基础名，丢失外层限定符。现在这四个入口采用同一个有界类型拼写，保留真实限定符和命名 typedef；数值、捕获可用性和 guest 上下文权限保持原有约束。

## 真实例子

原始 C-family producer 的 source_primitive_probe 参数：

| 变量 | 值 | 修复前类型 | 修复后类型 |
| --- | --- | --- | --- |
| retained | 73 | int | const int |
| enabled | true | _Bool / bool | const _Bool / const bool |
| fraction | 1.25 | float | const float |
| wider | -2.5 | double | const double |
| tagged | 123 | SourceTicket | const SourceTicket |
| barrier | 17 | const volatile int | const volatile int |

barrier 原先通过 canonical frame-relative object reader 已正确，是独立保留对照。其余五项在旧产品四个实际 O1 / DWARF4/5 / 两仓库会话中正向复现，单步前后四个入口均丢失 const；这些原始证据保留。

## 实现与约束

source_dwarf_values 的数值查询复用既有 source_type_declarators，先用真实 DW_AT_language 判断 C-family，并让 TinyGo/Zig producer 标记优先。source_dwarf_objects 改为共享同一判断。Rust、未知 CU language、TinyGo、Zig 不通过类型名推断 C 语法。

生成的限定符前缀与名字共用原有结果字符串预算；越界或 malformed 元数据返回失败，不发布部分变量列表。使用 fast_io::string 与 fast_io::concat_std，未新增文件 I/O。缺失 native capture 或无原始 DWARF location 的变量仍 unavailable，不制造值或读取权限。模块分区 import 已同步，未实测 module BMI 编译。

## SSH linux 验证

- 两仓库完整 LLVM JIT 构建：runtime、host、main 三个 TU 及最终链接全部重新执行，源/依赖/provider 哈希前后闭合。
- 四个 frontends：C、C++、Objective-C、Objective-C++；Wasm32/64 × DWARF4/5 × O0/O1，每仓库 32 原始 producer profiles；instruction/unwind 两种栈策略共 128 基础矩阵会话。
- 共 1284 个新产品限定类型会话、46224 次 evaluate、2568 个当前帧快照；所有六项变量在真实 Wasm 单步前后核对值和类型，立即排队的旧 frame/scope/evaluate 均被拒绝，fresh frame 重新认证读取。
- 两仓库各 473 项 DAP 单测、8 个原生 C++ 组件程序通过；732 个 saved caller、primitive、float、true/false composite、C/Rust/TinyGo/Zig 等真实回归会话通过。
- 连续覆盖 32 profiles 并交替两种栈策略：uwvm2: 606.867s, 576 sessions / uwvm2-ros: 605.731s, 576 sessions。
- 新公开 DATA case debug_source_dwarf_qualified_values 在 16 个 Linux 架构 × 两仓库执行，共 32 次目标 ELF：aarch64, armel, armhf, i686, loongarch64, mips32, mips32el, mips64, mips64el, ppc32, ppc64, ppc64le, riscv64, s390x, sparc64, x86_64。ELF machine/位数/端序、原始依赖/provider 清单和 QEMU 输出对实际 pinned native x86_64 的一致性全部核对。用例包括 const/volatile/typedef、复制 carrier bits、字符串预算、非 C producer、malformed qualifier 和 unavailable capture。
- 共 59 条成功监督收据。所有编译器、validator、DWARF verifier、VM、DAP、组件和 QEMU 实测均在原 64 GiB / swap 0 cgroup 中执行。所有 guest 原始 OS wait、broker wait、DAP EOF 成功，监督树按 birth/PIDFD 退役，OOM 未增长。
- 原始 producer Wasm/DWARF 不重写。首个 cross stage 清单覆盖错误在编译前被拒绝，重新建立 a2 冻结清单后通过；组件首个 launcher 的错误 command 文件名在 admission 前停止。两项不计为成功测试。

## 尚未完成的范围

本轮完整产品以完成的 R56 冻结源加五段 R57 自有修改构建，使用此前单独固定的 LLVM23 provider。同期其他 agent 的最新整体工作区、最新 ROS provider、uwvm-int full 尚未获得本轮完整构建资格。本机精确片段和新测试同步已核对，未覆盖其他改动。

QEMU 证明本轮 portable DATA 路径的 16 架构行为，未证明完整目标 VM/JIT/DAP 或 native IDE 体验一致。所有语言/ABI、所有优化 DWARF location、standard Go/AssemblyScript 全能力、运行时格式化、变量赋值/watchpoint/guest call 等仍未全部完成。88 项能力统计保持 14 implemented、39 partial、33 missing、1 separate level、1 prohibited；ASM 仍限 guest Wasm 上下文。

## 证据和空间

详细收据：[debug_qualified_scalars_qualification_20261008.json](debug_qualified_scalars_qualification_20261008.json)。

SSH canonical：/home/macromodel/Documents/uwvm3-implementation/retained-qualified-scalars-r57-final/
归档 18,506,724 bytes，SHA256 729885e4ff6cdcb50876a635b49f24f22d3a9430fffc4c35693727974395cc00，11830 个成员逐项流式验证并 fsync。仅清理本轮 70 个可恢复中间对象，释放 354,239,984 bytes；完整 VM、原始 Wasm、目标 ELF、源清单和日志保留。

本轮自有目录约 0.609 GiB，SSH 可用空间约 31.93 GiB（当时观察值）。本机只下载小型证据与清单，未重复下载新 tar。为处理本机 ENOSPC，仅删除经 SSH SHA256 确认的旧 R49 重复下载 tar，恢复路径和摘要保留在 REMOTE.json。
