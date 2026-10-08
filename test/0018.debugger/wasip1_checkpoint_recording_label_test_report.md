# WASIp1 单环境 portable import 标签一致性修复（R15）

已同步修复 UWVM2 和 UWVM2-ROS：单环境导入请求的 recording_label 必须与快照中的 16 字节标签完全相同。此前组导入已检查这一条件，单环境却接受任意非零请求标签。现在返回 invalid_portable_snapshot 和 recording label differs from import metadata，在准备替换资源和发布 FD/文本之前拒绝。

此标签检查是数据一致性检查。标签、文件校验和和元数据都不提供恢复权限；原来的源码、builtin interface、真实 Core3 capture、完整 cooperative stop、host gate、GC 排除和 publication 证明仍然必须成立。它也不证明用户另选的 Wasm checkpoint 与 WASIp1 文件属于同一停点。

## 同步改动

两个仓库各四个文件：

- src/uwvm2/runtime/lib/uwvm_runtime_wasip1_portable_environment.h：补上单环境标签匹配检查。
- test/0017.runtime/debug_wasip1_portable_runtime.cc：真实停点新增 recording-label / recording-label-before 场景。
- test/0013.debugger/wasip1_recording_labels.cc：新增跨 OS 文件格式、标签和 FastIO 原生文件测试。
- src/uwvm2/uwvm/debugger/wasip1_checkpoint.md：说明原生 API 的请求与快照标签要求。

没有改变 wire version、CLI/DAP 命令或 ROS 的 mode 集合。C++ 文件、字符串、打印和既有命令解析使用 FastIO。正常 CLI/DAP 从载入文件复制标签，因此正常使用流程不变。

## 实际验证

Linux 的运行测试全部在原 64 GiB cgroup、swap=0 中执行。两仓库均使用 LLVM-full，真实 Core3 fused validator/compiler、typed before-park capture、GC root、完整暂停域和原 host gate。没有注入假暂停状态、capture key 或 native FD。

旧实现对照共 8 个进程、864 项断言：两仓库 × instruction/unwind × save/restore。逐字节翻转请求标签的 16 个位置，旧单环境导入都返回 restored；修改快照标签并重新产生有效 wire checksum，也被旧实现接受。对照使用上一轮已成功编译、源码依赖及 SHA 验证的 R14 对象，配合本轮新编译的真实停点 harness；历史对象的实际编译来源记录在 receipt 中。

修复版共 80 个单环境进程、6,184 项断言：两仓库的 runtime 和 host API 用本轮独立冻结的当前源码重新编译。十个场景为 standard、sparse-reserved、root-alias、normalized-path、dsync、root-flags、root-choice、fd-slot、resource-alias、recording-label；每种都跑 instruction/unwind 和 save/restore。

新增标签场景验证：

1. 请求标签任意一个字节不同，都拒绝，并且不返回 capsule/portable owner。
2. 快照标签不同，即使 wire checksum 有效，也拒绝。
3. 每次拒绝后重新导出的完整 wire 逐字节不变，覆盖 FD 图、rights、flags、cursor、free list、argv/env。
4. 零请求标签仍保持原 invalid_recording_label 行为。
5. 匹配标签正常返回 restored，完整 wire 不变，guest 最终返回 7。
6. 单独提供匹配标签、不给真实 capture，仍返回 unavailable_capture。

多环境组回归共 4 个进程、1,916 项断言，两仓库的 instruction/unwind 均通过。正向 Linux 合计 84 个进程、8,100 项断言。

| OS | UWVM2 | UWVM2-ROS | 实际执行 |
|---|---:|---:|---|
| Linux | 70 项 | 70 项 | 原 cgroup 原生进程 |
| Windows | 70 项 | 70 项 | 原 cgroup 内 QEMU/KVM、真实 Windows guest |
| FreeBSD | 70 项 | 70 项 | 原 cgroup 内 QEMU/KVM、真实 FreeBSD guest |
| macOS | 70 项 | 70 项 | 本机原生进程，编译/运行内存上界最大 974094336 bytes，低于 2 GiB |

四 OS 合计 560 项，验证 16 个标签字节的独立快照 round-trip、同标签 group、混合标签 group 拒绝、有效 nested/outer checksum 仍不能授权混合标签或覆盖输出，以及 FastIO Unicode 文件名和单/group 文件存取。Windows/FreeBSD 均有精确 guest nonce、退出码 0、KVM enabled 和未变化的 base image。

四 OS 部分测试的是 detached metadata 与原生文件接口；真实运行中的 import/restore 和组事务在 Linux 验证。断言次数不代表 Wasm 特性数量，也不表示四个 OS 都跑了完整 VM checkpoint。

## 原生 API 用法

从文件载入 portable snapshot 后，让 request.recording_label 等于 request.portable_restore->recording_label，再调用原来的 capture/import host API。标签相同仍须满足真实停点、源码/interface 和目标 capabilities 等全部条件。

控制台 bindings 仍使用逗号分隔，例如 `set wasip1 import 0 PATHHEX 2=3,3=4`。上一轮 R14 报告中并列 bindings 的例子漏写了逗号；其历史证据保持原始字节，本报告在这里纠正。

## 证据与资源限制

最终 SSH Linux 证据归档：

`/home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/wasip1-recording-label-20261006-r15/source-and-test-evidence.tar.gz`

大小 239749501 bytes，全部 513 个 payload 验证，SHA-256：

`61ee8071d5ef2099cafdd3163b6d1fb426b6b3ec71d0c088baa39f1c75a10edd`

包含实际对象、程序、Wasm、wire、编译依赖/SDK/链接 SHA、测试日志、成功和失败 guard 历史、确切执行脚本、VM receipts/serial、补丁及本轮文件原始备份。内嵌 immutable source/Mac archive 逐项验证了 18,556 个 payload，SHA-256 为 2217a765d8e2d39fb8b8bae2067c0954cdb79f69701489a61eea03acd5136fa3。

当前源码冻结包含其他 agent 已有的 native debug 更新，因此修复版没有直接复用 R14 runtime。最终 feature guards 全部通过，PID 经 pidfd 确認退出，parent memory events 未变化。没有对其他 agent 发信号。记录了此前共享 cgroup 超过 63 GiB 的编译中止；保护线只终止本任务进程。只在无 feature 子进程的情况下撤销本任务的旧 admission 等待。启动水位按实际驻留临时文件和已测编译峰值调整，64 GiB 上限和 swap=0 不变。

编译器专用 jemalloc 与配置 SHA 均记录；guest 使用原运行环境。元数据归档遇到用户文件系统空间限制后，只为本轮新私有文件预分配和写入，保持全局保留策略及 VM base image 不变。源码归档的完成字节已逐项校验，失败的准备/权限收尾记录保留，未计作功能通过。

**提醒：Wasm 与 WASIp1 checkpoint 必须在同一个真实 cooperative stop 同时采集。文件内容、外部 I/O、时钟和 kernel buffers 不属于 portable 恢复；本轮没有实现整实例的 Wasm/WASIp1 联合原子恢复。**
