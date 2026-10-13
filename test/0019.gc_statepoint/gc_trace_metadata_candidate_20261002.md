# Precise trace metadata: implemented source candidate, default off

The implementation and focused native test sources are present in both repositories.
This candidate is not the frozen r11 two-header performance experiment. It is
disabled unless **both runtime and CLI** are compiled with
`-DUWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1`. Default builds and the
original six-switch profile do not enable it. No local native compilation,
official Wasm validation, VM execution or performance result exists for this
candidate. Keep it disabled by default until actual current mutable/reference
baseline, cold controls and measured regressions close.

The original r11 headers are preserved in each repository's
`build/wasm3-evidence/source-only-gc-trace-metadata-before-20261002/`:

* `gc_object.h`: 277,916 bytes, SHA-256
  `55cb0d40fb264ba9b825f551d2de16c1f79b0cc9db2eb08de092651ef29ee036`.
* `descriptor.h`: 31,354 bytes, SHA-256
  `9a770293c14aa2fd7406af8dff5b4b3c0d6d34a4f9511b3f0b2b546872dffb62`.

The descriptor remains unchanged. The saved before-image manifest names exact
local paths and hashes; it does not invent a baseline product/source fingerprint.
The existing approved ABBA wrapper deliberately rejects this candidate's extra
source changes. Use a separately admitted later recipe, not an edited r11
manifest or a current candidate runtime substituted into an old baseline row.

## Authority and behavior

`gc_trace_metadata.h` owns a per-type index array for unpacked reference fields
of a struct. Arrays need only an element-reference flag; numeric/packed-only
aggregates need no index allocation. A function layout is not a numeric leaf.
Indices preserve field order, contain every true reference exactly once and
ignore packed value-type bits, whose unused heap/kind information has no Core3
meaning. An array's declared single element field does not mean its runtime
length is one: every real reference element is visited, including a legal empty
reference array's zero visits.

The store constructs metadata from **its own copied immutable layouts after
successful canonical recursive validation/interning**. No parser pointer is
retained. `type_layout` is private and never exposes its metadata builder after
publication. Build validates count geometry and classification enums, and
commits unique ownership only after the complete index array is initialized.
Failure leaves metadata unusable; store construction sets `valid_=false` and
does not register the failed cohort member. The destructor still frees all
already-owned field/layout/index/canonical allocations.

Both existing collection variants check the authenticated object's actual
owner/type/kind/layout/length, then match its immutable plan's shape. They call
the same `visit_precise_reference_fields` helper. Numeric leaves skip only the
old loop's repeated numeric/reference classification. Reference structs use
bounded index lookup and check each index against the actual payload length;
reference arrays visit their entire bounded initialized carrier span. Every
real reference still reaches the original visitor. The helper explicitly rejects
non-aggregate kinds. Packed numeric array byte representation checks remain
before the scanner. No numeric carrier is cast to a reference because it looks
pointer-shaped.

Whole-cohort/registry admission, every object's preflight, invalid references
in unreachable objects, precise-root authentication, bounded work allocation,
exception graph/native exception edges, epoch/reservation commit order, mark
and sweep, global token unlink and foreign lease pruning remain in their
original order. The same optimized scanner serves preflight, marking and lease
pruning; the optimization does not turn a cached type fact into membership or
liveness authority. No borrowed plan/view crosses a callback, safepoint,
mutator resume, registry unlock or owner retirement.

## Memory and construction cost

Charge `sizeof(gc_trace_metadata)` **for every type layout**, including numeric
and function types, plus requested `reference_count*sizeof(size_t)` for every
reference-bearing struct. Arrays and numeric leaves request zero index bytes.
The standalone native control prints actual `sizeof(metadata)` and requested
mixed-struct index bytes for the real target. On a typical 64-bit ABI the object
may be 32 bytes, but this is not an actual compiled size receipt yet.

Those requested bytes exclude allocator cookie/rounding/control metadata,
changed enclosing-layout alignment, the retained original field descriptors,
canonical registry storage, process mappings and work buffers. Preserve actual
constructor RSS/peak RSS and allocation receipts; do not label the simple sum
as total heap commitment. New per-type allocations and two constructor scans
may cost more for short-lived modules. No per-object header/carrier size, guest
token format, mutable lock or synchronization rule changes. Evaluate throughput
and cold/module initialization separately before release.

## Focused remote cold controls

The sibling `gc_trace_metadata_cold_manifest_20261002.json` pins these source
files and expected contracts. It is `execute_ready=false`, contains no made-up
source ID/ELF/runtime/compiler receipt and is not a runnable benchmark plan.
Only the Linux keeper may bind it to fresh actual builds and the verified
64 GiB/swap-zero scope. Compilation uses admitted E cores; no sampling/counter
measurement overlaps compiler/Windows/profile work.

1. Compile and run `test/0019.gc_statepoint/gc_trace_metadata.cc` normally.
   It covers fresh/unready/empty/function plans, exact mixed indices 1/4/6,
   packed reference-looking numeric fields, numeric/reference array flags,
   shape/index/enum/count overflow rejection, untouched outputs on failure and
   reset/cleanup. Compile/run a second ELF variant with
   `UWVM2TEST_TRACE_METADATA_OOM=1` and the exact target nothrow-array link wrap.
   It must observe precisely one failed actual index allocation, an unusable
   cleared plan and successful rebuilding. Neither is collection or VM proof.
2. Compile/run `test/0019.gc_statepoint/gc_trace_metadata_store.cc` against
   default-six-off and experiment6 profiles, each with precise metadata off/on.
   It owns real canonical stores, strong module lease owners, mixed struct
   reference indices 1/3, packed/numeric fields, cross-store graph edges, a
   rooted A→B→A cycle, reference/numeric/zero-length arrays and duplicate roots.
   Every collection acquires a genuine exclusive admission lease. Three
   successful collections reclaim exactly 1+1+6=8 objects; the intervening
   invalid-later-root failure reclaims zero and leaves payloads intact. Dropped
   intact cycles are actually reclaimed; all retired tokens reject readback.
3. For each metadata-on store profile, compile/run the separate ELF fault
   variant with `UWVM2TEST_TRACE_METADATA_STORE_OOM=1` and
   `UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE=1`, plus the exact nothrow-array
   wrap. The test-only rendezvous arms failure **immediately at the real
   metadata index allocation**, not before unrelated canonical allocations.
   Require one matched probe/one actual failure, `valid()==false`, full cleanup
   and a later healthy two-store cohort. The probe/callback and allocator wrap
   are absent from all product/performance builds. ASan/UBSan variants must
   preserve logs and diagnose partial-allocation cleanup independently.
4. Cold build/execute the actual eight general-GC fixtures under genuine
   default and experiment6 product profiles with metadata off/on only after
   native controls pass. Preserve official Wasm validator/Wasmtime identical-byte
   oracle receipts. Actual mixed/ref/array workloads, stale/foreign/invalid
   layout controls and existing compact-directory cases remain necessary.
   A standalone index test is insufficient release acceptance.
5. Repeat the existing complete exception graph/native exception-root controls
   with precise metadata **off and on**, using matched current product profiles.
   Include aggregate↔exn cycles, an old exnref retained by a newly caught wrapper,
   an actual registered native exception owner, unreachable invalid payloads,
   stale/foreign tokens, root drop and failure with zero reclamation. The exact
   original immutable exception identity and trace must survive while reachable.
   `gc_trace_metadata.cc` covers classification of an exn reference field, but
   does not create or collect an exception graph. Preserve the actual
   `gc_exception_graph.inc` and native-root header hashes before/after.

   The historical sources `exception_payload_roots.cc` and
   `llvm_gc_exception_roots.cc` under `test/0019.gc_statepoint` provide payload
   visitor/native-EH controls; the latter and `product_cohort_sweep.cc` still
   contain aggregate-only prototype assertions that a live exn registry causes
   rejection. Such an assertion cannot qualify current graph-enabled collection.
   Bind the keeper's current graph controls and expected results explicitly;
   do not silently reinterpret a historical overlay result as product proof.
   Their associated runners are not authorized here to substitute an old store
   overlay or a differently configured runtime into the new candidate.

On x86-64 ELF the linker wrap name is `_ZnamRKSt9nothrow_t`; on a confirmed
32-bit ELF size_t ABI it is `_ZnajRKSt9nothrow_t`. Record actual compiler/linker
and symbol receipts; do not pretend one ABI's wrap qualifies another target.
Additional source hashes and full compiler/rsp/profile bindings must match
before/after execution. All current acceptance/performance fields remain false.

## Static preservation receipt

A pure-text selection of the new candidate conditionals to their off branches,
with the new standalone header include removed, reproduces the complete saved
`gc_object.h` before-image byte-for-byte. This is an actual source-body
preservation check, **not** C++ preprocessing, compilation or assembly proof.
The new header/module are imported/exported through storage `impl.h/.cppm`
and `wasm_module.cppm` in both repositories. Module builds remain untested.

The performance gate must distinguish old whole-guest counters, a separately
timed native collection API and a real default/mutable/reference VM workload.
No speedup, industry ranking or default-path performance pass is claimed here.
