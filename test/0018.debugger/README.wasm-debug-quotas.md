# Live Wasm debugger quota boundaries

These fixtures run real paused LLVM-full Wasm guests. Run compilers, wasm-tools,
VMs, QEMU and Python test imports only inside the verified Linux 64GiB/swap0
test cgroup, with the shared lock and the owned-process supervisor. The paired
repositories use the same fixtures. Functional results apply to a pinned source
cut and its newly linked products; concurrent working-tree changes need their
own qualification.

`run_wasm_debug_quota_cli.py` checks:

* A 513-element typed array: the 256-member graph preview announces truncation;
  original-index pages recover every exact value, including the final window.
* Chains of 128 and 129 real GC objects: the exact boundary succeeds; exceeding
  the graph budget explicitly refuses the full graph. Scalar locals, the Wasm
  operand stack and compressed original-root paths remain readable.
* A cyclic object: 128 live path handles succeed and the next creation reports
  a quota. Extending an existing handle still works at this boundary. Clearing
  frees capacity without reusing labels. A 4096-edge path remains readable;
  its next extension and both source/target mutation suffixes must report the
  quota, preserve the stop and existing path, and leave guest values unchanged.
  Eight simultaneous 4096-edge paths then fill the 32768-index aggregate
  budget; a new zero-edge handle fits, its first edge is refused, existing
  full paths remain readable, and clearing restores capacity.
* A 64-element typed table populated by a short Wasm loop, holding 128 real
  GC objects with long canonical type spelling: the actual
  32KiB text budget must truncate complete object blocks explicitly. Root and
  child member pages still recover the omitted graph with exact alias identity.

Every fixture then resumes to a second real Wasm opcode stop, verifies the
guest's updated scalar and unchanged typed operand, retires any created path handles,
and exits normally. Each VM lifetime retains its original PIDFD admission and
actual retirement proof. `--old-path-diagnostic` records the prior incorrect
stale-stop diagnosis for comparison; it is not a passing fixed-build mode.

`debug_wasm_path_data.cc` separately checks the 32768-index aggregate budget,
4096-edge limit and 128-entry limit, diagnostics, preservation after rejection,
and monotonic label retirement. This is detached DATA-only coverage: it cannot
qualify actual GC borrowing or Wasm continuation authority. The aggregate-index
limit needs the separate eight-path live-VM checks above; the unit alone does
not establish live coverage.

The fix forwards explicit preparation failure reasons in both query and
mutation controllers. It does not enlarge quotas or add JIT execution probes.
It does not qualify allocation-failure/OOM recovery, arbitrary host state,
other architecture SDKs, or debug-off performance measurements.

## Verified frozen cut (2026-10-08)

The independent Linux cgroup audit passed 160 actual VM lifetimes and 308
typed pauses: 120 matrix quota runs, eight fresh-producer native quota pilots,
and 32 native reference regressions. Both repositories pass the quota matrix
on x86_64, QEMU RISC-V64 and QEMU AArch64, default/max JIT and
instruction/unwind call stacks. All 24 matrix cyclic runs also verify the
32768-index aggregate boundary in real paused guests. The audit checks 740
quota member pages and 1704 reference member pages, actual target ELF hashes,
original PIDFD retirement, source pins and compile dependencies.

The paired fix distinguishes invalid selection, stale session/stop/generation,
and exhausted resources in path preparation; query and mutation controllers
forward that exact reason. Rejected source/target mutation suffixes preserve
the old path and guest values. Fresh production completed 24 compile/link
commands, six DATA-only quota unit executions and 100 detached DAP tests.

Qualified source identities:

* uwvm2: `0fa4e8d0e05f65ee43772a9b205e06ac7cf74f2323075510dffe37783681190a`
* uwvm2-ros: `f50a0451cd2d31c7a827dc9ded0ab29dd3136b6442c79e396286d4ebedf29fdf`

The report is `documents/runtime/wasm-debug-quota-runtime-20261008.json`
(SHA256 `2b67f71294a152017987ee6a0dcfc9c744a8385f1526073b4e66e0b18ffe3d06`).
This qualifies the immutable producer cut and separately pinned runtime
harness. Later language/native-controller changes are preserved and require
their own qualification.

Retained predecessor failures include an unrolled large saved-parameter
fixture exceeding the QEMU RISC-V64/max 600-second preparation deadline.
Its actual VM was reaped with -9 and the failed test root with 1. The passing
matrix uses the short-loop table fixture above; the preparation timeout
remains recorded. Allocation/OOM recovery, other full-LLVM architectures,
all Core 3 feature coverage and measured debug-off performance remain open.
