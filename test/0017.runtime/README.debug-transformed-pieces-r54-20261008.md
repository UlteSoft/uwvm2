# R54 — 优化结构体的 DWARF 分片与终态查询

本轮在 uwvm2 和 uwvm2-ros 同步实现了受限整数变换分片。Clang 的真实 O1 location list 将结构体字段放在多个 DW_OP_piece 中；high 需要逻辑右移，enabled 需要 AND/NE 布尔归一化，DWARF5 还包含 unsigned convert。旧产品在相同真实断点上将 record 显示为 unavailable；修复后 Source variables、Watch、hover、variables context 都能展开 low=165、high=90、enabled=true/false、narrowed=-128，并保留真实字段类型与声明顺序。

每个 atom 最多四步，作用于已初始化、已复制的 i32/i64 local。转换必须解析当前 CU 中准确的 base DIE，检查 encoding、byte_size、可选 bit_size/位偏移，仍使用类型边预算。完整 native carrier 先转换为逻辑整数，再经 fast_io 转为 Wasm 小端字节；piece/bit_piece 最后切片。没有按类型名字猜宽度。浮点、未捕获 local、global/operand、未解析转换及超宽移位不会产生伪造值；不支持的 atom 只让对应片段保持 unknown。

真实 Wasm 单步验证了 location 生命周期：narrowed 的位置失效后，前三个字段仍正确，narrowed 为 unavailable；随后 enabled 也为 unavailable，low/high 保持正确。旧 frame、scope、object 引用在单步后均拒绝。编译器 line-zero 指令上的 DAP Source variables 缺失被单独记录，尚未修复；并未把这些停点算作源码层能力通过。

持续测试还发现并修复了独立 broker 退出竞态：guest 控制端点先关闭，原进程退出尚未被 broker 观察；随后的 wait 可能触发 EPIPE，或内部 status 返回终态后过早关闭连接。现在只有认证后的 status/wait 可以按原始 Popen 的真实 OS poll/wait 结果得到终态回复，终态窗口有时间上限；允许 status 后紧接 wait。存活进程、超时、异常包、未知是否执行的写入仍失败，不重放命令，不伪造成功。实际延迟 250ms 的终态查询和八组专项单测验证了这条路径。旧 ROS 第 240 会话的失败、过渡修复失败以及未启动测试的 Docker 入组超时均保留，未计入通过数。

验证结果：

- 两个完整 LLVM-JIT 产品各三个 TU 均重新编译、链接，实际依赖和前后输入 SHA 闭合；两个仓库各通过六组 C++ 组件检查和 463 项 DAP 单测。
- 真实 C/C++/Objective-C/Objective-C++ × Wasm32/64 × DWARF4/5 × O0/O1 × true/false，各仓库 64 个原始 producer profile，均通过 Wasm validator 和 DWARF verifier，未重写或剥离调试信息。
- 最终新 broker 下，分片测试共 1478 个合格会话、5912 个字段核对、17736 次成员求值，含实际分页、变量树、原始 guest 自检和引用退休。
- uwvm2 连续 626.66 秒，ROS 连续 626.73 秒；两种调用栈策略交替覆盖全部 32 个 true profile，false 完整矩阵另行覆盖。
- 原有 primitive 128、float 128、Rust/TinyGo/C 76、Zig 16 个会话在最终 broker 下全部通过。
- 16 架构 × 两仓库 = 32 个实际 target ELF 分片数据层用例通过，并与 native x86_64 输出一致。包括 aarch64、i686、riscv64、ppc64/ppc64le/ppc32、mips64/mips64el/mips32/mips32el、sparc64、loongarch64、s390x、armhf、armel 和 x86_64。
- 所有合格原 guest OS wait、broker wait、真实 DAP EOF 均为 0；保留的监督记录 OOM 增量均为 0。所有实际编译、validator、DWARF、VM/DAP 和 QEMU 执行均在 SSH Linux 原始 64GiB/swap0 cgroup 中完成。本机仅编辑、AST/哈希核对和保存小报告。

证据归档保留在 Linux：/home/macromodel/Documents/uwvm3-implementation/retained-transformed-pieces-r54-final/transformed-pieces-r54-evidence.tar.xz。29691664 字节、20095 个文件，逐成员 SHA 校验并 fsync；SHA256 为 f1750425ddb243c79ab49be62bcad9435589fc0ce36af006d59815d480c79fb6。归档不复制 SDK、完整 VM 或全量产品对象。本机只保留较小元数据。核验后清理 338.1 MiB 本轮自有可生成对象，最终 VM、原始 Wasm、最终组件/QEMU ELF、SDK、源码和证据均保留。

资格边界：完整产品使用 R53 冻结切片叠加本轮四份 C++ header 修复，配独立固定的历史 LLVM23 developer provider；host broker 修复在独立固定的最终 DAP/回归切片验证。冻结 manifest source ID、产品 SHA 与继承的 embedded build label/旧 receipt scope 分别记录。并发 agent 的最新源码修改未被覆盖，也不能据此宣称最新整个工作区、最新 ROS provider 或 uwvm-int full 已通过。

16 架构只证明实际 ELF 数据路径，尚未证明完整 VM/JIT/DAP 体验等同 x86_64。一般 DWARF 栈/分支/算术、generic/signed/float conversion、operand/global 捕获、VM 本体调试均未加入。标准 Go、AssemblyScript 的完整 ABI、动态 ObjC runtime、C++ RTTI/STL、Rust trait/runtime、goroutine、赋值/watchpoint、调用 guest 函数和返回值捕获等仍有未完成项。88 项能力状态保持 14 implemented / 39 partial / 33 missing / 1 separate-level / 1 prohibited-by-scope，未因有限分片支持而改为全部完成。ASM 调试始终限制在 Wasm guest 产生的上下文。

分片和位操作语义参考：[DWARF5](https://dwarfstd.org/doc/DWARF5.pdf)。本轮成功范围由原始编译器输出与实际执行证据决定。
