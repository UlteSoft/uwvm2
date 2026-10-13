# R63：语言条件断点的 Boolean 类型与真实假条件恢复

日期：2026-10-08。两仓库同步修复条件断点：旧 manager 将条件拼成 (expr) != 0，随后只接受 signed_integer。C++ 比较结果为 bool 后被判 unavailable；Rust bool 与整数零比较也不合法。原 R62 两 full VM、两种策略、C++/Rust DWARF4/5 共 **32 个真实修复前会话**均复现：Watch 显示 false，断点仍以 unavailable 停住。原 guest/broker/adapter 最终 OS wait 为零；原产品在对照闭合、逐成员 hash/fsync 恢复归档后才删除。

修复采用 (expr) != false，由真实当前帧 CU 的语言语义求值；manager 接受 C signed predicate 或 Boolean。C/C++ 使用数值零/非零规则，含 signed、unsigned、浮点负零；Rust/Go/Zig 的这个有限路径要求 Boolean。没有使用 !!expr：Rust !integer 是按位取反。不可用变量、除零、非法类型及短路中无法解析的变量保留原停止；正确类型的 false && (1/0 == 0) 不计算除零。新增 C++ 拼接、输出和测试 IO 使用 fast_io。

## 使用方法

CLI 安装、更新与清除条件：

~~~text
break-source 0 /path/to/file.cpp:48 if leaf_negative == -31
condition 1 leaf_negative == -31
condition 1
~~~

DAP 使用 setBreakpoints 的 condition 字段：

~~~json
{"source":{"path":"/path/to/file.cpp"},"breakpoints":[{"line":48,"condition":"leaf_negative == -31"}]}
~~~

本轮实际 C++/Rust 示例：leaf_boolean、value == 5、leaf_cookie == 16、leaf_fraction > 1.0；Rust 字符比较为 leaf_point == '\u{1f642}'。本轮验证安装及 condition ID EXPR 更新；省略表达式清除形式为既有接口与命令 DATA 回归，没有另宣称清除路径 live 资格。条件规则参考 [GDB Conditions](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Conditions.html)；函数调用、副作用、完整表达式语法、指针真值和 native IDE 等价仍未取得完整资格。未实际运行 GDB/LLDB/Delve 或 IDE UI。

## 实际验证

所有编译、VM、验证器、DWARF、DAP 和 QEMU 执行均在 SSH Linux 原 64 GiB / swap=0 cgroup。没有在 Mac 编译或运行测试。两产品三 TU 与链接全部重新构建，实际依赖和输入前后 hash 闭合。普通版使用固定 LLVM23 provider；ROS 使用真实 canonical X86 23.1.1-uwvm-ros.12 provider 与准确 consumer 链接顺序。ROS 模式范围未扩展。

| 仓库 | DAP 单测 | 条件安装会话 | 连续测试 | 实际参数/局部变量及 CLI 更新会话 |
| --- | --- | --- | --- | --- |
| uwvm2 | 497 | 1200 | 601.269 s / 1040 会话 | 32 |
| uwvm2-ros | 497 | 1200 | 600.709 s / 1040 会话 | 32 |

修复后共 **2464 个真实 DAP 会话**：真条件 992、假条件 932、不可用条件 540。另有上述 32 个修复前会话，不能混入修复后通过数。

假条件必须越过实际 leaf 命中，到原 caller 的后调用见证停点；在该真实停点读取断点表，要求原条件 hits > 0，随后原 guest 自检退出 0。没有在退出后已关闭的 broker 通道读表，也不把未命中当作假条件通过。Rust 优化 DWARF 的见证行可能向后映射到 outer 返回处，记录真实帧/行/偏移，要求原 outer 且实际行不早于请求的后调用位置。真条件必须停在原 leaf、无 condition error，独立 Watch 验证预期值。不可用条件保留原 leaf；使用当前 opaque frame，避免把旧 frame 错误算成表达式失败。

真实参数 value 和局部变量 leaf_cookie 的 **64 个会话**通过 CLI 将初始 false 条件更新为表达式，再验证 true/false。原 C++/Rust Wasm、DWARF4/5、source、oracle 和 producer 日志不重写。全部实际 guest/broker/adapter wait/EOF 通过；通过 guard 的 OOM/max 事件均为零。

两仓库各 6 个 native 组件 build/run 全通过：96 项 C/C23/C++/Rust/Go/Zig condition 真值 DATA，及 Rust char、C++ UTF、短路 preflight、language numeric、条件命令回归。16 Linux 架构实际构建/执行 **32 个 target ELF**：aarch64、armel、armhf、i686、loongarch64、mips32/el、mips64/el、ppc32、ppc64/le、riscv64、s390x、sparc64、x86_64。class/machine/endianness、实际 dependencies、ELF hash/退出码和与 x86_64 相同 stdout 均验证。仅资格化数值/类型 DATA，完整目标 VM/JIT/DAP 仍未完成。其他语言本轮没有新增真实条件 DAP 资格，不继承 C++/Rust 结果。

## 来源与占用

当前两仓库四个自有测试文件字节相同并匹配 B2/L 快照；controller 的两个条件处理区段逐字匹配实际测试快照。controller 完整文件在测试冻结后增加了其他 agent 的 MIPS 大端条件宏改动，两仓库该改动相同，保留且不计入本轮资格。完整产品来自 B1 本机捕获，加上显式 B2 测试/registry 修正。最终 L helper 加入实际变量、CLI 更新并更正 listing 元数据字段；原 B2/Baseline/D1 helper 与原结果保留，不假称旧 helper 执行了新选项。其他 agent 检查点源码改动保留；后续差异见 qualification.local_integration，未测差异不继承通过。AST/type 数据不授予 guest memory、写或 host VM ASM 调试权限。

保留失败切片：BE1 缺 dwarfdump 准入 pin；BE2 helper 名被局部列表遮蔽；BE3 退出后读已关闭通道；BE5 过严要求优化 Rust 行恰等于请求行；C1 错误假定 C predicate 内部 category，以及非 Zig 的 && 拼写。只计修正后的 BE6/C2/Q1/B2/D1/L1。Mac ENOSPC 时未截断源码；核验远端副本后只清理自有旧本地副本。Linux 产品/冗余目录先验证恢复包，不触碰其他 agent 文件。

能力表仍为 88 项，conditional_break 保留 partial，完整 native 等价为 false。Rust 数字后缀、原始 Unicode/字形、完整其他语言/运行时、完整跨架构 VM/JIT/DAP，以及原 Rust 默认 DWARF5 debug_names producer 问题继续待完成。

主证据包：/home/macromodel/Documents/uwvm3-implementation/retained-conditional-truth-r63-final/evidence.tar.xz，96712884 bytes，SHA-256 95ce37a8c31852c379bd00f91716723a57acdc375c6a5df091ea5c590febd8f5。regular member 流式 hash、只引用此前已验证的内部 hardlink、原文件前后 hash 与 fsync 全通过。本轮 full VM 和 fresh objects **包含在主归档**；历史 R48/R62 产品恢复包和源上传另行固定 hash。详见 [qualification](debug_conditional_truth_qualification_r63_20261008.json)。
