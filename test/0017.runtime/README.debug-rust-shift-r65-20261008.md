# R65：Rust 移位语义修复与跨架构验证

日期：2026-10-08。uwvm2 与 uwvm2-ros 同步修复 Rust 有符号左移：结果按 guest 类型位宽截断二补码位型，不再受现有 C 表达式路径的负值/有符号结果越界限制。有符号右移使用显式 guest 位宽符号扩展，无符号右移补零；实现不依赖宿主有符号右移。规则对照 [Rust Operator Expressions](https://doc.rust-lang.org/reference/expressions/operator-expr.html)。

~~~text
-1i8 << 1                      -> -2
64i8 << 1                      -> -128
-128i8 << 7u64                 -> 0
1i64 << 63u16                  -> -9223372036854775808
-3i8 >> 1u16                   -> -2
leaf_negative << 1u8           -> -62（原始 Rust 停帧值 -31）
leaf_negative >> 1u16          -> -16
~~~

支持有限 i/u8、16、32、64 及 guest ABI 下的 isize/usize。左右操作数可以使用不同整数类型。负移位量、移位量不小于左值位宽、浮点/bool/char 操作数被拒绝；移位结果后续发生取负、除法或取余溢出也会报错。false && ((1i8 << 8u8) == 0i8) 可以短路而不计算非法移位，死分支的错误操作数类型仍被拒绝。原 C/C++ 路径行为保留并做回归。

真实 Rust 停帧的 Watch、Hover、Variables 与条件断点均验证，包括安装和 CLI 更新：

~~~text
break-source 0 /path/to/file.rs:74 if leaf_negative << 1u8 == -62i32
condition 1 -3i8 >> 1u16 == -2i8
~~~

路径、行号与断点 ID 应替换成自己的实际值。例中 Rust fixture 的 leaf 行为 74。语言规则来自实际 Rust CU；DAP 语法准入不提供语言、guest 内存或宿主 VM 调试权限。本轮改变只涉及复制的标量值，不增加 ASM 对宿主 VM 的访问。

## 验证结果

全部编译器、VM、验证器、DWARF、原生测试程序及 QEMU 执行都在 SSH Linux 原 64GiB、swap=0 cgroup 内完成。本机没有编译或运行这些测试。既有 keeper 已运行，本轮直接复用；host 25GiB、arena 原预留及测试输出上限未降低。

两个完整产品分别从冻结的本机完整 source 捕获重建，三个 TU 和链接全部新构建，源文件/实际依赖 hash 前后闭合。普通版使用固定 LLVM23 provider；ROS 使用真实 canonical X86 23.1.1-uwvm-ros.12 provider 与原 consumer 链接顺序。其他 agent 在冻结前的改动保留于 source-review；后续工作区变化不能继承本轮资格。

| 仓库 | DAP 单测 | rustc/生产求值器对照 | 连续真实 DAP |
| --- | --- | --- | --- |
| uwvm2 | 499 | 9,096 | 601.004 s / 412 会话 |
| uwvm2-ros | 499 | 9,096 | 601.083 s / 412 会话 |

每仓库 9,096 个对照用例包括：3,000 个固定种子的原整数字面量回归、4,096 个穷举 i8 值/移位量/方向用例、2,000 个固定种子的整数位宽、符号和 count 类型组合。实际 wasm32 rustc metadata const/type 编译分别验证 9,163 个正见证、57 个负见证。生产解析/求值器结果按 bits、width、signedness、复制的类型名比对；i128/u128 在 Rust 中合法但有限 64 位载体尚不支持，不计入 rustc 非法表达式见证。

每仓库 7 个组件 build/run，Rust 数字组件通过 3,328 项检查，保留 Rust char、C++ UTF、条件命令、真值、短路和原数值回归。16 个 Linux 架构构建、执行 32 个 target ELF：aarch64、armel、armhf、i686、loongarch64、mips32/el、mips64/el、ppc32、ppc64/le、riscv64、s390x、sparc64、x86_64。每个 ELF 执行 3,328 项检查，ELF ABI、endianness、依赖、hash、退出码与 x86_64 相同 stdout 通过。这是标量组件资格，不等于完整跨目标 VM/JIT/DAP 体验。

通过的真实 DAP 共 1656 个会话，其中条件回归 816 个；Rust 后缀与新移位条件覆盖 256 个会话。矩阵有 78 个有效表达式、59 个拒绝输入，使用原始 Rust/C++ Wasm32 O1 与 DWARF4/5，不重写 DWARF。每个数字会话有两个真实停点，并检查旧 frame/scope 引用退役和只读写入拒绝。全部通过会话重新读取完整原始协议，核对 guest/broker 实际 wait=0、adapter wait=0、EOF 完整读取。假条件跳过已计数的 leaf 命中，到原 caller 的后调用停点；不可用条件保留暂停的 leaf。

另保留 16 个修复前真实对照会话。R64 两个原 VM 在当前真实帧对每个 Rust 会话的 10 个合法左移、两个停点均产生拒绝；单独保留，不混入修复后通过数。首次组件编译因测试 helper 的动态 C 字符串格式化失败，已改为 fast_io 的 os_c_str 并从头验证；初始失败切片保留。该改动是明确的测试源 overlay，完整产品中的生产源字节不变。C++ 新增字符串、输出、格式化均使用 fast_io。

## 证据与剩余项

主包留在 Linux：/home/macromodel/Documents/uwvm3-implementation/retained-rust-shift-r65-final/evidence.tar.xz，121068372 bytes，SHA-256 4bcc6e0f79603be058c9e52f71bc65c05c48f389f4e357002885b8a5e1d1b211。包含两个完整 VM、六个 fresh objects、冻结源码、guard、测试结果和失败切片；源 transport 及 R64 baseline 主证据分别固定 hash。逐成员流式 hash、原始文件前后字节、fsync 校验通过。本机只保存小型报告与清单；未删除 SDK、原 producer 或其他 agent 文件。

完整原生语言 dbg 尚未完成：一般复合常量推断、i128/u128 载体、Rust 原生表达式类型显示及完整语法、优化位置/语言 runtime/IDE 等仍有缺口。-Rdbg 当前仍只支持 LLVM-JIT full；uwvm-int full 的原生式实时语言调试和完整跨目标 VM/JIT/DAP 尚待实现、验证。没有运行 GDB/LLDB/Delve 或 IDE UI。本轮能力表保留原 88 项状态统计及 native=false，只追加 R65 的有限验证记录。

详见 [qualification](debug_rust_shift_qualification_r65_20261008.json)。
