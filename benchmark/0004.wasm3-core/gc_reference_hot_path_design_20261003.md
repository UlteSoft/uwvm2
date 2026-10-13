# GC allocation-chain and reference-cost source review

Status: source and pure-data preparation. Neither new C++ fixture has been
compiled, run, disassembled or measured. No new production optimization is
implemented here. The existing single-CAS publication experiment stays off by
default and awaits its independent native/product qualification. A Python oracle
cannot qualify a native binary, a provider, a complete pause or a VM root map.

## The original 117–118 ns observation

The original `generate.py` allocation-ring case uses one **immutable i32** field,
an exported 1024-slot `anyref` table and this loop:

    state = state * 1664525 + 1013904223  (modulo 2^32)
    index = remaining & 1023
    struct.new → table.set → table.get → ref.cast (ref type0) → struct.get
    remaining -= 1

The original 16M-step number does not measure mutable field locking or a
foreign proper-subtype relation. An improvement of those separate paths must
not be presented as its explanation. The old source-bound gate also predates
the current nonmoving collector and several independent experiments; a new
product/source/provider/profile must be measured again.

The prior manifest identities supplied by the bench owner are preserved, not
regenerated or relabelled as newly verified Wasm:

| Original count | Original final state | Original Wasm SHA256 |
| --- | ---: | --- |
| 2,000,000 | 750095765 | f1b1637cf4b7c1380ba6320065cf534818c6c154d8eabe63a7b2eafd8013c316 |
| 16,000,000 | 493211925 | 66874f9a4a0a5977b4ace1cc702f949c6c2627311dce0bcbe87c818c55516e3e |

The final real table slot `s` must contain the state at step
`N - ((s + 1023) % 1024)`. Thus slot1 contains stateN and slot0 contains
stateN−1023. Ascending `step & 1023` is a different fixture. The new pure-data
plan checks this formula against a direct descending loop and the original
generator's `expected_for_case`; it does not compile WAT or verify the old
binary hash itself.

Actual S6e hardware reports have separate relevance. In its reference-array
allocation profile, named sampled-self collector locate was 15.77%, publish
3.17%, and embedded retention 1.29%. Its mutable-struct mutation sample had no
collection and only coarse samples. That exact binary used compatibility-map
thread state, so `get_thread_state` cost is not attribution of a native-TLS
build. Unknown JIT-address buckets and multiplexed uarch summaries remain
unknown/limited. See the source-bound receipts in
[the actual VTune report](CURRENT_GENERAL_GC_VTUNE_RESULTS_20261002.md) and
[the independent plain/HW measurements](CURRENT_GENERAL_GC_RESULTS_20261002.md).
Those figures do not isolate the original allocation ring or qualify a new
candidate.

## Priority witness: real immutable allocation/table chain

`gc_immutable_table_chain_20261003.cc` owns a real `wasm_module_storage_t`, one
real `local_defined_table_storage_t`, its actual owner pointer and table
declaration, and a real native typed-slot vector. Its store and independent
module lease list are genuine shared owners. The fixture calls the existing
storage leaves in the original order:

1. `uwvm2_gc_struct_new` for the immutable type0 scalar.
2. Actual table family and bounds checks, `runtime_table_slot_from_gc_reference`,
   `retain_runtime_table_reference`, and the real slot assignment.
3. Actual table family and bounds checks, `runtime_table_slot_to_gc_reference`,
   `uwvm2_gc_retain_reference`, and the loaded complete carrier.
4. `uwvm2_gc_reference_type_matches` for nonnullable defined type0.
5. The existing checked full-carrier `uwvm2_gc_struct_get`.

It uses actual static root visitation during collection. Under a genuine
exclusive admission lease that `protects_shared` its exact outer reader, it
first counts then copies `visit_quiescent_cohort_static_roots` over its actual
strongly owned module. The only snapshot array is populated by that visitor;
it does not stand in for the table. Count/copy agreement and 1024 complete slots
are required before the closed canonical cohort collector is called. The
native instance remains unowned by the product initializer serial protocol;
the fixture does not manufacture an initialized VM or automatic-GC authority.

`collect4096` performs explicit collection every 4096 completed chain steps.
`deferred-collect` leaves all collection to untimed qualification. Both perform
real final root readback, reclaim overwritten objects, clear the actual table
to null slots, reclaim the final 1024 objects, and reject old tokens. Total
reclamation equals actual allocations. Privileged weak audit tokens are not
semantic roots or payload addresses.

The internal `chain_ns` includes checks, selection, loop arithmetic and timing
checks; explicit root census/copy/collection intervals are subtracted and
reported separately. Setup/final readback/qualification are outside this
internal interval but inside process wall and whole-process PMU. These metrics
must retain their different regions of interest. A phase difference is not an
isolated instruction or collector cost.

### SDK boundary and actual code generation

`wasm_module.h`, its codecs/retention helpers and `gc_static_roots.h` have no
LLVM SDK dependency for this component. The actual `llvm_jit_table_get_bridge`
and `llvm_jit_table_set_bridge` reside in `single_func_emit.h`, which also
contains the LLVM IR emitter. They instantiate an actual RT debug table guard.
Including the emitter requires the provider SDK and the guard's implementation
requires actual runtime linkage. This fixture supplies **no fake RT symbol or
inert guard replacement**.

It does not measure full bridge ABI scratch-buffer copies, RT debug guard,
TLS state lookup, guest polls, original root-map traffic, generated status
dispatch or immutable cast/get witness fusion. Current scalar32 and immediate
immutable-cast witnesses must be inspected in the actual generated product:
`single_func_gc_emit.h` extracts exact integer kind/payload fields, selects
same-block/immediately-adjacent immutable witnesses only after all eligibility
checks, and retains the checked mutable/foreign fallback. Native full-carrier
component calls are not equivalent to those emitted integer bridges.

SDK-free component cold/ASM/PMU is a supplement. Final acceptance still needs
fresh full-JIT binaries running the same original 2M/16M Wasm bytes, both
instruction/unwind strategies, matching profiles and actual source/provider
closure. The publication experiment must be compared against its same-source
default while all other experiments and native TLS settings are held fixed.

## Secondary reference-cost witness

`gc_reference_costs_20261003.cc` is prepared for later attribution and is not
the next native priority. It separates `match-defined`, `get32`, `set32` and
`set-reference` across local, foreign-canonical-equal and foreign-proper-subtype
cases. There are two actual store control blocks and two real module lease
owners. A leading unrelated function type shifts the receiving module's base
type index. Equal canonical IDs are asserted despite distinct local indices;
a derived type has a different ID, a declared base parent and an additional
immutable i64 field. Scalar32 is mutable in this separate fixture.

Every case creates 1024 source objects and 1024 typed reference holders. Setter
phases read back each written value; proper-subtype and equal cases vary the
actual opaque source token. At qualification, only real holder roots are
retained. The collector must keep exactly the distinct currently referenced
sources and reclaim the rest; clearing all roots then reclaims every remaining
object. Stale cast/get operations must fail. This checks existing foreign
retention and the complete graph; it does not prove generated VM roots or that
independent strong pins were unnecessary.

The timed setter phases each contain setter plus readback; match/get have one
API operation. Setup and two closed collections are untimed. Results cannot
be subtracted into an asserted lock/type/owner cost without actual generated
assembly and profile evidence. The count and oracle plan never grants native
execution permission.

## Source facts and bounded larger candidates

Current ordinary get/set first authenticate the opaque token by acquire local
membership; foreign misses use the global stripe to promote the actual origin
control block before unlocking. They then establish the receiving module's
real foreign lease. Mutable reads/writes retain `object_lock`. No small
numeric carrier or single-thread benchmark is a proof to remove that lock:
host/native actors and module aliases can access the same actual object.
The carrier is 16 bytes, and changing only the scalar writer to atomics while
existing readers/copy/fill still access it normally would create a C++ data
race. v128 and composite references need a coherent representation as well.

Reference writes currently validate the source's dynamic kind/nullability/type
and independently resolve/retain it for `destination.value_leases`. This second
lease must survive receiving-module root release because the destination object
may outlive that module. It cannot be replaced with the receiver's module lease.
The existing default-off array-local-auth experiment already reuses a
same-operation boolean for authenticated ordinary local sources; it is not a
general foreign-resolution ticket.

### Candidate A: immutable owner-local canonical ancestry

The current registry's proper subtype relation is a single-parent ancestor
query. Each valid store already owns an immutable local parent forest and its
immutable canonical-ID map. Same-owner parent success and exact canonical-ID
equality already avoid the registry mutex; only remaining proper-subtype
queries need the mutable registry vector today.

A prospective separate default-off experiment can build private per-type
`depth` and `jump` metadata from the validated local forest before publishing
the store. No object, opaque token, field storage or root ABI changes. Parent
indices are strictly smaller; depth is less than the checked type count.
For depth d, jump targets depth `d & (d - 1)`, matching the current registry's
bounded low-bit ancestor algorithm. A root jumps to itself and needs no absent
parent dereference. After bounding both actual and expected type indices and
checking kind, an actual local ancestor at expected depth is compared to the
expected immutable canonical ID.

The invariant is `localCanonicalID(localParent(t)) ==
registryParent(localCanonicalID(t))`. The intern key includes the supertype
relation, and construction only accepts the already validated canonical map.
Induction establishes equal local/global depths and ancestor IDs; a canonical
equivalent declaration in a different module therefore has the same depth.
This applies to declared function/struct/array subtyping, not arbitrary
unvalidated structural guesses. A random-forest Python model is only algebra
sanity checking; native canonical interning/recursive-group tests are required.

Construction failure remains an invalid store before cohort enrollment; checked
allocation-size arithmetic and OOM behavior must be explicit. The global
registry still serializes interning. The executor reads only actual
owner-pinned immutable metadata, so concurrent module insertion cannot
invalidate a vector view. Use an independent cache policy key and coherent
all-TU definitions if implementation is later authorized. Do not first add a
quadratic array of every ancestor or replace the current logarithmic fallback
with an unbounded linear chain on adversarial deep modules.

This candidate targets foreign proper-subtype tests, not the old single exact
canonical type ring. Its merit requires native depth1/64/1024 positive and
negative controls, duplicate canonical declarations at different indices,
concurrent interning, wrong kind/invalid index rejection, and actual O3/HW
results. It is not implemented in production here.

### Candidate B: one-operation aggregate authentication and retention

A prospective private resolver can return the authenticated source view and,
only for a foreign source, the genuine promoted canonical strong owner. It
must retain existing receiver-module lease acquisition and error order. After
the exact canonical/nullability check succeeds, embedded retention consumes
that same authenticated result. A same-origin destination needs no foreign
object lease; a foreign destination still calls its independent
`value_leases.hold` with the actual strong owner.

This removes a repeated global/local membership traversal, repeated weak owner
promotion and duplicate receiver lease scan; it does **not** remove any final
object-owned lease, publication fence, mutable lock or OOM check. Default local
numeric paths must not acquire/copy a `shared_ptr` or grow the carrier. Prefer
an inlined private operation with a foreign-only slow result, not an optional
owner object attached to every guest reference.

The proof lifetime is one synchronous mutation under existing real entry and
owner pins. It crosses no poll, allocator slow path, callback, reentry,
collection, module drain, status continuation or generated function boundary.
It is not an issued-token cache or a reusable public ticket. Destination-kind,
field extent and mutability failures still precede input type/lease checks;
input type failure precedes final embedded-lease OOM. No partially written
field is visible when retention fails. Independent extern bridge and exnref
ownership/root protocols remain original fallbacks; their pointer kinds cannot
inherit an aggregate result.

Testing must include different actual source/destination/expected-type owners,
module aliases, overwrites followed by real lease pruning, foreign cycles,
local/foreign wrong kind and stale tokens, original exn traces, bridge identity,
OOM/error precedence, and native thread handoff. Real field-profile evidence
must justify its priority; the old S6e retain self bucket was much smaller than
collector locate. This candidate is not implemented in production here.

### No arbitrary caller can borrow a collector cache

A collector-local hit cache belongs to one uninterrupted genuinely stopped
canonical cohort and can be minted only after actual membership authentication.
Enrollment/cohort mutex, reader count, token geometry, copied pointer or a
caller-supplied boolean is not world-stop authority. It must be discarded before
epoch commit/sweep/prune/free can invalidate anything. Ordinary casts, getters,
setters and foreign readers do not acquire this cache identity. The designs
above instead use existing immutable metadata or the actual synchronous
operation's lifetime and independent ownership leases.

## Remote acceptance

The Linux keeper owns all compilation/native execution in the existing
swap-disabled 64GiB cgroup. Build/correctness use the admitted E-core set;
performance uses a qualified P-core. Preserve actual UID/TID/birth/retirement,
source/provider/artifact closure, frequency and resource telemetry. Run small
cold controls first, then paired unprofiled A/B and actual official-CLI hardware
VTune Hotspots/uarch with separate coverage. Whole-TID exact PMU and sampled
microarchitecture are separate measurements. No thermal-only refusal or
software-counter substitution follows from this plan.

Supplemental source leaves must not overwrite newer shared production headers
or immutable joint freezes. No native compile, sanitizer, Wasm parser, QEMU,
macOS or Windows result exists for these new fixtures yet. New platform results
must qualify their actual provider/ABI/endianness; a native little-endian pass
does not grant a big-endian or ISA32 pass.
