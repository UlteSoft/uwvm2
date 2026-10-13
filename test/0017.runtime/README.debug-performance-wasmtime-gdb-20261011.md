# Wasmtime 原生调试与 Wasm 插件性能对比，2026-10-11

本轮在 SSH Linux 的实际 64 GiB cgroup（swap=0）完成 **669 个有效计时样本**。计时固定 CPU16、交替模式顺序、串行执行，无并行编译或分析器竞争；本机构建与执行测试均未运行。所有内存事件为0。测量工具为 Wasmtime **49.0.2**（3c8a3e79a）、GNU GDB **16.3**、wasi-sdk **34** 所带 LLDB **23.1.0-wasi-sdk**。官方发布包 SHA256 与 API 发布元数据一致。

结论：GDB 原生路径在无命中断点时接近普通 Wasmtime；完整 Wasm 插件有明显开销，但同输入的直接调用和嵌套调用，仍远低于当前 uwvm2 的 `-Rdbg` 开销。三层调试需求不能简化成只调原生代码；也不能据此认为百倍开销是必要的。

| 调试路径 | 执行与状态恢复方式 | 可直接观察 |
|---|---|---|
| `-D debug-info` + GDB | 生成原生 ELF/DWARF；GDB 在原生指令地址设置断点 | 原生 ASM/寄存器、源代码；可恢复的变量 |
| `-D guest-debug` | 编译器维护可检查的 Wasm 栈帧槽与元数据，未连接前端 | Wasm 状态插桩的执行成本 |
| `-g PORT` + LLDB `--plugin wasm` | 在上述插桩基础上通过 gdbstub、epoch中断、异步调试组件运行 | Wasm 字节码、locals、globals、operand stack、Wasm 调用栈、线性内存、源码 |

“gdbstub”是远程协议服务，不等同于 GNU GDB 的 Wasm 前端插件。Linux测试机的 GNU GDB16.3 对 `set architecture wasm32` 返回 `Undefined item`；官方 Wasmtime 文档的 Wasm 指令调试前端是 LLDB。LLDB Wasm 模式没有原生 CPU 寄存器集合，hostcall 被视为不可分割的操作。uwvm2 的源码/Wasm/ASM 共存能力比这一个模式更广。[官方原生调试文档](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/docs/examples-debugging-native-debugger.md)、[官方 guest 调试文档](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/docs/examples-debugging-guest.md)。

下面是11个原有相同 Wasm 输入，每个模式7次的内核时间中位数。倍率各除以各自普通执行；uwvm2/ROS 使用v45中相同未变化二进制的历史样本。编译器不同，绝对时间和归一化倍率应一起看。输入覆盖整数、浮点、分支、br_table、缓存扫描、随机依赖访问、直接与嵌套调用、FFT。

| 输入 | Wasmtime普通 ms | GDB原生 ÷ 普通 | 仅Wasm插桩 ÷ 普通 | LLDB Wasm ÷ 普通 | uwvm2 dbg ÷ 普通 | ROS dbg ÷ 普通 |
|---|---:|---:|---:|---:|---:|---:|
| integer | 0.465894 | 0.996× | 1.705× | 2.383× | 5.12× | 5.11× |
| branches | 2.092672 | 1.024× | 1.395× | 1.546× | 11.03× | 10.70× |
| dispatch | 2.267550 | 1.016× | 1.228× | 1.333× | 2.56× | 2.57× |
| scan32k | 1.477332 | 1.000× | 5.311× | 6.693× | 29.67× | 29.66× |
| scan256k | 1.508475 | 0.998× | 5.269× | 6.575× | 29.63× | 29.64× |
| scan8m | 1.581189 | 0.981× | 4.994× | 6.259× | 29.24× | 29.23× |
| chase8m | 30.110567 | 0.940× | 1.035× | 0.952× | 1.02× | 1.02× |
| calls | 0.386619 | 1.000× | 3.633× | 4.007× | 101.66× | 102.31× |
| callchain | 0.936867 | 0.997× | 2.185× | 2.450× | 117.10× | 116.12× |
| float | 0.445999 | 1.000× | 1.065× | 2.743× | 5.14× | 5.15× |
| fft | 0.056807 | 0.982× | 6.595× | 6.970× | 13.31× | 13.17× |

这些短内核不能把低于1的结果解释为加速。缓存/地址布局、频率、系统噪声会影响结果；O0的全部原始样本也保留在JSON中，但没有把它们包装成“GDB零成本”的普遍保证。长调用与长随机访问另各5次，使用与v45相同的扩展Wasm；不是混入短样本：

| 长输入 | 普通 ms | debug-info ms | GDB ms | GDB ÷ 普通 | Wasm插桩 ms | LLDB Wasm ms | LLDB ÷ 普通 |
|---|---:|---:|---:|---:|---:|---:|---:|
| calls | 380.587 | 380.678 | 380.834 | 1.00065× | 1395.263 | 1549.538 | 4.07145× |
| chase8m | 824.986 | 825.715 | 819.269 | 0.99307× | 827.008 | 839.510 | 1.01761× |

同一原始调用Wasm，uwvm2 dbg为45.128 ms，Wasmtime LLDB Wasm为1.549 ms。当前差距主要应针对运行中的状态维护与ABI桥优化；不同JIT生成的代码本身也有差异，不能把整个差距都归因于前端。

旧 FFT 输入没有 DWARF；另外用 wasi-sdk 在 Linux 从同一 FFT 源文件编译了完整 `-g -O3 --export-all` 输入，7种模式各5次，以补上真实源码调试的性能口径。该输入与旧FFT字节不同，单独列示，未与旧uwvm样本混算：

| 完整DWARF FFT | 内核中位数 ms | ÷该输入普通 |
|---|---:|---:|
| ordinary | 0.059014 | 1.000× |
| debug-o2 | 0.058900 | 0.998× |
| gdb-o2 | 0.058756 | 0.996× |
| guest-instrument | 0.379290 | 6.427× |
| guest-lldb | 0.394709 | 6.688× |
| debug-o0 | 0.069004 | 1.169× |
| gdb-o0 | 0.068860 | 1.167× |

256个无命中断点在单独完整DWARF断点输入上测量，9种模式各5次。GDB先在JIT注册处停住，确认256个断点全部有原生地址，再在内核计时前删除注册钩子；LLDB确认256个Wasm位置均已绑定。禁用模式也单独测量，所有程序checksum=10000、正常退出。该新输入没有与v45旧断点输入混算；本轮没有测忽略计数/命中延迟。

| 完整DWARF断点输入 | 内核中位数 ms |
|---|---:|
| ordinary | 0.010398 |
| debug-o2 | 0.010366 |
| gdb-o2 | 0.010261 |
| gdb-cold256 | 0.010195 |
| gdb-disabled256 | 0.010271 |
| guest-instrument | 0.027465 |
| guest-lldb | 0.034813 |
| guest-cold256 | 0.034858 |
| guest-disabled256 | 0.034832 |

冷断点相对无断点未显示超出微秒级样本噪声的额外内核开销。设置、注册、代码复制、保护切换的成本在内核之外，完整进程耗时保留在JSON，不能把它叫纯JIT时间。

原生路径的实现是：Cranelift生成原生地址映射，将Wasm DWARF行表与变量位置表达式转换为原生DWARF；Wasm local用编译器ValueLabel的寄存器/CFA栈位置表示，线性内存解引用加上VMContext中的内存基址。然后生成、重定位内存中的ELF并通过 `__jit_debug_descriptor` 的链表及 `__jit_debug_register_code()` 向GDB通知注册/卸载。通知发生在代码注册时，GDB的JIT接口本身不要求逐指令调用运行时。不是靠Python插件逐条解释Wasm。[行表转换](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/cranelift/src/debug/transform/line_program.rs)、[位置表达式转换](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/cranelift/src/debug/transform/expression.rs)、[注册实现](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/jit-debug/src/gdb_jit_int.rs)、[GDB官方JIT接口](https://sourceware.org/gdb/current/onlinedocs/gdb.html/JIT-Interface.html)。

这条原生路径不能保证完整Wasm局部值与操作数栈恢复，禁用优化也不是完整性的证明。49.0.2位置表达式转换对WasmGlobal/WasmStack仍有TODO并放弃转换；guest_debug才直接维护VM状态。不要用原生GDB的低开销冒充完全相同的状态能力。[Config中两种状态恢复的说明](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/wasmtime/src/config.rs)、[转换边界](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/cranelift/src/debug/transform/expression.rs)。

Wasm插件路径从真实Wasm模块读取原始DWARF与字节码：LLDB的ObjectFileWasm/DynamicLoaderWasmDYLD加载模块；SymbolFileWasm解释 `DW_OP_WASM_location`，把local/global/operand编码为带类型标签的虚拟寄存器，再由RegisterContextWasm发送 `qWasmLocal`、`qWasmGlobal`、`qWasmStackValue`。UnwindWasm使用 `qWasmCallStack`；模块与线性内存通过合成64位地址和标准远程内存读取访问。Wasmtime的gdbstub Target将请求转发到实际Frame/Instance/Memory API，并把Z0/z0断点请求转为模块Wasm PC断点。不会逐指令读取TCP；值查询集中在停止后。[Wasmtime Target](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/gdbstub-component/src/target.rs)、[LLDB SymbolFileWasm](https://github.com/llvm/llvm-project/blob/895aa2c896ada719451be2e3673c83da8ddf1141/lldb/source/Plugins/SymbolFile/DWARF/SymbolFileWasm.cpp)、[虚拟寄存器读取](https://github.com/llvm/llvm-project/blob/895aa2c896ada719451be2e3673c83da8ddf1141/lldb/source/Plugins/Process/wasm/RegisterContextWasm.cpp)、[远程请求](https://github.com/llvm/llvm-project/blob/895aa2c896ada719451be2e3673c83da8ddf1141/lldb/source/Plugins/Process/wasm/ProcessWasm.cpp)。

Wasm执行端的关键是**稳定栈帧槽 + 静态元数据 + 按需代码修改**。编译期分配local/operand槽与类型布局，local变化直接写槽，操作数栈只同步新产生/变脏的值；PC、栈形状、槽位置由FrameTable描述。断点调用使用PreserveAll ABI，最终链接时保存启用字节、填充为架构NOP；设置断点时才恢复相应调用字节。每个Store私有代码副本防止影响其他Store中的实例，修改代码时取消执行权限、再发布。这仍有插桩：槽写入、NOP、代码体积/布局、寄存器约束均会产生开销；低开销不是“完全没有插桩”。[状态槽与ABI](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/cranelift/src/func_environ.rs)、[最终NOP填充](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/cranelift/src/compiler.rs)、[FrameTable](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/environ/src/compile/frame_table.rs)、[Store代码所有权](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/wasmtime/src/runtime/code.rs)、[断点修改](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/crates/wasmtime/src/runtime/debug.rs)。

LLDB连接模式比单开guest_debug更慢的差值，包含CLI自动开启的epoch检查、异步yield与调试组件；不能称为纯TCP/LLDB接入成本。49.0.2 CLI会强制guest_debug和epoch_interruption，并安排可被调试器中断的执行。[实际CLI配置](https://github.com/bytecodealliance/wasmtime/blob/v49.0.2/src/commands/run.rs)。

功能测试使用真实PTY：GDB命中 `probe_hot(x=0)`，源码next后 `a=7`；RIP/RSP/RAX可读，原生disassembly和 `__vmctx->memory`成功。LLDB命中Wasm `local.get 1`，单步从虚拟PC `0x400000000000023a`到`0x400000000000023c`；读取local0=0、global0=65472、operand0=65456、3层Wasm调用栈及线性内存。初始尚未进入任何Wasm帧的PC0会出现官方文档所示的memory-read提示，该时刻未用于状态结论。功能检查中的人工停止时间不计入性能数据。

对uwvm2的下一轮实现建议是：把未启用追踪、没有命中断点的常态路径变成编译器直接更新必要的原生槽，使用编译期固定的类型/偏移/栈形状表；只有停住、查询或明确开启追踪时才解码并构造调试视图。函数调用采用编译器可描述的帧元数据和保留寄存器的断点ABI，削减每次调用上的shadow-ledger重建、动态ABI桥和generation-plan查找。断点开关修改具体JIT位置，保留目前的模块/代际/停住所有权、JIT代码范围、typed栈完整性和三层执行位置校验。原生指令中途的Wasm视图仍要区分真实可捕获状态与下一条位置，不能把无法恢复的状态标成`=>`。这些是由测量与源码得出的实现方向，本轮没有把运行时改成这套架构，也没有宣称优化已经完成。

v45硬件热点已经指向动态/静态materialize ABI桥、shadow_ledger材料化与generation-plan获取。Wasmtime的方案与这些热点形成了明确的可检验假设：应让未命中路径避免运行时解码和发布，而不是只继续压缩逐指令回调中的小分支。本轮只有计时与协议采样，没有对Wasmtime新做VTune/PMU采集；不能把v45的uwvm硬件计数当作Wasmtime计数。[v45报告](README.debug-performance-gdb-comparison-20261011.md)。

原始结果、命令、输入/工具/源码SHA256、资源回执、功能回复和失败尝试的排除信息见 [JSON](debug_performance_wasmtime_gdb_20261011.json)。9个失败/中止的准备或验证尝试保留，均未混入669个有效计时样本。重现工具为 [计时脚本](run_debug_wasmtime_comparison.py) 和 [冷断点脚本](run_debug_wasmtime_breakpoints.py)，需要先在同一Linux资源限制下准备manifest描述的fixture；适合复用既有 `make_debug_workload_matrix.py`。

本轮测试工具与报告同步到uwvm2和uwvm2-ros；生产代码保持v44状态。本轮没有Windows/QEMU/macOS运行测试，遵守用户要求在SSH Linux执行。
