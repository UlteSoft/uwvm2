# Pending LLVM loaded-range retirement regression (R49)

pending_range_retirement.cc uses real MCJIT engines and real synchronous
loaded-object callbacks. Two engines define the same four analogous entry names
(typed/raw and typed/raw continuation entries). Each engine accepts only its own
exact observed function entries; zero, interior and other-engine entries refuse.
The component's four ordinary LLVM functions are not generated Wasm continuations.

The retirement fault is injected with the actual callback's ObjectKey and
LoadedObjectInfo, before or after collection. The same real load notification is
redelivered while those borrowed objects remain alive. This tests whole-candidate
revocation and replay refusal without manufacturing a code range or world token.

With the original repository-specific header and
UWVM_TEST_RETIRED_RANGE_BASELINE, both notification orders reproduce stale-range
acceptance and publication. With the fixed header, both orders leave an empty
failed candidate, reject all four entries, publish no text/function extent and
return invalid diagnostic provenance. A revoked candidate cannot be reenabled.

The implementation shares revoke_observation() with allocation-failure rollback.
It clears pending text/function and conditional explicit ELF extents, invalidates
diagnostic rows, and retains a sticky failure flag. Commit detaches the listener
before returning and never publishes a failed candidate.

Both repository variants were compiled and run on SSH Linux x86_64 in the
original guarded 64 GiB cgroup against the existing full LLVM SDK. FastIO performs
test output; LLVM EngineBuilder requires its own std::string error argument.
No VM or host code/stack/memory becomes available through an ASM debugger API.

This is a real loaded-object component with an injected freeing notification.
It does not demonstrate an actual OS code unload, compilation/execution of the
private staged Wasm world, complete-instance restore, external I/O rollback,
named-module builds, or nonnative platform qualification. The earlier source
proposal's unrun world-integration cases remain distinct from this regression.
