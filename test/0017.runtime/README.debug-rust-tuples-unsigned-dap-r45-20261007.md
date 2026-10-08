Rust 元组与 C/C++ unsigned int 的真实 DAP 验证（R45）

本轮给 uwvm2、uwvm2-ros 补齐 Rust 元组的真实语言停点验证，并修复实际 C/C++ 算术结果在 DAP 里显示整段协议文本的问题。源码已有数字成员别名，但此前 rust_tuple_field 的证据主要是 metadata/owned-byte 查询，未取得真实产品停点资格。新 Rust fixture 使用 tuple、tuple struct、嵌套 tuple 和 tuple 数组，在原 source_tuple_probe 停点检查 22 行完整复制树；Source variables、Watch、Hover、Variables 的值与类型一致。

| Rust Watch 示例 | 实际值 |
|---|---|
| object.pair.0 | -9 |
| object.pair.1 | 42 |
| object.tuple_struct.0 | -13 |
| object.tuple_struct.1 | 55 |
| object.nested.0.1 | 21 |
| object.arrays[1].1 | 13 |
| (*object.next).pair.0 | -9 |
| object.pair.0 + (object.pair.1 as i32) | 33 |

数字字段的每个结果还与实际 __0/__1 producer 选择器对照，偏移仍由原 DWARF 决定。六种 tuple 子对象均可直接求值、展开并按 named children 分页；数组层仍是 indexed children，tuple 字段保留原 __0/__1 名称和 named 分类。括号和显式 guest pointer 解引用通过原 frame/stop/guest copy transaction，不从 DATA 标签生成查询或内存地址。越界、冗余零字段 .00、不存在字段、动态下标、赋值、自增、命令注入和 raw pointer 隐式成员读取均拒绝。真实单步后旧 frame、scope、object、tuple/nested/array 子对象等七个读取拒绝。

Rust Reference 将 tuple/tuple struct 的字段访问定义为 decimal tuple index，并限制冗余前导零等形式：[tuple indexing](https://doc.rust-lang.org/reference/expressions/tuple-expr.html#tuple-indexing-expressions)。本轮只资格化原 rustc 1.99.0 Wasm32 O0 DWARF4/5 产物上的上述有限只读选择器，不宣称完整 tuple 构造、Rust 类型语义、引用自动解引用、Deref/trait、collections、泛型/优化位置或完整 native 体验。[GDB Rust 文档](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Rust.html)区分 tuple expressions、语句式表达式和泛型推导等限制；本轮没有实际运行 native GDB，也没有把文中 rustc 1.8 的历史文字作为当前编译器结论。

C/C++ 对照使用与 Rust producer 相同的 __0/__1 字段名。真实 C/C++ CU 的显式 .__0/.__1 正常读取，12 个对应数字别名拒绝，确认本轮有限别名不能只凭字段拼写跨语言启用。检查这些对照时发现原 VM 已输出 $expression: type=unsigned int, offset=0, bytes=4, value=33，适配器却保留原整段 source-value packet；原 C DWARF4 失败已保存。

修复只扩展 source_evaluation_display 的有限整数根语法：精确 canonical unsigned int 必须四个 copied bytes，取值必须在 0..4294967295，保留 unsigned int 类型文本和 readOnly/variablesReference=0。类型别名、其他 C primitive/未知 ABI 仍保留原 opaque display。负数、溢出、非 canonical decimal、错误位宽和把 unsigned int 移用到 Go $len/$cap 均拒绝；没有增加查询、写接口、evaluateName 或 memoryReference。VM 的 copied_type_name 明确产生该 canonical primitive spelling，已有 fast_io formatter 继续提供原结果；本轮没有新增或改写 C++ I/O。

| C/C++ 实际 Watch 表达式 | 原 guest 编译自检及 DAP 结果 |
|---|---|
| object.pair.__0 + (object.pair.__1) | 33 |
| object.pair.__1 + 2147483606u | 2147483648 |
| object.pair.__1 - 43u | 4294967295 |
| object.pair.__1 - 42u | 0 |

两仓库各自的原 no_std Rust fixture 和 C-family 对照源文件均由原 pinned official Rust/Clang 工具链在 Linux 原 cgroup 编译并由原 wasm-tools validate。最终 producer 每仓库两个 Rust、两个 C、两个 C++ Wasm32 DWARF4/5 profiles，Wasm/DWARF bytes 没有重写。每个 guest 的真实自然运行自检 4096 次并返回 130；C/C++ 同时检查上述三个 unsigned 边界和四个部分和。首次 Rust tuple fixture 是 130 的总和自检，后续原 fresh producer 增加分组编译自检；每个 cut 及原 receipts 独立保存。

每仓库最终 418 项冻结 DAP tests 通过，五项新增 unsigned-root tests 检查范围、typed/untyped、位宽/格式、Go builtin type 和 unknown type fallback。机器记录仍列出未调用的三个外部 controller formatter-packet runner，不能把 unit PASS 当作其新 WASIp1 runtime 资格。最终 cut 中 Rust DWARF4/5 × instruction/unwind 共 8 个会话；C/C++ 短测为 DWARF4/5 × instruction/unwind × 两仓库共16个会话，包含真实 unsigned 边界。随后每仓库至少600秒的 C/C++ 长测轮换四个 profile 与两个 call-stack policy，所有组合均出现。

| 仓库 | 实际 C/C++ DAP 循环 | 会话 |
|---|---|---|
| uwvm2 | 601.972 秒 | 171 |
| uwvm2-ros | 603.402 秒 | 172 |

最终 cut 的 C/C++ 会话共 359 个；与8个Rust会话合计 1468 次完整树呈现、32296 行、16803 次字段读取、6750 次tuple子对象读取、4404 次部分和比较、3231 次真实unsigned边界比较、8712 次预期表达式拒绝及2569次真实单步旧引用拒绝。

C/C++每会话45次原字段/括号/显式pointer读取、18次子对象展开、12次部分和比较、9次unsigned边界比较、24次表达式拒绝；Rust每会话81次字段读取、36次子对象展开、12次部分和比较、12次表达式拒绝。四种入口均核验22行树，七个旧引用检查均在真实单步后执行。

两仓库已有 C/C++/Objective-C/Objective-C++ × Wasm32/64 × DWARF4/5 的32个 enum/array会话、TinyGo两个会话及180个 builtin 比较、Rust 原变体/Option 与嵌套变体各4个会话在修复后适配器回归通过。ROS该回归是原 final adapter 的 R45 cut4（与最终 cut5 的 production adapter bytes 相同）；普通版 cut5 重跑其缺失回归。Rust 元组在修复前适配器另有持续测试：uwvm2 600.035秒 / 40会话；uwvm2-ros 612.404秒 / 41会话。这些重复计数不是独立功能数量或原生等价百分比。

55个原 guard jobs中50个通过、5个原失败保持失败：一个是实际 unsigned int 显示问题，四个是20秒 Docker cgroup admission timeout，尚未执行功能测试，原停止 bootstrap 已按 birth/PIDFD 退役并reap。另修复新对照 runner 的检查遗漏：非Rust旧frame evaluate改为此前真实可读的 .__0 路径，避免一个原本就不支持的 .0 查询造成失效检查假阳性；原失败/未执行 cut3保留。最终 cut5 的16个串行 jobs全部通过，没有放宽 guards 或替换失败 receipts。

所有实际编译、validator、VM、DAP、unit 与功能测试都在 SSH Linux 原64GiB/swap0 cgroup及birth/PIDFD supervisor中执行。195个producer tool/runtime pins、108个DAP运行库pins以及每个closed code cut前后不变。每个成功会话核验原OS guest wait=0、adapter实际EOF/returncode=0，原协议逐成员SHA/fsync验证后仅退役其已保留的raw文件。owned RSS限1GiB、outputs64MiB/file8MiB/log1MiB，arena floor9288400896 bytes、host reserve25GiB/inode4096未降低，全部OOM counters不变。keeper与peer进程保留，不要求整个共享cgroup为空。

本轮复用原 R41 scoped full VM，未重建当前并发整个工作区。ROS仅使用原固定 LLVM23 provider的full产品；最新ROS provider仍未资格化。没有新增实际IDE GUI、完整QEMU架构VM/JIT/DAP、Rust Wasm64/优化layout或完整语言层native等价资格；ASM仍限定Wasm产生的guest上下文。能力清单仅为rust_tuple_field、aggregate、int_expr追加有限证据，保留原statuses及其他85类，不扩大source_status_counts。

机器记录：[debug_rust_tuples_unsigned_dap_qualification_20261007.json](debug_rust_tuples_unsigned_dap_qualification_20261007.json)。Linux证据：/home/macromodel/Documents/uwvm3-implementation/retained-rust-tuples-r45-final。主归档11407976 bytes / 3644 members，SHA256 7f4765e9bdfd4ba94eb3f2520082f09ed85de94ac7b8e872c9dc71f707d534a0；archive-proof-r45.json列出逐成员hash/fsync和原输入不变。原R41 VM与R42工具链归档按记录SHA引用，没有重复保存大文件；仅本轮8个已独立归档验证的冗余upload tar退役，释放12033024 bytes，保留所有source cuts、products、toolchains与peer文件。
