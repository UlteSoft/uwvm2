# R40：frame、source 与普通 Wasm locals 的停止状态寿命

两仓已同步修复普通 `Wasm locals`、`scopes`、`source` 接受过期引用的问题。SSH Linux 现有 64 GiB、swap=0 cgroup 中验收通过。

原先，普通 locals 直接发送 `locals THREAD`，没有验证创建 scope 时的停止状态。另一控制操作推进 VM 后，旧 `variablesReference` 可能读到新状态；旧 frame 的 `scopes` 和 Wasm `sourceReference` 也可能继续返回成功。真实 LLVM JIT 基线已复现：stop 2 的 frame 在外部推进到 stop 3 后，`scopes` 仍返回成功。

现在：

- `scopes`、`source` 在发布缓存 frame 的内容前重新读取完整 `status`，核对原停止状态与 frame 寿命。
- 普通 Wasm locals scope 绑定创建时的停止状态，查询按 `status → locals → status` 执行。复制途中状态变化时，不发布局部变量数据。
- 同一 PC 的新 stop、切换到 native 停点、其他参与线程变化、running、exited、closed、畸形状态都会拒绝旧引用。
- 获取新的 `stackTrace` 和 scope 后可直接读取；引用编号保持会话内唯一。
- native frame 仍只有寄存器 scope；旧 Wasm locals 不能借 native 停点重新绑定。ASM 的 SP、FP、宿主内存能力仍不可读。

可以按以下顺序观察修复：在 Wasm 停点获取 frame 和 locals scope；从控制通道执行 `step wasm THREAD`；再用旧 frame 请求 `scopes` 或旧 scope 请求 `variables`，收到失败响应；重新请求 `stackTrace` 和 `scopes` 后，locals 正常返回。

| 验证 | 结果 |
| --- | --- |
| UWVM 完整 DAP 单元测试 | 269 项通过，0 跳过 |
| ROS 完整 DAP 单元测试 | 269 项通过，0 跳过 |
| 新增寿命回归 | 5 个方法；旧实现有 36 个失败子用例 |
| 真实 JIT 新用例 | 两仓 × instruction/unwind，共 4 个通过 |
| 原有真实 ASM 返回、超时回归 | 两仓 × 两种策略 × 两类，共 8 个通过 |

新增真实用例使用 VM 中的 i32 local=17。每个用例验证 `scopes`、`variables`、`source` 的旧引用拒绝，以及复制 locals 后真实推进 VM、随后拒绝发布数据；总计 16 次真实拒绝。每次拒绝后重新获取的引用均可用。结束时通过真实 Wasm global 让循环退出，等待原 broker/guest 进程自然结束，退出码为 0。

新增脚本直接调用实际 `Adapter.handle` 并读取其 DAP 帧，连接真实认证 Unix broker；它刻意不运行 adapter 的空闲轮询，让外部推进与请求处理的顺序可重复。状态、locals 和步进回复全部来自实际 VM。原有 8 个 ASM 用例另外覆盖真实 DAP stdio、84 次成功的 native watch、原生返回与超时后引用失效。原有脚本按原 Popen 清理 broker/guest，可返回 130；这部分不作为 guest 进程自然退出证据。

Linux 运行配置：UWVM 为 `-Rcc jit -Rcm full`，ROS 为 `-Raot`，均使用完整 LLVM JIT、`-Rct 0`、native-unwind，分别测试 instruction 和 unwind 栈策略。原有 full interpreter + full LLVM 的 R37 C++ 产物复用；本轮复核了 30,930 条源码/SDK 输入及 8 份编译依赖闭包，没有重新构建 C++ 或复制 SDK。并行任务的其他 C++ 改动不计入本轮构建覆盖。本轮生产修改均在 Python DAP adapter，没有新增 C++ I/O。

最终测试快照为 `scope-r40-v2`；新增真实脚本的自然退出等待修正为 `scope-r40-v3`。生产 adapter 与其他测试输入相同，只有该脚本变化，因此仅重跑对应 4 个真实用例。初次验收误把原有脚本主动清理时的 broker 返回码也要求为 0；已按原脚本清理约定修正，保留失败验收记录。

资源记录：独立 inode 占用 3,503,751,168 B（约 3.3 GiB），磁盘余量 113,834,053,632 B（约 106 GiB），测试进程监测 RSS 峰值 133,873,664 B（约 128 MiB），cgroup 全部 memory.events 为 0。原共享锁、Popen/PIDFD、进程出生标识和 cgroup 检查保留；所有本轮进程已回收，没有接管或清理其他任务。

验收与证据位于 SSH Linux：

```
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/completion-summary-r40.json
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/primary-evidence-r40.tar.gz
```

本机恢复记录和原始证据副本：

```
/Users/liyinan/.codex/state/uwvm2-dbg/20261006-r40-frame-scope-lifetime/
```

本轮测试快照的 adapter SHA-256：`652e2b7d4b5c1a0ff4cc3ad269fc4fc8c61ee4d68503c14cd823a12ca0cbfdc3`。

保存时另一任务补充了 language 表达式中整型类型说明符的排列别名，两仓当前 adapter SHA-256 为 `a379c266bfed36607165c06d38aca11b6ca26166deef572f966c1c8dc5ba3ef5`。整份 `Adapter` 类及本轮其他改动文件与测试快照逐字一致；保留该并行变更，但不将其新增解析行为计入 R40 验收。

本轮实际验证平台为 Linux x86_64；不据此声称完成其他平台或 IDE UI 验收。
