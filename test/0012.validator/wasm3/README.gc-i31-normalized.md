# Shared GC scalar-reference transition (private source candidate)

This slice moves only `0xfb 28..30` (`ref.i31`, `i31.get_s/u`) to one
`describe_core3_i31_instruction` signature and the already applied fixed-arity
typed stack kernel. Pure validation, interpreter full/lazy and LLVM full/lazy
consume their actual frame stack and retain rich references. Unknown value Bot
can supply a missing operand; a reified concrete reference heap Bot cannot
satisfy `ref.i31`'s i32 input. The existing GC feature gate still rejects before
decoding/stack mutation. Cast/conversion/control opcodes 20..27 are unchanged.

LLVM's existing i31 IR body is mechanically relocated into the bounded original
decoded-DATA entry. Packing, target DataLayout endianness, truncation, sign
extension and null-kind trap remain the original instructions. The typed scanner
calls it after the original dispatch provenance/debug-safepoint prologue; no raw
cursor enters the entry. The obsolete raw FB path explicitly declines 28..30.
No runtime bridge/ABI/source permission, ordinary guest poll or memory access
policy changes. This is not complete shared Core3 semantics/raw decoding.

Normative primary sources consulted 2026-10-03:
[Core3 scalar-reference validation](https://webassembly.github.io/spec/core/valid/instructions.html#scalar-reference-instructions)
and [execution](https://webassembly.github.io/spec/core/exec/instructions.html#ref-i31).
`ref.i31`: i32 to non-null i31; both gets: nullable i31 to i32, trapping on null.

No compiler/module/unit/WAT/WAST/oracle/product/native/assembly test has run.
Minimum keeper qualification uses current R11+this candidate with matching
fresh RT/main/host and C++ provider; prior products cannot qualify changed code.
Compile/run `gc_i31_transition.cc` (no LLVM closure), then compile named modules.
Officially encode/validate all five WATs: the three `known_*_invalid` must reject
official validation; valid edges and null trap must pass official validation.
Do not mislabel an invalid fixture as an officially valid module. Preserve
actual WAST outcomes rather than assuming its expected-message strings match.

Run the called positive and three negatives in ordinary validation/int full/int
lazy/LLVM full/LLVM lazy, with cache disabled and GC/function references enabled.
ROS supports validation/int full/LLVM full only, not a fabricated lazy mode.
The positive actually invokes signed, unsigned, preserved-prefix and dead-branch
callees; lazy modes must compile them. Check null fixture yields the actual
null-reference trap, not an uncaught exception/unknown backend error. GC-off must
report `--wasm-feature-enable-gc` before stack effects. Keep reference/memory ABI
and existing interpreter musttail/register-ring machine-code inspection checks.
All native execution remains solely in keeper's Linux cgroup lane.
