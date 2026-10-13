# Wasm / WASIp1 联合候选世界准备与四系统测试（R27）

日期：2026-10-07。普通版 uwvm2 与 uwvm2-ros 同步实现。ROS 仍仅提供 full uwvm-int 和 full LLVM JIT。**本轮完成联合候选世界准备及四系统真实执行验证；完整的联合 restore/replay 仍未提供。**

## 实现

主机接口 `llvm_jit_checkpoint_prepare_instance_host_api(ticket, captures, request)` 新增 `include_wasip1` 和默认开启的 `require_managed_wasip1_resources`。真正的 coherent manager 在同一完整协作停止点，持有同一 execution lease、关闭的 host admission、独占 GC 和 publication guard，同时复制 Wasm 对象图及所有不同的可见 WASIp1 环境。相同 recording label 本身不能证明状态来自同一停止点。

WASIp1 候选资源先私有准备，再保持存活直至 Wasm 资源、实际 LLVM engine 以及私有 frame/root 规划完成。managed 匿名文件使用 FastIO 创建、写入和定位，保留二进制内容、游标、flags、跨环境共享别名；同时准备 FD rights、free-list 次序、保留空槽、argv/env 所有权和 views。后续任何阶段失败均通过 RAII 释放候选，不交换原 FD 表，不改变原世界执行位置。累计 descriptor、文本和 managed 内容配额继续有效。

默认 strict 策略拒绝 stdio、mount-backed directory 等外部资源，返回 `wasip1_preparation_declined` / `unsupported_resource`。显式设置 `require_managed_wasip1_resources = false` 只允许保持已有 owned 外部资源的 binding-only 排练；它不恢复外部文件内容、kernel offset 或已经发生的 I/O，也不使 observer 可恢复。

Wasm funcref 重定位新增真正 factory-issued WASIp1 builtin 的验证：原始 source/module/import ordinal、实际新 native adapter identity、精确 signature 和完整 108-byte canonical interface descriptor 全部匹配。描述符构造使用 FastIO buffer、print、write 与 little-endian 输出。引用链必须终止于相同实际 builtin，名称或签名相同不授权任意 provider。别名 fixture 覆盖 global、active table 和 passive element 对同一 WASIp1 funcref 的引用。没有增加宿主执行权限；ASM debugger 仍只能访问 Wasm 产生的上下文。

并发任务的私有 frame/root 准备实现纳入本轮冻结源码的集成验证，未将该实现归为本任务单独修改。

## 使用与返回值

```cpp
// 在发布 engine 前配置有限 observation budget。
llvm_jit_configure_debug_value_observation_host_api({});
llvm_jit_checkpoint_prepare_request request{};
request.recording_label[0] = std::byte{1};
request.include_wasip1 = true;
auto result = llvm_jit_checkpoint_prepare_instance_host_api(
    current_ticket, genuine_current_captures, request);
```

默认 unlimited observation profile 不会自动转换为 resumable profile，按预期返回 `engine_preparation_declined`。`prepared_and_discarded` 表示全部候选准备完成后已销毁，**不是已恢复**。`wasip1_prepared_together` 仅在整体成功后为 true；`wasip1_environments` 统计不同环境。未请求联合准备但存在 WASIp1 时，`wasip1_checkpoint_required` 提醒调用方必须同时保存 WASIp1。

## 实际执行结果

同一 merged integration source cut；每个 OS、每个仓库各运行原 fixture 与 builtin-alias fixture 的 instruction/unwind 两种策略。全部链接实际 ros.11 LLVM SDK，测试实际 native guest pause 与 guest WASI 继续读取；没有以交叉编译成功代替运行。

| OS | uwvm2 断言 | uwvm2-ros 断言 | 执行方式 |
|---|---:|---:|---|
| Linux | 764 | 764 | SSH Linux 原生，64 GiB cgroup |
| Windows | 744 | 744 | Linux cgroup 内实际 Windows QEMU / KVM |
| FreeBSD | 764 | 764 | Linux cgroup 内实际 FreeBSD 15.1 QEMU / KVM |
| macOS | 764 | 764 | 本机 arm64 原生，聚合上界低于 2 GiB |

合计 **32 次实际 native 程序运行、6072 项累计断言**。Windows 每次 186 项，其他 OS 每次 191 项；差别为 5 项 POSIX-only 断言，不是 Windows 跳过联合准备。断言累计不是独立功能数。

两库在 Linux 另各通过 complete-instance 与 complete-preload、instruction/unwind 共 4 次完整对象图回归，总计 8 次，未计入 6072。每次验证真正双线程 census、两个 positive episode、2 frames / 12 root carriers，以及循环 GC 别名、struct/array、稀疏 memory/table、passive segment/drop、异常 trace / throw_ref；preload 回归包含实际多模块 import 关系。它们仍只验证准备及原世界继续运行。

新增行为覆盖 strict 外部资源拒绝、binding-only 排练、managed-only 成功、capsule registry 耗尽、早期 Wasm quota 与后期 frame/root quota 失败、18 次重复准备/销毁与 registry 回收、伪造 capture 拒绝、原 FD 内容/别名/游标保持，以及实际 guest write 后第二次停止、恢复并读取 91。

macOS 使用未 strip 的原始资格二进制，本机 arm64 原生执行。384 MiB 自有 RAM disk 以及 128 MiB controller 余量计入保守聚合内存上界；最高上界为 899284992 bytes（857.625 MiB），低于 2 GiB。QEMU guest RAM 各为 2 GiB，退出 0，原始只读底盘前后 stat 一致，没有修改主机 KVM ACL。

## 源码和证据边界

冻结 manifest SHA256：`a4113876b763909d56c47ab8c6b5baa7adf53d5bea05c1fab36a450dc6dcd6b1`，8191 个文件；源码归档 SHA256：`44830b0b43256cb7fedf0784093baf27faf174a1f1eb03b6d79ab0321bab7a78`。完整逐平台执行回执、编译 guard、日志 SHA 和原始测试产物归档 SHA 见同目录 [机器结果](wasip1_joint_preparation_r27_test_report.json)。独立 [source manifest](wasip1_joint_preparation_r27_source_manifest.json) 保留精确输入。

本轮资格验证对应该冻结 cut。收尾发现两库 `uwvm_runtime.h` 和 coherent manager 已被其他任务增加 native retirement 接口；这些更新保留，但本轮测试不覆盖它们。其余 5 个本任务编译/fixture 路径仍匹配冻结 cut；README 后续补充能力边界。逐文件比对见 [workspace scope](wasip1_joint_preparation_r27_workspace_scope.json)。不把冻结产物冒充并发最新工作区的完整重建。

本轮不改变 portable wire，不重新声称完成 12 个跨 OS 迁移方向；那些属于 R26 单独证据。portable WASIp1 仍仅保存外部资源的元数据，目标需要重新配置 mnt 或提供 rebindings，外部文件内容不包含在 checkpoint 中。

## 环境恢复与占用

编译、Linux 测试和 QEMU 测试均受独立 8 GiB ext4 测试卷、6/7 GiB 准入/停止阈值、512 MiB 卷余量、inode 余量、32 GiB 主机磁盘余量和 64 GiB / swap=0 cgroup 约束。原始 object、executable、SDK archive 的重复展开副本仅在整体 archive SHA 与全部 payload 逐项回读后退休；源快照、原始产物归档和失败诊断保留。历史只读冷证据限制为 1 GiB，原历史路径通过资格回执指向新位置。未删除或终止其他任务资源。

初次 Windows 编译磁盘准入拒绝时没有启动测试子进程。macOS 磁盘准入曾拒绝，随后使用计入内存预算的 RAM disk。错误的 strip/architecture 辅助尝试未用于真实运行；最终测试使用 original arm64 binaries。旧本机 archive helper 的 FIFO 等待、Python streaming-tar Path 参数及磁盘不足问题属于测试/归档器问题，原失败诊断保留，不计为 runtime 通过。

测试完成后两机再次重启。本机 RAM disk 和 `/tmp` 副本随重启消失，不能声称它们经过后来手动卸载；有效 native 日志、回执和完整二进制归档已在重启前存入 Linux 专用卷。当前复用其他任务已经恢复的 64 GiB cgroup，按原 UUID `ae4d0a7f-dff9-493d-a216-82058227ed0e` 重新挂载同一 8 GiB 卷，未格式化。更新自有 guard 的 boot/PID/birth 后，最终元数据资格生产器在恢复后的 cgroup 内再次通过，复核 8191 个冻结输入及全部测试、依赖、产物 archive 和回执。

## 仍需实现

完整联合 restore/replay 需要把私有 WASIp1 环境安装到新世界、完成旧世界 worker retirement/join、安装 live GC roots、登记实际启动 worker、以不可失败的 commit 发布完整 Wasm/WASIp1 世界，并验证真正恢复后的执行。当前接口不执行这些操作，准备成功不能越过这些要求。

**保存 Wasm checkpoint 时必须在同一协作停止点同时保存 WASIp1 checkpoint。** WASIp1-only restore 不恢复 Wasm 内存和执行位置。

收尾存储实测：测试卷文件按唯一 dev/inode 计占用约 5.88 GiB，卷可用约 2.00 GiB，主机可用约 65.26 GiB；只读历史冷证据约 566.70 MiB。详细状态见 [重启后收尾记录](wasip1_joint_preparation_r27_post_reboot_state.json)。
