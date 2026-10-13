# Stage 3 candidate: private numeric leaf with unchanged native unwind

Source design, 2026-10-02. Stage 2 v2 is a separate frozen observation-only
cold candidate; it has no native result yet. This document implements no
private bridge, clone or execution permission and reports no speedup. It
proposes the smallest first executable experiment after those observations
pass real fused native tests.

## Measured reason to investigate

The exact r9 ordinary stack4096 hardware call-stack report has visible
trace-capture/Backtrace branches totaling 40.45% of sampled function-head
self weight, and Raise/Resume/cxa_throw branches totaling 50.98%. Some
41.63% of branch weight still has skipped-frame warnings. These are
sampling-path lower bounds, not stopwatch phase durations. Later source
cannot inherit r9 lambda identities. Pure hardware whole-guest counters
are a different interval and do not separate these two walks. The
[VTune receipt analysis](../../benchmark/0004.wasm3-core/VTUNE_CLI_20261002.md)
retains these distinctions and original DSO/source identities.

Actual `try_emit_runtime_local_func_llvm_jit_throw_tuple` already emits a
direct tuple branch for a first same-function non-reference handler. That
path must stay intact. The remaining cross-function native numeric bridge
copies its validated tuple, builds the normal immutable exception value,
calls `capture_runtime_exception_trace`, then `throw_value`. Native C++
raising remains a separate real operation. The narrow target is therefore
a genuine callee throw with a proved consuming handler at the selected
caller's direct-call site, not a fabricated same-function native bridge.

## Initial executable scope

Use a separate exact-1, default-off gate and default-false request, proposed
`UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF`. Do not fold it into the observer,
pending numeric ABI, six GC experiments, r11 collector or scalar setter.
Initially admit only the same closed source-owned full module as Stage 2:
actual qualified native TargetMachine/host ABI, all local functions fused
validated/emitted, no imports/memories/tables/globals, no pending dispatch,
no debug/safe-point/patchable target slots, no lazy/OSR/runtime call route.
No architecture-name whitelist substitutes for target/ABI qualification.
The ordinary public fallback remains available everywhere else.

A Stage 2 result is insufficient to execute anything. The runtime must bind
this compiler attempt to its actual immutable full-source owner, current
runtime generation, finalized object/native-code owner and complete module
publication under its existing lock/entry admission. Source validation must
be completed by the real full compiler for every function, including
unreachable instructions. A caller supplied epoch, generation integer,
bool or restored object-cache summary cannot mint this binding. Cached
objects without the actual required proof use the original path. Initially
disable candidate object-cache reuse until a versioned safe replay contract
exists; this avoids claiming an unproved optimization permit from a key.

The executable admission should be a private immutable typed owner minted
only by the real publisher after checked compiler/source/native-code
binding. It retains source/tag identities and native/unwind registrations
through active execution and retirement. The observer domain is retained
as provenance, not used instead of that admission. Failure before
publication selects the original validated public IR/code; no half-redirected
module or partial permission is published.

## Actual call-site identity needs one additional hook

Stage 2 deliberately drops CallBase pointers and retains only integers.
Those integers cannot be mapped back to calls by callee name or incidental
LLVM block order: a resolved call_ref can name the same public target, and
parallel linking reconstructs objects and contexts. Do not add a second
Wasm-body pass to recover that identity.

The smallest Stage 3 hook can attach versioned integer LLVM metadata to the
same actual call only at Stage 2's successful post-validation/emission
commit. It contains actual function index, expression offset, event ordinal
and callsite ordinal from the private attempt, and accompanies a retained
immutable witness. It is optimization input only; metadata from arbitrary
bitcode/cache is not trusted. The source-owned witness, exact final function
and exact callee/type/CC must independently agree after link. Each expected
callsite must occur exactly once; duplicates, removed or rewritten
unexpected nodes decline that edge. Attach before pre-link optimization,
then either move this selection before that optimization or decline when
the node no longer survives. Do not guess from a similarly named CallBase.

This is a Stage 3 IR change under its own gate. Stage 2's approved IR-equality
test stays unchanged. Metadata never contains a persistent raw IR address.
Native execution must not accept metadata merely because it survived
serialization. Publication binds it to the genuine new traversal and source.

## Minimal transform and bridge

After original full fusion, joins, link and successful verification, while
the publisher still owns the original unoptimized module:

1. Select only witnessed ordinary direct-call edges whose real numeric
   leaf is complete, has no outgoing calls/unreviewed effects, and has
   escaping actual tags. Every escaping instance must encounter a first
   matching non-reference catch/catch_all at that call site. Preserve
   inner-first clause order; a first matching catch_ref blocks selection
   even when an outer consuming handler also exists.
2. Create one private clone per selected leaf and version. Preserve the
   original exact typed FunctionType, argument/result ABI, calling
   convention, body, source identity and all cleanup/root/CFI behavior.
   Do not add `nounwind`. Do not expose it as a function index, export,
   table/ref.func/raw thunk, debug patch target or replaceable public entry.
3. In that private clone only, replace the exact identified numeric throw
   bridge with a versioned register-wide no-trace numeric bridge. Preserve
   module ID, tag identity/index, initialized tuple allocation, byte extent,
   field kinds/bits, trap policy and native `throw_value` ownership. A
   signature-only or loosely named bridge match is insufficient. No tuple
   reference/v128 or throw_ref route is supported initially.
4. Redirect only those exact proved ordinary call nodes to that private
   clone, leaving public leaf bodies and all unproved callers unchanged.
   Caller handlers, normal/unwind blocks, end_catch and result copying
   remain their existing instructions. Verify the whole transformed module
   again, then optimize and materialize through the original publisher.

The proposed helper symbol is
`llvm_jit_throw_numeric_private_leaf_no_trace_wide_r1`, with four
`uintptr_t` arguments and noreturn/may-throw semantics. The LLVM declaration
uses four target-intptr arguments and host CC; it must follow the existing
type/version discriminator and target-specific materialization route. No
narrow enum/bool ABI, custom C interface or new guest-controlled permission
argument is introduced. Its body retains every existing numeric bridge
check, allocation, immutable value/source-retention publication and native
throw, replacing only diagnostic trace construction with an empty trace
at this statically proved private route. Public bridges remain unchanged.

The existing trace-capture observer and root snapshots cannot be removed
globally. GC/source-retention entry leases and selective exception retirement
must behave exactly as before. A host allocation failure or a non-guest
C++ exception is not a consumed Wasm tag; it keeps the original host policy
and cleanup. The candidate must not convert such failures into success or
pretend to provide a normal Wasm diagnostic trace for them.

## Code lifetime, mapping and invalidation

Native unwind still traverses this clone. Its actual object CFI, personality,
landingpad cleanups and loaded unwind registration are mandatory. The
runtime must register the private code extent under the original logical
module/function identity and this actual generation, retaining ownership
until every active call has drained. Public function vectors remain
unchanged. Otherwise an unexpected host unwind/profile could encounter an
unmapped private PC even when the proved Wasm exception is consumed.

Debug attachment and replacement cannot silently mutate this generation's
proof. Initially decline the candidate whenever management is armed at
publication; no private address enters patchable target tables. A future
management feature would require retiring/rebuilding private edges with
new actual function/tag/source/code identities. A generation number alone
does not authenticate that transition. Thread entry remains the existing
VM-managed native entry and actual source pin; no TLS guessed caught flag
controls trace suppression.

For initial cache-disabled tests, store the exact gate/request/version,
typed bridge symbol/type, actual source/record/callsite mapping, ABI/target,
post-transform IR/object hashes and native mapping receipt. Later cache
support needs these identities plus actual symbol rebinding and loaded-code
admission. Ordinary LLVM IR hashes may cover changed bytes but do not
replace semantic replay or source/generation authority.

## Small acceptance and performance round

Use identical official-encoded bytes with Wasmtime and both products. Add
true caller/callee catch cases for two distinct same-signature tag instances,
first matching catch_ref plus outer catch, catch_all, partially consumed tag
sets, local branch-only catch, throw_ref rethrow and uncaught cross-call
diagnostics. Explicitly verify public leaf direct/export paths retain the
original colored detailed trace. Check reference/tail/indirect/helper and
unreachable effects decline; missing actual source/attempt, duplicate call
metadata, wrong final CC/type, failed module verification, host allocation
failure and native cleanup must preserve rejection/fallback semantics.

Real compiler/IR/object tests must prove only selected nodes call the private
clone, private throws call the exact no-trace bridge, public bodies still
call the ordinary bridge, and native EH/CFI stays emitted. Test supported
native ABIs, including RV64's actual pointer materialization where available;
do not assert AddSymbol declarations on a target that uses direct addresses.
On/off semantics, root/exception graph tests and sanitizers precede timing.

The first timing round is a small frequency-matched P0 ABBA using explicit
native unwind on the existing cross-call numeric catch fixture and its
plain/normal-EH controls. Record unprofiled guest/wall/user/sys/frequency,
separate exact whole-guest hardware cycles/instructions, and separate
VTune hardware Backtrace vs Raise/Resume paths. Temperature is observation
only. Pure counters do not become collector/EH-only latency; VTune elapsed
does not replace unprofiled results. Pending auto is a separate control,
never passed off as unchanged native raising. No industry ranking follows
from the initial small round.
