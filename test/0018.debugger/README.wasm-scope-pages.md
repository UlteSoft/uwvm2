# R43：七类 Typed Wasm scope 的标准 DAP 分页

uwvm2 和 uwvm2-ros 同步补齐 `variables` 对 Typed Wasm locals、operand stack、saved if parameters、control stack、handler clauses、globals 和 table 0 的分页。先前标准 scope 把省略或零 `count` 转换为 64，并拒绝 `count > 64`。

每份标准响应现在最多返回 1024 项，通过多次最多 64 项的真实底层查询组成。省略或零 `count` 读取从 `start` 开始的所有剩余项；超过响应预算时要求显式分批。超过总数的 `start` 返回空数组。七类 scope 都是原始索引集合，`filter: named` 只刷新停止状态、返回空数组，不借用状态值。非法参数在 broker I/O 之前拒绝。分页语义参考 [DAP 官方 schema](https://raw.githubusercontent.com/microsoft/debug-adapter-protocol/main/debugAdapterProtocol.json)。

先用当前停点的 `stackTrace` 取得 frame，再用 `scopes` 找到所需 scope 的 `variablesReference`。例如：

```json
{
  "command": "variables",
  "arguments": {
    "variablesReference": 1002,
    "start": 63,
    "count": 80,
    "filter": "indexed"
  }
}
```

`1002` 只是示例，必须替换为当前实际引用。160 项集合返回第 63 到 142 项；`start=157,count=0` 返回最后三项；省略 `count` 返回全部 160 项。显式 `uwvm/wasmState` 和 CLI 查询仍保留每次最多 64 项的接口边界。

每份响应先重新查询总数，然后逐页重新借用原 participant、module、frame、selector 和 table。核对完整停止状态、module、runtime epoch、总数、页长度及 operand snapshot notice；当前 module 的 epoch 还须匹配停止位置。任一页变化、不可用或过短，整份请求失败，已复制的前缀不会发布。旧 scope 不能绑定新停点或 native 停点。

分页后的 GC roots 保留原 local/global 索引，可以继续展开字段。它们和 scope 都受原停止状态约束；引用不是宿主地址。operand scope 的值仍是最后一次 Wasm safepoint 的快照，scope 名称继续提示它可能不同于当前 native 状态。control 和 handler 只返回词法元数据，不返回 native catch 对象、SP、FP、宿主栈或内存能力。

本轮新增 C++ I/O 为零，生产变更只在 Python adapter。C++ 状态生产者既有格式化继续使用 fast_io。

真实夹具 `fixtures/debug_wasm_scope_pages_dap.wat` 在进入循环前一次性初始化 locals、globals、funcref table 及 GC struct；用 160 项 if 参数和深层 block/try_table 构成 operand、saved、control 和 handler 状态。GC 对象在循环前分配一次，避免运行期间反复分配。测试脚本安排真实外部 Wasm 步进来打断复制，随后重新取得相同循环 safepoint，检查恢复后的读取。

| 真实停点的集合 | 项数 |
| --- | ---: |
| Typed locals | 161 |
| Operand stack | 160 |
| Saved if parameters | 160 |
| Control stack | 165 |
| Handler clauses | 160 |
| Globals | 161 |
| Table 0 | 161 |

两仓各 295 个单元测试通过，0 跳过；32 条验收命令全部通过，包含 26 个真实 JIT/broker 用例。新增七类 scope 的两仓 × instruction/unwind 共 4 个用例通过，验证 35 个有限页面、每类的省略/零 count、末页和空页、GC 字段、外部步进后旧 scope 和 GC root 拒绝、每类在复制实际第一页之后外部步进并拒绝整份响应、恢复同一 safepoint 后重新读取。共 64 次真实新 scope/root 旧状态或复制途中拒绝。

22 个既有用例再次通过：GC 成员 4 个、普通 locals 4 个、frame/scope 寿命 4 个、ASM finish/timeout 8 个、首条命令提前排队 2 个。ASM 回归包含 84 次真实 native watch，继续验证 SP、FP、宿主栈/内存能力不可见。18 个非 ASM 用例等待原 broker/guest 自然退出 0；8 个原有 ASM 用例保留原 Popen 清理约定，broker 返回 0 或 130。

修正后的旧 adapter 基线有 32 个失败子用例。真实旧 adapter 在同一夹具上成功读取第 129 项开始的 8 个 locals，随后 `start=63,count=80` 返回 `bounded Wasm state indices required`；只替换 adapter 为 R42 版本，其他 89 个输入与最终快照相同。

新大夹具连接前等待 20 秒 JIT 启动，生产 broker 的 6 秒 RPC 超时、凭据、PIDFD 规则均未调整；排队首命令使用独立原有夹具。首次大夹具试验在编译完成前发送命令而超时；随后脚本恢复停点时误对已暂停 VM 发出 pause，改为重设循环断点后继续。单元模型最初的 globals 后缀和大索引 i32 值也已修正。所有中间失败及记录均保留，最终 v6 完整通过。

再次核对 30,930 条源码/SDK 输入和 8 份编译依赖闭包。复用已取得资格的 R37 full interpreter + full LLVM C++ 产物，没有重新构建 C++ 或复制 SDK；当前验收是 R43 adapter 与该冻结生产者的互操作。其他 agent 的后续 C++ 变更不继承此资格。实际平台为原共享 Linux x86_64 64 GiB、swap=0 cgroup；一份 Python 实现共用，无其他 OS/ISA、实际 IDE UI 或完整 Wasm 特性/语言模型验收声明。

资格核验时本任务目录独立 inode 占用 3,593,179,136 B（约 3.35 GiB），磁盘余量约 96.0 GiB，测试监测 RSS 峰值约 720 MiB，cgroup memory.events 全为 0。原共享锁、64 GiB/swap=0、出生标识及原 Popen/PIDFD 检查保持，所有本轮进程已回收，其他任务未接管或发送信号。

原始验收与证据：

```
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/completion-summary-r43.json
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/primary-evidence-r43.tar.gz
```

本机恢复记录与证据副本：

```
/Users/liyinan/.codex/state/uwvm2-dbg/20261007-r43-wasm-scope-pages/
```

最终快照 `scope-pages-r43-v6` 包含 90 个测试输入。adapter SHA-256：`248938e2d2503658c0d06f4023bf482c1b818e1020de4b8c093dc99ae13d0256`。
