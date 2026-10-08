# Actual WASIp1 environment native capsule fixture

The new `debug_wasip1_environment_capsule_runtime.cc` uses the existing official-modern-syntax fixture `fixtures/debug_wasip1_environment.wat`. It retains the real original environment fixture's owned-source-before-parse, GC root, before-park producer, original FD renumber operations, genuine capture roster, host closure, N exclusion and later guest WASIp1 imports. It does not fabricate a capture or resource owner.

Native management ABI: `llvm_jit_checkpoint_capture_wasip1_environment_host_api(ticket, complete_canonical_captures, request)` is opt-in LLVM JIT full only. The returned opaque native capsule may retain original owned FD/dir/socket RC resources and typed FastIO duplicates of observer resources. `llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api` authenticates registry pointer and control block before copying its bounded metadata. This is same-process lifetime retention, **not** persisted descriptor restoration, kernel offset/flags rollback, external-effect replay, or the new-world issuer. Flag3 code-binding DATA grants none of those rights.

The finite fixture checks actual owned argv/env, original native stdio ownership and exact capsule duplicate count, actual original Wasm/builtin interface digests, sorted renumbered descriptors, invalid module/label/quotas, different shared control-block refusal, immutable historical text after real environment edits,16-slot registry exhaustion/release, monotonic issuer serials, stale episode refusal, and metadata lifetime after real VM reset. It does not inspect any arbitrary host/native pointer or issue new VM debugging rights.

Keeper recipe after ROOT integrates an immutable source cut:

1. Fresh-build the standalone runtime fixture against that exact cut's full runtime/main/static objects and the same explicit source/build binding recipe as `debug_wasip1_environment_runtime.cc`. Do not reuse a pre-change runtime object.
2. Use official `wasm-tools parse fixtures/debug_wasip1_environment.wat -o fixture.wasm`, then `wasm-tools validate fixture.wasm --features all` in the same sandbox. Modern GC/nondefault locals are required; old-MVP-only parsing is not qualification.
3. Fresh-run `debug_wasip1_environment_capsule_runtime fixture.wasm instruction` and independently `... fixture.wasm unwind`, both products. Existing20-second deadline bounds the real park, and the guard/watchdog must bound the whole binary.
4. Windows/BSD/other architecture native compilation must qualify the typed FastIO observer duplication branch. No actual typed duplicate or directory lifetime is assumed from another platform's success. Public DATA carries no integer-native-handle fallback.

Exact stdout must contain `debug_wasip1_environment_capsule_runtime PASS actual closed-host native capsule capture and immutable DATA; no resource restore/replay claim`; exit0. Existing original WASIp1 implementations and ordinary generated guest IR remain unchanged. Linux memory/guard receipt and physical endianness remain recorded by the keeper. Directory/socket positive lifecycle and kernel state rollback are not covered by the stdio fixture; their producer sources still require independent native qualification.

Stdio oracles follow the original initializer, without changing it: on native
platforms where the original initializer uses `io_dup`, the three stdio rows
are owned files and the capsule retains their existing RC, with zero additional
observer duplicates. Its original fallback observer platforms require three
actual capsule duplicates and retain observer metadata. The CPP asserts both
that exact count and all three descriptor kinds under the same original platform
condition. Owned-preopen lifecycle remains unsupported on its original legacy
provider branch and is never counted as a native owned-resource PASS.
