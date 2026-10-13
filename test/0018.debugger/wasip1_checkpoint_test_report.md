# WASIp1 FD / checkpoint 实现与测试报告

日期：2026-10-04。实现同步到本机 `uwvm2` 与 `../uwvm2-ros`。所有实际编译、C++/Python 测试、wasm-tools 验证和 uwvm 执行都在 SSH `linux` 的原指定 cgroup 中完成。

## 本次交付

- 增加匿名受管文件构造、guest FD 复制和关闭。构造支持二进制 NUL，使用真实 WASIp1 FD 表、分配规则和 rights 比较；不接受 host FD 或 host 路径。
- 增加 live WASIp1 checkpoint 的保存、恢复、释放。保存 argv/env、FD 表、rights、别名、关闭槽分配顺序、预留空槽，以及受管文件的内容、长度、游标、flags。
- 恢复前准备完整候选状态；一个保存的受管资源只创建一个新 backing，所有别名共享该 backing。校验/分配/原生操作失败时不提交；旧 owner 的 noexcept 析构可关闭已经退役的句柄。
- `bindings` 允许通过原始 owned resource 保留外部 FD 绑定；`strict` 遇到外部资源直接拒绝。它们都不回滚外部 I/O。
- C++ 文件操作、字符串、输出和输入解析使用 FastIO，包括 `native_file{io_temp}`、pread/pwrite、seek、`system_call`、print、`parse_by_scan`。Linux `io_temp` 默认 O_APPEND 已在构造时清除，避免初始定位写变成追加写。
- 新增联合捕获 API `llvm_jit_checkpoint_capture_instance_with_wasip1_host_api`，在同一个真实完整暂停、host closure、GC exclusion、publication 作用域捕获 Wasm 图与可见 WASIp1 环境。失败时不会返回部分图/capsule 组合。
- Wasm 单独捕获 API 会打印提醒，并返回 `wasip1_checkpoint_required` 标志；CLI/DAP 的 WASIp1 checkpoint 回复也提醒两者必须一起保存。
- 修复既有 checkpoint effect hook 缺少 JIT 编译保护的问题，保证关闭 JIT 的完整解释器构建可通过。ROS 未加入任何已删除的基础模式。

设计与接口完整说明：[wasip1_checkpoint.md](../../src/uwvm2/uwvm/debugger/wasip1_checkpoint.md)。

## 使用示例

在 `-Rdbg` 的真实 LLVM full cooperative stop 中执行：

```text
set wasip1 file 0 410042
info wasip1 fds 0
set wasip1 checkpoint 0 0
# 继续执行或修改 WASI 环境后，在新的真实暂停点：
set wasip1 restore 0 0 bindings
unset wasip1 checkpoint 0 0
```

`410042` 是 A、NUL、B。构造回复的 `affected=N` 是实际 guest FD。复制/关闭要带查询到的两个旧 rights：

```text
set wasip1 fd-dup 0 N 0x60006e 0
unset wasip1 fd 0 N 0x60006e 0
```

不能将 N 当作 native FD。可追加 `if-stop STOP_ID`；失效比较不会提交。SLOT 范围 0..7；restore 必须明确选择 `bindings` 或 `strict`。普通 stdio/preopen 环境通常会拒绝 strict。

**提醒：checkpoint Wasm 时，要同时 checkpoint WASIp1，而且必须在同一个 cooperative stop。单独恢复 WASIp1 不会恢复 Wasm 内存、globals、栈等状态。** 原有 Wasm graph 捕获当前是 native API，此次没有新增一个虚构的 `checkpoint wasm` 控制台命令。需要同一状态切面时使用联合 native API。

DAP `uwvm/wasip1Edit` 新增 `createFile`、`duplicateDescriptor`、`closeDescriptor`、`saveCheckpoint`、`restoreCheckpoint`、`dropCheckpoint`；restore 使用显式 `resourceMode`。

## 实测结果

| 检查 | uwvm2 | uwvm2-ros |
|---|---|---|
| WASIp1 C++ component | 4 个程序通过；新增命令用例 17 项 | 相同 |
| DAP unit test | 14 项通过 | 14 项通过 |
| 真实 CLI / Linux broker / DAP | 170 个检查记录全部通过 | 170 个检查记录全部通过 |
| 新 checkpoint native fixture | instruction / unwind 各 116 项通过 | instruction / unwind 各 116 项通过 |
| 既有 environment / capsule / owned capsule native 回归 | 3 个程序 × 2 种策略通过 | 相同 |
| LLVM full 普通 WASIp1 执行 | 通过 | 通过 |
| uwvm-int full 独立构建和普通 WASIp1 执行 | 通过 | 通过 |
| ROS 删除模式的拒绝检查 | 不适用 | lazy / tiered 均拒绝 |

170 项是每个仓库包含两种栈策略的总检查记录数；不等同于 170 个 WASIp1 syscall。新增 native fixture 四次合计 464 个断言。WAT 经 wasm-tools parse 和 all-features validate 后再执行；这不是 Wasm 3.0 全特性覆盖率声明。

新增 native fixture 的重点覆盖：

1. 真实 before-park owner 与完整 cohort；原始 WASI 初始化与 active segment 初始化；不伪造暂停标志、native FD 或世界恢复权限。
2. A/NUL/B 文件构造、rights 比较、dup 的真实共享 RC、唯一资源计数、每文件/总 payload 上限、16 个 live capsule 注册限额。
3. Wasm-only 提醒；同一 recording label 的联合捕获；资源注册耗尽时没有半份联合快照。
4. 改写内容/长度、seek、修改 O_APPEND、修改 argv/env 后恢复；strict 与当前 FD-limit 拒绝时内容和 FD 表不变。
5. 原有外部目录/stdio 的 authentic owner 保留；FD close 后 number reuse；旧快照取回原绑定、别名和分配顺序；rights 恢复。
6. 当前文件超过捕获上限时拒绝新捕获，仍可恢复一个合法旧 capsule；不复制已经过大的当前 payload。
7. 同地址不同 shared_ptr control block、伪造 before-park owner、错误 module 和退休 pause 均拒绝；reset 后数据副本可读，但不能取得执行权限。
8. **恢复后真正继续执行 guest：WASI fd_write 将文件变成 xyz!，在下一真实暂停点恢复，再由真正的 fd_read 读取 A/NUL/B；另一 alias 随后读到 EOF，证明共享游标。**

CLI/DAP 另验证槽位边界、显式模式、失效 stop guard、失败 strict 后状态未变、清理/释放、环境与 trace 回归。预留 FD0 专项测试证明恢复后它不会被分配给文件，guest 原始 `fd_close(0)` 语义保持，调试管理输入仍可使用。

## 测试条件和证据

只读/准备操作在本机或 SSH 做；编译和测试进程由 guard 进入原 cgroup 后执行。原 cgroup：

```text
/system.slice/docker-bec3a6e4e013ba07c1583e46eeb81707c90c59375ae1c208a6bb87cc4f773f45.scope
memory.max = 68719476736 (64 GiB)
memory.swap.max = 0
cpuset = 0,2,4,6,16-31
本任务亲和性 = 16-31
```

测试前执行仓库的 `require_wasm3_test_cgroup.sh`。guard 使用实际 PID/birth/UID、pidfd 和继承 cgroup 检查，最终选中的每个成功区间 memory.events 前后相同，追踪的 owned pidfd 全部 retired。累计历史 OOM 计数不是零；不把历史计数说成本次零 OOM。共享组中的其他进程未被接管或发信号；没有更改共享组限额。

clang 23 / libc++；为降低共享组编译峰值，LLVM-only 和 interpreter-only 变体分别构建，所有 TU 使用一致 backend 宏。main / host-api 使用 -O1，runtime / native fixtures 使用 -O0，component 使用 -O3，C++ 调试信息为 -g0。这里验证功能正确性，不是性能或 C++ -g 调试体验认证。实际 JIT 的优化/profile 不因 host C++ 优化级别而替换。ROS 使用真实构建的 .10 LLVM SDK，uwvm2 使用既有 .9 SDK，没有伪造版本或放宽校验。

测试在固定集成源码快照中执行。工作区的并发 language scalar-expression / breakpoint 改动被保留；本任务的 WASIp1 controller、console formatter 和 help/protocol 片段与测试快照完全一致。其他实现/测试文件与冻结输入一致。测试后仅调整设计文档的措辞并写入本报告；不宣称认证期间不断变化的全部并发改动。

早期共享内存压力中止、fixture 初始化问题和解释器缺少 JIT guard 的失败记录保留，不计入通过结果。本文数据只取对应修复后的成功检查。

### uwvm2

冻结源码 ID：`sha256:05a7ae5769fb4dc068b7ce266dc96c926760f9ea4e63dedef0650da2596b528a`。

- llvm 产品 SHA256：`2a8e5d16454697888603008c7f223b8c44abd3a3954c34a4322f059e434b5ca0`。二进制：`/tmp/uwvm2-wasip1-checkpoint-1791102988565542000/uwvm2-llvm-out/main`（SSH linux）。
- int 产品 SHA256：`f32b366123edfea032b00d341e244721d94435053d44178203928d102657eb11`。二进制：`/tmp/uwvm2-wasip1-checkpoint-1791102988565542000/uwvm2-int-out/main`（SSH linux）。

各 phase 的实际 source ID、结果/日志 SHA256、guard receipt 和退役证据见 [wasip1_checkpoint_test_results.json](wasip1_checkpoint_test_results.json)。既有 component / native 回归在较早快照完成；之后仅新增 no-JIT effect-hook 条件保护，LLVM 路径定义不变。新 checkpoint / CLI / 普通两种 backend 的最终测试在上述最新冻结源码完成。

### uwvm2-ros

冻结源码 ID：`sha256:a279356c1ea04685aedab6f75e86302b8c626db521700ccdda35eef996e62be8`。

- llvm 产品 SHA256：`e4820a71dc2f5d8742d3c7716ac12ca986d5a5f9b47f954e207b8e996b748ce2`。二进制：`/tmp/uwvm2-wasip1-checkpoint-1791102988565542000/uwvm2-ros-llvm-out/main`（SSH linux）。
- int 产品 SHA256：`90211070243eee4e9a13ea294bede68b0ddf9fc934c23453173a74070b28baa3`。二进制：`/tmp/uwvm2-wasip1-checkpoint-1791102988565542000/uwvm2-ros-int-out/main`（SSH linux）。

各 phase 的实际 source ID、结果/日志 SHA256、guard receipt 和退役证据见 [wasip1_checkpoint_test_results.json](wasip1_checkpoint_test_results.json)。既有 component / native 回归在较早快照完成；之后仅新增 no-JIT effect-hook 条件保护，LLVM 路径定义不变。新 checkpoint / CLI / 普通两种 backend 的最终测试在上述最新冻结源码完成。

## 已实现的恢复范围

目前是 Linux、LLVM full、同进程同 runtime generation 的 live WASIp1 capsule。准确恢复受管匿名文件与 guest 环境/绑定；普通外部资源只保留绑定。没有回滚真实文件路径、目录变更、已经输出的数据、stdin 消耗、pipe/socket 缓冲、网络消息、时钟/随机事件或 kernel inode/time metadata；未实现持久化 checkpoint 重建、deterministic replay，以及完整 Wasm 世界与 WASI 的联合原子恢复 dispatcher。后续可通过虚拟文件系统/虚拟 stdio/事件日志扩展。

所有操作继续要求真正的 cooperative stop。ASM debugger 的 Wasm 上下文边界没有扩大；这些接口不允许通过 ASM 调试 VM/host 本身。

后续四 OS 修复及最终原生结果见 [2026-10-05 跨 OS 报告](wasip1_checkpoint_os_test_report.md)。以上保留当时的 Linux 测试计数。
