# TinyGo Source variables 自动对象展开 — R38，2026-10-07

uwvm2 和 uwvm2-ros 已同步补齐当前源码帧的有限根变量显示及懒展开。在实际 TinyGo collectionProbe 停点，Source variables 中的 box 从 R37 的 unavailable 变为 guest:0x… / pointer，返回正的 variablesReference。用户展开它时，VM 重新选择该源码帧中的 `*box`，复制 main.CollectionHolder，返回 11 个只读字段；无需事先添加 Watch。这些字段与原来的 `*box` Watch、Hover、Variables evaluate 三个入口逐项一致。

## 使用

附着后在实际 source breakpoint 暂停，取得 stackTrace 给出的当前 frameId，然后请求 scopes，从名称 Source variables 的条目取得真正的引用；不要猜测或复用历史编号。以下编号只示意响应关系：

    {"command":"variables","arguments":{"variablesReference":123}}

响应中的 box 类似：

    {"name":"box","type":"pointer","value":"guest:0x108c0","variablesReference":124,"presentationHint":{"attributes":["readOnly"]}}

客户端 initialize 协商 supportsVariableType: true 才显示新增对象的 type。guest:0x… 是当前复制显示，不能据此发起地址操作。用响应的 124 请求 variables 即展开字段，也可传 filter: "named"、start、count 分页。真实 fixture 的字段依次为 Numbers、Named、Empty、NilMap、Buffered、NamedChan、Closed、EmptyChan、NilChan、Receive、Send；Named / NamedChan 保留原 producer 的具名类型，nil 显示 guest:0x0，Receive/Send/Buffered 的 guest bits 相同。字段指针保持叶子，不会继续追踪 map buckets 或 channel buffers。单独 Watch box 仍是指针显示叶子，显式 Watch `*box` 沿用 R37 的复制树展开。

## 查询和生命周期

先读取既有 locals source THREAD STOP ORDINAL 的有限 numeric packet。只有当前实际源码帧中、整份枚举唯一、未转义且匹配 [A-Za-z_][A-Za-z0-9_]{0,127} 的根标识符，原值恰为 unsupported/unknown scalar type 时，才尝试该页的 print-frame THREAD STOP ORDINAL ROOT；保留已支持的 scalar 原显示，不查询隐藏页。保留字、重复根（包括页外重复）、转义/Unicode/带分隔符的名称、无实际 frame 绑定的旧显示、非当前 caller/native 帧不会成为查询配方。每页最多 32 次根查询，额外显示预算 64 KiB；超出者保留原 unavailable。

非零指针的展开引用保存原根符号及实际 frame/thread/stop 绑定。初始变量显示不读取 pointee；用户展开时发起唯一 *ROOT 查询，仍由 VM 的实际 source activation、DWARF、完整 stopped cohort 和 guest-memory transaction 决定是否可读。指针显示的数字以及复制字段标签均不充当查询选择器。成功后保留只读复制快照，后续分页仅前后观察真实 status；不会再次追踪字段或嵌套指针。nil 是引用 0。明确的单次源读取 unavailable 只保留根原值，或缓存一个不可展开的 `<unavailable>` 子项；其他逻辑错误、传输故障及不完整包使整个请求失败并退休引用，不能返回一部分对象。

根查询和第一次 pointee 查询各有真实前后 status 检查，验证同一完整 stop key 与原 frame 绑定，校验完整 formatter envelope 和原根名称回显；单包仍限定 64 KiB、1024 nodes、depth 32，child extent 在复制 parent 内。懒指针和最终复制树统一计入每停点 8192 nodes / 512 KiB 保留预算，配额在发布引用前检查。continue、单步、替换、可变 debugger 操作、退出、错误及直接收到新 stopped 快照清理懒选择器、Source scope/frame 绑定、复制树及配额，opaque references 不重复使用。真实排队单步后，未展开的懒指针引用与已复制的对象、scope、frame 都被拒绝。

本轮仅改 Python DAP adapter 和测试，不重编或改动 C++，VM 原路径及 fast_io I/O 保持。ASM 范围继续只限 Wasm 产生的上下文；没有新增 VM 私有地址或 host helper 调试权限。

## 实际验证

所有功能回归、Python 测试和原 VM 执行均在 SSH Linux 原 64 GiB / swap=0 cgroup 内，经 birth/PIDFD supervisor 启动，使用原 CPU 集合。两个仓库分别以独立 R37 adapter 对照，真实复现 Source variables box unavailable；对照会话各自然退出 0，并核对原 OS wait / EOF。最终 R38 结果：

| 仓库 | protocol/回归单元 | instruction+unwind 短会话 | 持续会话 | 持续秒数 | 持续 Source 字段核对 | 持续标量比较 |
|---|---:|---:|---:|---:|---:|---:|
| uwvm2 | 110 | 2 | 62 | 605.657 | 682 | 4092 |
| uwvm2-ros | 110 | 2 | 62 | 609.115 | 682 | 4092 |

最终共 128 个真实会话、1408 次 Source scope 字段核对、384 个原对象 evaluation contexts、4224 次对应 Watch/Hover/Variables 字段核对和 8448 次 len/cap 标量比较。重复会话不算独立功能。每次核对原 source path/line/breakpoint、compiler arguments、11 个字段及 named/nil/alias、分页、非法表达式和实际新停点。原 guest 自检自然 exit 0，原 Popen 被 OS wait，broker/adapter 正常返回并排空真实 EOF；没有 post-exit quit。

14 项新增 protocol DATA 回归覆盖惰性、零指针/type negotiation、离页/转义/重复根/32 次预算、明确不可读与故障区别、snapshot 分页、wrong echo/完整包、new-stop/两次读取退休、native 转换及节点/字节预算，不能冒充其他语言实际 producer 资格。原 96 项一并通过。最终 8 个 guard job 全通过，本轮全部 10 个成功和 2 个失败 receipt 均保留：初版标量路径多一次 status，完整回归检测后去掉重复查询，重新冻结最终切面再测。全部已启动 roots 已 reap、descendant PIDFD 可读退休；memory.events 无增长且各项为零。

明确复用 R35 原 fresh 3 TU + link 完整产品和原 receipt，uwvm2 / ROS 实际依赖 pins 3446 / 3411、各 108 个原收录 dynamic-library file pins 测试前后匹配。库文件 pins 不声称全部实际加载。原 TinyGo 0.42.0 / Go 1.27.1 / LLVM 22.1.4 O0 no-scheduler gc=leaking Wasm、validator/DWARF 资格和路径保持，没有重写 Wasm。新 adapter、实际 helpers、runner/tests 单独冻结并核对前后 pins；最新并发工作区、其他 producer 和 ROS 最新 LLVM provider 没有因此获得完整产品资格。

原资源限制保持：每独立切面 1 GiB owned RSS、64 MiB 总输出、8 MiB 单文件、1 MiB root log，6 GiB shared spare admission；arena floor 9288400896 bytes、host floor 25 GiB、inode/boot/FSID/exec/cwd/OOM 约束不放宽。FSID 12191019222208619510，boot c8d3550f-3a19-40d7-8507-a76c2045ace5，直接复用 keeper，没有重启或删除 peer 文件。

增量证据 archive：/home/macromodel/Documents/uwvm3-implementation/retained-tinygo-scope-r38-final/tinygo-scope-r38-evidence.tar.xz，339620 bytes，SHA-256 60f3220dc87f031fa0e8dcbbd6af2d177937cde1e2524a042581cadc0edc764f，821 个成员逐一流式读取核验并 fsync。保留三阶段小型冻结源码、真实 raw DAP、JSON、guards 和成功/失败 receipts；C++ 完整 source/产品通过再次核验的 R35 archive 引用保存，新增完整 VM 副本数 0。原 archive 收录核心 qualification；archive 自身 SHA、事后文档和工作区检查由侧车记录提供，不能声称核心 archive 含自身 SHA。归档核验后仅退休前版切面的 8 个重复 raw 文件、348024 logical bytes；所有 results/guards/source 和最终 raw 保留，不把 global free 变化归因于独占物理释放。

## 未完成范围

能力清单 aggregate 继续 partial。当前实际资格仅为该 TinyGo 当前帧 box -> CollectionHolder 的变量窗口根/指针展开。其他语言或标识符语法、shadowing/歧义根、真实 nested arrays、enum/variant/string/slice、其他 TinyGo scheduler/mutex、map entries/iteration/runtime pretty-print、完整 Go grammar、gc Go/goroutines、Rust .len()、实际 IDE/general fast attach、全部语言 native 等价、完整跨架构 VM/JIT/DAP 及最新 ROS provider 仍待实现或实际验证。之前的 QEMU DATA 资格不代替完整产品测试；R37 文档中的 Source variables unavailable 是历史对照，本 R38 文档记录新的有限资格。

机器记录：[debug_source_scope_dap_qualification_20261007.json](debug_source_scope_dap_qualification_20261007.json)。生产实现 tools/debug/dap_adapter.py；实际测试 test/0018.debugger/run_dap_tinygo_objects.py；新分离 protocol DATA test_dap_source_scope_objects.py。
