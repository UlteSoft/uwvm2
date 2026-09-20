Tools

- `tools/ci/patch_llvm_libcxx_hash_memory.py`: CI helper script to work around an upstream LLVM libc++ issue in `__functional/hash.h` related to `_LIBCPP_AVAILABILITY_HAS_HASH_MEMORY`.
- `tools/wasm_opcode_counter/opcode_counter.py`: Count opcode occurrences in the Wasm code section (e.g. `i32.const` count).
- `tools/wasm_operand_stack_stats/stack_stats.py`: Count operand stack height after each opcode in the Wasm code section; reports `> threshold` vs `<= threshold`.
- `tools/wasmtime_full_backend_matrix.py`: Compare Wasmtime with `uwvm-int-full` and LLVM AOT/full over a WAT corpus.

## Wasmtime full-backend release gate

The matrix keeps compatibility mode as its default because feature-profile and
profile-delta campaigns historically used validation results to filter a mixed
corpus. Compatibility mode records every selected case as either `executed` or
`skipped`, warns when the release conditions are not met, but preserves the old
skip-related exit status. A compatibility-mode exit code of zero is not release
evidence.

Release runs must pass `--strict` and set a reviewed corpus floor. Strict mode
requires at least `--minimum-executed` cases to reach every engine for the
selected mode (Wasmtime and both UWVM backends in execute mode), and fails on
every selected-case skip unless a deterministic selection skip is explicitly
allowed. For example:

```bash
: "${EXPECTED_EXECUTED:?set the reviewed release-corpus floor}"
: "${EXPECTED_SELECTED:?set the reviewed post-limit selection count}"
python3 tools/wasmtime_full_backend_matrix.py \
  --source-root /path/to/wasmtime-tests \
  --uwvm /path/to/uwvm \
  --wasmtime /path/to/wasmtime \
  --wat2wasm /path/to/wat2wasm \
  --wasm-validate /path/to/wasm-validate \
  --wasm-objdump /path/to/wasm-objdump \
  --work-dir /path/to/matrix-results \
  --strict \
  --minimum-executed "${EXPECTED_EXECUTED}" \
  --expected-selected "${EXPECTED_SELECTED}"
```

`--allow-skip` is repeatable and accepts only deterministic profile-selection or
callable-entry selection statuses. Tool failures and timeouts remain unexpected
even if the surrounding skip status was allowlisted. An `all`-profile validation
rejection and unparseable/empty `wasm-objdump` output are never allowlistable.
Strict mode requires `--minimum-executed` to be written explicitly and is
incompatible with `--allow-reference-mismatch`; `--expected-selected` adds an
exact post-limit corpus contract. The report records include/exclude patterns,
`--max-cases`, and selection counts before and after that limit.

`--uwvm-feature-arg` accepts at most one of `--wasm-feature-mvp` and
`--wasm-feature-wasm2`; it cannot inject runtime mode or `--run` arguments.
The JSON report schema v6 preserves stdout as exact base64 plus byte count and
SHA-256, and every outcome comparison includes those original stdout bytes. It
also records per-status executed/skipped counts, case-level skip detail, the
allowlist decision, unexpected cases, and release-gate violations. Provenance
includes the `full` product profile plus chunked SHA-256/size fingerprints for
the driver and every runtime/WABT executable. Each case fingerprints its source
WAT and the newly generated Wasm.

Compilation invalidates an old case `input.wasm`, writes to a unique temporary
directory on the same filesystem, and atomically publishes only a new regular,
non-empty output. A successful `wat2wasm` exit without that output is an
unallowlistable `wat-compile-skipped`; stale output is never reused. This
driver invokes uwvm-int-full with the explicit `-Rcm full -Rcc int` selectors;
Full's `-Rint` shortcut is size-dependent auto mode and must not be labelled
full. Equal timeouts, signals, and unclassified runtime errors are operational
failures rather than matching results. The driver covers the two full backends
only; Full-build lazy and tiered profiles remain separate matrix dimensions and
are not silently treated as covered here.
