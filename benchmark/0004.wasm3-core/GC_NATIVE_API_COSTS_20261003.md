# Native GC cost components: source-only keeper supplement

This independent diagnostic separates real GC APIs before redesigning their hot
paths. It is not an interpreter/JIT benchmark, a same-Wasm engine comparison,
proof of automatic roots, or performance qualification of the bitmap candidate.
The original R3 binary, 6ABC snapshot and new bitmap r1 package remain immutable.
Only the sole Linux keeper may compile or run it in the existing 64 GiB/swap-zero
scope, on E cores for compilation and a quiet P core 0 for measurement.

`gc_native_api_costs_20261003.cc` has six distinct processes/phases:

* `allocate-numeric`: initialize/publish one mutable numeric field through the
  actual numeric slab and native token/index machinery. No per-object post-write.
* `allocate-reference`: initialize/publish a numeric field and nullable reference
  field through the actual general object allocation/type/publish machinery.
* `mutate-numeric`: 1,024 preallocated roots; each unit does one mutable get and
  one set with independent modulo-32 scalar/root checks.
* `mutate-reference`: 1,024 preallocated real objects; each unit reads the old
  typed edge, writes an authentic varying LCG-selected target, and verifies that
  edge by readback. It does not rewrite the same periodic target every round.
  Reference compatibility/ownership and mutable synchronization remain active.
* `trace64` / `trace65`: create 4,096 real self-cyclic objects per round, retain
  1,024 roots, and time the actual closed collector. The first round reclaims
  3,072 objects; every later round reclaims 4,096. The final untimed root drop
  reclaims all 1,024 remaining objects, which then reject stale-token readback.

All four canonical layouts are copied from temporary parser-owned declarations
before use. The module-like foreign lease owner and real shared admission outlive
the store. Every collector transaction acquires an actual exclusive lease that
`protects_shared` authenticates against that exact live reader. There are no
additional mutators/readers in this private native fixture. This is not proof
that the VM admitted or enumerated every Wasm/native participant.

Every mode performs actual untimed reclamation and stale-token controls; total
actual timed+qualification reclamation must equal the actual successful
allocations. The producer `prepare_gc_native_api_costs.py` separately calculates
scalar checksums and outputs argv only. It never starts a compiler, process,
profiler, SSH or native VM. Bounds are 4,096–1M allocations, 4,096–16M mutations
or 1–256 bounded collection rounds. Unknown modes, truncated/overflow decimal
arguments, checksum disagreement or missing admission fail. Decimal input and
output use fast_io; no custom decimal/endian parser or secondary filesystem API
is introduced.

Internal allocation/mutation clocks include the API, status checks and scalar
loop arithmetic. Collection clocks include real closed admission and collector
status checks. Setup, batch allocation for tracing, reachability readback and
final reclamation are outside those internal intervals. Plain whole-process
wall/CPU/RSS and whole-process hardware PMU include all of them. **Do not divide
whole-process PMU counts by the internal collector interval or call these an
isolated lock/CAS/malloc cost. The collector wrapper deliberately remains a
separate native symbol for attribution; its noinline call is diagnostic overhead.** VTune hotspot/source/assembly evidence and
separate matched C/D comparisons are needed for causal conclusions. The 64-field
metadata word affects collection scanning; allocation and mutation modes should
expose unrelated hot costs and regressions, not demonstrate scanner speedup.

Use fresh same-source default/C/D component binaries, with all other switches,
TLS, compiler optimization, ABI and paired 23.1.1 Release C++ headers/static
libc++/ABI/unwind tuple fixed and actually recorded. This component does not link
LLVM, so a matching LLVM SDK is not its dependency; the full product still needs
that SDK. Preserve actual driver/link/dependency proof and exclude accidentally
selected old `__2` libraries, duplicate system unwind or test allocator wrappers.
Macro witnesses, source/vendor/header closure, hashes and assembly are actual
keeper receipts, not inferred from this producer's prospective argv. Each
profile reports actual metadata class size and whether the type layout uses it;
class size when the plan is off is not allocated per-type metadata overhead.

First run the six small semantic cells with the independent producer's numbers.
Only after they pass, use at least nine low/high reversed-order quiet P0 pairs,
save all stdout/stderr/process/internal timers/RSS and actual UID/TID/affinity,
in-window frequency, cgroup OOM/swap/throttling and foreign-task/noise receipts.
Perform separate official installed VTune CLI hardware hotspots and
uarch-exploration captures. Safe temperature does not cancel measurement;
missing hardware/frequency/callstack evidence restricts the claim. The native
component remains distinct from the Core3 WAT/Wasmtime copying/managed-language
production comparison and all interpreter combine-delay/LLVM assembly gates.
