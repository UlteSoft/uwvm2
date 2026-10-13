# LLVM full checkpoint materialization, native producer revision 1

This is an incremental executed **entry-state recording producer**, not accepted
whole-instance checkpoint restoration. `-m debug-jit` alone remains observational;
only a trusted native manager can select the separate owned checkpoint policy
before compilation. No save/restore, reverse or deterministic-replay command is
made available by these changes.

## Engine identity and default cost

`compilation_profile` is independently immutable and retained by each actual
full `ExecutionEngine` publication and sealed function plan. Runtime selection
is full compile plus LLVM-only plus an existing actual debug session. The setter
rejects native reentry and any prior compiled/publication/bridge state. Reset
retires the policy only after the existing genuine execution drain. The profile
itself is data, not authorization or a pause ticket.

Default `nullptr` generates no checkpoint alloca, symbol, call, store, load or
runtime policy check. Existing debug activation hooks continue to be absent from
ordinary full; observational debug-full also retains its original bridge code
and gains no checkpoint TLS/branch. Only the checkpoint policy selects separate
activation entry/leave control bridges at compile time. The checkpoint function attribute, full typed entry packet and
materialization bridge exist only in explicitly selected checkpoint engines.
The cache key contains the nine-field canonical fast_io numeric tuple: domain
magic, policy revision 2, database schema 3, logical continuation revision 1,
16-byte native slot width, and four resource limits. No pointer, mutable enable
bit or borrowed policy lifetime substitutes for that key.

## Exact entry metadata and native packet

The existing single validation/emission traversal copies exact Core3 local and
result declarations into a compiler builder before any opcode consumes operands.
This is neither a prevalidation/body-prescan nor a reconstruction from LLVM
carrier integers. Final sealing occurs only after the same validator succeeds
and the emitted module remains present. Sealing owns an independent copy and
validates source offsets, dense site IDs, resource bounds and exact initialized
slot declarations once.

A true function entry has local values, no operand values, and the implicit
function control frame. Parameters are initialized. Numeric/vector and nullable
reference declared locals have their specified default; a nondefaultable local
has an exact type and explicit uninitialized state. Its native alloca is NEVER
loaded by this producer. The entry packet owns one zero-padded 16-byte slot per
local, without the 256-slot observational display limit. Initialized values are
copied with their native LLVM type, including IEEE NaN bits, v128 and the entire
opaque reference carrier. This private native ABI representation is never wire
format and cannot be serialized as a host address. Actual GC membership,
canonical owner pins and typed graph-ID relocation remain mandatory for export.

This differs from legacy debugger `availability` (readability under the fused
validator's conservative initialization context). The latter can become false
at a control merge after a physical assignment. It is not actual executed local
initialization state and MUST NOT be reused for executable restoration.

## Executed local initialization data

`executed_local_initialization` owns bounded flags using the actual module's
parameter count and the independently sealed entry declaration. It rejects an
unset parameter, an initialized nondefaultable declared local at entry, or a
parameter count outside the complete original-index local extent. This count
and this component are data, never manager/capture permissions. The executed
assignment operation changes only the selected original slot; validator
block/else/end proof rollback cannot erase a physical assignment. A genuine new
activation/self-tail reset restores parameter/defaultable initialization and
clears nondefaultable declared locals. Detached replacement prevalidates every
canonical flag and cannot clear a parameter/defaultable local; any rejection
leaves the prior vector intact. There is no native producer or restore dispatcher
for these dynamic flags yet.

`dynamic_native_packet` prevalidates the complete original-index canonical flag
range before reading any value payload. A statically proven-readable local
cannot be marked unset. A nondefaultable local whose static proof is false may
carry a genuine assigned value if the actual executed flag is true; an unset
slot must remain all-zero without becoming a fabricated null. Exact numeric,
NaN and v128 bits are retained. Parser `ref.func` indices, unknown carrier kinds,
nonnull nulls and incompatible known abstract heap envelopes are rejected.
Defined-type subtyping, host-versus-wrapper identity and actual GC membership
still require the canonical runtime/type/store owners. This bounded data helper
does not infer them from opaque bits and never dereferences a reference token.
Only one detached owned packet is swapped into the result after every slot
succeeds; invalid flags, source ranges, tags or slot data preserve the earlier
packet. Its code is not connected to a generated native flag producer or a
resume dispatcher yet.

The next independent compiler helpers are
`single_func_checkpoint_initialization_emit.h` and
`single_func_checkpoint_packet_emit.h`; shared opcode/runtime selection is not
applied yet. The first emits actual original-index initialization flags only for
an explicitly selected profile, records assignment after the real value store,
and resets only when a genuine new activation installs new parameters/defaults.
The second prevalidates the actual builder site identity, exact logical layout,
native array/local alloca function owners and complete nonlocal SSA carriers
before constructing packet IR. Locals, actual operand values and saved control
parameters retain their independently validated Core3 types. LLVM carrier
equality checks ABI only and never reconstructs semantic heap/nullability types.

An uninitialized nondefaultable local is protected by a real conditional IR
edge: only its executed flag equal to one reaches the local alloca load. The
unset edge retains an all-zero native slot; loading an uninitialized value and
selecting afterward is prohibited. All original local flags are copied without
index compression, and the native consumer prechecks every canonical marker
before reading any payload. `llvm_jit_checkpoint_dynamic_packet_ir.cc` is a real
LLVM MCJIT component candidate covering assigned/unset i31 after a control merge,
IEEE NaN payloads, complete v128, typed operands and saved control parameters.
It supplies no actual Wasm continuation/root/host management authority. Native
execution and generated IR/assembly acceptance are pending on the current
matching LLVM source/provider cut; portable codec endian qualification cannot
be borrowed for this private native ABI helper.

The separation follows [Core3 function invocation and local assignment](https://webassembly.github.io/spec/core/exec/instructions.html): incoming parameters form the new frame's prefix,
declared locals receive their specified defaults, and an actually executed
`local.set` updates that frame's selected local. Compiler-emitted dynamic flag
stores must occur after the corresponding real native value store and only on
the selected execution edge. These flags cannot be inferred from DWARF display
availability, optimized LLVM values or another speculative branch.

## Actual runtime authority

An opt-in outer native execution scope owns the shadow ledger; other modes do
not allocate it. The existing genuine debug activation entry mints incarnation,
parent, continuation and runtime epoch. Before checkpoint native payload reading,
the runtime checks the live thread participant, canonical record/source registry
membership, actual full engine/context, exact profile pointer AND control-block
ownership, sealed local plan, module/function identity, function generation and
runtime epoch. Public numerical labels, source rows, native PC/SP/GPR, shared_ptr
aliases or a file checksum cannot mint this authority.

The native producer's complete range is synchronously borrowed only after its
immutable slot count, pointer extent and PTRDIFF_MAX bound are checked. Values
are built detached and published with one vector swap after complete validation.
Unknown native reference tags and parser-only `ref.func` indices are rejected.
References are copied into constructed carriers; no pointed-to object is read.
Allocation failure poisons recording explicitly without fabricating a snapshot.
The exact activation leave hook retires shadow state before genuine musttail
(and on frame-exiting native exception cleanup); nothing is inserted between
musttail and its required immediate return.

A cold scalar observation is possible only inside the actual owned before-park
callback, with the genuine current location and activation. It reports the last
recorded site and `at_current_opcode`. A retained entry site at a later opcode
has that flag false. It returns no values, host pointers or pause capability;
`executable_restore_available` is false until real continuation/GC/effects
qualification exists. This report is not a full world-stop census.

## Remaining executable state obligations

Entry revision 1 records actual native entry state. It does not implement all
subsequent opcode, PHI/else/end, branch/loop, call/return, typed-tail, handler or
exception continuations, so a parent without an actual awaiting-return layout
is explicitly unmaterialized at child entry. It does not pretend to restore a
caller from old entry locals, optimized source values or native stack/register
blobs. Debug-full GC is currently rejected by the existing production
`debug_reader` policy; removing that rejection requires the complete actual root
population, including retained historical references, current compiler native
frames, all globals/tables/segments, native exception roots and every participant.

`visit_recorded_native_references` is a cold typed visitor only, not registration.
Its copied native carriers must be independently retained as actual owned roots
BEFORE the first collector can reclaim them and until the real collection ticket
ends. Heap-backed history cannot be inserted into the compiler's strict-LIFO
scoped-root chain. An unavailable/poisoned population must reject collection.

True restore must allocate a detached typed instance graph, relocate cycles and
aliases to fresh tokens, rebuild locals/operand/control/EH/call continuations and
use a real generated resume dispatcher, then commit the new instance generation
atomically. Old native provenance rows/captures must be invalidated, never rebound.
All active guest threads require one coherent private stop; native/foreign host
operations must have closed admission and drained borrowing WITHOUT cancelling
parked guest leases. `execution_domain.stop_and_drain()` cancels executions and
is not that checkpoint transaction. External imports need actual versioned
virtualization/record-replay adapters; unknown imports remain explicitly
nonreplayable. The control-assets registry must prove closed admission plus a
complete FD/preopen/hardlink/ancestor/plugin census before native-owned storage
can publish. Mapped input additionally needs actual immutable/sealed backing;
`MAP_PRIVATE`, a checksum and one `fstat` are insufficient against truncation.

## Primary specification and acceptance

[Core3 runtime configurations](https://webassembly.github.io/spec/core/exec/runtime.html)
and [reference typing](https://webassembly.github.io/spec/core/exec/values.html#valid-ref)
define typed values, reference identity, store, frames and continuations. Native
register/save-area layout is not part of that portable state. [QEMU replay](https://www.qemu.org/docs/master/system/replay.html)
and the [rr paper](https://arxiv.org/abs/1705.05937) require recording relevant
nondeterministic inputs and event order, not a software instruction trace alone.
[CRIU external FD restoration](https://criu.org/Inheriting_FDs_on_restore) shows
why filesystem/socket objects require an explicit external-resource contract.

`debug_checkpoint_shadow_ledger.cc` qualifies only metadata/ledger components.
`debug_checkpoint_entry_runtime.cc` must be built with entirely fresh compiler,
runtime and host/main TUs, then executed on the actual new Core3
`debug_nondefaultable_local_availability.wat` under both instruction and unwind
strategies. LLVM IR must show no unset-local load, complete opt-in typed slot
stores and no checkpoint IR in the default mode; genuine function result 42
must survive the first actual parked entry. These native checks are pending for
this revision. Codec R4 has separately passed actual native little-endian tests and four
fresh s390x big-endian QEMU binaries (both products, EH and no-EH). All five
executions produced the exact independently generated 4347-byte canonical
golden SHA256 `731ddd5ee0febc08055ac4f19faf34fd63af7f0ea9690b658622c5dabee9ff01`.
These are codec/schema component checks, not native continuation or whole-VM
restoration checks. No complete VM restore/reverse/replay acceptance has
been obtained.
