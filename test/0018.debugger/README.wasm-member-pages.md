# R42：Wasm GC 成员分页与客户端数量提示

uwvm2 和 uwvm2-ros 同步补齐标准 DAP `variables` 的 GC 成员分页。原先省略或零 `count` 被转换为 64，较大数组仅返回前 64 项，`count > 64` 被拒绝。真实旧 adapter 已在 161 项数组上复现：第 129 项开始的 8 项可读取，随后 `start=63,count=80` 返回 `bounded original-index GC member page required`。

现在可返回跨多个底层页面的完整请求：每次实际 root/path 借用最多 64 项，每份 DAP 响应最多 1024 项。省略或零 `count` 返回剩余成员；超过响应预算时要求显式分批请求。超过成员总数的 `start` 返回空数组，`filter: named` 返回空数组且不借用成员，原始成员索引始终保留。显式 `uwvm/wasmMembers` 接口仍使用每次最多 64 项的原有边界。

从当前停点的 GC 变量取得实际 `variablesReference` 后：

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

`1002` 仅为示例。161 项夹具返回 `[63]` 到 `[142]` 的 80 项，值为 1063 到 1142。`start=158,count=0` 返回最后 3 项；省略 `count` 返回全部 161 项。同一数组既可从 local 直接展开，也可经 struct 的 `[0]` 字段展开。CLI 的有界嵌套查询仍可使用：

```text
members locals 1 0 0 0 2 129 8 0
```

其中 participant、local root 和路径必须取自实际当前停点；示例选择 local 2 的字段 0，再读取数组第 129 项开始的 8 项。

对象变量现在包含精确的 `indexedVariables` 和 `namedVariables: 0`，便于支持分页的客户端按成员数分批请求；真实数组及经字段取得的数组均返回 161，外层 struct 返回 1。这些是显示数量，不授予新的读写权限。数量超过 DAP int32 范围时省略提示，原始数量和索引不截断。字段和客户端分页语义见 [DAP 官方 schema](https://raw.githubusercontent.com/microsoft/debug-adapter-protocol/main/debugAdapterProtocol.json)。本轮验证实际 DAP 回复，不声明已验收 IDE 的可视界面。

每份响应首先重新借用原 root/path，检查对象类型、成员总数和 root module；每页核对 module、runtime epoch、对象身份元数据、完整页长度及完整停止状态。回复中的稠密对象编号仅作显示，不成为下一次查询的输入。停点在任一页复制途中改变时，整份响应失败，已复制的前缀不会发布。旧对象引用和嵌套引用均不能绑定新停点；重新取得当前 roots 后可继续展开。对象展开保持只读，无宿主 `memoryReference`。

| 验证 | 结果 |
| --- | --- |
| 两仓完整 DAP 单元测试 | 各 285 项通过，0 跳过 |
| 原成员分页实现的回归 | 8 个方法，22 个失败子用例 |
| 新增模型覆盖 | 模块/类型/总数校验；int32 数量提示边界；跨页状态改变与短页拒绝 |
| 真实直接及嵌套数组分页 | 两仓 × instruction/unwind，共 4 个通过 |
| 普通 locals、GC 字段及 frame/scope 寿命 | 两仓 × 两种策略 × 两类，共 8 个通过 |
| ASM 返回和超时回归 | 两仓 × 两种策略 × 两类，共 8 个通过，84 次 native watch |
| 首条命令提前排队回归 | 两仓共 2 个通过 |

共 22 个真实 JIT/broker 用例。新夹具只在进入循环前分配数组和 struct，并用 Wasm 指令初始化成员为 `1000 + index`，避免运行期间重复分配。每个新增用例验证两种 root 路径、跨 64 项边界、末页与空页、完整 161 项、数量提示、外部步进后两个旧引用拒绝，以及复制实际第一页后外部步进、整份响应拒绝、重新获取引用后恢复读取。共 12 次真实 GC 旧引用或复制途中拒绝。新增脚本通过实际 `Adapter.handle` 的 DAP 帧与真实认证 broker/VM 交互，测试钩子仅安排真实外部步进。

14 个成员、locals、寿命及启动用例等待原 broker/guest 自然退出 0；8 个原有 ASM 用例使用既有原 Popen 清理约定，broker 返回 0 或 130。ASM 回归继续核对 SP、FP、宿主栈与内存能力不可见。

首次回归的输出目录转换导致成员夹具和旧 locals 夹具重名，旧用例启动失败；已修正命令生成，并预先检查 22 个 native 输出目录互不重复。v2 完整回归通过后新增成员数量提示，v3 再次执行全部回归；保留所有失败及中间记录。

本轮生产修改仅在 Python adapter，没有新增 C++ I/O。复用已取得资格的 R37 full interpreter + full LLVM C++ 产物，并再次核对 30,930 条源码/SDK 输入和 8 份编译依赖闭包；没有重新构建 C++ 或复制 SDK。实际平台为 Linux x86_64 原共享 64 GiB、swap=0 cgroup。其他 OS/ISA、实际 IDE UI、任意语言对象模型和所有 Wasm 中间状态不继承此验收。

验收时本任务目录独立 inode 占用 3,551,195,136 B（约 3.3 GiB），磁盘余量约 97.7 GiB，测试监测 RSS 峰值约 139 MiB，cgroup memory.events 全为 0。原共享锁、64 GiB/swap=0、出生标识及 Popen/PIDFD 检查保持；所有本轮进程已回收。证据归档另占有限 MiB。

原始验收与证据：

```
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/completion-summary-r42.json
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35/primary-evidence-r42.tar.gz
```

本机恢复记录与证据副本：

```
/Users/liyinan/.codex/state/uwvm2-dbg/20261007-r42-gc-member-pages/
```

最终快照 `member-pages-r42-v3` 包含 84 个测试输入文件，adapter SHA-256 为 `a29ed3f2b67570c1c53597d8cd40a3b2c06a1b7201e8a93f6d378c1c81fd35c8`。
