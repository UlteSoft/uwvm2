# Core 3 fused validation audit — 2026-10-02

This is a source audit and a remote regression recipe. It is not a claim of complete Core 3 conformance or a local execution result. Production edits belong to the main developer; this audit added only synchronized test fixtures, runner cases, and this document.

## Authoritative rules

The current [Core 3 validation algorithm](https://webassembly.github.io/spec/core/appendix/algorithm.html) describes a single instruction pass suitable for integration with decoding. Its polymorphic stack still tracks subsequently pushed values: a missing operand yields Bot, untyped select can materialize Bot, and Bot matches each required value type. A known wrong-width integer remains invalid after unreachable. Local initialization is tracked separately and restored when a control frame is popped.

The [Core 3 instruction rules](https://webassembly.github.io/spec/core/valid/instructions.html) assign memory.grow the selected memory's address type. Bulk copy length uses the smaller of the source and destination address widths; memory.init retains i32 source offset and count.

## Concrete discrepancy and regression

Before the main developer's r10 fix, LLVM's memory.grow check compared a materialized Unknown slot's legacy carrier directly with the address carrier. Its typed bulk helper made the same comparison for memory.init/copy/fill. Pure validation and the interpreter instead used the shared reference_carrier_matches policy, which accepts whole-value Bot and rejects known numeric mismatches. Both LLVM full and lazy compile through the affected walker.

Relevant source paths:

- LLVM: compile_all_from_uwvm/translate/opcode/memory_cases.h, memory.grow; opcode/wasm1p1_cases.h, validate_typed_bulk_operands.
- LLVM: opcode/variable_cases.h, untyped select; single_func.h, try_pop_concrete_operand.
- Pure: validation/standard/wasm3/validator.h, memory.grow and validate_storage_operand.
- Interpreter: compile_all_from_uwvm/translate/opcode/memory_cases.h, memory.grow; opcode/wasm1p1_cases.h, pop_expected_operands.
- Shared: validation/standard/wasm3/reference_policy.h, reference_carrier_matches.

The main developer has now replaced the two LLVM comparisons with that existing shared policy. No additional validation pass or runtime memory-access check was introduced by this change. Execution confirmation remains the Linux keeper's responsibility.

Eight fixtures are registered in run_wasm3_fused_validation.py:

| Operation | Legal case | Invalid control |
| --- | --- | --- |
| memory.grow | memory64-polymorphic-grow-valid | memory64-polymorphic-grow-wrong-i32 |
| memory.copy | memory64-polymorphic-copy-valid | memory64-polymorphic-copy-wrong-i32 |
| memory.fill | memory64-polymorphic-fill-valid | memory64-polymorphic-fill-wrong-i32 |
| memory.init | memory64-polymorphic-init-valid | memory64-polymorphic-init-wrong-i32 |

Each probe has an i32 condition. _start calls it with zero; the body therefore undergoes real lazy compilation, while its unreachable branch is not executed. The legal branch materializes Bot using select. The invalid branch pushes a known i32 into the required i64 slot. Tests explicitly enable memory64 instead of inheriting the runner's default memory64-disabled policy.

Expected outcomes: official wasm-tools validation agrees with the legal/invalid classification; pure validation and every requested product mode succeed for the legal cases and report code validation errors for the controls. Parser, CLI, materialization, and unrelated runtime failures are distinct outcomes and cannot count as valid rejection. Run only in the remote 64 GiB cgroup, using --only-case for each of the eight names. Compare immutable r9 first, then freshly built r10.

## Entry and shared-policy checks

- Interpreter full and lazy converge on compile_all_from_uwvm_local_func and its instruction validation/emission dispatch. The canonical whole-module helper in that header has no normal compilation call site.
- LLVM full and lazy converge on compile_all_from_uwvm_local_func, which invokes validate_runtime_local_func while emitting private IR. Explicit validation-only APIs, debug replacement checks, and a pending numeric partial-fragment refusal retain their separate safety checks.
- Lazy structured execution-unit indexing and LLVM direct/unwind callee discovery can still scan opcodes before compilation. These are metadata scans; the source audit does not assert that bytes are decoded only once.
- Pure, interpreter, and LLVM GC stack adapters invoke validate_core3_gc_instruction and the shared cast/branch-cast rules. They each explicitly admit Unknown in operand matching.
- Nondefaultable locals share core3_local_initialization. Each walker initializes after a checked local.set/tee and restores the frame checkpoint at else/end; entering try_table saves a checkpoint too.
- memory64/table64 declarations use the shared declaration gates. Address-width selection follows the chosen imported/local memory or table. Reviewed table operands and atomic operands already preserve Unknown; no additional direct Unknown omission was found there.
- Every one of the 26 local Wasm3 header components is included by impl.h and exported by impl.cppm. No wasm1p1 or wasm2 pure-validator source was changed by this audit.

These observations do not replace exhaustive opcode conformance tests. Remaining useful interaction coverage includes mixed memory32/memory64 copy lengths, mixed table32/table64 copy lengths, typed GC element/data operations, large unreachable array.new_fixed counts, and nondefaultable local initialization around exceptional branches. Existing direct/dynamic function subtype and nominal-recursion-group cases should remain in the full suite.

## Actual remote r10 result

The keeper first reproduced the four valid-case refusals on immutable r9:
ordinary JIT full/lazy/lazy+verification supplied 12 wrong refusals and ROS
full supplied four. Pure validation and interpreter entries passed, and all
known-i32 negative controls were rejected correctly. Official wasm-tools and
Wasmtime 49 outcomes were checked on the same eight binaries.

Fresh r10 runtime and CLI builds then passed all eight priority cases and the
105-case full corpus: ordinary 1,029 checks (819 product entries), ROS 561
checks (351 product entries). The 17 cross-module cases also passed 229/127
checks. Runtime/CLI were independently rebuilt with O3 in the exact 64 GiB,
swap-zero cgroup; source identities were unchanged before/after each build.
The [mirrored build and test evidence](../../build/wasm3-evidence/fused-inline-20261002-r10-compact/mirror-manifest.json)
contains actual argv, source manifests, binary identities, artifacts and rows.
This closes the named numeric-Bot discrepancy, not every Core 3 interaction.
