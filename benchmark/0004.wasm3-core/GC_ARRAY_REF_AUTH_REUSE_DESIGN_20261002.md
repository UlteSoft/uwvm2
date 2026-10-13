# Same-operation local reference authentication for array.set

Status: read-only source design, 2026-10-02. No production source, frozen source packet, compiler, VM, profiler or remote state was changed. The implementation and performance qualification below are pending.

The next small candidate is to remove the second authentication of an ordinary local aggregate reference during one mutable reference-array write. Keep the current destination lookup, layout/bounds/mutability checks, canonical reference check, publication acquire, object mutation lock and final store. Capture the first source authentication privately; it supplies no executable authority and is discarded before array_set returns.

## Current source and evidence

In src/uwvm2/uwvm/runtime/storage/gc_object.h, array_set at line 4731 looks up the destination, checks index/layout/mutability, invokes value_matches at line 4741, then retain_embedded_reference at line 4742. The reference verifier at line 1536 authenticates a struct/array source through checked_object (or checked_aggregate_object when compact support is enabled), then compares the actual owner/type with the field's expected canonical type. Retention at line 1666 authenticates the same source again before immediately returning ok for source.owner == destination.owner. checked_local_object at line 2041 reads the published local chain with acquire ordering, compares the opaque token and kind, and never dereferences the guest token.

The complete S6e VTune packet, archived as build/wasm3-evidence/current-general-gc-vtune-complete-20261002-r1/actual-small-packet.tar.gz, has SHA256 44b539bfdf05f1a31d6d364e3286cb749d60e7157914c9302c6822e0fa8625c6. Its filtered reference-array allocation sample reported checked_aggregate self 0.119048 s and retain_embedded_reference self 0.025792 s of 2.000932 s sampled CPU time. These are sampled self costs, not inclusive costs or pure collector latency. The higher get_thread_state sample came from the actual compatibility profile with UWVM_USE_THREAD_LOCAL absent in runtime/main/host argv. The native TLS branch is direct thread_local storage; the map helper is compiled only in its alternative branch. Those old proportions cannot rank the next aligned native-TLS production build. The fresh TLS profile must decide whether this candidate deserves implementation.

## One change, without an extra speculative query

Have the private verifier used by array_set optionally capture an ordinary local source object as a by-product of its original membership query. Prefer an inline/template internal result so existing callers that request only bool retain their old interface/code path. Initialize the private output to null before verification; publish a non-null result only after the complete reference/canonical condition succeeded, after proving the queried node is an ordinary published member of the invoking store.

array_set requests that proof only after its actual destination has passed the original checks and destination.owner == this. A non-null proof permits skipping the same-owner retention lookup. The source owner must be this, not merely structurally/canonically equivalent to this or equal to a foreign destination owner. All other cases receive a null proof and run the existing retention operation. Do not probe the local chain separately and then redo verification on a miss: that would add a third query for foreign values.

Expected query counts, assuming a valid reference field:

| Case | Existing source authentication | Candidate |
| --- | --- | --- |
| Ordinary destination and source owned by invoking store | Match lookup + retention lookup | Match lookup once; private proof reused |
| Foreign destination or foreign source | Original match lookup + retention lookup/leases | Same sequence |
| Compact source | Original match and retention route | Same sequence |
| Null, i31, function, extern or exception source | Original kind-specific path | Same sequence |
| Failed match/type/kind/token | Original invalid_value before retention/write | Same failure order, proof remains null |

This first candidate changes array_set only. Keep struct_set, array_fill/copy/init, constructors and allocation paths unchanged, so its effect is attributable. Do not retain an authenticated source across allocation_poll, allocation, callback, safepoint, domain transition, collection, lease release or another operation.

## Lifetime and synchronization proof

The runtime_execution_entry_scope acquires the genuine managed GC shared lease at default.cpp line 7743 and execution generation lease at line 7746. Their member lifetimes cover native cleanup; reset closes admission and drains admitted execution before resources retire. managed_collection::allocation_poll obtains exclusive(1) only at the existing allocation threshold and validates the closed initialized cohort before census/sweep. Administrative readers/writers also own counted shared leases.

The proposed proof is created after the bridge's existing managed_page_boundary and stays inside the original synchronous store call. There is no allocator, VM poll, guest callback, weak-owner promotion or new lease insertion between local source lookup and final write. canonical_subtype keeps its existing immutable local/ID checks and registry guard when needed. object_lock still performs the original acquire spin/release around the destination carrier write. Concurrent field mutators cannot bypass that lock. A pending reset still waits for the actual outer generation lease. The optimization neither adds an independent root nor replaces precise activation roots with this borrowed pointer.

The caller must still obey the existing gc_object_store native-reader contract: pin the owner and exclude collection/teardown through immediate access. The optimization does not mint that contract from TLS state or an active-count guess. In a public native use without real admission/lifetime protection, both old and proposed borrowed accesses are outside the existing API contract; do not test by deliberately violating it.

Foreign lookup and embedded retention remain separate: the receiving module lease protects a borrowed foreign source, while the destination object's value_leases protects its embedded foreign graph if the module unloads first. Do not collapse them. Exceptions retain the native/external census and deferred-retirement rules. Do not generalize the proof to weak references or collector-local authority.

## Source and cache boundary if approved

A small implementation should be limited to private verifier plumbing plus array_set in gc_object.h, and one explicit LLVM cache/build identity key in runtime/llvm_jit_cache/environment.h. Any first experimental gate must use exact == 1, be default off, have a versioned local-proof key and keep the original off text projection. All runtime/CLI/compiler/host TUs must have identical macro values and native TLS selection; do not mix the S6e non-TLS RT with a fresh native-TLS consumer. Do not change the existing 13-overlay collector experiment, R5/R3 packets or their manifests. A privately included implementation fragment would not add a public source/module API.

No generated bridge ABI, JIT symbol name, validation body scan, public carrier layout, root frame, NoTail/Win64 context, status trap mapping or runtime output changes are needed for this candidate. Before images and a source diff should show only this single optimization. An implementation must document every new borrowed pointer assignment in the project's safety diagram style.

## Minimum useful qualification, after fresh TLS attribution

1. Native semantic tests under actual shared admission/owner pins: nullable and non-null fields; local struct/array sources; declared subtype and equivalent canonical type at a different index; wrong canonical type/kind; forged/stale/outside token; foreign source and foreign destination in both same/different stores; recipient module retirement while an embedded object remains; immutable and out-of-bounds destination precedence; actual OOM foreign lease status. Assert unchanged destination/readback on every failure. Retain existing exception/collection tests, including invalid later root with zero reclaim.
2. Emit actual host O3 IR and disassembly for the fixed opcode-14 bridge and a parsed/initialized Wasm reference-array mutation loop. The owned-local success path must have one source membership walk, the full canonical check and one acquire/release destination lock; no guest token inttoptr load, no extra VM/TLS accessor, no allocator/poll between proof and write. A foreign path must still show the original lease route. The optimizer may inline loops, so count CFG/data provenance rather than function-name occurrences alone.
3. A/B use one fresh aligned native-TLS baseline and one candidate differing only in the reviewed source/gate/key. Keep six experimental settings, collector-local gate, ABI, roots, Wasm bytes, runtime policy, LLVM/flags and SDK closure equal. First use the existing same-byte reference-array-mutate 1M/2M fixtures: exactly 2n array.set reference operations, initialization 3072 allocations, zero collection attempts/reclaims and phase_pending reason2; this is field/lookup qualification, not collector qualification. The allocate counterpart performs 6M allocations at n2M and must retain positive collection/reclaim, roots_requested=1, disabled=0/reason0 and the exact checksum/retained graph contract.
4. Use at least an ABBA diagnostic rotation with actual execution timer, parent wall/user/sys/RSS and frequency distribution. Whole-process hardware cycles/instructions (exact enabled == running, actual P0/owned PID/TID) and VTune sampling/uarch are separate runs and ROIs. Temperature is observed only. Check foreign-array/reference controls for regressions before considering promotion. Do not divide whole-process counters by n and call them pure array.set or collector cost.

## What primary implementations permit us to learn

[Wasmtime 49.0.1 pinned copying compiler](https://raw.githubusercontent.com/bytecodealliance/wasmtime/46c23a87dac1465986a8ad53ba6a7ae49372857b/crates/cranelift/src/func_environ/gc/copying.rs) emits checked inline bump allocation and a cold allocation libcall. Its no-barrier reference operations depend on its own stopped moving heap and precise relocatable stack maps. That ownership model does not authorize removing uwvm foreign-token authentication or mutable synchronization.

[OpenJDK 27 G1 barriers](https://raw.githubusercontent.com/openjdk/jdk/jdk-27-ga/src/hotspot/share/gc/g1/g1BarrierSet.inline.hpp) read a card-table identity once for its related accesses and preserve SATB/weak-reference conditions. [Shenandoah 27 barriers](https://raw.githubusercontent.com/openjdk/jdk/jdk-27-ga/src/hotspot/share/gc/shenandoah/shenandoahBarrierSet.inline.hpp) preserve phase-dependent forwarding, weak/phantom liveness and SATB handling. The applicable principle is operation-local reuse under real ownership/phase proof, not eliminating those protocols.

The locally checked WAVM commit 6f871e61c6e8fc54fa5317b5d61d128d681846f3 [compartment collector](https://raw.githubusercontent.com/WAVM/WAVM/6f871e61c6e8fc54fa5317b5d61d128d681846f3/Lib/Runtime/ObjectGC.cpp) uses explicit root references and compartment-owned scanning with synchronization. It is host-object GC source, not evidence that it implements the current Wasm3 aggregate benchmark.

[The primary Immix paper](https://www.steveblackburn.org/pubs/papers/immix-pldi-2008.pdf) separates mutator locality, reclamation cost and pinned-object treatment. Region/slab or arena changes could later address different costs, but they are broader than removing duplicate membership authentication. None of that paper's measured speedups applies to this unimplemented candidate.
