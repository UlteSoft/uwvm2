本轮同步补齐 uwvm2 / uwvm2-ros 的 C++ UTF 字符表达式，版本 R61（2026-10-08）。

在真实 `-Rdbg` 的 C++ 暂停帧中，Watch、Hover 和 variables 求值现支持 `u8` / `u` / `U` 前缀的单码元字符、数值转义、固定四/八位 Unicode 转义、`char8_t` / `char16_t` / `char32_t` 数值转换、`sizeof`、整数提升、比较和三目表达式。求值仍由原始选中帧 CU 的语言决定，语法预检不会赋予 C++ 语言、内存或指针权限。

| Watch 表达式 | 实际结果 | 类型 |
|---|---:|---|
| `u8'\x80'` | 128 | char8_t |
| `u'\u03bb'` | 955 | char16_t |
| `U'\U0001F642'` | 128578 | char32_t |
| `sizeof(char16_t)` | 2 | unsigned long |
| `+U'\U0001F642'` | 128578 | unsigned int |
| `1 ? u'\u03bb' : u'a'` | 955 | char16_t |
| `leaf_point == U'\U0001F642'` | true | bool |

C++ 的数值转义表示码元，Unicode 转义表示 Unicode 标量。例如 `u'\xD800'` 可以检查码元 55296，`u'\uD800'` 会拒绝；`u8'\u0080'` 和 `u'\U0001F642'` 分别需要多个 UTF-8/UTF-16 码元，也会拒绝。普通非 ASCII 字符、直接键入的非 ASCII 字符、`L` / wchar_t、C++ 新式带花括号/命名转义仍未实现。规则依据 [C++ 字符字面量](https://eel.is/c++draft/lex.ccon) 和 [整数提升](https://eel.is/c++draft/conv.prom)，并有真实编译器的值、宽度和类型对照。

所有编译、VM、DWARF/validator、原生组件和 QEMU 执行均在 SSH Linux 原有 64 GiB、swap=0 cgroup 内完成。普通版及 ROS 均重新编译三个完整产品 TU 并链接；ROS 使用真实 canonical X86 LLVM `23.1.1-uwvm-ros.12` 的完整静态 SDK 和 CMake 的精确消费者链接顺序。测试没有修改 Wasm/DWARF，C++ IO 使用 fast_io，转义数值使用 `parse_by_scan`。

- 每仓库 495 项 DAP 单元测试、3 个原生字符组件通过；ASCII 字符回归与 UTF 编译器对照同时运行。每仓库另有 10,000 组确定性 UTF 数值/类型/sizeof/三目属性检查通过。
- 新产品真实 DAP 共 1,016 个会话、109,728 次求值，其中 60,960 次为新增 C++ UTF 表达式；16,256 次直接 `DW_AT_const_value` 来源核对、11,176 次预期拒绝通过。
- 两仓库各 500 个连续会话，实际主驱动分别运行 603.573 秒、603.732 秒；监督器墙钟分别约 604.391 秒、604.509 秒。instruction / unwind 均覆盖 C++、Rust 的 DWARF4/5，原始 guest、broker、DAP EOF 与 PIDFD 退出均核对。
- 修复前产品在真实当前帧上拒绝新增 C++ 表达式，16 个基线会话共 320 次 C++ 拒绝。before-a1 使用失效引用，明确不计入有效基线。
- QEMU 实际运行 16 种架构、两仓库共 32 个目标 ELF，覆盖字符解析、值、类型、转换和提升。包括 x86_64、aarch64、i686、riscv64、loongarch64、ppc32/64/64le、mips32/32el/64/64el、sparc64、s390x、armel/armhf；目标 ELF 机器、位宽、字节序及实际依赖已核对。这些是组件 DATA 结果，全目标 VM/JIT/DAP 体验尚未获得同等验证。
- 所有合格监督器命令均原始 wait=0、所有本轮拥有的 PIDFD 退休完成，cgroup OOM 计数保持 0。未终止共享 cgroup 中其他任务。

仍需继续实现：Rust/Go/Zig 的原生 Unicode 字符表达式语义、字符/字形显示、DWARF UTF 变量在三目结果中的可靠原生类型身份、更完整的语言表达式与所有架构完整 VM/JIT/DAP 验证。Rust 默认 DWARF5 的 .debug_names 缺项问题仍在；本轮真实生产者用 `-C llvm-args=-accel-tables=Disable` 通过官方验证，没有改写 DWARF。完整 native/IDE 体验未宣称完成，既有 88 项统计状态及其他 agent 后续修改保留。

证据存放于 SSH Linux：
`/home/macromodel/Documents/uwvm3-implementation/retained-utf-expressions-r61-final`。
紧凑归档保留真实源码、失败记录、日志与协议；共享源树的硬链接只解析到归档内先前已验证的成员。当前两个完整 VM、原始源上传压缩包和两个对象恢复压缩包单独保留并校验哈希，未重复放入该归档。两次归档准备因 25 GiB 磁盘保留线中止，已记录并只移除本轮未完成输出；最终归档 SHA/清理定位见配套 JSON。只清理了逐项校验、已有恢复归档的本轮旧 VM、对象、安装包和本机重复证据，没有删除其他任务文件。
