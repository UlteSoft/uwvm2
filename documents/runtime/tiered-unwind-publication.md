# Tiered native unwind publication

This applies to ordinary UWVM's lazy tiered runtime. ROS has no tiered mode.
The frozen r216 runtime qualifies instruction and unwind T2 with actual numeric
exception execution. The implementation preserves T1 owners when promoting to
T2. Earlier r214 evidence covers instruction T2 only.

## Lifetime and visibility

1. Each T2 job exclusively builds its full-module IR, engine and address vectors.
   The scheduler claims its compile state once. A failed job cannot replace a
   previously published engine. T1 uses separate materialized owners.
2. MCJIT finalization performs relocation, registers EH frames, then finalizes
   executable memory permissions. UWVM verifies finalization before resolving
   entries. Its object listener records executable ranges; this is separate
   from actual FDE registration.
3. Runtime writers serialize range/address builders and publish immutable
   snapshots. Execution threads register a hazard reader at the outer host
   entry, before running Wasm. Trap/exception lookup copies an identity from one
   protected version; no vector element escapes the borrow.
4. T2 installs engine/context owners and every address/map before acquiring the
   per-module cold publication lock. It publishes direct/raw/typed/indirect
   targets, then release-stores full readiness inside that lock. Readers
   acquire readiness before reading full address vectors.
5. All T1 publishers (materialization callback, entry demand and OSR readiness)
   use the same lock and recheck readiness inside it. A late T1 callback selects
   T2 addresses. An already loaded T1 address remains callable because its
   engine stays owned.
6. Lazy group materialization has a separate preparation hook. After its owners,
   addresses and CFI are complete, this hook records the entire group's mappings
   before any member's ready flag is released. Thus another host cannot enter
   the first member and call an as-yet-unmapped sibling.
7. Reset closes execution admission and drains outer leases, then stops compiler
   workers, clears snapshots and destroys engines. It never deregisters a live
   caller's CFI. Process exit uses the existing drain/owner policy.

The publication lock is absent from generated function entry, normal native
dispatch and guest memory access. No instruction-stack push/pop is introduced
for native unwind. Existing native tail-call emission is unchanged. Unwind T1
and OSR requests remain inline; normal background scheduling is enabled only
for T2 when T2 and compiler workers are enabled. Urgent T1 scheduling/prefetch
remain disabled under unwind so they cannot split a demanded direct-call group.

## Native provider contract

The reviewed LLVM MCJIT path calls registerEHFrames during finalization and
deregisters during engine destruction. LLVM libunwind's dynamic FDE cache
serializes lookup with a shared lock and registration/removal with an exclusive
lock. This provider synchronization is distinct from UWVM's address snapshots;
both are necessary. See [LLVM MCJIT](https://github.com/llvm/llvm-project/blob/main/llvm/lib/ExecutionEngine/MCJIT/MCJIT.cpp),
[RuntimeDyld memory manager](https://github.com/llvm/llvm-project/blob/main/llvm/lib/ExecutionEngine/RuntimeDyld/RTDyldMemoryManager.cpp)
and [libunwind FDE cache](https://github.com/llvm/llvm-project/blob/main/libunwind/src/UnwindCursor.hpp).

The Linux qualification records the actual loaded libunwind pathname and SHA.
It exercises concurrent registration/removal of unrelated engines while the
engines being executed remain pinned. It does not authorize retirement of
active code or assert qualification of an untested native provider.

## Focused tests

test/0014.llvm_jit/run_tiered_publication.py builds the deterministic late-T1
ordering test and the four-reader native CFI/EH probe. It preserves source
hashes, commands, IR, objects, relocations, assembly and the loaded dependencies.
The native test has no logical trace helper.

run_exception_tiered_cli.py --phase t2 --trace instruction --trace unwind
requires real full-entry dispatch, not compilation logs alone, for caught and
uncaught direct/indirect/tail fixtures. The uncaught cases check original throw
order and absence of a retired tail frame. --mode lazy --mode lazy+verification
runs both validation policies. These are functional tests, not performance
measurements, and cannot establish complete Wasm 3 support.

## Foundation evidence before the new runtime build

The r215-r2 focused probe passed both deterministic publication orders and
80,000 concurrent publication callbacks. Four native executor threads completed
28,044 actual CFI walks and typed/foreign exception executions while 48 other
MCJIT engines registered and retired their FDEs. Eight deliberate missing-symbol
failures exercised the production pending-range listener and committed no range.
This is native foundation evidence. The complete runtime's T2 evidence appears
below; concurrent Wasm-frame diagnostics use a separate integration fixture.

The r215-r1/r2 archive contains 168 SHA-verified files and has SHA256
cfededebde2e76984af9802cc8a0de57727f32cd827ae67c0aebe30bca8a7d46.
The separate O3 cold publication probe has one byte exchange on the uncontended
path, followed by the readiness selection, target store and release-byte clear.
Only the contended branch calls sched_yield. No allocation or helper call is
introduced on its successful path; generated dispatch remains unchanged. The
assembly evidence manifest SHA256 is
8c2bb5bfc9c946398d209854a818011ca542a7346e3db9de4b03565aa38b8731.

## Frozen r216 runtime execution

The ordinary O3 executable passed 32 actual T2 executions: eight numeric
caught/uncaught direct/indirect/tail fixtures, two lazy validation modes and
instruction/unwind policies. Each execution required a full-entry dispatch
address matching the published T2 entry and all three full-module functions
ready. Uncaught cases required the original throw stack [0,1,2], or [0,2] for
tail calls, without a truncation marker. Publication alone cannot satisfy these
checks.

The same binary passed 24 bounded T1 executions with T0/T2 disabled and 20 OSR
executions, including the first native activation escaping exceptionally before
the normal OSR completion counter could increment. All 76 runs passed the
Wasmtime semantic reference. They ran in the Linux 64 GiB cgroup with cpus
0,2,4,6,16-31. These logging-enabled runs establish functionality, not throughput
or qualification of other providers/platforms. They also do not replace the
separate concurrent VM diagnostic-frame test.

That separate r216 ordinary LLVM-lazy/unwind test also passed: 16 processes
rotated the sole throwing chain across eight concurrent native chains, plus one
all-normal control process. A host-import barrier proved all eight three-frame
chains had entered native code. Each elected exception retained the exact three
Wasm function indices and payload without truncation; every normally returning
chain produced 3. This specifically qualifies concurrent lazy mapping visibility,
not simultaneous tiered T2 promotion. The test overlay fixed an initial test-only
non-void thread-lambda fallthrough; the failed attempt remains in its evidence.
Its archive, shared with the r216 code-generation checks, is
build/wasm3-evidence/llvm-native-exception-concurrent-trace-codegen-r216.tar.gz,
SHA256 95a98472d21a9e85d782558ae156066d250e6eade3ecfb858828ec95edd713c9.

The frozen source SHA256 is
1d17c4d13e68ec1d6d0425d9b546b066dbdfc0f5c5afb4da862d678c63576dc2;
executable SHA256 is
a539936c5d9891e122ee7a773daed5b1bc794a540a9e9abeceb554d8ce113030.
Archive build/wasm3-evidence/tiered-numeric-eh-r216.tar.gz contains 273
individually SHA-verified files (598,950 uncompressed bytes); archive SHA256 is
dd52e629afb9d75bdd58a844c16a88cbbef44fe3df79c680bab7a7bb9a65018a.
It also preserves the two initial ENOSPC attempts, which stopped while copying
the runner and executed no tests. Successful reruns have separate directories.
