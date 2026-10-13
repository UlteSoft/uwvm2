本轮在 uwvm2 和 uwvm2-ros 同步增加了 `until/u`、`advance/adv` 到指定源码行的接口，并修复裸行号被当作文件路径补全的错误。能力清单中 `source_until` 从 missing 更新为 partial；当前 88 个有限类别是 14 implemented、37 partial、35 missing、1 separate_level、1 prohibited_by_scope。这是类别统计，完整原生语言调试仍未完成。以前日期的报告保留各自冻结源码的历史结论；本报告提供新增资格。

在真实源码停点使用：

```text
until THREAD STOP /original/source/probe.c:35
advance THREAD STOP /original/source/probe.c:14
until THREAD STOP 35
advance THREAD STOP 14
```

`THREAD`、`STOP` 取自当前真实 `bt`/停点输出。只有一个实际停止的参与线程时可用：

```text
until /original/source/probe.c:35
u 35
advance /original/source/probe.c:14
adv 14
```

裸行号由控制器从经过验证的当前源码位置取得文件；接口参数没有读取宿主或 VM 的权限。路径必须匹配实际 producer 的原始映射，可包含空格或 Windows drive colon。解析器按最后一个 `:LINE` 分割，不打开源码文件。不存在的文件、没有生成 statement 的行、当前 until 激活之外的目标和过期 STOP 都在恢复执行前拒绝。

`until` 跳过更深的调用和递归激活，等待原物理激活/当前 concrete inline instance 的目标。`advance` 可以进入真实子调用或 tail successor，到达同一模块的指定源码 statement。两者在原激活实际返回或退出当前 inline instance 时可以先停下。每次继续都使用新捕获的 runtime activation chain、source owner、epoch/generation 和实际 emitted Wasm safe point；不是手工临时断点的包装。计划保留现有 deadline 和 65,536 个实际停点上限，真实断点优先终止计划。已有 Ctrl+C 的 cooperative pause/cancel 路径保持不变，本轮没有新增 live Ctrl+C 资格。

目前仍缺无参数 until 的循环语义、完整 locspec/相对/函数/地址目标、跨模块目标和选中保存 caller 的 until 起点。因此不能标成原生 GDB/LLDB 完全等价。原生文档仅作为[控制语义基线](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Continuing-and-Stepping.html)，没有实际运行原生调试器做对照。

| 实际验证 | 结果 | 范围 |
| --- | --- | --- |
| R20 两仓库完整 LLVM JIT 产品 | 两仓库各 128 PASS | C/C++/C-compatible ObjC/Rust no_std，DWARF4/5，instruction/unwind，两组各 8 种会话 |
| R20 成功重复矩阵 | 每仓库 10 轮，共 2,560 PASS | 20 个成功 root 合计 529.3 秒；原长测批次有 ENOSPC 失败，第 10 轮改用独立 a3 成功，失败保留 |
| R20 原始 Zig/TinyGo/真实 ObjC | 24 PASS | 原 compiler artifact，两栈策略，until/advance，两仓库；ObjC 使用实际 selector dispatch |
| R20 AssemblyScript | 8 PASS | 原 source-map-v3/stub runtime，实际 Code PC 和物理栈对照；不提供 typed locals |
| R21 ROS 完整 LLVM JIT 产品 | 128 PASS | main/runtime/host API 全部新编译并实际链接，真实 .ros.11 SDK |
| R20 和 R21 各 16 个 Linux profiles | 各 32 DATA cases PASS | 两仓库实际目标 ELF/QEMU/native，stdout 与 x86_64 组件相同；R21 包括裸行号补全修复 |
| R20 控制台/DAP 与策略回归 | 控制台 8 DATA cases PASS，source run-to/source step 4 DATA cases PASS | 不授予真实 VM frame/memory 权限；R21 source run-to 两仓库组件也通过 |

矩阵 8 种会话为：until-call、advance-call、until-recursive、advance-recursive、return-bound、refusals、breakpoint、replacement。每个 producer 产物先由 official Wasm validator 和 llvm-dwarfdump --verify 检查，再比较原始 line table 的实际 Code PC、statement 和真实物理深度。编译优化为 O1，wasm-ld 使用 -O0 保留有效 DWARF5 字符串偏移；原始合并 debug 字符串导致的 invalid DWARF 失败产物保留，没有跳过 verifier。

热替换用例通过真实 `replace` 接口将未激活 leaf 的原始 raw function body 发布成 generation 2，没有声称改变该函数的语义。原 leaf 的源码目标随后拒绝，仍有效的 caller 可以运行到目标。该用例验证源码映射随 generation 退休，不允许把旧 DWARF 带到新 body 上。

TinyGo 使用原始 `probe-unknown.go` 和 unknown-O0 Wasm。`AFTER_VALUES` 第 33 行没有生成 statement：until33 明确拒绝且真实停点不变；until35、advance14 到达原始 DWARF 的实际 statement。没有重命名源码、伪造 source row 或把 UNAVAILABLE 算 PASS。AssemblyScript 对原始 sidecar 做 Code PC/物理栈比较；类型、局部变量、inline/caller metadata 和语言表达式并未由 source map 自动实现。

16 profiles 为 x86_64、aarch64、i686、riscv64、ppc64 BE/LE、ppc32、mips64 BE/LE、mips32 BE/LE、sparc64、loongarch64、s390x、armhf、armel。组件测试的 compile defines 和每个实际 ELF/依赖/执行 argv 都保留；这些 DATA 结果不构成各架构完整 VM、语言停点或 guest ASM 的资格。

ROS SDK 已从实际 vendored LLVM 23.1.1-uwvm-ros.11 源码构建，64 个静态 archive 共 158,981,062 字节，generated headers 共 8,014,471 字节。File API/CMake consumer link contract、完整输入和输出哈希均核对，SDK 受管 root 543.9 秒正常完成。三个通过的完整 JIT 产品都新编译 main/runtime/host API 并实际链接；不复用旧产品对象。它们没有编译 interpreter；因此 ROS uwvm-int full 的语言 run-to 完整产品仍未验证，也没有恢复 ROS 的基础 mode。

所有 compiler/version/runtime/QEMU/unit 测试都在 SSH Linux 原 cgroup 的自有 PIDFD supervisor 内进行。64 GiB、swap0、原 cpuset、16 GiB 自有 RSS、共享内存距上限 1 GiB 的中止线，以及原 9,288,400,896 字节磁盘 floor 均未放宽。所有被选入 PASS 的 root/后代已退休，root 实际 wait/reap 完成，各批 OOM 计数未增加；其他任务的进程未被终止。新增 inode guard 按实际矩阵 203 个文件的规模从 16,384 校准为 4,096，原有内存/磁盘阈值保持不变。

最新普通版 R21 的完整编译被 ENOSPC/共享压力中止；额外语言和控制台的当前复测也有资源中止或未启动记录。证据封存时 `/tmp` 可用 5,960,089,600 字节，低于原 floor，共享内存 current 为 65,931,902,976 字节。没有把 R20 的完整产品 PASS 转授给 R21 普通版。gc Go 四次当前 full-JIT 尝试在 prepared 前触及共享压力中止线，未形成实际 source-refusal/run-to PASS；原 gc Go Wasm 不带 DWARF，仍需要专门的 source/runtime 后端。

冻结 R20：普通版 `sha256:b411cf0db89fa5cf05d3223a48ff93cd8c9c12810eda61ac92cca5606899af14`，ROS `sha256:9509a9af6c3da3efb32cde8163d3eccff6ad2b2544029b5f498ef091be493094`。冻结 R21：普通版 `sha256:14d3e299de62df46282a9e4b3cf4636a536263e1a054ea819aa177e31302ada2`，ROS `sha256:3708d796dba40f2472bdf9d64b6fd71226dca52c85bf8a35d2abbb19a02cbfcd`。本轮直接涉及的 14 个生产代码/测试文件两仓库一致；其他 agent 的更新和各仓库原有资格字段保留。本轮不是全部当前工作树的产品资格。

新增可复用测试是 `run_debug_source_run_to_cli.py`、`run_debug_source_run_to_dwarf_extra_cli.py`、`run_debug_source_run_to_assemblyscript_cli.py`。后两者在冻结 R21 source archive 之后加入并另行 pin；extra-DWARF 脚本与 R20 成功脚本相同，AssemblyScript 永久脚本加强 build closure 并将 imports 延后以支持 --help，但该永久路径的加强版尚未实际启动复测。所有后续调用仍须由原 PIDFD supervisor 受管执行。

剩余 35 项 missing 主要是 source watchpoint、反向执行、finish 返回值、值历史/tracepoint、?:/取地址/指针算术/函数调用/赋值、C++ 动态类型/STL、Rust trait/collection、Go map/channel/goroutine/runtime、ObjC dynamic/Foundation、split DWARF、Zig error-union/slice/comptime，以及 AssemblyScript typed locals/class/collections/GC/表达式/替换 metadata。ASM 只允许经过验证的 Wasm 生成上下文，本轮没有扩大到 VM/host 调试，也没有新的正向完整 ASM 资格。Wasm3 全特性和 WASIp1 外部 I/O/资源完整回滚没有新增完整产品资格。

完整证据 5,179 文件、原始 426,486,187 字节，已压缩并逐项验证。本机包 `/tmp/uwvm2-source-until-20261005/source-run-to-evidence-20261005-a2.tar.gz`，SHA-256 `3a16d8eb8517e205df1975b92f8bee51009aeb802a01f36cbf54a50f62d496f7`，大小 66,043,098 字节。另有较早 R19 的 8 个已完成构建输出完整备份，逐文件验证后仅回收其自有旧路径，源码、日志、SDK 静态库和 R20/R21 产品保留；本机备份 SHA `edd08aa4b8d7e30298bde8a2c2ffe7d90445460e34cc51ab1af1e241eec33d65`。精确 receipt/source/binary/input 哈希、失败及未启动记录见 [本轮资格](debug_source_run_to_qualification_20261005.json)，全部类别见 [能力清单](language_debug_capabilities_20261004.json)。
