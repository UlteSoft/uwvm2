# Dense/reference GC scanner controls: separately frozen source r2

The source-only supplement adds `trace-dense64` and `trace-dense65` to the six
original native API phases. It uses a new source filename and schema; the
original API r1 packet, bitmap r1 packet and historical 6ABC snapshots remain
unchanged. No new production switch or default behavior is enabled.

Every dense object has exactly 64 or 65 mutable nullable `eq` reference fields.
Field 0 contains an authentic `i31` created by the existing runtime constructor,
with the LCG value reduced to 31 bits. Each other field holds the object's
actual published self reference. This gives 63 or 64 genuine heap edges plus
one nonheap reference. The 64-field case exercises every bitmap bit including
bit 63; the 65-field case uses the original owned-index representation. Sparse
64/65 controls still have numeric fields and one genuine final self edge.

The native fixture copies all six canonical declarations, acquires an actual
shared admission, keeps its module-like foreign lease owner alive, and protects
that exact reader with a genuine exclusive lease for every collection. Each
round creates 4,096 objects, retains 1,024 actual roots, and times only the closed
collector and its status checks. The first round actually reclaims 3,072 objects,
later rounds 4,096, and an untimed final root drop reclaims 1,024. Every dense
heap edge and the active i31 member are read back after each sweep. All successful
allocations must equal actual timed plus qualification reclamation; a retained
privileged token sample must be rejected after final retirement.

Batch allocation and its 63/64 synchronized field writes, verification reads,
construction, and final reclamation are outside the internal collection clock.
They are inside process wall/CPU/RSS and whole-process hardware PMU. Never call
whole-process PMU divided by the internal clock an isolated scanner cost.
Sparse r1 measurements cannot qualify dense64 throughput: serial bit selection,
clearing and branch costs may differ substantially from sequential index loads.
Metadata class size, alignment and owned-index bytes must be measured on each
actual target; the extra word is not automatically a heap-memory saving.

`generate_gc_dense_sparse.py` emits four independently checksummed Core3 graph
shapes, with explicit recursive declarations, typed tables, `struct.new_default`,
`struct.set/get`, `ref.eq`, and, for dense shapes, `ref.i31`, `ref.cast (ref i31)`,
and `i31.get_u`. The [Core3 execution specification](https://webassembly.github.io/spec/core/exec/instructions.html#exec-ref-i31)
defines truncation to 31 bits and unsigned unboxing. Each generated module checks
all final live self edges, scalar and root checksums, and its exported return
value. It does not import WASI or add unrelated proposals.

The WAT and native fixture match canonical field kinds, LCG payload sequence,
actual planned allocation count, 1,024-root ring, genuine self-edge count and
scalar checksums. Native explicit collections and VM automatic collections have
different schedules. Native initial fields are supplied before publication;
WAT uses default allocation followed by field writes. These fixtures cannot be
compared as the same native operation interval or expected GC count. Independent
Wasmtime/uwvm comparisons must execute the exact same actually parsed Wasm bytes.

The Python producer only emits source, scalar oracles and prospective argv. It
never invokes a compiler, native VM, profiler or SSH. Official wasm-tools parse,
validate and roundtrip, actual Wasm hashes and executions remain pending. The
Linux sole keeper must first run all eight small native semantic cells and all
four fresh Wasm shapes; then do matched default/C/D plain reversed-order P0
runs, actually unscaled hardware counts, official VTune CLI hardware hotspots
and uarch-exploration, in-window frequency/noise/UID/TID/cgroup/64GiB/swap-zero/OOM
receipts, actual paired Release C++ closure and generated assembly. Bounds are
1–256 trace rounds, never a guessed VM single-mutator authority. Full product
builds still require their matching LLVM SDK. This SDK-free component does not.

Keep profiles fixed: default uses neither precise nor inline flag; C sets only
`UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1`; D adds
`UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA=1`. All source and header identities,
macro witnesses and actual static C++/ABI/unwind symbols must be recorded.
No allocation-fault wrappers belong in a performance binary. A future publisher
candidate is an independent source/macro axis and must not enter this freeze
silently. The bitmap optimization remains default OFF and unaccepted until
actual dense, sparse, allocation, mutation and real VM evidence passes.
