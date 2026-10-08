# General GC hot paths: source hypotheses and candidate boundaries

Current update: the original generator/source model and narrow setter proposal
below are historical design receipts. The actual current general generator is
v2 `eb6048d1...`, whose explicit export-order change keeps original binaries and
scalar math unchanged. S6e's SET32 r5 uses five `uintptr_t` arguments and a
register-wide status, not the earlier u32 proposal. Its status-zero v2 and the
shared getter/generic two-phase status lowering are separately frozen source
candidates and require fresh aligned runtime/consumers; they are not in the
S6e performance baseline. See [actual results](CURRENT_GENERAL_GC_RESULTS_20261002.md)
for the completed 32 unprofiled and 8 pure hardware observations, exact counters,
internal-versus-parent timing boundaries and qualifications.

This is a read-only design supplement. It changes no frozen collector,
representation, runtime gate or JIT lowering. The precise trace metadata
candidate skips repeated field classification during collection; it does not
change ordinary allocation, token publication, mutable synchronization,
reference mutation, table lookup or cast helpers. Historical whole-workload
117 ns/step and experiment-profile roughly 9 ns/step observations cannot
establish the cost of these new general families or an isolated collector.

The archived v1 generator/checker are frozen at `23f57a8f...`/`fbc30b4f...`.
The historical `prepare_general_gc_hot_path.py` checks those full hashes and
therefore declines the live v2 generator; the command below requires the archived
v1 byte images. It compares all four checksums against the independent scalar
model and emits source-level dynamic operation formulas plus hashes of the
real lowering/helper sources. The export-only v2 change leaves those operation
formulas unchanged, but this historical helper is not a current test recipe. It never
runs a Wasm validator, VM, native program, profiler or guard. Its counts model
one successful `run`, including mutate setup and final root traversal. They
are neither actual helper calls nor emitted machine instructions: a correct
compiler may remove redundant source operations.

```sh
python3 benchmark/0004.wasm3-core/prepare_general_gc_hot_path.py --iterations 65536 --out <new-evidence-dir>/source-hot-model.json
```

This command is pure source/math preparation, not a performance plan. Bind
the same binary hashes and scalar returns to later actual keeper receipts.
The report does not invent source IDs, elapsed times or hardware counts.

## Exact workload decomposition

Let `N` be iterations, `R=1024`, `G=N` for allocation and `G=R` for mutation.
Allocation reads `O=N-R` old groups before replacement; mutation has `O=0`.
`M=O+R` complete retained readbacks include old roots and final traversal.
Loaded root groups are `L=O+R` for allocation and `L=N+R` for mutation.
Reference-array loads two tables per group; all other families load one.

| Family / phase at N=65,536 | Source allocations | Scalar gets | Reference gets | Scalar sets | Reference sets | Casts | Independent run return, u32 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| mutable-struct / allocate | 65,536 | 131,072 | 0 | 65,536 | 0 | 65,536 | 1,844,969,472 |
| mutable-struct / mutate | 1,024 | 66,560 | 0 | 65,536 | 0 | 66,560 | 2,253,127,680 |
| reference-cycle / allocate | 131,072 | 327,680 | 327,680 | 65,536 | 196,608 | 65,536 | 1,880,423,424 |
| reference-cycle / mutate | 2,048 | 198,656 | 264,192 | 65,536 | 132,096 | 66,560 | 4,032,481,792 |
| numeric-array / allocate | 65,536 | 655,360 | 0 | 131,072 | 0 | 65,536 | 3,547,855,872 |
| numeric-array / mutate | 1,024 | 139,264 | 0 | 131,072 | 0 | 66,560 | 2,136,901,120 |
| reference-array / allocate | 196,608 | 655,360 | 851,968 | 65,536 | 196,608 | 131,072 | 3,654,089,728 |
| reference-array / mutate | 3,072 | 139,264 | 272,384 | 65,536 | 132,096 | 133,120 | 4,144,679,424 |

Scalar/reference gets and sets above sum the corresponding struct and array
operations. The JSON keeps them separate. Mutation has zero main-loop
allocations; its table still has setup allocations and final readback. The
checksums come from two independently coded scalar models, not an executed
engine. The actual Wasmtime invoke result must still match modulo 2^32.

`mutable-struct` tests one mutable i32; it is ineligible for immutable compact
admission but can use the existing numeric slab. Reference-cycle creates A
and B, changes A.next to A and observes it, restores B and observes it, then
checks B.next=A. Reference-array adds an eight-element reference array and
a second table rooting A: 2,048 final table slots and 3,072 reachable objects,
not the same root traversal cost as numeric-array. Numeric/reference arrays
both change and read two distinct lanes; final traversal reads all eight.
Allocation drops intact cycles; semantic exit zero is not cycle reclamation
proof. Require actual allocation/collection/reclaimed counters and RSS.

## Actual source lowering and native work

The paths below are read from the checked-in source, not attributed to a
sampled frozen executable. Source report hashes identify the actual revision.
Names identify candidates for actual finalized-object/VTune inspection; an
inline function may not retain a separate sampled symbol.

| Operation | Current lowering/helper route | Work that remains |
| --- | --- | --- |
| i32 struct read | `try_emit_...gc_struct_get32` → `llvm_jit_gc_struct_get32_bridge` → `struct_get32` | register integer carrier extraction, real local membership/foreign fallback, actual owner/type/field, mutable `object_lock`, raw-bit status return |
| numeric or reference struct write | generic/fixed `llvm_jit_gc_aggregate_fixed_bridge<5,2>` → `struct_set` | generated initialized carrier inputs/output, native input copying, exact membership/layout/mutable checks; references additionally validate type and retain origin; original lock and field commit |
| struct creation | fixed aggregate helper, allocation-only managed helper when roots enabled → `struct_new` | precise roots/poll first, complete input validation, allocation/initialization/retention, opaque token publication |
| array creation | fixed `<6,2>` → `array_new` | full length/geometry, actual reference compatibility/lease before full fill, real publication; reference arrays retain no compact admission |
| reference array read/write | fixed `<11,2>` / `<14,3>` → `array_get` / `array_set` | full reference carrier, membership/type/length, actual mutable lock, reference compatibility and lease on writes |
| numeric array read | generic path by default; scalar get32 with `PACKED_NUMERIC_ARRAYS=1` | exact numeric representation plus bounds/type/lock remain; default and experiment6 are different paths |
| numeric array write | generic path by default and experiment6; scalar set32 only with separate `NUMERIC_ARRAY_SET32=1` | original checked mutable field and lock remain; this extra default-off gate is not one of the six |
| table root read | `llvm_jit_table_get_bridge` | actual table/debug guard, table resolution/bounds, complete carrier, checked root transfer; a local mutable/reference object cannot use the immutable compact table shortcut |
| mutable/reference cast | `llvm_jit_gc_ref_test_bridge` → `reference_type_matches` | full carrier slot, actual type/member lookup and canonical matching; immutable numeric adjacent-load witness eligibility fails for every new family |

Paths are `translate/single_func_gc_emit.h`, `single_func_gc_array_get32.inc`,
`single_func_gc_array_set32.inc`, `single_func_emit.h`, and store
`gc_object.h`/`numeric_array_{get,set}32.inc`. All paths include the prefixes
recorded in the source report. Generic helpers already have fixed opcode/count
specializations, so it is wrong to assume every operation pays a runtime switch
or allocates the native large-input array. All new family inputs fit the fixed
small range. Scratch buffer/ABI removal still differs from instruction counting
and requires the actual O3 assembly, not source-only memset counting.

`allocate` uses a numeric slab for eligible numeric structs; each reserve and
commit takes its real slab lock. Reference structs/arrays use a real byte-array
fallback; numeric arrays can use a packed tail under their separate gate.
`publish` still allocates a nonrecycling opaque ID, release-publishes the local
membership and object chains with two CAS operations, then links the global
stripe before returning the carrier. Removing malloc alone cannot remove this
publication cost. Preserve it in the first allocation candidate.

On a reference write, `value_matches` calls `reference_matches`, which resolves
the source object and checks the expected canonical heap type. Then
`retain_embedded_reference` resolves the same source again to establish its
true originating store and independent destination-object lease. Local same
owner needs no foreign hold, but lookup still occurs. This duplicated source
authentication is a concrete source hypothesis; no current general-family
profile proves its percentage. A table load followed by cast and field read
also enters several distinct checked helpers. It does not license a raw token
to become an object pointer.

## First candidate: scalar mutable struct setter ABI

Prioritize a separate default-off numeric32 setter experiment if the real
mutable-struct mutation profile is helper/ABI-heavy. This is smaller than a
new heap or exclusive execution mode. Follow the existing array_set32 model:

* LLVM selects only a fused-validated mutable i32/f32 or packed i8/i16 field and
  matching actual native pointer/endian carrier layout, before creating IR.
  Unsupported/wide/reference shapes retain the exact old generic route.
* A private named helper has five C integer arguments: bound native module,
  reference kind, original opaque payload, field index and raw u32 bits. Return
  only the u32 `gc_object_status`. f32 uses an LLVM bitcast, not a floating
  argument or operation; packed writes use the original pack/truncation rules.
* Native work still proves actual membership/kind/null, real owner/canonical
  layout, field extent, mutable eligibility, applicable compatibility/lease
  rules and the original mutation lock in the same failure order. It must use
  the actual object's type, not the compiler's expected carrier as authority.
  A reconstructed zeroed carrier can remain inside the checked leaf.
* Preserve the same managed-page boundary and sealed-cursor retirement before
  any original shared reader path. No hidden GC polling, allocation or new
  safepoint is added to the setter. This does not remove the lock or hot memory
  protection; it removes generated input/output carrier marshalling only.
* Give the exact new function an explicit semantic/versioned relocation name
  and matching `llvm::FunctionType`; update actual bridge symbol binding and
  cache/profile identity. Never reuse the generic `<5,2>` symbol for a different
  ABI. Cached objects must rebind the new helper and retain native module and
  execution-generation ownership. The helper is `noexcept`; preserve existing
  instruction/unwind diagnostics and exact trap selection on error.

Cold controls: null/stale/forged/foreign tokens, wrong kind, actual canonical
subtype, first/last/out-of-range field, immutable/ref/wide fallback, i8/i16
truncation, f32 NaN payload/signed zero, aliasing and concurrent mutable readers
and writers, module unload, cross-call/trap/unwind cleanup, cache reload and
actual full/lazy codegen. Compare original/specialized setter on identical
mutable-struct bytes; collect actual relocations/disassembly and matched
default/experiment6 profiles separately. Do not turn the extra array setter
or precise metadata gate on accidentally during this comparison.

## Second candidate: source authentication inside one reference operation

Prepare one privately constructed authenticated-source descriptor inside the
current `struct_set`/`array_set` or allocation operation. It contains the exact
input token/kind, authenticated actual header/type and true canonical owner
pin required by the original local/foreign path. Canonical subtype comparison
and destination lease retention may consume that descriptor without searching
the source membership a second time. It is neither a guest carrier nor public
API. Null/i31, defined/abstract heap, funcref, extern and exn retain their own
complete rules; begin with aggregate references only, with original fallback.

The descriptor must not cross an allocation, safepoint, callback, entry-admission
release, registry unlock that ends its proof, or source retirement. A foreign
lookup promotes the real registered owner under its stripe; keep that strong
pin through compatibility and independent object-owned lease commit. If a
foreign lease requires allocation/reentrancy or pause, retire the view and use
the original checked path before the field store. A reused canonical type ID
never substitutes for the input token's origin membership. Preserve failure
ordering/OOM and the original mutable lock; this is authentication reuse in
one operation, not caching values across Wasm instructions.

Native components should first isolate local exact-type/local-subtype versus
foreign stores, same/fresh sources and repeated writes. Check unload/stale/ABA,
OOM before commit, overwritten foreign-edge pruning and concurrency. VM
reference-cycle/reference-array allocation and mutation distinguish repeated
compatibility/lease work from allocator pressure; a main loop with no
allocations still has lookup, writes, casts and table accesses.

## Later candidate: genuinely exclusive writer window

Neither `LLVM full`, one module, no shared memory, TLS, `active_count()==1`, an
owner pointer nor one OS thread is exclusive mutation authority. Mutable
synchronization stays mandatory unless a stronger real factory proves the
complete closed interval. The existing managed-page cold entry shows the kind
of proof needed: exact canonical store/control block, live execution-generation
lease moved from actual entry, admitted shared lease and genuine exclusive
lease protecting it, actual participant/pause-domain census with only the
collecting native owner, initializer serial and same native owner identity.
Its current numeric eligibility cannot be extended by a benchmark guess.

## Version-aligned primary source and next measured hypothesis

The installed comparison's v49.0.1 source tag resolves to commit
`46c23a87dac1465986a8ad53ba6a7ae49372857b`. Its [compiler copying path](https://raw.githubusercontent.com/bytecodealliance/wasmtime/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/func_environ/gc/copying.rs)
is 17,475 bytes, SHA `a6ac6f742b68c1390d78c973b6e31809e2ad1a84366684b09e9dfd84b231b733`:
allocation loads bump/end, uses widened overflow-safe size arithmetic, branches
to a cold libcall on exhaustion, and writes initialized headers/fields on the
fast path. Relocatable references are recorded in stack maps. This source
explains a code-generation difference; it does not prove binary timing or
permit dropping our opaque-token/foreign-store checks.

The [runtime copying source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs)
is 46,826 bytes, SHA `2f014aa49e4380c586f95edbfa900a78538c41724cbc7f81193fc1327313fc29`.
It uses a moving semispace collector. The [DRC source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/wasmtime/src/runtime/vm/gc/enabled/drc.rs)
is 50,859 bytes, SHA `78a109648f872d50d3f83c6ede81d028faa78e766824921367d5bc08486f191b`;
it defers activation-root counts but has no tracing cycle collector, so it is
not an interchangeable cycle-reclamation reference. These content pins are
source reads, not a rebuild of the installed release.

Our actual mutation cells attempted no collection, so their execution and
whole-TID instruction costs isolate a *collection-free workload*, not a single
field instruction. It still includes table access, casts, loop arithmetic,
root setup/readback, membership and mutable access. Current managed allocation
cells collected and reclaimed real objects with no disabled/rejection result.
The differing allocation/readback formulas prevent subtraction of those two
workloads from becoming pure allocator or collector time. Root metadata only
helps cold classification; it does not explain the current mutation cost.

Before implementing same-operation source reuse, first verify that actual HW
Hotspots attributes meaningful self samples to membership/canonical matching
or reference-write bridges. Then begin with `struct_set_object`/`array_set`
aggregate-reference inputs only: privately authenticate the input once, use
that same authenticated kind/token/actual type/origin for both canonical
compatibility and independent destination-object lease establishment, retain
the original mutable lock and trap/failure order, and commit no field on OOM.
The existing borrowed-view contract explicitly requires the caller's real
owner pin and collection/teardown exclusion until final access. A raw view,
weak owner, canonical type ID, CLI-depth flag, or active-count observation
alone does not supply that exclusion. Retain the original route whenever that
contract is unavailable. No view crosses Wasm allocation/poll/callback,
admission release or source retirement; allocator variants stay out of this
first patch. Null/i31/funcref/extern/exn retain the old specialized paths.

Numeric allocation already reserves an eligible slab slot under its original
lock, commits it under the original slab protocol, then publishes a unique
nonrecycling token into local membership/object chains and the global stripe.
Reference storage uses real byte-array envelope/tail allocation and initialized
16-byte carriers. A per-type reference arena can preserve that envelope,
pointer provenance, lifetimes and publication; it must not be selected merely
because a mutable type resembles the immutable four-byte compact layout.
Consider it only if actual allocator/reservation/publication samples dominate
after the smaller status-lowering and reference-authentication comparison.

## Closing anonymous native PC attribution

An old object file and an old lambda number cannot identify a current sample.
For each actual materialization, use its captured object SHA and the real
`LoadedObjectInfo` section addresses to translate symbol offsets into loaded
native ranges. Bind those ranges to the same finalized object, actual module
source/function index, publisher-owned generation and engine lifetime before
guest execution. Preserve original perf mapping events and sample interval;
address reuse across generations needs separate records. Existing diagnostic
COFF/Mach-O inferred extents must not masquerade as exact ELF symbol-size proof.
These are source requirements; no such current VTune registration is claimed.

The [Intel JIT Profiling API](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/jit-profiling-api.html)
accepts method identity, loaded address/size and optional module/line metadata.
It describes reporting inline regions before executing their parent and the
effects of overwritten address registrations. An optional profiling-only
publisher observer could register the actual function-level ranges at that
cold boundary, with bounded storage, distinct generation IDs and matching
unload/rollback. It must not log on every GC operation, invent DWARF/source
locations, or change ordinary roots/polls. Native function attribution alone
still cannot separate every inlined field operation without exact PC metadata.

## Cross-language semantics after same-byte Wasmtime controls

For later Java/.NET source comparisons, mirror four families with heap classes,
strong static/global 1,024-element root rings and an extra A-root ring for the
reference-array case. Keep the 32-bit LCG and checksum wrap exactly (Java int,
C# unchecked uint), eight array lanes, observable self/cycle restoration and
independent final root traversal. Reference payloads remain classes, not boxed
value-type approximations. Preserve the exported/global roots and readback;
do not add pooling/reuse, identity-hash calls, artificial allocations or
unrelated I/O inside the timed loop. Record actual allocations/collections if
the VM exposes them, rather than presuming escape analysis was prevented.
Startup/JIT warmup/tier transitions are separate from the steady workload;
whole-process measurements and heap/collector settings stay explicit. A
multi-TID Java/.NET process cannot inherit the single-TID Wasm hardware counting
qualification. This is an unexecuted semantic plan, not an industry result.

A future general window would be newly minted by the actual runtime entry
factory after all independent checks. First cover only already published
local objects and scalar access. Keep original registry and token identities.
Resolve an opaque token through real membership once, then bind a private
bounded view to actual owner, canonical type, object kind/extent, exact token,
slab generation/epoch if applicable and the live window identity. Generated
loads/stores receive that proved native view, never `inttoptr(guest_token)`.
Before claiming lock elision, every native reader/writer/admin API must be
counted or the lane must reject; the loader's external caller contract and a
quiet sample alone do not prove omitted accesses absent.

Drain/revoke all native views and cached pointers before host import/callback,
Wasm thread creation/join, foreign object, ordinary unproved helper, table or
global mutation by outsiders, debug/hot replacement, generation stop, native
exception escape, tail/reentrant entry, collection or return. Publish complete
precise roots and outstanding objects before releasing exclusive admission.
Do not enter a shared-reader fallback while holding exclusive: retire first
to avoid self-deadlock. Unknown reference roots/exception owners close the
lane rather than guess. Nested guest entry may reuse only the same actual
window with certified callee boundaries; arbitrary native reentry may not.
Periodic real polls must retire/reopen cooperatively so administrative/debug
stop and threads cannot starve. Reopen needs a fresh factory proof, not stale
shape/generation flags. Unwind/early traps must use the actual ownership cleanup
path, with no bypass through manual SP/FP control.

A view cached across reference mutations cannot cache field values. Reference
stores still require compatibility and the complete graph/foreign lease
barrier. Collection root snapshots use complete opaque references; native
views are not extra roots. Invalidating an epoch before sweep must retire all
views, even though nonrecycled tokens prevent token ABA. First prove cold
wrong-owner, stop/reentry, spawn/host/debug, foreign, invalidated-epoch and
partial drain cases; actual concurrent behavior and latency remain mandatory.

## Later candidate: bounded per-type nonmoving arena

If actual allocation/initialization dominates, batch reservations for canonical
fixed reference structs and bounded small arrays without changing public
tokens. Own the original real byte-array pointer, alignment and envelope/tail
lifetimes as the current slab does. Geometry is a checked class of actual
canonical type, kind, length and element representation; arrays require bounded
length classes and a checked large-object fallback. Use an actual shared entry
lease plus generation/root admission for local reservation, and only a true
exclusive proof for synchronization elision. A thread-local cursor alone is
not ownership. Keep shared refill/retire locks and original per-object token,
membership/global publication and mutable lock initially.

Reserve a bounded batch under the existing allocator authority, initialize
every complete reference carrier before publication, then commit or retire
each slot exactly once. No free/reserved slot is a resolvable object. OOM,
failed reference compatibility/foreign lease, token exhaustion, generation or
epoch exhaustion and construction failure must roll back unpublished slots
without exposing holes. Drain unused reservations before collection and
destruction; keep live/dead/free metadata until true slot reclamation. A reused
native address gets a fresh nonrecycled token. Existing registry removal and
object/foreign-lease destruction order remains authoritative.

Charge actual block bytes, slot stride/header/metadata, retained field plans,
unused reservations, live/dead fragmentation, per-type cursors and work buffers.
Cap arena growth by real backing budget and report peak RSS separately.
Nonmoving locality/size-class batching is the first step; moving native headers
would need a separate complete registry/cache/view rebuild while every borrow
is stopped. No copying or concurrent RC/SATB change is included here.

## Measurement contracts and primary-source lessons

Use three separate evidence families: unprofiled guest/process wall/user/system
CPU with actual frequency P05/median/P95 and sibling activity; grouped P0 pure
hardware counts over the whole guest including startup/JIT with exact
enabled/running closure; VTune hardware sampling/uarch with the actual interval,
symbol mapping, MUX and stack warnings. Neither VTune wall nor its topdown
percentages substitute for unprofiled timings or exact unscaled counts.
Temperature is observation only under the current user rule. No software
sampling fallback is part of this plan.

For each source-matched profile, record allocation/refill/publication, table
transfer/cast, scalar get/set and reference compatibility/lease stacks where
real symbols allow attribution. Preserve unknown JIT weight; do not relabel
`ld.so.cache` or a lambda ordinal as a known GC operation. Finalized function
object/relocation hashes must match that particular ELF/source. Compare mutation
controls to allocation rows, but do not subtract them as an exact pure allocator
timer: setup, old-root reads, lifetime/collection, root slots and locality differ.
Increase the reviewed N only after official identical-byte oracle and actual
bounded collection coverage; a row too short for frequency quality stays
unqualified. An explicitly timed native `collect` call is a separate component
ROI; its timer excludes cold admission and separately records root/readback and
teardown. Whole-VM elapsed divided by N is not collector latency.

The pinned current [Wasmtime copying source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/e2a58746c83390954f225bb7e63f4e4204c4833f/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs)
exposes bounded bump state to compiled Wasm and scans copied objects with a
Cheney worklist. Its heap indices, reference updates and no-GC scopes differ
from our nonrecycled opaque token/global registry contract. Borrow contiguous
reservation/locality, not unchecked pointer conversion or barrier removal.
The pinned [DRC source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/e2a58746c83390954f225bb7e63f4e4204c4833f/crates/wasmtime/src/runtime/vm/gc/enabled/drc.rs)
separates reference-count writes and delayed activation roots and lacks tracing
cycle collection; it is not a bounded cycle-reclamation baseline.
[OpenJDK TLAB](https://raw.githubusercontent.com/openjdk/jdk/6c48f4ed707bf0b15f9b6098de30db8aae6fa40f/src/hotspot/share/gc/shared/threadLocalAllocBuffer.inline.hpp)
suggests separating checked local reservation from refill. [Immix](https://www.steveblackburn.org/pubs/papers/immix-pldi-2008.pdf)
suggests nonmoving block/line locality with charged fragmentation; [LXR](https://www.steveblackburn.org/pubs/papers/lxr-pldi-2022.pdf)
requires complete field logging, deferred roots and tracing cycle fallback.
These design ideas are not measurements of either uwvm product and do not
justify removing synchronization or whole-graph preflight.

## Pure source preparation receipts

Both repositories preserve ordinary and ROS source-model JSONs in
`build/wasm3-evidence/source-only-general-gc-hot-path-20261002/` with a small
hash manifest. Their eight rows and independent scalar results are identical;
their `single_func_emit.h` hashes differ because the ordinary file includes
additional mode infrastructure. Never substitute one complete source hash for
the other. The source report pins each real product's file separately.

Local preparation exercised the 1,024/1,025/2,048 boundary formulas and all
eight independent scalar models, rejected four invalid family/phase/count
inputs, and produced the 65,536 report in both repositories. These are Python
source/math controls, not a native opcode trace, official validator, VM or
performance result. The keeper's cold and profiler receipts are still required.
