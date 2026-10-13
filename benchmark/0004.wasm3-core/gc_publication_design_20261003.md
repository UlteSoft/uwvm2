# GC publication optimization and the next collector boundary

Status: source review and executable witness preparation only. No measurement in
this document qualifies a new product, provider tuple, collector, architecture or
industry ranking. The old 1024-root, 16-million-step observation was approximately
117–118 ns/step for the then-tested JIT products. It does not isolate publication,
field locking, tracing, native bridge calls or automatic-root collection costs.

## Evidence and independent algorithm references

The present `gc_object_store::publish` reserves a nonrecycling token, prepends the
object to its local membership bucket with release CAS, prepends it to the store
object list with another release CAS, and prepends it to the process-wide foreign
index under `global_guard`. Each global stripe is selected from the exact same
bucket index as the local membership bucket. Mutable field reads and writes use
the object's mutation lock. Aggregate reference validation and independent
embedded ownership retention can perform two lookups. These are source facts,
not a hardware-counter attribution of the old whole-Wasm time.

Primary sources were inspected independently:

- The [Core 3 reference instructions](https://webassembly.github.io/spec/core/exec/instructions.html)
  define allocation of a fresh typed struct identity, field packing, nullable
  reference traps and typed field access. The private index ordering must not
  alter any of those observable results. The
  [threads runtime model](https://webassembly.github.io/threads/core/exec/runtime.html)
  distinguishes shared linear-memory actions; its memory type alone does not
  establish an unshared ownership proof for this runtime's cross-module GC
  objects or native host actors.
- Wasmtime local commit `3f3f222b77a198db939863d8769af6092ee547e6`,
  `crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs` and
  `crates/cranelift/src/func_environ/gc/enabled/copying.rs`: Cheney semispace
  collection, inline bump allocation, precise relocating stack maps and no
  concurrent-mutation read/write barriers. Its
  [collector documentation](https://docs.wasmtime.dev/api/wasmtime/enum.Collector.html)
  explains the throughput, pause and heap-utilization tradeoffs. A DRC result
  without cycle collection cannot qualify a cyclic workload's reclamation.
- WAVM local commit `6f871e61c6e8fc54fa5317b5d61d128d681846f3`,
  `Lib/Runtime/ObjectGC.cpp`: compartment-owned runtime-object tracing, explicit
  root counts and an exclusive compartment lock. This code does not establish
  that the inspected version implements modern Core 3 struct/array GC.
- OpenJDK's primary
  [ZGC barrier source](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/gc/z/zBarrier.inline.hpp),
  [Shenandoah barrier source](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/gc/shenandoah/shenandoahBarrierSet.inline.hpp)
  and [G1 barrier source](https://github.com/openjdk/jdk/blob/master/src/hotspot/share/gc/g1/g1BarrierSet.inline.hpp)
  distinguish relocation/load healing, concurrent SATB preservation and
  generational remembered sets. These mechanisms solve collector invariants;
  importing their names or barriers into a stopped, nonmoving collector does not
  establish a performance improvement. Master sources are research references,
  not the version pins of any measured JVM binary.
- The primary [LXR paper](https://arxiv.org/abs/2210.17175) combines nonmoving
  Immix-style allocation, coalesced reference counting, concurrent cycle tracing
  and selective stopped copying. Its reported application results are the
  authors' measurements, not predictions for this runtime or this workload.

No upstream implementation is copied into product code here. The first change
below follows the current runtime's actual indexing and stop protocol.

## Candidate: one CAS during ordinary object publication

`UWVM_EXPERIMENTAL_SINGLE_CAS_GC_PUBLICATION=1` is an independent, default-off
candidate. Its scope is ordinary header publication; compact numeric descriptors,
exception tokens and extern bridges keep their own existing publication paths.
The original preprocessor-off path must retain its ordering and generated code.

The stricter publication order is:

1. Fully initialize the unpublished header and its typed payload, and reserve its
   globally unique, never-recycled token. Exhaustion still destroys the private
   unpublished object and returns `size_overflow`.
2. Obtain the existing global stripe for `bucket_index(token)`. Every publisher
   into that local bucket obtains this same stripe. A stripe may serialize
   additional buckets; that is the existing lock mapping, not a new lock.
3. Read the current local bucket head and global head. Initialize this fresh
   header's `hash_next` and `global_next` while it is still undiscoverable.
4. Prepend to `objects_` using its original release CAS loop. Initialize `next`
   before each CAS attempt. The chain now exposes a complete live header.
5. Prepend to the protected global index. Publish the local bucket head with
   `store(memory_order_release)`, replacing its redundant CAS loop.
6. Release the stripe. Only then assign the native result's reference kind and
   token to the caller's initialized result slot.

This order removes one CAS, introduces no fields or extra reader checks, retains
the existing stripe lock and leaves every local lookup's acquire load intact.
The store-list CAS now occurs while the stripe is held. Longer lock hold time is
a real possible contention regression and must be measured with 1/2/4 publishers
and multiple stores; it must not be hidden by a single-thread-only result.

An alternative, shorter lock interval can prepend `objects_` before obtaining
the stripe. Ordinary readers do not traverse the list; an exclusive collector
must stop all publishers before inspecting it. That alternative is not the first
implementation: initializing all links before first index visibility makes the
initial candidate easier to review and audit.

### Safety invariants

- The token remains an integer identity. No token is cast to a header address or
  dereferenced. A forged, wrong-kind, future or stale key passes the same actual
  bounded local/global membership lookup before any payload read.
- All concurrent writes of a candidate local bucket head originate in
  `publish` and hold its token's exact stripe. Collector bucket splices occur
  only in the existing genuinely stopped canonical cohort; they cannot overlap
  any candidate or ordinary publisher. Mixing different macro definitions of
  this inline class across translation units is forbidden.
- The release store publishes the complete initialized `hash_next` chain.
  A local reader's acquire makes the payload and every predecessor link visible.
  Unlock/relock of the stripe also synchronizes successive writers. A relaxed
  head load inside that stripe is therefore sufficient for the writer.
- A foreign reader obtains the same stripe. It cannot observe the intermediate
  global/local head update and promotes the actual originating store's weak
  control block before releasing the lock. Existing receiver and embedded-value
  leases remain mandatory. No copied pointer, active count, TID or unshared flag
  replaces a canonical lifetime lease.
- Objects retain their original physical allocation, type/canonical IDs,
  mutation locks, trace metadata, value leases and generation behavior. Reads,
  writes, packed fields, v128, imported aliases, recursive subtyping, exnref and
  extern conversions receive no reduced checking contract.
- Admission does not become weaker. A collector cannot run while a publisher,
  native reader, initializer, teardown actor or host callback lacks a completed
  stop. Unknown or incomplete roots still prevent reclamation.
- List insertion and publication allocate no new resources after issuing the
  token. The stripe scope contains no callback, owner release, host I/O,
  safepoint or foreign retain. Thus it creates no new exception or reentrant
  lock-order boundary.
- All successful CAS retries modify only the fresh object's `next`; no
  previously published link is mutated during insertion. Unlink/free remains
  exclusively in the existing stopped collector or drained teardown.
- The JIT cache environment must carry the candidate's exact publication policy
  key. All native components and all product translation units must be freshly
  built with the same flag; an old cached object or a reused runtime object does
  not qualify a new provider/source tuple.

### Required actual witnesses

`test/0017.runtime/gc_publication_handoff_20261003.cc` uses two canonical
`shared_ptr` stores and two real lease-owner lists, plus real admission leases in
all producers and the foreign consumer. Each producer owns distinct output
slots and publishes their initialization with release/acquire progress. The
consumer resolves actual references through the other store while publication
is in progress, checking full i32, packed i16 and v128 contents. This also
checks cross-module type equivalence and real foreign owner pinning. The `same`
mode publishes into one store; `split` distributes actual publishers between
both stores and resolves every object through the opposite store. Collision
cases record the actual source-coupled stripe intersection and require shared
stripes, covering cross-store writers using the same global locks.

After joining every actor, the test checks every local membership and every
token's uniqueness, creates typed self-cycles, and makes a receiving-store
reference array the only explicit semantic root. It acquires a genuine exclusive
lease protecting the actual outer reader, includes both stores in the complete
canonical cohort, and checks the exact reclaimed count. Both local and foreign
queries must reject retired tokens. Clearing the last root must reclaim the
remaining cycles and the array. A later allocation must obtain a new token.
The retained native token arrays are privileged stale-identity audit samples,
not product activation roots or permissions to dereference reclaimed objects.

The handoff timer includes producers, thread scheduling, status checks, foreign
lookup and payload checks. It must not be reported as isolated publication cost.
The independent `gc_native_api_costs_20261003.cc` allocation phases, and the same
Wasm artifacts used for product comparison, are separate performance controls.
Fresh O3 assembly must show exactly one publication-list CAS in the candidate
and preserve the original acquire lookup. Cold token-block refill and allocator
CAS instructions must be distinguished from the hot publication operation.

Actual test matrix:

- Both source trees, macro absent and macro exactly 1; header and named-module
  consumers; EH/no-EH where supported; cold default plus ASan/UBSan and a
  separate genuine thread-race sanitizer lane.
- 1/2/4 publishers, 2 objects for cold setup, 16,384 and 131,072 objects per
  publisher for bucket collisions and scaling. The upper bound is 262,144.
  Provider headers/archives, artifact hashes, boot/cgroup/TID identity and
  in-window memory peaks must be recorded. Linux tests remain in the shared
  64 GiB cgroup; native Mac tests are prohibited until measured Linux peaks
  plus platform-specific overhead establish a safe margin below 4 GiB.
- The existing dense/sparse 64/65-reference, packed/SIMD, GC cycle, foreign
  cycle, subtype, extern-conversion, exnref and automatic-root Wasm fixtures.
  New pointer-width and big-endian QEMU cases must use source-bound fresh builds.
- Same-platform plain wall-time reversed pairs and actual hardware-counter
  VTune hotspot/uarch runs. Record actual CPU frequency and scheduler noise.
  Do not subtract unrelated startup/collection costs or rescale unavailable
  hardware counters into fabricated publisher costs.
- A source-identical macro-off build must not regress interpreter/JIT memory64
  or normal call throughput. Macro-on acceptance requires an actual improvement
  on ordinary GC publication, no regression in foreign or concurrent modes,
  and every correctness witness above. Default enablement is a later decision.

## Larger redesign: stable identity over a paged nonmoving heap

Removing one CAS cannot reproduce a copying VM's inline bump allocation by
itself. The next substantial design should separate guest identity from physical
ownership in a bounded paged handle directory. Each canonical store receives
stable owner identity and owns physical nonmoving pages. A private participant
may reserve a fixed-size initialized allocation region once, then allocate
objects within that region without a store-wide CAS per object. Handle segments
are published once with release, and indexed slots publish complete headers.
Objects retain a never-recycled generation, and all slot accesses first prove
owner/segment bounds and full identity. A handle is never a raw host pointer.

The important boundary is not the number of hash maps but the runtime's exact
ability to prove managed references. Unforgeability inside validated Wasm does
not make arbitrary native imports trustworthy. Host boundaries require checked
handle creation/retention; every engine's rooting and handoff rules must carry
the same canonical owner and generation. A cheap generated field load is only
legal when its dominating proof covers that actual object, exact type, owner,
poll-free interval and active participant. It must be invalidated at calls,
polls, callbacks, module replacement, checkpoint restore and collection.

Nonmoving mark/sweep is the first compatible physical collector because existing
exception payloads, extern wrappers, imported aliases and conservative root
paths may retain opaque identities without writable root locations. A future
moving collector requires precise, writable local/operand/stack-map locations
in interpreter full/lazy, LLVM full/lazy, tiered transitions, tail calls,
exception landing pads, host callbacks and checkpoint shadows. Any opaque
unknown/conservative external root requires pinning or collection refusal.
Replacing these locations with conservative byte scans does not qualify moving
GC. Moving the body behind a stable handle avoids rewriting guest identities
but adds a real indirection whose generated-code and cache cost must be measured.

A generational design requires a remembered-set invariant for every old-to-young
write: struct.set, array.set/fill/copy, array.init_elem, globals, tables, active
and passive elements, host-import transfers, exception payloads and debugger/
checkpoint writes. Young unpublished initialization can omit a barrier only
under a private, checked allocation-generation proof and before the first poll
or escape. Thread-local write logs need release publication and bounded capacity
before safepoints. Direct copying of numeric fields never creates reference
edges. Cyclic old objects still require major tracing. An age byte, a young-list
label or an optimized numeric-only benchmark cannot replace these obligations.

Implementing concurrent SATB, coalesced RC or relocation barriers before this
ownership/root foundation is complete would add hot-path work without proving
the intended pause or throughput benefit. The paged/participant design should
first produce separately bound default-off native primitives, then prove full
Wasm-root integration and mixed-feature operation, and finally earn enablement
with same-artifact industry comparisons and actual hardware/ASM evidence.
