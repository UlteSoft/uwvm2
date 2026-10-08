uwvm2 与 uwvm2-ros 已同步补上有限只读数值表达式的 `?:`，并修复其 `sizeof` 元数据类型查询。SSH Linux 重启后已恢复原 64GiB cgroup；两个完整产品均从当前冻结源码和重新构建的真实 ROS.11 LLVM SDK 编译。96 组 QEMU 组件、468 项 DAP 回归、20 个真实条件表达式用例及 30 分钟连续实测通过。完整原生语言调试仍未完成。

语法使用 C 风格的低优先级和右结合规则，支持嵌套条件。结果类型先从两个分支的字面量或当前真实源码帧的 DWARF 类型推导，再只求值被选中的分支。类型查询不读取变量值，不执行除法等数值运算，也不解引用实际指针。所选分支的实际类型必须与查询结果一致，否则不发布值。

```text
print-frame THREAD STOP FRAME count > 0 ? count : 0
print-frame THREAD STOP FRAME 1 ? 42 : 1 / 0
print-frame THREAD STOP FRAME decimal32 > 0 ? decimal32 : decimal64
print-frame THREAD STOP FRAME 1 ? -1 : 0u
```

THREAD、STOP、FRAME 应取自当前真实停点。第二例应返回 42；第三例即使选中 f32，也按两个分支的公共 f64 类型返回；第四例返回 unsigned 32 位的 4294967295。未选分支含已有类型、但值不可用或空指针的变量时，类型信息仍可能足够；不存在的变量、缺失类型、指针结果、聚合结果和位域结果则拒绝。

无可读线性内存的已捕获局部值路径和运行时内存事务路径都使用当前真实帧的独立类型查询。仅选中的 guest-memory 操作数可以进入完整事务重试。所选和未选 `sizeof(表达式)` 都使用当前帧的类型大小元数据，可以接受完整聚合体、指针和空指针目标；不会为求大小读取这些值。位域、缺失或不完整类型明确拒绝，位域拒绝也适用于已有直接 `sizeof` 路径。

DAP watch、hover、variables 保留完整表达式，在向 broker 发送命令前检查两个分支的语法。任一分支中的调用、赋值、自增、自减、裸地址解引用、指针转换和逗号表达式都拒绝。DAP 本身仅做语法和回复身份检查，值由 C++ 控制器在真实停点求得。沿用 256 可打印 ASCII 字符、128 节点和 32 层嵌套限制；新增类型遍历预算为 4096 次节点访问。

这实现的是有限数值副本，不是完整 C++ 条件表达式的类型与值类别系统。相同窄整数/bool 分支、enum、pointer/class、重载、glvalue、void/throw 等仍不支持。[C++ 条件运算符规范](https://eel.is/c++draft/expr.cond)还定义了这些额外规则；[GDB C/C++ 运算符文档](https://sourceware.org/gdb/current/onlinedocs/gdb.html/C-Operators.html)包含更完整的语言求值操作。Rust、Go 没有原生 `?:`，不得把共享表达式语法视为这些语言的原生体验。

新增回归包括数值/公共类型、嵌套和优先级、未选除零与溢出、不可用值、元数据不一致、实际 production 类型遍历的未选指针、guest32/64、预算及不安全语法。共享 DAP/C++ 语料已加入条件表达式。QEMU runner 注册了新组件，覆盖已有 16 Linux profiles；真实语言 runner 的 `--conditional-subset` 覆盖 C、C++、Objective-C 当前停点、公共类型及恢复后的旧停点拒绝。本轮已执行 16 profiles × 2 仓库 × 3 组件，共 96 组；这是实际目标 ELF/QEMU 的组件语义验证。非 x86_64 的完整 VM/JIT 调试体验仍未验证。

完整清单目前是 14 implemented、38 partial、34 missing、1 separate_level、1 prohibited_by_scope，共 88 个有限类别。`conditional_expr` 标为 partial，有限功能已取得本轮冻结产品资格；完整原生类别仍为 partial。其余历史测试记录保持原冻结源码对应关系。ASM 仍只允许 Wasm 生成代码和其真实激活上下文；本次修改不扩大 VM、宿主栈或宿主内存调试权限。

本轮源码冻结后，其他 agent 又更新了 console/session、native 与文档相关文件，均保留。本次 PASS 对应两个精确冻结源码，不能代表这些后续改动。直接涉及的解析器、控制器、语料和测试文件在两仓库一致，且与已测试的冻结输入一致。完整恢复、失败尝试、真实停点与长测记录见 [重启后的验证记录](debug_linux_reboot_sizeof_qualification_20261005.json)；旧尝试与归档仍保留在 [条件表达式记录](debug_conditional_expression_qualification_20261005.json)，清单见 [语言能力清单](language_debug_capabilities_20261004.json)。

本任务旧构建的 19 个已结束产物已在本机 `/tmp/uwvm2-conditional-expression-20261005/retired-old-products-20261005.tar.gz` 完整归档并逐项核对 SHA-256，随后退休了原远端输出文件；旧源码、日志、构建记录和真实 ROS.11 SDK 输入保留。包按 `/tmp/uwvm2-language-fix-20261004/` 的相对路径保存，必要时可恢复历史二进制。归档 SHA-256 为 `c24b5b76fc8d69544e1d2e66b04d7f3f9bc74b5cdaa1d4a780020f638787e30b`。
