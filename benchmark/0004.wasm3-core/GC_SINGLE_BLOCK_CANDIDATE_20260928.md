# One-block allocation candidate for the native GC store

This is an isolated allocation-layout change to the exact owner-checked
collector header `16fcee47d451c614b222db978ff2fc2570d8c3ada94c782a0505b0ae1407b1f5`.
It does not change LIVE source, integrate VM roots, start automatic collection,
remove a lock, or establish an industry performance ranking. The older
`21f90b8…`/`7eca359…` layout experiment is retained separately and is not the
baseline for this candidate.

The current draft is
`/tmp/uwvm2-gc-single-block-16fcee-r2/include/uwvm2/uwvm/runtime/storage/gc_object.h`,
SHA-256 `0c17afb7976fb6d22c6708f0187952f638cac9c19f8e7f23c100223b5e31feeb`.
Its `single-block.patch` SHA-256 is
`07940ccad77e0460f615a84ee650310a0ceb5babdbb3e910343dfded407a7969`.
The preceding uncompiled r1 draft remains in its own directory and manifest.
These are local candidate paths; a future SSH run must stage and rehash them
instead of assuming the same `/tmp` paths exist remotely.

## Lifetime and layout

One ordinary nothrow allocation contains the original object header, padding
rounded up to `alignof(gc_object_value)`, and a real zero-initialized placement
array of full 16-byte values. The checked total includes the header and padding,
and is bounded by `PTRDIFF_MAX` before multiplication or pointer arithmetic.
Length zero creates only the header and leaves the view null. The ordinary
allocator must supply both alignments; compile-time assertions reject a target
that does not meet the default-new alignment requirement.

The field view contains one native pointer, with the same size and alignment as
the previous `unique_ptr<gc_object_value[]>`. The original mirror and all object
size, alignment, and hot-field offset assertions remain. Get, indexing, and
array-copy calls retain their existing interface. No additional data member,
per-access condition, ownership lookup, or reference dereference is introduced.
Guest-visible identities remain nonrecycling opaque tokens.

Global non-allocating placement array new supplies a genuine array lifetime and
no array-cookie overhead. A target-specific array-limit rejection retires the
already constructed header before publication. These properties follow the
[C++ draft's new-expression rules](https://eel.is/c++draft/expr.new) and
[non-allocating placement overload](https://eel.is/c++draft/new.delete.placement).
All elements are trivially destructible; releasing the whole block ends their
lifetime under the [object lifetime rules](https://eel.is/c++draft/basic.life).

The object's class-specific unsized `operator delete` returns the original base
through the matching global scalar deallocator. Every existing failed
construction, token-publication failure, teardown, and sweep `delete object`
therefore runs the header destructor, releases its foreign leases, and frees
one complete block. The non-owning view never deletes its interior pointer.
Extern bridges retain their independent allocation/deallocation paths.
The [delete-expression rules](https://eel.is/c++draft/expr.delete) permit the
non-array placement-created header to select this class-specific scalar
deallocator; the tail array is never an operand of `delete[]`.

## Qualification and measurement

`run_gc_single_block_collect_ab.py` accepts two one-header include roots. Both
variants override the exact 16fcee header in otherwise identical source
contexts. An r2 product's historical `a24…` header is recorded as context
provenance, not silently used as the layout-only baseline. The native fixture
links no product runtime object, preventing mismatched GC class definitions
across translation units.

The edge fixture counts real ELF scalar/array nothrow allocation calls and
tracks every successful native allocation until matching deletion. It checks
zero-field structs, zero-length arrays, a 65,536-element i64 array, packed i8/i16
extension, all numeric carriers, NaN and signed-zero bit patterns, v128,
references, array copy, first/second/large-block OOM injection, total-size
overflow, partial foreign-field failure, explicit cross-store cyclic
reclamation, stale-token rejection, nonreuse, and foreign lease release.
Reference struct padding is not mistaken for Wasm data. ASan, UBSan, and LSan
must run successfully on SSH Linux. Existing closed-cohort and four-thread
opaque-token fixtures provide additional graph and concurrent-identity checks.
The edge-result decoder requires all twelve explicit collections and exact
returned reclamation totals, including the candidate's successful second-call
fault control. A truncated or zero-collection marker cannot qualify it.

Four actual O3 get/set wrappers are compared after normalizing only local label
names and assembler source comments. Any changed instruction body stops the
run for review. Identical instructions establish no new field-path software
work; they do not prove identical cache behavior.

Build-only qualification runs on E16 within its separately authorized bounded
slot. Formal measurements require an explicitly handed-off quiet window in the
64 GiB, swap-free, exact `0,2,4,6,16-31` cgroup. They include nine reversed pairs
of a 1M–10M allocation workload with 1,024 native roots and explicit collection,
plus 1/2/4 P-core publication and teardown. The ring's scalar is mutable to
support its following set/get phase; it is a native component workload and
does not represent the earlier immutable-field Wasm ring.

Each sample records source, header, fixture, compiler and binary hashes,
allocation/checksum/collection/reclamation counts, actual RSS checkpoints,
native collection durations, cgroup events, frequency, temperature availability
and CPU throttling. Short timing cells and nine process-level samples cannot
establish small regressions or a service p99. A field-path regression requires
repeatable, temperature-matched data and cache/layout investigation before
adoption, even if allocation improves.

Only Python syntax and offline accounting/assembly-normalization controls have
run for this new draft. C++ compilation, Linux sanitizers, paired generated
assembly and formal P-core timings remain pending. Neither a positive layout
result nor explicitly invoked native collections can clear the full-VM GC
release blocker.
