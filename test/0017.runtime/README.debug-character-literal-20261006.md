# 字符字面量回归：2026-10-06

uwvm2 与 uwvm2-ros 已同步修复共享只读表达式解析器。旧代码使用会跳过空白的
consume 检查结束引号，可能把 `'a '` 算成 `'a'`、把 `'\n '` 算成 `'\n'`，
并接受裸单引号 `'''` 和直接写入的换行。修复后，结束引号必须紧接一个字符或现有
支持的转义；裸引号、原始 LF/CR 和额外字符均被拒绝。合法的 `' '`、`'\''`、
`'A' + '\n'` 分别仍得到 32、39、75。

这是既有有限 ASCII 数值语法的正确性修复。[C++ 工作草案](https://eel.is/c++draft/lex.ccon)
条件性支持多字符字面量；本调试器子集继续拒绝它们，也未实现 Unicode 或所有语言的
原生字符/rune 类型语义。生产补丁没有引入新的 I/O；C++ 回归用例中的字符串构造、
参数读取和输出使用 fast_io。

源码路径为 src/uwvm2/uwvm/debugger/source_scalar_expression.h。controller.h 的
source_type_locked 在值解析、调用帧选择或 guest-memory 路径前检查解析结果；
断点条件的配置路径也先检查完整表达式。这是源码检查，未据此声称完整 VM 打印
或断点会话已经重新验证。

## 实际测试结果

所有编译、ELF/QEMU 执行和 Python/DAP 检查都在 SSH linux 的原 64 GiB cgroup
内完成。每个命令独立准入，使用固定 E16 CPU 集合；资源不足时等待，不修改其他
任务的资源或终止其进程。

- 两仓库原生组件各通过 14,791 条断言，覆盖 Wasm32/64 数值上下文、93 个
  可打印非引号/反斜杠字符、现有六种转义、引号内空白、截断输入、短路语法、
  非 ASCII 拒绝、重复解析和失败时零值解析器调用。
- 两仓库均通过现有 scalar、conditional、DAP bridge 回归。
- 两仓库各用旧头文件独立复现空白、裸引号、裸换行三个问题；六次预期失败的
  日志、返回码和具体失败原因保留。当前源码对应检查通过。
- 每个目标运行 594 个合法黄金表达式、1,208 个拒绝表达式和 10,000 个独立计算的
  算术属性，共 11,802 条 DAP/C++ 对照；拒绝输入不产生值或调用值解析器。
- 16 架构 × 两仓库 = 32 个实际目标全部通过，累计 473,312 条组件断言和
  377,664 条对照行，其中算术属性 320,000 条。实际 ELF machine/class/endian、
  QEMU、依赖、二进制和日志均有哈希记录，输出与 x86_64 基准一致。
- 测试已接入公共 run_debug_linux_qemu_components.py。更新后的真实入口又在
  x86_64 两仓库运行成功，分别验证 QEMU/native 组件输出及 11,802 条对照。
  入口使用与矩阵相同字节的 verifier、fixture 和生产头文件。

| Linux 目标 | uwvm2 | uwvm2-ros |
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

35 个成功监控区间共 682.619 秒，包括编译、组件执行
和回收；不是连续 VM 或硬件性能测试。最大所属 RSS 为
749961216 字节。每次区间前后的 memory.events 完全一致；
已有 OOM 历史计数保留且未增加。所有所属 PIDFD 进程树均已退出并回收。

小型 DATA 的独立策略为：6 GiB 空闲内存准入、1 GiB 所属 RSS 上限、距 cgroup
上限 1 GiB 时中止、实际写入文件系统保留 1 GiB、所属输出不超过 64 MiB。只准入
明确固定的组件脚本，不能用于 VM、JIT、SDK 或语言生产器构建。TMPDIR 与输出
位于本任务 /run/user/1000 目录，校验实际文件系统及停止状态子进程环境；
完整产品原门槛没有降低。

## 证据和边界

[机器可读资格记录](debug_character_literal_qualification_20261006.json) 包含两份生产
源码指纹、实际 ELF、日志哈希和监控区间。原始来源：

- /tmp/uwvm2-character-literal-20261006-a1：完整冻结的两仓库源码。
- /run/user/1000/uwvm2-character-literal-20261006-a1：ELF、日志、依赖、完整收集记录、
  回收证据和最终公共入口快照。
- 修复前头文件 SHA-256：97bbf7b2a748a19e51356324d9e5e3d4b19b83191c47cc5ccde072dd123e4308。
- 修复后头文件 SHA-256：d58991fb7caf1b5e8351dfff3dae06206fbebe5252a99503943ee8f880deba8f。
- uwvm2 冻结生产指纹：sha256:a9e24ea765da7d1d05fd39fc999cd0a1cd2ab599bed6c5050a3f4e14864e4c0e。
- uwvm2-ros 冻结生产指纹：sha256:a9470e170b6906266f4a0d7e6cf088592e9d22babde1dae347bb32ff90bb9b44。

源码和原始证据以独立归档步骤保存为 character-literal-evidence-a1.tar.xz。
归档元数据、成员级 SHA-256 清单及持久目录 archive.json 决定备份是否成功；
不把失败或部分副本算作完成。外部安装的工具链、SDK、sysroot/QEMU 不复制，
实际依赖已记录哈希。Linux 持久盘当时可用空间为 0，未通过原归档加 64 MiB
保留空间的复制准入，因此没有创建 Linux 持久副本。完整归档已逐字节校验并保存到
本机持久路径：
/Users/liyinan/.codex/artifacts/uwvm2-character-literal-evidence-20261006-a1/character-literal-evidence-a1.tar.xz。
归档 SHA-256：cadd1dc37a64171de0e1ae4ff903c157f9151d07f01220cfaf753e30696fd5ed，36,368,132 字节，9,209 个成员。
Linux 上原冻结源码、原始日志和完整归档仍保留；本机 archive.json 记录实际备份。

本机写盘不足时，只退役本任务中已完整备份并逐项验证的失败暂存副本及重复清单；
其他任务的文件未被删除。源码修复和入口更新保留在两个本机仓库。

本轮未启动新的完整 CLI/VM 构建。原完整产品 /tmp 磁盘准入要求
9288400896 字节，当时只有 852185088 字节。
组件结果不代表真实八语言生产器 DWARF、运行时停点、热更新/检查点、所有 Wasm3
特性、ASM guest 边界或完整原生语言调试体验已实现或已验证；这些仍需后续实现
和完整产品测试。
