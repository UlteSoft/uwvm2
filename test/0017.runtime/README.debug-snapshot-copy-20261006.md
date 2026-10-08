uwvm2 与 uwvm2-ros 同步修复了 LLVM 调试局部变量快照生成中的重复 IR：每个函数创建私有地址表和共享的 noinline 复制函数，各真实 opcode 传入该位置的初始化状态。数值公开快照与原始检查点在类型及范围一致时共用复制函数；引用公开快照仍仅提供 nullness。未初始化或无效 flag 先选择不可变零存储，再读取值，不会解引用未初始化的引用。16 字节槽的填充为零，i32/i64/f32/f64/v128 位模式（包括 signaling NaN）保留。另修复 LLVM 23 对尚无 terminator 的 entry block 调用 getTerminator 所触发的断言。

私有复制函数没有 DISubprogram、Wasm provenance、activation 或公开 safepoint。ASM 调试权限仍受真实 Wasm 生成上下文限制；VM、宿主及插桩辅助函数不进入公开反汇编、寄存器或步进范围。生产修复没有新增 I/O 后端。新增 C++ 回归的文件、输出、LLVM raw stream 适配与参数解析使用 fast_io（native_file、print/println、parse_by_scan）。

所有编译、编译器版本检查、真实 VM、native MCJIT、QEMU 和测试均在 SSH linux 原来的 64 GiB cgroup 中执行：memory.max=68719476736、swap.max=0、cpuset=0,2,4,6,16-31，测试 affinity=16-31。保留原 PIDFD、birth、boot、UID、exe、压力、RSS、磁盘和日志限制；没有终止其他 agent 的进程或重置其源码。相关保护器记录的 memory.events 在这些测试前后均为 max=162、oom=0、oom_kill=0；这不是整个共享 cgroup 历史 max=0 的声明。

| 精确冻结产品的验证 | 结果与实际范围 |
| --- | --- |
| 两仓库完整产品 | 全新 main/runtime/host 对象及链接通过；真实 23.1.1-uwvm-ros.11 SDK，未混用陈旧对象 |
| 共享复制 IR | 1024 个调用位置 × 256 locals，真实执行 262144 项复制；parent 3074 条 IR，单个数值复制函数 2817 条 IR、一个 block |
| 类型与初始化 | 实际 i32/i64/f32/f64/v128、引用公开 nullness 与原始位、未初始化引用、空候选地址、真实 store 后立即可见、异常描述符及缺失缓存属性通过 |
| 真实检查点观测 | 两仓库 × instruction/unwind × 普通/嵌套调用，共 8 次；真实 fused validator/JIT、非默认 i31 引用与 i64 局部值、活跃 operand、pause/capture/resume/drain；普通运行结果 176/1176；executable_restore=false |
| ASM 范围 | 1/256 locals × 两种策略 × 两仓库，共 8 次真实执行；Wasm 的 SI/NI 与数值寄存器可用，私有 copier、VM/helper 字节及过期停点拒绝规则通过 |
| 语言有限回归 | 24 个数值、8 个源码断点、12 个 TinyGo/Zig/AssemblyScript 用例通过；C/C++ guest32/64、Objective-C/Rust guest32，instruction/unwind |
| DWARF run-to | 256 个 DWARF4/5、until/advance、递归/返回、拒绝、断点与替换用例通过 |
| DAP | 两仓库各 234 项，共 468 项通过 |
| 主动持续回归 | 1839.18 秒，44 轮、1408 次真实启动/退出及 worker join；458656 次控制器操作、71280 个真实源码位置；全部通过，没有以等待时间充数 |

持续回归覆盖 C、C++、Objective-C、Rust、TinyGo、Zig、AssemblyScript，两仓库与 instruction/unwind 两种物理调用栈策略。C++64 已单独通过有限用例，持续矩阵为 32 个真实用例；标准 gc Go 没有进入通过矩阵。AssemblyScript 本轮验证 source-map 断点与 into/over/out，source-map 没有类型变量元数据，typed values 仍不可用。

复制组件在 x86_64、aarch64、i686、riscv64、ppc64、ppc64le、ppc32、mips64、mips64el、mips32、mips32el、sparc64、loongarch64、s390x、armhf、armel 共 16 个 Linux profiles 上，两仓库各通过一次，共 32 项。实际 LLVM producer 分别生成 32/64 位、大小端 IR，再由真实目标 Clang 降低、链接、检查 ELF class/endian/machine 与非可执行 GNU_STACK，最后 native/QEMU 执行。这是有限字节/flag 组件验证，不赋予完整目标 C++ emitter ABI、source/activation/ASM authority 或完整跨架构 VM/JIT 体验资格。MIPS 使用已验证的 freestanding UAPI 入口设置 gp/t9 与调用者栈，不以此声称 libc CRT ABI 通过。先前 RISC-V ABI、缺失 llc backend、MIPS CRT exec-stack 与 gp=0 等失败尝试及诊断日志均保留。

共享源码随后扩展了完整 locals 的私有 heap 地址表及按复制 extent 分离的缓存。本轮保留这些并行修改，并对后续独立冻结输入补测：两仓库 native 复制组件通过，16 profiles × 两仓库的组件再次通过；1001 个实际 locals、256 项公开 prefix、两种复制先后次序、完整末项更新、未初始化项及边界 sentinel 均通过。后续源码的完整产品与长时 VM 回归没有在本记录中重新资格化；其 PASS 仅覆盖上述组件。

标准 Go 仍未修复完成。当前普通仓库实际优化 Go guest 的 -Rdbg 冷启动在 180 秒内没有给出 prompt；显式 full policy=debug（O0）也超时，未修改生产默认优化策略。当前非优化 guest 被共享 cgroup 的 1 GiB 压力保护在 91.62 秒停止，不能把它写作本轮 180 秒逻辑失败。ROS 优化 Go 队列连续 900 秒缺少 10 GiB 入场空余，未启动测试；ROS 非优化队列也未启动。

真实 Go 1.27.1 Wasip1 优化/非优化产物均没有 DWARF，分别有 1252/1999 个本地函数，每函数参数与 locals 总数最大 18，没有超过 256 项的函数。因而不能把 Go 冷启动问题归因于超过 256 locals 的 fallback。独立私有诊断仅增加 fast_io 阶段统计，使用更严格的 6 GiB 自有 RSS 上限；最后记录到 function=562，尚未记录 optimizer begin，在 44.33 秒因共享内存压力停止（自有 RSS 峰值约 3.72 GB）。这只支持“此受限样本仍处于 IR 生成阶段”的判断，不是完整 Go 执行或唯一瓶颈的证明。之前两仓库 Go full interpreter 的 4 项通过仍仅属于 interpreter 历史结果。

完整语言原生能力清单仍为 14 implemented、38 partial、34 missing、1 separate_level、1 prohibited_by_scope，共 88 类。本轮优化与回归不会增加这些功能统计，也不代表所有语言特性、全部 Wasm 3.0 调试或所有架构完整 VM/JIT 体验已经完成。检查点本轮验证的是真实状态观测；完整可执行恢复仍不可用。

完整产品冻结源码 ID：普通 b4262eaefb55119b342f3d54bd420e1d2011b5deb64e5e7e3c2bd837fccd4106；ROS 79d3fb5940d9e5db846fb7be0b6660875fa4d0ff437bfbaae8ac71dd5af1681e。后续组件冻结源码 ID：普通 28df1fbf3dec941af52cc0153d0e48059b3dd21b33b79e63ae28bb14f0f25209；ROS bae604c0c4a439aa1196e2f0e92350f8ed773eaaeabd9d7c336810a05f04e9b9。最终观测时本机三个 copier 相关生产文件与后续组件输入一致，随后 runtime/native 文档等并行修改保留，均按精确冻结输入限定 PASS。

新增可复用回归入口为 debug_snapshot_copy_ir.cc 与 run_debug_snapshot_copy_cross_linux.py。后者需要 --root、--repository、--unit、--out，检查真实 unit 的 source-root、source ID、完整输入哈希，并且只能通过原 cgroup 保护器执行。两仓库的 native CLI 回归新增 --extra-locals 0..255（实际 local_count=1+extra）；Go CLI 控制器新增 --full-policy，默认行为保留。后续 large-prefix 回归来自共享源码，实际编译与执行记录单独归档。

精确路径、输入哈希、成功/失败保护器记录与范围见 [本轮资格记录](debug_snapshot_copy_qualification_20261006.json)。原始证据已备份至本机 /tmp/uwvm2-snapshot-followup-20261006/snapshot-copy-evidence-a1.tar.gz，317786956 字节、16447 个成员，本机与远端均逐成员核对，SHA-256=4ca8e306be39bc60152050bc3189fa02102eacc810606e0ea7e9fbea126447a4。归档包含精确冻结源码、两完整产品、选定 native ELF、QEMU 输出、全部持续回归日志、guest DWARF/source-map 输入及受限诊断；工具链与完整 SDK archive 不重复打包，真实 SDK 来源记录及哈希保留。原始远端输入和产物保留。

只清理了已在旧验证归档中逐字节核对的本任务历史持续日志：5760 个文件、157407600 个名义字节。旧 summary、源码、guest、DWARF、工具链与产品均保留，没有假定清理等于 cgroup 内存下降。
