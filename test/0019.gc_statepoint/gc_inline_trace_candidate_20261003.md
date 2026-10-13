# Inline GC reference classification: default-off source candidate

This is a separate source candidate, not a qualification of the immutable
6ABC snapshot or the old R3 executable. No native compiler, VM, sanitizer,
profiler or benchmark was run locally for this change. Actual Linux qualification
belongs to the sole keeper in the existing 64 GiB cgroup. The new gate is enabled
only when `UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA=1` and
`UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1` are both compiled into every
participating translation unit. Enabling the first without the second is a
compile error. Neither switch is enabled by default.

## Changed behavior and limits

The canonical store already owns a copied immutable trace plan. For a struct
with at most 64 actual fields, the new plan classifies unpacked reference fields
in a private `uint64_t` word. Collection iterates its set bits once, in field
order. A larger struct keeps the original owned index array and allocation
failure handling. Arrays retain the original numeric-leaf/reference-element
classification and their actual runtime length. No guest token, native address,
borrowed parser pointer or debug authority is stored in the word.

The complete field-count and byte geometry checks precede every field access.
The bit shift is limited by `index < field_count <= 64`; the set-bit visitor
checks shape, reference count and the selected field before visiting it. The
store checks the selected field against the authenticated initialized payload
extent before loading the carrier. Every true edge still passes through the
original membership visitor. Invalid rebuilds clear old indices, bitmap, ready
state and counts before returning. Large-layout OOM never publishes a plan.

Whole-cohort admission, canonical validation, whole-graph preflight (including
unreachable objects), opaque-token authentication, every real reference edge,
precise roots, exception edges, foreign ownership, epoch checks, commit and sweep
remain unchanged. This changes collection scanning only. It does not remove
mutable-field locks, object publication indexes/CAS, global stripe locks or
ordinary memory access checks. It cannot by itself explain or close the previous
~117 ns/step general-GC observation.

The additional word occupies storage in every type's metadata when the gate is
on. Record actual `sizeof(gc_trace_metadata)`, enclosing-layout size/alignment and
constructor RSS for both profiles. Do not equate saved index bytes with total
memory saved: numeric/function/array types gain the word without index savings;
large structs keep the index allocation too. On a 64-bit target a one-reference
small struct saves one 8-byte index while adding an 8-byte word before ABI
padding; this is not a measured ABI or allocator receipt. Report index allocation
count/bytes separately from object storage and total RSS.

The cache profile adds
`gc-inline-trace-metadata=owned-small-struct-bitmap64-v1`. This separates native
cache contexts even if an embedder deliberately reuses a source ID. It does not
make C++ objects built with different macros ABI-compatible. Runtime, main and
host API translation units must all be rebuilt for a product profile.

## Focused native qualification

Use one new complete frozen source tree for each repository, with the renamed
storage `.h` dependencies and actual source/vendor hashes. Do not overlay the
new `gc_object.h` onto an old snapshot that has only the historical `.inc`
paths. The two profiles compared for this candidate are:

* C: precise trace metadata only, with all other experimental switches fixed.
* D: precise trace metadata plus inline trace metadata, identical source bytes,
  provider tuple, compiler flags, features and build inputs otherwise.

A default-six-off component control checks the original classification path.
The original A/B/C recipe, results and manifests remain byte-for-byte unchanged.

1. Build/run `gc_trace_metadata_inline.cc` in D. It covers sparse bit 0/63,
   dense 64 references, 65-field fallback, empty structs, packed numeric fields
   with unused reference-looking bits, arrays, shape/index rejection, unchanged
   output on failure, stopped visitor, invalid rebuild and overflow before the
   supplied one-field storage could be read. Preserve actual metadata size.
2. Build/run a separate ELF OOM variant with
   `UWVM2TEST_INLINE_TRACE_METADATA_OOM=1` and
   `UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE=1`. Wrap the actual target's
   nothrow array allocator: `_ZnamRKSt9nothrow_t` for confirmed 64-bit ELF
   size_t, `_ZnajRKSt9nothrow_t` for confirmed 32-bit ELF size_t. The test arms
   failure at the real 65-field two-index allocation and requires one exact
   probe/failure, no usable failed plan, and successful rebuilding. Never carry
   this probe, wrapper or fault macro into a product/performance build.
3. Build/run `gc_trace_metadata_inline_store.cc` in default, C and D. Two
   real canonical stores have 64/65-field mutable layouts. One authentic root
   retains a cross-store A↔B cycle through fields 63/64; two numeric carriers
   deliberately contain full reference-looking bits but their garbage targets
   must be reclaimed. Packed numeric readback, intact reference edges, dropping
   the final cycle, foreign ownership and rejected retired tokens are checked.
   Every collection obtains the real exclusive admission lease. These are native
   components, not automatic VM root or Wasm-thread qualification.
4. Repeat the existing `gc_trace_metadata_store.cc` and
   `gc_trace_metadata_exception_graph.cc` cold semantics in C and D, without
   their historical small-layout index-allocation OOM expectations. Their exact
   allocator-fault variants remain tests of the original C plan. The old
   `gc_trace_metadata.cc` index-byte expectations are also C-only. Use actual
   current graph controls, including aggregate↔exception cycles, native exception
   roots, invalid later roots/edges with zero reclamation, source teardown and
   old exception identity/trace retention; never substitute aggregate-only
   historical prototype results.
5. Run the new metadata and store controls under ASan/UBSan in separate fresh
   E-core builds. Native C++ module/header builds, 32-bit ABI/OOM wrapper and the
   keeper's AArch64/RISC-V64/i386 QEMU lanes remain additional qualification.
6. Build/run `llvm_jit_gc_inline_trace_metadata_cache_key.cc` in gate-off/on
   LLVM-full configurations and verify the exact profile marker. Product builds
   must record macro witnesses and dependency/artifact closure for all three
   fresh translation units. Old R3 binaries do not satisfy this check.

## Core 3 guest and performance qualification

`benchmark/0004.wasm3-core/generate_gc_trace_width.py` creates independent
self-checking 64/65-field recursive structs with mutable typed self references,
a 1,024-entry typed root table, packed numeric and SIMD fields. Each allocation
creates a self-cycle; the loop and final roots have independent modulo-32
checksums. The two widths have identical scalar results. The generator emits
WAT and expected JSON only; actual Wasm bytes/hashes are null until the official
`wasm-tools` parser/validator runs remotely.

First qualify the 2,048-step cases under all four ordinary modes and both ROS
full modes, using the actual supported feature switches. Preserve each accepted
module's official parse/validate/round-trip receipt and independent Wasmtime49
same-byte `run` result and `_start` exit-zero result. This does not claim the
int/lazy modes have complete precise roots. VM collection qualification remains
LLVM-full with genuinely captured roots and actual native participants.

Only then run 250,000 and 2,000,000 steps in matched C/D LLVM-full builds. Record
actual automatic collection count, reclaimed objects, root/epoch closure,
post-root-drop stale-token rejection, RSS/heap/metadata bytes and initialization
cost. A checksum pass without actual reclamation is insufficient. The 64-field
case exercises the word; the 65-field case is a deliberate fallback regression
control, not an expected speedup. The existing mutable struct, cyclic struct,
reference-array and numeric-array families remain part of the eventual full gate.

Use quiet P-core-0 ABBA plain wall-clock/CPU/RSS pairs and separately bound
VTune hardware hotspots and uarch-exploration captures, following the installed
CLI's official help. Save command, source/vendor/macro/provider/binary/fixture
hashes, logs, cgroup OOM/swap/throttling, observed affinity and in-window frequency.
No compiler, Windows VM or other profile may overlap these samples. Missing
frequency or skipped stack frames restrict conclusions; safe CPU temperature
is not a cancellation criterion. Software fallback samples are not hardware
counter evidence. Whole-process JIT cost and guest execution must be labeled
separately; sampled CPU seconds are not plain elapsed time.

Wasmtime49 copying is the initial same-Wasm, same-platform reference. Use the
installed binary's observed collector/feature help, exact artifact hash and
actual same-byte semantic qualification. Its DRC/null configurations may be
reported separately but cannot qualify cycle reclamation. WAVM and other engines
need current syntax and actual collector support before entering a timing row.
Java/GraalVM HotSpot, CLR and V8 families are semantic analogues with different
layouts/GC/JIT; retain their actual collector flags, root checks, allocation/GC
telemetry, heap limits and in-window timing rather than calling them identical
Wasm peers or treating no-reclamation controls as production collectors.

## Primary design references

[Wasmtime49 copying source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs)
uses a Cheney semi-space collector with JIT-visible bump allocation, small-struct
reference bitmaps, array classification and out-of-line larger plans. The word
classification is the narrow principle used here; its movable heap-index
representation does not authorize changing our opaque-token or ownership model.

[OpenJDK25 TLAB source](https://raw.githubusercontent.com/openjdk/jdk/jdk-25-ga/src/hotspot/share/gc/shared/threadLocalAllocBuffer.hpp)
and [CLR10 GC design](https://raw.githubusercontent.com/dotnet/runtime/v10.0.0/docs/design/coreclr/botr/garbage-collection.md)
provide the next allocation principle: bound a genuinely thread-owned allocation
context and amortize refill/synchronization. This requires actual mutator/pause
and lifetime proof here, not an inferred single-thread benchmark. [Immix](https://www.steveblackburn.org/pubs/papers/immix-pldi-2008.pdf)
and [LXR](https://www.steveblackburn.org/pubs/papers/lxr-pldi-2022.pdf) are later
nonmoving block/line allocation and hybrid cycle/barrier design references.
They are design input, not performance results for this candidate.

The text-only default-off selection receipt reproduces the saved production
C++ token stream for all eight changed paths. It is not native preprocessing,
assembly equality, sanitizer success or a measured no-regression result. Keep
the gate disabled until the actual component, guest, memory and timing gates close.
