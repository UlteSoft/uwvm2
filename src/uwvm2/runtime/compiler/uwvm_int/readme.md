# UWVM2 “u2” Interpreter: A Measurement-Driven Threaded VM Design (Register-Ring + Fusion)

## Abstract

**u2** is UWVM2’s WebAssembly interpreter (`src/uwvm2/runtime/compiler/uwvm_int/`). It is a **non-JIT, non-self-modifying** engine built around **direct threading** (a stream of opfunc pointers + immediates) and a register-friendly **stack-top cache** implemented as a compile-time specialized **register ring**.

The design goal is strong end-to-end performance for an interpreter tier: **fast translation** (linear-time emission of a code page with no runtime opcode decode) and **high interpreter throughput** (reduced operand-stack memory traffic + reduced dispatch density via fusion). u2 explicitly trades **code size** for predictable hot-path shape: it contains many template-specialized opfunc variants and therefore is not positioned as a minimal/lightweight interpreter in the wasm3/WAMR class. In a tiered system, u2 is intended to serve as a **JIT warm-start backend** rather than a smallest-possible standalone interpreter.

This document summarizes both (1) the architecture and (2) the optimization workflow used to reach competitive results: instrument → quantify dispatch/fusion → audit codegen via AArch64 “asm stitching” → implement leaf-preserving fusion and translation-time peepholes.

## 1. Goals and non-goals

### 1.1 Goals
- **Fast steady-state execution** for stack-heavy Wasm by turning hot scalar ops into mostly register-resident ALU.
- **Low translation overhead**: emit a direct-threaded stream once; avoid runtime opcode decode.
- **ABI-scalable caching**: widen the stack-top cache when the platform ABI provides more argument registers.
- **Leaf-friendly opfuncs** in tail-threaded mode to keep dispatch as “load next pointer + indirect branch” without prologues on hot paths.

### 1.2 Non-goals
- Claiming “fastest” or “optimal” across all workloads.
- Minimizing code size (u2’s optable is intentionally large due to specialization).
- Eliminating dispatch entirely (that is the domain of JIT/AOT/tracing/superinstruction systems beyond adjacent fusion).

## 2. System overview

### 2.1 Components
- **Translator**: `compile_all_from_uwvm/wasm1.h`
  - Parses Wasm bytecode and emits u2 bytecode (“code page”) per function.
  - Performs local, linear-time fusion and peepholes (no global optimizer pass).
- **Optable (opfunc set)**: `optable/*.h`
  - Dispatch targets for arithmetic/control/memory/calls plus fused patterns.
- **Register-ring cache**: `optable/register_ring.h`
  - Spill/fill and transform operations between canonical operand stack memory and the cached stack-top segment.

### 2.2 Execution model (direct-threaded stream)
u2 executes a pointer stream:
1) `ip` points at an opfunc pointer stored as raw bytes in the code page.
2) The opfunc reads any immediates following the pointer (e.g., local offsets, memarg offsets).
3) The opfunc tail-dispatches by loading the next opfunc pointer and `br`/`UWVM_MUSTTAIL return` to it.

The translator emits opfunc pointers directly (see `emit_opfunc_to(...)` in `compile_all_from_uwvm/wasm1.h`), so runtime does not decode Wasm opcodes.

## 3. Register-ring stack-top cache (operand-stack traffic reduction)

### 3.1 What is cached
WebAssembly is a mixed-typed stack machine. u2 caches the hot segment of the operand stack in an ABI-packed “opfunc argument tuple”:
- fixed interpreter arguments: `(ip, operand_stack_top_ptr, local_base_ptr)`, plus
- a configurable number of stack-top cache slots carried across dispatch.

The cache is modeled as per-type rings (i32/i64/f32/f64, optionally v128 carriers), and ranges may be merged to reduce register pressure.

### 3.2 Spill/fill and transforms
When the cache cannot satisfy an operand or must materialize state, the translator emits spill/fill opfuncs:
- **spill**: cache → operand stack memory
- **fill**: operand stack memory → cache

These mechanisms live in `optable/register_ring.h` and are specialized so hot paths remain small (pointer bumps + bounded moves).

### 3.3 Control-flow re-entry: register-only canonicalization
Control-flow joins (loops, if/else merges) can require a canonical cache layout. u2 supports a register-only “transform-to-begin” that rotates the ring to a canonical begin position without touching operand-stack memory:
- emission: `compile_all_from_uwvm/wasm1.h`
- helpers: `optable/register_ring.h`, plus control variants in `optable/conbine.h`

## 4. Translation pipeline (startup cost and translation throughput)

Translation in `compile_all_from_uwvm/wasm1.h` is designed as a single-pass state machine:
- Track a compiler-side model of operand stack + cache residency.
- Emit direct-threaded bytecode: opfunc pointer bytes + immediates.
- Batch consecutive spill/fill when safe to reduce dispatch and bytecode size.

Because the runtime stream is already “decoded” into opfunc pointers, translation time is part of the end-to-end performance story: u2 aims to keep translation close to linear in Wasm instruction count and to keep fusion localized (bounded lookahead / peepholes), avoiding heavyweight IR pipelines.

## 5. Fusion layers (dispatch density reduction)

Threaded interpreters have a hard ceiling set by dispatch (`load next target + indirect jump`). Once operand-stack traffic is reduced, dispatch can become dominant. u2 therefore uses translation-time fusion to increase “work per dispatch”.

### 5.1 `conbine`: adjacent-op fusion (soft/heavy/extra)
The translator maintains a small pending state machine (`conbine_pending_t` in `compile_all_from_uwvm/wasm1.h`). When a provider sequence can continue (`conbine_can_continue(...)`), emission is delayed and later flushed as a fused opfunc. This provides superinstruction-like wins while keeping translation local and predictable.

Optable implementations:
- baseline: `optable/conbine.h`
- heavy (higher-coverage skeletons): `optable/conbine_heavy.h`
- extra-heavy (mega-ops): `optable/combine_extra_heavy.h`

### 5.2 `delay_local`: treating `local.get` as a virtual operand source
Many real Wasm workloads are dominated by “providers” (`local.get`, `*.const`) feeding short consumers. A major performance lever is to avoid standalone provider dispatch and instead select consumer variants that read from locals/slots directly (wasm3’s systematic `_rs/_sr/_ss` strategy).

u2’s `delay_local` provides analogous consumer variants:
- It defers materializing `local.get` until the first consuming op.
- It emits consumer opfuncs that load directly from the local slot (e.g., `uwvmint_i32_binop_localget_rhs` in `optable/delay_local.h`).
- It compiles `local.get rhs; binop` as a **net stack effect = 0** transformation relative to the pre-`local.get` state, avoiding push/pop state-machine overhead.

Build knobs (see the analysis note for empirical motivation):
- `--enable-uwvm-int-combine-ops={none,soft,heavy,extra}`
- `--enable-uwvm-int-delay-local={none,soft,heavy}`

## 6. Leaf hot-path discipline (tail-threaded correctness and performance)

In tail-threaded mode, u2 keeps opfunc hot paths strictly **leaf** where possible. This matters on AArch64 because a normal call (`bl`) tends to force prologue/epilogue in the caller to preserve the link register, even if the call is cold.

Numeric traps illustrate the pattern:
- `optable/numeric.h` defines cold, noinline tail wrappers such as `trap_integer_divide_by_zero_tail<CompileOption>()` and `trap_integer_overflow_tail<CompileOption>()`.
- Hot opfuncs (including `delay_local` variants in `optable/delay_local.h`) branch/tail-call to these wrappers on exceptional paths, keeping the fast path as leaf + direct tail-dispatch.

## 7. Engineering workflow: quantify → audit → patch

The u2 design is validated and refined by a measurement-driven loop (captured in the MacroModel analysis note `test/uwvm2test/u2_vs_wasm3_asm_stitch_analysis.txt`, dated 2026-02-20…2026-02-23):

1) **Quantify fusion and dispatch** with u2 runtime logs (`-Rclog`): wasm op counts, opfunc counts (dispatch), spill/fill counts, and control-flow pressure.
2) **Audit codegen limits** by compiling opfuncs to AArch64 assembly and “stitching” representative hot sequences. This reveals when both interpreters are already near the same dispatch skeleton ceiling.
3) **Implement the highest-leverage fixes**:
   - Reduce dispatch by systematic provider/consumer handling (`delay_local`).
   - Add high-hit-rate skeleton fusions in heavy mode (e.g., loop counters).
   - Add translation-time peepholes that remove semantic no-ops.
   - Preserve leaf hot paths (trap tail wrappers; avoid cold `bl` in opfunc bodies).

## 8. Representative high-impact optimizations (source-linked)

These mechanisms are examples of u2’s “heavy but general” direction: they aim for broad coverage without workload-private mega-ops.

### 8.1 Dead stack traffic elimination (`const* + drop*`)
Some modules (and microbenchmarks) contain long runs of side-effect-free providers followed by the same number of `drop`s; semantically these segments are no-ops. u2 can remove them at translation time to avoid pathological dispatch-bound loops:
- implementation: `try_elide_const_run_with_drops()` in `compile_all_from_uwvm/wasm1.h`

### 8.2 Trivial local-defined call fast path
Small local-defined callees that are semantic no-ops or constant-return functions can be recognized and executed without constructing a full call frame:
- matcher: `match_trivial_call_inline_body()` in `compile_all_from_uwvm/wasm1.h`
- metadata: `trivial_defined_call_kind` and `compiled_defined_call_info` in `optable/define.h`

### 8.3 Loop skeleton fusion (“inc; cmp; br_if”)
High-hit-rate counter-loop patterns can be emitted as a single fused opfunc to reduce per-iteration dispatch:
- opfunc: `uwvmint_for_i32_inc_lt_u_br_if` in `optable/conbine_heavy.h`
- translator rewrite at `br_if`: `compile_all_from_uwvm/wasm1.h`

## 9. Interpreting results: when u2 is “good”

u2 is evaluated on two axes:
- **Translation performance**: linear-time translation to a direct-threaded stream; localized fusion/peepholes.
- **Interpretation performance**: register-ring cache reduces operand-stack traffic; fusion reduces dispatch density.

Key lessons from the 2026-02 analysis on AArch64 (Apple M-series) are:
- If the average fusion ratio is low (e.g., ~1.5 wasm ops per dispatch), different threaded interpreters tend to converge toward the same dispatch ceiling (indirect branch + next pointer load), and performance differences narrow.
- Large gains often come from reducing dispatch count (provider/consumer variantization, loop skeleton fusion) rather than shaving single instructions inside an already-minimal opfunc.
- u2’s generality (notably in memory and runtime paths) can lengthen dependence chains and increase register pressure in memory-heavy or branch-heavy kernels; this is an accepted trade for broader functionality and JIT-warm-start positioning.

### 9.1 AArch64 case study (2026-02-20…2026-02-23)

The following results are *not* universal claims; they summarize a specific AArch64/Apple Silicon workflow captured in `test/uwvm2test/u2_vs_wasm3_asm_stitch_analysis.txt`:

- **Corpus matrix (wall-time, 2026-02-23)**: scanning `*.wasm` under `/tmp/uwvm2test` and comparing modules where both engines succeeded (n=383), the reported wall-time ratio geomean was `uwvm/wasm3 = 0.5957` (u2 faster on average), with median `0.7116`.
- **Algorithm kernels (internal-time, 2026-02-20)**: for representative WASI algorithm modules, the report includes examples such as ChaCha20 (u2 398ms vs wasm3 515ms), SHA512 (u2 202ms vs wasm3 220ms), and BLAKE2s (u2 3094ms vs wasm3 3427ms).
- **Code size trade (Mach-O `__TEXT`)**: enabling `delay_local` was measured as a small increase in text size (e.g., heavy+delay=none `__TEXT≈1,409,024` vs heavy+delay=heavy `__TEXT≈1,441,792`), while extra-heavy options increased it further in that setup.

The main engineering takeaway from these measurements is methodological: when two interpreters share a similar direct-threaded dispatch skeleton, the largest remaining wins often come from **reducing dispatch count** (systematic provider/consumer handling and high-hit-rate skeleton fusion), while maintaining the “leaf hot-path” invariant needed for clean tail-threaded codegen.

## 10. Repository map

- Translation and fusion state machine: `compile_all_from_uwvm/wasm1.h`
- Stack-top cache ring, spill/fill, transforms: `optable/register_ring.h`
- Adjacent fusion: `optable/conbine.h`, `optable/conbine_heavy.h`, `optable/combine_extra_heavy.h`
- Delay-local fused opfuncs: `optable/delay_local.h`
- Numeric ops and trap wrappers: `optable/numeric.h`
- Memory ops (generality and fast paths): `optable/memory.h`
- Per-target translate options (ABI sizing): `src/uwvm2/runtime/lib/uwvm_runtime.default.cpp` (`get_curr_target_tranopt()`)
