# 浮点修复为什么这样写：维护注释与回归索引

本文补充代码旁的注释，覆盖下表中的浮点修复链，不引入新的执行逻辑。这里的 sNaN 指 signaling NaN，qNaN 指 quiet NaN；“能传 NaN”不等于“能逐位传递 sNaN”。

后续新执行的 RV32、MIPS N32/R6、大端/软浮点及 s390x 生产入口补测与新增代码修复，另见[跨架构补测记录](floating-point-cross-coverage.md)；不要把本文末尾的历史“仅补注释”验证与后续执行覆盖混为一谈。

## 对应提交

| 主仓库提交 | 修复目的 | ROS 对应 |
|---|---|---|
| `8d398d5eb` | 解释器入口及 native callback 的浮点环境边界 | 纳入 `0f7f5205` |
| `7d09288f3` | 跨架构控制寄存器、libc 回退、显式契约快路径 | `0f7f5205` |
| `e4d032a90` | x87/68881 单次正确舍入与常见值快路径 | `e6b9e83e` |
| `c1a55bfa5302c35c80e110f3a6ea49d8feb0afa1` | 从解析到解释器/JIT ABI 的原始浮点位保留 | `75b15b26` |
| `f103770f1` | 原生 NaN 编码、升降精度、整数舍入、RISC-V LLVM 舍入 | `77b66e98` |

ROS 只同步其已有功能对应的修复与说明；表中不表示为 ROS 恢复 SIMD、lazy 或 tiered 实现。原有测试配置、汇编与性能记录分别见 [环境与位模式审计](floating-point-environment.md) 和 [原生 NaN 补充审计](floating-point-native-nan.md)。

2026-09-15 后续验证说明：当前两仓已有的 SIMD 实现已纳入配对测试，ROS 的 i686 浮点/SIMD 矩阵也已完成。下文历史条目的“主仓库”标签不是当前 ROS 不含 SIMD 的声明；实际快照、测试数量和未完成范围以[配对平台验证记录](../../test/0014.llvm_jit/PAIRED_PLATFORM_VALIDATION.md)为准。同步仍不恢复 ROS 已删除的 lazy/tiered 模式。

## 1. GCC 无优化时为什么可能传不对 sNaN

问题不是“所有 GCC 都不能传 NaN”，也不是 `memcpy` 或 `std::bit_cast` 的语言语义允许任意改位。触发点是生成的浮点临时值、复制以及调用 ABI。

在已复现的 GCC 15/i386 配置中，普通浮点返回经 x87 ST0；启用 `-msse2 -mfpmath=sse` 并不把普通 C/C++ 浮点返回 ABI 一并改成整数或 SSE 返回。x87 装载/返回一个 sNaN 时可能将其静默化。GCC `-O0` 保留的辅助函数调用容易暴露这个边界；优化后的内联、常量传播可能恰好消除它，不能以 `-O3` 通过作为跨优化级别的契约。审计还遇到 GCC 无优化的 `_Float64` 位转换/复制路径生成 `fld/fst`，以及 68881 浮点搬运的同类风险。

例如，下面这种接口**不是位精确传输接口**：

```cpp
float decode(std::uint32_t bits)
{
    return std::bit_cast<float>(bits);
}
```

即使创建出的对象表示正确，随后返回到调用者的过程仍可能改写 signaling bit。再在调用者里 `bit_cast` 回整数已经太晚。把结果改成 `volatile`，或者事后恢复 fenv，也无法找回丢失的位。

因此需要从输入到最终存储都不经过浮点值 ABI，例如：

```cpp
void decode(std::uint32_t bits, float& out)
{
    static_assert(sizeof(bits) == sizeof(out));
    std::memcpy(&out, &bits, sizeof(out));
}
```

这个例子假定整数已按 Wasm little-endian 解码；生产代码还必须保证调用者后续继续以字节复制，而不是紧接着又返回 `Float`。这正是输出引用、`const&`、整数载体、直接 operand-stack copy 要一起改的原因，不是为了绕开某一处编译警告。

## 2. 三种保证不能混为一谈

| 操作/边界 | 必须保证什么 | 不能拿什么代替 |
|---|---|---|
| 常量、locals/globals、load/store、调用、select、reinterpret、SIMD lane 搬运 | 被选中或搬运的位模式不变 | 不能统一把 NaN canonicalize |
| abs、neg、copysign | 只改规定的符号位，保留其余位 | 不能先经过会 quiet sNaN 的浮点返回 |
| 算术、sqrt、ceil/floor/trunc/nearest、数值升降精度 | 正确舍入、signed zero/subnormal 与允许的算术 NaN | 不能以“保留输入 sNaN”为正确结果 |
| 进入 Wasm / native callback 返回 | RN-even、gradual underflow、异常屏蔽及所需精度 | 环境正确不保证 ABI 位保留或没有 double rounding |

算术 NaN 与符号操作的规则来自 [WebAssembly 数值语义](https://webassembly.github.io/spec/core/exec/numerics.html)。算术允许的 NaN 集合不是对所有浮点值进行规范化的许可。

## 3. c1a55bfa 的逐层原因

源码路径相对 `src/uwvm2/`；编译器相关路径省略 `runtime/compiler/` 前缀，同组 opfunc 文件省略重复目录。

| 位置 | 为什么修改、维护时不能退回什么写法 |
|---|---|
| `parser/wasm/standard/wasm1/features/global_section.h`、`wasm1p1/features/types.h` | 先做整数 endian 解码，激活 union 成员，再复制原始位。解析阶段丢位发生在执行环境 guard 之前。 |
| `uwvm/runtime/initializer/init.h` | 定义全局、导入全局及 resolved-global 初始化继续复制解析器保存的位；普通 FP 赋值可能重新引入搬运问题。 |
| `uwvm_int/optable/constop.h`、`variable.h` | 未缓存的常量/local/global 路径直接复制字节；load helper 用输出引用。不能在 cache 分支之前先构造一个共享 FP 临时值。 |
| `uwvm_int/optable/memory.h` | load 输出引用、store 输入 const 引用，字节到整数再 endian 转换；调用者也必须避免 FP-return getter。保留原有内存边界、锁与 trap 语义。 |
| `uwvm_int/optable/stack.h`、`convert.h` | select 选字节，源可能就是目的地址，故用允许重叠的 memmove；未缓存 reinterpret 同宽，只换逻辑类型，字节和栈高度不变。 |
| `uwvm_int/optable/numeric.h` | abs/neg/copysign 在整数位上执行；copysign 先读取两边再写入，保留别名语义。算术仍走独立的 strict evaluator。 |
| `delay_local.h`、`conbine.h`、`conbine_heavy.h`、`combine_extra_heavy.h` | 融合路径会绕过普通 opfunc，必须独立覆盖。符号/选择操作保位，算术共享正确舍入；不拆融合，不额外增加分派。 |
| `uwvm_int/optable/define.h` | i386/68881 原有默认配置不缓存 FP；static_assert 进一步拒绝不合规的自定义配置。仅关闭 x87 算术、只检查 SSE2 宏并不能证明 FP ABI 安全。 |
| `uwvm_int/.../opcode/const_compare_cases.h`、`single_func_emit_helpers.h` | NaN literal 在进入 Float 型延迟融合状态前按整数分类，flush 后直接发出原始位；泛型 const 引用 emitter 同时接收原始整数与有限浮点常量，有限值保留融合快路径。 |
| `uwvm/wasm/type/local_imported.h` | 引用 getter 避免 provider 自己的浮点值返回；type-erasure wrapper 绑定 const 引用再 memcpy。旧按值 getter 仍兼容，但其 ABI 内已经丢失的位不可恢复。 |
| `llvm_jit/.../translate/single_func_emit.h`、provider callback 声明 | native global bridge 用 i32/i64 参数/返回表示 FP 位；原生函数指针、直接生成与字节码回退声明必须一致，只在 IR 内 bitcast。 |
| `shared/strict_float_bits.h`、`strict_float_jit.h` | i386 typed 函数、声明、raw wrapper、间接调用统一采用私有 no-x87 ABI；跨 native 边界仍用整数/字节，不能混用普通 C++ Float 返回原型。 |
| `shared/wasm1p1_simd.h`、`uwvm_int/optable/wasm1p1.h`（主仓库） | lane 搬运走同宽整数路径；pmin/pmax 返回所选操作数的位，不等同于算术 min/max 的 NaN 修复。 |

这些路径也要覆盖 byref 与 tail 两种解释器分派，不是只修一个宏展开实例。

无 SSE 的 i386 JIT 还需要整数 IR 比较和整数返回的转换桥接：普通 hard-float SDK 不保证提供 no-x87 lowering 所需的软浮点比较 helper。SSE2 可直接实现的算术/i32 转换继续原生生成，SSE4.1 舍入继续使用原生指令；仍可能经 x87 legalization 的 i64 转换不能据此放行。

`memory.h` 的 cold trap helper 属性也是 ABI 约束：cold 只决定布局，不能丢掉解释器调用约定；裸 cold 属性不能代替包含 i386 fastcall 等约定的 opfunc cold 宏。

## 4. 前后相关数值修复为什么不能省

- **环境与 tail dispatch**：在公开入口保存/恢复完整宿主环境；native callback 返回前恢复 Wasm 控制。provider 高频 global 回调可只恢复影响后续运算的控制位，因为 Wasm 不观察 accrued flags。guard 放在普通 helper frame 内，析构完成后才回到 opfunc 进行 musttail；不是在有待析构 guard 的 frame 中强制尾调用。调用约定和 tail call 条件参见 [LLVM call 指令说明](https://llvm.org/docs/LangRef.html#call-instruction)。
- **控制寄存器不只看当前 TU 编译选项**：x86_64 no-SSE 构建里的 JIT/callback 仍可能用 MXCSR；ARM64EC 的宏不能误判成执行 x86；RISC-V 用 ISA FPU 可用性判断 FRM。x87 必须先以 no-wait 指令处理 pending exception，再恢复控制字，避免回调留下的未屏蔽异常干扰恢复。
- **辅助 SIMD 控制与 libc 互补**：PPC VSCR.NJ、MIPS MSACSR 可独立影响向量浮点。generic Linux PPC 用 OS HWCAP 判断可否执行 AltiVec，并保留 libc 对 FPSCR/OS 异常状态的处理。没有专用 guard 的架构继续走 libc，不是空保护；没有 MSA guard 的构建不能另外启用未覆盖的 MSA 代码生成。
- **固定环境是显式信任契约**：`UWVM_ASSUME_FIXED_WASM_FP_ENVIRONMENT` 按是否定义生效，定义为 0 也启用。宿主必须保证所有入口、回调、相关信号路径的控制位；x87 需要 PC=53/64，而不是 PC=24。它只省环境工作，不省内存/trap/重入/bridge 权限检查，也不省正确舍入或 NaN/ABI 修复。
- **x87/68881 double rounding**：扩展精度先舍入再存为 f64，可以在 destination midpoint 处两次舍入出错，RN-even 环境及 volatile store 都不足以解决。常见非歧义结果保留显式存储的原生快路径；midpoint/subnormal 等走共享 round-to-odd/jam 后 RN-even 打包。相关控制与 shift 范围约束写在 `strict_float.h`，不能直接改回 C++ 运算符。
- **GCC SSE2 舍入是相反的问题**：未用 SSE4.1 的 ceil/floor/trunc 可能原样返回 sNaN，而算术舍入需要 quiet。整数位舍入同时处理符号零、小于 1 的值、整数区间、ties-even 与 NaN，避免 libm 后再分类修补；分支先排除特殊值，使后续位移严格在有效范围。
- **legacy/default NaN**：HPPA、SH、非 NaN2008 MIPS 的 signaling-bit 约定不同，m68k/SPARC 的 invalid 可产生 all-ones 默认 NaN。宿主可能改变 payload，仅 OR quiet bit 不总能满足 canonical 输入的结果要求；受影响目标的算术结果因此规范化为 canonical NaN。现代无此问题的目标不增加这项处理。
- **升降精度先分类输入**：HPPA 小 payload NaN 窄化可能成为 infinity，只检查结果会漏掉；PPC f32→f64 原生 promotion 可能保留 sNaN。NaN 输入使用符合 Wasm 的结果，有限值保留原生转换。数值转换不是 reinterpret。
- **LLVM 覆盖、顺序及幂等**：先降低需要整数 bridge 的扩展精度算术，再修复残留原生算术 NaN，避免 m68k vector 重复处理。修复同时覆盖 scalar/vector 和 ordinary/constrained intrinsic；替换 uses 时保留原指令到 raw bitcast 的边，避免自环，metadata 防止二次重复插入。
- **RISC-V native rounding**：其转换式舍入也可能保留 sNaN。根据生成 module triple 修复，不根据编译宿主宏猜测；这一步必须在“无需旧架构 bridge”的 early return 之前，否则原生 RISC-V 会漏掉。这里只修舍入结果，不把所有现代算术改成 helper 调用。
- **模块与缓存**：`.cppm`/module runtime 的全局模块片段要有相同 helper 声明，不能只修传统 include 构建。eager、lazy（主仓库）、raw wrapper 的生成入口必须调用共享 lowering。ABI、扩展精度、native NaN 语义分别进入对象缓存 fingerprint；即使没有嵌入 git revision，也不能加载修复前对象。缓存命中仍需在当前进程绑定 bridge symbol，不缓存进程地址。

## 5. 回归为何这样设计

| 回归入口 | 要防止的漏测 |
|---|---|
| `test/0013.uwvm_int/uwvm_int_fp_bits.cc`、`uwvm_int_fp_fused_bits.cc` | O3 内联隐藏 O0 的 ABI 问题；普通 opfunc 通过但融合/byref/tail 某一条路径仍丢位。 |
| `uwvm_int_fp_parser_bits.cc`、`uwvm_int_fp_provider_bits.cc`、`uwvm_int_fp_bit_environment.cc` | 测试在调用 opfunc 前已经 quiet 输入，或者完全绕开解析、初始化、provider、direct/indirect/raw 入口。 |
| `uwvm_int_fp_rounding.cc`、`uwvm_int_fp_nan_encoding.cc` | 只检查 isnan，没有检查 canonical 规则、signed zero、tiny-payload conversion；使用被测 bridge 生成 expected，让同一 bug 自证正确。 |
| `uwvm_int_simd_fp_bits.cc`、`uwvm_int_simd_nan_encoding.cc`（主仓库） | 只测标量或 lane 0，漏掉混合 lane、编译器 vector 与 fallback 分支。 |
| `test/0014.llvm_jit/run_fp_bits_cross.py` | IR 正确但真实 i386 对象 ABI 错；还需分别构建 x87/SSE2 native bridge，覆盖生成代码与宿主编译选项不同的情况。 |
| `run_legacy_nan_cross.py`、`run_native_rounding_cross.py` | 只测试 helper 而没执行生成代码；漏 constrained/vector/重复 lowering，或者遗漏 disabled-bridge 的 RISC-V 原生路径。 |
| `test/0017.runtime/wasm_fp_*.cc`、`llvm_wasm_fp_environment.cc` | 默认环境下测试碰巧通过，未覆盖 hostile callback、辅助寄存器、nested entry 或 fixed contract；架构分支跳过不等于该架构已经实测。 |
| `tools/ci/probes/strict_float_oracle.cpp` 及 generated/SIMD consumer | 用相同的宿主 long double 双舍入产生 expected。独立 MPFR corpus、整数观察及所有 lane 检查才能避免同源错误。 |

舍入 oracle 的分数分解方法不调用被测 ceil/floor/trunc/nearbyint；整数转换期望直接解码二进制数，避免 oracle 自己执行越界的 native FP→integer 转换。需要验证真实调用边界时，不能让常量传播或测试侧内联消掉本来要测的调用。

## 6. 性能与本次注释补充的边界

保留融合分派、有限常量融合、现代 FP cache、原生 SSE2 算术/SSE4.1 舍入、正确舍入的常见值分支，是实现中的明确选择，不是“修复应该自动零开销”。旧架构、异常值密集负载与必要的回调恢复可能增加成本；既有实测数字及未覆盖平台列在前两份审计文档中，QEMU 耗时不作为真实硬件性能证明。

本次只增加维护注释、原因和测试索引，不更改宏、分支、指令选择、签名、cache key 或测试断言。验证重点是修改前后非注释 token/预处理指令结构一致，并保留工作区原有的其他开发改动；历史跨架构与汇编结果不冒充本次新跑的全矩阵结果。

注释补充验证：主仓库 71 个、ROS 62 个源码文件的 Clang raw-token 比较通过；改动均为完整独立注释行，没有插入宏续行。dirty 工作区另与补注释前快照核对，非注释内容保留。本机 macOS AArch64、Apple Clang 21 显式指定 SDK 后，两仓的 `check_wasm_fp_environment.py` 各 6 个测试入口通过；其中非本机架构的条件分支不计为新的跨架构执行证据。本次没有重跑历史 Linux/QEMU 全矩阵或汇编基准。
