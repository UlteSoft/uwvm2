# R37：候选引擎的真实原生端点与 CFI 归属

两个仓库已同步接入候选引擎的私有 NativeOwnerTable V2 端点观察：真实优化 IR 的 typed Wasm 函数为 role 1、同一 sealed plan 的 resume 函数为 role 2；raw adapter 为 role 3，其余 helper 为 role 0。观察器在 LLVM 装载前安装，保留完整 object/symbol ledger。未知、缺失、重复、别名或重叠端点会拒绝候选；typed/resume callable 还须等于真实新引擎解析的入口及对应代码起点。新引擎保留其实际 engine-owned CFI manager 借用，生命周期依附该引擎。

冻结捕获只保存 private DATA，独立 staged_frozen 标记不会设置 LIVE sealed，epoch 保持零。原 live sealing 路径明确拒绝 staged_only。此数据不提供 ASM lookup、断点或执行权限；ASM 仍只允许真实 Wasm 生成的上下文，VM、host helper 和 raw adapter 没有获得 Wasm 调试上下文。

maximum_private_native_endpoint_functions 默认 65,536，累计每个模块的 typed+resume 期望，在生成对应候选 engine 前检查上限。观察 object 时先为解码和 graph backing 保守预留对象字节的 16 倍，再逐 symbol 计费名称和 claim backing，均使用原 maximum_native_payload_bytes。quota 状态不会被 generic compiler type_mismatch 覆盖。未启用 NativeOwnerTable V2 时不宣称捕获端点。ROS 原有 full-only validator 调用签名保留。

| 最终源码验证 | 结果 | 范围 |
|---|---:|---|
| C++ 前端 | 12 项通过 | 两仓库 Linux runtime/core/WASI；Windows、FreeBSD、macOS runtime |
| Wasm 输入组装/validator | 8 项通过 | 两仓库核心与 WASIp1 的间接调用输入 |
| R37 原生 guest 执行 | 0 次 | 新 C++ 行为断言尚未运行 |

全部最终检查在原 SSH Linux 64 GiB cgroup 中完成，使用 LLVM-only 和 delayed template parsing；Windows、FreeBSD、macOS 项为对应 SDK 的交叉前端分析。这不替代四 OS 原生 full-mode 测试。补齐 CFI 借用前的 V1 检查单独保留，最终结果只引用 V2 的 28 项冻结输入。Mac 本机没有运行本轮编译或 guest，2 GiB 测试限制保留。

测试新增了 endpoint quota 拒绝后原 guest 保持同一 pause/epoch、未退休的断言，以及实际 typed/resume/helper/完整 symbol 分类的正向计数断言。它们只通过了编译器前端，运行行为仍待原生验证。

原生启动门槛没有降低：测试目录低于 6 GiB、宿主盘保留至少 32 GiB。最终状态还触及测试卷空闲 inode 少于 8,192 的原定门槛，仍拒绝原生启动；本轮分析由 16 MiB 文件上限控制，没有新增 native objects、可执行文件或 VM 镜像。原 R34 的原生结果仍属于 R34；其他 agent 的文件、进程和改动保留。

**整体 world 发布、恢复线程启动和 guest replay 仍未实现。** 下一步需要用实际旧 generation drain/OS join 证明交接 source、engine、GC、WASIp1 和端点数据，再建立新 generation 的真实 startup lease、根/ledger/pause enrollment；不能通过把 staged 标记或 epoch 数字改成 live 来替代。

Wasm checkpoint 发现 WASIp1 时必须同一次停止点一起 checkpoint WASIp1；文件内容和外部 I/O 不会回滚。命令、源码与日志 hash、PIDFD 回收、内存峰值和磁盘状态保存在同名 JSON 与本轮证据目录。
