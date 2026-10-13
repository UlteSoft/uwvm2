# R52 — 浮点 Source variables 与直接 Watch 显示

两仓库同步修复了真实浮点参数导致整个 DAP Source variables 列表被拒绝的问题。VM 的 fast_io console 正确输出 `f32/f64 bits=0x… value=…`，旧 adapter 却只接受 bits 字段。新 parser 检查限定长度的十进制语法，并以精确有理数、IEEE ties-to-even 单次舍入验证它与实际复制的位模式一致；不会先转 host double 再转 float。使用 Decimal.copy_abs() 保留完整精度，避免默认 Decimal 算术上下文提前舍入。

Source variables、Watch、hover 和 variables context 现在显示浮点值及真实 producer 类型。真实的负零等根参数有时由 controller 返回 `object: type=float/double`，该直接 primitive 路径也得到修复。旧 bits-only 回复继续作为不透明显示；compound 根报文保留原显示。显示不产生 memoryReference、evaluateName、扩展引用或写入权限；实际停点和 frame 的前后检查保留，真实 Wasm 单步后旧 frame、scope、Watch 请求必须拒绝。

示例：

```text
Source variables / Watch f_minus_zero       -> -0       (float)
Source variables / Watch f_subnormal        -> 1e-45    (float)
Source variables / Watch d_subnormal        -> 5e-324   (double)
Source variables / Watch f_infinity         -> inf      (float)
Source variables / Watch d_negative_nan     -> -nan     (double)
```

NaN 十进制文本只证明 NaN 类别，不能恢复 sign 或 payload。原始 console 位模式及 guest 的逐参数 bit self-check 才提供这些证据，均保留。

测试矩阵：

| 真实前端 | 目标与元数据 | 参数与运行验证 |
| --- | --- | --- |
| C、C++、Objective-C、Objective-C++ | Wasm32/64 × DWARF4/5 × O0/O1，每仓库 32 个原始编译产物 | 各 12 个 f32/f64 参数；正负有限值、正负零、最大值、最小 normal、正负 subnormal、正负 Inf、正负 NaN |
| 相同 32 个 profile | instruction 与 unwind 两种调用栈策略 | 真实 -Rdbg prepared 启动、源码断点和 frame；变量分页；每会话 72 个直接求值；单步退休；原 guest 自检退出 |
| Rust、TinyGo、C-family 与 Zig 现有产物 | 两种调用栈策略 | 原有 finite Boolean、对象、数组、参数、稀疏 pieces 和数值控制回归 |

Objective-C/Objective-C++ 的新样本验证 primitive 前端元数据，不继承 ObjC runtime 或动态对象语义资格。本轮没有新增 C++ runtime 改动；既有 fast_io I/O 路径保留。

修复前的两个仓库都在真实停点正确记录 24 个参数，但旧 DAP 返回 `unsupported or malformed source-variable value`。这两次失败及其非零清理状态单独保留，不计入通过数。早期测试驱动路径类型错误、校验器执行放行遗漏，以及首次浮点 object 显示失败也保留。

原 Clang21/lld21 产物通过 Wasm validator，但部分 C++ DWARF5 字符串表偏移被 LLVM23 DWARF verifier 拒绝。使用 lld21 -O0 重新链接真实编译对象后，每仓库 32 个原始 Wasm 与 DWARF 校验全部通过；不修改 Wasm 字节。原始失败样本、对象、编译和校验日志均保留。

测试结果：

- 两仓库各通过 448 项 DAP 单测，含固定 IEEE 边界、单次舍入 midpoint/tie、Decimal 上下文精度、独立随机 IEEE round-trip、矛盾 bit/decimal、旧协议和停点退休检查。
- 真实新浮点 DAP 会话共 1058 个；各会话验证 24 个原始参数以及 72 个直接 evaluate，共 25392 个参数观察、76176 个 Watch/hover/variables 求值。
- uwvm2 持续 612.85 秒 / 448 个会话；ROS 持续 632.94 秒 / 480 个会话。两种策略交替覆盖全部 32 个 producer profile。
- 原有 Rust/TinyGo/C-family 回归 76 个会话；Zig Debug/ReleaseSafe × Wasm32/64 × 两种策略另 16 个会话，全部通过。
- 所有合格会话的原 guest OS wait、broker wait、实际 DAP EOF 均为 0；所有保留的 guard OOM 事件增量为 0。失败记录不计入这些通过数。
- 原始保留文件 105934133 字节；压缩归档 10688840 字节、7174 个文件，逐成员 SHA 核验并 fsync。SHA256: 91873c2e6ac456f9eff1ed5f1e03b87d08b231a671add28ae0c0d27b8abc079e。
- Linux 归档：/home/macromodel/Documents/uwvm3-implementation/retained-float-variables-r52-final/float-variables-r52-evidence.tar.xz。SDK 和完整 VM 不重复打包；本机只保存较小报告、资格与归档元数据。

新测试只覆盖固定的 R51 完整 VM 产品与历史 LLVM23 provider，产品身份由 binary SHA 固定。R51 的全部 TU 曾重新编译；本轮只修复 Python adapter，因此复用那些产品，并重新核验哈希。R51 manifest source_id 与继承的内嵌 build-source 标签分别保留。最新整个 dirty workspace、其他 agent 的修改、最新 ROS provider、uwvm-int full、完整跨架构 VM/JIT/DAP 和完整 native 语言调试体验不由本轮资格化。两个本机 adapter 的本轮三个函数逐字匹配已测试切片，文件整体的其他并发修改独立保留。

标准 Go、AssemblyScript 及其他语言的全部浮点 ABI 不继承本轮 C-family 测试资格。Rust/TinyGo/Zig 回归验证原有有限路径，不意味着这些语言全部特性完成。原 88 项统计状态继续保留：14 implemented、39 partial、33 missing、1 separate_level、1 prohibited_by_scope。

完整证据及失败记录见 debug_float_variables_qualification_20261008.json。Linux 归档留在测试机；不重复归档 SDK 或完整 VM 二进制。所有实际编译、validator、DWARF verifier、VM、DAP 测试都在原 64 GiB / swap 0 cgroup，经原进程 birth/PIDFD 守护执行。每项 bounded component 限制自身 aggregate RSS 1 GiB、输出 64 MiB，并保留主机至少 25 GiB；共享 cgroup 不能作为性能基准。持续测试每会话间隔 250 ms，并对协议逐成员压缩、SHA 核验和 fsync，以限制目录占用。

归档 SHA、逐成员核验和 fsync 完成后，仅移除两个本轮 producer 目录的 128 个自有中间 .o，释放 697128 字节；原始对象在证据归档及保留切片中仍可恢复，Wasm、源码、SDK 和其他 agent 文件保留。
