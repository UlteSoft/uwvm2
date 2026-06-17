# UWVM2 u2 Interpreter Instruction-Reschedule Note

## Abstract

Instruction rescheduling is a translation-time optimization for the UWVM2 u2 interpreter. Its purpose is to increase useful work per direct-threaded dispatch and to keep the register-ring stack-top cache occupied on LLVM-style WebAssembly that is dominated by shallow `local.get` producers.

The current implementation is intentionally conservative. It does not run a global optimizer and it does not reorder arbitrary WebAssembly instructions. The first implemented form recognizes a pure integer left-fold:

```text
local.get a
local.get b
i32.add / i64.add
local.get c
i32.add / i64.add
...
```

and emits a single fused integer add-reduction opfunc:

```text
uwvmint_int_add_reduce_nlocalget<..., LocalCount, ...>
```

This gives the translator a bounded, semantics-preserving way to turn an LLVM-local-heavy expression into one register-ring-friendly dispatch.

## 1. Motivation

u2's register ring is most effective when several live operand-stack values stay in ABI argument registers across adjacent opfuncs. LLVM-generated MVP WebAssembly often uses locals aggressively and keeps the operand stack shallow. A common shape is:

```text
local.get lhs
local.get rhs
i32.add
local.get next
i32.add
```

This shape repeatedly creates and consumes only one or two operand-stack values. A delay-local peephole can remove some provider dispatch, but it still operates around a short producer-consumer window. Instruction rescheduling broadens the window when the translator can prove that doing so is equivalent.

For the implemented add-reduction, the emitted opfunc reads all participating locals directly, computes the modulo integer sum, and pushes exactly one result. The logical WebAssembly stack effect remains identical to the original chain.

## 2. Legality Model

The optimization is legal only because the recognized window has all of the following properties:

- Every producer is `local.get`.
- Every consumer is `i32.add` or `i64.add`.
- All participating locals have the same integer value type.
- The window contains no memory access, global access, call, branch, table operation, or control-flow boundary.
- The window contains no operation that can trap.
- The translator is not in a polymorphic unreachable segment.
- No pending combination or delay-local state is active when the window starts.

WebAssembly integer addition is defined modulo `2^32` or `2^64`. Under that arithmetic, addition is associative, so the translator may regroup a left fold without changing the result. This rule does not apply to floating-point addition, integer division/remainder, numeric conversions that can trap, memory operations, calls, or branch conditions.

## 3. Current Translation Strategy

The implementation lives in the `local.get` case of:

```text
compile_all_from_uwvm/translate/opcode/variable_cases.h
```

At the first `local.get`, the translator performs bounded lookahead:

1. Read the current local's type and frame offset.
2. Scan repeated pairs of `local.get same_type` followed by the matching integer add opcode.
3. Stop at the first opcode that does not match the exact safe pattern.
4. Require at least three local reads. Shorter windows are left to existing delay-local and adjacent-combination paths.
5. Emit `uwvmint_int_add_reduce_nlocalget` with an immediate local-count byte followed by the local frame offsets.
6. Update the validation operand-stack model with one pushed value of the integer type.
7. Commit one typed stack-top push in the register-ring model.

The lookahead is capped at eight local reads. This keeps template instantiation count, bytecode immediate size, and generated machine code bounded while still covering the register-ring filling case that motivated the optimization.

## 4. Runtime Shape

The fused opfunc is implemented in:

```text
optable/conbine_heavy.h
```

It has tail-call and by-reference variants, matching the rest of u2's optable split. Its bytecode layout is:

```text
[opfunc pointer]
[local_count : uint8_t]
[local_offset_0 : local_offset_t]
...
[local_offset_N-1 : local_offset_t]
[next opfunc pointer]
```

The opfunc loads local values from the compiled frame, folds from the last local back to the first, and pushes one integer result through the normal stack-top helper. Folding from the last local back to the first matches the value that the original stack-machine left fold would leave on top of the operand stack.

## 5. Controls and Logging

The optimization is part of heavy opcode combination:

```text
--enable-uwvm-int-combine-ops=heavy
```

At runtime it is controlled with the existing opcode-combination switch:

```text
--runtime-uwvm-int-disable-opcode-conbination
```

The public option intentionally keeps the historical spelling `conbination`.

When runtime compiler logging is enabled, the function summary reports:

```text
reorder{cand=...,applied=...,local_add=...,local_reads=...}
```

`local_reads` records the total number of `local.get` producers consumed by successful add-reduction reschedules. This makes the pass visible in microbenchmarks and in real clang-generated modules.

## 6. Relationship to Future Scheduling

This pass is a safe first stage, not a full scheduler. A future stack-SSA DAG scheduler can generalize the idea by building basic-block-local dependency graphs, respecting trap and side-effect ordering, and scoring schedules by register-ring occupancy, dispatch count, and code-size pressure.

The current add-reduction pass deliberately avoids that complexity. It establishes the translator interface, opfunc shape, runtime logging, and legality discipline needed for broader instruction rescheduling while keeping the first shipped behavior easy to audit.
