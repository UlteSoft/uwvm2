# DAP 状态发布回归（2026-10-06，R38）

此前 `pause`、`continue`、各层单步及部分 REPL 命令先发送成功响应，
再解析运行时返回的停止状态。如果解析发现无效 native PC、错误 stop ID
或相互冲突的状态，异常处理会为同一个 `request_seq` 再发送失败响应。
`continue` 还可能提前发送 `continued` 事件。

两仓 `tools/debug/dap_adapter.py` 现在先解析状态，再发送响应及事件。
解析失败时清除旧停止引用，只发送一次失败响应；成功时仍保持响应在事件
之前。已解析的快照直接提交，不再二次解析。真实退出仍按顺序产生
`exited`、`terminated` 事件，源代码 watch 等标量回复不进入此路径。

`test_dap_status_publication.py` 通过真实 `Adapter.handle` 和 DAP 消息封装
检查 15 条请求路径，包括 language/source、Wasm、native 三层单步及 REPL。
每仓覆盖 60 个错误状态组合和 30 个正常停止/退出组合，并检查旧 frame、
scope 引用失效。这些使用模拟 broker 回复，属于协议回归，不是实际 JIT
执行或 ASM 边界验证。修复前的 45 个错误组合全部复现了重复响应。

SSH Linux 原有 64 GiB、禁用 swap 的 cgroup 中，两仓各 252 项 DAP 单元
测试全部通过，无跳过项。完整发现测试需要 WASIp1 测试的 native packet
输入：本轮传入经过 SHA-256 核验的已有 Linux formatter 输出，同时保留
原始编译及捕获记录。最初未传入该必需输入的发现测试失败，记录予以保留；
该次没有执行后续 native 用例。

另有 8 个本轮实际 LLVM JIT DAP 用例通过：两仓分别运行 instruction、
unwind 策略下的递归返回和不返回超时用例。合计验证 12 次实际父调用返回、
84 次当前 native 寄存器只读查询、64 次排队旧引用拒绝、4 次真实超时、
24 次 native code 内存访问拒绝及 4 次根调用返回拒绝。4 个超时用例最后
真实退出，退出码为 0，并收到实际 DAP 退出事件。当前 `$pc` 可查询，
`$sp`、`$fp` 不暴露；ASM 调试仍限于 Wasm JIT 上下文。

本轮仅修改 Python adapter，复用 R37 已完整核验的 full interpreter / full
LLVM 二进制。资格核验重新检查其 C++/SDK 输入、编译依赖闭包、产品哈希
和本轮冻结的 Python 测试输入。未新增 SDK 副本。原始 Popen/PIDFD 保护器
负责 cgroup、资源与进程退出核验，没有接管或终止其他任务。

Linux 证据目录：

```
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35
  dap-status-r38-v2/manifest.json
  packet-inputs-r38/provenance.json
  packet-inputs-r38/native-capture-qualification.json
  evidence-status-r38-baseline/
  evidence-tests-r38-v1/
  evidence-tests-r38-v2/
  completion-summary-r38.json
```

范围为已核验快照的 Linux x86_64 native 测试；后续并行修改、其他 OS/ISA、
实际 IDE 界面和全部 Wasm 3 特性覆盖不在本报告范围内。
