Native Wasm GC, SIMD and exception regression corpus (2026-10-06)

263 frozen WAT fixtures: 251 positive semantic oracles and 12 negative inputs.
manifest.json records their source hashes and the qualified frozen Wasm hashes.
New wasm-tools versions may produce a different binary encoding; source hashes
must still match. The runner records the rebuilt binary hash.

Run only in a Linux cgroup with memory.max <= 64 GiB and swap disabled. Put the
output below an existing /tmp directory on the bounded disk filesystem, not an
uncapped tmpfs. Select a permitted native P-core using actual host topology.
Use the shared resource_guard.py from the recovered test environment to enforce
RSS, filesystem, timeout, PSI and directory caps; this runner only validates
cgroup membership, memory settings and CPU affinity. Never launch competing
compilers or profilers during the timed comparison.

Example, from inside the recovered cgroup entry shell:
python3 "$UWVM_TASK_DIR/control/resource_guard.py" --label corpus-replay \
  --kind test --timeout 1800 -- python3 run.py \
  --uwvm /tmp/path/to/uwvm --wasm-tools /path/to/wasm-tools \
  --out /tmp/bounded-workspace/replay --cgroup "$TASK_CGROUP" --cpu 6

Add --ros for the ROS CLI. Output directories must not already exist. --only
accepts a regular expression over fixture names for an explicit subset.

The qualification build uses LLVM JIT full mode and enables these experimental
policies consistently in ALL translation units:
LOCAL_FULL_GC_EXCEPTIONS, EXCEPTION_SOURCE_RETENTION, EXTERNAL_EXCEPTION_HANDLES,
NATIVE_EXCEPTION_ROOTS, EXCEPTION_GC_GRAPH, REGISTERED_EXCEPTION_COLLECTION,
GC_ARRAY_BYTE_PRESSURE, PENDING_NUMERIC_FUSED_CATCH, GENERAL_GC_SLAB,
PACKED_NUMERIC_ARRAYS, COLLECTION_LOCAL_MEMBERSHIP, SINGLE_CAS_GC_PUBLICATION,
PRECISE_GC_TRACE_METADATA, INLINE_GC_TRACE_METADATA.
Each is prefixed UWVM_EXPERIMENTAL_ and defined as 1. The native component suite
also checks legacy, trace-disabled and ASan/UBSan configurations. A complete
Wasm performance matrix for default-disabled policy builds is separate work.

Positive fixtures verify real Wasm results, raw v128/NaN bits, live roots,
mutable/immutable defaults, sparse/dense/cleared graphs, cycles, exception-held
GC references, tail calls, capacity boundaries and owned globals/memory. GC
fixtures require actual automatic collections with precise roots enabled;
exception-root fixtures also require registered-exception collections.

This portable runner assembles/validates NEGATIVE fixtures only. It does not
claim that an arbitrary process crash demonstrates the correct native trap.
The qualification GDB suite separately verifies the actual native instruction,
guard-page fault address, diagnostic and trap/validation frame. Its Linux
scripts and receipts remain in the bounded qualification workspace.

This runner measures correctness, not JIT time, GC-only RSS or pause percentiles.
The VTune comparison attaches only after first JIT execution, then measures a
native ROI with semantic oracles retained. Clock samples discard two more full
batches. Heap policies differ between engines. Current native qualification is
x86_64; no QEMU performance claim or native AArch64/PPC64 measurement is made.
