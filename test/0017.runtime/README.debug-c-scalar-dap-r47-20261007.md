C 系浮点、布尔表达式的真实 DAP 显示修复与验证（R47）

两仓库现在将 VM 已计算的 canonical bool、float、double 表达式结果显示为标量值和显式类型。修复前，真实 C 停点的 `(float)object.seed / 4.0f` 已由 VM 正确计算为 float / 4 bytes / 1.25，但 DAP 展示整个 source-value packet。原失败协议、日志及 process receipts 保留；本轮修复仅位于 tools/debug/dap_adapter.py 的 source_evaluation_display。

仍在验证完整 packet、真实 pre/post stop 之后投影显示。bool 严格要求1字节及 true/false；float/double 分别严格要求4/8字节，保留既有 fast_io formatter 的原拼写，包括 -0、inf/-inf、nan/-nan。有限十进制按实际 IEEE 宽度检查溢出及非零下溢；FLT_MAX 的最短十进制应按 binary32 舍入校验，不能用其精确数学值误拒绝。NaN 文本不用于推导符号或 payload bits。已知 primitive 的错误位宽、非 canonical 真值、非有限十进制或伪装为 Go len/cap 的标记均拒绝。未知 typedef/CV/pointer、plain char 等仍保留 opaque display。

所有结果保持 readOnly、variablesReference=0；没有新增 evaluateName、memoryReference、查询、写入、地址或控制权限。原整数投影及原控制接口保持。本轮未新增 C++ I/O，保留 VM 已有 fast_io formatter。

| Watch / Hover / Variables 表达式 | 结果 | 类型 |
|---|---|---|
| `(float)object.seed / 4.0f` | `1.25` | float |
| `-(float)(object.seed - object.seed)` | `-0` | float |
| `(double)(object.seed - 4) * 5.0e-324` | 最小 binary64 次正规数，bit pattern 1 | double |
| `(float)object.seed / (float)(object.seed - object.seed)` | `inf` | float |
| `(_Bool)(object.seed - 5)`（C / Objective-C） | `false` | bool |
| `object.seed > 4`（C / Objective-C） | `1` | int |
| `object.seed > 4`（C++ / Objective-C++） | `true` | bool |

每仓库16个真实编译配置：C、C++、Objective-C、Objective-C++ × Wasm32/64 × DWARF4/5，C/Objective-C 为 C11、C++/Objective-C++ 为 C++20，全部 O0、完整 -g，由原 Clang/wasm-ld 编译链接、原 wasm-tools validate，Wasm/DWARF bytes 没有重写。fixture 在同一停点后用原编译代码执行相同表达式自检，以 memcpy 复制有限值/Inf 的实际 IEEE bits，以 isnan 校验 NaN 类；包含±1.25、±0、最大有限值、最小正常数、最小次正规数、±Inf、NaN、正负下溢，以及布尔转换和语言比较结果。C++ 不使用 union 类型别名读取。

原4096次循环样例单会话约48–51秒，完成13个真实会话后通过 receipt 中实际父进程 birth tuple 取得 supervisor PIDFD 并 SIGTERM，原 supervisor 仅退役/reap 其 owned 后代。这个人为取消的 guard 不计功能 PASS，也不计调试器故障；完整成功会话及未完成原始会话均保留。校准为256次自检循环后重新冻结 producer-a2 / dap-a4，先跑单会话，再跑全部短矩阵与持续测试。dap-a3 在任何功能启动前检查发现 sample 路由遗漏，保持原样并弃用；没有改写已测 cut 或失败 receipt。

每个完整会话在 Watch / Hover / Variables 三入口检查72次浮点、15次布尔、6次语言比较结果，另验证整数边界、原22行复制对象树的四入口、字段及子对象分页、部分和、24次预期表达式拒绝及真实单步后的7个旧引用拒绝。两个 instruction / unwind 短矩阵合计64个会话。

| 仓库 | 实际持续 DAP 时间（秒） | 会话 |
|---|---|---|
| uwvm2 | 601.268 | 120 |
| uwvm2-ros | 603.497 | 121 |

持续测试分别至少600秒，每仓库完整覆盖32个 profile/policy 组合；并发测试只证明功能和进程/资源约束，不作为性能资格。最终矩阵及持续测试共 305 个 C 系会话，浮点比较 21960 次、布尔比较 4575 次、语言比较 1830 次，整数边界回归 16008 次；完整树 1220 次 / 26840 行，预期表达式拒绝 7320 次、旧引用拒绝 2135 次。校准1会话和原慢样例13个完整会话独立列入机器证据，不重复计入最终矩阵统计。比较次数不是独立功能数或 native 等价百分比。

两仓库各432项冻结 DAP 单测通过，新增7项 tests 覆盖真值、IEEE边界/特殊值、错误位宽、溢出/非零下溢、未知类型 fallback 和零权限。原 C 系整数专用回归32会话、Rust tuple8会话、嵌套变体4、原变体4、C系 enum/array32、TinyGo数组2及180次 builtin 比较通过。三项外部 WASIp1 formatter-packet runner 未运行，本轮不新增 WASIp1 runtime 资格。

30个原 guard jobs 中28个通过；2个非通过分别为修复前真实显示缺口和上述主动取消。最终 cut 的23个 guard jobs全部通过。所有功能编译、validator、VM、DAP、单测均在 SSH Linux 原64GiB/swap0 cgroup、birth/PIDFD supervisor 内执行，OOM计数不增加，全部 owned PIDFD退役/reap，keeper和peer保留。保留 arena free floor 9288400896 bytes、25GiB host reserve、1GiB owned RSS、64MiB output、8MiB file和1MiB log上限；新测试使用同一host文件系统的有界slot，保留原arena身份及free-floor检查，避免继续填充临时arena。所有源码/tool/runtime pins前后不变，成功会话的原 guest OS wait=0、broker cleanup=0、adapter实际EOF/returncode=0、协议逐成员SHA/fsync核验通过。

主证据包 9379328 bytes / 2579 members，SHA256 `427d9cd1a57415431a12bbec4d2b891c5b2f6562f44b4e82920741ac8578fdf5`。逐成员流式核验并fsync后仅退役本轮6个冗余上传tar，共 10035200 bytes；源码cuts、wasms、工具链、R41产品和peer文件保留，不重复复制大产品/工具链。

本轮复用原R41 scoped Linux x86_64 full产品，ROS仅用full模式。未资格化当前整个脏工作区、latest ROS LLVM provider、full QEMU VM/JIT/DAP、native GDB或IDE GUI。Rust/Go/Zig 的完整原生布尔比较语义及其他producer/优化布局仍未完成；AssemblyScript及各语言完整native体验不据此声称完成。源码共享 numeric predicate 路径的语言差异仍是后续工作。ASM范围继续限于Wasm生成guest上下文，禁止越界调试VM自身。原88项状态与计数不变，仅为 float_expr / casts 增补有限证据。

机器证据：[qualification](debug_c_scalar_dap_qualification_20261007.json)。
