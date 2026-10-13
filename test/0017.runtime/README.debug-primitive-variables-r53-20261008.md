# R53 — 原生风格整数/Boolean 显示与 O1 DWARF location

两个仓库同步修复了 Source variables 仍带有 i8=/u64=/bool= console 前缀的问题。Source variables、Watch、hover、variables context 使用经过范围检查的显示值，并保留真实编译器类型。例：s8_min = -128 (signed char)、u64_max = 18446744073709551615 (unsigned long long)、enabled = true (_Bool/bool)。原始 console 载体另行保留；显示不会增加 memoryReference、evaluateName、可展开引用或写入权限。

开始完整矩阵后，发现并修复了独立的 C++ 运行时问题：O1 下 20 个整数参数正常，但两个 Boolean 参数的真实 DWARF location 被拒绝。DWARF4 使用 DW_OP_WASM_location; DW_OP_constu 1; DW_OP_and; DW_OP_stack_value；DWARF5 使用两个 DW_OP_convert 后再 stack_value。旧完整产品在 uwvm2、ROS 上均重现了“location needs unsupported evaluation or memory access”，失败报文与原始 Wasm 均保留。

新增求值只接受最多四步、作用于已初始化且已复制的 i32/i64 local 的整数掩码或 unsigned conversion。转换引用必须指向当前 CU 内准确的 DW_TAG_base_type，检查 encoding、byte_size、可选 bit_size 与位偏移；没有名称猜测。真实 Clang21 产物中，DW_ATE_unsigned_1 和 DW_ATE_unsigned_8 两个类型的 DW_AT_byte_size 都是 1，均没有 DW_AT_bit_size，因此按 8 位属性求值。DWARF4 的 AND 则明确取最低位。一般表达式栈、分支、host register、解引用、函数调用、frame-base 转换与 guest 内存权限均未加入。

原来的 22 个 O0 参数的直接 Watch 已经可用，本轮保留了正对照，没有将它们记为新实现的求值功能。object 整型 primitive 的格式化分支增加了限定类型、位宽和范围检查；这组真实参数主要经过 source-stop numeric 路径，新的 object 整型分支只获得分离格式化单测覆盖，不据此宣称新的语言 ABI 已完成。

验证使用原始 C、C++、Objective-C、Objective-C++ 前端产物：Wasm32/64 × DWARF4/5 × O0/O1，各仓库 32 个 profile；Wasm validator 和 DWARF verifier 均通过，未重写或剥离调试信息。每个实际会话核对 22 个原始参数、Source variables/分页、66 次 Watch/hover/variables 求值；guest 逐参数自检，真实 Wasm 单步后旧 frame/scope/Watch 必须拒绝。两种实际调用栈策略均覆盖。

测试结果：

- 两仓库各通过 455 项 DAP 单测，并通过 5 个 C++ metadata/copy 程序的实际编译与执行；新产品各三个 TU 完整重编译并重新链接，实际依赖与前后输入 SHA 均闭合。
- 新 primitive DAP 共 1156 个合格会话，核对 25432 个原始参数、76296 次 Watch/hover/variables 求值。
- uwvm2 持续 621.50 秒 / 512 个会话；ROS 持续 621.67 秒 / 512 个会话。两种策略交替覆盖全部 32 个真实 producer profile。
- Rust/TinyGo/C 原有回归 76 个会话；Zig Debug/ReleaseSafe × Wasm32/64 × 两种策略 16 个；原始浮点矩阵 128 个会话、9216 次直接求值，全部通过。
- 16 架构 × 两仓库 = 32 个 QEMU target ELF copied-value 数据层用例均通过，与实际 native x86_64 输出一致。架构包括 x86_64、aarch64、i686、riscv64、ppc64/ppc64le/ppc32、mips64/mips64el/mips32/mips32el、sparc64、loongarch64、s390x、armhf、armel。
- 所有合格会话的原 guest OS wait、broker wait、实际 DAP EOF 均为 0；所有保留的 guard OOM 事件增量为 0。失败与过渡阶段不计入通过数。
- 保留原始证据 201074337 字节；Linux 压缩归档 20441568 字节、9410 个文件，逐成员 SHA 核验并 fsync。SDK、完整 VM 和全量产品对象文件不复制进归档。
- 归档：/home/macromodel/Documents/uwvm3-implementation/retained-primitive-variables-r53-final/primitive-variables-r53-evidence.tar.xz。SHA256: 727c896cbe1a0ec468fc9a01498f45b1bbf277bf380b3fa125144df05a6756a8。本机只保留较小报告、资格和归档元数据。

归档逐成员核验后，清理了本轮 72 个自有可生成对象/过渡重复 ELF，共 356075544 字节（约 339.6 MiB）。最终 VM、原始 Wasm、最终组件/QEMU ELF、SDK、源码和证据均保留。清理记录单独保存。

新增 C++ metadata/copy 检查涵盖掩码、明确位宽、类型属性与名称不一致、多个 CU 的同一相对 offset、错误/中间/越界 DIE、未初始化 local、浮点拒绝、frame-base/global/deref 拒绝。旧 copied-value 测试改为按字段赋值，避免依赖过时的 type_record 聚合字段顺序。Linux credential-window 单测在长 TMPDIR 下使用短私有 socket 路径，认证窗口和退休检查保持原断言。

所有编译器、validator、DWARF、VM/DAP、QEMU 执行均在 SSH Linux 的原始 64GiB/swap0 cgroup 中完成，监督器使用原始 boot/init birth 与 PIDFD 追踪。组件独立 RSS/输出限额、完整编译限额、共享压力中止、宿主磁盘 25GiB 保留与原进程树回收仍执行。本机只进行文件编辑、AST/哈希核对和保存小型报告。

资格边界：完整新产品基于 R51 冻结源切片，只叠加本轮三份 C++ header 的自有修复并重新编译全部三个产品 TU。使用单独固定的历史 LLVM23 developer provider；产品 SHA、当前冻结 manifest source ID 和继承的 embedded build label 分别记录。并发 agent 的本机源修改保留在资格切片之外，不能将本轮结果推广到整个最新工作区、最新 ROS provider 或 uwvm-int full。

16 架构 QEMU 只验证实际 ELF copied-value 数据层，包含大小端与 32/64 位 ABI，并与实际 x86_64 native 输出比较；没有重新证明这些架构的完整 VM/JIT/DAP。Objective-C/Objective-C++ 仅证明原始 primitive frontend 元数据，不包含动态 runtime/Foundation。标准 Go、AssemblyScript 的全部 primitive ABI、C++ RTTI/STL、Rust trait/runtime、goroutine 上下文、源码赋值/watchpoint、guest 函数调用、返回值捕获等仍未完成。转换到 generic、signed/float conversion、带转换的 piece 表达式及一般 DWARF 算术也不属于本次有限实现。

能力清单仍为 88 项，状态分布保持 14 implemented / 39 partial / 33 missing / 1 separate-level / 1 prohibited-by-scope。ASM 调试只能处于 Wasm guest 产生的上下文中，不能调试 VM 本身。
