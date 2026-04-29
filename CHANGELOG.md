# Changelog

This document tracks externally visible release history for `uwvm2`.

Each release entry should record:

- release version and channel
- release date and lifecycle status
- distribution baseline commit
- beta-phase fixes
- general-availability or post-release fixes
- maintenance support window
- deprecation notice dates and removal targets

## Entry Conventions

- `Added`: user-visible capabilities first introduced in the release.
- `Changed`: non-breaking behavioral, packaging, or documentation updates.
- `Fixed`: defects resolved in the release cut itself.
- `Beta Fixes`: fixes made while the release remains in beta.
- `Release Fixes`: fixes made after general availability.
- `Deprecated`: items officially marked as deprecated, including notice dates and removal targets where known.

## V2.0.1.1

- Release channel: `Beta`
- Release status: `Initial public beta`
- Release date: `2026-03-30`
- Planned general availability: `2026-06-30`
- Distribution baseline commit: `TBD` when the release artifact or tag is finalized
- Current source baseline reference: `4da46fb2f84cf8d6d6cd4839b4b1401a17f655bb`
- Source baseline timestamp: `2026-03-31T22:04:01+08:00`
- Maintenance support window: `TBD`
- Deprecated notice date: `None`
- Removal target: `None`

### Added

- Initial public beta release of the UWVM2 2.x line.
- Cross-platform WebAssembly runtime foundations, including the `u2` interpreter, parser and validation pipeline, WASI Preview 1 host integration, plugin and preload module support, and multiple linear-memory backends.

### Changed

- No prior public 2.x release exists, so no release-to-release functional delta applies yet.
- Build configuration currently force-disables JIT support because no production-ready JIT backend is available in this release line.
- Contributor documentation for the register-ring header now uses clearer transport-system wording and correctly refers to the native interpreter implementation.
- CI workflow can now skip the Windows MINGW job unless it is explicitly requested, reducing unnecessary default validation runs during beta maintenance.
- Commit reference: `4da46fb2f84cf8d6d6cd4839b4b1401a17f655bb`
- `xmake` JIT configuration is restricted to `none`, and JIT is therefore unavailable for `V2.0.1.1`.

### Fixed

- None recorded for the initial beta distribution cut.

### Beta Fixes

- Corrected `uwvm_int` byref store lowering so delayed combine operands are flushed before plain `i32`/`i64`/`f32`/`f64` store and narrow-store opfuncs, preserving Wasm MVP address/value stack order when tail-call store fusions are not selected.
- Hardened strict `uwvm_int` regression coverage for byref store pending-flush paths, `br_if` i64 local-eqz fusion expectations, extra-heavy i32 sum-loop execution, and strict test helper build hygiene.
- Corrected `uwvm_int` `br_if` fallthrough typing in polymorphic Wasm MVP control-flow so single-result label values are re-materialized on the non-taken path, keeping the compiler-integrated translator aligned with the W3C stack-polymorphism algorithm and with the standalone validator/LLVM JIT validator behavior.
- Corrected `local_set` type checking in both validation and `uwvm_int` translation for polymorphic control-flow regions so values pushed after `unreachable` are still validated as concrete operands instead of bypassing type mismatch diagnostics.
- Added strict regression coverage for polymorphic `local_set` mismatch cases, including delayed combine/local-update translation paths in `uwvm_int`.
- Downgraded LLVM-only `-Wundefined-inline` diagnostics from error to warning for test targets so the Clang/LLVM toolchain can build the test suite without relaxing warning-as-error handling for runtime sources.
- Removed unintended LLVM JIT environment probing during `xmake f` configuration when JIT support is disabled.
- The hidden `llvm-jit-env` configuration path has been removed so `llvm-config` is no longer queried for `V2.0.1.1` builds.
- Renamed the build-toolchain option from `--use-llvm` to `--use-llvm-compiler` to make it explicit that the flag selects the LLVM/Clang compiler toolchain rather than enabling JIT.
- Fixed `memory.grow` to return the previous page count on success and `-1` on failure across strict and silent growth paths, including concurrent linear-memory backends where the old size must be captured inside the grow critical section.
- Added stable linear-memory access snapshots for relocation-capable backends so preload copies and local-imported memory reads and writes no longer race against concurrent `memory.grow` relocation.
- Exposed local-imported memory snapshot, read, and write helpers through the runtime bridge so host-side memory transfers use the same validated snapshot semantics as native-defined memories.
- Strengthened `with_memory_access_snapshot` synchronization so snapshot callbacks wait for in-flight memory operations to finish before entering the exclusive-access section, improving correctness for concurrent linear-memory access.
- Added exception capture and propagation for parallel compilation failures so background compiler tasks can terminate cleanly and surface stored error state deterministically.
- Hardened `br_table` validation and `uwvm_int` translation against malformed target counts by rejecting encodings whose remaining bytes cannot contain all branch labels plus the default label before reserving storage, preventing oversized allocation and resource-exhaustion paths.
- Added a dedicated `br_table_target_count_exceeds_remaining_bytes` validation error and diagnostic output so malformed `br_table` encodings now report the offending target count, remaining bytes, and computed maximum target count.
- Guarded operand-stack requirement calculations for `br_if`, `br_table`, and `call_indirect` against `size_t` overflow in both validation and `uwvm_int` translation, preserving correct stack-underflow handling for extreme arities.
- Corrected single-result `br` and `return` lowering in the `uwvm_int` translator so temporary result preservation now compares live stack depth relative to the branch target base instead of relying on an overflow-prone addition.
- Corrected `else` transition result validation in both the standard validator and `uwvm_int` translator so `if` then-branch checking now matches `end` semantics: polymorphic paths may omit required values, but still reject extra stack values and still validate concrete result types, including multi-value result signatures.
- Aligned the `uwvm_int` compiler-side validator with Wasm MVP stack-polymorphism semantics so operand underflow is now bounded by the current control-frame base instead of the whole function stack, while concrete operands pushed after `unreachable` remain fully type-checked.
- Closed a high-risk validation gap where invalid modules could pass the compiler-integrated validator path but fail the core validator, covering numeric operators, `if`, `br`/`br_if`/`br_table`, `return`, `call`/`call_indirect`, `drop`/`select`, `local.set`/`local.tee`, `global.set`, and `memory.grow`.
- Added strict regression coverage for block-base underflow, polymorphic concrete numeric mismatches, and polymorphic `br_if` condition mismatches so the standalone validator and compiler-side validator stay behaviorally aligned.
- Corrected nested `block`/`loop`/`if`/`else` control-frame validation so stack-polymorphism no longer leaks in from an outer unreachable region; newly entered frames now start reachable, matching the W3C WebAssembly MVP validation algorithm.
- Added strict regression coverage for operand underflow inside nested blocks and `else` frames entered from polymorphic outer control flow, preventing recurrence of the validator/compiler-validator mismatch in this release line.
- Corrected `br_if` validation in polymorphic Wasm MVP paths so fallthrough typing now re-materializes missing label values for single-result labels, matching the W3C stack-polymorphism algorithm and preventing false acceptance of later concrete type mismatches.
- Stopped adding `-Wno-maybe-musttail-local-addr` on Apple platforms so the strict validation test targets can build under Apple Clang without failing on an unsupported warning-suppression flag.
- Corrected `uwvm_int` `call_indirect` validation-stack bookkeeping so the table selector is no longer popped twice after validation has already consumed it, preventing false operand-stack underflow reports on later instructions in valid Wasm MVP modules.
- Added strict regression coverage for a valid `call_indirect` followed by `select`, keeping the compiler-integrated `uwvm_int` validator aligned with the W3C WebAssembly MVP stack effect for indirect calls and parametric instructions.
- Aligned `uwvm_int` native-memory `memory.grow` with the JIT/native Wasm failure model by rejecting definitely-over-limit growth requests up front and by refining interpreter-side grow limits against backend-specific native-memory maxima.
- Expanded the object-layer memory documentation with a standards-oriented explanation of uwvm2's `strict` versus fail-fast `memory.grow` policies, including Linux overcommit modes, Windows reserve/commit semantics, and BSD-family host-policy differences.
- Added upfront UTF-8 validation for WebAssembly module names accepted by `--wasm-set-preload-module-attribute`, so invalid names now fail immediately with a detailed diagnostic and usage guidance instead of being deferred to a later loader path.

### Release Fixes

- Not applicable until general availability.

### Deprecated

- No deprecations have been formally announced for this release line at this time.

### Notes

- Update `Distribution baseline commit` to the exact tagged commit used to produce release artifacts when the beta package is cut or re-cut.
- Record every externally relevant beta hotfix and post-release maintenance fix under this version entry until a newer public version supersedes it.
