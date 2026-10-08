# R64：Rust 数字后缀、直接常量类型推断与有界长测

日期：2026-10-08。两仓库同步补齐 Rust 有类型数字字面量和直接相邻的无后缀常量推断。有限只读求值由真实停止帧的 Rust CU 决定语言规则，DAP 的联合语法准入不授予语言、内存或宿主 VM 调试权限。

支持 i/u8、16、32、64，isize/usize 跟随 guest 位宽；f32/f64 支持十进制、小数、指数和整数式写法 1f32。二、八、十六进制前缀和 Rust 下划线形式按实际 rustc 验证。无后缀数字在没有约束时分别使用 i32/f64；本轮只补直接字面量或其单层负号与相邻数字操作数的类型约束。规则对照 [Rust Literal Expressions](https://doc.rust-lang.org/reference/expressions/literal-expr.html)，负号、溢出和运算对照 [Rust Operator Expressions](https://doc.rust-lang.org/reference/expressions/operator-expr.html)。

~~~text
255u8 - 1                 -> 254
1 + 1u8                   -> 2
-128i8                    -> -128
0xff_u8                   -> 255
1.5f32 + 2.0              -> 3.5
leaf_positive == 0        -> false（实际 u32 = 23）
leaf_fraction > 1.0       -> true（实际 f32 = 1.25）
~~~

可在 Rust 停止帧的 DAP Watch/Hover/Variables 中使用。CLI 仍为 print THREAD STOP EXPR。带后缀条件安装和更新也经过真实验证：

~~~text
break-source 0 /path/to/file.rs:74 if leaf_positive == 23u32
condition 1 leaf_fraction > 1.0f32
~~~

行号及断点 ID 应使用自己的源文件和实际返回值。本轮 fixture 的 Rust leaf 行为 74。无符号负号、越界、错误后缀、混合明确类型和溢出被拒绝；短路分支仍进行类型检查，符合类型的 false 条件不会计算分支中的除零。C++ 原有数字/UTF 字符语法和真假条件作为回归保留。C++ 新增的字符串、输出、扫描均使用 fast_io；无后缀浮点保留规范化原始数字，再直接 parse_by_scan 到目标 f32，避免先经 f64 再转换。

完整原生语言 dbg 尚未完成。i128/u128 在 Rust 中合法，本调试器的有限 64 位载体尚不支持，不能把这两个拒绝算作 rustc 的非法表达式。更一般的复合常量推断、完整 signed shift、原生类型名/完整语法、优化位置和其他语言/IDE/runtime 仍待完成。

## 实际验证

所有编译器、VM、验证器、DWARF、DAP 和 QEMU 执行均在 SSH Linux 的原 64GiB、swap=0 cgroup；Linux 重启后恢复了原 keeper 和原有界镜像。未在 Mac 编译或执行这些测试。

B5 是冻结的完整本机源码捕获，两产品的三个 TU 和链接全部重新构建，源文件和实际依赖前后 hash 闭合。普通版使用固定 LLVM23 provider；ROS 使用真实 canonical X86 23.1.1-uwvm-ros.12 provider 和原 consumer 链接顺序。本轮 live DAP 为 LLVM-JIT full；完整 uwvm-int full、跨目标 VM/JIT/DAP 和八语言 native 等价没有继承这些结果。

| 仓库 | DAP 单测 | 随机整数 | 完整连续测试 |
| --- | --- | --- | --- |
| uwvm2 | 499 | 3,000 | 600.908 s / 440 会话 |
| uwvm2-ros | 499 | 3,000 | 600.861 s / 440 会话 |

本轮通过的真实 DAP 会话共 **1600**：数字求值 912，条件回归 688；其中 **128** 个 Rust 后缀条件会话覆盖 DWARF4/5、两种策略、安装和 CLI 更新。数字矩阵含 50 个有效表达式、43 个拒绝输入，原始 C++/Rust Wasm 与 DWARF 不重写。每个数字会话有两个真实停点，并验证旧引用退役、当前新 frame、只读对象和 guest 自检。通过的每个会话均重新检查原 guest/broker 的实际 wait=0、adapter wait=0、EOF 完整读取及原始协议归档字节。假条件必须跳过已计数的 leaf 命中并到原 caller 的后调用停点；不可用条件保留原 leaf。

每仓库 7 个组件 build/run，通过 229 项 Rust 数字语义检查和原有 Rust char、C++ UTF、条件命令、真值、短路和数值回归。每仓库真实 wasm32 rustc metadata const/type 检查 3,043 个正见证、41 个负见证；随后执行生产求值器的 3,000 个固定种子随机整数用例。合计 6,000 个随机用例。没有安装或执行 native Linux Rust std，也没有运行 GDB/LLDB/Delve 或 IDE UI。

16 Linux 架构实际构建/执行 **32 个 target ELF**：aarch64、armel、armhf、i686、loongarch64、mips32/el、mips64/el、ppc32、ppc64/le、riscv64、s390x、sparc64、x86_64。ELF class/machine/endianness、实际依赖、hash、退出码和与 x86_64 相同 stdout 通过；各执行 229 项检查。这是标量语义组件资格，完整目标 VM/JIT/DAP 体验仍未完成。

## 对照、占用与来源

16 个修复前真实对照会话单独保留：原 R63 VM 上的 Rust 后缀输入到达实际当前帧后被拒绝，避免把已退役 frame 的错误当作语法缺口。未混入修复后的通过数。

保留 B1 的过宽 DAP 准入对 C 数字语法的失败切片、早期 rustc 准入与 const oracle 修正，以及 B4 在发现直接无后缀推断缺口后主动取消的 282.757 秒长测。D2 的长测分别在 555.063/487.113 秒被原 64MiB 输出预算终止，不能算 600 秒通过；此前闭合的 9 个阶段各自合格，完整 D2 guard 调用仍记录失败。D3 补齐每个已结束会话 results.json 的 XZ、fsync、解压字节/hash 校验，再退休明文；两次 600 秒长测从头执行，保留原 64MiB 上限。完整原始结果和 DAP 协议均可恢复。

B5 产品源码不因 D2 条件测试或 D3 保存方式变化而修改；两个测试 helper 的显式 overlay 有独立 hash，已同步到本机两仓库。9 个自有文件字节一致；后续其他 agent 的 source 变化保留，完整当前工作区不继承 B5 的通过。具体差异、source IDs、VM/guard/工具/producer 和输入证明见 qualification.local_integration。

旧产物仅在既有或新恢复包的逐成员 hash、原文件前后字节、fsync 和进程引用检查闭合后回收。未删除 SDK、原 producer、其他 agent 文件。R64 累积退休的原始文件字节约 3.7GB，恢复包占用单独记录；该数不是磁盘净增长。能力表仍保留 88 项、原 source 状态统计及 native=false。

主证据包：/home/macromodel/Documents/uwvm3-implementation/retained-rust-numeric-r64-final/evidence.tar.xz，123577336 bytes，SHA-256 3dc888f49a46b66c091ec98eb467ac6e699819b8e6bc4bb4dca375b4ca676f85。包括本轮完整 VM、冻结 source、guard、通过及中断切片；fresh objects、原始 source transport 和历史恢复包分别固定 hash。主包逐 regular member / 此前内部 hardlink 的流式验证及原文件前后 hash/fsync 通过。详见 [qualification](debug_rust_numeric_qualification_r64_20261008.json)。
