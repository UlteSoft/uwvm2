# Common typed stack GC/EH slice (source candidate)

The proposal unifies unconditional-transfer current-frame retirement/polymorphism, concrete-frame underflow, value-polymorphic Bot, complete rich type matching, repeated operands, and `ref.as_non_null` reification through `typed_stack_semantics.h`. Pure wasm3, interpreter fused GC/EH, and LLVM fused GC/EH consume their existing owned stack through this same kernel. Backend accounting and lowering remain in their existing adapters. Interpreter accounting is collected after successful semantic consumption; valid output accounting is unchanged, while semantic failures take precedence over a backend accounting limit.

The LLVM aggregate 0xfb subopcodes 0–19 now lower the bounded scanner's original decoded immediate without replaying its bytes. The actual validation dispatcher already sets function-relative provenance, rejects pending-numeric incompatibility, and emits the exact debug-safe-point before any opcode handler. The new helper preserves unreachable suppression and calls the original aggregate emitter, including precise roots, sealed retirement, bridge ABI, and GC31 specializations. Other GC subopcode lowerings and other compiler families still await migration; this slice does not claim complete shared control semantics or eliminate every older whole-body scanner.

`throw_ref` uses the complete reference type in all three consumers; LLVM's former storage-carrier-only check is removed. `ref.as_non_null` reifies value Bot to non-null reference heap Bot, so the reified value can match an exception reference but cannot satisfy a numeric operand. Native EH routing, interpreter musttail/ring handlers, and ordinary memory IR are unchanged.

Normative sources consulted on 2026-10-03: [Core 3 validation algorithm](https://webassembly.github.io/spec/core/appendix/algorithm.html), especially `pop_val`, `pop_ref`, and unreachable-frame rules; [Core 3 instruction validation](https://webassembly.github.io/spec/core/valid/instructions.html) for array creation and exception reference typing. No source permission, runtime stop ticket, or native code capability comes from these compiler DATA callbacks.

No build, unit, WAT parse, official validation, VM execution, or assembly inspection has been performed for this candidate. The new unit uses actual parser-owned type metadata and actual GC/EH helpers. The seven WAT sources comprise one called positive and six known-wrong negatives. The positive calls both new-syntax callees so a lazy backend must compile them; its selected branches avoid executing intentional dead traps/huge allocations.

Keeper's minimum Linux-CG sequence: build and run `typed_gc_eh_stack.cc` with the same fresh source and paired C++ provider; compile named modules including the new module; encode/validate all seven WATs using official Core3 tools (invalid cases must be preserved as invalid fixtures); run the positive in interpreter full/lazy and LLVM full/lazy, and assert all six negatives reject with their GC/function-reference/exception gates enabled. Re-run feature-disabled controls before stack effects. Reuse existing compilation/assembly evidence machinery to compare the positive's GC lowering and interpreter musttail/ring output; no local native execution is authorized. Before merging, combine these precise hunks with the before-init, memory, and SIMD owners' separately reviewed candidates rather than copying whole shared headers.

The R4 matcher descendant also removes the pure/interpreter/LLVM empty-context
policy split. When GC is disabled, plain `0x60` function signatures use the legacy
parser and may retain no composite records. All three adapters now call
`core3_value_type_matches_with_context`: genuine nonempty contexts use canonical
subtyping, abstract-only pairs use the same Core 3 hierarchy even for a null runtime
context, and concrete function heaps preserve the existing structural fallback.
This is a compiler semantic decision; no initializer, runtime bridge, memory access,
ordinary generated IR, or native EH personality is changed.

The additional called positive `called_noexn_without_gc_valid.wat` must be encoded
and officially validated under Core 3, then run with GC disabled and only exception
and function-reference gates enabled in all four full/lazy modes. Its untaken but
statically reachable branch checks `noexn <: exn`; `_start` actually calls the callee.
The additional `known_noextern_throw_without_gc_invalid.wat` must be encoded using
the official tool's invalid-fixture option and rejected by official validation and
all products. Do not mark the negative as officially valid. Exception and
function-reference disabled controls must also reject the positive before stack
mutation. The component separately compares null/empty section adapters against
explicit positive/negative abstract pairs and preserves concrete legacy-function
structural matching. Native execution remains pending.

Primary matching rules: [Core 3 heap and reference type matching](https://webassembly.github.io/spec/core/valid/matching.html).
Confirmed source trigger: `wasm1p1/features/types.h` leaves the complete-type parser
on the legacy path when GC is disabled and signatures contain no GC prefix;
`initializer/init.h` publishes a null runtime `core3_context_ptr` for that empty
metadata. This is independent of whether exception reference instructions are enabled.
