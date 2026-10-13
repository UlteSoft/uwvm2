Rust、Go/TinyGo、Zig 原始类型谓词修复与有限验证（R48）

两仓同步修复三类语言比较结果退化为 int 的问题：根据真实所选帧的 DW_LANG_Rust / Go / Zig 元数据，或原已认证 TinyGo producer 信息，比较结果保留 copied bool / 1 byte / true 或 false。未知语言仍走既有 shared numeric fallback。新增语言分类只选择表达式规则，不授予 frame、memory、pointer、VM control 权限。

Rust / Go / Zig 的逻辑操作数必须为布尔；类型预检查先验证两侧，即使右侧不会执行，也拒绝未知类型、数值 truth conversion 和非法转换。实际短路仍不读取或执行跳过的右值；类型 DATA 的 bits 保持0，实际读取类型必须与声明匹配。Rust 支持 primitive bool 的 eager & / | / ^、bool 排序，以及整数 ! 的逐位取反并保留原宽度/符号；Go / Zig 的 ! 只接受布尔。Go 拒绝数值与布尔互转；Rust bool 到整数的有限转换保留，bool 到浮点及数值到 bool 拒绝；Zig 有界 @as 路径保留，通用 cast 路径拒绝。

Rust 位运算比比较更强，Go 移位、& / &^ 与乘除同级，| / ^ 与加减同级。程序现在用 fast_io::string 保存最多4096字节的原表达式，在真实语言规则下重新解析，保留128节点、32层及4096次类型查询预算。调用方改写原输入缓冲区不会改变程序。C/C++/Objective-C/Objective-C++ 的既有解析和结果规则保持。DAP 增加 Zig and / or 的有限语法与关键词边界；关键词不决定帧语言，其他帧仍由 backend 拒绝该语法。

| 表达式 | 结果 | 验证路由 |
|---|---|---|
| Rust `object.seed > 4` | bool / true | 原 producer 的真实停点，三种 DAP 入口 |
| Rust `false & true == false` | bool / true | 原始类型组件及真实 DAP |
| Rust `!object.seed`（seed=5） | 有符号32位整数 / -6 | 原始类型组件及真实 DAP |
| TinyGo `box.Array[0] == 1` | bool / true | 原 TinyGo O0 停点，三种 DAP 入口 |
| Go `1 + 2 << 3 == 17` | bool / true | 原始类型组件及 TinyGo 真实 DAP |
| Zig `(a > b) and flag` | bool / true | 组件；本轮未验证真实 Zig producer 帧 |

结果继续保持 readOnly、variablesReference=0，未增加 evaluateName / memoryReference、写入或控制权限。所有新增 C++ 诊断使用 fast_io print，保存表达式使用 fast_io::string，没有新增文件系统 I/O。

每仓库组件新增测试通过721385次检查，涵盖两个 guest address width、三类语言、同宽 signed32 随机比较的独立 C++ oracle、同类 f64 的 ±0、有限边界、次正规数、±Inf/NaN、逻辑预检查、短路、转换拒绝、Rust取反和优先级。另有八组条件、标量、属性、转换、Zig preflight、DAP表达式旧回归；两仓各434项冻结 DAP 单测通过。次数不是功能数或 native 等价比例。

新 Linux x86_64 full 产品基于原R41冻结源码，只覆盖本轮 source_scalar_expression.h 及两个组件测试路径；两仓三处 product 翻译单元全部重新编译并重新链接，实际依赖和所有输入前后SHA一致。单独冻结本轮 DAP adapter 和真实测试脚本。外部 LLVM23 provider 仍为历史固定版本，未资格化 latest ROS provider 或当前整个脏工作区。

Rust 复用原正式 producer 的 Wasm32 DWARF4/5 模块，TinyGo 复用原 O0 DWARF 模块，源文件和 Wasm bytes 未重写。每个完整 Rust 会话三入口检查66次布尔结果/类型、9次整数控制、81次 tuple selector、四次22行对象树、子对象分页及真实单步后的7个旧引用拒绝；每个 TinyGo 会话检查51次布尔结果和90次原数组 len/cap，13次新增拒绝及原树、文本、nil数组和引用失效回归。预期谓词使用原停点已核验值及语言文档规则；组件随机比较 oracle 来自独立 C++ 编译结果，不声称为新的各语言编译器类型推断 oracle。原 guest tuple/array 自检、guest OS wait、broker cleanup、adapter实际 EOF/returncode 全部为0，原协议逐成员SHA/fsync可恢复。

| 仓库 | 最新版本实际持续 DAP 秒数 | 完整会话 |
|---|---|---|
| uwvm2 | 612.235 | 37 |
| uwvm2-ros | 614.434 | 39 |
| TinyGo / uwvm2 | 601.208 | 86 |
| TinyGo / uwvm2-ros | 602.215 | 86 |

Rust与TinyGo每仓分别至少600秒；Rust完整覆盖两种DWARF × instruction/unwind 四个组合，TinyGo覆盖instruction/unwind。最终真实 Rust 会话84个 / 布尔比较5544次 / 整数控制756次；TinyGo会话176个 / 布尔比较8976次 / builtin回归15840次。另有136个C系、枚举、Rust变体和整数真实会话回归。本轮旧冻结版本的成功会话和持续测试独立留存，不重复计入最终统计。

最新组件在两个实际 Linux QEMU profile x86_64 / ppc64、两仓共4个目标执行通过，每目标721385次检查，ELF machine/bits/byte order 已核验，输出与已固定的原生x86_64完全一致，包括两种模拟guest地址宽度。本轮原provider根其余14个SDK/链接器路由缺失，完整缺失路径保存在机器报告；这14个架构及 full QEMU VM/JIT/DAP 均未资格化。

所有功能编译、组件、VM、DAP、QEMU和单测均在SSH Linux原64GiB/swap0 cgroup、birth/PIDFD监督内进行。原 arena free floor 9288400896 bytes、host 25GiB reserve 保留；组件1GiB owned RSS / 64MiB output / 8MiB file / 1MiB log，full构建16GiB RSS / 2GiB output分别受控。OOM计数未增加，owned后代均退役/reap，keeper和peer保留。最终31个guard jobs全部通过。原参数启动错误、运算符表编译错误和缺失SDK和TinyGo持续包装器输出父目录遗漏的预启动错误均原样保留；旧header的预期SIGILL基线证明原Rust谓词类别错误。它们不计新版本功能通过或VM故障。

证据包30138904 bytes / 3913 members，SHA256 `51e335611944fa15248e7f4bf8fe51fb7fa42cab10c8067a5c3f51dd93b84ada`。逐成员流式SHA及fsync验证后，只删除本轮10个已归档冗余上传tar，共12124160 bytes；不复制大产品、object或工具链。所有测试cuts和原产品仍保留。

原88项状态计数保持14 implemented / 39 partial / 33 missing / 1 separate / 1 prohibited。本轮仅资格化上述有限primitive类型、结果类别、操作和真实停点，仍未完成完整 native 语言解析、literal coercion、混合类型/溢出规则、named Boolean类型、重载运算符、完整类型推断、标准Go运行时、Zig live producer、AssemblyScript完整表达式、IDE GUI及原生GDB体验。ROS真实会话使用llvm-jit full；uwvm-int full 的完整语言层会话本轮未增补。ASM继续只允许Wasm生成guest上下文，禁止通过asmdbg调试VM自身。

规则依据：[Rust operators](https://doc.rust-lang.org/reference/expressions/operator-expr.html)、[Go operators](https://go.dev/ref/spec#Operators)、[Zig operators](https://ziglang.org/documentation/0.14.1/#Operators)。

机器证据：[qualification](debug_native_predicate_qualification_20261008.json)。
