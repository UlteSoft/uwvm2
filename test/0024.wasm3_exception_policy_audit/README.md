# Exception declaration policy before/after fixture

This is an independent **source-only, not yet executed** regression component.
It parses and initializes the original oracle-produced bytes with all features
enabled, then changes only `disable_exceptions` in the validator/compiler policy.
GC remains enabled, isolating this case from the separate `requires_gc` metadata
false-positive repair. It does not edit retained types, re-parse under a different
policy, or substitute a synthetic source module for the finalized runtime.

The intended rejection covers declared syntax even when the empty `_start`
never uses the declaration. The source prediction is that current table/global/
element and unused type/import cases will be accepted incorrectly. Existing
signature/local-carrier checks should already reject the zero-count and unused
nonnull local controls. These predictions are **not actual product results**.

`plan.json` defines 27 independent WAT consumers and their required diagnostic
subject. `fixtures/provider.wat` supplies real compatible table/global/function
exports for the import cases. `binary_zero_locals.json` holds four complete
original modules with zero-count exn/noexn local runs. Validate those exact
bytes with the official oracle; WAT print/parse can erase a zero-count run and
thereby erase the declaration whose gate is under test.

Run the component first with int support only, then with the existing LLVM test
build's `UWVM2TEST_RUNNER_USE_LLVM_JIT`. It exercises pure Core3, the runtime-policy
facade, int full, actual int lazy materialization where available, LLVM full
with IR verification, and actual LLVM lazy materialization where available.
ROS excludes the unavailable lazy headers. Every consumer owns one empty or
nonthrowing `_start` function; all compile units are explicitly materialized so
the test does not count an unused lazy body as validated.

The CLI takes triples `accept|reject`, `none|table|global|element|function|local`,
`actual.oracle.wasm`. For the import batch, prefix
`--provider actual.oracle.provider.wasm`; its registered name is
`eh-policy-provider`. Use only `reject table` for `table_exn`, for example.
The process prints `EH_POLICY` rows for EH enabled and disabled. A before-fix
acceptance where rejection is required increments `normative_mismatches` and
returns nonzero. An allocation, native or unrelated LLVM error is rethrown and
must not be counted as feature-policy rejection.

The source uses `fast_io::native_file_loader` for RAII loading and
`fast_io::print` / `mnp::dec` for diagnostics. Inputs are bounded to 64 KiB.
Run native builds and tests only through the managed SSH Linux test keeper's
shared 64 GiB cgroup. No local native run or independent SSH lane is authorized
for this auditing agent. The CPP harness was adapted from the separate GC
two-phase source fixture; its successful native compilation is still pending.

Official references consulted:

- [Core 3 heap and reference types](https://webassembly.github.io/spec/core/syntax/types.html#heap-types):
  exn/noexn belong to the exception hierarchy, independently of GC; a defined
  function/aggregate heap is not an exception heap.
- [Binary reference types](https://webassembly.github.io/spec/core/binary/types.html#reference-types):
  shorthand and explicit nullable/nonnull spellings denote the same hierarchy.
- [Module declarations and element validation](https://webassembly.github.io/spec/core/valid/modules.html#element-segments):
  empty segments still carry a declared reference type.
- [Official modern EH test](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/exceptions/throw_ref.wast)
  and [official local initialization test](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/local_init.wast).

Current source chain: parser `wasm1p1/features/types.h` retains the exact table/
global/element heap and only aggregates `requires_function_references`; initializer
`init.h` copies those aggregate bits into runtime module ownership; shared
`wasm3_runtime_validation_module.inc` copies them into the borrowed validation
facade. Shared `wasm3/declaration_policy.h` checks tags and the current function/
local carriers, but receives no exception requirement for the unused declarations.
The exact type information survives; the missing gate consumes too little of it.

`exception_policy_proposal.h.txt` is a review-only proposal and is not included
or applied anywhere. It follows the GC owner's aggregate-flag design, adds no
second body pass or fully-enabled declaration walk, and leaves old pure
wasm1p1/wasm2 validators unchanged. Root must record actual before observations
before reviewing/applying any production repair.
