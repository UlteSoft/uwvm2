Reference-array batch-scan Wasm correctness corpus (2026-10-06, R13)

150 positive WAT modules: null/i31/alternating prefixes, lengths
7/8/9/15/16/17/32/33/65/257/4097, and real GC edges at the first slot,
the last slot, and around an 8-slot boundary. Two linked nodes form a cycle;
their only live root after local retirement is the reference array. Another
8192 allocations must cause real automatic GC before readback.

These modules passed on the exact final MAIN/ROS LLVM JIT full products and
on the pinned Wasmtime49 native reference. The runner is for correctness,
not throughput or GC pause latency. It now checks both the WAT SHA-256 and
the frozen assembled Wasm SHA-256 before starting the runtime command.

All Linux compilation/testing must run in the recovered <=64GiB/swap0
cgroup and the bounded workspace mounted under Linux /tmp. Example, after
using /tmp/wasm3-perf-20261004-host/enter.sh to enter that cgroup:

python3 "$UWVM_TASK_DIR/control/resource_guard.py" \
  --label YOUR_UNIQUE_LABEL --kind test --timeout 240 -- \
  python3 PATH_TO_THIS_DIRECTORY/run.py \
    --uwvm "$UWVM_TASK_DIR/build/r13/cli-uwvm2-candidate/uwvm" \
    --wasm-tools /home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/assets/wasm-tools/wasm-tools \
    --out "$UWVM_TASK_DIR/build/YOUR_NEW_OUTPUT_DIRECTORY" \
    --cgroup "$TASK_CGROUP" --cpu 6

For ROS use its matching binary and add --ros. The output directory must
not exist. Keep native execution on Linux; do not treat QEMU as performance
evidence. After a restart, reuse the persisted recovery control/restore.sh
under /home/macromodel/Documents/uwvm3-gc-recovery-20261006-r8.

All CLI translation units were freshly built with native TLS and these
UWVM_EXPERIMENTAL_* values uniformly equal to 1:
LOCAL_FULL_GC_EXCEPTIONS, EXCEPTION_SOURCE_RETENTION,
EXTERNAL_EXCEPTION_HANDLES, NATIVE_EXCEPTION_ROOTS, EXCEPTION_GC_GRAPH,
REGISTERED_EXCEPTION_COLLECTION, GC_ARRAY_BYTE_PRESSURE,
PENDING_NUMERIC_FUSED_CATCH, GENERAL_GC_SLAB, PACKED_NUMERIC_ARRAYS,
COLLECTION_LOCAL_MEMBERSHIP, SINGLE_CAS_GC_PUBLICATION,
PRECISE_GC_TRACE_METADATA, INLINE_GC_TRACE_METADATA.

Runtime C++ TU uses O3; main/host TUs use O1/g0/fno-inline to bound compiler
memory. This is not a uniform O3 release ranking. Guest JIT is full and its
cache is disabled. Native x86_64 qualified; AArch64/PPC64 still need native
hardware checks. This corpus does not cover every Wasm3 opcode, concurrent
Wasm mutator GC, or pause-p99 metrics. Existing GC/SIMD/EH regression cases
remain in wasm_gc_simd_exceptions_20261006 beside this directory.
