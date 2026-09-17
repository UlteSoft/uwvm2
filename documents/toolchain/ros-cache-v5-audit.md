# ROS cache v5 / LLVM 23.1.1 复核（2026-09-17）

这是 `.1` 首轮快照的历史结果；VE/AVR/MSP430、x86_64 noSSE 修复及 `.2` 重跑见
[后续目标修复报告](ros-llvm23-target-repair-audit.md)。下面的旧计数没有改写成新结果。

本报告区分“真实运行”“代码生成”“仍失败／未验证”。不能概括为全平台无 bug。
缓存改动与两版布局见 [产品隔离说明](llvm-jit-cache-product-isolation.md)。

## 本轮源码与资源限制

ROS CLI 快照 `cd239b3f973c3bcb2b05d0fb92ec9ace522e62f16d54914221d63427fdebd766`，
Clang 22.1.8 / header build / 仓库固定 LLVM 23.1.1-uwvm-ros.1（含原 MIPS 修复），
构建前后 manifest 一致；没有调用或采用系统 llvm-config。
CLI SHA256：`04c00db9c18fd23dff1aa2059bb3fea99e872cb831ca1c0b51cf3187ddf784d0`。
快照之后仅增加／加强测试与报告，生产缓存代码未再变化。

Linux 每个测试、编译及其所有子进程均进入同一个 aggregate cgroup：
MemoryMax=32 GiB、MemoryHigh=28 GiB、MemorySwapMax=0、AllowedCPUs=16–31。
未借用另一 agent 的 32 GiB。没有删除用户源码或旧用户缓存。

## 实际执行通过

| 检查 | 本轮结果 |
| --- | --- |
| 两版缓存产品隔离 fixture | 20 次调用通过，含 unsigned API、双向外来文件、改 magic、v4、坏签名 |
| ROS 完整缓存集成 | 路径／默认目录／异步写入错误／真实命中／损坏重编译／12 次确定性 fuzz 恢复通过 |
| Core 2 语料 | 2,296 次 ROS 执行，83,152 次断言，0 失败；仍有 4,673 项明确排除 |
| SIMD 跨页／grow | 652 项通过 |
| full feature / import alias / DataCount | 65 项通过 |
| mixed signedness conversion | 13 次执行、104 断言、6 次认证缓存回放 |
| 栈耗尽 | 15 场景 × 10 profile = 150 次通过，含 libunwind |
| native-unwind 递归 trap + 持久缓存 | 108 次 CLI 执行，其中 72 次认证缓存回放；unreachable/oob/float-to-int |
| 实际链接的 native provider | 20 条递归链，每条精确 5 个 JIT frame；10 次 object 编译、30 次内存 cache 回放 |
| 真实 Wasm 翻译 IR | instruction: push=2/pop=2；native-unwind: push=0/pop=0/indirect-host=0；4 个生成定义 |
| 强化标量 NaN | 77 个 O3 native case 通过，分别约束 canonical 和 arithmetic payload |
| SIMD 原生／QEMU | 15 个配置，每个执行全部 236 个 opcode，另有跨页 guarded-store fault oracle |
| uwvm-int 实际汇编 | 18 个选定的 f32/f64/f32x4/f64x2 add 函数，原生加法＋尾分派，无函数内 call 或栈访问 |

15 个执行配置：x86_64、i686-x87、i686-sse2、aarch64、aarch64-nosimd、arm-neon、
ppc32、ppc64be、ppc64le、riscv64、riscv64-v、s390x、loongarch-lsx、loongarch-lasx、sparc64。
规范化 triple 后，15 项已用重新构建的 emitter/finalizer 全部重跑通过。
这里的 QEMU 检验的是生成对象及整数 buffer ABI oracle，不是这些平台的完整 CLI/动态装载/OS unwind 集成。

NaN 判据参考 [Core 2.0 §4.3](https://webassembly.github.io/spec/versions/core/WebAssembly-2.0.pdf)：
canonical-only NaN 输入要求 canonical payload；其他 NaN 输入允许 arithmetic payload，但不能返回 signaling payload。
位搬运、重解释和规定保持 payload 的操作不能套用任意 NaN 归一化比较。
本轮发现标量测试原来只查 quiet bit，补充了 canonical payload 和正负非 canonical qNaN；未发现本轮执行中的生产计算错误。

## 全注册目标代码生成

`check_all_llvm_targets.py` 读取实际 `llc --version` 的 48 个注册名（**包含别名，不是 48 个独立 ISA**），
不采用手写“常见平台”清单来代替枚举。对每个目标运行 production FP lowering、O3、最终向量 legalization、
`llc -verify-machineinstrs` 并保留汇编／失败诊断。

两个 probe：生产函数／unwind 属性与标量 FP 的 portable buffer contract；全部 SIMD opcode emitter。
前者不是完整 Wasm 跨编译运行时；宿主相关 C++ ABI／桥接调用约定不能靠改 triple 验证。

规范化 triple 后的重跑结果：contract **40/48**、SIMD **34/48** 代码生成通过。两项均通过的目标包含 AArch64 各 endian/ILP32、
ARM/Thumb 双 endian、Hexagon、Lanai、LoongArch32/64、MIPS32/64 双 endian、PPC32/64 双 endian、
RISC-V32/64 双 endian、SPARC variants、SystemZ、Wasm32/64、x86/i686、x86_64、XCore。
WebAssembly 后端生成通过不代表提供了宿主原生 unwind；CFI 的存在也不代表 runtime 已能注册、解析它。

未通过项保留，不算 PASS：

- AMDGPU（含别名 amdgcn）、R600、BPF 三种 endian 名：FP helper/libcall lowering 不可用。
- AVR SIMD：LLVM 转换产生 vector/scalar trunc 类型不匹配。
- MSP430 SIMD：LLVM machine verifier 报 call-frame-size 不一致。
- NVPTX32/64 SIMD：library-call lowering 不支持。
- SPIR-V logical contract/SIMD：pointer-cast legalization 崩溃；SPIR-V32/64 SIMD 在宽 guarded-store vector legalization 失败。
- VE contract/SIMD：machine verifier 报未定义物理寄存器。尚未完成最小化和真实 VE 执行，不能判定为无害，也不能直接宣称已定位错误代码。

这些不是 ROS 现有主机 JIT 支持范围的自动扩展。不能为消除红项关闭 verifier 或删除检查。
尤其 AVR/MSP430/VE 的失败仍需要独立后端定位，本轮没有擅自把额外补丁加入固定 LLVM 正式版源码。

## 额外 ISA / ABI / OS codegen probe

`fixtures/llvm_target_variants.json` 的 19 项中 contract **19/19**、SIMD **18/19**。
这两个计数也已由规范化 triple 和显式 x86-extended 模式的重跑确认。
包含 i686 x87、AArch64 no-SIMD/SVE2、PPC64 no-VSX 双 endian、LoongArch no-SIMD/LASX、
MIPS32r2 legacy NaN 双 endian、MIPS N32 双 endian、RV32 soft-float、RV64 vector、ARMv5 soft-float，
以及 x86_64/AArch64 Windows 和 Darwin 的对象属性 probe。

复核还发现测试工具自身的 ABI 问题：`llvm::Triple` 构造器不负责规范化简写；
`mips64-linux-gnuabin32` 曾使 emitter 获取 N64 DataLayout，然后 llc 又按显式 N32 发射。
已在两版 SIMD emitter 和 ROS contract probe 中先调用 `Triple::normalize`，并断言 N32 指针为 32 位。
**首轮 N32 记录因此撤销为有效 ABI 覆盖，必须以修正工具后的独立重跑记录为准。**
修正后的大小端 N32 contract 与 SIMD 均通过；IR 明确为
`mips64[el]-unknown-linux-gnuabin32`、DataLayout 包含 `p:32:32`，仍不宣称 N32 完整 VM 执行通过。
另为 x86_64 no-SSE 提供显式 `x86-extended` lowering 模式，避免把发射器宿主的 SSE2 默认宏带入测试。

失败项是强制 `x86_64 -sse,-sse2` 的 SIMD rounding libcall：LLVM 报 `SSE register return with SSE disabled`。
不能与“用关闭 SSE 的 C++ 参数构建解释器”混为一谈；现有 JIT 使用 `getHostCPUFeatures`/`setMAttrs`
选择本机代码特征，并不从解释器编译宏直接继承禁用 SSE。此人工目标配置未通过，尚无完整 no-SSE CLI/JIT 通过结论。
Windows/Darwin 这里只检查 cross codegen，未执行真实平台的 SEH／signal trampoline。

## 无 instruction 残留的含义

native 模式在生成前确定，和 instruction tracking 是二选一，不是用 CFI 加速后仍偷偷维护逻辑栈。
真实 IR 的正反对照、provider 递归以及三个 trap 的新进程缓存回放分别验证发射、CFI 注册和运行时 frame 恢复。
[LLVM 的 unwind 机制](https://llvm.org/docs/ExceptionHandling.html) 依赖匹配的平台表及运行时支持；
仅出现 `.cfi_startproc`/`.seh_proc` 不是充分条件。
当前 native platform allow-list 之外仍明确采用 instruction fallback，不声称所有 ISA 已有 native libunwind。

## 性能与尚未闭环项

同 Clang 22、同 bundled LLVM、header 模式，旧 v4 对比本轮 v5，31 轮交替基准：
JIT 中位时间比 **1.0000488**，解释器 **0.9754480**。这不是所有负载的性能保证；
解释器差异不能归因于缓存格式优化。
3 个 scalar/f64/v128 核心函数、全部 1,574 字节 text 及完整 JIT object **逐字节一致**。
因此本次产品隔离没有给这组生成的热路径增加检查／调用。

v5 的完整 named-modules 重构建尚未重跑，上一轮 v4 的 modules 通过记录不能冒充 v5 验证。
两版本轮模块依赖／顺序静态检查通过；它不替代实际 BMI 编译链接。
普通版完成共享缓存代码与标量测试同步，以及当前头文件的隔离 fixture；本轮重点 CLI/跨目标执行是 ROS。
全平台完整 CLI、所有子 ISA、所有 OS unwind、官方有状态脚本／外部模块图仍不能声称全量通过。

Linux 原始证据前缀：`/tmp/uwvm-comprehensive.eUFnxH/ros-cache5-*`，隔离 fixture 在 `cache5-fixtures-06`。

首轮证据归档 `ros-cache-v5-all-targets-evidence-20260917.tar.gz`：6,632 个文本记录，
SHA256 `bc1f56571eaf912778b820f742314ec2a7a85903c248fd04df0f7a1f6291b244`。
已逐成员校验，并在 Linux `/var/tmp/uwvm-comprehensive.kH4XCt/` 和
Mac `/tmp/uwvm-ros-llvm23.GQTbOH/` 保存两份 SHA256 相同的副本。
归档不含用户私有缓存 payload／签名材料，不包含完整可执行文件。

规范化 triple 后的最终补充归档 `ros-cache-v5-normalized-targets-evidence-20260917.tar.gz`：
2,142 个文本记录，SHA256 `86f0092217b94f62111037f2977952547bed4584afcf7b34ecc90e6d04ca5f2d`，
同样已在上述两处保存并校验。最终 cross-codegen／15 组执行以其中
`ros-cache5-normalized-all`、`ros-cache5-normalized-variants`、`ros-cache5-normalized-exec` 为准。
