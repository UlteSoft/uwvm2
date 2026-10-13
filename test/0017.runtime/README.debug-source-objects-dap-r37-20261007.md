# TinyGo 源码对象 Watch 展开 — R37，2026-10-07

两个仓库同步增加了 DAP 中完整复制对象树的只读展开。真实 TinyGo 当前源码帧中，Watch、Hover、Variables evaluation 的 `*box` 显示 `main.CollectionHolder` 并返回正的 `variablesReference`，IDE 可用 `variables` 请求取回 11 个字段。具名 `main.NamedMap` / `main.NamedChannel` 类型、nil 的 `guest:0x0`、Receive/Send/Buffered 的相同 guest bits 均在实际原始 Wasm 上核对。

## 如何使用

在已附着并暂停于 `collectionProbe` 的真实当前源码帧，把 `*box` 加入 Watch；取得当前 `stackTrace` 的 opaque frameId。客户端 initialize 需协商 `supportsVariableType: true` 才显示 type；Hover 能力沿用 R36。

```json
{"command":"evaluate","arguments":{"expression":"*box","context":"watch","frameId":123}}
```

这里的 123 只示意当前 frameId，不能自己猜测或复用上次停点的编号。正常 body 类似：

```json
{"result":"main.CollectionHolder","type":"main.CollectionHolder","variablesReference":124,"namedVariables":11,"indexedVariables":0,"presentationHint":{"attributes":["readOnly"]}}
```

继续用该响应实际给出的引用请求 `variables`，例如 `{"variablesReference":124,"start":0,"count":3}`。前三项为 Numbers、Named、Empty。值为原 guest 指针显示，字段本身不获得解引用或地址操作权限。单独 Watch `box` 返回 `guest:0x… / pointer`、引用 0；需要用户显式请求 `*box`，不会根据指针显示或成员名称自动追踪 runtime map bucket/channel buffer。

Source variables 的根列举仍使用原有限 numeric packet；其中 `box` 仍显示 unavailable，本轮没有从其转义标签推导选择器。当前可用入口是显式 Watch `*box`，不能称自动聚合 locals / map entries 已完成。

## 对象显示范围

VM 的既有 `print-frame THREAD STOP FRAME EXPR` 在原有 DWARF、实际 source frame/activation、完整 stopped cohort 和 guest memory transaction 下复制对象。原对象文本不带 scalar encoding，可能属于浮点/typedef 的 -0 或超整数范围十进制保留完整原始显示，避免推断错误 integer ABI；R36 有限 len/cap/expression 根继续用其严格类型/宽度检查。本轮 C++ 路径未修改，I/O 继续沿用 fast_io。adapter 在既有真实前后停点检查及完整 formatter envelope 验证后，只消费有界复制对象树；叶子数值/指针变为单独显示，结构字段可展开。新增的 variables 请求只读取该份复制快照并前后查询 status，不发起另一条 print/read/dereference，也不把字段转义解码成表达式。显示不提供 evaluateName、memoryReference、setter 或 host/native address 权限，ASM 上下文限制保持原规则。

支持完整有限固定树、具名/索引显示、分页和只读提示。单个包上限仍为 64 KiB、1024 nodes、depth 32；子项 extent 必须在复制 parent 内。每停点最多保留 8192 nodes / 512 KiB 包字节，配额在分配 references 前检查。分页 start/count 为 0..1024，count=0 取剩余项；named/indexed 只分类现有复制标签。未知扩展、enum 注解、variant/string/text 格式和 omitted/truncated packets 保留完整不透明显示，不公布可展开前缀。 nested named/indexed trees 本轮有分离的 protocol DATA 测试，不冒充实际新 producer/VM 验证。

continue、单步、替换、可变 debugger 操作、退出、错误及直接收到不同 stopped 快照都会退休对象缓存和计数，references 不重复使用。实际排队单步后旧 object/scopes/evaluate 被拒绝；同 PC 的新 stop/generation 也不能复用旧视图。对象值是该次源查询的复制快照，不声称每次分页重新读取 mutable guest memory。

## 验证结果

所有功能测试和 VM 执行都在 SSH Linux 的原 64 GiB / swap=0 cgroup，经 birth/PIDFD supervisor 完成，使用原 CPU 集合。两个仓库最终版本均通过：

| 仓库 | protocol/回归单元 | instruction+unwind 短会话 | 超过10分钟会话 | 持续秒数 | 持续字段显示核对 | 持续标量比较 |
|---|---:|---:|---:|---:|---:|---:|
| uwvm2 | 96 | 2 | 63 | 603.147 | 2079 | 4158 |
| uwvm2-ros | 96 | 2 | 63 | 603.636 | 2079 | 4158 |

最终共 130 个真实会话，390 个 object evaluate contexts、4290 次字段显示核对、8580 次原 len/cap 标量比较；重复会话/context 不算独立功能。每次同时核对原 source breakpoint/path/line、compiler arguments、对象字段/type/nil/alias、分页和非法表达式、实际新停点、原 guest 自检自然 exit 0、原 Popen OS wait、broker/adapter 正常返回和真实 EOF。没有 post-exit quit。实际 VS Code UI 不在本轮范围。

最终 8 个 guard job 全部通过。另保留 21 个所有阶段成功 job 与 4 个失败/取消 receipt：初版单元里的转义拼写/fixture stop 编号修正，以及发现直接 new-stop 缓存清理遗漏/浮点未知编码兼容后，通过验证 PIDFD 取消三份前版 soak，改用最终版本重测。最终各 96 项包含该清理回归。已启动 roots 均被 reap、descendant PIDFD 均退休，memory.events 无增长且各项为零。

本轮不重编 C++，明确复用 R35 的原 fresh 3 TU + link 完整产品及原 receipt，没有转换成其他 receipt schema。uwvm2 / ROS 实际依赖 pins 3446 / 3411 和各 108 个原收录 shared-library file pins 前后再核对匹配，后者不声称全部库实际加载。TinyGo 0.42.0 / Go 1.27.1 / LLVM 22.1.4 原 O0 no-scheduler gc=leaking Wasm、validator/DWARF 原资格和 source paths 均保持；新 Python、全部实际 helpers 和 tests 单独冻结/前后核验。最新并发工作区及 ROS 最新 LLVM provider 没有因此获得完整产品资格。

每个独立切面上限为 1 GiB aggregate owned RSS、64 MiB 输出、8 MiB 单文件、1 MiB root log；6 GiB shared spare admission、原 arena floor 9288400896 bytes、host floor 25 GiB、inode/boot/FSID/exec/cwd/OOM 检查不放宽。FSID 12191019222208619510，boot c8d3550f-3a19-40d7-8507-a76c2045ace5；直接复用 keeper，没有重启或删除 peer 文件。

增量证据归档：`/home/macromodel/Documents/uwvm3-implementation/retained-tinygo-objects-r37-final/tinygo-objects-r37-evidence.tar.xz`，500540 bytes，SHA-256 `3560d5ba842d7703edc1800f0b869784f55fd7da6e0b162d9c35dd6d1214efca`，1390 个成员全部逐一读取核验并 fsync。收录五阶段小型源码快照、raw DAP/JSON、guards、成功/失败/取消记录和原产品引用；VM/完整 C++ source 通过再次校验的 R35 archive 保存，不重复堆积产品。归档核验后只退休前版切面 383 个重复 raw 文件、16112297 logical bytes；最终 raw、全部 results/guards/source 均保留，global free 的变化不归因成独占物理释放。本 archive 收录核心 qualification；archive 自身及事后文档/工作区检查作为公开机器记录侧车补充，不声称核心 archive 含自身 SHA 或事后记录。

能力清单 aggregate 继续 partial。仍未完成：自动 aggregate Source variables、Go map entry/iteration/runtime pretty-print、实际 IDE/general fast attach、其他语言/真实 nested-array/enum/variant/string/slice 显示资格、其他 TinyGo scheduler/mutex 布局、完整 Go grammar/gc Go/goroutines、Rust .len、全部语言 native 等价、完整跨架构 VM/JIT/DAP 及最新 ROS provider。此前 QEMU DATA 资格不代替这些实际产品测试。

机器记录：[debug_source_objects_dap_qualification_20261007.json](debug_source_objects_dap_qualification_20261007.json)。生产实现为 `tools/debug/dap_adapter.py`；实际入口为 `test/0018.debugger/run_dap_tinygo_objects.py`，分离 protocol DATA 为 `test_dap_source_objects.py`。字段和 reference 生命周期对照 [DAP Variable](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#Types_Variable) 与 [EvaluateResponse](https://github.com/microsoft/debug-adapter-protocol/blob/main/specification.md#Types_EvaluateResponse)。
