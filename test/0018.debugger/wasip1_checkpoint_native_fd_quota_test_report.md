# WASIp1 native checkpoint FD 配额修复与测试

本轮同步修复 uwvm2 与 uwvm2-ros；测试开始于 2026-10-05，完成于 2026-10-06。ROS 的编译模式范围保持既有 uwvm-int full / LLVM-jit full。

## 已复现的缺陷

在真实 LLVM-full guest 的 cooperative stop 中，使用原始 debug_jit 初始化产生 FD0 保留空槽。实际环境有 5 个活跃 FD 和 1 个保留槽，maximum_descriptors=5：

- 修复前 native capture 返回 captured。
- portable capture 正确返回 resource_limit。
- guest 随后正常恢复执行，返回 7。独立复现程序通过 28 项检查。

旧 native 路径只对活跃 descriptor rows 检查配额，保留空资源槽未计入。它们仍占据 WASI FD 分配器位置，因此两条 capture 路径的配额语义不一致。

## 修改

两仓库各同步 6 个文件：

- fd_manager/fd_map.h：增加锁内使用的 occupied slot 配额检查；先验证 free-list 数量，再用减法和短路比较防止无符号下溢及求和溢出。
- uwvm_runtime_wasip1_environment_capsule.h：在 pin 原生资源、发布 capsule 前检查活跃别名与保留槽的总量。
- uwvm_runtime_wasip1_portable_environment.h：使用同一检查，在构造 FD rows 前拒绝不足的配额。
- wasip1_fd_checkpoint_quota.cc：新增真实 FD manager、FastIO 原生文件、close/renumber/别名及 65536 边界测试。
- debug_wasip1_portable_runtime.cc：使用原始 Rdbg 初始化，并增加真实 stop 下的配额边界、20 次连续拒绝和完整状态不变测试。
- wasip1_checkpoint.md：说明配额与总 scan-cell 上限的区别。

每个活跃 FD 别名及保留槽各计一项；已关闭的可复用槽不计入 occupied 配额。稀疏 INT32_MAX FD 计一项。65536 总 scan-cell 上限继续包含已关闭槽。所有新增 C++ 文件 I/O、路径字符串、打印和读取使用 FastIO。

## 本轮通过的验证

| 范围 | uwvm2 | uwvm2-ros | 合计 |
|---|---:|---:|---:|
| Linux 原生 FD manager | 47 | 47 | 94 |
| 本机 macOS 原生 FD manager | 47 | 47 | 94 |
| Windows 实际 KVM QEMU 客体 FD manager | 47 | 47 | 94 |
| FreeBSD 实际 KVM QEMU 客体 FD manager | 47 | 47 | 94 |
| Linux LLVM-full 单环境 capture/restore，instruction + unwind | 268 | 268 | 536 |
| Linux LLVM-full 双环境事务，instruction + unwind | 958 | 958 | 1916 |
| Linux DAP WASIp1 协议测试 | 28 | 28 | 56 |

C++ 回归共 2828 项，DAP 另 56 项；前述旧实现复现另计。

新增真实 VM 检查确认：少一项即拒绝、恰好足够即接受；拒绝不发出部分 owner，连续 20 次拒绝后仍可正常 capture。前后 portable wire 完全一致，覆盖 FD graph、rights、flags、cursor、free-list、argv/env。保留槽计入配额，但不会被伪装成活跃 descriptor row。

四 OS 单元测试使用真实 FastIO 文件和原始 fd_close_base/fd_renumber_base，验证共享 native RC 与游标、关闭及高位 FD 移动后的计数、文件内容不变，以及无符号边界。它们不伪造 debugger ticket 或 native capsule。

Linux 编译、执行、QEMU 与证据采集均在原来的 64 GiB cgroup 中；memory.max=68719476736，swap.max=0。各自有测试阶段串行，认证 PID birth/UID/pidfd 后回收自有进程；未向其他 agent 的进程发送信号。客体使用私有 overlay，基盘前后元数据一致，客体与 QEMU 均退出 0，KVM 权限未改变。

macOS 用真实 libproc 监控自有进程，固定并保存监控器源码；单目标编译器最多有一个直接 frontend/linker 子进程，测试程序禁止 fork。保守合计内存上界最大 1482031104 字节，小于 2 GiB。

## 范围与并发修改

本轮在四 OS 上验证的是共同 FD manager 与原生文件操作；完整 native capsule capture/restore 与双环境事务在 Linux 上验证。本轮没有重跑完整跨 OS capsule 恢复矩阵，也没有重新认证整机 Wasm 状态恢复。

所有本轮修改在两个仓库中逐文件一致，且当前本机内容与测试冻结输入的 SHA256 一致。冻结前后另有其他 agent 的 ROS CFI 实现变动；交付时另有 snapshot emitter 与其文档的并发变动。证据分别记录这些差异，本轮未覆盖或回退这些并发修改。

已记录安全护栏停止的两次共享内存竞争尝试、首次复现选错普通 stdio 初始化的尝试，以及 macOS 首次过于保守的进程数上界估算。最终资格以成功的自有 guard、实际客体 PASS、退出状态与文件哈希为准。

保存 Wasm checkpoint 时仍应同时保存 WASIp1 checkpoint；portable 保存环境状态与 FD 元数据，文件内容不随它跨 OS 搬运，目标 mount 仍需重新绑定。

## 可恢复证据

完整结果：wasip1_checkpoint_native_fd_quota_test_results.json

SHA256：fb6ac8103e9b077c326c92292da35c04f9974881868aa6f30a4193fa6b2d9fbc

SSH Linux 持久归档：

/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-native-fd-quota-20261005-r5/test-inputs-evidence-1791216743812865620.tar.gz

归档大小：27002040 字节；SHA256：3dc9546b1eeca6245c12fdb041669d95ad42c337067e5840f344ae0a2521c4d2。包含 before/post 冻结源码、用例、命令、依赖哈希、guest 日志和规范化结果。原生对象/二进制和私有 VM overlay 可按保存的输入及命令重建，未放入该归档。真正 LLVM SDK 的前置归档路径与哈希保存在 archive_qualification 中。

复测入口为 /tmp/uwvm-wasip1-native-fd-quota-20261005-r5/admit.py，阶段包括 components、runtime、group、cross-windows、cross-freebsd、vm-windows、vm-freebsd。重启后先按实际 boot/cgroup anchor 核验护栏，不能直接使用历史 PID。
