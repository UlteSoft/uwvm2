# Current v7 Core 3 validation identity fixture

The current source uses the actual raw builder/parser, canonical context encoder,
metadata comparator and path digest in memory. Old/current ABI contexts differ
only by the canonical `wasm-core3-validation=fused-rich-call-reference-v1` field.
Both inert DATA roundtrips must succeed and the old context must be rejected by
the independent current DATA comparison. Computed paths must differ. No cache
object is published, loaded by the production loader or executed in this test.

The public fixture seed, raw Ed25519 signature and any synthetic image digest are
DATA, not kernel-origin authentication. Signed DATA coverage requires the actual
signing provider; unsigned DATA coverage is reported separately. Calls to the
real persistent sync/async/load APIs clear the weak origin and must refuse without
publishing anything. They do not mint a Runtime ticket or alter dumpability.

Expected current marker contains `CORE3_VALIDATION_CACHE codec_data_only=1
standalone_persistent_refused=1 native_cache_replay=0 previous_context_rejected=1
fresh_roundtrip=1 signed=0` (or `signed=1` for raw fixture signing). A successful
codec result cannot qualify persistent replay. Actual production CLI cold/store,
authenticated warm/hit, same-version different executable SHA separation and
corruption recovery are independent mandatory qualification gates.

All actual imports, compilation, generation and execution must occur on SSH Linux
inside the existing 64GiB/swap0 cgroup. This current Source migration has not been
compiled or run. Module/header, actual CLI, DBG, checkpoint, cross-architecture and
performance results remain pending until separately recorded.

The historical v5 notes below remain exact evidence of the old authored test,
not the current store/loader contract or a reclassified old successful receipt.

---

## Historical v5 authored fixture description (retained verbatim)

# Core 3 call-reference validation cache revision

This fixture checks the actual binary metadata and cache store/loader after the
shared rich-first `call_ref` reference matcher change. It holds the logical key,
source/version ID, product, target, LLVM, Wasm and codegen fields fixed, removes
only the new `wasm-core3-validation=fused-rich-call-reference-v1` pair to model
the previous revision, and checks distinct paths plus independent loader refusal
when a legitimate old blob is copied to the new filename. It also checks a new
exact-revision round trip. Files use `fast_io::native_file_loader` and
`fast_io::native_file`, and normal RAII scope exit after synchronous complete writing.

Build/run separately in both products. The sole argument is a fresh private
cache directory created by the SSH Linux keeper. Signed coverage is reported
only when the actual build has its signing provider. The old and new contexts
retain the same per-user/ISA/codegen signature seed; the signed canonical context
metadata and actual loader comparison still reject the old validation revision. An unsigned test receipt
is distinct and does not qualify CLI native-cache trust. No Mac run is authorized.
No fixture may reuse an old binary to claim the new identity passed.

Expected stdout ends with:
`CORE3_VALIDATION_CACHE previous_context_rejected=1 fresh_roundtrip=1 signed=0`
or `signed=1` for real signing support. Native execution, header-first/module
compilation, product cache warm/cold behavior, instruction/unwind and checkpoint
cross-revision restore checks remain pending until separately recorded.

Source admission order remains fused: full IR and each demanded lazy function
are validated and translated before native cache lookup. This revision does not
add another body pass and does not fix plain lazy admission of unused invalid
bodies, which remains separate unfinished work. LLVM full/parallel-full and lazy
single/group/tiered use the same `default_cache_context` ABI fingerprint. The
checkpoint materialization identity hashes actual `uwvm_abi` metadata, so its
compilation context changes together with this revision; this fixture does not
claim executable checkpoint restore coverage.
