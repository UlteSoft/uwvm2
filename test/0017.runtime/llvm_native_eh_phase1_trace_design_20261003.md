# Reusing the actual native EH search for original Wasm diagnostics

Status: source review and proposed seams, 2026-10-03. No production file,
provider source, runtime binding, cache key, or build option is changed by
this document. No new native build, execution, assembly inspection, or
performance result is claimed. The existing default-off GC publication
experiment and frozen native/private-leaf EH candidates remain independent.

The current native-unwind throw path takes a separate diagnostic walk and
then starts real C++ propagation. Removing the separate walk requires an
all-frame observer inside a qualified unwind provider and a construction
boundary that seals the original trace before any Wasm or host consumer can
observe the exception. A wrapper around the ordinary C++ personality is
insufficient. This proposal preserves the original uncaught diagnostic,
including exceptions first captured as an exnref and thrown again later.

## Actual path and scope

`uwvm_runtime.default.cpp::capture_runtime_exception_trace` reserves
`tls.frames.size() + llvm_jit_unwind_backtrace_storage::max_frames` compact
identities, takes `_Unwind_Backtrace`, merges actual registered native
boundaries with older logical interpreter spans, resolves each PC/FDE pair,
and calls `diagnostic_trace::make_compact`. The latter validates the symbol
owner/indices and allocates a shared immutable trace owner. The numeric and
complete-tuple throw bridges then build an owning value and use a real C++
`throw guest_exception` expression.

The reviewed current physical bound is **64**, not the older 256. It counts
physical records before Wasm filtering. Logical T0 spans can add identities
beyond that physical bound. A candidate must preserve the current capacity
overflow checks, native/logical ordering, and truthful truncation status.

"Actual escaping throw" in the existing comment describes a throw escaping
the current local branch lowering; it does not prove the exception is
uncaught. A caller can immediately catch it. Same-function, proved non-ref
catches already branch with their checked payload and do not take this
native path. The current private-leaf consumption proof is also a separate
candidate; do not report either alternative as faster real native unwind.

`llvm_jit_throw_ref_abi_bridge` intentionally retains and throws the existing
immutable value without recapturing its trace. `catch_ref` publishes an
owning exnref before ending the native catch. Reference payloads are rooted
before propagation and before catch retirement. All those steps remain.

The repeated walk and compact ownership allocations are source facts and
performance candidates. Their share of current caught-EH time has not been
established by a source-qualified hardware-counter run.

## Why the personality is not an all-frame observer

The Itanium protocol searches without restoring frames, stops at a handler,
then starts cleanup. A catch can rethrow and restart search. Clients must
not repurpose the private unwind-header fields. See the
[Itanium EH ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html).

The actual vendored LLVM implementation advances the cursor for every
physical frame but calls a personality only when `frameInfo.handler != 0`.
An observer placed solely around `__gxx_personality_v0` therefore misses
otherwise unwindable frames without a personality. The required hook is
after a successful physical step/procedure lookup and before that branch.
See the [pinned Level 1 implementation](https://raw.githubusercontent.com/llvm/llvm-project/6dfe1677ab8dffbc6ec13d53a1e0215d75147689/libunwind/src/UnwindLevel1.c).

Our compiler's `emit_runtime_local_func_llvm_jit_may_throw_call` emits a
plain `CreateCall` when there is no lexical handler, logical-frame cleanup,
GC-frame cleanup, or debugger activation cleanup. Such JIT frames must
remain visible when their real CFI is present. Giving every frame a dummy
landingpad/personality would change code generation and may impair tail
calls; it is not the proposed fix.

Do not identify these frames by `nounwind`. LLVM's `nounwind` is an exception
behavior contract, while `uwtable` separately requests unwind-table
information. A genuinely throwing path must not be falsely marked
`nounwind`; a `nounwind` function may still have unwind tables. See the
[LLVM attribute definitions](https://llvm.org/docs/LangRef.html#function-attributes).

The existing typed landingpad matches the exact C++ `guest_exception` type.
Its cold code subsequently checks the actual Wasm tag identity and ordered
handlers. Phase 1 may stop at an intermediate C++ catch whose Wasm tags do
not match; that handler calls `__cxa_rethrow`. The original upper callers
have not necessarily been visited by the first native search. LLVM also
treats Windows runtime EH as a separate model; a DWARF callback cannot be
assumed to cover SEH or MSVC funclets. See the
[LLVM EH model](https://llvm.org/docs/ExceptionHandling.html).

Capturing only at the final uncaught wrapper loses already removed lower
frames. Capturing only at a reference catch does too. Repeated function
indices cannot be deduplicated: recursive activations have distinct actual
CFAs. Neither TLS instruction frames deliberately omitted in native mode
nor an invented function list is a substitute.

## Comparison sources and what cannot be copied

The reviewed Wasmtime revision is
`3f3f222b77a198db939863d8769af6092ee547e6`. Its handler search walks the live
Wasm stack and compares actual instance/tag identity. Its trap-handler path
can select a Wasm catch before allocating the missing trap backtrace. This
is useful evidence that handler selection and diagnostics can be separated;
it does not prove that its exception object promises our immutable original
trace across later `throw_ref` and concurrent value sharing. See its
[handler search](https://raw.githubusercontent.com/bytecodealliance/wasmtime/3f3f222b77a198db939863d8769af6092ee547e6/crates/wasmtime/src/runtime/vm/throw.rs)
and [live-stack throw dispatch](https://raw.githubusercontent.com/bytecodealliance/wasmtime/3f3f222b77a198db939863d8769af6092ee547e6/crates/wasmtime/src/runtime/vm/traphandlers.rs).

The local WAVM checkout is revision
`6f871e61c6e8fc54fa5317b5d61d128d681846f3`. `Lib/Runtime/Exception.cpp`
captures an owning call stack in `throwException(type, arguments)` before
its C++ throw. This checkout's `Lib/LLVMJIT/EmitExceptions.cpp` additionally
contains a custom tag unwind header held in one thread-local object. That
local implementation is not authority to reuse a native header for our
nested throws, saved exnrefs, or concurrent throw_ref activations. Every
activation must continue to receive its own genuine provider-owned header.
This is a local-source observation, not an upstream WAVM performance claim.

## Narrow phase A: observe, retain the baseline capture

First qualify an optional provider extension without changing diagnostics
or the existing C++ guest type. Proposed name/version:
`uwvm_native_eh_search_observer_v1`. It is a host-only, default-off provider
experiment, not a guest import or a Wasm feature. There is no runtime
`dlsym` interposition, guessed provider name, or `exception_class`-only
authentication. Ordinary builds preserve the existing capture and throw
byte path.

The actual paired libcxxabi/libunwind provider owns both association points:

1. After its real `__cxa_throw` has initialized the genuine exception header,
   an opt-in registration ties that **actual header**, thrown-object address,
   exact guest typeinfo object, thread, generation, and live private capture
   session together. The host supplies its expected guest object through a
   private throwing bridge, not a guessed header offset. Registrations are
   scoped to real live activations and nested sessions form a bounded stack.
2. In `unwind_phase1`, after each successful step and procedure lookup, the
   provider reports raw PC, IP-before-instruction status, CFA, and region
   start to that authenticated session, including personality-free frames.
3. On `HANDLER_FOUND`, the observer records that actual handler frame exactly
   once. Before the provider starts phase 2, it can continue a **separate,
   provider-owned copy** of the real cursor toward older frames. The copy
   performs read-only CFI stepping and never calls their personalities,
   installs context, runs cleanup, or changes the selected handler. It
   stops after recording the proved outer native-entry CFA, or at the bound.

No client casts `_Unwind_Context` to a private cursor. Cursor-copy validity
is a provider/target contract that must be reviewed against its real
architecture implementation; bytewise copying opaque storage is not an
assumption. An implementation may instead expose a provider-internal
read-only continuation abstraction with the same verified state.

The callback writes only bounded owned integer records. No allocation,
printing, filesystem operation, signal injection, type matching, Wasm
callback, module mutation, or provider-state mutation occurs there. It is
`noexcept`, checks the bound **before** selecting a destination element,
and does not throw or request forced unwind. Foreign exceptions, forced
unwind, cancellation, and unsupported propagation continue through the
provider unchanged. The observer result cannot change a native EH reason
code or cleanup ordering.

Session registration must occur only after any potentially throwing setup
allocation completes. A nested host failure must not be mistaken for the
armed guest activation. Unregister on genuine activation destruction and
every failure/return route; bound exhaustion declines the experiment for
that throw. No dangling address remains associated after header reuse.

Phase A keeps the old full trace and therefore makes no speedup claim.
Compare normalized original Wasm identities and native/logical boundary
order against the observation; retain raw PC/CFA/region evidence for
recursion and omitted-frame checks. Do not introduce a per-call observer or
instruction stack into the JIT's normal fast path. Hooks exist on actual
throw/search paths only.

## Phase B: seal a trace before publication

Phase A alone cannot remove the current pre-throw allocation/walk: the
current `value_ref` and `diagnostic_trace_ref` factories publish immutable
objects immediately. Mutating their indices through a surviving alias in
phase 1 would violate their contract even if the next consumer happens to
run on the same thread.

A separately reviewed private `native_throw_preparation` must own all
payload roots, the actual tag root, symbol owner, capture capacity, and
unpublished storage until completion. Preallocate everything which could
fail **before** raising the real C++ activation. Its builder is distinct
from the public immutable trace and value. Before phase 2 starts, a
no-allocation finalizer merges the exact captured native frames with the
already pinned logical spans, validates every compact identity against the
canonical immutable symbol owner, and seals the trace/value. Only after
that seal may `guest_exception::instance()`, a Wasm handler, host catch,
checkpoint exporter, or debugger observe the value.

Do not add an empty public value, mutable public diagnostic owner, global
pending exception, or a wait-on-unsealed accessor. The standard typeinfo
personality must not call guest code. This construction change needs the
exception/value owner to approve its exact object lifetime and shared-owner
layout before implementation. A preallocated backing allocation with
properly constructed, final immutable objects may be usable; aliasing a
shared pointer to an object whose lifetime has not begun is not publication.

The provider completes the lower prefix using steps it already performs,
and continues above the selected C++ handler only for the missing original
diagnostic suffix. If `D` is the original bounded diagnostic depth and `H`
the actual first search prefix, baseline stepping includes approximately
`D + H` before cleanup; this candidate includes approximately `D` before
cleanup. It does **not** remove phase 2, full upper diagnostic retention,
payload rooting, genuine C++ header allocation, or native retirement. A
nearby catch with small `H` offers less walk reuse than a deep propagation.
These are work-count estimates, not timing or hardware-counter results.

The initial candidate still keeps the original complete trace for every
fresh native exception. Discarding an unseen trace after a non-ref catch is
a later, distinct proof: a C++ handler-found event alone does not establish
Wasm consumption or absence of host/debug/checkpoint observation. Existing
private-leaf proofs may already cover some such cases without this provider
extension; do not duplicate them.

The same actual header may search again after `__cxa_rethrow`. A sealed
original trace must remain unchanged. A new `throw_ref` creates a fresh
native activation associated with an already sealed value, so it must not
capture or overwrite that value's origin. Two concurrent throw_ref
activations share only the immutable value, never session buffers or native
headers. Nested new throws get independent sessions. Deduplication by
function identity, raw header address without generation, or a reused CFA
outside the same activation is invalid.

## Failure and fallback boundaries

Provider/target qualification must be established before deciding to omit
the baseline capture. Missing actual provider association, unsupported
unwind ABI, absent thread registration, invalid boundary proof, unavailable
preallocation, or observer nesting exhaustion selects the existing path
**before the throw**. A cache or compile flag alone cannot establish that
the live libcxxabi and unwinder are the paired implementation.

Once omission is selected, an observer error before phase 2 cannot merely
return an incomplete trace as success. Because the original physical stack
is still live during phase 1, a provider may perform a bounded read-only
fallback diagnostic walk into preallocated storage at that point. That
fallback must be independently proved no-allocation/no-throw and preserve
the original frame set, boundary merge, and honest truncation. It must not
change native search results. If this exact fallback is not implemented
and qualified, phase B is not ready to enable. Failing after phase 2 has
removed frames cannot be repaired by a late wrapper backtrace.

The current code-range/identity lookup uses frozen full-mode vectors or
hazard-protected immutable snapshots in other supported modes. Every
capture must hold the actual execution/code and symbol lifetime authority;
do not add a collector world-stop assumption or a writer mutex inside
search. The first phase-B scope should be qualified LLVM full with frozen
registries and the existing real outer-entry proof. Lazy/tiered and hot
replacement require their real publication/retirement snapshots before
joining that scope. A missing outer boundary may use the existing bounded
physical traversal only if the fallback is qualified; it is not permission
to omit older interpreter spans.

Signal traps retain the present diagnostic path. Do not reuse synchronous
EH sessions as signal-safe state. CET/shadow-stack, AArch64 GCS/PAC and
descriptor ABIs keep their actual provider stepping/resume machinery;
read-only observation does not pop protected stacks or synthesize returns.

The first provider experiment is the paired Itanium/DWARF path. GNU Win64
SEH, MSVC funclets, ARM EHABI, SJLJ and other unreviewed providers retain
baseline diagnostics until their own genuine implementation and remote
ABI witnesses qualify. This is an optimization eligibility boundary; it
does not restrict existing native-unwind platform support to x86_64.

## Independent allocation candidate

Before undertaking phase B, a smaller candidate can reduce compact trace
ownership allocations while retaining the exact existing backtrace. A
private exclusive fixed-capacity identity builder can avoid the separately
allocated vector on all-native short stacks and publish one owning compact
trace allocation. Actual logical-span overflow requires an owned dynamic
fallback, not dropped frames. The frozen symbol owner, lazy name cache and
public frame/diagnostic APIs must remain correct after module retirement.

Do not enlarge every trace with a zero-filled maximum array without
measuring the initialization and allocator cost. A proposed inline layout
or variable-sized owner needs proper C++ object lifetime/alignment,
checked size arithmetic, and exact single-owner destruction; no guessed
`shared_ptr` control-block layout. Benchmark this change independently from
provider observation to identify which mechanism changes time. No such
allocation change is implemented here.

## Source seams and remote acceptance

| Seam | Proposed scope and invariant |
| --- | --- |
| Vendored/patch-only libcxxabi `__cxa_throw` association | Bind the actual native object/header to the private fresh guest preparation; exact type and lifetime. No client-private-header access. |
| Vendored/patch-only libunwind `unwind_phase1` observer | Observe every successful real frame, preserve all native actions and provider errors, include no-personality frames. |
| Provider-internal handler-found continuation | Preserve original live upper suffix with an independently owned cursor; never search extra personalities or change cleanup. |
| New ordinary `.h` private bounded session/builder | Check capacity and generation before every element access; no persistent guest token or per-call instrumentation. Import/export only after owner review. |
| Exception/value preparation and seal | Fresh value remains private until a complete immutable trace is fixed; throw_ref and already published values unchanged. |
| Narrow `default.cpp` throw/capture binding | Select actual qualified provider/preparation or the current path before the throw; signal/uncaught formatting unchanged. Shared checkpoint seams excluded. |
| Environment/code-cache identity | Add a versioned key only when emitted bridges/provider contract actually differ; current defaults and frozen identities remain unchanged now. |

Required witnesses are genuine Core 3 `try_table`/`throw`/`throw_ref`/ref
catches, not only older EH syntax:

- Ordinary non-ref caller catch, same-function branch control, first ref
  catch, catch-all-ref shadowing, aliased imported tags, and intermediate
  C++ catches whose Wasm tags mismatch and rethrow.
- Recursive throwing callee and several personality-free intermediary JIT
  frames, mixed T0/native spans, host tail reentry, and physical truncation
  at/around the current bound. Exact original ordered frames are the
  oracle, not merely a nonempty stack.
- Saved exnref thrown after the original native activation and module entry
  return; older exnref carried inside a newly caught payload; concurrent
  throw_ref of one original value; nested fresh throws and reused provider
  header addresses across generations.
- Genuine foreign C++ exceptions, allocation/setup failure before session
  arming, cleanup failure policy, and forced-unwind cancellation where the
  platform supports it. They cannot be captured by Wasm catch-all or mutate
  guest trace state.
- Exact emitted CFI/FDE/LSDA and plain-call frame proof, unchanged normal
  call/musttail code and instruction-stack omission, real cache load/store
  and actual code/symbol retirement where admitted.

The keeper runs native tests/builds only in its real 64 GiB cgroup; source
packet, provider pin, loaded image, binary hash, target ABI, mode, dispatch,
UID/TID and CPU/cgroup admission remain part of evidence. Use forced real
`native-unwind`, cache controlled, actual ordinary/ROS products and the
same semantic input for paired baseline/observer/omission variants. Report
short/deep caught EH, tag-mismatch rethrow, catch_ref+later throw_ref and
uncaught quality separately. Capture-on phase A is a correctness control,
not an optimization benchmark. Real wall time and qualified VTune hardware
counter hotspots/microarchitecture evidence are both required; include
allocation/trace, actual raise/search/cleanup, rooting and retirement costs.
QEMU/Windows/macOS/BSD qualifications belong to their assigned lanes and
must not be inferred from native Linux. No local native test is requested.

## Unapplied phase A1 provider candidate

The accompanying `llvm_native_eh_phase1_observer_provider_20261003.patch`
targets the pinned upstream provider root. It is an unapplied source
candidate; the two independent `.h` leaves contain exactly the proposed
new provider API/implementation bytes. No third-party source tree or
production include was edited. The sole opt-in macro is
`UWVM_EXPERIMENTAL_NATIVE_EH_PHASE1_OBSERVER == 1`; undefined or other values
leave the original provider flow intact. This first candidate explicitly
requires its reviewed ELF/DWARF GNU-compatible scope and matching
libcxxabi; it does not enable GNU Win64 SEH or other provider branches.

A1 is narrower than the full phase-A proposal: it records only the real
**first native search prefix**, not an older-frame continuation or complete
original stack. After successful arming, the paired `__cxa_throw` supplies
its actual initialized header/object/type. A mismatching primary throw
consumes and discards the registration. `_Unwind_RaiseException` consumes
only a matching real header and moves the descriptor into its own frame;
TLS contains no active pointer during callbacks. Native generation
exhaustion/busy arm declines. The record bound is 64; extra real native
search frames continue normally and only set the observation overflow
flag. Metadata lookup failure likewise declines observation without
changing native propagation.

The finish event runs after phase 1 returns and before any phase-2 cleanup.
It clears all descriptor pointers, so rethrow never reuses this session and
native cleanup cannot retain a pointer into an expired throwing scope. A1
does not patch `__cxa_rethrow`, forced unwind, signal paths, guest value
construction, or existing trace capture. A first search returning
`_URC_NO_REASON` means the real handler was found; the separate callback
never creates a new native EH action or changes that return code.

This ABI supports one pending arm per actual thread. Nested arming declines
until the first descriptor has been consumed, and the callback contract
forbids registration/reentry. The host needs a genuine scope guard that
disarms its provider-minted generation on every setup failure, normal exit,
and unwind. Provider TLS is touched on arm before search; the actual image
must remain pinned. The callbacks use a local session pointer, not a TLS
lookup on every reported frame. Caller storage must be real preallocated
host ownership, not a borrowed guest address or unchecked provider cursor.

A1 still lacks a production host arming bridge, independently authenticated
loaded-provider binding, build integration, raw-prefix/baseline comparison
fixture, and any native qualification. Both runtimes must compile the exact
same opt-in macro and expose the companion header to libcxxabi's existing
LLVM-unwinder include path. Default-off preprocessing, C99/C++ declaration
compatibility, real ELF binding, callback no-allocation/no-lock behavior,
native cleanup and unchanged trap diagnostics all require remote evidence.
No production auto-arming or API mapping is proposed here, and these
host-only symbols must not become Wasm imports. A1 must not be promoted as
a trace-removal optimization; phase A2's qualified read-only suffix and
phase B's unpublished value/trace builder remain later designs.

## Reviewed source identities

The following are the read-only review snapshots. Runtime files can change
independently in the shared workspace; any implementation packet must bind
its own fresh source closure and must not overwrite newer owner changes.

| Ordinary source | SHA-256 |
| --- | --- |
| `src/uwvm2/runtime/lib/uwvm_runtime.default.cpp` | `aa76cf2ce5b5c79a9f5024760f82f43ee9748a435ae666a57bc5e72c7b9cf4f2` |
| `src/uwvm2/runtime/exception/value.h` | `8bd973346a0ff0ddd69f569005ff480de4b69c01b246a230b79d6cbc99c0625e` |
| `src/uwvm2/runtime/compiler/llvm_jit/native_unwind_abi.h` | `bb1f61a67fe62aababa57e255d410ba45beab78514da27c16a63729e1a42d88b` |
| `src/uwvm2/runtime/compiler/llvm_jit/native_exception_symbols.h` | `63b11ae0e36e1b0ae4f1a0f0293cd0702f9fb4c6eea2abc3bcaccb4ee891a376` |
| `src/uwvm2/runtime/compiler/llvm_jit/native_exception_landingpad.h` | `0aee7f5c8dafdcf90fe0040dbdee3c7de21d5630061152c0084e649b64ca8000` |
| `src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_exception_emit.h` | `e8b174ed038c32ed20caf337fce52b1386ec8ae35cb515811ee465610711894a` |

ROS runtime SHA-256:
`084b7ee9a65ac290d66a9b49339a74c0b9c8fe1be7fa9495875f88c1a9d324df`.
ROS exception emitter SHA-256:
`168053ee11c5228e05141fb8aad546d2467e5178ef4ce21ffa67e3d9fc03fa99`.
The four other reviewed exception/compiler headers are byte-identical.

ROS `third-parties/llvm-runtimes/upstream-manifest.json` records official
LLVM 23.1.1 / `llvmorg-23.1.1`, commit
`6dfe1677ab8dffbc6ec13d53a1e0215d75147689`. Reviewed provider bytes:

| Provider source | SHA-256 |
| --- | --- |
| `libunwind/src/UnwindLevel1.c` | `16e16fbc0c85873df1a445d7b204fb363e46c68d6a2815dc68257108b5c944c8` |
| `libunwind/src/UnwindLevel1-gcc-ext.c` | `93d2bb050caf6b4f80819b93340ae5aad91f44e633996cc7dd32dca90da05fe0` |
| `libcxxabi/src/cxa_exception.cpp` | `0acb03590a416db189c6ebef789e25233ac4bc54de92a273e7cfed4bc345f77a` |

Those source identities do not prove which unwind images are loaded by any
existing executable. The manifest's source-only compilation/execution
flags do not constitute a provider qualification. All proposal, execution,
assembly, performance, and cross-platform status remains separate.
