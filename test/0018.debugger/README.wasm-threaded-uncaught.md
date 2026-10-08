# Real guest threads and uncaught Wasm exceptions

`../0017.runtime/debug_wasm_threaded_uncaught_runtime.cc` runs two actual
native guest threads in the full LLVM JIT. One executes a Wasm loop with distinct
i32/i64 locals; the other throws through a real Wasm caller. The fixture installs
the runtime's owning debug observer and observation profile before compilation.
It does not use WASIp1 or manufacture thread IDs, frames, code owners or captures.

`wasm_threaded_uncaught_cases.py` supplies six independent, validator-checked
modules: numeric `throw`, tuple `throw` containing v128 and a GC reference, and
`throw_ref`, each with a matching-handler control and an unhandled control.

At an unhandled exception the throwing worker requests one cooperative pause.
Both live participants issue their actual typed captures before parking. The
manager confirms the complete pause, then checks distinct thread locals, the
throwing leaf's i64 operand prefix and its still-live caller's data stack. Missing
or duplicated captures must refuse the query. Continuing the unhandled exception
retains the VM's fatal exit; this is an expected failure exit, not a successful
normal return. The handled controls produce no uncaught event, preserve both
result ABIs, physically join their workers and reject reads through old captures.

This fixture exercises the real runtime embedding API. It does not qualify the
CLI/DAP presentation of multiple threads, all thread instructions, native host
exception observation, checkpoint restoration, other architectures or debug-off
performance. The local imported-function dispatcher has a native `noexcept`
boundary; arbitrary C++ exceptions from that provider are not a supported Wasm
exception source. Native callback reentry retains an incomplete foreign island
and cannot supply a complete before-unwind Wasm capture merely from its frame IDs.

Run all compilation, module parsing/validation and execution on Linux inside the
existing 64GiB/swap0 test cgroup, with the shared lock and owned-process supervisor.
An invocation has this shape:

```text
threaded-uncaught numeric-uncaught.wasm instruction default uncaught
threaded-uncaught numeric-caught.wasm unwind max caught
```

The unhandled invocation prints `WASM_THREADED_UNCAUGHT PASS` only after its
before-unwind assertions pass, then continues to the real uncaught-exception
fatal diagnostic. That marker alone does not qualify the process: the runner
also checks its original PIDFD retirement, actual exit signal and diagnostic.

## Closed qualification: 2026-10-08

The six real full-LLVM embedding products passed the finite matrix in both
repositories: native Linux x86_64, QEMU RISC-V64 and QEMU AArch64, default/max
JIT, and instruction/unwind call stacks. Six modules per combination give
144 actual VM lifetimes and 288 actual guest worker lifetimes. The 72 unhandled
cases each capture both workers before parking; the 72 handled controls capture
the remaining worker after the throwing worker physically returns and joins.
Together these produce 216 authentic typed captures.

The unhandled cases check the throwing leaf's original operands and the
awaiting caller's locals and operand prefix. Tuple throws retain exact i64 and
v128 bits and a live GC structure field with value 42. The actual `throw_ref`
exception retains its original tag identity and payload 111. The other worker
has distinct i32/i64 locals. Missing and duplicate cohort entries, and a forged
owner sharing the same native address through a foreign control block, refuse
all typed rows. Handled controls produce no uncaught event, preserve both
result ABIs, join both workers, and reject old captures after departure.

The 72 unhandled cases then reach the genuine fatal diagnostic: x86_64 and
RISC-V64 exit by SIGILL (recorded return code -4), while AArch64 exits by SIGTRAP
(-5). The runner binds this oracle to the actual target ELF machine and checks
original PIDFD retirement. These are expected fatal continuations; only the
72 handled controls return zero. Earlier failures are retained separately:
the invalid `catch_ref` block shape, buffered before-fatal evidence, and an
incorrect SIGILL-only AArch64 oracle. Explicit `fast_io::out()`, the corrected
validator shape and target-bound signal checks fix the tests. No production
C++ change was required by this matrix.

All compilation, independent wasm-tools validation, UWVM parsing/validation,
JIT execution, QEMU execution and qualification audits ran inside the verified
64 GiB/swap-zero SSH Linux cgroup with the shared lock. All original producer,
runtime, audit and artifact roots were PIDFD-retired and reaped zero; all cgroup
memory events remained zero. No foreign process was adopted or signalled.

This qualification uses the separately frozen quota-build-r4 production cut:

- uwvm2: `sha256:0fa4e8d0e05f65ee43772a9b205e06ac7cf74f2323075510dffe37783681190a`
- uwvm2-ros: `sha256:f50a0451cd2d31c7a827dc9ded0ab29dd3136b6442c79e396286d4ebedf29fdf`

These identities hash the canonical sorted repository-relative src/third-parties
path/hash mapping. The new fixture and generator hashes are separately bound in
[the paired report](../../documents/runtime/wasm-threaded-uncaught-runtime-20261008.json),
SHA-256 `ec642d9d7bf8a155d12c26a707b49766dab53701ab9d00dc17fcc51f0c57a320`.
Successful reused compiler objects/products retain their original source and
precompilation dependency bindings; later concurrent working-tree changes are
preserved and are not qualified by this cut. ROS keeps its full interpreter and
full LLVM modes.

The complete evidence archive remains on SSH Linux at
`/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/wasm-core3-debug-20261007-r1/wasm-trap-stop-20261007-r1/bundles-threaded-uncaught-r1/wasm-threaded-uncaught-20261008-evidence.tar.gz`.
It is 18,152,097 bytes, with 519 members, SHA-256
`61cd3be2f193c69f8b612897bc83223e755f001a2b5dcc6673239e20ffaf8b00`.
Every member hash was checked twice and by the independent closure audit.
It excludes compiled products, objects and SDKs. Six compressed qualified
products remain remotely; only 1,269,767,792 bytes of our redundant raw products
were retired after exact hash and process-closure checks. The small report and
closure metadata were transported locally and hash verified; no functional
test ran locally.

Remaining outside this qualification: host-origin exception stops, joint
thread/atomic-wait/native-reentry cases, more than two guest workers, the
multi-thread CLI/DAP frontend, other full-LLVM ISAs, whole-instance checkpoint
restore, all Wasm 3.0 feature coverage, allocator failure/OOM recovery, measured
debug-off performance, and later concurrent working-tree changes.
