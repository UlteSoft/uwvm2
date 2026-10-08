# Next runtime integration boundary (SOURCE design; not implemented)

Current live tiered T0 INT admission and LLVM admission still each perform a
typed body walk. Existing T2 retained LLVM replay removes a later body walk; it
does not make both initial backends share one typed walk. Neither parent i32
slice nor this memory64 successor changes that current runtime behavior.

A future whole-module factory must invoke the original INT fused walker once
with synchronous physical sinks for EVERY Core3 opcode family. Original typed
transitions and accepted events issue both ring stream/fixups and per-function
LLVM SSA/context; structured control, EH, GC, SIMD, table/reference operations,
ABI-call metadata, tails and trivial-loop/matcher metadata must be typed issuer
callbacks before this can replace the two initial admission phases. This finite
integer slice cannot be selected for modules it cannot retain completely.

The genuine runtime consumer must keep the exact compile-time INT option, actual
feature policy, runtime/module/source owners, function type identity, semantic
function indices and private compiler context. It must preserve the existing
full/lazy/tiered stage and generation transactions, quota/cleanup/error handling,
shared GC/object/address ownership, shutdown joins and full-only compiler registry
refusal. Runtime loader admission may seal typed-complete with valid original
ring and unavailable LLVM, but it may not publish a half-native plan as READY.

T0 can consume the actual complete ring artifact. T2 can consume only already
verified owned LLVM modules/bitcode and target-machine metadata; merge/codegen,
ALL actual core/OSR entry definitions+CC/prototypes, finalization/resolve/native
text owner ranges/unwind registrations must precede lazy/tiered READY release.
Any unavailable SSA family must fall back to that exact retained actual ring,
never run a second pure body validator or translate the original source again.
Default full unselected compiler remains unchanged. No IR guard or lock is added
to guest memory execution. Debug/full checkpoint publication is a separate
actual generation/engine/source transaction, not granted by compiler DATA.

The remaining raw post-walk trivial matcher is deliberately NOT invoked by this
research factory. Before product selection, its cost-significant trivial/call
metadata must be emitted from the same authoritative first walk, rather than
silently restoring a raw matcher or accepting a generic-call performance loss.
Cache/GC/debug instrumentation profiles also need their original private emit
policies and exact identity; this first native slice uses default full typed ABI
without those profiles and cannot serve as their universal issuer.

Required future actual tests: all unused-body syntax failures before startup,
late lazy call and mutual recursion, modern typed call_ref/tail-call and exnref,
GC/SIMD/memory64/table64/atomics/bulk features, no-defaultable locals, dead branches
and feature-off negatives. Native lowering decline is not invalid Wasm. Measure
startup/retained IR memory and lazy materialization independently; then assembly
inspect register-ring/musttail and LLVM memory loads/stores/guard counts. Until
that complete actual producer/consumer closure is implemented and measured,
whole-tiered one-walk and performance qualifications stay false.
