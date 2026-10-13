# TinyGo 数组与字符串 DAP — R39，2026-10-07

两个仓库已同步修复完整有界字符串 packet 在 DAP 中显示整段协议文本的问题。实际 TinyGo 当前源码帧的 box.Message 在 Watch、Hover、Variables evaluation 中返回 type=string、带引号的转义文本和只读 variablesReference；展开后看到同一次 VM 复制的 ptr / len，指针保持叶子，不发起另一条 guest 读取。

原 R38 adapter 的独立真实对照会话仍显示完整 source-value 协议文本、引用为 0。本轮还补齐固定、具名、二维、零长度、零尺寸元素数组的实际 DAP 元素与分页资格；这些数组树在原实现已能展开，新增真实验证将其与此前的 detached protocol DATA 区分。

## 使用

在原 arrayProbe 的 GO_ARRAY_READY 源码断点暂停后，取得真实当前 frameId。Source variables 中展开 box，可以看到 ArrayHolder 的 9 个字段；继续展开 Array / Named / Matrix / ZeroSized，即获得索引成员。对应的显式 Watch `*box`、Hover、Variables evaluation 显示同一复制树。Matrix 两行各三个 int32 值，共 1..6；Named 保留 main.NamedArray 和值 5..8；零长度数组没有伪造子项，六个零尺寸 struct 元素仍按原索引显示。

为观察字符串 payload，显式求值 box.Message：

    {"command":"evaluate","arguments":{"expression":"box.Message","context":"watch","frameId":123}}

123 只示意本次 stackTrace 实际给出的引用。正常 body 类似：

    {"result":"\"a\\xce\\xbb\\xf0\\x9f\\x99\\x82\"","type":"string","variablesReference":124,"namedVariables":2,"indexedVariables":0,"presentationHint":{"attributes":["readOnly"]}}

引用 124 同样只示意响应关系。其 ptr 是 guest:0x…，引用 0；len 是 7，引用 0。这是原 fixture 的 aλ🙂 UTF-8 七个字节，显示保留原 VM 的 ASCII/quote/backslash/非 ASCII 字节转义，不解码成控制字符、地址、路径或命令。initialize 协商 supportsVariableType: true 才显示新增 type。

Source variables 的嵌套 Message 字段仍显示 string 及复制的 ptr/len carrier 结构；本轮不根据字段名或数字地址自动读取 payload。字符串 native 等价、Unicode 直接渲染、所有语言 String ABI 和自动字段 pretty-print 不能据此宣称完成。

## 实现范围

VM 原 source_language_expression.h 在真实 source owner/activation、完整 stopped cohort、DWARF 及单次 guest-memory transaction 中读取有界字符串。Go string 最多复制 4096 bytes，完整声明范围、guest address width 和受限读取次数先经既有 runtime 检查；本轮仅改 Python DAP adapter，不修改 C++ 或 Wasm。C++ I/O 继续沿用原 fast_io，ASM 限制继续只允许 Wasm 产生的上下文。

适配器将结构字段与其后第一个 text= 部分分开验证，payload 内可以含 : type=、offset=、bytes=、text=、分号及命令拼写，均为复制显示数据。文本以既有 canonical metadata escape 验证器按原始字节计数，最多 4096 输入 bytes、最多四倍 ASCII 转义宽度。只接受无 scalar 后缀的文本对象或已验证 guest pointer 的文本装饰；未知 scalar/reason 装饰、enum/variant 等其他格式继续保留不透明显示。空字符串显示双引号。

现有文本协议对截断标记缺少独立结构字段，因此末尾的 (truncated) 或 omitted-elements 数量标记保留整段不透明显示，引用为 0，不公布可展开前缀；可能与这些标记同尾的真实文本也保守处理。非法转义、原始 control/non-ASCII 字节、超字节预算、错误 child extent 和停点变化使请求失败并退休引用。所有复制树沿用单包 64 KiB / 1024 nodes / depth 32 与每停点 8192 nodes / 512 KiB 保留预算，分页前后检查真实 status，不创建 evaluateName、memoryReference 或 setter。ptr、数组标签和字符串值均不成为读取选择器。

## 验证结果

所有 Python 功能回归和实际 VM/guest 执行都在 SSH Linux 原 64 GiB / swap=0 cgroup 内，由 birth/PIDFD supervisor 驱动。保留原 CPU 集合和所有资源阈值，没有重启 keeper。

| 仓库 | 回归单元 | instruction+unwind 短会话 | 持续会话 | 持续秒数 | 持续树行核对 | 持续字符串求值 | 持续 builtin 比较 |
|---|---:|---:|---:|---:|---:|---:|---:|
| uwvm2 | 118 | 2 | 84 | 602.999 | 11088 | 252 | 7560 |
| uwvm2-ros | 118 | 2 | 84 | 601.829 | 11088 | 252 | 7560 |

最终共 172 个实际会话、688 次完整树呈现、22704 行复制树核对（含 5676 行 Source variables）、516 次实际字符串求值和 15480 次原 Go len/cap 值比较。每会话以 5 个实际 compiler arguments 校验 oracle，四个树入口各 33 行，三种 evaluation context 各 30 个 builtin 表达式；重复会话不算独立功能。

覆盖普通/具名数组、二维行列、零长度和六个零尺寸 struct、非零/nil/零长指针、原 UTF-8 字符串及 ptr/len carrier、named/indexed 分类、分页、非法表达式。实际排队单步后新 stop-id 增加，旧 Source scope、未展开懒指针、二维根/行、文本 snapshot 和 frame 均拒绝。每会话原 guest 自检结果 42306、自然 exit 0，原 Popen 被 OS wait，broker/adapter 正常返回并排空真实 EOF；没有 post-exit quit。

原 110 项回归加 8 项新文本 protocol DATA 测试均通过。新 DATA 包括文本 delimiter/命令拼写、canonical escaping、空值/type negotiation、pointer decoration、截断/歧义、原始字节预算、malformed/control/extent 和停点退休；C pointer 等 DATA 不充当实际其他 producer 资格。本轮最终 8 个 guard job 全通过；共 12 个各阶段成功 job，失败/取消 0 个。已启动 roots 均 reap、descendant PIDFD 可读退休，memory.events 无增长且各项为零。

本轮复用 R35 原 fresh 3 TU + link 的完整 uwvm2 / ROS 产品及原 receipt，实际依赖 pins 3446 / 3411、各 108 个原收录 shared-library file pins 前后匹配；库文件 pins 不声称全部实际加载。使用 R34 原 TinyGo 0.42.0 / Go 1.27.1 / LLVM 22.1.4 O0 no-scheduler gc=leaking producer Wasm、原 source paths、validator/DWARF 资格，没有重写或重新编译 Wasm。新 Python、实际 helpers/tests 和两份独立 soak 切面全部冻结并核对 pins。并发工作区和最新 ROS LLVM provider 没有因此获得完整产品资格。

每切面仍上限 1 GiB aggregate owned RSS、64 MiB 输出、8 MiB 单文件、1 MiB root log。因实际会话约 454 KiB，两个 soak 分到独立切面，避免累计超过原输出上限。6 GiB shared spare admission、arena floor 9288400896 bytes、host floor 25 GiB、inode/boot/FSID/exec/cwd/OOM 检查不放宽。boot c8d3550f-3a19-40d7-8507-a76c2045ace5，FSID 12191019222208619510。

开始长测前，重新核验自己的 R37/R38 已 fsync 且逐成员验证的归档 SHA，以及待删文件各自的归档成员 SHA/bytes；只退休 1032 个历史重复 raw 文件、46248834 logical bytes。所有历史 results/guards/source/产品均保留，不把 global free 变化归因于独占物理释放，没有删除 peer 文件。

本轮增量归档：/home/macromodel/Documents/uwvm3-implementation/retained-tinygo-arrays-dap-r39-final/tinygo-arrays-dap-r39-evidence.tar.xz，528144 bytes，SHA-256 30fba2eac7048cb1bbbdfd0cbd6b2a19ff85695de432f33363ac4087fe77648c，1088 个成员全部流式读取核验并 fsync，包含四阶段小源码切面、真实 raw DAP、JSON 和 guards。原完整 VM/C++ source 与 producer/validator 证据分别通过再次核验的 R35 / R34 archive 引用保存；新增完整 VM 副本为 0。核心 archive 的自身 SHA 与事后文档/工作区检查在侧车公开，不声称核心 archive 含自身 SHA。

## 仍待完成

aggregate、strings 清单状态继续 partial。map entries/iteration/runtime pretty-print、其他语言与 String/slice ABI、enum/variant/截断的 richer packet、自动嵌套字段 payload、identifier/shadowing/优化 location、gc Go/goroutines/其他 TinyGo scheduler、Rust .len()、实际 IDE/general fast attach、全部语言 native 等价、完整跨架构 VM/JIT/DAP 及最新 ROS provider 仍需实现或实际验证。此前的 QEMU DATA 不能替代真实跨架构完整产品。

机器记录：[debug_array_text_dap_qualification_20261007.json](debug_array_text_dap_qualification_20261007.json)。实现 tools/debug/dap_adapter.py；实际测试 test/0018.debugger/run_dap_tinygo_array_objects.py；分离文本 DATA 回归 test_dap_source_text.py。R37/R38 报告记录各自完成时的状态，其最终 raw 后续由本轮历史退休记录所列 archive 保存。

归档逐成员核验且 fsync 后，又对本轮已完成切面的重复 raw 逐一核对归档成员 SHA/bytes，退休 696 个文件、62678146 logical bytes。全部 results/guards/source 保留，完整真实 DAP/EOF/raw 证据由上述已验证增量 archive 保存，可按成员恢复；global free 变化不归因成独占物理释放。对应事后机器记录见 current_round_duplicated_raw_retirement，核心 archive 没有包含事后退休记录。
