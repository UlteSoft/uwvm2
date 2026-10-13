# 2026-10-06：语言元数据判定与切片范围回归

uwvm2 和 uwvm2-ros 同步修复了语言表达式中的三条错误路径：

| 路径 | 旧行为 | 当前行为 |
| --- | --- | --- |
| Rust sequence/string 识别 | 仅凭 `&[T]`、`&mut [T]`、`&str`、`&mut str` 名称及结构识别 | 还要求原始编译单元为 Rust（DW_LANG_Rust = 0x1c） |
| 隐式成员解引用 | 任意语言中以 `&`、`*` 开头的 pointer 类型名都可能触发 | Rust 引用前缀要求 Rust CU；Go 要求 Go CU；TinyGo 要求 C99 CU 与 TinyGo producer adapter；C++ 使用既有真实 DWARF reference profile |
| slice 索引范围 | 只检查选中元素 | 先检查完整声明的 length × element extent 能否落在 Wasm32/64 地址宽度内，再检查与读取选中元素 |

例如 Wasm32 的 `ptr=0xfffffffc, len=2, element_size=4, index=0`：第一个元素的四字节能落在地址宽度内，第二个元素越界。旧代码仍调用 reader；修复后返回 unavailable，reader 调用次数为零。恰好到达最后一个地址字节的有效完整范围仍能读。实现用经过前置检查的减法和除法证明范围，避免先计算 length × extent 导致溢出。

`ptype slice[0]` 继续只依赖元数据，不因运行时指针或 length 无效而丢失已知类型。Go/TinyGo 的 cap 可以因 DW_OP_piece 缺失，索引仍只需要真实可用的 pointer 与 length。显式 `->`/解引用、Go 未命名 pointer dot 和 C++ 左右值引用保持既有行为。C++ 字符串构造、诊断与 LE carrier 编码使用 fast_io。

Rust 字段规则包含引用自动解引用，Go selector 规则允许结构体指针的字段选择：[Rust field expressions](https://doc.rust-lang.org/reference/expressions/field-expr.html)、[Go selectors](https://go.dev/ref/spec#Selectors)。本实现仍是有限、只读的表达式 evaluator，未覆盖 Rust Deref trait、完整方法调用或语言求值；[GDB Rust 文档](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Rust.html)也分别列出其语言行为与限制。

## 实际执行结果

- Linux native：两仓库各 7 次编译、6 个正向执行、3 个旧 header 负向控制，全部符合预期。正向包含新 provenance/bounds fixture、已有 language expression、sequence、string preview、C++ 左右值 reference、Rust tuple 与 Zig postfix DATA 测试。
- 旧 header 的三个独立模式 `range`、`gates`、`members` 各自触发预期断言失败，诊断分别对应上述三条路径。
- 16 profiles × 两仓库 = 32 个实际目标 ELF/QEMU PASS。每次新 fixture 执行 1907 项断言（包括 fixture 自身的范围与语法检查），输出与 native x86_64 完全相同，共 61024 项跨架构断言。
- 目标：x86_64、aarch64、i686、riscv64、ppc64、ppc64le、ppc32、mips64、mips64el、mips32、mips32el、sparc64、loongarch64、s390x、armhf、armel。按实际 ELF 的 machine/class/byte-order 核对目标。
- 34 次成功保护器执行合计 565.757 秒（编译、链接、组件执行与回收），最高 owned RSS 595570688 字节。这不是连续 VM 运行或性能基准。
- 每次成功执行的 memory.events 前后均相同。保留非零既有事件基线；没有声称整个共享 cgroup 从未发生过 OOM。

全部编译和执行测试在 SSH linux 原 64 GiB cgroup 内完成：memory.max=68719476736、swap=0、cpuset=0,2,4,6,16-31，测试线程 affinity=16–31。保护器验证原 boot/init/birth/UID、PIDFD 和进程 ancestry，最终确认所有 owned 子树回收。

本轮 source header/DATA 使用独立的小型保护策略：6 GiB 内存 admission、1 GiB 共享压力停止线、1 GiB owned RSS、8 MiB 单文件、1 MiB log、64 MiB 全部 own 输出、1 GiB 写入文件系统余量、4096 inode。原完整 VM/LLVM SDK 构建门槛没有更改。/tmp 空间不足时，最初尚未启动任何测试的等待控制器被 original PIDFD 正常取消；实际输出及编译 TMPDIR 改用同机已有的 `/run/user/1000/uwvm2-sequence-provenance-20261006-a3`，保护器检查这个实际写入文件系统的 fsid、余量与 TMPDIR。

失败记录保留：第一次新 fixture 编译的 fast_io const-char-pointer 参数错误；第二次新 fixture 对 aggregate 输出节点数的错误假设；首次 ROS x86_64 cross 编译期间共享 memory.swap.max 的 zero-swap 验证短暂失败，保护器中止并回收 own 子树。随后读回原配置，独立重试通过。没有更改共享 cgroup，也没有删除或终止其他任务。

## 资格边界与证据

这里执行的是生产 header、实际 C++ 编译器/标准库及 QEMU 架构的组件检查。输入是 fixture 声明的 DWARF type_record 与 OWNED DATA reader；没有新生成各语言 producer Wasm，没有实际 stop/source transaction/guest memory 权限测试，没有新完整 CLI/VM/JIT、Wasm3 全特性或 ASM parity 资格。ASM 仍限定 Wasm 产生的上下文。标准 Go 启动路径和完整八语言 native 体验还需要后续真实产品验证。完整语言能力清单的状态计数未因这些组件 PASS 更新。

生产源码 cut：

- uwvm2：`sha256:07a00db1867e0ea8bca677607e597e0f52b3f5359e7afc7f30a370e97e0c105b`
- uwvm2-ros：`sha256:7aae0755ae01b3166fdaf2b17758a0e0f4ebd5d8b474de5b12593aaa1bdf42e4`
- 两仓库本轮 header SHA-256：`b4c129bf9acdf814d72dfd1b96084a96f74d9fc94a862cbff8aac3fc0957efd0`
- 新 fixture SHA-256：`7dd4188d9d343eb51862447b0e20dba511f1da27214f81bb88a4a5016b54286c`

冻结全部源码、实际编译依赖、工具、目标 ELF、stdout 与保护器原始记录均有哈希，完成后再次核对；后来的其他 agent 修改不自动获得这些资格。当前本轮 header 与测试的两仓库内容再次核对一致。

源码和脚本原件保留在 SSH `/tmp/uwvm2-sequence-provenance-20261006-a1`；成功运行证据在 `/run/user/1000/uwvm2-sequence-provenance-20261006-a3`。完整证据包共 9333 个文件，逐项 byte-verified；持久 Linux 副本为 `/home/macromodel/Documents/uwvm3-implementation/uwvm2-sequence-provenance-evidence-20261006-a1/sequence-provenance-evidence-a1.tar.xz`（31355236 字节，SHA-256 `be23beb1c6eaf53079f22d3baa68eea9036ddea5ddf2bcac49298fd4c905fc8a`）。XZ 包保留源码、原始日志、ELF、脚本与保护器记录；外部已安装工具链和 SDK 保留在其原有哈希路径，不包含在包中。本机下载因 ENOSPC 失败，仅退休了本轮自己的不完整副本；完整持久 Linux 副本再次验哈希，原始证据仍保留。

精确记录见 [本轮资格 JSON](debug_sequence_provenance_qualification_20261006.json)，用法见 [语言表达式说明](README.debug-source-language-expressions.md)。原完整构建目录本轮资源观察未满足磁盘门槛，未启动完整产品重建；这不影响上述组件结果的范围。
