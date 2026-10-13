# Wasm 新查询与旧引用生命周期回归（2026-10-06，R39）

此前修改状态、执行脚本或发现新的停止状态后，第一次
`uwvm/wasmState` / `uwvm/wasmMembers` 查询会错误地失败，重试才成功。
原因是查询在刷新 status 之前保存旧 stop key，再要求新状态匹配这个旧值。

两仓现在先确认当前协作停止状态，再为新的逻辑 root 查询取得 stop key。
旧 scope、GC 对象展开则显式携带原 stop key：状态变更时，在读取 root
之前就拒绝，不能绑定到新停止状态。复制后仍再次检查状态。旧引用号码
不复用；运行中、已退出、native trap 和复制期间状态变更继续拒绝读取。
拒绝无效请求时，adapter 仍保守地清除全部缓存显示。

`test_dap_wasm_query_refresh.py` 覆盖 18 个协议组合，包括 Wasm/WASIp1
修改、脚本之后的首次查询，以及新停止状态、旧 scope/object 拒绝和复制中
状态变化。使用真实 Adapter.handle 和消息封装，broker 回复是模拟 DATA，
不能作为实际 GC/JIT 执行证据。修复前复现了 10 个失败子用例。

SSH Linux 原有 64 GiB、禁用 swap 的 cgroup 中，两仓各 264 项 DAP 测试
通过，无跳过。套件包含冻结时的 20 个 test_dap 文件；WASIp1 formatter
和 controller packet 测试使用经过哈希核验的已有真实 Linux 捕获记录，
保留其来源和编译记录，不将这些旧捕获算作本轮 native 执行。

本轮另通过 12 个实际 full LLVM JIT DAP 用例：两仓各运行 instruction、
unwind 策略的递归返回、原有超时及新增 GC 查询用例。新增
`fixtures/debug_wasm_query_refresh_dap.wat` 在 global 3 中保存真实 struct。
运行时逐次将其字段从 9 修改为 42、43，在同一 DAP 写入批次中分别测试
“state 在前、members 在后”和相反顺序。合计 8 次实际 GC 字段修改、
16 次首次逻辑查询成功、16 次旧对象/frame 引用拒绝，并验证新对象展开。
4 个 GC 用例最终真实退出，退出码为 0，收到实际 DAP 退出事件。

新增用例复用原有 `run_dap_native_finish_timeout.py`，加
`--wasm-query-refresh`。该选项为固定 GC fixture 显式启用：

```text
--wasm-feature-enable-gc
--wasm-feature-enable-function-references
--wasm-feature-enable-reference-types
```

最初的测试顺序错误和两次遗漏 feature 参数的失败记录均保留。修正后只
重跑新增 GC 用例；已通过的单元测试和原有 native 用例沿用同一 adapter
哈希下的原始记录。原有 native 用例未使用该选项。

本轮只改变 Python adapter 与测试。复用 R37 已核验的 full interpreter /
full LLVM 产品及原有 SDK；资格核验重新检查 C++/SDK 输入、编译依赖闭包、
产品哈希和两份冻结 Python 视图。ASM 仍只显示获准的 Wasm JIT 上下文，
不新增 host stack、SP/FP 或任意 host memory 读取。没有修改其他任务的代码、
文件或进程。

Linux 证据目录：

```text
/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/asm-dbg-r35
  wasm-query-r39-baseline/manifest.json
  wasm-query-r39-v2/manifest.json
  wasm-query-r39-v4/manifest.json
  packet-inputs-r39/
  evidence-wasm-query-r39-baseline/
  evidence-tests-r39-v1/
  evidence-tests-r39-v2/
  evidence-tests-r39-v3/
  evidence-tests-r39-v4/
  completion-summary-r39.json
```

测试范围为冻结快照的 Linux x86_64 native；后续并行改动、其他 OS/ISA、
实际 IDE 界面和全部 Wasm 3 特性覆盖不在本报告范围内。
