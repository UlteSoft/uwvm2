# WASIp1 portable checkpoint：FD 扫描预算修复与测试

日期：2026-10-05。仓库：uwvm2、uwvm2-ros。
测试冻结目录：/tmp/uwvm-wasip1-scan-budget-20261005-r4。

## 已复现和修复的问题

portable 格式原来只分别限制 live bindings、reserved slots 和 closed slots，允许三者总数超过 65,536。65,536 个关闭槽加一个高位 FD，可以构造校验、编码和解码都接受的 65,537 槽位快照。真实 LLVM JIT 导入成功，但后续 FD 查询和再次 checkpoint 都返回 resource_limit。

本轮两仓库同步修复：

- valid(snapshot) 在分配验证数组前限制 bindings + reserved + closed 的总和。
- decode 在分配快照各行数组前拒绝超限总槽位，并检查 opens_size / closed_count 边界。
- 单环境和组格式、FastIO 文件导入导出，以及真实 native 单环境/组恢复使用同一校验。
- 保留有效的 65,536 槽位边界；稀疏 FD 数值仍可到 INT32_MAX，数字大小不等于槽位数。

没有更改 wire version。文件/字符串/打印/固定宽度字段编解码继续使用 FastIO。文件内容不进入 portable checkpoint，目标 mnt 仍需重新配置；Wasm 与 WASIp1 checkpoint 应在同一个 cooperative stop 同时保存。

## 当前测试结果

| 运行 OS | uwvm2 | uwvm2-ros | 验证范围 |
| --- | ---: | ---: | --- |
| Linux | 2,101 | 2,101 | native codec、FastIO 文件读写、FIFO 拒绝 |
| macOS | 2,101 | 2,101 | native codec、FastIO 文件读写、FIFO 拒绝 |
| Windows | 2,100 | 2,100 | 真 Windows QEMU/KVM guest，native codec 和文件读写 |
| FreeBSD | 2,101 | 2,101 | 真 FreeBSD 15.1 QEMU/KVM guest，native codec/文件/FIFO |

合计 **16,806 项 codec/文件断言**。Windows 未运行 Unix FIFO 用例，因此每次少一项。

Linux 使用真实 full LLVM JIT、实际 guest/cooperative pause、原 manager 发布入口，两仓库都测试 instruction / unwind：

- 单环境每种策略保存 25 项、恢复 57 项，4 组共 **328 项断言**。
- 多环境每种策略 **479 项断言**，4 次共 **1,916 项断言**。
- DAP 每仓库 28 个 unit tests，共 **56 个用例**。

真实恢复新增验证：

1. 65,537 槽位拒绝为 invalid_portable_snapshot；前后重新 capture 的字节完全一致，涵盖 FD graph、权限、游标、allocator order、argv/env。
2. 65,536 槽位且含 INT32_MAX 稀疏 FD 的快照成功恢复；FD 查询、再次 checkpoint 均正常；随后恢复原布局并继续真实 guest，返回 7。
3. 多环境事务第二个环境超限时，两个环境的 FD/文本/游标快照均保持一致，文件内容保持原状。
4. 正确的 nested / outer SHA-256 不能让超限快照通过；失败不替换解码输出。
5. FastIO 拒绝超限导出且不创建文件；拒绝读取已正确计算校验和的超限单环境/组文件。

## 测试环境和范围

远端所有编译、测试及两个 QEMU 进程均在原 cgroup：64 GiB memory.max，memory.swap.max=0，CPU 0,2,4,6,16-31。本轮 boot ID：fe09fe71-9493-4b3c-9295-694bcadeee5f。使用 UID/birth/pidfd 验证本任务进程；所有拥有的进程均已退出，未收养或向其他 agent 的进程发信号。QEMU 使用独立 overlay，原 VM base 前后状态一致，实际退出码与 PASS 计数均核对。

macOS 编译及测试进程保守合计 RSS 上界为 **2050260992 bytes**，低于 2 GiB。磁盘空间紧张时仅去重本任务持有、哈希一致的冻结输入副本。

测试以本轮冻结源码及真实 ros.11 SDK 为准。两个仓库本轮修改的 10 个文件与测试哈希一致。并行产生的 PPC64 / LoongArch 分支改动已单独记录源码差异；本轮未测试这些额外架构。四 OS 结果是 native codec/文件回归；真实 native 单环境/组事务新增回归在 Linux 完成。本轮没有重跑历史 64 组跨 OS JIT 迁移矩阵，也不声明完成整个 Wasm 运行时恢复验收。

## 证据与复现输入

[完整原始记录](wasip1_checkpoint_scan_budget_test_results.json) 包含真实 PASS 日志、编译/链接输入哈希、冻结清单、VM receipt、cgroup guard 和进程退出证明。历史报告未改写。

控制器失败重试已保留，未计入通过结果：第一次 group 传输因本机磁盘瞬时耗尽而失败，实际跑到旧 462 项 fixture；下一次新 fixture 已通过 479 项，但控制器漏计 content() 的 10 个辅助断言，误期望 469；修正后四次完整运行均确认 479。证据收集脚本的一次路径类型错误也已修复并重跑。

本轮冻结源码、测试脚本和证据已保存到 SSH Linux 持久目录：

/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-scan-budget-20261005-r4/test-inputs-evidence-1791213184333904170.tar.gz

归档 SHA-256：37864dce5881e7d2251ec15fee9bc70c3c8194859003b51295438435211fdf5a。SDK 使用原始记录引用的上一轮已校验持久归档。恢复后需重新认证 boot/cgroup anchor 再运行 guard；保存的 guard 记录原 boot 身份。
