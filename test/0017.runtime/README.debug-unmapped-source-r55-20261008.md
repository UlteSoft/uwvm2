# R55 — 没有源码行映射的指令仍保留语言变量

uwvm2 和 uwvm2-ros 已同步修复 DAP stackTrace 的源码行前置条件。底层以真实 Wasm PC、activation 与 DWARF DIE 作用域查询帧；源码行表的 line-zero 不代表变量作用域不存在。语言级 stackTrace 现在对已识别的 guest Wasm 停点查询认证 source-frame page，不再以源码行号为前置条件，成功后绑定准确的帧 ordinal；没有源码行时保持 line=0、column=0，不虚构路径或 sourceReference。

修复前，原始 Clang O1 产物在 Wasm offset 29、31 的 status 没有 source 行，DAP 只给出 Wasm locals/WASIp1 scopes；同一真实停点的 frames 与 locals source 已能返回有效帧及参数。两个仓库的前态证据均保留。修复后，这两个停点具有 Source variables，record.low=165、record.high=90、record.enabled=true；record.narrowed 已结束有效位置，准确显示 unavailable。false 产物在这些指令上同时结束 enabled/narrowed 的位置，两个字段都保持 unavailable。

Source variables、Watch、hover 和 variables context 的实际树与每个字段逐项一致。每次真实 Wasm 指令步进后，紧随排队的旧 frame、Source scope、object tree 与 Watch 均拒绝。没有 DWARF 或 metadata 查询失败时，诊断 backtrace 标签仍不能产生 Source variables；native stop 不查询语言帧，Wasm level 仍使用自己的 identity page。ASM 只能访问 Wasm guest 生成的上下文，禁止进入 VM 本体。

验证结果：

- 原 64GiB/swap0 cgroup 仍有效，直接复用；所有实际 VM/DAP、DWARF verifier 和协议单测均在 SSH Linux cgroup 内执行，受原 PIDFD/进程 birth、CPU、内存与磁盘监督约束。
- 两仓库各通过 473 项 DAP 单测，含新增 10 项无行号作用域与边界控制。
- C/C++/Objective-C/Objective-C++ × Wasm32/64 × DWARF4/5 × O0/O1 × true/false × 两种调用栈策略，完整矩阵共 256 个真实会话通过。每仓库复用 64 个原始编译 profile，未重写 Wasm 或 DWARF。
- 本轮合格源码会话共 1348 个，真实 location walk 停点 4604 个，其中 1352 个没有源码行映射。初始成员求值 16176 次，步进区间的成员求值 55248 次。
- uwvm2 / ROS 连续 607.32 / 607.35 秒，分别 544 / 544 个会话；两种策略交替覆盖全部 32 个 true profile。
- 原有整数 128、浮点 128、Rust/TinyGo/C/Wasm 76、Zig 16 个回归会话通过，共 348 个。
- 所有合格会话的原 guest OS wait、broker wait、真实 DAP EOF 都为 0；所有合格监督记录 OOM 增量为 0。

本轮只有 Python adapter 生产改动，没有改动 C++，也没有新编译大型对象。R54 两个完整 LLVM-JIT 产品、全部实际依赖与原始 producer Wasm/源码已重新核对 SHA。没有重复 QEMU 执行；R54 的 16 架构/32 个实际 ELF DATA 用例仅作为未改动 C++ 路径的继承证据，完整跨架构 VM/JIT/DAP 体验仍未资格认证。持续 true 测试使用早一版 R55 walker；最终公开 walker 只修正 false producer 的位置范围结束预期，两者生产 adapter 修复完全相同。

原失败也保留：第一轮单测的无 DWARF formatter 缺少新 frames 查询的明确拒绝；false runner 的哈希不闭合在测试入组前被拒绝；false producer 的两个字段同时结束位置，旧 true-only 范围预期导致测试继续走到真实 caller。分别补齐协议 fixture、修正冻结 runner closure、改用真实 false 范围并要求两个无行号停点；未把失败重算成通过。

Linux 证据归档：/home/macromodel/Documents/uwvm3-implementation/retained-unmapped-source-r55-final/unmapped-source-r55-evidence.tar.xz。17373440 字节、11492 个文件，逐成员 SHA 校验并 fsync；SHA256：a5453a2a8034b63ed71e4dc3aeab19d7b2351160f72d87243e6fc23498e844bf。原始协议逐会话压缩保存，没有复制 SDK、完整 VM 或编译对象到归档或本机。

88 项能力分类保持原状态，本轮只关闭 R54 记录的 line-zero Source variables 缺口。标准 Go、AssemblyScript 完整 ABI、动态 ObjC runtime、C++ RTTI/STL、Rust trait/runtime、goroutine、赋值/watchpoint、guest 函数调用和返回值捕获等仍有未完成项；当前整个并发修改工作区、最新 ROS provider、uwvm-int full 与 native IDE 体验也不能据此宣称已全部通过。
