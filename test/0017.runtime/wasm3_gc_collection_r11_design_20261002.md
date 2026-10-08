# r11 GC root and compact-mark proof — 2026-10-02

This is an audit/design artifact with the subsequently authorized minimal A/B implementation. The remote r10 snapshots were not edited. No local compile, VM, WAT tool, benchmark, or SSH run was performed. The new r11 candidate must receive a distinct source receipt and remote qualification. Current r10 Linux success and a pending Windows qualification do not qualify the r11 candidate.

## Scope and source identity

The two repositories had these byte-identical r10 GC headers at the audit boundary; the table identifies the exact before-image for the line references below:

| Relative file | SHA-256 |
| --- | --- |
| src/uwvm2/uwvm/runtime/storage/gc_object.h | 209de02c372a7c082be0aada2760699b6a543631fa9e280b4263ebae528ea456 |
| src/uwvm2/uwvm/runtime/storage/compact_numeric/descriptor.h | aa290fd3ba2ed501996594d73f64967f0224ec902d0ba86cba77c171ba3c9894 |
| src/uwvm2/uwvm/runtime/storage/compact_numeric/collection_directory.inc | f19ff82466cf577d4401cf2fcad97b8ecb7061b8804c2c81a3b44414f28e5b0d |
| src/uwvm2/uwvm/runtime/storage/gc_exception_graph.inc | cbd3f8894b1261667f6aec8ba17ce57217a9deed7d2b00963e6acc7a996695a4 |
| src/uwvm2/runtime/gc/collection_transaction.h | 84ff4775f3f1b3e406aa3370b47f7af2dbf9cc0bd8f9114021fa0d97602f2760 |

Line numbers below refer to these audited bytes, not to lambda ordinals from another executable. The parent reports an actual hardware-profile collector self hotspot. This audit has not inspected that report's binary/source attribution, so it does not claim which current lambda produced those samples.

## Repeated operations and the minimal change

In the compact branch, gc_object.h:2697-2698 validates each root using validate_reference; :2735-2736 immediately passes those same immutable snapshot roots to mark. Both paths resolve aggregate tokens through locate. Legacy :3067-3068 and :3098-3099 repeat the same operations.

For a compact root with collection_directory enabled, locate_compact_directory (collection_directory.inc:86-123) resolves an exact opaque integer token to one authenticated descriptor and live slot. mark then calls mark_token_while_stopped (gc_object.h:2721); descriptor.h:290-298 calls the stopped locator again. Thus this root currently performs two directory lookups and three stopped slot authentications. With the directory disabled, two store/segment searches precede the additional stopped authentication.

A removes the successful-path standalone root-validation loop and retains the existing error-returning mark loop. Every root is still authenticated once. All heap objects and immutable exception values still undergo their existing graph preflight before this loop. Nothing is freed until roots and the transitive mark worklist have succeeded.

B uses the exact compact_slot returned with the exact descriptor by that just-completed locate. Add a private collector-only mark operation that checks slot < capacity, slot < reserved_width, and therefore slot / 64 < marks.size(), then ORs the bit. It need not repeat token lookup, phase/frontier/live/readers loads in this uninterrupted stopped interval. Alternatively, a store-member local helper may write the bitmap directly through the existing friend boundary; this exposes no new descriptor API.

No new data member, public capability, guest ABI, native function ABI, cache schema, write barrier, persistent lookup cache, or allocation is needed for A/B. Both collector branches need A; only the compact branch needs B. The generated interpreter/JIT guest memory-access path is outside this change.

## Error ordering and failure guarantee

The old root loop runs before the object work-array allocation. Moving authentication behind that allocation would alter the result when an invalid root and work-array OOM occur together. Preserve the old precedence in the exceptional allocation-failure path:

    preflight every legacy object, compact descriptor, and immutable exception value
    count bounded legacy work entries
    attempt the existing bounded work-array allocation
    if allocation fails:
        validate every snapshot root using the original predicate
        if a root is invalid: return invalid_reference
        return out_of_memory
    for each root:
        if mark(root) fails: return invalid_reference
    drain all aggregate/exception work
    validate every store epoch transition
    commit epochs, prune leases, unlink, sweep, retire

The rare OOM path may repeat authentication; the successful path does not. Existing compact-directory or exception-registry allocation failures retain their earlier ordering. Do not claim invalid roots precede every possible collector allocation failure.

Null nonempty native spans and PTRDIFF bounds remain checked before any indexing. Out-of-domain, forged, stale, and wrong-kind tokens remain invalid_reference. A late invalid root can leave marks set on previously visited objects, exactly within the existing failure contract: reclaimed remains zero, tokens/live bits/frontiers/epochs/leases/registries remain unchanged, and no object is freed. The next collection resets legacy marks and every compact bitmap before using them. An OOM or validation error never continues into commit.

The bounded worklist remains sized by actual live legacy headers. Each header is enqueued once through its mark bit. B marks zero-edge numeric leaves without adding work. Retain work_count/object_count checks and all size-overflow checks; do not replace an impossible overrun with unchecked writes.

## Authority and borrowed slot proof

Neither a non-null descriptor nor an integer slot is authority by itself.

The caller must still retain its real execution-generation lease and every canonical store pin, close native entry admission, stop every enrolled guest/native mutator, and maintain the complete precise/static/native root snapshot. collection_transaction.h:80-89 states that contract; :117-141 requests, waits for, and commits the actual pause. The two-pass root census/copy at :194-242 remains unchanged, including count agreement. It proves snapshot extent/stability, not heap-token membership.

gc_object.h:2490-2520 authenticates canonical shared_ptr control blocks, store validity, unique pins, the complete cohort census including empty stores, and absent unenumerated bridge roots. The legacy branch retains the corresponding :2918-2955 checks. Partial cohort membership cannot authorize either A or B.

Compact descriptor preflight at :2635-2668 authenticates the actual store-owned strong chain, canonical control block, type/layout/canonical ID, scalar representation, payload capacity, frontier, token geometry, published phase, and readers == 0 through begin_marks_while_stopped. No admission lock or readers count alone substitutes for this closure.

With the directory enabled, records are built from those actual store-owned chains, not from guest pointers. The builder confirms published phase, control-block ownership, exact head/chain count, representable reserved ranges and disjoint sorted intervals. The locator chooses the record using opaque integer keys and authenticates that descriptor's actual live slot. Descriptor and compact_slot are returned together in aggregate_object_view. B must consume that same pair immediately. An imported/foreign root may legitimately belong to another pinned store in the same closed cohort; do not infer ownership from the consuming Wasm module.

Without the directory, checked_local_compact_object (:575-605) visits each canonical owner's real native chain, checks the token's actual descriptor geometry/frontier/live bit, and returns that exact descriptor, owner and slot. Earlier begin_marks proved readers == 0; the genuine exclusive pause prevents a new reader, append, reset or retirement throughout lookup and marking.

Descriptor capacity is immutable in this interval and reserved_width is 1024. A located slot is below frontier <= capacity <= 1024. Hence slot / 64 is in the 16-word marks array, and slot % 64 defines a valid shift. Keep explicit scalar/array bounds in B as a defense against accidental trusted-native misuse. Document each bitmap/native-array access with the complete bound and lifetime proof.

The existing descriptor already has gc_object_store, compact_numeric_reader and optional sealed_compact_entry friends. Do not add friendship or expose B publicly. The proposed operation's sole new call site is the store's closed collector; existing friends are trusted implementation code and are not new guest authority. Prefer a store-local helper if compiler-enforced absence of any additional descriptor API is desired. Do not call B from sealed guest cursors, ordinary readers, generated entry helpers, a boolean 'stopped' request, or across poll/call/collection boundaries.

## Checks kept once, checks kept for every new edge

Keep each canonical pin, store validity, whole-cohort census, bridge/exn registry closure, descriptor/layout preflight, stopped reader state and epoch transition proof. They can cover the same immutable collection interval; they cannot be removed.

Keep whole-heap visit_fields + validate_reference at :2679-2690 and legacy :3048-3060, including unreachable objects. Current behavior rejects malformed dead edges/layouts before any reclamation. A/B does not change that contract to reachable-only validation.

Every previously unseen root/aggregate edge/exn edge must still be resolved against genuine membership and checked runtime kind. Never reinterpret an opaque token as a native header. Legacy locate (:2558-2585, :2986-3006) retains the global stripe lock, token identity, actual kind, and authenticated in-cohort owner check. No broad 'threads stopped, remove all global locks' change follows from A/B.

checked_type (:1460) already performs bounds/kind checks and one store-owned array address. visit_fields repeats it for heap preflight, reachable trace and retained-object lease pruning. These operations are real costs, but changing trace-work-item shape or object ABI is outside the smallest revision. Numeric/packed arrays still have checked width/byte-tail bounds and no invented reference edges.

Lease pruning (:2780-2805; legacy :3140+) remains based on current validated field owners, not prior overwritten leases. It runs before compact directory invalidation/retirement and requires fresh safe ownership lookup. Keep directory invalidation at :2854 before any descriptor is unlinked or freed.

## Original exceptions and concurrent counterexamples

gc_exception_graph.inc:141-305 retains the locked complete registry/recipient membership proof and owning immutable exception values. :319-341 validates every registered and native payload; :360-388 marks and traces actual entries. Membership/root_count never becomes reachability. Neither A nor B clones, reidentifies, clears or recaptures an exception diagnostic trace.

An old exnref carried by a newly caught non-ref wrapper remains an independent graph edge to its old immutable value and original throw trace. Aggregate <-> exn cycles need joint marking. A root whose kind is wasm_exn must continue through graph.mark; it cannot be treated as a numeric compact slot, a raw entry pointer or an automatically live native shared_ptr.

Concurrent readers not parked/drained, module admission during census, outside-cohort teardown, an incomplete static visitor, a root-copy count mismatch, or timeout must prevent collection. A/B cannot turn readers == 0, no registered native roots, an empty frame list or a matching module name into a complete-pause proof. No borrowed view may survive the callback or its canonical pins.

## Official collector comparison

Wasmtime v49 implements Cheney semispace copying. Its process_roots (lines 951-966) immediately forwards each root; forwarding reuses the existing copied-object record. scan (540-593) reads inline tracing metadata, using a struct reference bitmap or an array element-reference flag, with an out-of-line fallback. This supports combining root work and reusing authenticated metadata. Its object movement, forwarding headers and root rewrites require a different representation from our stable opaque tokens and native exception ownership, so they are not an A/B implementation recipe. [Official Wasmtime copying.rs](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.0/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs)

A later layout-cache experiment must be owner/lifetime-bound: a recycled type index alone is insufficient. Wasmtime's documented DRC pooled-heap stale trace-cache issue caused a host bounds panic after type-index reuse. Use it as a cache-lifetime regression example, not evidence of a defect in uwvm2. [Official Wasmtime issue 13417](https://github.com/bytecodealliance/wasmtime/issues/13417)

If later hardware evidence identifies repeated membership lookup, consider a fixed bounded collector-local successful-lookup cache keyed by exact opaque token AND runtime kind, minted only after the original lookup succeeds. Lifetime is one uninterrupted closed pause; reset before epoch commit/sweep. Do not cache failures, source permission, native entry addresses or issued tokens across collections. This is a separate experiment, not required for A/B.

## Remote acceptance and new adversarial cases

Use the identical qualified P core, 64 GiB cgroup, source-bound O3 builds and existing GC checksums. Separate VM GC/end-to-end ring timings from native component collection, root-readback timing and process startup. Measure allocation/collection separately; include root count 0/1/1024, duplicate roots, segment-boundary roots, many segments, legacy mutable scalar/reference graphs, compact immutable scalar leaves, and mixed stores. Capture median/spread/RSS and generated assembly. Static operation counts predict fewer authentications; no speedup is claimed yet.

Add or extend these remote component/VM regressions:

| Case | Required result |
| --- | --- |
| Valid early roots followed by a forged late root | invalid_reference; reclaimed 0; earlier live/dead tokens still resolve; subsequent valid collection succeeds. |
| Invalid root plus injected legacy work-array OOM | invalid_reference preserves old precedence; valid roots plus same fault give out_of_memory; no commit. |
| Null span with nonzero count or checked-count overflow | Reject before indexing/allocation. |
| Compact reserved-but-unissued slot; frontier slot; swept live-bit slot; wrong kind | Reject; mark helper never receives an unauthenticated view. |
| Same slot index in two descriptors/stores with different token ranges | Only the root's authentic descriptor is retained, including cross-store roots. |
| Partial cohort, duplicate pin, fabricated aliasing control block, outside owner | Reject before marking/commit, even for an empty store. |
| Dead unreachable object with malformed edge/layout via privileged test fault | Reject whole collection; do not hide it through reachable-only tracing. |
| Numeric/packed/v128 bytes resembling a token | Not an edge; byte bounds and scalar bits remain intact. |
| Aggregate/exn cycle; original exnref as wrapper payload; registered native exn owner | Preserve reachable old trace/value; reclaim only when real roots disappear. |
| Busy compact reader, pause timeout, late participant/admission, static count/copy mismatch | No collection authorization or reclamation. |
| Repeated collection after late validation/OOM failure | Reset stale marks; correct surviving/dead set; no token ID reuse. |
| Final empty descriptor retirement, then another collection/type instantiation | No dangling directory/view and no reused layout proof. |

Faults inaccessible to safe Wasm must use privileged component fixtures; do not add guest-callable corruption hooks. Existing gc_explicit_sweep_ring and gc_single_block_collect_edge_cases provide fault-injection/foreign-cycle scaffolding. Preserve their distinction from VM-qualified automatic root enumeration.

Implementation should remain one synchronized A/B patch in both repositories. Remote official semantic tests plus current whole-VM GC, exception ownership and native-thread pause regressions are required before attributing any improvement to it.

## Implemented candidate, awaiting remote qualification

The authorized r11 implementation changes only the two production headers below, synchronized byte for byte. The original non-null/span bounds, canonical cohort admission, whole-heap and exception preflight, descriptor directory authentication, epoch commit, lease pruning, sweep and exception retirement are unchanged. Reconstructing their before-images from the A/B diff reproduces both exact r10 SHA-256 hashes in the table. This distinguishes the small revision from preexisting untracked source work.

| Production candidate | SHA-256 |
| --- | --- |
| src/uwvm2/uwvm/runtime/storage/gc_object.h | 55cb0d40fb264ba9b825f551d2de16c1f79b0cc9db2eb08de092651ef29ee036 |
| src/uwvm2/uwvm/runtime/storage/compact_numeric/descriptor.h | 9a770293c14aa2fd7406af8dff5b4b3c0d6d34a4f9511b3f0b2b546872dffb62 |

The new private method checks capacity, reserved width and actual marks-array bounds. Its sole new collector caller passes the just-located descriptor/slot pair; the old private token-marking method still performs its stopped locator and delegates immediately afterwards. No friendship, public method, data member or guest-path caller was added.

Two existing component fixtures were minimally extended, preserving their success/reclaimed-count contracts:

- benchmark/0004.wasm3-core/gc_explicit_sweep_ring.cc: duplicate and explicit foreign roots, valid roots preceding a late forged or dead root, failed-collection object readbacks, invalid-root/work-OOM precedence and separately injected valid-root OOM. The fault test allows an implementation to reject roots before allocating; it does not assert one specific internal ordering.
- test/0019.gc_statepoint/compact_collection_directory.cc: genuine duplicate roots, a direct foreign compact root, late wrong-kind and stale roots after valid compact marks, unchanged epoch and surviving scalar-bit checks. Its existing 65 actual descriptor ranges, cycle/array graph, directory OOM and final retirement checks remain intact.

All four changed source/test files are byte-identical between repositories. Only static inspection, exact-before-image reconstruction, new-line whitespace checking, and Python AST parsing of the separate EH runner were performed. Neither the C++ implementation nor these fixtures have been compiled, executed, benchmarked, or qualified on a target platform in this subtask.
