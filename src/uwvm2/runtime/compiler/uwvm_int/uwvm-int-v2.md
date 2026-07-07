# UWVM2-INT V2 Architecture Specification

## Status

Draft architecture specification for the next-generation UWVM2 interpreter backend.

This document defines the design direction of **uwvm2-int-v2**, a hybrid interpreter architecture that replaces the aggressive full register-ring model of **uwvm2-int-v1** with a more practical split architecture:

```text
integer / reference / control / address path:
    m3-plus stack-top window

floating-point / SIMD vector path:
    FV register-ring
```

The primary goal of v2 is not to maximize the theoretical number of cached WebAssembly stack values. The goal is to match the shape of real Wasm programs: local-heavy integer/control code, shallow operand stacks, mixed address calculation, and floating-point/vector expression chains.

Empirical tuning results have already shown that the previous u2 register-ring model can be optimized on x86_64 SysV, but that the aggressive 5-int / 8-float ring model is much less stable on aarch64-macos; the reported u2-aapcs64 paired median gain was only +0.11%, while m3 changed-only tuning showed a more visible +2.29% gain. The v2 architecture is designed around this lesson: keep the useful part of the register-ring, but remove the harmful integer-ring state space.

---

# 1. M3-Plus as the Primary Integer Architecture

## 1.1 Why m3-plus exists

The original m3 interpreter is successful because it follows the natural execution shape of WebAssembly.

WebAssembly is a structured stack machine, but real Wasm code generated from C, C++, Rust, Zig, Swift, and similar languages is not a deep arbitrary expression stack. It is usually a shallow stack wrapped around frequent local access:

```wasm
local.get $a
local.get $b
i32.add
local.set $c
```

or:

```wasm
local.get $base
local.get $i
i32.const 8
i32.mul
i32.add
f64.load
```

In this shape, the operand stack is not the true long-term storage. Locals are. The stack is mostly a short-lived expression transport.

The original m3 model works well because it treats the Wasm stack as a set of slots and keeps only a very small number of top values in real C-level operation arguments or temporary registers. It does not try to reinterpret the entire Wasm stack as a virtual register file. This keeps the interpreter simple, compact, and predictable.

**m3-plus** keeps that philosophy, but generalizes it for a C++ implementation.

The key idea is:

```text
m3:
    small, manually managed, C-based stack-top cache

m3-plus:
    template-generated, ISA-selected, C++ stack-top window
```

m3-plus is not a register-ring. It is a bounded stack-top window. It does not rotate. It does not carry an integer ring begin state. It does not create an `int_begin * fp_begin` state explosion.

It simply says:

```text
The top N integer/reference stack values may be represented as direct operation operands.
All remaining values stay in stack slots or locals.
```

The value of `N` is selected by the target ISA, ABI, and xmake configuration.

---

## 1.2 What m3-plus is used for

m3-plus is the default execution substrate for:

```text
i32 / i64 arithmetic
reference-like values
local.get / local.set / local.tee
branch conditions
loop induction variables
memory address calculation
integer comparisons
hashing
tree walking
control-flow-heavy code
algorithm-heavy integer code
mixed integer-address + floating-point compute
```

The most important observation is that these workloads are normally **local-heavy**, not deep-stack-heavy.

For example:

```wasm
local.get $i
i32.const 1
i32.add
local.tee $i
local.get $n
i32.lt_u
br_if $loop
```

A register-ring tries to keep a rotating abstract stack alive across such operations. m3-plus instead treats locals and stack-top values as direct operand sources:

```text
i32.add     R0, imm(1)
local.tee   $i, R0
i32.lt_u    R0, local($n)
br_if       R0
```

This is closer to what the CPU and the Wasm producer actually want.

---

## 1.3 Operand-source model

m3-plus uses a typed operand-source model.

For integer/reference values:

```cpp
enum class ISrcKind {
    R0,
    R1,
    R2,
    StackSlot,
    Local,
    Immediate
};
```

For a concrete build with `M3_TOP = 3`, the integer source set is:

```text
R0
R1
R2
stack slot
local slot
immediate
```

For `M3_TOP = 1`, it becomes:

```text
R0
stack slot
local slot
immediate
```

For `M3_TOP = 0`, it becomes:

```text
stack slot
local slot
immediate
```

`M3_TOP = 0` is still a valid architecture. It means the interpreter uses m3-style slot execution without exposing any cached stack-top register operands. This is useful for small platforms, hostile ABIs, or portability-first builds.

The source model is intentionally independent of physical registers. m3-plus does not promise that `R0`, `R1`, and `R2` always map to hardware registers. It promises that the generated operation ABI gives the compiler a chance to keep them in registers when the target calling convention allows it.

---

## 1.4 Commutative and non-commutative operations

m3-plus reduces operation-space growth through canonicalization.

For commutative operations:

```text
i32.add
i32.mul
i32.and
i32.or
i32.xor
i64.add
i64.mul
f64.add, when used outside FV-ring fallback
f64.mul, when used outside FV-ring fallback
```

the operand order can be canonicalized:

```text
R0 + Local(x)
Local(x) + R0
```

can share the same generated operation form.

For non-commutative operations:

```text
i32.sub
i32.shr_u
i32.lt_u
i32.lt_s
i64.sub
f64.sub
f64.div
```

operand order must be preserved:

```text
R0 - R1
R1 - R0
```

are different operations.

This gives m3-plus a scalable operation-space model:

```text
commutative binary op:
    approximately unordered source pairs

non-commutative binary op:
    ordered source pairs

unary op:
    one source

local.set:
    one producer source

br_if:
    one condition source
```

The design is explicit: m3-plus accepts some template expansion, but only where the operand-source shape is actually useful.

---

# 2. Why m3-plus is different from original m3

## 2.1 Original m3 is C-based

The original m3 interpreter is written in C. This is a major strength for portability and code size, but it limits how far the source-variant model can be expanded.

A C implementation must usually express operation variants through handwritten macros, manually selected function tables, or carefully maintained C source expansion. This works well for one or two hot registers, but it becomes increasingly hard to maintain when the number of stack-top registers grows.

For example, with one cached integer stack-top value, the source model is small:

```text
register
memory/slot
local
```

With two or three top values, the number of source combinations grows quickly. For every binary operation, the implementation must consider:

```text
R0 op R1
R1 op R0
R0 op slot
slot op R0
R1 op slot
slot op R1
R2 op local
local op R2
...
```

In C, this becomes a maintainability problem.

---

## 2.2 m3-plus is C++ template-generated

m3-plus is a C++ architecture. It uses template generation to instantiate operation families from a small number of generic definitions.

Conceptually:

```cpp
template <std::size_t M3Top, typename Op, typename SrcA, typename SrcB>
struct M3PlusBinaryOp;
```

The implementation does not manually write every source combination. It defines rules and lets the C++ compiler instantiate the selected subset.

This gives m3-plus an important property:

```text
The architecture is unbounded.
The build configuration is bounded.
```

In other words, m3-plus can theoretically support any stack-top window size:

```text
M3_TOP = 0
M3_TOP = 1
M3_TOP = 2
M3_TOP = 3
M3_TOP = 4
...
```

But production builds should not blindly instantiate all possible forms. The build system chooses a practical `M3_TOP` based on the target ISA and ABI.

This is the critical difference:

```text
m3:
    small fixed C design

m3-plus:
    C++ template architecture with ISA-selected expansion
```

m3-plus can expand deeply when the platform can afford it, and collapse to a tiny model when the platform cannot.

---

# 3. ISA and xmake-based register-count selection

## 3.1 Design principle

The number of m3-plus stack-top registers is not a universal constant.

It must be selected from:

```text
target ISA
target ABI
calling convention
availability of musttail or equivalent tail-call lowering
number of fixed interpreter operation arguments
code-size budget
platform class
xmake build profile
```

The architecture allows an unlimited stack-top count in theory, but the default production configuration must be conservative and automatic.

The default policy is:

```text
large 64-bit ABI:
    use M3_TOP = 3

medium or constrained ABI:
    use M3_TOP = 1 or 2

small or hostile ABI:
    use M3_TOP = 0 or 1
```

The purpose of `M3_TOP = 3` is to cover the overwhelming majority of integer/control Wasm stack shapes without paying the cost of an integer register-ring.

In practice, most integer/control Wasm operations need at most:

```text
one active accumulator
one secondary operand
one branch/address/temporary value
```

Therefore:

```text
M3_TOP = 3
```

solves the majority of real integer-path cases without introducing ring state.

---

## 3.2 Default target policy

Recommended default policy:

```text
aarch64 / AAPCS64:
    M3_TOP = 3
    FV_RING = 8

x86_64 / System V:
    M3_TOP = 3
    FV_RING = 8

x86_64 / Windows x64:
    M3_TOP = 1 or 2
    FV_RING = 4 or 8
    final value depends on operation ABI design

i686 / fastcall:
    M3_TOP = 0
    FV_RING = 0 or 4
    reason: fixed operation parameters already consume the useful fastcall registers

i386 / cdecl:
    M3_TOP = 0 or 1
    FV_RING = 0 or 4

embedded / size-first:
    M3_TOP = 0 or 1
    FV_RING = 0, 2, or 4

debug / portability:
    M3_TOP = 0
    FV_RING = 0

native speed profile:
    auto-select by ISA and ABI
```

The i686 fastcall case is especially important. If the interpreter operation ABI already needs three fixed parameters, fastcall has no useful register budget left for an expanded m3-plus top window. Opening more stack-top operands there would only move pressure into spills and call-frame traffic.

Therefore, v2 must not treat m3-plus expansion as mandatory.

m3-plus is a scalable architecture, not a fixed policy.

---

## 3.3 xmake configuration

The build system should expose explicit controls:

```text
uwvm_int_v2 = true / false

uwvm_int_v2_m3_top =
    auto
    0
    1
    2
    3
    4
    ...

uwvm_int_v2_fv_ring =
    auto
    0
    2
    4
    8

uwvm_int_v2_profile =
    small
    balanced
    speed
    native

uwvm_int_v2_musttail =
    auto
    required
    off
```

The default should be:

```text
uwvm_int_v2_m3_top = auto
uwvm_int_v2_fv_ring = auto
uwvm_int_v2_profile = native
uwvm_int_v2_musttail = auto
```

A simplified decision table:

```text
if profile == small:
    M3_TOP = 0 or 1
    FV_RING = 0 or 2

if profile == balanced:
    M3_TOP = 1 or 2
    FV_RING = 4

if profile == speed:
    M3_TOP = target_default
    FV_RING = 8

if profile == native:
    M3_TOP = isa_abi_default
    FV_RING = isa_abi_default
```

For large 64-bit targets:

```text
aarch64-aapcs64:
    M3_TOP = 3
    FV_RING = 8

x86_64-sysv:
    M3_TOP = 3
    FV_RING = 8
```

For hostile or small targets:

```text
i686-fastcall:
    M3_TOP = 0
    FV_RING = 0 or 4
```

---

# 4. Difference from uwvm2-int-v1

## 4.1 v1: full register-ring interpreter

uwvm2-int-v1 is based on a full register-ring stack-top cache.

The v1 model can be summarized as:

```text
integer values:
    integer register-ring

floating-point values:
    floating-point register-ring

operand stack:
    modeled as ring positions

local access:
    optimized with delay-local and fusion

operation selection:
    depends on ring state and operand type
```

On a target such as AAPCS64, v1 may attempt a model like:

```text
INT_RING = 5
FV_RING  = 8
```

This looks attractive on paper because the target has many physical registers. But the interpreter operation space becomes tied to both ring begin states:

```text
state space ≈ INT_RING * FV_RING
            = 5 * 8
            = 40
```

This is the core architectural problem.

Even when a specific operation does not need both integer and floating-point ring state, the interpreter architecture still has to manage the global state model. That increases operation variants, lowering complexity, dispatch target count, and code-size pressure.

---

## 4.2 Why v1 is too aggressive

The v1 register-ring model assumes that keeping more operand-stack values in ring registers is generally good.

For Wasm, this assumption is only partially true.

It is useful for:

```text
floating-point expression chains
SIMD vector transforms
dot products
FFT-like kernels
matrix kernels
a*b + c*d shapes
```

It is much less useful for:

```text
branch-heavy code
local-heavy integer code
hashing
tree walking
memory address calculation
loop induction variables
mixed integer-address + floating-point compute
```

The integer path is usually dominated by locals, branches, and address calculations. A rotating integer ring adds state-management cost to code that does not need a deep stack cache.

In other words:

```text
floating-point/vector code often wants a ring.
integer/control code usually wants local operands and a small stack-top window.
```

v1 applies the ring idea too broadly.

---

## 4.3 v2: split architecture

uwvm2-int-v2 replaces the full register-ring model with a split architecture:

```text
integer / reference / control / address:
    m3-plus

floating-point / vector:
    FV register-ring
```

The v2 state model is:

```cpp
struct uwvm_int_v2_state {
    // m3-plus integer/control window
    uint64_t r0;
    uint64_t r1;
    uint64_t r2;     // only when M3_TOP >= 3

    // floating-point / vector ring
    fv_value fv_ring[FV_RING];
    uint8_t  fv_head;
    uint8_t  fv_depth;

    // frame state
    value*   locals;
    value*   stack_slots;
    uint8_t* memory;
};
```

The important absence is:

```cpp
// not present in v2
uint64_t int_ring[5];
uint8_t  int_begin;
uint8_t  int_depth;
```

v2 does not have an integer ring.

Therefore, v2 avoids:

```text
int_begin * fv_begin
```

The maximum ring state is only:

```text
FV_RING
```

For the default high-performance configuration:

```text
M3_TOP = 3
FV_RING = 8
```

the theoretical ring begin state is:

```text
8
```

not:

```text
5 * 8 = 40
```

This is the defining architectural simplification of uwvm2-int-v2.

---

# 5. Operation lowering in v2

## 5.1 Local access is not a standalone operation when avoidable

v2 treats `local.get` as an operand source whenever possible.

Given:

```wasm
local.get $a
local.get $b
i32.add
local.set $c
```

v2 should not lower this to:

```text
op_local_get
op_local_get
op_i32_add
op_local_set
```

The preferred lowering is:

```text
op_i32_add_local_local_set a, b, c
```

or at least:

```text
op_i32_add_LL      a, b
op_local_set_R0    c
```

This rule is central to m3-plus.

The integer path should not materialize locals into a virtual integer ring. Locals are already the natural storage form of Wasm-generated code. m3-plus turns them into direct operation operands.

---

## 5.2 Integer path lowering

For integer operations, v2 uses the m3-plus top window.

Example:

```wasm
local.get $x
local.get $y
i32.mul
local.get $z
i32.add
local.set $r
```

Possible m3-plus lowering:

```text
R0 = i32.mul local($x), local($y)
R0 = i32.add R0, local($z)
local.set $r, R0
```

No integer ring is involved.

For branch code:

```wasm
local.get $i
local.get $n
i32.lt_u
br_if $loop
```

Preferred lowering:

```text
R0 = i32.lt_u local($i), local($n)
br_if R0
```

For loop induction:

```wasm
local.get $i
i32.const 1
i32.add
local.tee $i
local.get $n
i32.lt_u
br_if $loop
```

Preferred lowering:

```text
R0 = i32.add local($i), imm(1)
local.tee $i, R0
R0 = i32.lt_u R0, local($n)
br_if R0
```

This is exactly the type of code where an integer register-ring is unnecessary.

---

## 5.3 Floating-point and vector path lowering

Floating-point and vector code uses the FV ring.

For:

```wasm
local.get $a
local.get $b
f64.mul
local.get $c
local.get $d
f64.mul
f64.add
```

v2 may lower this to:

```text
fv_fill_local      a
fv_mul_top_local   b
fv_fill_local      c
fv_mul_top_local   d
fv_add_top_prev
```

or, when fusion is available:

```text
fv_f64_muladd_local4 a, b, c, d
```

The FV ring is designed for:

```text
f32
f64
v128
```

It is not only a floating-point ring. It is a floating-point/vector ring.

That means the same architectural concept can serve:

```text
f64 dot products
f32 vector transforms
f64x2 SIMD operations
f32x4 SIMD operations
matrix kernels
FFT kernels
FIR filters
stencil computations
```

The ring must be implemented as a head-index ring, not a physically rotated array.

Correct model:

```cpp
head = (head - 1) & mask;
fv_ring[head] = value;
```

For top-update operations:

```cpp
fv_ring[head] = op(fv_ring[head], source);
```

No physical rotation should occur.

---

## 5.4 Mixed integer-address and floating-point compute

Mixed workloads are one of the main reasons v2 exists.

Example:

```wasm
local.get $base
local.get $i
i32.const 8
i32.mul
i32.add
f64.load

local.get $scale
f64.mul

local.get $acc
f64.add
local.set $acc
```

v1 tends to entangle integer address calculation and floating-point ring state.

v2 separates them:

```text
address calculation:
    m3-plus integer window

f64.load result:
    enters FV ring

floating-point multiply/add:
    FV ring

loop/index/branch:
    m3-plus integer window
```

This split is the core of the v2 hybrid design.

It avoids spending register-ring complexity on integer address calculation while still allowing the floating-point part to benefit from ring-style expression chaining.

---

# 6. Tail-call and operation dispatch model

v2 should keep the existing direct-threaded / tail-call interpreter direction.

Operation functions should be compiled into a tail-call chain when the compiler and target support it.

Preferred execution shape:

```text
op_A -> tail call op_B -> tail call op_C -> ...
```

not:

```text
op_A calls op_B
op_B returns to op_A
op_A returns to dispatch loop
```

The build should expose:

```text
uwvm_int_v2_musttail = auto | required | off
```

Recommended policy:

```text
required:
    fail the build if the required tail-call form cannot be emitted

auto:
    use musttail where available; otherwise use fallback dispatch

off:
    use portable dispatch
```

For performance-sensitive native builds, `required` is useful because otherwise benchmark results may measure function-call overhead rather than interpreter architecture.

For portability-first builds, `auto` or `off` is acceptable.

---

# 7. Register-count policy and operation-space control

## 7.1 M3_TOP is compile-time selected

`M3_TOP` controls how many integer/reference stack-top values can be represented as direct operation operands.

Examples:

```text
M3_TOP = 0:
    slot/local-only m3-plus

M3_TOP = 1:
    close to classic m3

M3_TOP = 2:
    enhanced m3-style stack-top window

M3_TOP = 3:
    default high-performance target for AAPCS64 and x86_64 SysV

M3_TOP > 3:
    experimental only
```

Although the C++ template system can generate arbitrary `M3_TOP`, production builds should not use unbounded expansion by default.

The correct interpretation is:

```text
m3-plus has no architectural upper limit.
The build profile imposes the practical limit.
```

---

## 7.2 Why the default high-performance value is 3

`M3_TOP = 3` is a practical maximum for large 64-bit ABIs.

It covers the common integer/control shapes:

```text
binary operation:
    two operands

branch/update:
    result + bound/local

address calculation:
    base + index + scale/offset

loop induction:
    current + increment + limit

mixed memory:
    address temporary + loop variable + condition
```

This covers the majority of integer-path pressure without moving to a ring.

The design intent is:

```text
M3_TOP = 3 solves most integer/control cases.
FV_RING solves floating-point/vector chains.
No integer ring is needed.
```

---

## 7.3 Why small platforms must reduce or disable m3-plus

On small platforms, exposing more m3-plus operands can be harmful.

For example, on i686 fastcall, fixed interpreter operation parameters may already consume the useful fastcall registers. If the operation ABI requires three fixed parameters, there is no meaningful register budget left for a large stack-top window.

Therefore:

```text
i686 fastcall:
    M3_TOP = 0
```

is a valid and expected configuration.

This is not a failure of m3-plus. It is the point of m3-plus: the architecture can scale down.

A small build may choose:

```text
M3_TOP = 0
FV_RING = 0
```

and still use the same v2 framework.

A balanced build may choose:

```text
M3_TOP = 1
FV_RING = 4
```

A native speed build may choose:

```text
M3_TOP = 3
FV_RING = 8
```

---

# 8. Relationship between m3-plus and FV ring

m3-plus and FV ring solve different problems.

m3-plus is for:

```text
shallow stack
local-heavy code
integer control
address calculation
branching
hashing
data-structure traversal
```

FV ring is for:

```text
deep or medium floating-point expression chains
SIMD transforms
scientific kernels
multiply-add patterns
dot products
FFT
FIR
matrix operations
stencil operations
```

The architecture must not merge them into a single global ring state.

The correct model is:

```text
m3-plus:
    integer/control substrate

FV ring:
    floating-point/vector accelerator
```

This makes v2 a hybrid interpreter, but not a symmetric hybrid.

m3-plus is the base. FV ring is an accelerator.

---

# 9. Compiler and lowering implications

## 9.1 LLVM / Wasm producer guidance

For v2, LLVM or any Wasm producer should not try to keep all integer values on the operand stack.

Instead, the desired shape is:

```text
integer/control:
    local-friendly
    shallow stack
    compare+branch fusion
    local.tee+branch fusion
    address+load fusion

floating-point/vector:
    expression-chain-friendly
    a*b+c*d shapes
    dot/FIR/FFT/stencil shapes
    avoid unnecessary local materialization
```

The backend cost model should distinguish:

```text
integer stack pressure
floating-point/vector expression pressure
local reuse distance
branch density
memory address pressure
```

A single generic “keep more values on the stack” policy is not correct.

---

## 9.2 Recommended cost model

A v2-aware Wasm backend should estimate:

```text
int_stack_peak
fv_stack_peak
local_get_count
local_get_to_consumer_distance
local_set_count
branch_density
memory_density
fv_chain_length
mixed_addr_fp_count
```

Then apply different rules:

```text
integer/control:
    prefer local-as-operand
    prefer M3_TOP <= 3
    avoid deep integer stackification

floating/vector:
    prefer expression chains
    prefer FV ring occupancy
    prefer fused multiply-add-like operation groups
```

The backend profile should model:

```text
M3_TOP = target-selected
FV_RING = target-selected
INT_RING = 0
```

This is a major difference from v1, where the backend had to reason about both integer and floating-point ring pressure.

---

# 10. Operation families required by v2

## 10.1 Integer/control m3-plus operations

Initial operation families:

```text
i32.add
i32.sub
i32.mul
i32.and
i32.or
i32.xor
i32.shl
i32.shr_u

i64.add
i64.sub
i64.mul
i64.xor

i32.eqz
i32.eq
i32.ne
i32.lt_u
i32.lt_s

local.get as source
local.set as sink
local.tee as sink+source
br_if with source condition

i32.load address
i32.store address
```

Important fused forms:

```text
i32.add_local_local_set
i32.add_R_local_set
i32.lt_u_local_local_br
i32.lt_u_R_local_br
local_tee_cmp_br
addr_local_index_load
addr_local_index_store
```

---

## 10.2 Floating-point/vector FV-ring operations

Initial operation families:

```text
f32.add
f32.sub
f32.mul
f32.div

f64.add
f64.sub
f64.mul
f64.div
f64.neg
f64.abs

f64.load
f64.store

v128.load
v128.store

f32x4.add
f32x4.mul
f64x2.add
f64x2.mul
```

Important fused forms:

```text
f64_muladd_local4
f64_dot2_local
f64_dot4_local
v128_muladd_local
v128_axpy_local
f64_load_muladd
v128_load_muladd
```

Ring implementation rules:

```text
physical rotate:
    forbidden in the fast path

head movement:
    allowed

top update:
    preferred

spill:
    allowed only when required

integer ring interaction:
    forbidden
```

---

# 11. Correctness and measurement counters

v2 should expose architecture counters from the beginning.

Required compile-time counters:

```text
input_ops
emitted_ops
fused_ops
local_get_total
local_get_folded
local_get_materialized
local_set_total
local_set_folded
opstream_bytes
unique_opfuncs
theoretical_state_count
```

Required runtime counters:

```text
dispatch_count
slot_load_count
slot_store_count
local_load_count
local_store_count
fv_ring_fill_count
fv_ring_spill_count
fv_logical_head_move_count
fv_physical_rotate_count
fv_top_update_count
branch_count
branch_taken_count
```

The following invariant should hold for v2 speed builds:

```text
fv_physical_rotate_count == 0
```

The theoretical state count should be:

```text
theoretical_state_count = FV_RING
```

not:

```text
INT_RING * FV_RING
```

because v2 has no integer ring.

---

# 12. Architecture invariants

uwvm2-int-v2 must satisfy these invariants:

```text
1. There is no integer register-ring.

2. Integer/reference/control/address operations use m3-plus.

3. Floating-point and v128 operations may use FV ring.

4. FV ring is head-index based.

5. FV ring fast path does not physically rotate.

6. local.get should be folded into the consumer when possible.

7. local.set should be folded into the producer when possible.

8. M3_TOP is selected by ISA, ABI, and xmake configuration.

9. M3_TOP may be zero.

10. FV_RING may be zero.

11. A small-platform build is still a valid v2 build.

12. The architecture has no int_begin * fv_begin state space.
```

These rules are more important than any individual optimization.

---

# 13. Summary

uwvm2-int-v1 was a full register-ring interpreter. It attempted to cache both integer and floating-point stack values through ring state. This worked in some cases, especially with delay-local and operation fusion, but it made the interpreter architecture too aggressive for real Wasm integer/control workloads.

uwvm2-int-v2 changes the center of gravity.

The new design is:

```text
m3-plus first
FV ring second
integer ring never
```

m3-plus provides a scalable C++ template-generated stack-top window for integer, reference, control-flow, and address-calculation code. Unlike original m3, it is not limited by handwritten C operation variants. It can theoretically expand without an architectural upper bound, but production builds select a finite `M3_TOP` according to ISA and ABI.

On large 64-bit targets such as AAPCS64 and x86_64 SysV, the recommended default is:

```text
M3_TOP = 3
FV_RING = 8
```

On small or hostile platforms, such as i686 fastcall, the correct default may be:

```text
M3_TOP = 0
FV_RING = 0 or 4
```

The floating-point/vector register-ring is retained because it matches scientific and SIMD expression chains. The integer register-ring is removed because integer/control Wasm is usually local-heavy, branch-heavy, and shallow-stack.

The final architecture is therefore:

```text
UWVM2-INT V2 =
    ISA-selected m3-plus integer/control window
  + head-index FV register-ring for f32/f64/v128
  + local-as-operand lowering
  + producer-consumer fusion
  + no integer ring
  + no physical FV rotation
```

This is the intended next-generation interpreter architecture for UWVM2.
