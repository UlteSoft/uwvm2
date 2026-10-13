本轮继续审阅并在 uwvm2、uwvm2-ros 同步实现了 DAP 的有限数值表达式支持，修复显式源码帧参数、实际格式化输出兼容性、自增/自减误解析和测试收集器退出码。完整原生语言调试仍未完成；能力清单为 14 implemented、36 partial、36 missing、1 separate_level、1 prohibited_by_scope，共 88 个有限类别，不能视为原生语言支持百分比。

DAP 的 watch/hover/variables 现在能传送已有 C++ 引擎的有限整数、浮点、比较、位运算、短路、sizeof、基础数值转换和源码 selector。保留 Rust as、多词类型和字符字面量内的空格；按 256 个可打印 ASCII 字符、128 个节点和有限嵌套检查。函数调用、赋值、裸地址解引用、指针转换和连续 ++/-- 会拒绝，短路分支也必须符合语法。C++ 同步修复了把 value++1、--value 等当作普通二元/一元运算的错误。

新增 print-frame THREAD STOP FRAME EXPR、ptype-frame THREAD STOP FRAME EXPR，解决旧 print 命令在 *、负号等前缀下的帧参数歧义。DAP 使用显式命令，需要匹配的新 VM；补全与 help 已同步。ptype-frame 当前沿用 selector 类型查询，尚未增加任意数值表达式的类型推导。THREAD/STOP/FRAME 必须从真实当前停点和 source frame 取得，下面是占位符示例：

```text
print-frame THREAD STOP FRAME -shadow
print-frame THREAD STOP FRAME *p + 1
print-frame THREAD STOP FRAME pair.0 + 1
print-frame THREAD STOP FRAME @as(i64, value + 1)
ptype-frame THREAD STOP FRAME p.*
```

DAP 查询前后都检查当前真实停点及帧。回复同时识别数字值的 source-stop 包和 C++ 实际产生的 source-value stop=... name=... 包，后者必须有完整 source-value end。过期/重复身份、混合头、缺失或重复尾、空内容、越界和控制字节不能发布。对象和 guest pointer 保持复制的显示文本，不赋予宿主地址或 VM 内存权限。native frame 的 watch 仍走已有寄存器选择接口。

| 最新冻结 R15 的实际验证 | 两仓库结果 | 范围 |
| --- | --- | --- |
| DAP 回归 | 每仓库 225 PASS | 协议固定测试数据、停点/帧退休和错误拒绝 |
| 数值表达式及显式帧命令 | 16 Linux QEMU profiles × 2，32 PASS | 每组合 137 条：122 值结果、15 预期拒绝、81 条零 resolver 调用 |
| 跨架构输出比较 | 32 组合完全一致 | 实际目标 ELF，大小端及 32/64 位宿主；fixture guest ABI 为 32 位 |
| 真实 C++ 格式化器 | 10 个实际包通过、48 个篡改包拒绝 | production format_reply，QEMU/native 字节一致；固定 reply 数据 |
| 无异常编译及 UBSan | 8 个组件运行 PASS，8 个编译 PASS | 每仓库两配置、每配置两 C++ cases |
| 生成式整数性质测试 | 每仓库 10000 条，9020 条不同表达式 | 20000 次 DAP 语法检查、40000 次 C++ QEMU/native 结果检查 |
| 旧版本负对照 | 3 组 PASS | 旧 DAP 拒绝合法实际输出；旧解析器接受 value++1；当前收集器报告失败并退出 1 |
| ROS 真实 SDK 门 | 预期拒绝 PASS | 实际 .ros.10，当前要求 .ros.11；不构成完整 SDK 资格 |

16 profiles 为 x86_64、aarch64、i686、riscv64、ppc64 BE/LE、ppc32、mips64 BE/LE、mips32 BE/LE、sparc64、loongarch64、s390x、armhf、armel。生成测试使用独立整数 oracle，种子 20261005，验证有符号 32 位、非溢出、完整括号的共享数值语法。原生运行的是同一组件 ELF，不是 GDB/LLDB 或真实语言停点。

所有编译器、实际目标运行、QEMU 和单元测试都通过 SSH Linux 原 cgroup 的自有 PIDFD 保护器执行。64 GiB、swap0、原 cpuset、16 GiB 自有 RSS、1 GiB 共享余量中止及磁盘/日志/文件限制均保留。记录共 86 个有 receipt 的根尝试，其中 65 个正常完成；失败、压力中止和负对照均封存。每个通过批次的 OOM 计数未增加。共享计数从此前 60/12 变化为封存时 63/15，未将跨批次变化归因于本任务。全部已创建的本任务根/后代及封存进程已退休、回收。

普通版从冻结 R14/R15 新编译 main/runtime/host API 的实际编译均被原共享压力阈值中止；另一次 6 GiB 启动余量等待 120 秒未满足，未启动编译器。最新普通版完整二进制没有形成。ROS 实际 SDK 未满足 .ros.11。本轮两个仓库的完整最新 -Rdbg 产品仍未验证，组件结果不等同于真实 VM 停点、8 种语言 producer、LLVM-full 和全部目标运行的资格。

C、C++、Objective-C、Rust、Go/gc、TinyGo、Zig、AssemblyScript 仍在完整清单中。有限转换语法不等同于各语言的完整求值语义：共享算术优先级为 C 风格，Go 的 shifts/bitwise 分组、untyped constants 和任意大 shift 规则仍需单独实现，详见 [Go 官方规范](https://go.dev/ref/spec#Operator_precedence)。语言变量 watchpoint、反向执行、函数/方法调用、赋值、动态类型与容器、Go goroutine、完整 split DWARF 和多项 Zig/AssemblyScript 支持仍缺失或部分完成。条件断点保留其他 agent 的实现/资格注释，当前 source-only 观察仍为 partial。

ASM 继续只允许经过验证的 Wasm 生成代码和激活上下文，不能借此调试 VM、宿主栈或任意内存。上轮 x86_64 QEMU TF 的信号分类局限尚未解决，本轮未取得新的正向 ASM 停点资格。Wasm 3.0、WASIp1 全状态检查点与外部文件/管道/socket/I/O 的完整回滚也没有新增完整产品资格。

普通版 source ID 为 `sha256:16fa45a281f66090c25573af23942a59ff751f3f8f705c5e57ca4cacc1ce09e4`，ROS 为 `sha256:ee949375752ffcf85e695faae17b4eca9db30181d457a78023efbe3abc2f21d1`。冻结之后，其他 agent 修改了 source_dwarf_index.h、source_dwarf_types.h、source_type_declarators.h；内容已保留，未附加本轮资格。本轮直接受影响的 14 个代码/测试/说明文件在两仓库一致。

证据共 778 个文件，包含脚本、源码/工具/依赖哈希、目标 ELF、日志和保护器记录。本机包 `/tmp/uwvm2-dap-scalar-20261005/final-r15/evidence.tar.gz`，SHA-256 `fe270f60f61d1e07496d23bd00788a39cd986de9e10ca73c00863937ef5216d6`；已逐项核对包内输入哈希。精确记录见 [本轮资格](debug_dap_expression_qualification_20261005.json)，剩余能力见 [完整清单](language_debug_capabilities_20261004.json)，用法见 [表达式说明](README.debug-source-language-expressions.md)。
