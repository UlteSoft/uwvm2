# R62：Rust 字符表达式与原始 DWARF 字符身份

日期：2026-10-08。本轮只资格化有限的 Rust 字符 Source/Watch/Hover 表达式，以及 C++ UTF 回归；完整 native 语言调试和完整多架构 VM/JIT/DAP 仍未完成。

两个仓库已同步修复：Rust ASCII 字符、简单转义、两位 ASCII `\xHH`、最多六位十六进制的 `\u{…}`，以及 Rust Unicode scalar 的 `char` 类型。比较返回 Boolean；字符可显式转为整数，unsigned eight-bit 值可转为字符；同字符类型的转换保留字符身份。字面量、运算结果和 DWARF 字符变量采用同一有限原语类型路径。变量身份检查原始 Rust CU、`DW_ATE_UTF`、四字节 scalar、真实声明元数据及 Unicode 范围，不用显示名称推断类型。

| Watch / Hover 表达式 | 实际结果 | 显示类型 |
| --- | --- | --- |
| `'\u{3bb}'` | 955 | char |
| `'\u{1f642}'` | 128578 | char |
| `'\u{1f642}' as u8` | 66 | unsigned integer |
| `(255 as u8) as char` | 255 | char |
| `leaf_point == '\u{1f642}'` | true | bool |

Rust 数字后缀语法 `255u8` 仍未实现；上表的 `(255 as u8) as char` 使用已经支持的显式转换语法。Rust 原始 Unicode 字符输入、字形显示、byte literal、完整数字/类型名/语言语法仍待完成。字符加法、位运算、与整数直接比较、转换为 float/Boolean、signed eight-bit 或 wider integer 转为 char 等非法组合会拒绝；即使出现在 Boolean 短路的右边，仍验证类型。`\x80`、C 风格八进制、无效 scalar/代理值/超范围和非法六位格式也拒绝。前端只做有限语法准入，原始当前帧 CU 决定语言；C++、Go、Zig 等帧不能因此获得 Rust 字符语义。

语义依据：[Rust literal expressions](https://doc.rust-lang.org/reference/expressions/literal-expr.html)、[Rust token grammar](https://doc.rust-lang.org/reference/tokens.html#character-literals)、[Rust cast expressions](https://doc.rust-lang.org/reference/expressions/operator-expr.html#type-cast-expressions)。编译器 const assertion 在已经安装的 `wasm32-unknown-unknown` 目标上执行编译期求值；该 Rust 工具链没有 native Linux 标准库，本轮没有伪称执行 native Rust 见证程序。

## 真实测试结果

所有编译器、验证器、运行时、DWARF 和 QEMU 执行都在 SSH Linux 原 64 GiB cgroup；没有在 Mac 编译或跑测试。两产品的三 TU 与链接均重新构建，原始输入和实际依赖闭合。普通版使用独立固定的 LLVM23 provider；ROS 使用真实 canonical X86 `23.1.1-uwvm-ros.12` provider，保留准确 consumer link 顺序，不宣称其他本地/vendor/provider 版本已经通过。

| 仓库 | DAP 单元测试 | 随机解析器对照 | 真 DAP 会话 | evaluate | 连续测试 |
| --- | --- | --- | --- | --- | --- |
| uwvm2 | 497 | 10,000 | 480 | 80640 | 600.963 s / 472 会话 |
| uwvm2-ros | 497 | 10,000 | 484 | 81312 | 604.629 s / 476 会话 |

两仓库合计 **964 个实际 DAP 会话、161952 次 evaluate、57840 次新增 Rust 字符表达式求值、15424 项原始常量来源证明、29884 项预期拒绝**。每会话包含真实队列 step、旧 frame/scope 失效、新停点及原始 guest/broker/DAP EOF/OS wait；拒绝使用重新获取的当前帧，不把过期 frame 拒绝算成语言失败。

每仓库 10,000 seeded production-parser 对照；独立 Rust 编译器的 const assertion 检查值/比较/转换，24 个非法表达式须收到真实编译错误。C++ UTF、普通字符、ASCII escape、语言 numeric 与 logical preflight 的 6 个 native 组件各仓库全部通过。原始 C++/Rust DWARF4/5 Wasm、编译日志与常量来源不修改；Rust DWARF5 原 producer 的 Disable accelerator 选项及默认 debug_names 验证问题未被修复或隐瞒。

修复前对照使用已经存在的 R61 真 full 运行时，两策略、四原始 profile、两仓库共 16 会话：256 项新 Rust 表达式缺口，其中 128 项正确当前帧下拒绝、128 项返回错误原语类型/结果。它们的原二进制完成对照后以逐成员流式 hash 验证归档。

QEMU/本机目标实际构建并执行 **16 架构、32 个 target ELF**：aarch64、armel、armhf、i686、loongarch64、mips32/el、mips64/el、ppc32、ppc64/le、riscv64、s390x、sparc64、x86_64；逐一验证 class、machine、endianness、实际 dependencies、ELF/hash 及退出码。这里只证明 Rust 字符解析/值/类型/编码元数据 DATA，完整目标 VM/JIT/DAP 与硬件上的 native IDE 体验仍未资格化。

## 边界与证据

当前十个自有源码/测试文件在两仓库字节相同且与测试快照一致。两仓库各自冻结快照之后的其他 agent 检查点改动保留，详见 qualification 中 local_integration；这些后续改动不算本轮通过。ROS 模式范围不扩展。C++ 新 IO 使用 fast_io，字符数字扫描使用 fast_io::parse_by_scan。字符 primitive 与语法节点只持有 DATA，不发放 guest 内存、写权限、语言运行时、frame 或 host VM ASM 访问权。

条件断点尚需后续独立修复/资格化：当前源码的 condition manager 把表达式包为 `(expr)!=0` 且要求 signed_integer；C++ predicate 返回 bool，Rust bool 与 integer 比较无效。这是本轮源码审阅发现的未解决项，本轮没有测试或声明语言条件断点 live PASS。Host VM 本身的 ASM 调试仍明确禁止。

失败切片与原始日志保留：C1 改名错误、C2/C4 旧 source-cut 指向、C3 数字后缀未支持、P1 native Rust 标准库未安装、BE1/Q1 metadata admission pin 不全、Q2 磁盘保护主动停止。只有 corrected C5/P2/BE2/Q3 和本轮 D1 完成结果计入通过；不降低 cgroup/磁盘/出生身份/PIDFD 验证门槛。

证据归档：`/home/macromodel/Documents/uwvm3-implementation/retained-rust-characters-r62-final/evidence.tar.xz`，50467336 bytes，SHA-256 `6eaa3301f135f198be49df40534367daf5a318c9a22ea742cee301dfa556659f`。所有 regular members 流式验证，hardlink 只引用此前已经验证的内部成员，原文件前后 hash 相同并 fsync。当前两 full VM、源上传和已验证的旧产品/对象恢复包另行保存并固定 hash，不假称它们位于主归档中。对象删除前先验证恢复包；旧 source 副本删除前逐成员验证 R58–R61 的既有归档，只清理本任务拥有的可恢复文件，不触碰其他 agent 目录。详细数字与命令路径见 [qualification](debug_rust_character_expressions_qualification_r62_20261008.json)。
