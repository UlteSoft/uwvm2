# R41：普通 Wasm locals 分页与 Linux 首条命令竞态

uwvm2 与 uwvm2-ros 已同步修复并在 SSH Linux 原共享 64 GiB、swap=0 cgroup 验收。

普通 `Wasm locals` scope 现在支持 DAP `variables` 的 `start`、`count` 和 `filter`。原先忽略这些参数，并只能显示有限的诊断 locals 前缀；现在读取 typed producer 的真实原始 local 索引，跨页后仍保留类型及 GC 对象展开。每次复制前后核对完整停止状态，整份结果发布前核对 module、generation、总数和页长度。状态改变、短页或错误身份会拒绝整份响应。

从当前 `stackTrace` 的 frame 请求 `scopes`，取得 `Wasm locals` 的引用后，例如：

```json
{
  "command": "variables",
  "arguments": {
    "variablesReference": 1001,
    "start": 129,
    "count": 8,
    "filter": "indexed"
  }
}
```

这里的 `1001` 仅为示例，必须使用当前 scope 实际返回的引用。响应名称为 `local 129` 到 `local 136`。`filter: "named"` 返回空数组；`start` 超过总数也返回空数组。省略 `count` 或设为 0 时返回剩余 locals，每份 DAP 响应最多 1024 项，超过此限制须显式分批请求。每次底层借用最多 64 项；若值或对象复制预算不能填满该页，会要求缩小页数，不把诊断截断伪装成成功的完整页面。这是普通 scope 的分页，不更改其他显式 Wasm 查询接口的边界。

`variablesReference` 和 GC 展开引用绑定原 stop；外部 `step wasm THREAD`、继续执行或停止状态变更后，旧引用失败。重新取得 frame/scope 后可查询新状态。分页和对象展开不提供宿主 `memoryReference`。

分页语义参考 [DAP VariablesArguments](https://raw.githubusercontent.com/microsoft/debug-adapter-protocol/main/debugAdapterProtocol.json)；此实现为原始 Wasm 索引接受非负 u64 `start`，并对响应施加上述有限预算。

另一个修复在 Linux `secure_server.py`：创建 VM socketpair 后、启动 VM 前，在继承给 VM 的端点启用 `SO_PASSCRED`。旧实现只由 VM 稍后启用，首条命令若先排队，Linux 无法保留真实发送者身份，VM 按原身份检查拒绝并关闭通道。提前启用让首条命令也带有原 broker 凭据；VM 的 PID/UID/GID、PIDFD 和密封端点检查继续执行。

新增真实回归用 host launch gate 等待通道出现 POLLIN，**不读取命令**，随后 exec 原 C++ product；因此明确保证首条命令在 VM exec 前排队。相同 adapter、夹具与 gate 下，旧 broker 返回 `broker channel closed`；修复后两仓均能处理首条 `status`，暂停和修改 Wasm global，然后自然退出 0。无需增加初始化延时或重试，也无需减弱身份检查。

| 验证 | 结果 |
| --- | --- |
| 两仓完整 DAP 单元测试 | 各 276 项通过，0 跳过 |
| 原 locals 实现的分页回归 | 6 个方法，27 个失败子用例；新实现另加入 probe 身份检查 |
| 真实 161-local + GC 夹具 | 两仓 × instruction/unwind，4 个通过 |
| frame/scope/source 寿命回归 | 两仓 × 两种策略，4 个通过 |
| ASM 返回与超时回归 | 两仓 × 两种策略 × 两类，8 个通过，84 次 native watch |
| 首条命令先于 VM exec 排队 | 旧 broker 真实失败；修复后两仓共 2 个通过 |

共 18 个真实 JIT/broker 用例。161-local 夹具验证第 129 个之后的索引、跨 64 项边界、末页与空页、省略/零 count、local 160 的 GC 字段值 42，以及旧 scope/GC 引用拒绝。4 个 locals、4 个寿命及 2 个启动用例等待原 broker/guest 自然退出 0；8 个原有 ASM 脚本遵守既有原 Popen 清理约定，broker 返回 0 或 130。ASM 回归继续要求 SP、FP、宿主栈及内存能力不可见。

本轮复用已取得资格的 R37 full interpreter + full LLVM C++ 产物，重新核对 30,930 条源码/SDK 输入和 8 份编译依赖闭包。没有重新构建 C++、复制 SDK，或新增 C++ I/O。新增生产修改仅为 Python adapter 和 Linux broker。验证平台为 Linux x86_64；共享分页实现不等于其他 OS/ISA 或实际 IDE UI 已取得验收。

Linux 重启后复用其他任务已经恢复的同一个 cgroup，只重新绑定本任务保护器的 boot、keeper PID 与出生标识。原共享锁、64 GiB/swap=0 限额、Popen/PIDFD 及身份检查保持原样。初次依赖验收把重启后的身份更新仅按 PID 比较，遗漏出生标识，已修正验收脚本后通过；失败记录保留。运行中的外部任务未被接管、信号或清理。

验收时本任务目录独立 inode 占用 3,525,206,016 B（约 3.3 GiB），磁盘余量约 99.5 GiB，测试监测 RSS 峰值约 129 MiB，cgroup memory.events 全为 0。证据打包另占有限的 MiB。

原始验收：

```
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/completion-summary-r41.json
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/primary-evidence-r41.tar.gz
```

本机恢复记录与证据副本：

```
/Users/liyinan/.codex/state/uwvm2-dbg/20261007-r41-wasm-local-pages/
```

测试快照 `local-pages-r41-v4` 包含 78 个文件；adapter SHA-256 为 `0cf848b3977e7a64e4d07f6e7c75cc3f50a9a10cc99c58637bedb2c02b4113d5`，Linux broker 为 `a46bbec49c78734b702d98b20aaab4033d3dc7ec1e2999a2eb5063fc5bb612cc`。
