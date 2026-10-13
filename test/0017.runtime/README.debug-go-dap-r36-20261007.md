# TinyGo map/channel 的真实 DAP 验证 — R36，2026-10-07

两个仓库同步修复了 Linux broker 首条命令在完整 JIT 编译时过早超时，以及 Watch 返回整个控制台数据包的问题。实际首条暂停在一个最终短会话中等待了 9.018 秒；旧版固定 6 秒等待已实际失败。首次经过真实 peer/capability 鉴权的 VM 命令现在最多等待 120 秒，额度属于原 launch，重连不能续期；后续 VM 命令仍为 6 秒。客户端鉴权/普通命令仍用 8 秒，Linux 首条命令的客户端上限为 125 秒。客户端传输失败会关闭本连接。VM 回包超时、断开或格式/传输失败会退出原 launch，关闭原 endpoint/listener/capability，并按已有 finally 策略回收原 Popen guest，避免无请求序号的迟到回包回答下一客户端。普通客户端断连仍可重连。

真实 DAP stdio 的 Watch、Hover 和 Variables evaluation 对有限整数根返回单独数值、原类型和只读属性。例如 `len(box.Numbers)` 为 `3 / int`，`cap(box.Buffered)` 为 `4 / int`，nil map/channel 的合法查询为 `0 / int`。组合 `len(box.Numbers)+cap(box.Buffered)` 为 `7 / signed integer`，保留现有表达式引擎的类型名称，没有把它改称完整 Go 类型语义。adapter 声明 `supportsEvaluateForHovers`，新 evaluate 的 type 字段按客户端 `supportsVariableType` 协商。格式规则对照 [DAP EvaluateResponse](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#Types_EvaluateResponse)。未知/复合对象仍是有限不透明显示。

显示投影只发生在原有完整数据包验证和真实 source stop/frame/cohort 前后检查之后。它检查整数 extent、范围和 canonical decimal；没有额外查询或地址解引用。`variablesReference` 为 0，不新增 memoryReference、写入/展开/表达式权限。REPL 保留原控制台格式。C++ 查询/I/O 本轮未改，继续沿用 R35 的 fast_io 实现。ASM 的范围仍仅为 Wasm 产生的上下文。

测试使用原始 TinyGo 0.42.0 / Go 1.27.1 / LLVM 22.1.4 的 O0、wasm-unknown、scheduler=none、gc=leaking fixture。原始 Wasm validator、完整 DWARF verifier 和 producer receipt 沿用 R35，没有改写 Wasm、DWARF、源码路径或参数。六个编译器参数通过真实 DAP Source variables 给出 map/channel oracle，其余 expected values 来自原 fixture/Go 规范；22 个表达式在三个 evaluate context 中比较，每会话为 66 次比较。这些重复不是 66 个独立功能。

新入口 `test/0018.debugger/run_dap_tinygo_collections.py` 用真实 broker、实际 VM 和独立 stdio adapter。只有 Host 启动门等待首条真实鉴权 pause 已排队，再 exec 未修改 VM；门不读取命令或凭据，不修改 guest。本轮证明的是这个 Host 预先暂停的 attach 流程，不能据此声称一般未准备的快速程序 attach 或实际 VS Code UI 已完成。broker 启动用 `-m run`，不能把本地 console 的 `-Rdbg` 路径当成 inherited endpoint。普通仓库显式 `-Rcc jit -Rcm full`，ROS 使用其 full LLVM 产品，两者都禁用 LLVM cache。

每个会话核对原源码行的真实栈帧、opaque frameId、Source variables、六个参数、分页、三种 context 的 map/channel/nil/closed/directional 查询、八种非法表达式拒绝，以及排队的实际单步后旧 evaluate/scopes/variables 引用立即失效。原 guest 完成自己的编译器结果检查并自然 exit 0；启动包装器实际 wait 原 Popen guest，broker 和 adapter 返回 0，stdio 真实 EOF 被读尽。单元中的人工 formatter/进程 DATA 与这些实际 VM 检查分开统计。

| 最终版本检查 | uwvm2 | uwvm2-ros |
|---|---:|---:|
| 原产品实际 dependency pins 再核对 | 3446 | 3411 |
| 原记录的 shared-library file pins，前后匹配 | 108 | 108 |
| DAP/源码帧/显示/超时单元测试 | 82 | 82 |
| instruction + unwind 短会话 / 比较 | 2 / 132 | 2 / 132 |
| 超过 10 分钟实际 DAP 会话 | 61 | 61 |
| 持续值比较 | 4026 | 4026 |
| 持续秒数 | 606.703 | 605.146 |

最终合计 126 个实际会话、8316 次值比较，其中持续阶段为 122 个会话、8052 次比较。最终 8 个 guard job 均成功。本轮所有阶段另有 23 个成功 job、6 个失败 receipt 保留；失败包括最初 guard 路径闭包、旧 6 秒启动等待、测试准备文件/短 socket 路径和退出后多发 quit 的测试修正。全部已启动 roots 已 reap、descendant PIDFD 已退休，memory.events 均为零。另保留前一版本的两份 10 分钟测试记录，未混入最终版本的统计。

本轮没有重编 C++ 产品：固定复用 R35 新鲜 3 TU + link 的两份完整产品及原始 receipt，没有把它转换/冒充 R3 receipt。source ID 为 sha256:236c37b428d0a9f17c158ed52b8097dd9a449693ebd6a9890623e1b4676175cc、sha256:7ab8b0026feec0fcf33f4586335e2f56fb2c3911bdece88b884dbbf62461bc20；VM SHA-256 为 731c33be5994826e48319bff699cf569eab8e8262bfb406f74878e3d9fc0ada3、23315624053e752e67a397fb48881044640fbe61e5e5149291d1e3fd539e0af0。原始实际编译 dependency pins 和收录的 108 shared-library file pins 再核对均一致；后者不声称 108 个库均实际加载。新 broker、adapter、runner 和测试 helpers 单独固定并做前后哈希核对。当前工作区相对 R35 manifest 每仓库各 27 个差异保留，没有把这些并发最新源码也称为已测试。最新 ROS 完整 LLVM provider parity 仍未资格化。

所有功能 Python、VM 和测试在 SSH Linux 的原 64 GiB / swap=0 cgroup、原 CPU 集合内，经 birth/PIDFD supervisor 运行。每个独立切面的 aggregate owned RSS 上限为 1 GiB、输出 64 MiB、单文件 8 MiB、root log 1 MiB；6 GiB shared spare admission、原 9288400896-byte arena floor、25 GiB host floor、inode/boot/FSID/exec/cwd/OOM 检查均保留。FSID 为 12191019222208619510。本轮没有重启/替换已恢复的 keeper。

增量证据归档为 `/home/macromodel/Documents/uwvm3-implementation/retained-tinygo-dap-r36-final/tinygo-dap-r36-evidence.tar.xz`，391000 bytes，SHA-256 `a45b08d65c679a24fd45e918ed370db61488669985556fea8a08e338510dce44`，全部 1508 个成员逐一读取核验并 fsync。包含两阶段冻结 Python、真实 DAP/raw/JSON、guards、成功/失败日志和准备快照；原 VM/完整源码通过已再次核对的 R35 归档引用保留，没有重复占用磁盘。归档后才退休旧阶段 538 个重复 raw 文件（14558947 logical bytes），旧结果/guards 和最终 raw 输出仍保留。没有把 global free 变化归因成该删除独占的物理释放量。归档内的核心 qualification 不含事后 archive/retirement 元数据；这些元数据在本报告的机器侧车中明确追加。

仍未完成：实际 IDE 启动/配置/UI、一般快速程序 attach；`box` 等 Go aggregate variables 展开（当前实际为 unavailable）、map entry/iteration、其他 TinyGo scheduler/mutex 布局、一般 producer/别名、builtin shadowing、完整 Go grammar/优先级/untyped constants、标准 gc Go/goroutines、Rust `.len()` 和全部语言原生体验；完整跨架构 VM/JIT/DAP 也没有因为上轮 16-profile DATA QEMU 检查而变成已验证。能力清单 `go_len_cap` 保持 partial，其他 87 个功能和历史统计保留。

机器记录：[debug_go_dap_qualification_20261007.json](debug_go_dap_qualification_20261007.json)。生产实现为 `tools/debug/secure_server.py`、`tools/debug/dap_adapter.py`；实际测试入口和分离单元为 `run_dap_tinygo_collections.py`、`test_dap_integer_display.py`，必须由 Linux cgroup supervisor 驱动。
