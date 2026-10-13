C 系语言整数表达式的真实 DAP 修复与验证（R46）

两仓库的 DAP 现在将 VM 已计算的 signed char、unsigned char、short、unsigned short、long、unsigned long、long long、unsigned long long 正确显示为标量值和显式类型。此前真实 C 停点执行 `(signed char)(object.pair.__0 - 119)`，VM 返回 `$expression: type=signed char, offset=0, bytes=1, value=-128`，但 DAP 展示整段 source-value 文本。原失败协议及 receipts 保留。

修复范围是 tools/debug/dap_adapter.py 的 source_evaluation_display。仍先验证完整 packet 与真实 pre/post stop，随后只接受 VM source_scalar_expression.h 的 copied_type_name 明确产生的 canonical 整数类型。signed/unsigned char、short类、long long类分别严格要求 1/2/8 个复制字节；long 根据 packet 的明确 guest width 接受 4 或 8 字节，分别按该宽度检查范围，不从宿主机、typedef 或其他标签推断 ABI。unsigned int 保留 R45 的四字节限制。已有 int、signed integer、unsigned integer 与 Go len/cap 路径保留；C primitive 不能重标记 Go builtin。

有效结果保持 readOnly、variablesReference=0，不新增 evaluateName、memoryReference、写接口、查询、指针权限或控制权限。负的 unsigned、超出实际位宽、非 canonical decimal 与已知 primitive 的错误位宽整包拒绝。这些计算结果 packet 中的未知 typedef/CV/pointer、plain char、bool、float、double 等仍保留原 opaque display；本轮没有资格化这些类型的新显示行为。

| 真实 Watch / Hover / Variables 表达式 | 结果 | DAP 类型 |
|---|---|---|
| (signed char)(object.pair.__0 - 119) | -128 | signed char |
| (unsigned char)(object.pair.__1 + 213u) | 255 | unsigned char |
| (short)(object.pair.__0 - 32759) | -32768 | short |
| (unsigned short)(object.pair.__1 + 65493u) | 65535 | unsigned short |
| (long)(object.pair.__0 - 2147483639) | -2147483648 | long |
| (unsigned long)(object.pair.__1 + 4294967253ull) | 4294967295 | unsigned long |
| (long long)(object.pair.__0 - 9223372036854775799ll) | -9223372036854775808 | long long |
| (unsigned long long)(object.pair.__1 + 18446744073709551573ull) | 18446744073709551615 | unsigned long long |

Wasm64 另检查 long 的 INT64_MIN/INT64_MAX 及 unsigned long 的 UINT64_MAX；Wasm32 long 的实际四字节范围独立验证。新 fixture 的原 Clang 编译代码在同一真实停点之后执行这些相同表达式自检，4096 次 guest 循环自然返回 130，没有 I/O。VM 的既有 fast_io formatter 保留；本轮没有新增 C++ I/O。

每仓库各产生16个原编译器产物：C、C++、Objective-C、Objective-C++ × Wasm32/64 × DWARF4/5，全部 O0、完整 -g，由原 wasm-ld 链接并通过原 wasm-tools validate，Wasm/DWARF bytes 没有重写。各产物在 instruction 和 unwind 两个 call-stack policy 的真实断点矩阵均通过，合计64个短测会话。每个会话还验证原22行完整复制树的四种入口、字段及子对象分页、12次部分和、9次 unsigned-int 既有边界、24次预期表达式拒绝，以及真实单步后7个旧 frame/scope/object/后代引用拒绝。

| 仓库 | 实际持续 DAP 时间（秒） | 会话 |
|---|---|---|
| uwvm2 | 604.865 | 84 |
| uwvm2-ros | 606.274 | 86 |

两个持续测试均至少600秒，轮换16个 profile 和两个栈策略，每仓库32个 profile/policy 组合均出现。最终 C 系会话共 234 个，新增整数边界比较 12276 次，unsigned-int 回归比较 2106 次，完整树呈现 936 次 / 20592 行，预期表达式拒绝 5616 次、真实单步旧引用拒绝 1638 次。重复比较次数不是独立功能数或 native 等价百分比。

两仓库各425项冻结 DAP 单测通过；新增7项 tests 包含每个 canonical primitive 的边界、typed/untyped、溢出、错误位宽、非 canonical decimal、Go builtin 标签与未知类型 fallback。三项外部 WASIp1 formatter-packet runner 未运行，不能据此声称新的 WASIp1 runtime 资格。修复后两仓库 Rust tuple 回归8个会话、嵌套变体4个、原变体4个、C系 enum/array32个、TinyGo数组2个及180次 builtin 比较通过。

25个原 guard jobs 中23个通过、2个原失败保留：一个是真实 signed-char 显示缺口；另一个是新 runner 启动 Wasm64 时遗漏显式 --wasm-feature-enable-memory64，VM正确拒绝，修复 runner 后完整矩阵重跑。另有首次准入因缺少原文件系统身份记录而在功能进程创建前失败，随后补齐同一原记录。没有放宽资源/进程 guards 或改写失败 receipts。最终 cut 的20个串行 guard jobs 全部通过。

所有编译、validator、VM、DAP、unit 和功能测试均在 SSH Linux 原64GiB/swap0 cgroup、birth/PIDFD supervisor 中执行；OOM计数不增加，全部 owned PIDFD 退役并reap，keeper/peer进程保留。保留 9288400896 字节 arena free floor、25GiB host reserve、1GiB owned RSS、64MiB output、8MiB单文件及1MiB log上限；195个 producer tool/runtime pins、108个 DAP运行库pins和所有closed code cuts前后不变。成功会话的原 guest OS wait=0、broker cleanup=0、adapter实际EOF/returncode=0及逐成员SHA/fsync核验通过。

主证据包 6284692 字节 / 1764 个原成员，SHA256 `c55339a896c7e9c30b1223d6dc3f6bf2001b07428e9171958b06edf77996ccbe`；逐成员流式核验并fsync后才退役本轮4个冗余上传tar，共 7239680 字节。closed源码cuts、原wasms、工具链、R41产品及peer文件保留，没有重复复制大产品或工具链。

本轮复用原 R41 scoped full 产品：Linux x86_64、R35冻结源码加 Objective-C DWARF数组下界修复。不是当前整个脏工作区/latest ROS LLVM provider/full QEMU VM-JIT-DAP的新资格；ROS只用 full模式。没有实际运行 native GDB 或 IDE GUI，也没有证明所有语言native等价、优化位置/其他producer布局、完整浮点/布尔显示、完整Rust/Go/Zig/AssemblyScript体验。ASM可调范围仍限Wasm生成guest上下文。本轮只扩展已有 int_expr / casts 的有限证据，原88项状态及计数不变。

机器证据：[qualification](debug_c_integer_dap_qualification_20261007.json)。
