# WASIp1 debugger FD 空槽复用修复（R13）

已同步修复 UWVM2 与 UWVM2-ROS：FD 表达到 65,536 个扫描槽时，`set wasip1 file` 和 `set wasip1 fd-dup` 可以复用已关闭的稠密槽。所有槽都占用时仍拒绝增长；保留的空资源槽仍计入占用额度。

本轮修改是 FD 分配策略修复。四个 OS 执行的是原生分配策略与文件输出测试；真实 LLVM-full 停点、checkpoint/import 和调试 FD 编辑闭环在 Linux 上验证。表计数测试没有被当作真实停点测试，断言次数不代表 Wasm 特性数量。

## 原因与实现

原判断 `renumber_map.size() >= maximum_fd_scan - opens.size()` 在扫描槽数恰好等于上限时，无论自由列表是否还有槽都会拒绝分配。

新增 `wasm_fd_storage_t::fits_allocation_scan_limit(limit)`：先检查关闭计数和已有表大小，再区分复用与增长。操作仍在原表锁和真实 cooperative stop 的 host gate 下进行，后续继续检查自由列表尾槽、rights、保留槽和 occupied FD 限额。扫描上限保持 65,536。

扫描槽数为 `opens.size() + renumber_map.size()`；占用槽数为 `opens.size() - closes.size() + renumber_map.size()`。稀疏 FD 的数值可以到 INT32_MAX，每个实际映射只占一个扫描槽。

修改文件，两仓库各五个：

- src/uwvm2/imported/wasi/wasip1/fd_manager/fd_map.h
- src/uwvm2/runtime/lib/uwvm_runtime_wasip1_managed_files.h
- test/0013.debugger/wasip1_fd_slot_reuse.cc
- test/0017.runtime/debug_wasip1_portable_runtime.cc
- src/uwvm2/uwvm/debugger/wasip1_checkpoint.md

ROS 的 mode 配置沿用现有实现，本轮不增加 mode。C++ 测试的字符串、命令解析、原生文件和打印使用 FastIO。

## 测试结果

| OS | UWVM2 检查数 | ROS 检查数 | 执行 |
|---|---:|---:|---|
| Linux | 126,588 | 126,588 | 原 64 GiB cgroup 内原生进程 |
| Windows | 126,588 | 126,588 | 原 cgroup 内 QEMU/KVM，Windows NT 10.0.26100.0 |
| FreeBSD | 126,588 | 126,588 | 原 cgroup 内 QEMU/KVM，FreeBSD 15.1-RELEASE |
| macOS | 126,588 | 126,588 | 本机原生执行，内存上界约 0.923 GiB |

每次 OS 策略测试包含 2,891 组计数布局，覆盖稠密/稀疏/关闭槽组合、零限额、恰好边界、已超额表、非法关闭计数、SIZE_MAX 限额和高位稀疏 FD。原生策略合计 **1,012,704** 项断言，包含 OS、仓库和限额组合的重复。

真实 Linux LLVM-full 测试：两仓库 × instruction/unwind × 8 个场景 × save/restore，**64 个进程，4,640 项断言**。八个场景为 standard、sparse-reserved、root-alias、normalized-path、dsync、root-flags、root-choice、fd-slot。

新增 fd-slot 场景：

1. 通过原 initializer 设置 occupied FD 限额 65,537，再通过真实 portable consumer 恢复恰好 65,536 个扫描槽的表，隔离两种限额。
2. 真实控制域已停住，实际 Core3 GC root/before-park capture 和 host gate 均存在，没有直接注入 FD 表或伪造停点。
3. 新建 A NUL B 的匿名文件与复制现有 guest FD 都返回自由列表尾槽，表大小不增长。
4. 新建文件具有真实 managed identity、二进制内容和零游标；复制 FD 保持相同 native resource，共享游标。
5. 恢复原游标并按逆序关闭后，完整 portable wire 与基线逐字节一致，包含 rights、别名、flags、游标、argv/env、保留槽和自由列表顺序。
6. 通过真实 consumer 将空槽填为已保存文件的别名。仍有一个 occupied FD 余量但没有空槽时，创建和复制都拒绝增长，完整状态不变且仍可 checkpoint。
7. 恢复原布局后真实 resume，guest 返回 7。

旧实现对照：两仓库和两种停点策略均复现错误拒绝，共 **8 个进程、592 项断言**。拒绝后完整环境不变，guest 正常 resume。负例独立计数。最终正向断言合计 **1,017,344**。

编译器、实际依赖、SDK/链接输入、运行对象、测试可执行文件、Wasm、portable wire、日志和 guard 均通过 SHA-256 校验。macOS 实际编译依赖在归档前再次校验，测试源 APFS 克隆与工作区 inode 独立。

Linux 沿用原 cgroup：memory.max=68719476736，memory.swap.max=0，cpuset=0,2,4,6,16-31。所有最终 feature guard 通过，owned PID 经 pidfd 确认退出，parent memory events 未变化。Windows/FreeBSD 有精确 guest nonce PASS、退出码 0、KVM enabled 和未变化的 base image。macOS 编译与执行均低于 2 GiB RSS 上界。

## 保存的证据与失败历史

SSH Linux 持久证据：

`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-fd-slot-reuse-20261006-r13/source-and-test-evidence.tar.gz`

大小 169,134,421 bytes；全部 17,152 个 payload 逐一校验；SHA-256：

`cf1303ff80eb507ec440cce76b43c0532fec88aa5949c1cf442a1152b7a33d0a`

归档包含完整冻结源码、原始备份、实际探针、编译与运行产物、脚本、日志、guard、VM 证据与逐项校验过的 Mac 输入和结果。同目录保存已退出 Windows guest 的私有磁盘压缩副本，以及迁移的 R12 已完成测试程序、R8 本机重复证据。只归档和回收本任务产物，其他 agent 文件与进程未清理，全局系统保留空间与 cgroup 策略未调整。

Mac 临时输入和程序在持久归档逐项校验后回收，原编译记录和日志保留。

保留且不计入最终通过数的失败历史：初始源码暂存 ENOSPC；新增探针的一处 FastIO 原始 C 字符串被编译器拒绝，改为 mnp::os_c_str；新增正向探针错误地在稀疏映射读取已恢复到稠密表的 FD91，崩溃后修正并重跑；持久归档的初始空间预检拒绝，随后只对新建私有归档文件预分配空间并完成校验。

## 使用范围

本轮不回滚外部文件内容，不新增 whole-instance Wasm restore 或原子联合 Wasm/WASIp1 restore。Windows 目录 NONBLOCK 的现有 ENOTSUP 行为沿用原实现。

**提醒：Wasm 与 WASIp1 checkpoint 仍须在同一个真实 cooperative stop 同时采集。Wasm-only checkpoint 不包含 WASI FD 和环境状态。**
