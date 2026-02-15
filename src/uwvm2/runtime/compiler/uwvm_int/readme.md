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

## References (in this repository)

- Stack-top cache ring + spill/fill machinery: `optable/register_ring.h`
- Control-flow “transform-to-begin” emission in translation: `compile_all_from_uwvm/wasm1.h`
- Control-flow helpers that combine transform + branch: `optable/conbine.h`
