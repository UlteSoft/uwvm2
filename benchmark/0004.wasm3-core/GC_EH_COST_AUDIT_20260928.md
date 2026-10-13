# GC and exception cost audit, 2026-09-28

The measured GC ring first needs cheaper allocation/publication and fewer bridge
operations, together with real safe reclamation. It does **not** exercise the
mutable-field lock. The measured cross-function exception needs compact retained
diagnostic identities before attempting a different native unwinding protocol.
These are implementation targets, not measured improvements or release approval.

## Provenance and limits

The 117–118 ns/step ring and caught-exception timings belong to the historical
ordinary `c7f97991…62841` / ROS `43a37bcf…3a1c1a` O3 binaries. Their full IDs,
raw samples, thermal limitations and ELF hashes remain in
[the original report](REGISTRY_V3_AND_MANAGED_20260928.md). They cannot qualify a
later source batch, the five-argument bridge, or the explicit collector component.

The source audit below uses the immutable r2 ordinary ID
`sha256:3921b6c3dee437775c219bd9d25293f89c063f7b2637061ec1bdb700716221fa`
and ROS ID
`sha256:55246b7b19aad36e74eb5687c205aef0630bb629968f50fba962bb31edacacf7`.
Their actual O3 ELF hashes are respectively
`2a3cfa640699637dd9bb489aa35e39ef3752e47f973b5f61cedb695e2af368dd` and
`2781300e7569c94f2b53d671d11ea43985c190fc696a27e68195a715a638daa2`.
These binaries still have the seven-argument bridge. Later development changes
require an actual new ELF and a fresh paired measurement.

No new formal P-core measurement was run for this audit. Hardware profiling is
restricted by `perf_event_paranoid=4`; instruction counts and source paths cannot
be converted into sampled percentages of execution time. Primary source commits,
URLs and downloaded byte hashes are in `GC_EH_SOURCE_PINS_20260928.json`.

## Costs actually present

| Path | Evidence and consequence |
| --- | --- |
| Guest ring | The actual WAT has `(struct (field i32))`, an immutable field. Its emitted unwind loop has 76 instructions and five helper call sites per iteration: `struct.new`, `table.set`, `table.get`, `ref.cast`, `struct.get`. The captured Wasm SHA is `d20970fb576326215a3eae0d9b9ccaed61872eae4099ccf246baa5872cf2c173`. |
| Generic aggregate bridge | Two calls per ring step each retain eight 16-byte zero stores in actual O3 runtime assembly: 256 bytes of bridge initialization per step, plus caller slot initialization. Removing those stores alone does not remove allocator, registry, table or cast costs. |
| Allocation | r2 `gc_object.h:713` performs a separate object allocation and a zero-initialized 16-byte-per-value array allocation. `publish:734` inserts the local hash bucket and object chain using two CAS loops, then inserts the foreign index under a stripe lock. |
| Identity issuance | Tokens are opaque, nonrecycling and never dereferenced as addresses. `issue_object_token:451` reserves 1,024 IDs into TLS at a time. Its process-wide CAS is approximately once per 1,024 successful allocations, not once per object. |
| Local versus foreign access | `checked_local_object:809` searches an exact local token bucket. A local hit bypasses foreign-owner `weak_ptr` promotion and foreign-lease retention. Those latter costs must not be blamed on the local ring. Older bucket entries can increase search work, but a newly allocated ring root is near the head; historical object count alone does not establish its lookup length. |
| Mutable fields | `struct_get_object:847` takes `object_lock` only for mutable fields. `struct_set:1744` checks mutability, type and reference ownership, then locks. Numeric fields do not require a foreign lease. Mutable field/array costs belong to their separate update workload. |
| Escaping numeric exception | The existing 8M-iteration WAT calls a separate numeric `$step`; every sixteenth step throws, for exactly 500K catches. It uses `catch`, not `catch_ref` or `throw_ref`, so it is not an exnref-retention benchmark. |
| Exception trace | r2 `uwvm_runtime.default.cpp:2773` performs an extra physical stack walk before C++ propagation, merges identities, allocates identity/frame vectors and copies display names. The physical cap is 64 frames. Short names may use string inline storage; do not count every name copy as an allocation without allocator evidence. |
| Exception payload | r2 `value.h:266` already moves the builder vector with `make_owned`; the old second payload copy is gone. `value_ref{new value}` and `diagnostic_trace_ref{new diagnostic_trace}` still have separate object/control-block allocation sites. Actual escaping throws still invoke trace capture at lines 18486 and 18584. |

Local numeric throws caught without reference inside the same function are already
lowered to a branch by both backends. That existing optimization is not a new
proposal, and its cost cannot explain the cross-function fixture above.

## What the primary implementations contribute

Wasmtime 49.0.1 is pinned to commit
`46c23a87dac1465986a8ad53ba6a7ae49372857b`. Its copying compiler emits an aligned
bump allocation, sends exhausted-space allocation to a cold collector call, and
marks live reference values for stack maps. It has no read/write GC barrier;
moving live objects instead requires complete updated roots. Its fields use their
actual storage widths. This explains a structural gap beyond merely combining
two `new` calls.
[Pinned copying implementation](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/func_environ/gc/copying.rs)

The pinned DRC compiler adds references entering the stack to an approximate root
set and may collect when its threshold grows. Reference writes increment the new
value before decrementing the overwritten value. This is a different throughput/
latency policy, not a free optimization; DRC does not reclaim cycles.
[Pinned DRC implementation](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/func_environ/gc/drc.rs),
[pinned collector policy](https://github.com/bytecodealliance/wasmtime/blob/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/wasmtime/src/config.rs#L3533)

The moving online API page currently identifies itself as `50.0.0-dev`, while
both it and the pinned 49 config source retain a stale “under construction” note
for copying. Qualification therefore uses the actual versioned CLI, exact
`collector=copying/drc/null` arguments and self-check output, not that note. A
Null collector does not reclaim and cannot pass the GC release gate.

OpenJDK `jdk-27+35` is pinned to commit
`815ff4dc327fe17f2433c7d115a5a503af10f3c4`. TLAB allocation checks space and moves
a thread-local top pointer. G1 also uses local allocation buffers with slower
refill/region paths. The transferable mechanism is bounded per-mutator allocation
with a correct slow path, not Java's benchmark number or object ABI.
[TLAB source](https://github.com/openjdk/jdk/blob/815ff4dc327fe17f2433c7d115a5a503af10f3c4/src/hotspot/share/gc/shared/threadLocalAllocBuffer.inline.hpp),
[G1 allocation source](https://github.com/openjdk/jdk/blob/815ff4dc327fe17f2433c7d115a5a503af10f3c4/src/hotspot/share/gc/g1/g1Allocator.inline.hpp)

Generational ZGC uses pointer metadata, load/store barriers and phased root
remapping. It requires considerably more protocol than removing a field lock.
Concurrent remembered-set refinement research likewise trades reference-store
barrier work against later tracing work. These are future pause targets after
exact roots and stop-the-world collection are correct, not the shortest safe
route for this implementation.
[ZGC barriers](https://github.com/openjdk/jdk/blob/815ff4dc327fe17f2433c7d115a5a503af10f3c4/src/hotspot/share/gc/z/zBarrier.inline.hpp),
[Detlefs et al., USENIX JVM 2002](https://www.usenix.org/legacy/events/javavm02/full_papers/detlefs/detlefs_html/index.html)

WAVM source is independently pinned to commit
`4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09`, not asserted to match the installed
nightly comparator. Its `ObjectGC` traces runtime objects within a compartment;
that is not proof of Core 3 struct/array syntax acceptance. Its exception path
also captures a stack before a C++ throw, but delays human-readable stack
description. Retain compact identities and defer name formatting; do not claim
WAVM eliminates the extra walk.
[ObjectGC](https://github.com/WAVM/WAVM/blob/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Lib/Runtime/ObjectGC.cpp),
[exception implementation](https://github.com/WAVM/WAVM/blob/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Lib/Runtime/Exception.cpp)

## Implementation order and required proof

1. **Integrate precise roots and safe automatic nonmoving collection first.**
   The existing opaque tokens allow storage reclamation without changing guest
   identity. Protect interpreter register rings, LLVM SSA/locals/operand inputs
   before they are popped, spill roots, calls/host reentry, tables/globals, tag
   payloads/exnrefs, extern wrappers and host handles. Admit and pause all relevant
   mutators/readers; include parked `atomic.wait` threads, module admission and
   teardown in the epoch protocol. Unwind/diagnostic records are not root maps.
   Cross-store strong cycles require graph tracing and lease pruning, not only
   dropping module roots. The explicit 16fcee closed-cohort component is one
   functional building block; its caller-supplied root array is not VM integration.

2. **Qualify the fixed five-argument bridge in the real emitter and ELF.**
   Preserve caller initialization, typed argument extents, status/trap ordering
   and the larger-than-eight-input fallback. Native fixed-body proof is insufficient.
   Run all 49 real LLVM callsite cases, the Core 3 positives/negative gates, roots
   under forced collection and O3 object inspection before a same-Wasm paired
   comparison with the seven-argument baseline.

3. **Combine object/field storage, then evaluate reclaimable allocation regions.**
   A single checked, aligned allocation can reduce allocator calls and metadata
   traffic. The previous small field regression and thermal drift still prevent
   unconditional adoption. A following region/slab candidate should give each
   mutator bounded refill space and reuse swept slots while keeping nonrecycling
   tokens and exact foreign membership. Test OOM, zero-length arrays, overflow,
   32-bit alignment, destructor/lease paths, 1/2/4P contention and 64-store switching.
   Work queues and metadata must scale with current live storage, not all issued IDs.

4. **Make numeric storage compact and optimize typed access only after proof.**
   Precompute field offsets/widths from the validated type layout: packed i8/i16,
   i32/f32, i64/f64, v128 and references must retain their own semantics. Keep the
   bridge value ABI initially. A cached native object pointer is usable only with
   an issuer pin and a live-root/mutator epoch; no guest token becomes a pointer.
   Measure immutable and mutable access separately. Lock elision requires a
   proven exclusive/single-mutator capability, not an assumed single-thread guest.

5. **Compact exception diagnostics without changing catch/rethrow semantics.**
   Capture resolved function/generation identities once and pin immutable display
   metadata independent of the GC store; format names only when diagnostics are
   displayed. Preserve original throw identity, truncation and module/hot-replacement
   lifetime. Consider an inline small payload and coallocated shared ownership
   after allocation-count evidence, preserving large tuples and reference roots.
   Test long names, unload, catch/rethrow, `throw_ref`, mixed interpreter/JIT stacks
   and colored uncaught output.

6. **Investigate reuse of the native search walk only as a separate ABI project.**
   The Itanium ABI has search and cleanup phases with personality callbacks.
   Capturing diagnostics from the search phase could avoid the preceding backtrace,
   but requires real C++ ABI compatibility, cleanup/root correctness and a distinct
   Windows unwinding design. It must not mutate the unwinder's private state or
   remove frames needed on later rethrow. Keep the current safe path until measured
   platform evidence exists.
   [Official exception ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html)

LLVM statepoints/stack maps can describe supported live references at a safepoint;
they do not implement collection. The current 16-byte integer reference carrier
cannot be treated as a GC pointer merely by enabling a pass. Full/lazy/tiered OSR
entries and exception/tail exits require explicit root/frame ownership.
[LLVM statepoint design](https://llvm.org/docs/Statepoints.html)

## Next actual paired profile

Use exact frozen source/build/runtime/ELF IDs and the same Wasm SHA on both products
and accepted Wasmtime collector configurations. Perform nine reversed pairs on
P0, with all compiler/QEMU/Windows work absent. Keep 4P cells separate. Use counts
whose guest interval is at least 100 ms and independently verify every checksum.

The priority cells are: immutable ring; immutable versus mutable field reads with
the same retained 1,024-object graph; dynamic set/get; allocation/publication; and
the five actual ring helpers. Inspect loop bodies and generic/fixed bridge code
before interpreting slopes. Allocator call/byte diagnostics must be a separate
instrumented run, not used as uninstrumented throughput. If permitted profiling
is unavailable, report that limit and do not manufacture cycle attribution.

For exceptions use the existing same-Wasm `plain_normal`, `eh_normal`,
`eh_throws` and uncaught correctness fixtures, recording instruction/unwind modes
and actual 500K throw count. Fresh r2/r3 binaries are required to evaluate
`make_owned` or compact traces; the older samples cannot do so. VM spawn/join,
guest rendezvous and task-state-qualified parked wake remain separate thread cells.

Record every pair's CPU IDs/topology, governor/frequency, available temperatures,
cgroup CPU throttling and memory events. The previous 68–97°C drift prevents a
small single-pair gain from supporting adoption. Repeat reverse rounds within a
comparable thermal range; do not change host controls without recording them.

The release gate still requires all four ordinary/ROS JIT/int cells, actual
`collection_count > 0`, forced-root survival/cycle tests and its default RSS growth
limit of 64 MiB from 1M to 10M allocations. A subset, a native explicit sweep,
or zero-collection allocation throughput cannot print overall PASS. Count actual
collections and report live bytes, RSS, total allocation and enough pause events
for any claimed p95/p99; nine process samples alone are not a service p99.
Java/Graal, Node and .NET remain analogous workloads in a separate table.
