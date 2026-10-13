# Current-source EH and managed-thread next measurements, 2026-10-02

This is a fixed command-list method for the sole Linux keeper, not a new runner.
No current native/performance pass is claimed. Use a new-boot admitted64GiB/
no-swap scope and actual fresh compiler/product/runtime/dependency/DSO receipts.
Temperature is observation only; keep actual frequency distributions, original
PIDFD/birth/UID/cgroup/affinity ownership, wait4 retirement and bounded output.
Do not insert a profiler into the pure-counter closed roster or run benchmarks
beside compilation/QEMU/Windows. Reuse the already-reviewed protocols with an
explicitly reviewed current-source binding, never merely replace an old SID.

## EH fixed cells

The existing same-byte original modules are:

| Cell | Iterations | Actual catch assertion | Scalar checksum | Wasm SHA256 |
|---|---:|---:|---:|---|
| plain_normal |200000000|0|1231817216|03c56518a1d2ed9d0149791e3a241ddff773395e51bd48aeb67a9160d5bdba01|
| eh_normal |200000000|0|1231817216|f2b9df696835bdd0f759885b77862e4a3de56b506a96f3edc41e54b20a63be8a|
| eh_throws |8000000|500000|2464256|560243a56c6b29dbe0337548fa96626db2c168e56ea69d88208cffa4dfc48560|

Their provenance is in prepare_current_pcore_plan.py and the exception source
runner. Keep original modules unchanged. Recover and SHA-check the bytes; run
actual official validation and current-source self-checking cold execution.
The checksum catches input/loop elimination. plain_normal versus eh_normal
controls for enabled EH without a throw; eh_throws crosses a real callee-to-caller
catch boundary once per16 input states. This is different from an intrafunction
try_table direct branch or an externally escaping exception.

The command vector for each current CLI cell, after binding an actual product and
actual fixture path, is:

```text
taskset -c 0 ACTUAL_CURRENT_CLI -Rcc jit -Rcm full -Rllvm-full-policy pb-o3 -Rllvm-call-stack TRACE -Rllvm-exception-dispatch DISPATCH -Rllvm-cache-path disable -Rct 0 FEATURE_FLAG --log-verbose -Rclog err --run ACTUAL_PINNED_FIXTURE
```

TRACE is instruction or unwind; DISPATCH is auto or native-unwind. FEATURE_FLAG
is -WFD-exceptions for plain_normal and -WFE-exceptions for eh_normal/eh_throws,
matching the original enabled-versus-disabled control. Capture,
preload/audit and runtime/profiler overrides must be stripped and actually
observed absent; keep the exact current loader environment. All12 cells require
cold exit0 and the actual self-checking byte SHA. Actual dispatch must be recorded:
auto may select pending numeric, while native-unwind must really select native;
auto timings cannot be labeled native exceptions. Preserve full diagnostic stack
policy rather than silently disable trace capture for speed.

Then execute12 unprofiled cells in reversed/rotated small process pairs, separate
internal Wasm timer from parent wall/user/sys. For pure HW initially select only
unwind/auto and unwind/native eh_throws plus unwind plain_normal/eh_normal,
using the same single-TID CPU-core3c/c0 enable-ACK-before-GO protocol. Verify actual
FD/group/PMU enabled/running. A short auto row stays unqualified until a longer
same-semantic module receives independent cold checks. VTune HW Hotspots and
uarch use separate profiled guests and actual P0/PID/TID filtered reports, with
real unwind DSO SHA/build-id and skipped/lost/MUX warnings. No software fallback
replaces available HW. Determine whether native time belongs to trace capture,
RaiseException/personality search/cleanup, payload/root work or allocation by
actual self/inclusive paths, not by the whole wall ratio.

A newer prepared Core3 source is generate_native_eh_leaf_benchmark.py. Its actual
WAT contains tag/throw and typed try_table catch, exports run/checksum/catches and
checks an independent affine LCG oracle. Generate it into a fresh directory at
8M or a longer actual multiple of16, validate/assemble with the actual pinned
wasm-tools, then invoke the identical binary with Wasmtime copying and the current
CLI. Do not claim those newly produced bytes are the old560243a5 fixture.
The private-publication benchmark fixture is a separate candidate-only A/B: its
public/private selection and actual clone/CFI publication are required. It cannot
replace the public ordinary-native baseline or claim current default acceleration.

## Real VM thread creation and parked wake cells

Use existing test/0017.runtime/wasm_thread_performance.cc and
wasm_parked_wait_notify_performance.cc. Build their actual current source against
the exact matching fresh nativeTLS runtime/host ABI using the current3-TU recipe.
Keep source/compiler/MD/RT/artifact/link/actual Wasm closure. The old runners expect
historical build.json/source-ID shapes; do not blindly feed them an external-SID
R5 executable and call it qualified. No JIT archive or old runtime can be mixed.

For creation, build two fixture executables from the same source/profile: one
with UWVM_THREAD_BENCH_QUALIFY_CREATION for cold actual pthread-create counts and
one without it for timing. The qualifier must observe exactly the requested new
threads for threaded rounds and none for same-thread rounds. The timed binary
must not contain the creation-interposition/probe symbol. Fixtures use the same
1024-word32-pass integer kernel, shared Wasm memory and atomic.fence, and real
VM full/raw host entry. Each worker owns a disjoint4KiB slice; checksum/entry/exit
markers are checked. New native threads are embedder-created and admitted by the
VM; the threads proposal does not invent a Wasm spawn opcode.

Actual executable argv shapes are:

```text
taskset -c 0 ACTUAL_THREAD_QUALIFIER unwind 1 4 FRESH_QUALIFIER_DUMP.wasm
taskset -c 0 ACTUAL_THREAD_QUALIFIER instruction 1 4 FRESH_QUALIFIER_DUMP2.wasm
taskset -c 0 ACTUAL_THREAD_TIMED unwind 9 1024 FRESH_TIMED_UNWIND_DUMP.wasm
taskset -c 0 ACTUAL_THREAD_TIMED instruction 9 1024 FRESH_TIMED_INSTRUCTION_DUMP.wasm
```

The executable itself evaluates native/vm-entry/vm-raw, same-thread/new-thread and
one/four-worker configurations. It accepts samples1..99 and rounds1..1024, writes
the exact generated Wasm to its fourth parameter, warms twice, then prints samples.
Require fresh paths because its dump writer uses truncation. SHA-check and
independently validate those actual identical dumped modules. CPU0-only results
measure lifecycle/queueing on one P core. A separate taskset0,2,4,6 run can measure
scaling; retain actual per-TID allowed CPUs and frequency for each used core.

Thread wall includes construction, work, joins, thread object destruction and
TLS retirement. Constructor/join/start latency include scheduler behavior.
Admission/body/release markers have separate scopes, with body including two
fixed host transitions. Do not subtract overlapping workers' summed intervals
from wall or infer pure OS thread-creation latency. Whole-process wait4 includes
all native TIDs and any owned bootstrap, not single-TID pure-counter semantics.
First perform unprofiled measurements only; a later multi-TID counter protocol
needs actual per-TID event inheritance/coverage review and is not authorized by
the current single-TID HW wrapper.

For a genuine parked wake, the Linux fixture samples the waiter as stateS around
an actual futex wchan read before notify. It requires guest notify to return one
real waiter and the wait result to be success; empty polls/failed observations
are retained separately. The existing pinning code assigns notifierP0 and waiterP2.
The argument list is fixed:

```text
taskset -c 0,2 ACTUAL_PARKED_FIXTURE --dump-wasm FRESH_PARKED_DUMP.wasm
taskset -c 0,2 ACTUAL_PARKED_FIXTURE unwind
taskset -c 0,2 ACTUAL_PARKED_FIXTURE instruction
```

Dump/validate/cold controls must precede timing. Its actual2 warmups plus9 samples
of128 wakeups measure notify entry through waiter return after proven parking;
thread creation, setup and the /proc qualification interval are outside that
wake ROI and are reported separately. Preserve per-wake assertions, transitions,
parked qualification fields and wall process records. A nonblocking mismatch or
empty-notify CLI loop does not qualify blocking wake behavior. Fresh current-source
create/wake results, unwind diagnostics and VM-owned retirement are necessary
before any release/performance claim about the full threads extension.
