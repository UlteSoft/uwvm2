# WASIp1 checkpoint 根目录 FD 选择修复与测试（R12）

已同步修复 uwvm2、uwvm2-ros 的 portable WASIp1 checkpoint 导入：较低编号 FD 的权限满足、标志不匹配时，不能遮蔽同一真实挂载下较高编号、已经持有完整权限及正确标志的 FD。ROS 运行模式范围没有变更。

原始 guest path_open 创建独立根目录资源：151 的 base=PATH_OPEN（0x2000）、flags=0，inheriting 去掉 PATH_OPEN；174 的 base=0x2000、inheriting=0、flags=NONBLOCK（4）。旧实现选中 151 后误走重新构造路径，需要额外继承 PATH_OPEN，返回 capability_denied。两个仓库、instruction/unwind 真实 JIT 停点均复现，失败后的完整 portable wire 与恢复前相同。

修复把权限与标志共同纳入选择：一个实际 FD 必须覆盖全部保存的 base/inheriting rights，且标志相同，或独立持有 FD_FDSTAT_SET_FLAGS 来调整 APPEND/NONBLOCK。同步位 DSYNC/RSYNC/SYNC（mask=26）必须已经相同。否则仍需一个实际 PATH_OPEN 父 FD 授权完整构造。不同 FD 的 base、inheriting、SET_FLAGS 不能拼接。

原生观察和成功/失败选择只在本次实际封闭 host gate 内缓存，随后销毁。不同保存资源仍产生独立 FastIO native descriptions，aliases 继续共享同一资源。成功恢复后关闭 174，再次恢复正确拒绝；完整 wire 逐字节比较确认目标全部状态未改变。

原生测试调用原始 wasm32/wasm64 path_open、fd_fdstat_get，覆盖 owned/borrowed 根、错误状态遮蔽、完整匹配、单 FD mutable authority、同步拒绝、权限不能合并、失败观察、重排、空索引和缓存观察次数。没有伪造 native capture ticket。

| OS | uwvm2 assertions | ROS assertions | 执行环境 |
|---|---:|---:|---|
| Linux | 8,352 | 8,352 | 原 64 GiB cgroup |
| macOS | 8,352 | 8,352 | 本机，保守内存上界 < 2 GiB |
| FreeBSD 15.1 | 8,352 | 8,352 | 同一 cgroup 内 KVM/QEMU |
| Windows | 8,356 | 8,356 | 同一 cgroup 内 KVM/QEMU |

原生断言共 66,824。其中 65,536 项是缓存的重复查询断言（每个 ABI/所有权组合 1,024 次双断言），不能当作不同特性的数量。Windows 每个组合额外验证 directory NONBLOCK 明确返回 ENOTSUP 且不发布 FD。

Linux 使用 23.1.1-uwvm-ros.11 LLVM SDK、原始 owned-source initializer、fused validator/compiler、真实 Core3 GC local 与 cooperative stop。WAT 通过实际 wasm-tools parse/validate --features all。没有替代 native authority 或跳过实际 Wasm 运行。

| 真实 JIT 用例 | 每策略 save | 每策略 restore |
|---|---:|---:|
| standard | 51 | 83 |
| sparse-reserved | 52 | 89 |
| root-alias | 52 | 85 |
| normalized-path | 51 | 84 |
| dsync | 51 | 87 |
| root-flags | 53 | 103 |
| root-choice（新增） | 54 | 109 |

各用例在两个仓库的 instruction/unwind 下分别执行，56 次 fresh-process 保存/恢复共 4,016 项断言。四次真实多环境 group 测试各 479 项，共 1,916 项，覆盖成功恢复和准备失败后的完整原子回滚。所有 guest 正常 resume 并验证结果。修复前负向复现另有 304 项，重复的初次 UWVM baseline 未计入。

合计 72,756 项正向断言，非缓存重复部分为 7,220 项。Linux 编译、Wasm 验证、feature tests、cross compilation、QEMU 均在原 cgroup：memory.max=68719476736、memory.swap.max=0，保留 63 GiB parent abort 线。两个 VM 各 2 GiB，验证实际 OS、KVM enabled、输入 SHA、nonce statuses、正常关机、只读基盘未改动。
macOS 编译保守峰值：uwvm2=1662451712 bytes；ROS=1659486208 bytes。

保留了脚本解析/汇总失败、build metadata hardlink 隔离修复以及共享 cgroup 达到预留线后只停止本任务编译的记录。通过的执行重新生成并核对 compiler dependencies、object/binary SHA 和日志。历史 host-api object 重用以原始 dependencies、相同字节的当前输入和测试时 proof SHA 复核，未称为重新编译。只归档并核验本任务完成的临时副本，保留实际编译输入，没有向其他 agent 的进程发信号。

本轮四 OS 部分是原生 FD ABI/选择器测试；完整 LLVM checkpoint 执行在 Linux。Windows directory NONBLOCK 仍不支持，需要精确恢复该标志时拒绝。文件内容、kernel buffers、外部 I/O 不在 portable metadata 内。本轮不声称实现 whole-instance Wasm 或原子 Wasm+WASIp1 joint restore。Wasm checkpoint 仍须提醒在同一 cooperative stop 同时 checkpoint WASIp1。

源码：[FD 索引](/Users/liyinan/Documents/MacroModel/src/uwvm2/src/uwvm2/runtime/lib/uwvm_runtime_wasip1_mount_identity.h)、[consumer](/Users/liyinan/Documents/MacroModel/src/uwvm2/src/uwvm2/runtime/lib/uwvm_runtime_wasip1_portable_environment.h)、[原生用例](/Users/liyinan/Documents/MacroModel/src/uwvm2/test/0013.debugger/wasip1_root_selection.cc)、[JIT harness](/Users/liyinan/Documents/MacroModel/src/uwvm2/test/0017.runtime/debug_wasip1_portable_runtime.cc)、[WAT](/Users/liyinan/Documents/MacroModel/src/uwvm2/test/0017.runtime/fixtures/debug_wasip1_root_choice.wat)。六个 source/test/doc 文件在两个仓库同步，最终 SHA 位于相邻 results JSON。

完整证据在 SSH Linux 本任务专用目录：

- source archive："/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-directory-flags-20261006-r11-private-vm/r12-source-evidence.tar.gz"；94257606 bytes，51068 个逐项验证 payload；SHA256 be9e66201c18db3c1d6951a877d8d8c1bc8a5a0485f0768ed710631450965f16。包含 before/post、修改前 originals 与嵌套 macOS 完整证据。
- test archive："/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-directory-flags-20261006-r11-private-vm/r12-test-evidence.tar.gz"；22275456 bytes，303 个逐项验证 payload；SHA256 0317ab99566fbd5363d33430ec212fbf6d82d5eb1226c0807b45d31341350549。包含脚本、实际 argv/dependencies、qualified reuse、日志、WASM/metadata、guard receipts、失败历史与完整 results JSON。
- 完整 results SHA256：8cccb64dd73d457bf7204b9fa41154f0a1c3205c7e98d9f050e46824251250a0；patch SHA256：ffab9422412ae57e080d70b79a1088dae3ae8dff049c4f084831c285aaa64ef9。
- SDK 继续使用 R3 已验证的持久备份，未移除共享 SDK。归档退休均有 SHA 记录；归档完成后的本机空间整理记录作为 delivery supplement 单独保存。

两个仓库的报告、compact results、delivery receipt 字节一致。
