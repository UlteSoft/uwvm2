# Core3 declaration gates and lazy admission audit

All files are source-only candidates; no local native or SSH run was performed.
Neither proposed production repair is included or applied. Existing 0023 and 0024
corpora are separate; this directory does not change their expectations.

`plan.json` lists 26 independent WAT consumers, one real provider, seven original
binary controls and seven lazy admission/execution WAT probes. The declaration
component parses/initializes original oracle bytes with all features enabled,
then changes only SIMD, reference-types, multi-value or function-references.
It executes actual pure Core3/runtime-facade checks, int full/actual lazy compile
units and optional LLVM full/actual lazy units. The original body/type/module
ownership remains live; no synthetic metadata substitutes for actual parse/init.

Use CLI quads `FEATURE accept|reject SUBJECT actual.oracle.wasm`, for example
`simd reject function unused_v128_type.wasm`. Imports use the real provider prefix
`--provider actual.oracle.provider.wasm`, registered as `decl-policy-provider`.
`UWVM2TEST_RUNNER_USE_LLVM_JIT` enables both actual LLVM paths. ROS excludes its
absent lazy headers. All compile units are explicitly materialized, so an unused
lazy function cannot count as validated merely because it was never compiled.

The expected before counterexamples are unused/imported v128, externref and
multiple-result signatures and unused v128/externref globals/GC fields. Controls
check MVP funcref tables, MVP multiple parameters, nullable extern/function refs,
nonnull abstract GC refs and exact defined GC heaps. Strict function-reference
metadata should already distinguish function heaps from GC heaps. Source
predictions are not actual observations and may be disproved by the native run.

`binary_controls.json` must be decoded directly from its exact hex and validated
with the official oracle. WAT normalization erases zero-count local groups and
can shorten a long `(ref null func)` table encoding to 0x70. The long 0x63 table
is important: current parser rejects it with reference-types disabled, but the
runtime flags preserve only its false function-reference requirement. Its prefix
is at absolute byte 21; zero-count local type prefixes are at absolute byte 36.

The seven `lazy/` files are separate module-admission checks, not inputs to the
feature-policy harness. Official `parse` may encode an invalid module, while
`validate` must reject its unused nondefaultable-local, GC packed-field or EH
outer-label error. Four valid fixtures actually call their new Core3 probes from
`_start`: local initialization, recursive GC, throwing/catching, memory64/table64.
Run the CLI's pure validation, int full/lazy and LLVM full/lazy entry paths; never
normalize an invalid body away or count an uncalled lazy probe as tested. For an
invalid module, admission must reject before any start/entry side effect.

`checked_lazy_plan_design.md` documents the actual extra CLI validation traversal,
its current raw-byte lazy representations, and an owned translated plan that can
remove duplicate decoding safely. Do not delete admission validation or enable
an assume flag as a shortcut. `semantic_helper_proposal.h.txt` is a narrow shared
declaration requirement classifier/checker candidate; it adds no enabled-mode
walk, second body pass or generated memory-access guard. It preserves exact
encoding provenance for legacy table/element exemptions and count-zero locals.

Native execution must go through the keeper on SSH Linux in the shared 64 GiB
cgroup. First official parse+validate and `unused_v128_type` int component before;
then imports/binary controls, LLVM IR checks and CLI lazy admission matrix. All
formatting/loading in the C++ component uses fast_io, including explicit
`os_c_str` wrappers for runtime file/mode/feature labels. Before false acceptance
is a failing regression (`normative_mismatches`), never qualification PASS.

Official primary sources inspected at Core3 2026-10-02:

- [Binary heap/reference/value types](https://webassembly.github.io/spec/core/binary/types.html#reference-types).
- [Module/function/local validation](https://webassembly.github.io/spec/core/valid/modules.html#functions).
- [Core3 validation algorithm](https://webassembly.github.io/spec/core/appendix/algorithm.html).
- [Official local-init tests](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/local_init.wast)
  and [official SIMD constants](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/simd/simd_const.wast).

These official tests support semantics; our two-phase strict policy and original
encoding controls are independent repository API regressions, not verbatim
copies of those upstream test files. No claim of 100% conformance follows from
this source audit or these fixtures.
