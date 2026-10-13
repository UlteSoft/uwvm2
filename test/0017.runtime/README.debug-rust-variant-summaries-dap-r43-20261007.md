# Rust 活动变体摘要和并发 DAP 回归修复（R43）

两个仓库已补齐 Rust 活动变体的折叠摘要。R42 能展开完整对象树，但变量列表和直接求值只显示类型名；R43 在同一真实 source_variant_probe 源停点显示以下复制结果，Source variables、Watch、Hover、Variables 和直接 object.FIELD 求值保持一致。

| 字段 / 求值表达式 | 摘要 |
|---|---|
| object.negative | Negative { signed: -3 } |
| object.positive | Positive { unsigned: 7 } |
| object.empty | Empty |
| object.some | Some { __0.__0.__0: 13 } |
| object.none | None |

实际使用时，在 Rust 源断点选中 source_variant_probe frame，Watch 输入 object.negative 或 object.some，便能直接查看活动分支和复制载荷；展开后仍有原 <variant-part>、判别字段、活动分支、同名 wrapper 和全部载荷字段。整个对象仍有原来的 41 行，数组 1..6、自指针和类型保持原样。__0.__0.__0 是编译器真实字段路径的显示文本；尚未将它猜测转换成 Some(13)。guest self-pointer 仍是只读叶子。

[DAP Variable 定义](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#variable)建议给结构值提供折叠时可识别的单行表示，并用 variablesReference 保留展开入口；本轮只改复制数据的表示。没有额外 VM 查询、ABI/类型名推断、inactive payload 读取、标签派生 selector、evaluateName、memoryReference 或赋值入口。

摘要在整个 canonical packet 校验完成后生成。owner 必须只有一个已验证的 variant-part，且 VM 已选出唯一可用活动分支。完整载荷摘要限制为 64 个复制节点、8 层字段、32 个值、256 UTF-8 bytes。字段须为有限基本名称或数组下标；未知标签、不可读载荷、嵌套 variant、超过预算等情况只显示已证实的 case 名，原完整子树仍可展开。不可用 selection 或有普通 sibling 字段的 owner 保留原表示；不显示误导性的部分载荷。

## 测试和并发回归

所有功能测试均在 SSH Linux 原 birth/PIDFD 监督器和 64 GiB / swap0 cgroup 中执行。主 cut 每仓库 137 个 DAP tests 通过；4 个 R42 适配器对照会话确认原树已经能展开、owner value 等于 type。R43 两种 DWARF × instruction/unwind 短测和持续会话通过。

- uwvm2：实际 DAP 循环 601.006 秒，38 个会话。
- uwvm2-ros：实际 DAP 循环 615.117 秒，39 个会话。

主 cut 合计 85 个修复后 Rust 会话：340 次树呈现、13940 行复制节点、1700 次活动分支检查、1275 次直接变体求值、2975 次 owner 摘要检查、1530 次直接数组求值、1020 次边界拒绝、510 次真实单步旧引用拒绝。每个 guest 原循环自检 4096 次，自然退出 0；核验 original guest wait、broker/adapter returncode、实际 EOF，以及原始协议归档逐成员 SHA 和 fsync。

C/C++/Objective-C/Objective-C++ × Wasm32/64 × DWARF4/5 的 32 个原 enum/array 会话、TinyGo 两个原 array/text 会话与 180 次 builtin 比较、原 R42 Rust 树的 4 个会话均通过。以上为重复检查计数，不是独立语言功能数量。

期间另一并发改动加入 Wasm pre-trap 显示。冻结整合 cut 发现 pretrap/begin 初始化误放在 lexical-layout parser，而 typed-state parser 使用了未定义变量，造成 NameError。双仓库已将初始化移回 typed-state parser；只允许 operands 使用该 note，保留其他 parser 的原完整校验。新增有限协议测试验证正常包、pre-trap 包、两种 note 组合及错误选择/重复/未知 note 拒绝。最终整合 cut 每仓库 138 项测试通过，两种栈策略和 DWARF4/5 的 8 个真实 Rust 会话通过。主 cut 与整合 cut 合计 93 个修复后摘要会话。实际 VM trap capture 没有在本轮新增资格；该项修复只资格化复制协议处理和已有语言会话兼容。

24 个原 guard jobs 中 22 个通过、两个 NameError 整合失败保持原失败记录；修复后使用新 cut 验证，没有重写原 receipts。每个 cut 的 94 个 closed code/runtime pins 及 108 个运行库 pins 前后不变。仅改 Python presentation/tests，没有新增 C++ I/O。其他 agent 修改保留；最终整合 adapter 与配对工作区相同字节时核验。

owned RSS 仍限 1 GiB，outputs 64 MiB、file 8 MiB、log 1 MiB；arena floor 9288400896 bytes、host reserve 25 GiB、inode floor 4096 未降低。主长测 RSS 峰值约 227.7 / 226.7 MiB，OOM counters 不变。监督器只退役原始 birth/PIDFD 绑定的本轮进程，并核验 reap；cgroup keeper 和其他测试进程保留。

## 资格范围和仍未完成

复用原 R41 两个 scoped full VM 和原 R42 官方 rustc 1.99.0 Wasm32 O0 DWARF4/5 产物；没有重新编译 VM、改写 Wasm/DWARF 或重复保存 Rust 编译器和 VM 大文件。ROS 仍使用原固定 LLVM23 provider，只涉及其 full 模式。

本项为有限活动分支/字段摘要；完整 Rust pretty-printer、collections、trait/generic、优化变量位置、Wasm64、完整表达式/native GDB 等价仍未完成。[GDB Rust 文档](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Rust.html)是功能比较依据，本轮没有实际运行 native GDB。当前整个并发工作区、最新 ROS LLVM provider、实际 IDE GUI、完整 QEMU 各架构 VM/JIT/DAP 仍未取得本轮资格。ASM 仍只允许 Wasm 生成上下文，禁止进入 VM/host helpers。

能力统计仅给 aggregate 和 rust_variant 追加本轮有限证据；88 类的 source_status_counts 和其他 86 类保持原状态，未将 partial 升为完整实现。

机器证据：[debug_rust_variant_summaries_dap_qualification_20261007.json](debug_rust_variant_summaries_dap_qualification_20261007.json)。Linux 原始证据目录为 /home/macromodel/Documents/uwvm3-implementation/retained-rust-variant-summaries-r43-final；主证据归档 2139128 bytes、920 members，SHA256 fb4f5f922f8740fc3105a62294ce6a39f63eb53a7afb9d1cffad35d238388e24。archive-proof-r43.json 记录逐文件/逐成员 hash、原输入前后不变和 fsync。R41/R42 原产品、源码和 official compiler archives 按机器记录 SHA 引用；本轮只归档约 2.04 MiB 新证据。


R44后续：有限嵌套复制case/Option/数组载荷摘要已通过真实新样例，原完整子树保留，见 [README.debug-rust-nested-variants-dap-r44-20261007.md](README.debug-rust-nested-variants-dap-r44-20261007.md)。R43原证据与资格范围保留。
