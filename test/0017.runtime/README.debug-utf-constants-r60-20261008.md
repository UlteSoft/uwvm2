# R60：优化后的 UTF 字符常量与 Rust 浮点常量（2026-10-08）

本轮同步修复 uwvm2 和 uwvm2-ros 的 UTF 直接常量路径。真实 C++ `char8_t/char16_t/char32_t`、Rust `char` 在优化后仅有 `DW_AT_const_value` 时，现可通过 CLI、Source variables、Watch、Hover 读取，保留类型、只读属性及 concrete DIE/作用域/停点来源。另用真实 Rust producer 验证了 binary32/binary64 和负零直接常量。

## 修复与限制

`source_dwarf_constants.h` 接受已知 1/2/4 字节 `DW_ATE_UTF` 标量，按无符号码元解读。该编码用于 UTF 字符类型：[DWARF 定义](https://dwarfstd.org/issues/090109.1.html)。类型、宽度、地址宽度、直接属性与作用域校验仍保留。

Clang 21 实际把 `char8_t(0x80)` 写成 `DW_FORM_udata(18446744073709551488)`。解码器只接受与声明宽度完全匹配的全 64 位符号扩展位模式，恢复原始目标码元；一般整数未启用截断。真实原始 DIE 与源码见收据。[LLVM 常量生成实现](https://llvm.org/docs/doxygen/DwarfUnit_8cpp_source.html) 展示整数位模式和常量形式的生成路径。

负 signed UTF、任意溢出、错误扩展、8 字节 UTF、未知类型和指针仍不可用。UTF16 单个 surrogate 保留一个码元，本轮没有把它猜成字符或字符串，也没有补齐 Unicode 字面量表达式或原生 glyph/rune 显示。

C++ IO 复用现有 fast_io 数值读取和输出；没有新增 stdio/iostream、guest 内存读取或指针权限。ASM 调试仍限定 Wasm 生成的上下文，不能调试 VM 本身。

## 真实 producer 与调试验证

全部编译、validator、DWARF verify、VM、C++ 组件和 QEMU 执行只在 SSH Linux 原 64 GiB、swap 0 cgroup 内完成。每个测试树都有 birth/PIDFD 监督、原始 OS wait、子进程退役、CPU 和磁盘余量收据。shared cgroup roster 包含其他工作，不能当成本测试遗留进程。

两个仓库各重新编译三个完整 TU 并链接，冻结完整 src 与真实依赖。ROS 使用真实 X86 LLVM `23.1.1-uwvm-ros.12` provider 及原始 CMake 链接顺序。本轮没有替换其他 agent 的源码修改。

C++ 和 Rust 覆盖 O1、DWARF4/5、instruction/unwind 两种策略，每仓库修复前/后各八个 CLI 组合。Wasm 经过 validator 与官方 DWARF verify，源码、原始 Wasm 和 DIE 未重写；测试比较实际读取值及来源，不能从源码常量替代调试器返回值。

Rust 原 fixture 的返回行已离开局部常量范围；新增真实非内联观察调用，使优化 fixture 在有效范围内具有自己的 statement row。未扩大编译器范围或伪造来源。

当前 Rust 默认 DWARF5 样本的 `.debug_names` 被官方校验器拒绝。通过样本明确使用 `-C llvm-args=-accel-tables=Disable`，由编译器重新生成，未事后改写 DWARF。[Rust codegen 文档](https://doc.rust-lang.org/rustc/codegen-options/) 说明该选项传给 LLVM，[LLVM 实现](https://llvm.org/doxygen/DwarfDebug_8cpp_source.html) 定义关闭加速表的值。原始失败保留，默认名称索引兼容尚未计为完成。

DAP 验证真实 Source/Watch/Hover 类型和值一致、只读、分页、单步后旧 frame/scope 引用拒绝、新停点来源及正常 guest/broker 退出。公开回归入口为 `test/0018.debugger/run_dap_utf_constants.py`。

## 使用示例

在实际暂停的 `language_leaf_cpp` 中，用当前 stop-id 查询：

~~~text
print 1 <stop-id> leaf_octet
source local leaf_octet type=const char8_t = u8=128

print 1 <stop-id> leaf_unit
source local leaf_unit type=const char16_t = u16=955

print 1 <stop-id> leaf_point
source local leaf_point type=const char32_t = u32=128578
~~~

实际 Rust 返回：

~~~text
source local leaf_point type=char = u32=128578
source local leaf_fraction type=f32 = f32 bits=0x3fa00000 value=1.25
source local leaf_zero type=f32 = f32 bits=0x80000000 value=-0
~~~

每个成功值另有携带真实 stop、thread、Code PC、variable/scope/type DIE 身份的 `source-origin ... kind=DW_AT_const_value`。这是数值码元显示，本轮未提供 glyph 或字符串格式化。

## 最终结果

|仓库|单测|C++组件|修复前/后 CLI|DAP 会话（含循环）|循环秒数|循环会话|
|---|---:|---:|---:|---:|---:|---:|
|uwvm2|493|11|8 / 8|532|604.585|524|
|uwvm2-ros|493|11|8 / 8|532|603.951|524|

总计 51072 次真实 DAP evaluate，17024 条停点来源；修复后 CLI 另有 80 条 UTF/浮点来源。原始 guest/broker wait、DAP EOF 和退役收据通过，OOM 未增长。

16 个 Linux 架构、32 次实际目标 ELF DATA 测试通过，大小端输出一致。架构：aarch64, armel, armhf, i686, loongarch64, mips32, mips32el, mips64, mips64el, ppc32, ppc64, ppc64le, riscv64, s390x, sparc64, x86_64。值消费路径的真实依赖与最终产品字节一致；这些测试覆盖有界常量 plan 数值、类型、宽度及边界，尚未认证各目标完整 LLVM decoder/VM/JIT/DAP。

归档 406828340 bytes，SHA256 02455bd06970783b97f9d20430ad98dc1b3a5bd0659c48ffce591da17ae1a335；102401 个常规成员逐一流式校验并 fsync。先归档并清理自有对象/重复上传，最终再清理可恢复的自有旧构建 VM、重复子归档和中间对象。最新 VM、Wasm、ELF、源码、协议及外部 SDK 静态库保留。SSH 最终可用 25.60 GiB。

## 尚未完成

八种语言全部 native 调试能力、glyph/rune/string 与 Unicode 表达式、本轮其他语言真实新常量路径、默认 Rust DWARF5 名称索引兼容、完整跨架构 VM/JIT/DAP、uwvm-int full 和 module BMI 均未计为完成。历史 88 项全能力统计保持原状，只追加本轮有界资格。

完整收据：[debug_utf_constants_qualification_r60_20261008.json](debug_utf_constants_qualification_r60_20261008.json)。SSH canonical：/home/macromodel/Documents/uwvm3-implementation/retained-utf-constants-r60-final/。后续并发源码观察见 JSON，测试过的冻结 cut 与后来修改分别记录。原始 producer、失败测试、完整构建、组件、协议和监督记录均保留在校验归档中。
