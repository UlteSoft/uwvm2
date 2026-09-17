# LLVM 23.1.1 ROS：目标代码生成修复复核

这是 [v5 首轮报告](ros-cache-v5-audit.md) 的后续，不覆盖或抹去首轮失败记录。
全注册目标枚举、跨目标执行、完整宿主构建是不同层次；任何一层通过都不能代替其他层。

## 9 月 17 日最新状态补充

最新 `.6` 快照已补齐 MIPS O32/N32 的 PIC 宿主调用问题：`long-calls` 单独使用会被
ABICalls 模式忽略，现与 `noabicalls` 配套，两版同步。8 个 MIPS 配置的 40 个实际执行
场景、两产品共 168 项目标选择检查、24 项缓存隔离/旧策略拒绝检查通过。
microMIPS/MIPS16 当前明确拒绝原生 MCJIT，不计作实现通过。原因、失败对照、代码大小
变化和 NaN 测试编码修正见 [ELF/MIPS 回溯记录](elf-local-symbol-unwind.md)。

`.5` named-modules 构建被 systemd-oomd 因内存压力终止，没有得到最终二进制，后续
模块功能测试未执行；此前七组通过是 header CLI 的结果。32 GiB 总上限没有提高。
保留失败日志后，已清理可重建的旧探针与未完成模块中间产物，回测主线转至最新 `.6`。
该快照的九项构建前检查通过，新 LLVM/header CLI 完整构建及其后续回归正在进行；
最新完整模块、全平台执行和性能比较尚未完成，不能宣称全部通过。

## 修复与原因

| 问题 | 本轮修改 | 验证边界 |
| --- | --- | --- |
| VE 栈扩张后的参数物理寄存器成为未定义值 | 回算新 continuation / syscall block 的 live-ins，先后继再前驱 | 两条独立后端回归；不增加运行时指令 |
| VE VPU 将短向量扩成 256 lane，缺失转换模式且可能产生大 spill | UWVM 的宿主 TargetMachine feature 追加 `-vpu`，保留 guarded volatile memory 语义 | 全 SIMD codegen + verifier；没有 VE 硬件执行或性能结论 |
| AVR 固定向量位移产生 vector-to-scalar trunc 非法 IR | 在 AVR 原标量位移展开前，逐 lane 分解移位，保留各 lane 的计数 | 后端回归及全部 SIMD codegen；没有 AVR 整机执行结论 |
| MSP430 在活动 call frame 中分裂 select / shift block，frame size 被错误重置 | 新块继承分裂点 call-frame size | 后端回归及全部 SIMD codegen；没有 MSP430 整机执行结论 |
| x86_64 禁用 SSE 时，舍入 libcall 的 XMM0 返回 ABI 不可用 | 使用现有整数 FP bridge；min/max 使用整数选择 | 整数 buffer ABI 生成代码的原生执行；不是整个 C++ 宿主无 SSE |
| 上述 x86_64 路径虽可编译，x87 搬运却 quiet 了 splat 的 sNaN | 整数化敏感比较/转换并将 `-x87` 保留到最终发射 | 位保持 SIMD oracle；生成汇编没有 XMM/YMM/ZMM、FLD/FST |

VE 的后端只有机器级 Subtarget；只给函数加 `target-features=-vpu` 不生效。
因此设置位于 `get_llvm_jit_host_target_attribute_storage()`，由 full/lazy 预优化和最终 MCJIT 共同使用，
而不是在 SIMD emitter 中留下无效属性。测试也必须给非 x86 目标的 llc 传递机器级 features。
相反，x86 的最终 llc 不能在函数已追加 `-x87` 后再从原测试参数追加 `+x87`，否则重新引入 NaN 搬运错误。
测试工具在这两种情况中采用不同的 feature 传递方式，并始终开启 machine verifier。

正式 LLVM 基线仍是 23.1.1，不是 trunk。本轮后缀从 `.1` 改为 `.2`，没有改写 `.1` 的内容寻址构建目录。
与原始正式版比对：12,855 个保留文件，仅五个编译器源码修改、六个新增回归文件；
清单、来源校验与具体补丁理由见 [UPSTREAM_CHANGES](../../third-parties/llvm/UPSTREAM_CHANGES.md)。
VE 通用 VL 寄存器改写的诊断实验**没有纳入**正式补丁。

普通版与 ROS 都同步了 UWVM FP lowering、SIMD fallback、VE TargetMachine 策略和缓存语义 key：
`scalar-target-contract-v3`、`integer-globals-x86-no-sse-transport-v2`。
缓存格式仍为本轮已升级的 v5，两个产品各自隔离；不能因 source ID 未更新而接受旧错误对象。
新增测试同时要求新 key 出现、旧 key 不出现。普通版不因此自动获得 ROS 的 vendored LLVM 后端补丁。

## 已完成的 `.2` 结果

Linux 证据根目录：`/tmp/uwvm-comprehensive.eUFnxH/`。
这些测试使用 xmake 构建出的正式 `.2` 静态库及同目录的 llc/opt/FileCheck，不是中途实验性 relink 的 llc。

| 检查 | 结果 | 证据目录 |
| --- | --- | --- |
| 保留的 LLVM 后端回归 | 14 RUN，0 失败 | `release2-backend-regressions` |
| 48 注册名 portable contract | 41 通过，7 失败 | `release2-codegen-all` |
| 48 注册名全部 SIMD codegen | 37 通过，11 失败 | `release2-codegen-all` |
| 19 组 ISA/ABI/OS 变体 | contract 与 SIMD 均 19/19 | `release2-codegen-variants` |
| 原生/QEMU 的 SIMD 执行 | 18/18，每组 236 opcode + guarded-store fault oracle | `release2-codegen-exec` |
| 标量 NaN | 77 个优化后原生 case | `release2-codegen-tools/nan.log` |
| 与首轮 15 个执行配置的 SIMD 对象对比 | 15/15 整个对象逐字节相同 | `release2-codegen-object-comparison.json` |
| 来源/链接元数据/路径/引导编译器/实际 CMake 合同 | 7 组驱动通过，含预期拒绝的反例 | `release2-contracts` |
| 两产品新语义 key / 跨产品缓存拒绝 | 新 key 回归通过；隔离 20 次调用通过 | `release2-current-fixtures` |
| 回归脚本退出状态 | 真正失败返回非零；仅显式 inventory 模式允许继续，失败记录仍保留 | `release2-inventory-exit` |

18 个执行配置是：x86_64、i686-x87、i686-sse2、AArch64、AArch64-noSIMD、ARM-neon、
PPC32、PPC64BE/LE、RV64、RV64-V、s390x、LoongArch-LSX/LASX、SPARC64、
x86_64-noSSE、MIPS N32 BE/LE。后两种 N32 用正确的 normalized gnuabin32 triple、
32 位指针 DataLayout、N32 ABI 和对应交叉 SDK 实际执行，不沿用已撤销的旧 N32 结果。
QEMU oracle 仍不等于这些平台的完整 ROS CLI、装载器和动态 CFI 集成通过。

15 个 SIMD 对象不变只证明这组生成代码没有增加热路径工作，不能证明所有应用没有性能退化。
noSSE 的宿主 bridge/oracle 仍按标准 x86_64 ABI 构建；其调用成本不能称为纯原生 SIMD 的成本。

## 没有改标为通过的配置

剩余 11 注册名：`amdgcn`、`amdgpu`、`r600`、`bpf`、`bpfeb`、`bpfel`、
`nvptx`、`nvptx64`、`spirv`、`spirv32`、`spirv64`。
它们分别是 GPU 或字节码后端，不能因为出现在 LLVM registry 就当成当前原生进程 JIT 的宿主。
失败涵盖 constrained FP libcall、GPU address space、SPIR-V Logical pointer cast 和宽 guarded-store legalization。
没有通过删除 opcode、取消 verifier、改成非严格 FP 或跳过失败来宣称 48/48。
复核还强化了枚举脚本的退出状态：默认只要有失败就非零，诊断枚举必须显式传 `--inventory-only`。
用真实 BPF 失败和 x86_64 成功 probe 验证了这两种模式，避免把“枚举程序完成”误当“全平台通过”。
新增这些执行后端需要明确其内存、调用 ABI、trap 和回溯模型，不能只修到 llc 返回 0。

另外补测了**整个 C++ 宿主**使用 `-mno-sse -mno-sse2 -mfpmath=387` 的配置，
GCC 15 / Clang 22、O0/O3，共 56 项：29 项通过、27 项无法构建。
失败包括标准 x86_64 浮点返回要求 SSE 寄存器的编译错误，证据 `ros-host-no-sse-recheck-01`。
这与上表已经通过的“生成的整数 buffer ABI 代码无 SSE”是两个要求，不能互相替代。
完全无 SSE 的宿主支持需要进一步改造 C++ 浮点接口/软件浮点边界；没有偷偷打开 SSE 参数或宣称已完成。

## 完整 v5 named-modules 构建

`.2` 完整重建及后续运行已完成；不是旧 v4 binary 或首轮 v5 header binary。
构建目录 `ros-cache5-modules-repaired-03`，生产源码 manifest SHA256：
`b43cf8c49a8aebcdc2cdf44db911d1d6d3e1dbd84796ccfb40ecaf4fc03a7002`。
使用 Clang 22.1.8、C++ named modules、bundled LLVM `.2`、compiler-rt/libunwind。
后补的测试文件独立放在 `release2-current-fixtures`，没有在构建中改动冻结源码快照。
构建前后源码清单一致，可执行文件 SHA256：
`f059cf18d77eeef2da50535aab15dec9e21a9627903c7f6937e1ccfd6ab3c72b`。
`release2-modules-tests` 记录：108 次回溯/缓存运行（72 次 signed-cache 陷阱重放）、
2,296 次 Core 2 运行/83,152 次断言（4,673 项明确排除）、652 项内存边界/grow、
65 项集成、150 次栈耗尽、混合 signedness 转换与 18 个解释器加法函数汇编检查均通过。
`release2-modules-cache-fixture-02` 的缓存集成检查也通过，含签名损坏、IR shape 变化、
路径策略及 12 次 fuzz 恢复。初次驱动忘记给测试编译传递 CLI 相同的 source-ID 宏，
误跑“无来源构建应拒绝缓存”分支，失败保留在 `release2-modules-cache-fixture`；
修正驱动后才得到这里的成功结果，没有放宽生产缓存策略。
同 Clang 22 的 v4 模块基线对比，31 轮中位数 after/before：JIT 0.999861、解释器 1.007321；
不是普遍提速的证据。三个 JIT 基准内核及完整对象字节相同，总 text 1,574 字节。

## 新增 MIPS R6 比较缺陷与 `.3` 修复

扩展执行到 MIPS32r6el 后，`.2` 的 `f32x4.ne` 在相等输入时返回 `-2` 而非零。
根因是通用 SelectionDAG 谓词反转用整数结果类型选择 true 常量 `1`，而 R6 FP true 是 `-1`。
只看 i1 的最低位会隐藏错误；原来的寄存器 copy/spill 修复不能解决它。
新补丁按原比较操作数类型构造 true 常量，再 XOR；不增加运行时 helper。

补测 constrained 比较又发现 R6 继承了 pre-R6 FCC0/CMOV 路径，无法选指令。
改为 R6 quiet/signaling CMP 模式后，首个候选还会删除结果未使用的严格比较，丢失 NaN 异常。
最终候选同时保留 strict dependency chain 和 mayRaiseFPException；只给后者打标记不够。
普通比较的 source DAG 没有 chain，因此不会被这项修复强制串行化。

诊断编译器 `mips-strict-repair-03` 已通过：

- `mips-retained-predicate-03`：六条 BE/LE、O32/N64、microMIPS 回归；正式 `.2` 反例失败、修复后通过。
- `mips-strict-predicates-03`：MIPS32r6el 上 14 predicate × f32/f64 × quiet/signaling × 结果保留/丢弃 × 16² 输入，共 28,672 项结果位/FE_INVALID 检查。
- `mips-repair-matrix-03`：重新枚举 48 注册名（contract 41、SIMD 37），19 变体全部通过；23 个实际原生/QEMU 执行配置全部通过。

23 个执行配置在原 18 个上增加 RV32 glibc/musl、AArch64BE、ARMv5 soft-float、MIPS32r6el。
另外四个旧 MIPS O32/N64 配置因交叉 GCC 路径缺失未完成；不是执行通过。
与 `.2` 对比，这 23 个配置中仅有修复目标 MIPS32r6el 的对象改变，其余 22 个完整对象逐字节相同，
证据 `mips-repair-object-comparison.json`。这支持“未给这组未受影响目标增加热路径工作”，不是全局性能证明。
完整 NaN 异常 oracle 已保留为 `test/0018.build/check_mips_strict_predicates.py`，
参数显式指定 LLVM 工具目录、交叉编译/执行 profile，不搜索系统 llc。

新的正式版 downstream suffix 是 `.3`，来源比对通过：12,856 个保留文件，
九个编译器源码修改、七个新增回归；与原始 LLVM 23.1.1 正式 archive 逐文件比对。
**本节诊断 relink 不是 `.3` 完整打包构建/CLI 通过的证明。上面的模块结果使用 `.2`，
尚不能把它称为包含新 MIPS 补丁的最新模块结果。**
剩余 11 个 GPU/字节码注册名仍失败，没有改标为通过；普通版的外部 LLVM 也不会自动获得新补丁。

## 资源与证据保留

### `.3` 正式库补测与 compressed-MIPS 诊断（尚未收尾）

`release3-codegen-tools` 使用 xmake 构建的正式 `.3` 库，源码清单为
`bc45f8e65b395c67b5e75cf8ed11220dc71514b36c7cf5b52bfc538225153ce1`。
`release3-backend-regressions` 的 20 RUN、`release3-codegen-variants` 的 19 组、
`release3-codegen-exec` 的 28 组实际执行、77 项 NaN 测试，以及
`release3-contracts` 的 7 组构建合同检查均通过。`release3-strict-o32` 与
`release3-strict-n32` 各通过 28,672 项严格 FP 谓词/结果/Invalid 异常检查。
28 组包含新恢复工具链的旧 MIPS O32/N64 大小端，以及 R6 N32；之前的
缺失 SDK 记录仍保留，不能把后来的成功写回旧失败记录。
48 注册名仍为 contract 41/48、SIMD 37/48；上述 GPU/字节码边界没有改变。
进一步在 `release3-target-objects-01` 对成功的汇编 probe 实际运行 `-filetype=obj`：
19 个变体均通过，但 XCore 没有 MCCodeEmitter，NVPTX 两个别名的 contract
也不能直接输出对象。这些是汇编输出成功、直接对象输出失败的额外边界，
不能把 37 个 SIMD 汇编成功目标称作 37 个 MCJIT 宿主。此前未成功的 probe
原样保留，没有因为未进入对象阶段而标成通过。

`.3` 完整模块构建的 SSH 会话在约 52% 中断，未写出完整结果；
`ros-cache5-modules-release3-01/result.json` 中 configure 的返回零不是构建成功。
`ros-cache5-modules-release3-resume-01` 在重新核对同一完整源码清单后续建，
只有该次续建结束、产物哈希与前后清单验证完成，才能用于后续模块运行测试。
这些结果不包含下面尚在诊断的 compressed-MIPS 候选补丁。

新增 8 组 MIPS 子 ISA 探测发现 microMIPS 严格算术/min-max 缺失指令模式、
MIPS16 为带 metadata 的 intrinsic 建立非法本机调用 stub、以及 select 分块丢失
活动调用帧大小。`mips-additional-subisa-06` 的候选代码生成 8/8 通过，
但这仅检查汇编输出。`mips-compressed-execution-01` 进一步表明：
MIPS16 直接生成对象仍因未展开 pseudo/重定位失败；microMIPS R6 执行也失败。
不能将这一轮候选称为正式 vendored LLVM、对象生成或执行通过。

microMIPS 的实际运行继续揭示：紧凑返回选择了 pre-R6 JRC 编码（R6 解读为
MOVEP），常量装载的外部模式未继承目标指令的 ISA 限制，FP 条件寄存器搬运、
PIC prologue 与物理 FP COPY 的 MC 编码映射还有缺口。
`mips-compressed-regressions-09` 的五个独立测试共 26 项前后对照通过（包含
R2 正向控制），但 `mips-micro-repaired-execution-03` 仍有运行失败，候选还在修复。
初次尾调用 fixture 的参数数量不匹配、随后遗漏 LLVM 的 `-mips-tail-calls` 开关，
对应失败也予以保留；只有修正 fixture 后的前后对照用于后端结论。
这组诊断没有关闭 verifier，也没有将非严格浮点替代严格浮点。

`mips-compressed-execution-01/objects.json` 确认候选 02 对之前 28 个已执行对象
逐字节无变化；该结论不能自动推广到后续候选。
新增 R6 MSA O32/N32 两组 SIMD+guard 执行通过；MIPS16 外部 GAS 实验仍失败，
更没有把 GAS 引入生产 MCJIT 作为静默替代。

后续候选 `mips-compressed-repair-07` 在补齐 PIC/FP COPY/PseudoCVT 的 MC 映射后，
`mips-micro-repaired-execution-05` 的 236 个 SIMD opcode 与 guarded-store 全部通过；
`mips-micro-strict-predicates-01` 以真正的 `+micromips` 对象通过 28,672 项严格
谓词/结果/Invalid 检查。`mips-compressed-regressions-10` 的六个最小回归共有
30 项前后对照通过，包含新加入的实际编码/重定位检查和不变的 R2 正向控制。
候选 06 的首次 relink 因清理了旧构建工具入口对象而失败；
`link-recovery.json` 记录在候选目录中重新编译两个入口对象后成功，没有改写
生产库或伪造首次成功。候选 07 使用这个已补齐依赖的编译器继续验证。

Mac ROS 工作区已纳入这组修复，pin 更新为 `23.1.1-uwvm-ros.4`，对应缓存不能
复用 `.3` 对象；Linux 正在构建的 `.3` 快照没有被修改。与官方源码重新比对
通过：12,862 个保留文件、15 个编译器源码修改、13 个新增测试，35 条 RUN。
首次来源检查与 manifest 重建并发，读到了尚未完成的清单而失败；等待清单
完成后的完整重跑才通过。正式 `.4` 构建/测试尚未完成，不能用诊断结果替代。

`release3-unwind-units` 的新编译 runtime、无 instruction 残余的 IR 检查和
native provider 递归/缓存测试均通过；它们仍是 `.3` 的 header 单元测试，
不是尚在续建的模块可执行文件，也不是所有目标平台的动态 CFI 验证。

### 原生目标选择、对象输出与最新补测

`getProcessTriple()` 会按照指针宽度缩窄架构。MIPS N32 的指针虽为 32 位，
ISA 仍是 MIPS64；默认选择将其变为 MIPS32 + N32，触发 LLVM 的
`64-bit code requested on a subtarget that doesn't support it` 中止。
ROS full/preopt 尚未采用普通版已有的显式目标选择；两个产品的实时 unwind
探针也都绕过了实际运行时的选择。这些路径已改为复用宿主 CPU/feature/ABI
选择，并归一化默认三元组，避免三个字段的拼写丢失 ABI environment。
ROS CMake 合同同时要求默认三元组与宿主三元组一致：LLVM 允许为 AOT 设置
不同默认目标，但原生 MCJIT 不能按那个目标直接执行。新增配置反向测试检查
该覆盖在编译前被拒绝；显式逐模块 AOT 目标不受此限制。
普通版 full/lazy 已有的 AArch64 LLVM 22 workaround 保持原样，未无依据复制到
固定 LLVM 23 的 ROS。该修复不改变每条 guest 指令的执行路径。

新增 `check_native_target_selection.py` 编译从生产源码提取的完整选择函数体，
以 ELF `--wrap` 注入默认三元组。容器/字符串适配为测试替身，目标选择与
对象编码使用真实 LLVM；这不是交叉构建完整 N32 LLVM 库或 native VM。
`native-target-ros-02` 的 20 项与 `native-target-full-01` 的 full/lazy 各 20 项
均通过：包含 N32 大小端、R6、GNU/musl、x32、aarch64_32、VE `-vpu`，以及
8 个旧隐式选择的反向对照。首次 ROS 调用因同步目录层级错误找不到 fixture，
未进入编译；该失败不被删除，也不算作后端失败或通过。

`native-target-runtime-unwind-01` 另行重编译了带目标修复的 ROS runtime 与两个
unwind 单元：native IR 的 push/pop 为 0/0，instruction 对照为 2/2；provider
完成 20 条递归链、每条五个精确 JIT 帧、10 次对象编译与 30 次缓存重放。
这五步均通过。依赖仍来自冻结的 `.3` 库，候选文件另有哈希记录，因此它是
混合诊断快照，不冒充完整 `.4` 源码身份或最终模块构建。

`check_all_llvm_targets.py` 现在必须同时通过 asm 和 object 阶段；记录独立
`assembly_passed` / `object_passed`，对象失败仍保留汇编证据，默认退出非零。
`release3-complete-object-inventory-01` 重新跑了全部 48 注册名 + 19 个变体：
19 个变体的两个 probe 均通过（包括生成代码的 x86_64 no-SSE），48 注册名中
contract 对象为 38/48、SIMD 对象为 36/48。GPU/字节码 11 项和 XCore 的原有
限制仍记录为失败，没有改成跳过或通过；这不证明全宿主 no-SSE C++ 构建成立。

`mips-compressed-objects-07` 用最新候选 07 再次生成之前已执行的 28 个对象，
全部与 `.3` 逐字节相同。`mips16-host-interworking-01` 的大小端宿主 fixture
由 GCC `-mips16` 编译，调用标准 MIPS JIT 对象；两组 236 SIMD + guarded-store
执行均通过。它证明这组宿主互操作，不修复或掩盖 LLVM 原生 MIPS16 对象编码缺口。

第一轮模块续建在归档 SDK 完成前触发 2 GiB 磁盘保护，返回 -15；源码清单不变，
无新增 OOM kill。`ros-cache5-modules-release3-resume-02` 继续使用原清单与 BMI，
前一轮失败日志保留。尚未得到完整构建结果，不能标为模块验证完成。

第二轮续建的 SSH 又返回 255，未形成结果文件、无新增 OOM kill；不能将此归为
编译器失败或成功。第三轮改用 `uwvm-ros-release3-module-resume03.service`，在
同一 `uwvm-comprehensive.slice` 内运行，MemoryMax=32 GiB、SwapMax=0、CPU=16–31，
不再依赖 SSH 会话存活。结果目录为 `ros-cache5-modules-release3-resume-03`；
截至该记录，仍待服务完成后的文件哈希和运行测试。

新增 native capability 检查不是 AOT 白名单：LLVM `EngineBuilder::create` 对
`hasJIT()==false` 只打印 warning，仍继续创建引擎。因此 materialization、preopt
和 live probe 提前拒绝不支持的 native JIT/MC backend；BPF 虽标记 `hasJIT`，
其字节码也不实现这里的进程内 C ABI，需明确拒绝。
`native-target-ros-04` 的 27 项与 `native-target-full-03` 的 full/lazy 各 27 项
通过，包含 8 类不支持目标的受控拒绝；这 24 项拒绝检查绝不是平台执行通过。
前一轮 ROS-03/full-02 确实漏过 BPF，失败证据保留，不能省略该修复过程。
该检查不改变跨目标 IR/对象生成工具，也不证明 `hasJIT` 为真的所有 OS/ABI 都能运行。

为避免测试本身占满 tmpfs，SIMD fixture 增加显式 `--ir-only`，默认跨执行模式
仍生成完整 C oracle。`ir-only-mode-01` 的 67 个 IR 与原输出逐字节一致，native
SIMD 执行通过，默认完整 oracle 也逐字节一致，并检查错参数不能静默转为 native。
`release3-complete-object-inventory-ir-only-02` 再次跑完 67 组，原有 12 组失败完整
保留、退出非零；不会因测试模式省略 C 文件而少检查汇编或对象输出。

`.4` 源码已另置于 `ros-vendor23-release4`，未覆盖 `.3` 构建输入。
`release4-prebuild-02` 的来源比对、包装/路径/清单、编译器正反控制与真实 CMake
配置共 7 组通过。最初 CMake 控制失败包含诊断换行导致的字符串匹配问题、以及
默认目标检查遮盖缺少 Native backend 的主诊断；均修正后完整重跑，原失败保留。
这仍不是 `.4` LLVM 库、完整 CLI 或最终 named modules 的构建结论；其后新增的
native capability gate 和 IR-only fixture 还需要在最终冻结清单中纳入。

本 agent 及所有构建/测试子进程仍在同一个 32 GiB aggregate cgroup，MemoryHigh=28 GiB、
SwapMax=0、CPU=16–31；不是每个子任务各占 32 GiB。
旧 v4 module binary 留在 `/var/tmp/uwvm-comprehensive.kH4XCt/ros-cache4-module-baseline/uwvm`。
只删除已核实可再生成的旧 BMI/对象，以及已归档的生成 C oracle；不删除用户源码或私有缓存。

### 对象加载与 ABI 安全拒绝（不是新增平台支持）

`mcjit_target_support.h` 最初在两产品中使用相同规则，文件各自保留在仓库内；
后来针对 AArch64_BE 的差异见下方 `.5` 修复，不能再无条件同步该项白名单。
它区分 ELF/Mach-O/COFF 的 RuntimeDyld 实现，拒绝 XCOFF/GOFF 等不支持的对象格式；
不能只看 LLVM 架构注册的 `hasJIT` 位。AArch64 ELF ILP32 的 `R_AARCH64_P32_*`
也不在现有 resolver 中，区别于已有 Mach-O `aarch64_32` 支持。
`aarch64-ilp32-old-gate-01` 保留了旧 gate 放过三种 ELF ILP32 拼写的反例。

`native-target-ros-08` 的 45 项、`ordinary-llvm22-native-target-04` 的 full/lazy
各 45 项通过。这些是选择/对象编码/预期拒绝控制，不是 135 次跨平台 VM 执行。
普通版用真实 LLVM 22.1.8 开发档案验证了旧 API 及 libstdc++ ABI；ROS 仍只链接
vendored LLVM 23。最初普通版链接因 SDK lld 的动态库路径缺失而失败，补齐环境后
重跑；未将环境失败隐藏为编译成功。目标查询采用前后版本均有的三参数重载。

`native-target-runtime-unwind-03` 含新的共享 helper，使用独立的组合源码身份：
5 步通过，native IR push/pop 为 0/0、instruction 对照为 2/2，provider 为
20 条递归链 × 5 精确帧、10 次对象编译、30 次缓存重放。它早于下面最后追加的
PPC32/Thumb/ARM BE 拒绝，仍不是完整 `.4` named-modules 构建结论。

新增 `check_mcjit_object_loading.py` 使用 vendored `llvm-rtdyld -verify` 对原
67 组 codegen inventory 的可用对象逐个加载重定位，外部符号使用记录的虚拟地址。
不执行对象，不验证外部 helper 实现或 native CFI，且没有把缺失对象改标为成功。
`release3-mcjit-object-loading-02` 完成全部记录并返回非零。首次调用使用了不存在的
系统 llvm-nm 路径，未进入目标测试；随后从同一 vendored 源码构建两个检查工具。

该检查实际复现 PPC32/LE 的 REL24/REL32 缺口和 Thumb ELF 调用重定位失败。
更重要的是 `arm-loader-endianness-02` 的字节断言：小端正向控制通过，大端
ABS32 把预期 `0x11223344` 写为 `0x44332211`。最初 fixture 错用了 IR 注释前缀，
llvm-rtdyld 没有识别规则；改用独立 `.check` 文件的正反控制才用于结论。
当前 native gate 拒绝上述 ELF 路径，避免直接进入错误装载器；Mach-O/COFF 的
不同 resolver 单独判断，Windows Thumb 正向控制仍通过。这只是安全收口，
**不声称已经补齐这些 LLVM loader/ABI 的实现，更不把拒绝计作平台支持通过**。
原 AOT 枚举和失败证据不受 gate 过滤。

为保持 aggregate 内存空间，`retired-native-probe-binaries-01.json` 记录清理的
13 个旧 probe（1,719,969,568 字节）；`retired-superseded-codegen-binaries-01.json`
记录 28 个旧诊断工具（3,266,527,992 字节）。仅删除已完成且不在运行的可重建 ELF
文件，保留源码/生成对象/构建命令/结果/日志/二进制哈希；当前生产工具、最新
microMIPS 06/07、完整 module baseline 和唯一归档均未删除。

为模块构建腾出磁盘，10 个本轮下载的交叉 SDK 解包目录已逐成员校验并归档到
`/var/tmp/uwvm-comprehensive.kH4XCt/sdk-unpacked-20260917.tar.gz`，SHA256：
`b11b5e0d37599ab16b01924d17481d64d4bfa235e7cf2a1fe1b8b3aab41e1afb`。
`archived-idle-sdks-01.json` 保留文件哈希和精确删除目录；净释放约 3 GiB。
它是 SDK 的可恢复备份，后续跨执行前必须恢复对应成员；当前 native LLVM/module
构建不使用这些 SDK。不要删除这份仅存于 Linux 的归档。

另有两份新归档在上述 Linux 目录和 Mac `/tmp/uwvm-ros-llvm23.GQTbOH` 均完成校验：

- `retired-object-inventory-oracles-20260917.tar.gz`，SHA256
  `60de615afff0d76dacc9d378b96fea0ce36bdf35025089acbd4a1fb82ea6123b`：
  67 个不用执行的生成 C 文件，清理 1,630,445,804 字节；IR、对象和失败证据保留。
- `retired-llvm12-libdirs-20260917.tar.gz`，SHA256
  `2f547e138191678eae0fecfb868883d7a121e2d1dbeb518e4692e8274383b6bd`：
  旧 `.1/.2` 的两个精确 lib 目录，1,826 个文件、1,874,270,739 字节；
  `.3` 构建和所有 bin 目录不动。旧 relink/重编译前必须先恢复这些库/生成头文件。

本轮 168 个生成 C 文件（4,088,282,016 字节）已逐成员校验并归档到
`retired-repair-oracles-20260917.tar.gz`，SHA256：
`69a92b13f57b7d386c733a81f7694cdd053f058c253e40d49ff573698651c269`。
Linux `/var/tmp/uwvm-comprehensive.kH4XCt` 与 Mac `/tmp/uwvm-ros-llvm23.GQTbOH` 均有已校验副本。
释放这些中间文件后，IR、汇编、对象、程序和失败日志仍在原路径，生成 C 可从归档恢复。

已完成的 codegen / 后端回归 / 包装合同 / 初版补充 fixture / 宿主 noSSE 失败证据另存
`ros-llvm23-release2-codegen-evidence-20260917.tar.gz`（4,844 个文本记录），SHA256：
`8e296ef19b1791bb186388e78964cfa37a6b820fb649f992842ed88a48af9525`，同样已校验上述两处副本。
该归档先于新增的退出状态回归，不包含进行中的模块构建结果。

### 完整模块构建完成及其实际回归边界

`ros-cache5-modules-release3-resume-03` 最终返回 0，冻结源码清单未改变。
清单 SHA256 为 `bc45f8e65b395c67b5e75cf8ed11220dc71514b36c7cf5b52bfc538225153ce1`；
CLI SHA256 为 `7e06044acbc73dde51ee3f34e940623fd1cda9460f44ffa5d6308afaca5ba140`。
已另存 `ros-cache5-release3-module-baseline/uwvm`，不依赖后续可清理的 BMI。
这是 cache v5 / LLVM 下游 `.3` 的完整模块构建，不是后来 `.4/.5` 的结论。

`release3-modules-functional-01` 的 7 组 driver 全部通过：unwind 108 次 CLI
调用和 72 次已签名缓存 trap 重放；Core 2 共 2,296 次执行、83,152 个断言，
仍明确记录 4,673 个排除项；memory 652；integration 65；浮点转换；栈耗尽
15 场景 × 10 配置；解释器汇编 18 个函数。`release3-modules-cache-fixture-01`
另行通过签名、损坏恢复、IR 形状、路径隔离及 12 个 fuzz 恢复控制。
性能和整对象对比留待重型构建停止后执行，不能把功能通过写成性能通过。

### CFI 结构检查不是原生 unwind 执行

`release3-unwind-objects-03` 对 67 组 contract 对象都尝试检查；22 组仍为失败／
未验证，不以缺少对象或 decoder 能力标为成功。前两轮还暴露测试自身的假设：
Mach-O 叶函数可以只有 compact unwind；Thumb 符号地址带 ISA 位，而 EHABI
区间地址没有。修正后仍分别核对函数覆盖，不把两类重叠记录简单相加。

`release3-unwind-alternate-decoder-01` 使用独立 GNU readelf 复查 ARM_BE、Thumb_BE、
SPARC 三种目标及 SystemZ 的结构记录，均能解码；AVR/VE 仍缺少所需记录。
GNU 工具只是检查器，绝不是 ROS 的 LLVM 依赖。这些检查不能证明运行时 CFI
注册、物理 prologue 与 CFI 一致，或外部平台的递归/trap 回溯实际工作。

### `.5`：AArch64 加载成功仍可能得到错误指令

`aarch64-loader-stub-01` 首先复现 ELF BE far-call stub 字节序错误。AArch64 的
指令固定小端，与 ELF 数据端序不同；RuntimeDyld 原来按数据端序写五条 stub
指令。`aarch64-rtdyld-regressions-01` 还复现 LDR literal / ADR 位移的 `0xffc`
掩码错误：它截掉合法 ±1 MiB 范围内超过 4 KiB 的高位，大小端都会受影响。
同时把三个 resolver 的指令 read/modify/write 改为显式 little-endian，避免
依赖 loader 宿主端序。这些是加载期操作，不增加 Wasm 热路径指令或 stub 长度。

隔离候选保留旧 loader 的失败对照，8 组正反控制通过；新增正式保留的 8 条
RUN 也在 `aarch64-retained-candidate-01` 通过。候选只私有重编两个 RuntimeDyld
对象，没有修改已冻结的 `.3/.4` 生产库。不能将此扩写为 `.5` 完整构建成功。
期望位编码来自 AAELF64，测试核对全部 5 条 stub 指令、正负长位移及 ADR 低位；
源码旁保留原因，详见 vendor 的 UPSTREAM_CHANGES。

因此 ROS 下游 pin 升到 `23.1.1-uwvm-ros.5`，与 cache 文件格式 v5 是两个独立
版本维度；17 个编译器文件修改、19 个回归文件、43 条 RUN。公共头的版本检查
也核对整个后缀，拒绝同为 23.1.1 的未打补丁系统头与旧 `.4` 头。它不能替代
xmake 的源码/静态库认证。Mac Clang 的 8 个合成版本正反控制通过。

普通版依赖外部 LLVM，不能假定安装了 ROS 的 RuntimeDyld 修复，因此其 full、
lazy 与 live probe 共用的 gate 拒绝 AArch64_BE ELF；ROS 则依赖受 pin 保护的
修复库。`native-target-ros-09` 为 46 项，`ordinary-llvm22-native-target-05`
full/lazy 各 46 项均通过。这些仍只是目标选择／对象生成／预期拒绝测试，绝不
把拒绝当作 AArch64_BE 原生执行通过，也不证明普通外部 LLVM 的所有 relocation
缺陷均已修复。`.4` 完整生产构建保持原冻结输入，最新 `.5` 必须另行完成。
# September 17 regression continuation: completed `.5` baseline, `.6` repair

This section records a later snapshot; earlier failed/intermediate results below
remain historical evidence. Do not apply a completed snapshot's PASS to newer
workspace sources.

* `.5` header/LLVM build: `release5-production-build-01/result.json`, source
  manifest `b0e8091a609cff4241a53fbb50a83247b4d700573958c840d1789cc4bd0d2774`,
  executable `7da733a219b98bcfc367d6faeb494e79f92883ca17d44da32142c37e2fce5553`.
  The build finished successfully and rechecked unchanged source bytes.
* `.5` codegen: `release5-codegen-tools` completed all four fixture builds,
  native SIMD, 77 NaN checks, 43 retained backend/RuntimeDyld RUN steps and
  46 native-selection controls. The unfiltered 67-profile inventory remains
  57 contract / 55 SIMD object successes: **not all-platform PASS**.
* `.5` named modules: started in a fresh directory using the authenticated `.5`
  archives, still running when this section was written. The independently
  completed `release5-header-functional-01` passed all seven groups: unwind,
  Core 2 execution, memory boundaries, integration, conversions, exhaustion
  and interpreter assembly. These are header-CLI results, not module results.
* A new real loader defect was isolated and fixed in local `.6` sources:
  duplicate ELF local names corrupt RISC-V/LoongArch FDE relocations. See
  [the repair, ordinary-version workaround and precise test boundaries](elf-local-symbol-unwind.md).
  `.6` uses a new package identity, explicit cache policy and 49 retained RUN
  steps. Private repair tests and `.5` builds do not establish a complete `.6`
  package/module validation.
* Space reclaimed earlier: the completed `.3` module `.gens` and `.objs`
  intermediates only, 11,023,568,091 bytes. The tested CLI and hashes remain;
  these intermediates can be recreated by rebuilding, not by an archive restore.
  Ledger: `retired-release3-module-intermediates-01.json`.
  Subsequently retired only rebuildable `.3`/`.4` static archives and compiler
  objects: 426 archives / 661,651,704 bytes and 5,918 objects / 610,903,256 bytes.
  Both tested CLIs were hash-verified first; tools, source and generated headers
  remain. Recovery is by recompilation, not an archive restore. Ledgers:
  `retired-llvm34-static-archives-01.json`, `retired-llvm34-objects-01.json`.
* The aggregate Linux slice still has MemoryMax 32 GiB, MemorySwapMax 0 and
  CPU affinity 16–31. The OOM/kill counters did not increase during these tests.

The later MIPS direct-call control additionally required full-width C-ABI calls;
its measured instruction-size cost and pending ABI coverage are documented in
the linked repair note. Do not extend the local-label patch's byte-equality
result to the MIPS call-range change.
