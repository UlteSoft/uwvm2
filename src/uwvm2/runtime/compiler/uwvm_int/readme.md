# UWVM2 “u2” Interpreter Architecture (Register-Ring Stack-Top Cache)

UWVM2’s **u2** interpreter (see `src/uwvm2/runtime/compiler/uwvm_int/`) is a **non-JIT, non-self-modifying** WebAssembly interpreter. The design goal is to push a threaded interpreter close to its practical performance ceiling on modern calling conventions by aggressively reducing **operand-stack memory traffic**.

At a high level, u2 keeps the hottest portion of the Wasm operand stack in an ABI-friendly **opfunc argument pack** (values carried in registers/locals across opfunc calls), and uses a **compile-time specialized ring buffer** (“register ring”) to move values between:
- the **canonical operand stack** in memory (`sp`), and
- the **stack-top cache** that stays resident across opfunc dispatch.

This document describes the architecture and how it differs from:
1) Wasm3 / M3-style threaded interpreters (small fixed register file), and
2) classic stack-top / slot-stack optimizations (slot IDs + copies at control-flow boundaries).

---

## Goals and non-goals

### Goals
- **Fast steady-state execution** for stack-heavy Wasm by turning many hot operations into **register-register ALU** on cache-hit paths.
- **No RWX / no runtime codegen**: opfuncs are compiled ahead of time; runtime executes a precompiled opfunc set plus translated threaded bytecode.
- **ABI-scalable caching**: cache width scales with the platform ABI (how many integer/FP/SIMD arguments can stay in registers).

### Non-goals
- Eliminating dispatch cost entirely (that is primarily the domain of JIT/AOT, trace fusion, or superinstructions).
- Building a general-purpose optimizing compiler pipeline inside the interpreter.

---

## Architecture overview

### 1) Threaded interpreter core (“opfunc chaining”)
u2 is a **threaded interpreter**: each translated opcode points to a specialized **opfunc**; execution proceeds as “fetch next op pointer → advance PC → indirect tail-jump”. This is a common high-performance interpreter shape; u2’s main win is that many ops avoid **operand-stack loads/stores** when the stack-top cache hits.

### 2) Operand stack + stack-top cache (spill/fill)
WebAssembly is a **stack machine**. u2 keeps the top segment of that stack in the opfunc argument pack:

- **spill**: stack-top cache → operand stack memory
- **fill**: operand stack memory → stack-top cache

The implementation lives in `optable/register_ring.h` and is specialized so the runtime fast path is mostly pointer adjustment plus bulk moves when needed.

### 3) Per-type register rings (i32/i64/f32/f64/v128)
The stack-top cache is modeled as **per-type rings** (i32/i64/f32/f64/v128), not a single untyped buffer.

Key properties:
- **Compile-time known** spill/fill sizes and start positions are expanded via templates and compile-time evaluation, so runtime work stays small.
- Ranges may be **merged** (shared slots with union-like layouts) to reduce register pressure while keeping compile-time validation.

### 4) Translation-time “cache layout” options
u2 tracks stack-top cache ranges **per value category**, because ABI and target details affect how values map to registers/slots (e.g., integer vs FP register files, v128 availability).

This is represented by `uwvm_interpreter_translate_option_t` fields such as:
- `i32_stack_top_begin_pos` / `i32_stack_top_end_pos`
- `i64_stack_top_begin_pos` / `i64_stack_top_end_pos`
- `f32_stack_top_begin_pos` / `f32_stack_top_end_pos`
- `f64_stack_top_begin_pos` / `f64_stack_top_end_pos`
(and optionally v128)

---

## Why the ring matters

### Adjacent-by-construction operands (TOS/NOS)
Binary stack ops (add/or/mul/…) consume the **top two** stack values. In u2’s ring, the top (TOS) and next (NOS) are **adjacent positions by construction**, so opfunc specialization typically depends on:
- `StartPos` (logical top), and
- `Count` (how many cached values are available),

rather than an arbitrary `(i, j)` register-pair placement.

Consequence: specialization growth for 2-operand ops is approximately **O(N)** (possible start positions), not **O(N²)** combinations in “free pairing” designs.

---

## Control flow: keeping values cached across re-entry

Classic stack caching often pays at control-flow boundaries (loops, if/else joins) by forcing copies or spilling to memory.

u2 adds an **experimental register-only canonicalization** step for control-flow re-entry:
- It **rotates** the stack-top ring(s) so the current position becomes the range’s begin slot.
- This is a **register/argument-pack transform only**: it shuffles cached slots and does not touch operand-stack memory.

In the codebase this shows up as stack-top “transform-to-begin” helpers (e.g., `uwvmint_stacktop_transform_to_begin` in `optable/register_ring.h`) and control-flow variants (e.g., `uwvmint_br_stacktop_transform_to_begin` in `optable/conbine.h`).

---

## ABI leverage (why u2 benefits from “wide argument passing”)

u2 benefits most when the platform ABI passes **many arguments in registers** (integer + FP/SIMD), because the opfunc argument pack can keep a wider stack-top cache resident in registers/locals:

- x86_64 SysV: multiple GPR + XMM argument registers
- AArch64 AAPCS64: `x0–x7` for integer/pointers and `v0–v7` for FP/SIMD arguments

This directly increases the cache-hit rate, which is why u2 tends to scale sharply once it can keep **more than ~2** hot operands cached for typical code patterns.

---

## Comparison notes

### u2 vs Wasm3 (M3-style threaded interpreter)
Wasm3 is a well-known fast interpreter design that pairs threaded dispatch with a small, fixed VM register set mapped to hardware registers. u2 is also threaded, but it targets a different bottleneck: in a stack machine, steady-state cost is often dominated by repeated **operand-stack loads/stores**, not only dispatch. u2 tries to remove that memory traffic on cache-hit paths by keeping multiple stack-top values in the argument pack and executing **register-register** operations most of the time.

Worked-example intuition: for a hot arithmetic opcode (e.g., `i64.or`), a classic stack-machine interpreter often pays a chain of “compute stack address → load operand(s) → ALU → store result”. When u2 hits in the stack-top cache, the same opcode can reduce to “register-register ALU → continue dispatch”, leaving dispatch and control-flow structure as the dominant residual cost.

### u2 vs classic “slot stack” caching
Many fast interpreters use a **slot stack** (assign slot IDs and insert copies when control flow causes mismatches). u2 instead models the hot region as a **ring** where consumed operands are adjacent-by-definition, keeping specialization small and enabling control-flow re-entry canonicalization via ring rotation (register-only shuffles) rather than pervasive slot-copy code or forced spills.

---

## Why u2 is “near the interpreter ceiling”

On cache-hit paths, u2’s goal is to make operand-stack traffic largely disappear. Once the interpreter stops loading/storing operands from memory for most opcodes, a threaded interpreter is often left primarily with:
- the structural dispatch sequence (fetch next op pointer, advance PC, indirect branch), and
- unavoidable boundary cases (cache boundary spill/fill, memory-exposing ops, calls, etc.).

At that point, further large wins typically require changing the execution model (JIT/AOT, trace fusion, superinstructions) rather than squeezing more out of operand handling.

---

## Practical mental model

Think of u2 as:
1. A **threaded interpreter** (dispatch is shared and unavoidable),
2. with a **typed, ABI-backed stack-top cache** (operand-stack traffic is the main target),
3. implemented as a **per-type register ring** (operands are adjacent-by-definition, keeping specialization manageable),
4. plus **register-only control-flow canonicalization** to preserve cached values across re-entry when possible.

---

## Weaknesses and limitations (where u2 can lose its advantage)

u2’s wins come from replacing “load operands from memory → compute → store back” with “keep operands in registers/locals → compute → continue dispatch”. That only works when the platform ABI and the surrounding control-flow shape allow a wide, stable **argument-pack cache** to remain resident with minimal shuffling/spilling.

This section explains the main cases where u2 is structurally constrained, and why a design like Wasm3/M3 can look “closer” on those targets/workloads even if u2 is faster elsewhere.

### 1) ABI-constrained targets: why Win64 MS ABI and AArch32 AAPCS/EABI are hard

**Core mechanism reminder.** u2 uses an opfunc signature whose first fixed arguments are:
1) `ip` (bytecode / op-pointer stream),
2) `stack_top` (operand stack top pointer),
3) `local_base` (locals buffer base),
and then uses the **remaining argument slots** as the stack-top cache. This convention is encoded in the runtime’s target selection (`get_curr_target_tranopt()` in `src/uwvm2/runtime/lib/uwvm_runtime.default.cpp`) and is assumed by the translator (`src/uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/wasm1.h`).

On “wide” ABIs (x86_64 SysV, AArch64 AAPCS64), this is a good trade: after the 3 fixed args there are still multiple integer argument registers and multiple FP/SIMD argument registers available, so the stack-top cache can be several values wide for *both* scalar integer and scalar FP operations.

On **Win64 MS ABI** and **AArch32 AAPCS/EABI**, the situation is fundamentally different:

- **Win64 (Microsoft x64 ABI) has only 4 register argument positions total** (`rcx/rdx/r8/r9` and the “matching” `xmm0–xmm3` positions).
  - After the 3 fixed interpreter args, **only 1 register position remains**.
  - Worse (relative to SysV/AArch64): the MS ABI’s “4 positions” are shared between integer and FP arguments. If the first 3 positions are consumed by pointers, you effectively lose the ability to use the corresponding early XMM positions for FP cache entries as well.
  - As a result, “keep a meaningful stack-top slice in registers” collapses into a **1-slot cache** at best, usually implemented as a scalar4-merged layout. This increases register pressure and forces frequent spill/fill and shuffling, which often costs more than it saves.
  - The runtime therefore keeps stack-top caching **disabled by default** on Win64 MS ABI, and keeps a 1-slot experiment under `#if 0` for reference in `get_curr_target_tranopt()`.

- **AArch32 AAPCS/EABI has only 4 core argument registers (`r0–r3`)**.
  - After the 3 fixed interpreter args, there is at most **one remaining core argument register (`r3`)** for integer-like cached values.
  - While some ARM32 hard-float variants can pass FP arguments in VFP registers, the exact rules depend on toolchain/ABI mode and interact poorly with “mixed typed” signatures. u2 intentionally targets a portable baseline here and therefore disables caching rather than risking an ABI-dependent “it benchmarks well on one compiler flag but regresses elsewhere” outcome.
  - The runtime similarly keeps stack-top caching **disabled** for ARM32 in `get_curr_target_tranopt()`.

**Why Wasm3 can be closer on these targets.** M3’s design keeps a *small fixed* set of VM state in registers (pc/sp/mem + a tiny number of temporaries) and performs operand access via memory loads/stores. That is not ideal on stack-heavy arithmetic, but it is also much less sensitive to “how many argument registers exist after 3 fixed arguments”, because it does not try to encode a wide operand-cache into the ABI call interface in the first place. When u2’s cache width collapses to ~1, u2’s advantage (removing operand-stack traffic) shrinks, while its remaining costs (dispatch + occasional canonicalization) stay.

There is also a *threshold effect*: most hot Wasm scalar ops are binary (consume two operands). With a cache width < 2, u2 can keep at most one operand resident; the other operand must be fetched from the operand stack memory almost every time. At that point u2’s fast path starts to resemble an M3-style “one register + one stack load” topology, but still pays the overhead of maintaining a typed cache model and (optionally) canonicalizing it at control-flow edges. This is the main reason why MS ABI / AArch32 results can look surprisingly close to Wasm3 even when u2 wins decisively on SysV/AArch64.

### 2) Mixed-typed Wasm stack: why “partial caching” is not a free knob

The Wasm operand stack is *mixed typed*: i32/i64/f32/f64 values can interleave arbitrarily. u2’s translator models this stack and decides—at translation time—whether each operation’s operands are in the cache or must be materialized from memory.

For correctness and simplicity of the optable layouts, when stack-top caching is enabled for Wasm1, the translator currently requires **full scalar coverage** and tail-call chaining:

- In `compile_all_from_uwvm/wasm1.h`, enabling stack-top caching implies:
  - `CompileOption.is_tail_call` must be true (the threaded opfunc chaining relies on tail-call mode), and
  - the i32/i64/f32/f64 stack-top ranges must be enabled together (a `static_assert` enforces this).

This matters on ABI-constrained targets: it prevents a tempting “cache only i32” configuration that might fit into 1–2 registers. Without full coverage, a mixed-typed operand stack would force frequent type-dependent materializations and/or additional runtime tags, which would add overhead and complexity and often defeats the point of caching.

### 3) Control-flow dense code: dispatch and predictor pressure become the bottleneck

Once u2 removes most operand-stack memory traffic on cache-hit paths, the dominant remaining cost often becomes **threaded dispatch**:
- load next opfunc pointer,
- advance `ip`,
- indirect branch / musttail jump.

In **control-flow dense** code (short basic blocks, frequent branches, loop bodies with many side exits), two effects become more pronounced:

1) **Indirect-branch prediction and BTB pressure**: a hot loop that alternates among many opfunc entrypoints can stress the branch predictor, especially when code size grows and the instruction working set becomes less stable. Even with perfect operand caching, a mispredicted indirect jump is expensive.

2) **More live “cached state” at joins**: u2’s stack-top cache makes more values “live in registers/locals”. At control-flow joins and loop headers, the interpreter must ensure the cached layout is valid for the target block.

u2’s mitigation is *register-only canonicalization* (“transform-to-begin”):
- The register ring is rotated so the current logical top maps back to the range’s begin slot, without touching operand-stack memory.
- This is implemented in `optable/register_ring.h` (e.g., `uwvmint_stacktop_transform_to_begin`) and is used by control-flow helpers in `optable/conbine.h` (e.g., `uwvmint_br_stacktop_transform_to_begin`).
- The translator emits these transforms at appropriate control-flow edges (see `compile_all_from_uwvm/wasm1.h`).

This avoids the classic “spill everything at every join” strategy, but it is not free: on very branchy code, even small register shuffles add up, and dispatch remains the fundamental ceiling for a non-JIT threaded interpreter.

From a compiler/µarch perspective, there is a real trade:
- A **wider cache** increases the chance that operands are already in registers (good for arithmetic throughput),
- but it also increases the amount of live state that must be kept consistent across control-flow edges, and can increase the number of moves needed for “transform-to-begin” (and the risk of spills under register pressure).

The register-ring approach keeps this cost bounded and predictable (it is a rotation within a fixed range, not an arbitrary permutation), but it cannot make it disappear on extremely branchy kernels.

**Practical consequence.** For branchy dispatch-bound kernels, large gains often come from **superinstruction / mega-op fusion** that reduces the number of dispatches per unit work (u2’s `combine` layers). This preserves the u2 architecture (still threaded, still argument-pack caching) while amortizing the indirect-branch cost over longer semantic sequences.

### 4) Calls and memory-exposing operations reduce cache locality

Any operation that must expose canonical operand-stack state to memory, interact with host imports, or perform complex address computations can force partial or full materialization:
- `call` / `call_indirect`,
- host import calls,
- certain memory operations and boundary checks,
- operations that require precise stack layout for traps/debugging.

These are not “failures” of the cache; they are semantic boundaries. When a workload is dominated by such boundaries, u2’s cache-hit fraction drops and the performance gap to other interpreters naturally narrows.

### 5) Specialization and fusion trade code size for speed

u2 relies on template specialization (different `StartPos`/`Count`/merge layouts) and optional combine layers. This can increase:
- I-cache pressure (more opfunc bodies),
- BTB pressure (more indirect jump targets),
- compile time and binary size.

On large OoO cores this trade is typically favorable for hot code, but on small cores (or branch-heavy kernels with many unique opfunc targets) the “specialization tax” can reduce the marginal benefit of further fusions.

---

## References (in this repository)

- Stack-top cache ring + spill/fill machinery: `optable/register_ring.h`
- Control-flow “transform-to-begin” emission in translation: `compile_all_from_uwvm/wasm1.h`
- Control-flow helpers that combine transform + branch: `optable/conbine.h`
- Per-target stack-top cache sizing (ABI selection): `src/uwvm2/runtime/lib/uwvm_runtime.default.cpp` (`get_curr_target_tranopt()`)
