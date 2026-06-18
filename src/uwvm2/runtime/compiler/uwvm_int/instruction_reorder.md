# UWVM2 u2 Interpreter Instruction-Reorder Whitepaper

## Abstract

Instruction reorder is an experimental translation-time recompilation layer for the UWVM2 u2 interpreter. Its purpose is to increase useful work per direct-threaded dispatch and to keep the register-ring stack-top cache occupied on LLVM-style WebAssembly that is dominated by shallow `local.get` producers.

This layer is deliberately **off by default**. It is closer to a small register-ring-aware recompilation pass than to ordinary interpreter peephole fusion, and it should be enabled only for experiments or workloads that justify the extra translation policy.

Instruction reorder is not part of `conbine`. Adjacent opcode combination emits superinstructions for already-adjacent Wasm patterns. Instruction reorder first proves that a side-effect-free expression window can be legally regrouped, then emits an optable entry that represents the recompiled window. The implementation therefore lives in `optable/instruction_reorder.h`.

The current implementation is intentionally conservative. It does not run a global optimizer and it does not reorder arbitrary WebAssembly instructions. It has four bounded forms:

- full-register-ring `local.get` preload;
- same-op integer local reduction;
- one-step constant integer local update;
- mixed local/constant integer expression fold;
- expression-local-update recompilation for `local.set` and selected `local.tee` endpoints.

The base form recognizes consecutive same-typed `local.get` producers:

```text
local.get a
local.get b
local.get c
...
```

and emits one register-ring preload opfunc:

```text
uwvmint_reorder_preload_nlocalget<..., LocalCount, ...>
```

The preload form preserves the exact WebAssembly producer order. Its job is not to fold a consumer; it rebuilds a shallow LLVM local burst so the next arbitrary opcode can consume several live operands from the register ring.

The pure reduction form recognizes integer local-only left-folds:

```text
local.get a
local.get b
i32.add / i32.mul / i32.and / i32.or / i32.xor
local.get c
same integer op
...
```

with equivalent `i64` forms, and emits a single register-ring-aware local-reduction opfunc:

```text
uwvmint_reorder_int_reduce_nlocalget<..., Op, LocalCount, ...>
```

This gives the translator a bounded, semantics-preserving way to turn both LLVM-local-heavy producer bursts and selected integer expressions into register-ring-friendly dispatches.

The mixed expression form accepts a longer no-trap integer left fold whose RHS operands are either same-typed `local.get` or integer constants:

```text
local.get a
i32.const k0
i32.add
local.get b
i32.xor
i32.const k1
i32.shl
...
```

It preserves the original left-to-right stack-machine order and emits one fold opfunc only when the window is large enough to amortize the generic expression decoder.

The local-update form recognizes the same expression class when it is immediately followed by `local.set` or a non-branch-fusion `local.tee`:

```text
local.get a
local.get b
i32.add
local.get c
i32.add
local.set dst
```

Instead of producing a temporary operand-stack result and then consuming it with a separate local update, the recompiled opfunc computes the value and writes the destination local directly. The `local.tee` variant writes the local and pushes exactly one result, matching WebAssembly semantics.

The short constant-update form exists because LLVM-generated Wasm contains a very high volume of
one-step address and flag expressions:

```text
local.get src
i32.const k
i32.add / i32.and / i32.shl / ...
local.set dst
```

The generic mixed-expression decoder is intentionally gated to longer windows, because decoding a
one-step bytecode program at runtime costs more than it saves. The short form therefore emits a
dedicated compile-time-binop opfunc for the one-step case. It is used only when `dst` is different
from `src`; same-local updates remain owned by ordinary update-local combination, which has the
smaller established runtime shape for induction-variable updates.

## 1. Motivation

u2's register ring is most effective when several live operand-stack values stay in ABI argument registers across adjacent opfuncs. LLVM-generated MVP WebAssembly often uses locals aggressively and keeps the operand stack shallow. A common shape is:

```text
local.get lhs
local.get rhs
i32.add
local.get next
i32.add
```

This shape repeatedly creates and consumes only one or two operand-stack values. A delay-local peephole can remove some provider dispatch, but it still operates around a short producer-consumer window. Instruction reorder broadens the window when the translator can prove that doing so is equivalent.

The more common and less specialized shape is an LLVM local burst:

```text
local.get p
local.get q
local.get r
i32.add
i32.xor
```

Without preload, each `local.get` is a separate producer and the ring may never become usefully occupied before the consumer chain starts. With preload, the translator emits one dispatch that writes `p`, `q`, and `r` to the ring in the same logical stack order as three separate `local.get` opfuncs. The following `i32.add` and `i32.xor` are unchanged, but they now see a hotter stack-top cache.

For the implemented integer reductions, the emitted opfunc reads all participating locals directly, computes the selected integer reduction, and pushes exactly one result. The logical WebAssembly stack effect remains identical to the original chain.

## 2. Legality Model

The optimization is legal only because the recognized window has all of the following properties:

- Every producer in a preload or pure-reduction window is `local.get`.
- Mixed expression windows may use same-typed `local.get` and same-width integer constants as RHS operands.
- One-step constant-update windows must have exactly one same-width integer constant followed by one legal no-trap integer binary operation.
- Preload windows contain only consecutive `local.get` opcodes of the same scalar value type.
- Reduction windows contain one repeated integer operation among `add`, `mul`, `and`, `or`, or `xor`.
- Mixed expression windows contain only no-trap integer operations: `add`, `sub`, `mul`, bitwise ops, shifts, and rotates.
- Local-update windows may end in `local.set` or `local.tee` only when the destination local has the same value type as the computed expression.
- The one-step constant-update form is restricted to different source and destination locals so it does not steal same-local update-local combination.
- `local.tee` windows are not rewritten when the next opcode is `br_if`; that path is reserved for the branch-fusion machinery.
- All participating locals have the same value type. Preload accepts scalar `i32`, `i64`, `f32`, and `f64`; reduction accepts only `i32` and `i64`.
- The window contains no memory access, global access, call, branch, table operation, or control-flow boundary.
- The window contains no operation that can trap.
- The translator is not in a polymorphic unreachable segment.
- No pending combination or delay-local state is active when the window starts.

WebAssembly validation gives each `local.get` a statically known local type and a single stack push, while locals are mutated only by `local.set` and `local.tee`. A consecutive `local.get` preload therefore has no hidden side effects and does not cross a mutation point.

WebAssembly integer `add` and `mul` are defined modulo `2^32` or `2^64`; bitwise `and`, `or`, and `xor` are associative bit-vector operations. The translator may therefore regroup homogeneous left folds for these operations without changing the result. This rule does not apply to floating-point arithmetic, integer division/remainder, shifts/rotates with ordered RHS semantics, numeric conversions that can trap, memory operations, calls, or branch conditions.

## 3. Translation Strategy

The implementation is triggered from the `local.get` case of:

```text
compile_all_from_uwvm/translate/opcode/variable_cases.h
```

At the first `local.get`, the translator performs bounded lookahead. Same-op local-update reduction is attempted before generic mixed expressions because it can use a compile-time `Op` template and avoids the runtime expression switch. Then the translator tries mixed expression local-update, pure local reduction, mixed expression fold, and finally full-ring preload.

Before any bounded scanner runs, the translator reads the opcode immediately following the current
`local.get` once. If that opcode cannot start a supported reorder window, all reorder scanners are
skipped. A following `local.get` may start preload, reduction, or mixed-expression forms; a
same-width integer constant may start only a one-step constant-update or mixed-expression form. This first-op gate is part of
the profitability model: instruction reorder is runtime opt-in, but an opt-in pass still must keep
translation overhead proportional to real candidates instead of probing every `local.get` in a
clang-generated module.

For preload:

1. Read the current local's type and frame offset.
2. Scan following `local.get` opcodes while their locals have the same scalar type.
3. Read the current architecture/`CompileOption` register-ring size for that value type.
4. Require exactly one full register-ring worth of local reads for that value type. Shorter windows are left to delay-local and adjacent combination because they are usually better handled as ordinary producer-consumer pairs.
5. If the architecture exposes a larger ring than the currently generated preload opfunc family supports, do not partially preload the window; disable this preload form until the generated family is widened.
6. Spill enough existing cached values to make room for the whole burst.
7. Emit `uwvmint_reorder_preload_nlocalget` with an immediate local-count byte followed by the local frame offsets.
8. Update the validation operand-stack model with `N` pushed values.
9. Commit `N` typed stack-top pushes in the register-ring model.

For integer reduction:

1. Read the current local's type and frame offset.
2. Scan repeated pairs of `local.get same_type` followed by the same legal integer reduction opcode.
3. Stop at the first opcode that does not match the exact safe pattern.
4. Require at least three local reads.
5. Emit `uwvmint_reorder_int_reduce_nlocalget` with an immediate local-count byte followed by the local frame offsets.
6. Update the validation operand-stack model with one pushed value of the integer type.
7. Commit one typed stack-top push in the register-ring model.

For same-op local-update reduction:

1. Scan the same local-only reduction window.
2. Require a following `local.set` or a `local.tee` not immediately followed by `br_if`.
3. Emit `uwvmint_reorder_int_reduce_nlocalget_local_update` with an immediate local-count byte, destination local frame offset, and source local frame offsets.
4. For `local.set`, model no operand-stack change.
5. For `local.tee`, model one pushed value and commit one typed stack-top push.

For one-step constant local update:

1. Require the current `local.get` to be followed by `i32.const` or `i64.const` of the same width.
2. Require one no-trap integer binary operation: add, sub, mul, bitwise op, shift, or rotate.
3. Require an immediate `local.set` or a `local.tee` not immediately followed by `br_if`.
4. Require the destination local to have the same value type and a different frame offset from the source local.
5. Emit `uwvmint_reorder_int_const_binop_local_update` with source local offset, immediate, and destination local offset.
6. For `local.set`, model no operand-stack result.
7. For `local.tee`, model one pushed value and commit one typed stack-top push.

For mixed expression fold and mixed expression local update:

1. Start from the current typed local as the accumulator.
2. Scan up to eight RHS steps. Each RHS must be a same-typed `local.get` or a same-width integer constant.
3. Require the following operation to be a no-trap integer op with the same width as the accumulator.
4. Roll back to the start of the step when a RHS was parsed but the following operation is not a legal expression op. This avoids swallowing a constant or local that belongs to the next normal Wasm expression.
5. For a plain fold, require at least four steps and push one result.
6. For local-update form, require at least four steps, or use the same-op local-update reduction and one-step constant-update paths for shorter profitable windows.
7. For `local.tee`, skip the rewrite if the next opcode is `br_if`.

The generated preload family currently covers rings up to eight slots. The policy is intentionally strict: `N` is the active register-ring size, not an arbitrary profitability constant. Architectures with a wider future ring must add matching generated preload variants before this pass can use that wider ring.

## 4. Runtime Shape

The recompiled opfuncs are implemented in:

```text
optable/instruction_reorder.h
```

They have tail-call and by-reference variants, matching the rest of u2's optable split. The bytecode layout is:

```text
[opfunc pointer]
[local_count : uint8_t]
[local_offset_0 : local_offset_t]
...
[local_offset_N-1 : local_offset_t]
[next opfunc pointer]
```

The preload opfunc loads local values from the compiled frame and pushes each one in original source order. On tail-call stack-top builds, each push writes the next `ring_prev(currpos)` slot, exactly matching the effect of `N` separate `local.get` opfuncs. On by-reference or non-stack-top builds, it copies the same values to operand-stack memory in source order.

The reduction opfunc loads local values from the compiled frame, folds from the last local back to the first, and pushes one integer result through the normal stack-top helper. Folding from the last local back to the first matches the value that the original stack-machine left fold would leave on top of the operand stack.

The same-op local-update reduction layout is:

```text
[opfunc pointer]
[local_count : uint8_t]
[dst_local_offset : local_offset_t]
[local_offset_0 : local_offset_t]
...
[local_offset_N-1 : local_offset_t]
[next opfunc pointer]
```

The one-step constant-update layout is:

```text
[opfunc pointer]
[src_local_offset : local_offset_t]
[imm : wasm_i32 or wasm_i64]
[dst_local_offset : local_offset_t]
[next opfunc pointer]
```

The mixed expression fold and mixed expression local-update layouts are:

```text
[opfunc pointer]
[step_count : uint8_t]
[first_local_offset : local_offset_t]
[optional dst_local_offset : local_offset_t]
[
  op : uint8_t
  operand_kind : uint8_t
  rhs_payload : local_offset_t | wasm_i32 | wasm_i64
] * step_count
[next opfunc pointer]
```

The destination offset is present only for the local-update form. `operand_kind` selects either a local frame offset or an inline signed integer immediate. The runtime evaluator applies the encoded operations in original Wasm left-fold order; it never reassociates non-associative operations such as `sub`, shifts, or rotates.

## 5. Controls and Logging

The optimization must be compiled in explicitly:

```text
--enable-uwvm-int-instruction-reorder=y
```

Even when compiled in, runtime defaults to off. It must be enabled explicitly:

```text
--runtime-uwvm-int-enable-instruction-reorder
```

Alias:

```text
-Rint-reorder
```

The opcode-combination runtime disable switch does not disable instruction reorder. Reorder has its own opt-in switch because it is a separate recompilation layer, not a combine submode.

When runtime compiler logging is enabled, the function summary reports:

```text
reorder{cand=...,applied=...,local_preload=...,local_reduce=...,reduce_set=...,reduce_tee=...,expr_fold=...,expr_set=...,expr_tee=...,expr_steps=...,local_reads=...}
```

`local_reads` records the total number of `local.get` producers consumed by successful reorders. `expr_steps` records the total number of mixed expression steps executed by folded or local-update expression opfuncs. This makes the pass visible in microbenchmarks and in real clang-generated modules.

## 6. Relationship to Future Scheduling

This pass is a safe first stage, not a full scheduler. A future stack-SSA DAG scheduler can generalize the idea by building basic-block-local dependency graphs, respecting trap and side-effect ordering, and scoring schedules by register-ring occupancy, dispatch count, and code-size pressure.

The current bounded recompiler deliberately avoids that complexity. It establishes the translator interface, opfunc shape, runtime logging, and legality discipline needed for broader instruction reorder while keeping the shipped behavior easy to audit.
