# Zig 直接负常量调试求值（2026-10-06）

uwvm2 与 uwvm2-ros 同步补全有限 `@as` 求值器的一层直接负常量。
支持 unsuffixed 整数/浮点字面量，以及既有有限 ASCII 字符字面量；
整数字面量幅值仍最多 64 位。这不是完整 Zig CTFE 求值器。

## 行为

| 查询例子 | 当前结果 |
| --- | --- |
| `@as(i8, -128)` | i8 -128，bits=128 |
| `@as(i64, -9223372036854775808)` | i64 最小值，bits=9223372036854775808 |
| `@as(i64, -0xffffffff)` | i64 -4294967295，避免共享 C 字面量在 32 位无符号取负时变成 1 |
| `@as(f32, -0.0)` | f32 负零，bits=2147483648 |
| `@as(i8, -'A')` | i8 -65，bits=191 |
| `@as(u64, -0)` | u64 0 |
| `@as(u8, -1)`、`@as(i8, -129)` | arithmetic；无值发布 |
| `1 ? 7 : @as(i8, -129)` | arithmetic；未选中的转换也检查常量范围 |
| `@as(i64, -value)`、`@as(i64, -(1+2))` | unsupported；不读取变量 |
| `@as(i64, +1)` | unsupported；没有把一元加号作为 Zig 语法加入 |

控制台仍使用现有已鉴权接口，例如 `print THREAD STOP @as(i8, -128)`
或 `print-frame THREAD STOP FRAME @as(f32, -0.0)`。测试中的固定 thread/stop/frame
仅验证命令语法保留，不代表真实 VM 颁发的停止令牌。
这里的 `?:` 是调试器共享的有限 C 风格语法，不是 Zig `if`。

实现从拥有的字面量语法取得幅值，验证目的类型范围后转换。
最小有符号整数使用无符号位运算构造，避免 host signed negation 溢出。
取负的十六进制/二进制/八进制字面量不会先按共享 C 整数提升规则截断。
常量路径不调用值或声明类型回调；已声明变量转换保留上轮的提前类型证明和
声明/读取值一致性检查。逻辑短路仍不检查未执行的值/类型。
普通 C 风格一元表达式在 `@as` 外保留旧行为。

参照 Zig 的 [取负运算](https://ziglang.org/documentation/0.15.2/#Table-of-Operators)、
[常量可表示性](https://ziglang.org/documentation/0.15.2/#Type-Coercion-Compile-Time-Known-Numbers)
和 [code point 的 comptime_int 类型](https://ziglang.org/documentation/0.15.2/#String-Literals-and-Unicode-Code-Point-Literals)。
浮点仍是有限 f32/f64、double-backed 字面量模型；没有声称实现 Zig 原生
comptime_float 的 f128 精度或所有浮点/整数常量转换。

## 修复与接入

- 生产代码：`src/uwvm2/uwvm/debugger/source_scalar_expression.h`。
- 新回归：`test/0017.runtime/debug_source_zig_negative.cc`。
- 公共 QEMU registry 加入新组件；DAP 有限表达式 corpus 加入 15 条结果用例。
- 两仓库的生产代码、fixture、registry、corpus、语言用法文档逐字节一致。
- 源头文件 SHA-256：`7e520c7ced52b91e3e0dacd35f496226bd4a7a2a126d77fce23fa8af4a409fa3` →
  `3cb0a8adfb11cafb1914f0eaa4832110be4e8f724abdcc54df38ccb3eb332872`。
- C++ 新测试的字符串构造、argv 读取和诊断/输出采用 fast_io。
  生产路径没有新增 IO、host 文件访问或运行时内存读权限。
- ROS 模式限制及 ASM 只能访问 Wasm 产生上下文的边界没有修改。

## 验证

所有编译、运行、Python DAP 检查和 QEMU 执行都在 SSH linux 原有
64 GiB、swap=0 的 cgroup 里完成。本机仅编辑、阅读、比对与保存证据。

两个仓库各 21 个 Linux 原生阶段通过：9 次编译、8 个当前 fixture 执行、
4 个旧 header 预期失败对照。旧 header 分别拒绝 i8 最小值、较大负十六进制
整数、浮点负零，且不能按 arithmetic 对未选中的越界负常量分类；
新 fixture 在两仓库中分别复现旧失败并通过修复实现。

新组件每次 58,095 条断言，包括 Wasm32/64 类型上下文、10 种整数目标、
4 种进制、58 个边界/固定种子样本、0..256 的全部 i8 负幅值，
根/已选/未选分支、嵌套 widening、负零与 f32 subnormal、有限 ASCII 常量、
拒绝的 CTFE 形状和 C 风格旧行为。原有 preflight、typeproof、Zig coercion、
整数/浮点、字符、条件表达式和 DAP bridge 通过。
字符 C++/Python DAP 差分各 11,802 条，合计 23,604 条。
实际 DAP 求值入口 `validate_source_evaluation_expression` 的 178 条语法接受用例
和 60 条拒绝用例在两仓库都通过；该入口用于实际 evaluate 请求。
没有修改 DAP 源码。

公共 registry 在 16 个 Linux 架构、两个仓库执行
`debug_source_zig_negative`、`debug_source_zig_preflight` 和
`debug_source_dap_expression`，32 个组合、96 个组件结果全部通过。
新组件累计 1,859,040 条断言，preflight 308,864 条，合计 2,167,904 条；
另有 5,696 条跨架构 DAP/C++ 结果行。
所有 QEMU 输出与固定 x86_64 原生对应结果逐字节一致。
这仅证明这些有限组件的结果一致，不证明完整 native 体验、性能或真实各架构硬件。

| Linux 架构 | uwvm2 | uwvm2-ros |
| --- | --- | --- |
| x86_64 | PASS | PASS |
| aarch64 | PASS | PASS |
| i686 | PASS | PASS |
| riscv64 | PASS | PASS |
| ppc64 | PASS | PASS |
| ppc64le | PASS | PASS |
| ppc32 | PASS | PASS |
| mips64 | PASS | PASS |
| mips64el | PASS | PASS |
| mips32 | PASS | PASS |
| mips32el | PASS | PASS |
| sparc64 | PASS | PASS |
| loongarch64 | PASS | PASS |
| s390x | PASS | PASS |
| armhf | PASS | PASS |
| armel | PASS | PASS |

完整冻结生产源代码 source_id：

- uwvm2: `sha256:ba80c6cd10524a0d27d958611ad356e3383a0346f7de6b67675e636cf769bca3`
- uwvm2-ros: `sha256:ed9aefe94984a3d68e3a3542a76cce2ff9e98182796188adcf09dd51c6feace2`

资格记录核验完整 source cut、fixture、实际依赖、编译器/链接器/QEMU、
已安装链接提供者、ELF 类型和大小端、原始日志与监控回执的 SHA-256。
链接提供者按组合在运行前后校验。34 个成功的完整回归监控阶段累计 1668.844 秒；
最高自有进程树 RSS 747950080 字节。
这包含编译与监控，不是性能基准。

另保存 4 个补充诊断监控，以及 4 次已修正的前置失败：
a1 漏准备旧对照 header；a2 测试中 FastIO view 未显式构造；
a3 测试错误拒绝合法的已有 ASCII code point 常量；
a4 测试误调用仅支持 selector 的 DAP API。
前三次修正仅影响准备/测试/说明；a4 改用实际求值入口。
生产负常量实现没有因这些测试修正而变动。
原始失败脚本、日志、回执与修正说明保留。合格结果使用 a4 冻结源码、a6 原生 runner，以及 a6/a8 跨架构 runner/结果；a8 只更换私有输出目录，实际 registry、用例与冻结源码不变。
启动 cross a6 controller 时，标签里的下划线被 guard 在任何编译/运行前拒绝；
cross a7 只把标签规范为允许的字符，目标 profile 参数、runner 哈希和源码均不变。

跨架构首批还保存两次资源中断：ROS MIPS64EL 在共享 cgroup 距上限不足
1 GiB 时停止；ROS SPARC64 在自有总输出超过原 64 MiB 预算时停止。
这两条记录没有算作通过。原始监控回执证明只退休并回收本轮自有进程，
memory.events 前后不变。随后对已结束的自有输出做完整压缩归档，
逐成员校验持久化副本，再退休 69 个旧 cross ELF，释放 22,048,704 字节。
原始 ELF 可按 retirement manifest 从持久化 archive 恢复，其他日志、源码、
native 二进制、SDK 和其他任务文件保留。重跑期间，对已结束的前四个组合采用同样流程再退休 12 个 ELF（5,516,792 字节），保留了封存的终止回执。两批共 81 个 ELF 均可恢复。最终核验再次读取持久化归档中的
原始 ELF 字节，检查哈希、长度、machine/位数和大小端。
22 个首批成功组合保留，另 10 个组合（两条中断重跑和八条未启动项）均通过。
资源门槛和原 64 MiB 输出预算没有放宽。

所有自有进程 PIDFD 已退休、根进程已回收；memory.events 前后相同，
共享 cgroup 中的 init/其他任务保留。原 DATA guard 的 6 GiB admission、
1 GiB own RSS、单文件/日志/总输出、实际文件系统保留、CPU、身份与血缘
检查均保留；没有降低完整 VM/JIT/SDK/producer 的原资源门槛。

## 仍未完成的范围

- typed 8-bit bool/narrow carrier 区分、复合 CTFE、变量 comptime 证明、嵌套负号、
  Zig 原生 if、完整 Unicode 与 f128 comptime_float 模型仍需进一步实现。
- 没有新运行 Zig producer 或完整当前 VM/真实 stopped-frame 事务，
  没有重新证明八种语言完整 native 级体验。
- 热更新、检查点、ASM、WASIp1 接口本轮未改动。

原始冻结源码和运行记录在 SSH linux
`/run/user/1000/uwvm2-zig-negative-20261006-a4`。
公共 case 参数为 `--cases debug_source_zig_negative,debug_source_zig_preflight,debug_source_dap_expression`，
编译和运行仍必须使用对应 cgroup supervisor。
完整归档和持久化副本的最终状态记录在资格 JSON 的 `evidence_backup`；
归档后的最终报告/配对核验作为 sidecar 保存，原归档哈希保持不变。

## 实际持久化结果

完整归档含 7,356 个逐成员校验的条目，包括两套冻结源码、原始脚本/日志/回执，
以及已退休的 81 个 ELF 的恢复字节。已分别验证 SSH linux 持久化目录和本机副本。
归档大小 20,664,272 字节，SHA-256：`d7948d8fb538c6515b23b0b9ae83ad48fa817002c002409bcb32e9621ae6bada`。
本机归档：`/Users/liyinan/.codex/artifacts/uwvm2-zig-negative-evidence-20261006-a1/zig-negative-evidence-a1.tar.xz`。
SSH linux 归档：`/home/macromodel/Documents/uwvm3-implementation/uwvm2-zig-negative-evidence-20261006-a1/zig-negative-evidence-a1.tar.xz`。
最终报告、资格 JSON 与配对核验另存为 sidecar；完整归档哈希不再改写。
