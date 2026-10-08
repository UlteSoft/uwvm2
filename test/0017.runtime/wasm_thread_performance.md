# Real VM thread entry benchmark (LLVM full)

This fixture measures existing VM execution admission and return through both
`full_compile_and_run_main_module` and `llvm_jit_call_raw_host_api`. Host-created
threads enter the VM through its normal lifetime management. The fixture does
not invent a Wasm thread-spawn instruction: the threads proposal provides shared
memory and atomic instructions, while the embedder creates threads. This follows
the [official threads proposal](https://github.com/WebAssembly/threads/blob/main/proposals/threads/Overview.md).

`wasm_wait_notify_performance.cc` is a separate real blocking/waking test. One
host-created thread enters guest `memory.atomic.wait32` on a shared cell, and a
second host entry repeatedly invokes guest `memory.atomic.notify` until it
returns one actual parked waiter. Every wake checks the wait result. The timed
interval begins immediately before the successful notify entry and ends after
the waiter has returned; it includes VM host-entry, scheduler, and guest wake
work, but **not** creation of the waiting thread. Per-call notify duration and
the number of preceding empty polls are retained separately. This is not the
nonblocking mismatch/empty-notify loop in the CLI Core 3 matrix, and neither
test replaces the VM thread-creation benchmark below.

`run_wasm_wait_notify_performance.py` compiles that fixture against the exact
frozen O3 Linux runtime object and verifies source ID, runtime/product SHA,
compiler template, input hashes, and output binary. `--build-only` prepares it
on E core 16, asks the fixture to dump its exact generated `.wasm`, then
validates that bytecode with the SHA-bound `--wasm-tools` executable.
`--run-only` rechecks the executable and Wasm hashes and launches no compiler. The
default timing runs nine instruction/unwind process pairs with reversed order
on the assigned P cores 0 and 2. Each process warms up twice, then records nine
samples of 128 successful wakeups. Raw per-wake rows, process logs, CPU
frequency/thermal telemetry, cgroup OOM/throttling counters, and p50/p95/p99
are saved. `--pairs 1` is a short semantic smoke, not a formal paired result.
The process affinity allows either thread on either P core; it does not assert
per-thread pinning. The C++ fixture passed macOS syntax probes under 3.8 GB
watchers against ordinary v6 and ROS v7 headers, peaking at 2,220,933,120 and
2,889,826,304 bytes respectively. The ROS probe used its bundled LLVM 23
generated headers and `-Werror`; its [source-bound summary](/Users/liyinan/Documents/MacroModel/src/uwvm2-ros/build/wasm3-evidence/wasm-wait-notify-syntax-macos-ros-v7-20260927/summary.json)
has SHA-256 `49c28d79d9c6484521f4f268f19b912c3819cf7acbaf668e100a400fe5ccd792`.
These checks prove syntax only; exact Linux O3 linking and real guest wake
timing await the shared-cgroup window after Win64/Win11 work exits.

Each invocation executes actual shared-memory Wasm syntax and `atomic.fence`,
then initializes and transforms its own 4 KiB slice for 32 passes. One and four
workers use disjoint slices, so ordinary loads/stores do not race. A nonzero
rolling checksum is compared with a native integer oracle on every invocation.
VM host callbacks mark the guest interval and verify entry/exit counts. Module
loading, initialization, compilation, native checksum preparation and JIT warmup
occur outside sample timers.

The matched comparison runs the same VM API and same Wasm function on the
calling thread versus new `std::thread`s. Both perform the same number of calls.
A separate native kernel uses the same integer transformation to characterize
native thread creation and scheduling. Its code is not asserted to be equivalent
to generated Wasm code; its throughput ratio is not a VM regression baseline.

The recorded stages are deliberately explicit:

- `wall_ns` includes native thread construction, execution, joining and thread
  object destruction. VM thread-local destruction completes before join returns.
- `thread_constructor_sum_ns`, `join_sum_ns` and `start_latency_ns` include
  scheduling effects and are not pure operating-system thread-creation costs.
- `admission_ns` runs from the public API call to the first host marker. It
  includes VM/TLS setup and fixed call/host-transition work, not just a lease.
- `guest_interval_ns` includes the memory kernel and the fixed return/entry
  transitions of two marker imports. It is averaged per worker; concurrent
  durations are never subtracted from wall time.
- `release_ns` includes the last marker's return, guest return, VM stack/entry
  cleanup and public API return. Thread-local destruction remains in wall time.

A separate qualification executable interposes the real `pthread_create`
symbol, including libc++ shared-library calls. Every threaded round must create
exactly the requested number of native threads; same-thread rounds must create
none. The measured executable contains no interposition or creation counter.
Its `native_creations: 0` field is an uninstrumented placeholder, not an
observation of zero threads; use only the qualification executable's counts.

`run_wasm_thread_performance.py` accepts one frozen source tree, its intended
full source ID, and its genuinely O3 runtime object. It rejects a product build
or runtime object with a different source ID or SHA-256. It records those inputs
and hashes, compiles only the fixture,
checks both instruction and unwind policies, and records cgroup/CPU affinity.
The default is a small semantic run, with no performance conclusion. `--measure`
selects nine AB/BA sample pairs of 32 rounds after two warmup pairs, and must run
in an otherwise idle benchmark window. `--affinity single` (the default) pins
all workers to the selected P core; its four-worker results describe lifecycle
and queueing overhead on that CPU. `--affinity four --cpu 0` allows guest workers
on P cores 0,2,4,6 and records each core's available frequency/governor; run it
separately to assess four-core scaling. Preserve raw samples and inspect variation
before drawing conclusions. The native reference disassembly is archived; this
alone does not qualify the VM's generated memory instructions.

Run only inside the remote 64 GiB/no-swap cgroup:

```sh
UWVM_TEST_CPUSET=0,2,4,6,16-31 python3 \
  test/0017.runtime/run_wasm_thread_performance.py \
  --base-source-root /dev/shm/uwvm-sources/shared-r211/uwvm2 \
  --runtime-build /dev/shm/uwvm-builds/shared-r211/uwvm2/exception-runtime-o3 \
  --expected-source-id sha256:REPLACE_WITH_INTENDED_FROZEN_SOURCE_ID \
  --out /dev/shm/uwvm-builds/vm-thread-performance-example
```

Use `--variant timed` when native creation counts have already been qualified,
or `--variant qualify` for only the instrumented test. For the final isolated
timing window, rerun the identical command with `--run-only --measure --variant
timed` for one-P-core samples, then with `--run-only --measure --variant timed
--affinity four --cpu 0` for four-P-core samples. Input and binary hashes must
still match, and no compiler is launched.

The runtime revision in the report is authoritative. Qualification against an
older O3 runtime does not measure newer native-EH or other uncompiled changes.

Development qualification on 2026-09-22 used both repositories' frozen r211 O3
runtime objects. Ordinary creation counting passed both trace policies in r3;
r4 passed ordinary uninstrumented execution and ROS counted/uncounted execution.
Every selected variant/policy covered twelve configurations, with exact native
creation counts where instrumented, marker counts and nonzero checksums. These
small runs are semantic qualification, not performance results. Timed binaries
were also checked to contain no definition of the counter/interposition symbol.

The complete source, commands, binaries, Wasm fixtures, logs and native reference
disassembly are archived under `build/wasm3-evidence/` in the ordinary repository:

- `vm-thread-performance-r1-r3-ordinary-qualification.tar.gz`: SHA-256
  `fdd546514d6372254f2a7111aac70773075999386ee83513c9245b0517619675`.
- `vm-thread-performance-r4-semantic.tar.gz`: SHA-256
  `bdede666470af46f8f72c28501cb8cb5ad1e2bac0ac5a5bd084661cce1b094bd`.

Every archived file was checked against its manifest before the backed-up
qualification executables were removed. After the isolated r5 timing window,
the two uninstrumented binaries were also removed remotely only after verifying
their exact bytes in the r4 semantic archive and checking the remote hashes again.


The r4 measurement used nine pairs of 32 rounds on otherwise idle CPU0. Its
individual sample intervals were only 0.107–3.578 ms; VM wall-time variation
reached 24.91% coefficient of variation and 15.20% median absolute deviation.
Those samples are preserved as a short-run diagnostic, not used as a regression
acceptance threshold.

The independent `run_wasm_thread_performance_remeasure.py` wrapper reused the
exact r4 binaries, verified every recorded source/runtime/binary SHA before and
after execution, and wrote r5 to a new output directory. It ran 21 pairs of 1024
rounds per configuration, with two warmup pairs, one policy/process at a time.
Both repositories and both policies passed all checksum and marker assertions.
No compilation or other agent test ran during this exclusive window. The
wrapper records cgroup limits, CPU0 affinity, CPU statistics, and process lists
before and after each process; these snapshots cannot exclude every transient
host scheduling event.

The following values are medians of paired new-thread minus same-thread wall
time, in microseconds per round. Four-worker rounds are scheduled on CPU0;
they do not measure multicore throughput. Native references measure the same
memory transformation in C++ and are separate from the VM comparison.

| Repository / trace policy | Full API, 1 worker | Full API, 4 workers | Raw API, 1 worker | Raw API, 4 workers |
| --- | ---: | ---: | ---: | ---: |
| uwvm2 / instruction | 12.409 | 54.688 | 12.519 | 55.752 |
| uwvm2 / unwind | 13.776 | 54.690 | 12.453 | 55.961 |
| uwvm2-ros / instruction | 12.546 | 54.303 | 12.528 | 53.823 |
| uwvm2-ros / unwind | 12.627 | 55.047 | 12.423 | 56.084 |

The native `std::thread` reference adds 7.85–8.15 us for one worker and
34.48–35.82 us for four workers. The VM guest-interval median is approximately
12 us per worker, while VM sample intervals span approximately 13.8–112.6 ms.
VM wall-time median absolute deviation is 0.43–3.45%. Outliers remain: ROS
instruction/full/four-worker same-thread wall time has 23.95% coefficient of
variation, and ROS instruction/raw/one-worker threaded wall time has 14.82%.
No outliers were removed. These results characterize thread lifecycle overhead
on r211; they do not establish an older-versus-newer VM regression baseline,
measure native-EH changes, or establish a small performance difference between
trace policies run in separate processes.

The short run's adjacent native stage timestamps were approximately 10–14 ns
apart. This observes clock-call plus marker work, not an independent calibration
of `steady_clock`; multiple such reads remain inside each worker measurement.
That scale is much smaller than the VM's approximately 12 us guest interval,
but stages near tens of nanoseconds should not be interpreted as precise VM
operation costs. More rounds amortize sample noise, not the per-call clock cost.

The complete r4/r5 measurement reports, raw samples, commands, resource/process
snapshots, input hashes and exact wrapper are in
`build/wasm3-evidence/vm-thread-performance-r4-r5-measurement.tar.gz`:
SHA-256 `82cb740be06047d31a60f394564bca58d42921420bb67396e7e3ada128d58142`.
All 51 archived files were verified against their manifest and rechecked against
the remote originals. The archive contains reports, not replacement binaries;
the r4 semantic archive above preserves the measured executable bytes.

## Controlled r231 O3 rerun

Both repositories' immutable r231 sources were rebuilt with host `-O3`, and
the LLVM full JIT selected generated `pb-o3`. The real VM qualification counted
native `pthread_create` calls and checked guest checksums/markers for full and
raw host entries, both diagnostic stack policies, shared memory and
`atomic.fence`. The later timing runner reused the exact qualified binaries
without compiling. On CPU0 it measured 21 samples of 1024 rounds per
configuration. Median new-thread minus same-thread wall time, in µs per round:

| Product / trace | Full 1 | Full 4 | Raw 1 | Raw 4 |
| --- | ---: | ---: | ---: | ---: |
| ordinary / instruction | 12.442 | 52.579 | 12.327 | 54.712 |
| ordinary / unwind | 12.393 | 53.144 | 12.237 | 55.220 |
| ROS / instruction | 12.579 | 53.386 | 11.976 | 54.324 |
| ROS / unwind | 12.497 | 53.984 | 12.216 | 54.150 |

Four workers were deliberately scheduled on the same CPU, so these figures
measure lifecycle/scheduling overhead rather than parallel scaling. They do
not compare old and new VM implementations; raw samples and variation are
preserved. The archives also include the exact source IDs, commands, binary
hashes, cgroup snapshots and complete file checksums:
`build/wasm3-evidence/ordinary-o3-r231.tar.xz` (SHA-256
`804cc77100ca244f80643483bd402aa26bed86d2534c286f0c6bca17032bc288`)
and `build/wasm3-evidence/ros-o3-r231.tar.xz` (SHA-256
`93f1147fb5eddcf8f01d25ccbf8b92a0890f4390b7c2d51fffdecaab320a94ba`).
