# General allocator/collector study and next hypotheses

This is a read-only design study. No production representation, synchronization,
whole-graph preflight, registry or token-ownership rule is changed. The frozen
r11 experiment concerns its two reviewed headers and the immutable numeric
case; the [general-family plan](GENERAL_GC_FAMILY_PLAN_20261002.md) separately
covers the default mutable/reference/array paths. A new collector is not an
accepted optimization until its actual semantics and measured costs close.

The smallest metadata hypothesis now has a separate, default-off
[source candidate and cold contract](../../test/0019.gc_statepoint/gc_trace_metadata_candidate_20261002.md).
Its presence is not a native/VM validation or measured speedup receipt; the
frozen two-header experiment and default/six-switch profiles stay distinct.

## Primary sources pinned on 2026-10-02

Official GitHub commit metadata read through the API resolved Wasmtime `main`
to `e2a58746c83390954f225bb7e63f4e4204c4833f`, committed
2026-10-01T20:46:22Z. Raw bytes were then independently fetched by exact SHA.

| Source | Actual bytes | SHA-256 |
| --- | ---: | --- |
| [Wasmtime copying.rs](https://raw.githubusercontent.com/bytecodealliance/wasmtime/e2a58746c83390954f225bb7e63f4e4204c4833f/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs) | 46,964 | `c999bbc4bd3cf5e30030d8d93f351813285af891ff779c660d686d97aae483e5` |
| [Wasmtime drc.rs](https://raw.githubusercontent.com/bytecodealliance/wasmtime/e2a58746c83390954f225bb7e63f4e4204c4833f/crates/wasmtime/src/runtime/vm/gc/enabled/drc.rs) | 49,688 | `295bb2bf6412db481aa16bcca2e539cdb2c31d624afb99f618426ad977a015bb` |
| [OpenJDK25 TLAB](https://raw.githubusercontent.com/openjdk/jdk/6c48f4ed707bf0b15f9b6098de30db8aae6fa40f/src/hotspot/share/gc/shared/threadLocalAllocBuffer.inline.hpp) | 3,807 | `67eddfd13783c30f759455ff64143678192dfc805b1390891793e30daa079883` |
| [OpenJDK25 G1 barrier](https://raw.githubusercontent.com/openjdk/jdk/6c48f4ed707bf0b15f9b6098de30db8aae6fa40f/src/hotspot/share/gc/g1/g1BarrierSet.inline.hpp) | 6,050 | `8503a65c86b2771440b304a96f53370a5d7264733707b8a7a6634285b45da0c9` |

OpenJDK `jdk-25-ga` resolved to `6c48f4ed707bf0b15f9b6098de30db8aae6fa40f`,
committed 2025-08-12T17:15:36Z. These are source-study pins, not the build of
the installed Wasmtime49 control and not executed Java benchmark binaries.
Wasmtime v49.0.1 `copying.rs`/`drc.rs` were also read through their release
URLs. The pinned current source, rather than a stale "under construction"
documentation note, determines the design description below.

Wasmtime copying uses two equal semi-spaces, bump allocation and a Cheney
worklist within the copied range; roots and outgoing references are updated.
Its current implementation requires no read/write GC barriers and specializes
reference scanning using small struct bitmaps or array reference flags.
Its byte-range/type corruption checks still exist; "no barrier" is not an
authorization to remove our carrier authentication. Its tracing metadata and
heap-index representation differ from our public opaque tokens. These are
useful locality/allocation principles, not proof that copying can be pasted
into this store safely. [Pinned copying source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/e2a58746c83390954f225bb7e63f4e4204c4833f/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs).

Wasmtime DRC uses ordinary host reference counts and deferred activation-root
tracking so local transfers need not count every stack operation. Reference
writes update counts; allocation uses a free list. The current source explicitly
has no tracing cycle collector. A bounded cold success of our cyclic fixture
under DRC is therefore a semantic check, not a cycle-reclamation baseline or
fair memory-bounded industry comparison. [Pinned DRC source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/e2a58746c83390954f225bb7e63f4e4204c4833f/crates/wasmtime/src/runtime/vm/gc/enabled/drc.rs).

OpenJDK's TLAB fast allocation bounds the available thread-local range before
advancing its top; refill is a separate path. G1 reference access includes a
SATB pre-barrier during active marking and further access/barrier integration.
The useful lesson is to amortize reservation while keeping a complete collector
contract, not to bypass a mutable reference write or presume an object is
thread-local because the current benchmark is single-threaded.
[TLAB](https://raw.githubusercontent.com/openjdk/jdk/6c48f4ed707bf0b15f9b6098de30db8aae6fa40f/src/hotspot/share/gc/shared/threadLocalAllocBuffer.inline.hpp),
[G1 barrier](https://raw.githubusercontent.com/openjdk/jdk/6c48f4ed707bf0b15f9b6098de30db8aae6fa40f/src/hotspot/share/gc/g1/g1BarrierSet.inline.hpp).

[Immix, Blackburn and McKinley, PLDI2008](https://www.steveblackburn.org/pubs/papers/immix-pldi-2008.pdf),
DOI `10.1145/1375581.1375586`, combines contiguous block/line allocation and
reclamation with optional movement to reduce fragmentation. A nonmoving first
stage can retain stable native headers; line/block geometry and metadata must
be charged to memory, and its original parameter choices are not universal.
Its performance claims are not our measurements. We should borrow the
separation of local fast reservation and coarse shared refill, then measure
our actual object sizes and occupancy before choosing geometry.

[LXR, Zhao, Blackburn and McKinley, PLDI2022](https://www.steveblackburn.org/pubs/papers/lxr-pldi-2022.pdf),
DOI `10.1145/3519939.3523440`, combines field-logging/coalesced RC, deferred
root processing, lazy decrements, concurrent SATB tracing for old cycles,
remembered information and selective copying on an Immix heap. It is a useful
later direction for mutation-heavy/low-survival workloads. Adopting only
deferred counts without its cycle and barrier mechanisms would break the new
cycle workloads. It is not a narrow replacement for the current collector;
exception graphs, foreign leases and host mutation must participate in every
relevant barrier before a concurrent trace can be sound.

## Actual current uwvm store paths

The following observations concern checked-in code read during this study;
they are source hypotheses, not a new profile of the frozen r9 executable.
Production path names refer to `src/uwvm2/uwvm/runtime/storage/gc_object.h`.

* `compact_layout` admits only one immutable unpacked i32/f32 field. It rejects
  mutable fields and every reference-bearing layout. Do not extend that
  descriptor casually to mutable fields.
* `numeric_slab_eligible` already admits bounded numeric-only structs,
  including mutable i32. `reserve_numeric_slot` handles reservation/generation
  state. Saying every default mutable struct calls `new[]` is incorrect.
  Reference-bearing structs and arrays use the other bounded byte-array
  allocation path. Numeric arrays may use a packed byte tail under its existing
  experimental switch; that is not immutable compact-object admission.
* Legacy object headers retain their real owner, opaque nonrecycling token,
  kind, canonical type index, initialized payload span, local/global links,
  mutation lock and foreign-value leases. `publish` issues the token and
  publishes the initialized object through membership, object-chain and global
  indexes before exposing its guest carrier.
* `checked_object` authenticates local membership and only then takes the
  foreign slow path. Mutable reads and writes take `object_lock`. Reference
  writes additionally check value/heap compatibility and retain the referenced
  store before changing the field. A foreign object requires its actual owner,
  not the consumer's type carrier guessed as authority.
* Closed-cohort collection excludes mutations/readers/teardown, authenticates
  the complete cohort/registry, prevalidates every live object's layout and
  reference edges, prepares bounded mark/work storage, authenticates roots,
  traces and checks epochs before any reclamation. Sweep removes registered
  tokens and prunes only unneeded foreign leases under the existing proof.
  Invalid roots/edges, OOM and generation exhaustion must still leave tokens
  and object lifetimes safe. Epoch advancement invalidates cached reservations.

## Ranked hypotheses and required evidence

1. **Establish the actual general-family baseline first.** The preserved r9
   hardware counter is whole-guest startup/JIT+execution, with genuine grouped
   cycles/instructions and full time-running closure. The r9 GC sampled head
   names include collector lambdas but most sampled weight has unresolved JIT
   mapping; r10's byte-identical finalized hot loop is cold code evidence.
   Neither maps the default mutable/reference paths or proves which general
   allocator/collector phase dominates. The historical ~117 ns versus ~9 ns
   figures are whole-workload observations from different profiles. Measure
   default and experiment6 using identical new bytes before assigning priority.

2. **Precise immutable trace metadata is the smallest collector candidate.**
   Store initialization can build a checked reference-field index span/bitmap
   and numeric-leaf flag from each canonical layout, including packed-kind and
   array-element rules. During collection, after authenticating an object and
   its layout/length, visit exactly its reference positions rather than testing
   every numeric field repeatedly. Preserve whole-graph preflight and validate
   every real reference of every object, including unreachable objects. Preserve
   exception edges, invalid-field rejection and zero-reclamation failure.
   A checked layout view may be reused only inside the same closed pause and
   strong canonical store pin; no borrowed view crosses a poll/callback/unlock.
   Cache memory is bounded by actual types/fields, not issued-token history.
   Compare numeric arrays and mixed reference structs separately, plus metadata
   bytes. The separately gated source candidate remains uncompiled/unbenchmarked.

3. **Extend nonmoving allocation batching only if allocation is measured hot.**
   Reference structs and bounded small arrays could use size-class blocks with
   genuine original byte-array storage, aligned named object lifetimes and
   complete initialized tails, initially keeping the same public token and
   global registry entries per object. Reservation ownership, overflow, OOM,
   publication, teardown and slot-generation exhaustion must follow existing
   slab authority. Keep the global registry and authenticating owner/generation
   lookup; batching a raw address does not mint valid guest references. Arrays
   need length/element-width classes and a large-object fallback. No open hole
   or unpublished partial reference field may become a collector object.
   Per-thread cursors need actual mutator/pause admission and epoch retirement,
   not a `shared_memory=false` shortcut. Measure malloc/refill, publication and
   token/membership costs separately before redesigning the index.

4. **Avoid repeated authentication inside one proved operation, not globally.**
   Reference field writes currently need type compatibility and ownership/lease
   checks. A private authenticated source view could be consumed by both checks
   during the same canonical owner pin and admission interval, before a safe
   field commit, retaining failure ordering and OOM behavior. It cannot outlive
   a safepoint or justify removing object synchronization. Confirm generated
   code and source-bound native hotspot attribution before implementing this.
   Cross-store write, unload, stale-token/ABA and concurrent access controls are
   mandatory; a single-module benchmark is insufficient evidence.

5. **Nonmoving mark-region is an architectural follow-up; LXR is later.**
   Block/line bump allocation could improve locality and amortize allocation
   locks without immediate moving-reference machinery. Whole-graph safety and
   exact token/store authentication remain. Charge fragmentation, header,
   metadata, work buffers and foreign pins to memory. Moving native headers
   would additionally require rebuilding global/local links and updating token
   resolutions while every native borrowed view is retired. Deferred RC/SATB
   would require complete table/global/struct/array/copy/fill/exception/host
   write barriers, roots, remembered information and a proven cycle fallback.
   These changes are too broad to smuggle into the frozen two-header experiment.

No item above permits deleting the global registry, trusting an unchecked
numeric carrier as a header, removing mutable locks based on a single-thread
bench, skipping whole-graph preflight, or reusing a token after address reuse.
The first acceptance gate remains real general-family cold correctness and
actual collection/RSS accounting under both genuine profiles. Separate native
components may isolate collection/API costs; they do not replace the VM's
automatic root, safepoint, exception or lifetime tests.
