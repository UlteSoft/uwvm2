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
