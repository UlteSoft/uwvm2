最新 R14 补充见 [README.displays-hit-policy-20261005.md](README.displays-hit-policy-20261005.md)；下文保留 R10 历史记录。

# DBG 完整回归与本轮补齐，2026-10-05

本轮在两个仓库同步修复具体缺口，四组现有路径的下列回归通过。**这不表示所有 GDB/LLDB 功能或所有平台已全部实现。**

## 实现与修复

- ASM/DAP：支持当前 C++ 生产者的 architecture/word-bits 寄存器协议，修复 32 位 PC 格式；完整检查支持的十种架构名称、保留寄存器、数值位宽和旧协议。SP/FP/TLS/控制寄存器及未证明的高位仍不可读取。只接收当前 Wasm JIT 停点的投影，不引入宿主内存或 VM 调试入口。
- language：新增 `0b`、`0o`、前导零八进制、数字分隔符及共享只读数值子集中的 `&^`。使用 fast_io 的 bin/oct/hex/dec `parse_by_scan`；拒绝非法数字、分隔符和超过 64 位的输入。`&^` 使用乘法组优先级，完整 Go 类型语义仍不在本轮资格范围。[Go 规范](https://go.dev/ref/spec#Operator_precedence)
- ROS：清理启动代码中已删除的 auto/lazy/tiered/debug-interpreter 分支与配置引用，修复当前源码的真实编译错误。ROS 保留 uwvm-int full 和 LLVM full；普通仓库保留其已有模式。
- Wasm 3.0 测试：错误、unavailable、缺少状态头、过期 stop、错误线程/模块、缺失或重复数据行均不再被计作支持。执行和状态查询分别记录，并记录查询判定器哈希。

新增 C++ I/O/格式化/数字解析使用 fast_io。没有覆盖或撤销其他 agent 的改动。

## SSH Linux 原 cgroup 的实测

完整产品从冻结 R10 源码重新编译 main/runtime/host API 并链接；输入证明在运行前后核对 SDK、工具、库、源码和三个编译单元的实际依赖。每个仓库独立构建，未混用旧 runtime 对象。

| 路径 | 普通版 | ROS | 实际范围 |
|---|---:|---:|---|
| R9 组件构建/运行步骤 | 39 PASS | 39 PASS | 表达式、DWARF、编辑器、Wasm 状态/变更、检查点编解码、ASM 投影 |
| R9 DAP 单元测试 | 180 PASS | 180 PASS | 协议 DATA 回归；不是实际平台/停点资格 |
| 当前 ASM 边界 | 190 trap PASS | 190 trap PASS | 两种栈策略；真实 i32/i64/f32/f64/v128、安全可见指令；宿主地址、越界、伪造/失效身份拒绝 |
| 当前 TUI/补全/表达式 | 50 PASS | 50 PASS | 真实 `-g` C++ Wasm、PTY、两种栈策略；包含四个新增数值表达式 |
| 当前 Wasm 3.0 矩阵 | 56 PASS | 56 PASS | 28 功能组 × 两种策略；执行、入口停点的七种状态查询、步进及实际 opcode 跟踪 |
| 当前 WASIp1 集成 | 170 PASS | 170 PASS | 实际 CLI、认证 broker/DAP、args/env/fd/rights、追踪、变更与受限资源检查点 |
| 当前热更新 | 10 PASS | 10 PASS | 有效替换、旧代号/非法体拒绝、新代码断点；普通 run 启动时预设 `--debug-jit-control-fd`，中途暂停/替换/恢复；活跃帧拒绝 |
| 当前检查点继续执行 | 两策略 PASS | 两策略 PASS | 实际局部值、i31、操作数恢复；不重复既往 guest 写入；伪造、结果缓冲区、失效代码拒绝 |
| 查询判定器单元回归 | 6 PASS | 6 PASS | DATA 测试，验证上述误判不会被计入支持 |
| ROS 模式入口 | 不适用 | 12 PASS | help all、已删除模式拒绝、int/full 和 LLVM/full 执行、dbg/int 明确拒绝 |

十种架构的 11 种位宽/布局组合均用真实生产投影和 C++ formatter 输出交给 DAP 解析；这些属于跨架构协议 DATA 检查，**不等于相应架构或 OS 的 native 调试实测**。

Wasm 矩阵是功能冒烟覆盖，入口查询不会证明每个特性所有中间状态都已完整调试。完整语言层回归本轮实际生产者是带 DWARF 的 C++，没有把历史其他语言的结果充作当前结果。

## 尚未完成或未取得资格

- ASM 跨调用的 `ni`、native 返回/调用者展开和完整 guest native stack 体验仍有能力边界；缺少证明时保留停点并拒绝，不能扩展到 VM 指令/栈/内存。
- 语言条件断点、完整语言类型/运行时模型、自定义 pretty-printer 等仍有缺口；见 `test/0017.runtime/language_debug_capabilities_20261004.json` 的具体分类，不能称完整原生 GDB/LLDB 等价。
- 当前检查点继续执行明确输出 `complete_instance_restore=false`。外部文件偏移、管道、socket 等不能据此宣称完整回滚；WASIp1 严格资源回滚在不满足条件时拒绝。
- 公共解析、投影和界面代码由两个仓库共享。本轮真正认证的是 Linux x86_64 native；Windows/macOS/FreeBSD、aarch64、musl 等未在本轮运行。

## 失败与恢复记录

R8 独立表达式编译缺少 fast_io::array 声明及检查点测试缺少输出路径已修复，R9 对应组件通过。R9 普通 runtime 两次因共享内存压力由保护器中止；R10 两个产品最终完整编译链接通过。R9 ROS main 暴露删除接口的真实编译错误，本轮 ROS 启动分支清理后通过。首次矩阵/WASIp1 启动缺少已核验 CPU 集合环境变量、首次 ROS 模式测试使用默认 help 分组，均是测试启动问题，修正后用新输出目录重跑并保留失败记录。

原 64 GiB/swap0/cpuset 限制保持不变。所有本轮任务的根及后代 PIDFD 已退休；没有接管或发送信号给其他任务；最后成功批次没有新增 OOM/kill。

## 原始证据

远端私有目录 `/tmp/uwvm2-dbg-complete-20261005-r10-products-macromodel`，源码快照 `/tmp/uwvm2-dbg-complete-20261005-r10-sources-macromodel`。组件证据在对应 R9 私有目录。`final-regression-summary.json`、各批次 `receipts.json`、逐项回复/命令/日志及依赖证明保存了上述结果；并发 agent 后续改动不自动继承本轮资格。
