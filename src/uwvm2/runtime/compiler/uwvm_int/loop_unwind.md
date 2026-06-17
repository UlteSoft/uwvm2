# UWVM2 u2 Interpreter Loop-Unwind Whitepaper

## Abstract

Loop-unwind is a translation-time optimization for the UWVM2 u2 interpreter. Its purpose is not to perform general-purpose loop unrolling for dispatch reduction. Its narrower purpose is to reduce repeated register-ring canonicalization on hot WebAssembly loop backedges when the loop re-entry stack shape is fixed but the stack-top cache ring position drifts across one dynamic iteration.

The optimization is intentionally conservative. It applies only when the translator can prove that re-emitting the loop body preserves the interpreter's register-ring state model, code-size budget, and WebAssembly control-flow semantics. When the optimization is disabled at runtime, compiled out at build time, rejected by policy, or inapplicable to a loop, the interpreter falls back to the existing branch emission path.

## 1. Motivation

The u2 interpreter executes a direct-threaded bytecode stream. Each emitted operation is an opfunc pointer plus immediates. For stack-top values, u2 uses a register-ring cache: the top segment of the WebAssembly operand stack can reside in ABI argument registers instead of operand-stack memory.

Control-flow re-entry is the difficult case. A WebAssembly loop has a statically known label stack type, so every continue edge to the loop header must observe the same logical operand-stack shape. However, the physical register-ring position after one loop body may not match the canonical ring position expected by the loop header. Without loop-unwind, a hot backedge may repeatedly emit a register-ring transform before branching to the loop header.

The intended win is:

```text
without loop-unwind: per-iteration ring transform cost
with loop-unwind:    one transform per unfolded group, or no transform when the group returns to begin
```

This turns a repeated per-iteration repair into an amortized repair. In the full-period case, the unfolded group returns the register ring to the canonical begin position and the final backedge can be emitted as a plain branch.

## 2. WebAssembly Loop Label Semantics

It is important to distinguish loop results from loop continue parameters.

In WebAssembly:

- A `loop` result type describes the values produced when execution falls through the loop body and exits normally.
- A branch to a `loop` label is a continue edge to the beginning of the loop.
- In the multi-value/function-type model, the loop label type for a continue edge is the loop parameter type, not the loop result type.

Therefore, `loop (result i32)` does not by itself mean that `br 0` carries an `i32` value back to the loop header. Under MVP-style block types, loop continue arity is usually zero. Values can still exist below the loop frame, but they are not loop-carried parameters.

This matters for u2. Loop-unwind is valuable when the backedge has stack-top cache state that must be re-entered consistently. If the loop backedge has an empty stack-top cache, there is no register-ring transform to amortize, and the optimization should reject the candidate.

## 3. Register-Ring Recovery Period

For one register-ring range, let:

```text
ring_size = end_pos - begin_pos
delta     = (curr_pos + ring_size - begin_pos) % ring_size
```

If `delta == 0`, the range is already canonical. Otherwise, the minimum number of loop-body copies needed to return this range to the canonical begin position is:

```text
period = ring_size / gcd(ring_size, delta)
```

This is the minimum recovery period. It is not `ring_size * delta`. For example, on an AAPCS64-style five-slot integer ring, deltas 1, 2, 3, and 4 all recover after five iterations because each delta is relatively prime to five.

When multiple register-ring ranges are enabled, u2 computes the least common multiple of the enabled non-trivial range periods:

```text
global_period = lcm(period_i32, period_i64, period_f32, period_f64, period_v128)
```

The optimizer can then choose:

- full unwind: unfold up to `global_period`, making the final branch ring-canonical;
- partial unwind: unfold fewer copies under the size budget and pay one transform at the group backedge;
- no unwind: keep the existing branch path.

## 4. Translation Strategy

Loop-unwind re-emits the original WebAssembly loop body. It does not copy already-emitted u2 bytecode.

This distinction is required for correctness. u2 opfunc pointers are selected using the current compile-time stack-top model, including the current register-ring positions. A byte-for-byte copy of emitted opfunc pointers would preserve opfuncs specialized for the old ring position and would execute with the wrong calling convention state after the first unfolded copy.

The safe strategy is:

1. Save the current WebAssembly parser pointer.
2. Reset `code_curr` to the loop body start.
3. Translate the body again until the original backedge opcode.
4. Repeat for the requested replay count.
5. Restore the saved parser pointer.
6. Emit the original final backedge using the normal branch helper.

Because each replay pass runs through the normal translator, all opfunc selection, stack-top state updates, validation state updates, pending fusion flushing, label emission, and debug checks remain under the same rules as the original body.

## 5. Candidate Policy

The current implementation is deliberately limited. It targets conservative final `br 0` loop backedges where the existing branch path would otherwise need stack-top re-entry repair.

A candidate is rejected when any of the following holds:

- loop-unwind was compiled out or disabled at runtime;
- the control path is polymorphic;
- the loop body is empty;
- the active translation target does not support stack-top register transforms;
- the backedge has no stack-top cache state to repair;
- the register-ring period is one;
- the byte-size budget allows fewer than two total body copies.

The byte-size budget is controlled by:

```text
--runtime-uwvm-int-loop-unwind-max-size <bytes:size_t>
```

The optimization is also capped by an internal maximum unfold count. This prevents pathological code growth even when the loop body is small and the mathematical recovery period is large.

## 6. Build-Time and Runtime Controls

Loop-unwind has both build-time and runtime controls. This split is intentional.
The runtime switch is useful for measurement and diagnosis. The build-time switch
is useful for extremely small environments because it removes the loop-unwind
translation path from the binary.

The build-time gate is:

```text
UWVM_ENABLE_UWVM_INT_LOOP_UNWIND
```

It is controlled by the xmake option:

```text
enable-uwvm-int-loop-unwind
```

When this option is false, loop-unwind code is not compiled into the uwvm-int
translator. When it is true, the optimization remains subject to runtime policy.

Runtime controls:

```text
--runtime-uwvm-int-disable-loop-unwind
--runtime-uwvm-int-loop-unwind-max-size <bytes:size_t>
```

The default max-size budget is 4096 Wasm body bytes. The runtime aliases are:

```text
-Rint-no-loop-unwind
-Rint-loop-unwind-size
```

Related runtime controls for isolating measurements:

```text
--runtime-uwvm-int-set-opcode-conbination-level disable
--runtime-uwvm-int-disable-delay-local
```

The first option keeps the existing public spelling `conbination`; the level
selector is only present when opcode conbination is compiled in. In design text,
this document uses the standard spelling "combination" unless it refers to an
actual command-line option, source file, or symbol.

## 7. Full and Partial Unwind

Let `unroll_count` be the total number of loop-body copies represented by the group, including the original body.

For full unwind:

```text
unroll_count == global_period
```

After replaying `unroll_count - 1` additional bodies, the register ring is back at the canonical begin position. The final backedge can therefore be emitted as a plain branch.

For partial unwind:

```text
2 <= unroll_count < global_period
```

The group still reduces transform frequency, but the final group backedge must repair the ring state before jumping to the loop header.

For no unwind:

```text
unroll_count < 2
```

The original branch path is emitted.

## 8. Interaction with Fusion

Loop-unwind is independent of opcode combination and delay-local, but the implementation must not let pending local fusion state cross replay boundaries. After each replayed loop body, pending branch-if fusion and pending combination state are flushed or cleared before the next replay starts.

This preserves the translator invariant that a fusion candidate only spans the exact WebAssembly instruction sequence that the normal one-pass translator would consider valid.

## 9. Logging and Measurement

When runtime compiler logging is enabled, the translator records:

- candidate count;
- applied and rejected count;
- full and partial unwind count;
- replayed body count;
- replayed WebAssembly bytes;
- generated bytecode bytes;
- rejection reasons;
- per-loop period, chosen unroll count, body size, byte-size budget, and stack-top state.

This information is emitted through:

```text
--runtime-compiler-log [out|err|file <file:path>]
```

The function-level summary includes:

```text
loop_unwind{cand=...,applied=...,reject=...,full=...,partial=...,replay=...,wasm_bytes=...,bytecode_bytes=...}
```

This is intentionally visible because loop-unwind can otherwise look like a no-op on MVP-style corpora.

## 10. Current Implementation Status

The implemented path is a conservative MVP-compatible foundation, not the final multi-value optimization.

Current scope:

- final unconditional loop backedges;
- fixed target stack size at the backedge;
- stack-top transform amortization only;
- byte-size-limited replay through the normal translator;
- runtime and build-time disable controls.

Current limitations:

- loop-carried parameter payloads are not yet optimized in the non-zero label-arity path;
- conditional continue edges are not redirected to later unfolded copies;
- the pass does not try to reduce dispatch frequency for loops that have no stack-top re-entry transform;
- MVP/LLVM-generated loops commonly have empty backedge cache state, so loop-unwind often rejects with `empty_cache`.

In u2bench `int_dense` measurements on the current MVP corpus, loop-unwind candidates were observed but all were rejected because the backedge stack-top cache was empty. That is the expected behavior: if there is no ring repair to amortize, loop-unwind should not inflate code size.

## 11. Future Work

The main future target is the multi-value/function-type loop model. Once loop parameters are represented in the translator's branch payload path, loop-unwind should be extended to non-zero label arity so that fixed loop parameters can be carried through unfolded groups without repeated ring repair.

Other possible extensions:

- redirect conditional continue edges within an unfolded group when it is safe;
- apply hotness or profile-guided thresholds instead of static byte-size thresholds only;
- integrate loop-unwind decisions with branch/superinstruction fusion;
- provide a separate general loop unroller for dispatch reduction, distinct from register-ring re-entry repair.

Loop-unwind should remain conservative. Its value is not in forcing every loop to grow; its value is in removing a specific repeated register-ring repair when the WebAssembly loop type and u2 stack-top model make that repair both predictable and expensive.
