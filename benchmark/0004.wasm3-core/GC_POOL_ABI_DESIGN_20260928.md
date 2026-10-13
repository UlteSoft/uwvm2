# GC allocation and immutable-field implementation route

This is a source-pinned design and qualification plan. Exact one-block header
`0c17afb7976fb6d22c6708f0187952f638cac9c19f8e7f23c100223b5e31feeb`
passed the two-repository SSH Linux native qualification: 53/53 commands, clean
positive O3 and ASan/UBSan/LSan suites, real explicit collection and reclamation,
and 8/8 unchanged normalized field get/set assembly wrappers. It is now
integrated into both live development trees. This is a native component result;
full-VM lowering, automatic GC and formal P-core paired performance remain
pending. The independent numeric slab and scalar bridge candidates do not
inherit this qualification. The old 117–118 ns/step measurements belong to the
old c7f9/43a37 product binaries, and must not be assigned to these candidates or
the precise-frame emitter.

The immutable [native compact report](../../build/wasm3-evidence/gc-single-block-16fcee-linux-20260928-env2/compact-report.json)
has SHA-256 `64a6e425d9a22dbeac3d97df8097e7525c605b820203cbcdae972cac8d809742`;
its native summary is `a3b908492553acd31fc78855fd2b70128e42c7416cc1cb05ad1ae535fa8aef2e`.
The maximum observed command process-tree RSS was 561,954,816 bytes. The first
environment's strict host-observer failure remains a failure; only the fresh
second environment supplied this qualification. No old runtime object was
linked into the component TUs, and no whole-VM GC release gate is cleared.

The companion `GC_POOL_ABI_SOURCE_PINS_20260928.json` pins Wasmtime 49.0.1 to
`46c23a87dac1465986a8ad53ba6a7ae49372857b`, and the OpenJDK allocation reference
to JDK 25 GA tag `jdk-25+36`, commit
`6c48f4ed707bf0b15f9b6098de30db8aae6fa40f`. That historical OpenJDK source is an
algorithm reference, not the source qualification of a later downloaded Java
comparator. Both products' examined pre-one-block GC baseline header is the exact
owner-checked `16fcee47d451c614b222db978ff2fc2570d8c3ada94c782a0505b0ae1407b1f5`.

## Measured target and source-supported cost model

The historical self-checking allocation ring retains 1,024 exported table
roots. Its object field is immutable; `struct_get_object` reads that field
without `object_lock`. Mutable heap-update fixtures do take the lock, and
must remain a separate performance cell. Removing the mutable-field lock
cannot explain or repair this immutable ring's result.

The 16fcee allocation path constructs an object and a separate zeroed
`gc_object_value[]`; `publish` inserts local membership and ownership-chain
heads with separate CAS operations, then inserts a global foreign-lookup node
under a stripe. The token issuer reserves 1,024 IDs per TLS block, so it does
**not** execute the global token CAS for every allocation. Its local lookup
compares an opaque token through a membership bucket; foreign lookup is a cold
striped owner-promotion path. These are concrete source operations, not a
sampled distribution of elapsed time. A sampled profile and exact JIT/native
bridge disassembly remain necessary before attributing percentages.

For an uncollected step, separate allocation/zeroing, token/index publication,
bridge packing/status checks, table access, cast validation and field access.
For a collected run, additionally report real collection and root-publication
work amortized per allocation, alongside stop-the-world pause duration. Native
explicit-collection timing is a component measurement; it is not a substitute
for the same-Wasm guest loop or automatic-collection qualification.

## What the pinned competitive implementations provide

Wasmtime's Copying compiler emits a checked bump-pointer fast path. Capacity
failure reaches a cold `gc_alloc_raw` call which may collect or grow. The fast
path initializes header and fields in generated code and marks returned GC
references for stack maps. This is an architectural advantage over an allocator
call plus three per-object indexes; it is not an instruction-count result for
our particular measured object. [Pinned compiler path, lines 83–200 and
240–273](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/func_environ/gc/copying.rs#L83).

Its runtime uses two semispaces, forwards reachable objects, rewrites roots,
and scans the copied work region. Field access requires no concurrent-marking
barrier under this collector's stop-the-world protocol. The first UWVM
nonmoving collector should likewise obtain complete quiescence before claiming
that no concurrent barrier is needed; the current mutable synchronization
contract remains independent. [Pinned runtime collector, lines 1–11,
398–443 and 1040–1114](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs#L398).

Copying's fixed object header is 16 bytes, with 16-byte object alignment and
inline trace metadata where possible. Its shared layout builder uses each
field's real size and alignment rather than reserving a 16-byte carrier for
every numeric field. That compact layout is a further design difference;
changing UWVM's full carrier storage requires a separate tracing, array-stride,
packed-field and ABI migration, not a field-view reinterpretation. [Collector
layout](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/environ/src/gc/copying.rs#L6),
[field layout calculation](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/environ/src/gc.rs#L151).

HotSpot's TLAB fast path advances a thread-private top only after checking
remaining capacity. Its slow path considers unused-buffer waste, retires or
refills the buffer, and retains an outside-buffer allocation fallback. This
suggests measuring refill count and unused memory along with throughput, rather
than introducing an unbounded cache for every historical store. [Pinned TLAB
fast path](https://github.com/openjdk/jdk/blob/6c48f4ed707bf0b15f9b6098de30db8aae6fa40f/src/hotspot/share/gc/shared/threadLocalAllocBuffer.inline.hpp#L38),
[refill/fallback policy](https://github.com/openjdk/jdk/blob/6c48f4ed707bf0b15f9b6098de30db8aae6fa40f/src/hotspot/share/gc/shared/memAllocator.cpp#L253).

## First candidate: one allocation, complete existing semantics

The isolated r2 candidate is
`/tmp/uwvm2-gc-single-block-16fcee-r2/include/uwvm2/uwvm/runtime/storage/gc_object.h`,
SHA-256 `0c17afb7976fb6d22c6708f0187952f638cac9c19f8e7f23c100223b5e31feeb`.
Its patch is `07940ccad77e0460f615a84ee650310a0ceb5babdbb3e910343dfded407a7969`.
The original object header and a real zeroed full-carrier array occupy one
aligned ordinary allocation. A one-word nonowning field view and class-specific
deallocation preserve all original hot-field offsets and object size.
Tokens, every publication index, foreign leases, locks and cohort rejection
are unchanged. See [the detailed candidate and remaining gates](GC_SINGLE_BLOCK_CANDIDATE_20260928.md).

The completed SSH Linux native edge qualification covered:
zero-field and zero-length objects, full numeric/reference/v128 carriers,
packed extension, large arrays, checked total-size overflow, injected OOM at
both old allocation sites, partial foreign-field failure, explicit cycle
collection, stale tokens and nonreuse. Positive ASan/UBSan/LSan suites were
clean, and all four normalized O3 get/set wrappers in each repository retained
their instruction bodies. The remaining performance gate must measure nine
paired rounds with actual collection/reclamation/checksum counts,
1/2/4 P cores, root-ring RSS, field latency, teardown and temperature. A
positive allocation result cannot clear the full-VM GC release gate.

## Next bounded candidate: nonmoving size-class buffers

After the one-block lifetime/deallocation proof, replace its allocation backend
with store-owned size-class chunks. The first version keeps full carrier slots
and all existing indexes. Large arrays retain a checked single-block fallback.
A mutator leases a small batch of free slots or a bounded bump region from its
store; allocation within that batch needs no shared free-list operation.
Refill returns to a synchronized cold path. Chunk size and class thresholds are
measured parameters, not requirements inferred from another VM.

Every live slot contains the ordinary constructed header and complete field
array. An allocation descriptor owned by the chunk, or a separately constructed
prefix, must outlive the header and identify the slot's deallocation backend.
Do not read `object::owner` after its destructor has ended its C++ lifetime.
Every unpublished failure, sweep and teardown returns a slot exactly once;
foreign-lease destruction occurs outside the global index lock. An entirely
free chunk is released only after all mutator caches and native readers are
retired. Holding one surviving object must not retain an unlimited sequence of
otherwise empty chunks.

Native slot reuse must issue a **fresh nonrecycling opaque token**. Remove the
old token from local/global membership before reuse, and fail before token
wrap, including on i386. A stale token remains invalid even if malloc or a
chunk reuses an identical native address. The Wasm specification uses abstract
addresses and reference identity; it does not require exposing a host pointer.
[Core 3 runtime addresses](https://webassembly.github.io/spec/core/exec/runtime.html#addresses),
[reference equality](https://webassembly.github.io/spec/core/exec/instructions.html#ref-eq).

Keep per-thread caches bounded and keyed by an independent store identity with
an explicit detach protocol. Exercise 64-store round-robin switching and
thread exit, not only one store per worker. Record active chunk bytes, cached
unused bytes, refill/slow allocation counts, native allocation calls, registry
entries and issued token range. Metadata must scale with active chunks/live
objects, rather than the highest token ever issued. A TLS cache must not keep
unloaded stores permanently alive or restore a chunk belonging to a recycled
store address.

This candidate removes allocator overhead, not publication cost. The initial
version deliberately retains the two local publications and the foreign index.
Only after profiling this version should a different index layout be proposed;
it needs exact-membership, forged-unissued-token and owner-teardown proofs,
without a hot local-read regression. Bump allocation without reuse and actual
collection is a monotonic arena, not a production collector.

## Separate typed immutable helper candidate

A validated one-field i32 `struct.new` can use a dedicated scalar C bridge:
module address, type index, i32 value and reference-output address, with a status
result. It constructs a real `gc_object_value` internally and retains the
checked store/type/field count/publication and OOM path. This removes generated
input-slot packing for that operation without changing stored field layout or
passing a C++ aggregate by value. All unsupported layouts use the existing
five-argument fixed bridge or generic fallback.

For a statically known immutable i32 field, a similarly typed get bridge can
return an i32 through a real output slot and avoid a generic 16-byte output
copy. It must still reject null/forged/stale references and preserve bounds,
canonical subtype semantics and foreign owner promotion. Specializing by the
validated field descriptor may remove redundant dispatch/type-layout work;
that is a hypothesis requiring generated code and full semantic tests. An
immutable type must not be inferred merely from the caller's untrusted token.
Do not remove mutable-field locking as part of this candidate.

A cast immediately followed by get may reuse one validated membership lookup
only while no safepoint, host callback or store mutation can intervene. A
native object pointer is an ephemeral derived value: keep the opaque reference
as the root, and resolve it again after a collection boundary. If a typed helper
can allocate, every incoming reference and operand-prefix reference must be
published before entering it, and the fresh output must be rooted before a
subsequent callback. The failed current actual root-frame probe must be fixed
before these helpers can use automatic collection.

## VM integration and release acceptance

A store collector and allocation backend cannot discover all guest roots.
Automatic collection requires precise interpreter ring/frame maps and LLVM
full/lazy/tiered/OSR snapshots; globals, tables, host handles, pending calls,
exception payloads and reference results also participate. Enter/leave frame
records alone do not stop readers or prove a complete snapshot. Allocation
inputs must be rooted before operand descriptors are popped. Return and tail
handoff must retain result/argument references across any collecting boundary.
Ordinary numeric/memory-only paths must not acquire unnecessary root calls.

All admitted mutators, parked `atomic.wait` participants, initializing or
tearing-down modules and reentrant host/native readers need a common collection
admission protocol. Wait for participants without holding registry/object
locks. A stopped-world native pointer is usable only while that snapshot and
its strong cohort pins remain valid. Exception and extern-wrapper graphs need
typed tracing beyond the current aggregate-only collector's rejection scope.

For each step, run actual forced collections while a reference exists solely
in one root location, including nested host calls, cold blocks and EH payloads.
Check old references after slot reuse and cross-store owner teardown. Then run
same-Wasm ring, mutable fields, connected cycles and discarded exnrefs through
all supported product execution modes, both repositories and target platforms.
Demand positive authoritative collection counts, reclaimed objects, unchanged
checksum/live data and a bounded RSS curve. Keep same-Wasm Copying/DRC/Null
configurations separate, and Java/Graal/Node/.NET analogues separate from them.

Formal timing belongs only in a handed-off quiet 64 GiB/swap-free cgroup window
on the verified P cores. Preserve each source/compiler/binary/fixture hash,
raw reversed samples, frequency/temperature, RSS/heap checkpoints and pause
samples. Nine process samples do not qualify a service p99; a percentile of
collection pauses needs enough actual recorded pauses and must retain its
sample count. No candidate in this design is described as close to industry
best until complete reclamation and the measured safety/performance gates pass.
