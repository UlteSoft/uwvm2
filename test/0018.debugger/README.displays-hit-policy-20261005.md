# DBG display、命中策略与完整回归补充，2026-10-05（R14）

本轮在 uwvm2 和 uwvm2-ros 同步补齐 CLI 自动显示、原子断点忽略策略，并修复 DAP 错误接受未实现断点策略的问题。四组现有路径再次回归通过。**完整 GDB/LLDB 体验和所有 Wasm 特性的任意中间状态调试仍未完成。** 前一轮 R10 记录保留在 [README.complete-check-20261005.md](README.complete-check-20261005.md)。

## 新功能与修复

### 自动显示只读源表达式

使用带 DWARF 的 C++ Wasm，在实际源停点执行：

~~~text
display counter
display parameter + 1
info display
disable display 1
enable display 1
undisplay 1
delete display
~~~

每个新观察到的真实单线程停点自动重读已启用的表达式；同一停点的 status、print 等查询不重复显示。在这样的停点新增表达式会立即求值。`display` 表达式中 Tab 补全当前变量和成员；裸 `display` 或 `info display` 列出设置。省略 enable/disable/delete/undisplay 的 ID 表示全部。

实现只保存表达式文本与设置，最多 32 项、每项 256 字节。每次通过当前 THREAD/STOP/frame 0 的正常源值查询重新检查权限，未缓存旧值、指针、帧或 native 捕获权。prepared 初始位置没有真实参与线程时不伪造求值；变量不在作用域、元数据缺失或该停点不支持源值查询时保留表达式并报告失败。

这是一项有限的 CLI 自动显示实现：尚无多线程/任意帧选择、词法作用域绑定、格式选项、DAP watch 对象或通用 stop hooks。表达式沿用已有只读数值/选择器子集，赋值和函数调用被拒绝。C++ 文本存储、输出、数字解析使用 fast_io。

### 创建断点时原子设置 ignore

~~~text
break-source 0 /absolute/example.cpp:42 ignore 3
break 0 1 7 ignore 3
info breakpoints
~~~

忽略前三次匹配的可执行 safe-point 访问，第四次开始停止；之后每次匹配仍停止。注册与 ignore 安装在同一 breakpoint 锁内完成，避免先暴露普通断点再修改策略的窗口。同一目标显式重配保留 ID、重置 hits、启用断点并替换 ignore。已有 `ignore ID COUNT` 接口保留。

新 suffix 支持数字 Wasm 目标和 source path:line 目标；符号名目标的 suffix 尚未实现。命中计数是可执行 safe-point 访问数，不保证等于语言级函数调用次数。

DAP 的源断点与 Wasm 指令断点支持有限阈值：`hitCondition: ">=4"` 或 `">3"` 对应一个 `... ignore 3` 命令。N 为 unsigned 64-bit；`>18446744073709551615` 无可表示的更大命中数，明确拒绝。精确次数 `"4"`、`"==4"`、模数条件、非空 `condition` 或 `logMessage` 返回 `verified: false`，不再静默创建无条件断点。条件表达式与日志点本身仍未实现。

## SSH Linux 原 cgroup 的实际结果

普通版、ROS 均从 R14 冻结源码重新编译 main/runtime/host API 并链接。实际 .d 依赖、全部冻结源码及 20,945 项 SDK/工具/库输入在最后热更新/检查点回归后再次核对。最新 DAP Python 修复及测试使用独立哈希固定的 tools-r2 overlay，不把它冒充原始冻结文件。

| 路径 | 普通版 | ROS | 资格范围 |
|---|---:|---:|---|
| display / atomic ignore | 102 PASS | 102 PASS | 真实 C++ -O1 -g Wasm、PTY、instruction/unwind；实际第四次命中、重新设置第三次命中、作用域失败、禁用/删除、Tab、容量限制 |
| TUI / 动态补全 / 表达式 | 50 PASS | 50 PASS | 真实 DWARF 源/成员/符号/带空格路径；四组面板、调整大小、Ctrl+L、Ctrl+X A、终端恢复 |
| ASM 所有权边界 | 190 traps PASS | 190 traps PASS | 两策略各 95 个 genuine native traps；i32/i64/f32/f64/v128 投影、可见指令、高位屏蔽、隐藏 VM 指令、栈/宿主内存及伪造/失效身份拒绝 |
| Wasm 3.0 入口冒烟矩阵 | 56 PASS | 56 PASS | 28 功能组 × 两策略；实际执行、入口停点七种查询、步进/opcode 跟踪；**不是所有中间状态覆盖** |
| WASIp1 CLI/broker/DAP | 176 PASS | 176 PASS | args/env/fd/rights、追踪、变更与受限检查点；新增两策略各三种真实 DAP 拒绝，断点表保持不变 |
| 热更新 | 10 PASS | 10 PASS | validator 拒绝、旧代号/旧断点、新代码执行；普通 run 预设 control-fd 后中途暂停/替换/恢复；活跃目标拒绝 |
| 检查点继续执行 | 两策略 PASS | 两策略 PASS | 实际局部值/i31/操作数恢复、原 owner 退休前拒绝、两次恢复继续结果、不会重放既往 guest 写入 |
| 最新 DAP 单元回归 | 189 PASS | 189 PASS | 协议 DATA 检查，含新增七个命中策略用例；不能充作 native 平台资格 |
| displays module 编译 | PASS | PASS | 真 fast_io BMI、叶子 partition、最小 primary 与 consumer；**仅编译，不是完整 controller module 图运行** |
| ROS 模式入口 | 不适用 | 12 PASS | help all、删除模式拒绝、int/full 与 LLVM/full 执行、dbg/int 拒绝 |

R12 两仓库组件批次共 30 个步骤通过（含显示设置、源表达式及编辑器回归）；其 DATA 测试与 R14 实际停点结果分开记录。R14 最后完整回归批次 33 个命令、热更新/检查点批次 12 个命令及最后输入审计 2 个命令全部通过。

ASM 正向显示范围仍较窄：普通版每策略累计 1 个可见指令行、452 个隐藏行，ROS 每策略 3 个可见行、450 个隐藏行。测试要求至少一项真实合法可见指令，并验证不应公开的内容被拒绝；它没有证明完整 native 汇编显示、跨调用 nexti 或原生 GDB 等价。branch formatter 的 DATA 阳性也不能冒充真实分支目标运行资格。

检查点测试明确输出 `complete_instance_restore=false`，不声称所有实例状态、文件偏移、管道、socket 或外部 I/O 能完整回滚。

## 尚未完成与平台边界

语言能力清单仍有 14 个 implemented、33 个 partial、39 个 missing、1 个 separate_level 和 1 个 prohibited_by_scope。这些是有限分类数，不能换算为完整语言调试支持率。`display_expr` 改为 partial；其他语言编译器路线不继承本轮 C++ 的实测资格。

- 语言条件断点、until/advance、watch/value history、完整运行时类型模型和自定义 pretty-printer 仍有缺口。
- ASM 跨调用 nexti、返回/调用者展开、完整 guest native stack 尚未完整取得资格；缺少证明时必须拒绝，不能借 VM 的指令、native 栈或内存扩大可见性。
- Wasm 3.0 本轮已覆盖各组入口与公共查询，不能宣称任意深度调用栈、GC/EH/SIMD/原子等全部中间状态的调试/替换/脚本已穷尽。
- 这次实际 native 运行平台是 Linux x86_64。公共解析、投影与界面代码可共享，但 Windows/macOS/FreeBSD、aarch64、musl 等未在本轮重新运行。

具体清单：[language_debug_capabilities_20261004.json](../0017.runtime/language_debug_capabilities_20261004.json)。接口说明：[README.dap.md](../../tools/debug/README.dap.md)。

## 失败记录与资源约束

保留了 R11 FastIO view 编译错误、R12 FastIO 条件输出类型错误、R13 重复 switch case 编译错误及修复记录；R14 两版最终完整编译链接成功。R14 ROS runtime 首次编译因共享 cgroup 压力被保护器停止，重试通过，没有把中止算成 PASS。display harness 首次错误地期望 prepared 位置执行查询，修正为要求无真实参与线程时不伪造读取后重跑通过；产品行为没有为测试降低边界。

64 GiB、swap 0、原 cpuset 和原 lane authority 未修改。最终所有本任务根/后代 PIDFD 已退休，没有接管或向其他任务发送信号。OOM=60/kill=12 为已有累计值，本轮未增加。并发其他任务仍可在 cgroup 内运行。

## 证据定位

远端产品/私有测试记录：`/tmp/uwvm2-dbg-complete-20261005-r14-products-macromodel`。
冻结源：`/tmp/uwvm2-dbg-complete-20261005-r14-sources-macromodel`。
最新 Python overlay：产品目录下 `tools-r2/{uwvm2,uwvm2-ros}`。

关键文件为 `final-summary-r14.json`、`current-input-proof-after-tests.json`、各批次 `receipts.json` 和逐项 results/日志。副本位于本机 `/Users/liyinan/.codex/state/uwvm2-dbg/20261005-r14/`；压缩证据包 SHA-256 为 `f3ab5aea3f3d069e58e04a566dcb76fc9371ad8966a288af6f6fa4f7e5d951a2`。失败尝试仍在对应私有目录，不清空历史。

产品 SHA-256：

- uwvm2：`6a128ebaa766a338c945856389423b611b168a2e793f28ff8c0d5156553ab36e`
- uwvm2-ros：`611024fdee28e2f10ac1c7d28371adcc7a2d0611aabee6b0fc0664f7cdfe880f`

收尾时新增 display/ignore 相关其他文件与最新 Python 输入仍匹配资格记录；controller.h 随后由另一 agent 增加了 DWARF compilation-directory 路径重解析。本轮新增 ignore 安装改动保留，但此后追加的重解析实现不属于 R14 冻结测试资格。未覆盖或撤销该并发改动；详见本机 local-qualified-input-comparison.json。能力清单中其他语言的并发资格记录也各自保留，未整文件覆盖同步。其他后续变更不自动继承此资格。
