# GC and exception performance work, 2026-09-29

The reported 117–118 ns/step GC result is an optimization blocker. The new
candidates below are not measured speedups and are not release qualification.
The current automatic collector is a nonmoving aggregate collector; it is not
an implementation of Wasmtime copying GC, HotSpot TLAB, or Immix.

## What the reference implementations establish

[Wasmtime 49 copying lowering](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/cranelift/src/func_environ/gc/copying.rs)
uses inline bump allocation while capacity remains, a cold allocation/collection
call otherwise, and precise maps to relocate roots. Its ordinary reference
loads/stores do not need concurrent or reference-count barriers in that design.
That is a concrete target for reducing allocation work. Transferring it requires
UWVM's native handles, reference identity and root lifetimes to remain valid.

[HotSpot's TLAB allocation](https://raw.githubusercontent.com/openjdk/jdk/815ff4dc327fe17f2433c7d115a5a503af10f3c4/src/hotspot/share/gc/shared/threadLocalAllocBuffer.inline.hpp)
checks available words before advancing its local top. The useful principle is
local ownership of a bounded allocation region and cold refill. The design does
not justify removing UWVM locks while another native reader, initializer or
entrant is admitted. No HotSpot implementation code is copied here.

[Immix](https://www.steveblackburn.org/pubs/papers/immix-pldi-2008.pdf)
combines allocation into contiguous regions with reclamation at block/line
granularity and optional defragmentation. UWVM's existing opaque handles make a
nonmoving region path a possible first step. Moving bodies additionally requires
updating every reachable handle target and forbidding native borrows over the
move; that facility is not implemented by the current slab pool.

[Wasmtime's exception table](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/unwinder/src/exception_table.rs)
indexes real callsites and handler destinations and includes dynamic instance
context for tag matching. [Its pending exception API](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/wasmtime/src/runtime/exception.rs)
keeps the exception rooted on the store while it crosses host boundaries.
Neither an integer tag index nor an equal signature authenticates a tag instance.

[LLVM native exception handling](https://llvm.org/docs/ExceptionHandling.html)
requires real search/cleanup edges. The local WAVM runtime also allocates an
exception object and supports native propagation. Neither reference licenses
skipping a C++ destructor or using a raw jump across arbitrary host frames.

## Current GC work and the next allocation boundary

The qualified ROS R4D whole build has matching freshly compiled runtime and CLI.
The original 2M and 16M allocation rings have run under both instruction and
unwind policies with their original binary hashes and successful checksum
self-checks. The 16M run performed 3,906 automatic collections and reclaimed
15,997,951 objects for each policy. Peak process-tree RSS was 49,696,768 and
50,266,112 bytes respectively. These are actual E-core functional results,
not P-core throughput, the separate 1M-to-10M RSS-release gate, all modes,
reference exceptions or concurrent-participant qualification.

The actual R4D optimized IR and finalized MCJIT object preserve the strict
adjacent immutable cast/read witness. On its local success path, both policies
load the scalar directly from the checked carrier array without a second native
getter, membership lookup or lock. Foreign and other ineligible values retain
the checked fallback. The actual unwind caller has native `.eh_frame` metadata;
it does not simulate unwind with logical instruction-stack push/pop. Allocation
still uses the older buffer ABI in this qualified object.

The independently reviewed records are
[the 2M/16M functional review](../../build/wasm3-evidence/linux-r2-native-stage-20260928/root-r4d-2m-16m-functional-review-r1-20260929.json)
and [the actual machine-code catalog](../../build/wasm3-evidence/linux-r2-native-stage-20260928/r4d-formal-gc-machinecode-small-raw-r1/mirror-manifest.json).

The current allocator still reserves and commits slab slots under separate slab
locks and publishes objects through local/global indexes. A slab removes many
system allocations; it does not make these operations free. The following
source candidates keep their evidence and source identities separate:

* r8 holds all global stripes once per collection in a fixed order, then releases
  them before pruning leases and destroying retired bodies. Complete canonical
  cohort, graph/root validation, reservations and epoch preflight remain.
  Allocation and ordinary lookup paths are unchanged. It may cost more on tiny
  heaps; real sanitizer, deadlock, OOM and whole-VM tests are still required.
  The r8b refinement retains the unique marked worklist for post-sweep lease
  membership. It sorts only when an aggregate lease needs pruning, then uses
  integer-token binary search. Numeric-only heaps do not pay that sorting cost.
  Its separate native packet includes real work-array OOM, 64-store sparse
  cycles, the original unpublished-reservation/generation controls and a real
  after-unregister destructor rendezvous. Those new native tests are pending.
* r9 uses a native `WORD(WORD, i32, i32)` ABI for eligible single-field
  `struct.new`. Raw f32 bits, including signaling NaN, are transmitted without
  FP evaluation. Success returns the allocator's opaque token; status+1 occupies
  a disjoint low range. All other layouts retain the original buffer ABI.
  The actual root snapshot precedes allocation polling and operands retire only
  after success. This removes candidate input/output buffer traffic, not the
  allocator's locks or canonical type checks. Actual LLVM/object proof is pending.

A future TLAB needs a genuine owner lease acquired at the native entry boundary,
not a load of `active_count == 1`. Every other entrant, native handle borrow,
initializer and teardown must obey the same exclusion protocol. Its region is
pre-reserved with a bounded cursor and generation/epoch. Refill, exit, rejection,
collection and escape flush all unused reservations before the region retires.
A native borrow never survives a poll. Opaque IDs remain nonrecycled and region
memory is not used as authority for an arbitrary supplied token. No such TLAB
lease has been added to the production allocator yet.

The private managed-entry numeric-page r2c candidate now has complete physical
source images for both repositories. It moves a genuine execution-generation
lease into one noncopyable entry owner, proves exclusive admission and the
actual collection participant, and reserves 256 original slab envelopes with
nonrecycled IDs. Eligible immutable one-field i32/f32 objects are initialized
and authenticated locally. Collection, foreign escape, pause and retirement
drain the page before reopening ordinary membership or releasing its owner.
The final metrics boundary also drains before reading counters, including when
logging is disabled. The independently rehashed source images and native runner
are recorded in
[the full-source review](../../build/wasm3-evidence/linux-r2-native-stage-20260928/root-managed-page-r2c-fullsource-controller-review-r1-20260929.json).
The complete ROS source stage and its independent host namespace verification
have passed on Linux. Three native syntax checks and the O3 entry test passed.
The boundary test then failed at foreign retention: the fixture omitted the
module-owned lease root that the real initializer creates. The original 9/18
failure is preserved in
[the native result](../../build/wasm3-evidence/linux-r2-native-stage-20260928/page-r2c-native-failure-result-and-retirement-r1.json).
The private r2d fixture adds that root and tests both successful foreign retention
and rejection by a valid recipient without roots. All 16 product files and both
complete source IDs remain unchanged. The complete native retry passed all 18
commands, including O3 and ASan/UBSan/LSan entry, boundary and real allocation
failure tests. Its largest observed native tree used 2,163,380,224 bytes, with
no new cgroup memory-limit or OOM event. This qualifies the native component,
not guest execution or throughput. The same complete ROS source has also built
a fresh O3 runtime and CLI with the page macro enabled in both translation units;
their actual compilation peaks were 5,368,549,376 and 4,293,496,832 bytes.
All owned compile processes retired and all 45 locked inputs remained unchanged.
The independently read records are
[the actual native retry](../../build/wasm3-evidence/linux-r2-native-stage-20260928/page-r2d-native18-qualified-critical-small-raw-r4/mirror-manifest.json)
and [the fresh whole-build review](../../build/wasm3-evidence/linux-r2-native-stage-20260928/root-page-r2d-actual-fresh-whole-build-review-r1-20260929.json).
The matching PAGE product has also run the original 2M/16M GC ring in both
instruction and unwind policies. Guest checksum assertions, exact attempted
allocation counts, 488/3906 collections, paired refill/drain counts and zero
remaining issued objects passed. This is functional qualification on the E core;
it is not P-core timing. The actual LLVM probe compiled, ran its 68 native
checks and produced IR/objects. Its final assembly selection rejected a weak TLS
data symbol incorrectly classified by nm as a function. That failed report is
retained. A generic executable ELF function selector has been reviewed; full
machine-code completion, RSS release and formal P-core timing remain pending.
The actual original-workload review is
[the 2M/16M functional record](../../build/wasm3-evidence/linux-r2-native-stage-20260928/root-page-a42c-actual-original2m16m-functional-review-r1-20260929.json).

This candidate still has native leaf calls, authority checks, full object
initialization and per-object index publication during cold drain. The qualified
R4D local cast/read success edge executes four native calls in total and a direct
field load; its fifth getter callsite is a fallback. The page candidate has no
qualified machine-code count yet. Its batch reservation is not evidence that
those remaining costs are negligible or that other GC shapes are faster.


The separate compact dual-store r2 candidate stores an eligible immutable i32/f32
field in one real four-byte cell. A persistent descriptor authenticates the real
store, canonical type, issued token range, publication frontier and live bitmap.
Legacy objects and compact cells share the existing nonrecycling token issuer.
The store and collector handle both representations; legacy cycles and arrays
can mark compact numeric leaves. Foreign native readers retain the actual typed
store and exclusion admission. Neither a borrowed native pointer nor an empty
local shared root proves guest liveness. Default-off bodies and existing member
layout prefixes were independently checked against their exact preimages.

Its [source-only packet](../../build/wasm3-evidence/linux-r2-native-stage-20260928/compact-dual-store-r2-source-only-manifest-r1.json)
contains physical changes for both repositories and matching module imports and
exports. The [native proof review](../../build/wasm3-evidence/linux-r2-native-stage-20260928/root-compact-dual-store-r2-native-proof-source-review-r1-20260929.json)
requires real foreign leases, concurrent readers, stale IDs, closed-cohort sweep,
legacy reference edges and actual allocation failures. The native retry has now
passed all 13 syntax/build/run commands, including O3 and ASan/UBSan/LSan. The
store proof completed 177 checks and the OOM proof 2093 checks, including two
actual failed nothrow payload allocations. The closed-cohort collector reclaimed
objects in both representations. These are native storage tests against the
exact seven-header closure, not a fresh whole product or inline-JIT qualification.
The [actual native review](../../build/wasm3-evidence/linux-r2-native-stage-20260928/root-compact-dual-store-r2-actual-native13-review-r1-20260929.json)
records their precise scope. This compact storage still uses cold native
allocation and locks; it is not yet a qualified inline bump allocator. A sparse surviving cell retains its 1024-cell
payload page. Native entry authority and a JIT-visible sealed cursor are being
implemented separately so hot allocation can avoid a runtime call without
weakening admission, roots or publication. The original attempted-allocation
counter and collection-before-allocation schedule must remain exact, including
real OOM and fallback paths. No compact JIT throughput has been measured.


The separate complete sealed/table r2 source combines the owning-source EH
implementation with compact storage, inline JIT allocation and one local table.
It keeps real entry/generation/cohort authority; a guest token never supplies a
native payload address. Current-range table access and exact immutable cast/read
have no native helper on their successful edge. Older ranges, null/i31, imports,
unsupported layouts, callbacks and grow keep checked fallback after root
publication and cursor retirement. This source is still awaiting C++/LLVM/VM
qualification; none of its throughput is measured.

Independent review found a lazy entry could disable collection before acquiring
execution admission. The new source moves that policy change after admission.
Cold retirement now settles already successful allocations once, even when
new allocations have been disabled, while retaining the original allocation-poll
body exactly. A genuine descriptor frontier sampled at refill and retirement
must agree with the deferred count and first already charged allocation. Cold
receipts report actual committed slots, windows, refills and zero unsettled
pressure; no extra observer counter was added to the allocation hot path.
The source-only identity and matching RT/CLI requirements are in
[the sealed/table r2 manifest](../../build/wasm3-evidence/linux-r2-native-stage-20260928/sealed-table-and-accounting-r2-source-only-manifest-r1.json).

## Current exception candidate

The R5C private numeric island uses a caller-owned native context, passed as the
first hidden internal argument; the public function ABI is unchanged and a
multi-result output buffer remains last. Actual validator/type admission rejects
imports, reference payloads and unsupported guest instructions from this path.
Those cases keep ordinary native exception propagation. The default compile
option is still null; this path is not activated in product code.

A caught numeric exception reuses pending storage. An exception escaping the
island materializes an immutable value and compact diagnostic only at the
public boundary. The wrapper retains genuine invokes, cleanup landingpads,
resume and one owner destructor on normal/native exceptional paths. Tail calls
retain an immediate musttail plus return when the exact internal ABI permits it.

The separately frozen R5D/R2 header candidate replaces the per-call native presence
query with a standard-layout native-word phase load. An invalid phase still
traps before normal results are consumed. Cold whole-graph analysis folds only
sites anchored to the actual callee and same hidden owner. Guest escape and
native/C++ unwind are independent effects; a guest effect result never adds
LLVM nounwind. Aliasing, wrong-thread, inactive/busy owner, failed materialization
and retired generation remain actual-proof requirements.

This strategy is not a claim that diagnostic unwind is universally faster when
throwing. Native CFI removes ordinary logical push/pop in unwind mode; reading
CFI during a throw still has a cost. The existing instruction/unwind trace
policies and their diagnostics remain distinct.

The same-source R5E ROS runtime and CLI have completed a fresh O3 build after
fixing an explicit string-view conversion in the private emitter. This is only
compiler qualification: its product pending plan remains null. The separate
actual LLVM proof stopped at the first default-mode raw call: the native fixture
passed its input address as the opaque context and passed zero parameter bytes.
The existing generated ABI guard correctly trapped. Its immutable fixture-only
retry derives exact buffer sizes from the actual initialized function signature
and checks the real wrapper type and calling convention before calling
`context/result-address/result-size/parameter-address/parameter-size`. The
runtime guard and the original failed evidence remain intact. The r2 retry
passed actual syntax, O3 compilation and linking, then failed its structural
owner-storage witness. The actual byte-locked emitter reserves one fixed
`[sizeof(entry_owner) x i8]` array; the fixture incorrectly expected scalar i8
with an array count of `sizeof(entry_owner)`. The failure is retained in
[the r2 raw catalog](../../build/wasm3-evidence/linux-r2-native-stage-20260928/r5e-raw-abi-r2-actual-failure-small-raw-r1/mirror-manifest.json).
The private r3 fixture checks the exact fixed array extent, constant count one
and native alignment, and saves the actual unoptimized graph before checking
it. Raw ABI, construction/cleanup dominance, conservative native unwind,
musttail and effect witnesses remain unchanged. The actual r3 retry passed all
28 commands against the matching R5E runtime and CLI. It used the real parser,
validator and MCJIT in default, folded and unfolded modes; the default option
remained null. The actual wrapper preserves cleanup invokes, native unwind and
the fixed 3736-byte aligned owner array. Folded and unfolded graphs each retain
nine immediate musttail sites. Real owner creation/destruction balances at 55,
and the original 8M-step/500K-catch module returns checksum 2,464,256. This is
native LLVM prototype qualification, not automatic-GC interaction, activation
of the production plan or a performance result. The actual raw evidence is in
[the r3 catalog](../../build/wasm3-evidence/linux-r2-native-stage-20260928/r5e-owner-array-r3-qualified-small-raw-r1/mirror-manifest.json).
The separate owning-source production candidate additionally pins initialized source, plan,
LLVM context, engine and CFI in destruction order. Review found that numeric
global declarations could still resolve through the old process-wide symbol
map. The r2 correction binds each exact initialized scalar to its own engine
and checks the captured source/runtime epoch. A real interleaved dual-instance
compiler witness now uses the same metadata-sized raw ABI. Complete engine-local
r2 product source images have been frozen for both repositories, with every
product postimage verified and the canonical full-tree fingerprints recomputed.
R5H also fixes the explicit FastIO pointer/size view constructor in each source
owner's rename accessor; its safety comment states the borrowed range and owner
lifetime. A fresh ROS O3 runtime and CLI have now compiled successfully from the
complete R5H source, without reusing an older runtime object. Their matching
owning-source proof then failed during fixture compilation, before any guest
execution. The complete compiler failure and independent unchanged-input check
are retained in [the failed proof catalog](../../build/wasm3-evidence/linux-r2-native-stage-20260928/r5h-owning-pending-proof-r1-genuine-cpp-failure-small-raw-r1/mirror-manifest.json).
Fixture revision r5 adds the actual compiler declaration, corrects the FastIO
UTF-8 string adapter and explicitly constructs the command-line string view.
It preserves every product source image, runtime object, proof assertion and
runner. Its actual compile retry failed in a remaining native-path output
adapter. Revision r6 corrected that output adapter and compiled against the
same fresh runtime. All eight Wasm inputs parsed and validated, but the first
source preparation then stopped with SIGILL, before entering generated code.
The original executable and complete failed logs remain preserved.

One read-only GDB replay of that same executable located the trap in
`init_wasip1_environment`. Source review found a definite fixture contract
violation: a standalone stack parameter had been published as
`wasm_file_ppos`, although WASI subtracts it from `parsing_result.end()`.
Those pointers did not belong to the same array. The r7 fixture instead
constructs the real owning parameter vector before publishing its position,
keeps its filename alive, and restores the original allocation and position
with the existing noexcept vector swap. Product sources, default WASI
initialization, all prior assertions and the runner remain unchanged. This
fixture correction has completed source review; its actual retry is pending.
The GC production-entry fixture has received the same ownership correction
in its independent r3 revision and also awaits actual execution.

The ordinary command-line parser already provides a genuine array position.
Separate runs of the original 8M-step/500K-catch exception workload using this
same R5H runtime and CLI passed in both diagnostic policies. Their actual
activation logs identify the owning-source pending plan. The instruction run
reports emitted call-stack frames; the unwind run reports live unwind and
omitted instruction frames. These are functional E-core results, not P-core
timing or a replacement for the failed owning-source fixture. The GDB and
both CLI logs are retained in [the diagnostic and CLI catalog](../../build/wasm3-evidence/linux-r2-native-stage-20260928/r5h-r6-gdb-and-formal-cli-actual-small-raw-r1/mirror-manifest.json).
Production ownership, cleanup and performance qualification remain pending.

The first actual sealed-table/accounting R2 command-line smoke has now failed
with `SIGABRT` on the original self-checking 2M GC workload. The instruction
policy failed before any GC receipts were printed; unwind was not run. Its
fresh runtime and CLI compiled successfully, but that is not a guest pass.
The original binary and [complete small failure records](../../build/wasm3-evidence/linux-r2-native-stage-20260928/sealed-r2-actual-cli-2m-SIGABRT-small-raw-r1/mirror-manifest.json)
remain unchanged.

A [GDB capture of that same binary and original argv](../../build/wasm3-evidence/linux-r2-native-stage-20260928/sealed-r2-same-ELF-SIGABRT-gdb-actual-small-raw-r1b/mirror-manifest.json)
found the immediate `std::terminate` caller in `runtime_gc_run_metrics_scope`
cleanup. The only observed C++ throw was the earlier guest RTTI registration
probe. Independent source review found a definite normal-return defect: the
cursor retired its page while `actual_entry_page` still pointed to that page,
so the later metrics flush tried to drain retired authority and terminated.
The optimized executable has no field-level debug information; this capture
does not directly inspect the page's C++ fields or qualify performance.

Both R3 source postimages clear only the matching TLS transport after deferred
accounting and stop-callback synchronization, immediately before real page
retirement. The outer entry retains its page, counters and generation; terminal
and wrong-thread rejection remain unchanged. Removing the six added lines
restores both R2 headers exactly. The complete ROS R3 source has now passed a
fresh O3 build of both runtime and CLI with all four matching experiment flags.
The original 2M workload passed under both instruction and unwind policies:
exit status zero, original checksum, 2,000,000 committed allocations, 488 actual
collections, 1,997,823 reclaimed objects and zero unsettled issued slots. The
actual unwind log confirms native unwind replaces logical instruction frames.
PAGE allocation receipts remain zero; these runs exercise the sealed compact
path, not the older page allocation leaf.

These startup-inclusive E-core correctness runs took 0.114027 s and 0.114901 s.
They are not guest-only throughput or P-core measurements. The original full
logs and exact command lines are retained in
[the actual R3 dual-policy record](../../build/wasm3-evidence/linux-r2-native-stage-20260928/sealed-r3-original2M-cli-smoke-qualified-small-raw-r1/mirror-manifest.json).
The source changes have also been installed in both working repositories by
[a strict per-file integration](../../build/wasm3-evidence/linux-r2-native-stage-20260928/root-sealed-r3-live-per-path-integration-actual-r1-20260929.json).
All eight protected user files and every other source/vendor file remained
byte-identical. This working composition has a different source identity and
still needs its own fresh build; the private ROS result does not qualify it.

Exception/GC graph admission remains a separate blocker. The collector rejects
registered exception references because its typed graph currently traces only
aggregates. Recipient membership and the registry-owned immutable value are
ownership records, not liveness roots. A complete implementation must enumerate
real native activation and retained-handle roots, trace exception payload edges,
and sweep unreachable exception/aggregate cycles across all recipient indexes.
An ordinary shared-pointer count cannot identify those roots. Both the exn
collector rejection and the defined-tag population gate remain closed until
this complete root and sweep closure is implemented and tested.

## Acceptance boundary

The original formal 2M/16M GC ring and original numeric EH module are retained
byte-for-byte. The GC codegen probe uses the real parser, initializer, validator,
LLVM optimizer and MCJIT object cache. Its first structural failure is preserved;
the diagnostic retry dumps verified IR and actual counts before applying the
same strict borrow assertions. No handwritten positive LLVM is substituted.

The latest quiet P-core GC attempt passed process ownership and STOP/CONT
witnesses, then failed thermal admission before any warmup or timed sample.
It produced zero performance samples. It cannot qualify a speedup or a ranking.
The existing temperature, resource and ownership limits are not weakened.

All compilation remains in the SSH Linux 64 GiB/no-swap cgroup. Formal timing
uses an admitted P core with serial engines, unchanged Wasm, AB/BA ordering and
complete raw results. Correctness includes packed/mutable/raw FP values, OOM
atomicity, stale tokens, foreign references/cycles, real roots and teardown.
Memory-access assembly, original diagnostic policies and no-throw call overhead
must be checked again against matching binaries. Full release also requires
bounded RSS growth, both repositories, all supported modes and the outstanding
platform/debugger/thread matrices. Source controls and older native tests do
not confer these qualifications on a new candidate.


## Current measurement handoff, 2026-10-01

The newer fused-validation build enables six matching RT/CLI experiments,
including sealed compact allocation/table access, packed numeric arrays and
pending numeric catches. Its performance is not supplied by any historical
source ID in this document. The next quiet measurement must bind the actual
fresh products and independently close their new-syntax correctness first.

[The current P-core remeasurement plan](../../benchmark/0004.wasm3-core/PCORE_REMEASURE_20261001.md)
provides the original same-Wasm hashes, independent EH dispatch/diagnostic axes,
unchanged thermal limits and a plan-only current-build receipt adapter. It
preserves the incomplete older long-ring raw executions as rejected evidence;
no current throughput, complete collector release, or industry ranking is
claimed. The sole Linux keeper retains execution and cgroup ownership.
