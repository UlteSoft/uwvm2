# Platform-thread admission and guest wait/notify measurements

These results are a Linux x86_64 baseline for two immutable O3 builds. They do
not qualify a newer source tree. GC collection remains absent in these builds;
the GC release gate is still **FAIL**. Thread timings cannot remove that blocker.

## Builds, environment and clocks

| Product | Source ID | CLI SHA-256 |
| --- | --- | --- |
| ordinary | `sha256:c7f97991ba559981a71543aa4c692ec3b9ccd1cbd68057ce14469f7723062841` | `098f64a48cfe9261fef16bb42a4d593b2a390538eaf2505c1d678c6a213724fb` |
| ROS | `sha256:43a37bcffcae8dc85dba5f7e9896b0046da00bce8cdb1ea35a625f04a83a1c1a` | `842935987d5654f0a6d72f1f3e27fa0530fa92a1b4de45eccabaafd021d03b6a` |

Every native harness links the matching `runtime.o` and records its SHA, the
exact O3 build manifest, compile arguments, fixture/runner SHA, generated Wasm
SHA and `wasm-tools` parse/validation result. Compilation and the controller run
on E-core 16; the timed guest threads use P-core 0, or P-cores 0/2/4/6. Waiter and
notifier use P-cores 2 and 0 respectively. All work runs in the same cgroup:
`memory.max=68719476736`, `memory.swap.max=0`,
`cpuset.cpus.effective=0,2,4,6,16-31`. OOM/OOM-kill and CPU-throttling counters
remain zero. No competing compiler, QEMU or VM ran during the paired timings.

The governor is `powersave`. Per-round telemetry includes cpufreq, thermal
zones and cgroup counters. Package temperatures reached approximately 68–100°C
over this batch. Frequency reads outside the measured interval often see idle
frequency; they do not establish active-cycle frequency. Results therefore
describe this host and its thermal drift, rather than a stable industry ranking.

The native env4 attempt is **excluded**: a buffered JSON line interleaved with a
success marker on another output stream. Raw output and the strict parser
failure are retained. Env5 changes only the test success marker to use the same
stdout stream as its JSON; the timed kernel and product source are unchanged.

## Real platform-thread creation and VM host admission

The native fixture creates fresh `std::thread` workers, runs a self-checking
integer kernel, and joins them. Its full path enters the product's
`full_compile_and_run_main_module` host entry; a raw host entry and a native C++
kernel are independent controls. This measures host creation/join plus runtime
admission and guest execution. It is not a guest thread-spawn instruction or a
standalone measurement of a VM spawn API. The [official threads proposal](https://github.com/WebAssembly/threads/blob/main/proposals/threads/Overview.md)
assigns creation and joining to the embedder; Wasm provides shared memory and
atomic operations.

Instruction and unwind policies each run nine samples with 32 rounds per
sample. Each round creates one or four new threads. Same-thread and new-thread
paths alternate order and verify the same checksum. A separate, untimed
qualification binary intercepts real native thread creation to check counts.
The compiler-generated native kernel is retained for assembly review.

| Product / trace | 1 P-core, 1 worker: direct → new thread, µs/round | Paired median added wall time, µs | 4 P-cores, 4 workers: new threads, µs/round | End-to-end kernel updates/s, billion |
| --- | ---: | ---: | ---: | ---: |
| ROS instruction | 10.931 → 22.905 | 11.921 | 66.155 | 1.981 |
| ROS unwind | 11.594 → 22.762 | 11.138 | 69.109 | 1.897 |
| ordinary instruction | 11.757 → 23.432 | 11.548 | 72.766 | 1.801 |
| ordinary unwind | 11.795 → 23.258 | 11.463 | 86.666 | 1.512 |

Four-worker wall deltas include parallel speedup, scheduling and admission, so
they are not pure spawn latency. Kernel updates/s includes creation and joining.
The start-latency values in the raw data are averages over each sample's
rounds/workers. A p95 formed from nine such averages is only an estimate;
**individual-start p99 is not qualified**. Short intervals and high temperatures
also prevent interpreting small differences as a production regression threshold.

## Guest wait/notify rendezvous and parked-wait qualification

The two-thread Wasm module uses real `memory.atomic.wait32` and
`memory.atomic.notify`; every successful notify returns one enrolled waiter and
the waiter must return the notified result. All products/policies use the same
Wasm SHA:
`fa9dfad6a54cfd32aef4d78a1187ba7cf9040f2d46e334b5b9f9661a98e60542`.

Each policy/product has nine alternating-order process pairs, nine timed
samples per process, and 128 wakes per sample: **10,368 measured wakes**. Two
warmup samples precede each process's measured samples. Threads are created
before the per-wake clock starts. The interval includes VM host entry, notify,
wakeup and host completion observation.

| Product / trace | Rendezvous p50 / p95 / p99, µs | Park-qualified p50 / p95 / p99, µs | Park qualification p50, µs |
| --- | ---: | ---: | ---: |
| ROS instruction | 2.925 / 3.389 / 3.710 | 3.017 / 3.342 / 3.652 | 6.188 |
| ROS unwind | 2.987 / 3.507 / 3.748 | 2.964 / 3.182 / 3.538 | 6.243 |
| ordinary instruction | 3.074 / 3.612 / 3.968 | 3.098 / 3.281 / 3.733 | 6.320 |
| ordinary unwind | 3.161 / 3.613 / 3.849 | 3.079 / 3.718 / 4.144 | 6.320 |

Rendezvous verifies enrolment but does not prove that the kernel parked the
waiter. The separate Linux parked test reads the waiter's task state as `S`,
then `wchan=futex_do_wait`, then state `S` again before notifying. These reads
occur **outside** the wake interval and have their own clocks/probe counts. All
10,368 wakes/policy/product meet this qualification; empty-notify retries are
zero, and the median qualification requires one probe.

As the [Linux proc documentation](https://docs.kernel.org/filesystems/proc.html)
describes, these are observations of task state and sleeping location. The
three reads are sequential rather than an atomic kernel snapshot. Neither
test is a kernel-only futex latency benchmark. Pooled p99 values are descriptive
quantiles of serially dependent wake samples from nine processes; they are not
an independent service-tail confidence bound.

## Cross-language platform-thread analogues

This separate experiment uses the same integer operations, worker count and
checksums, rather than identical Wasm bytecode. OpenJDK/Graal use Java platform
threads, .NET uses `Thread`, and each Node Worker creates a new V8 isolate. Each
process performs two warmup samples and one timed sample of 32 rounds; nine
processes per runtime rotate and reverse order. These rows are **not** a
same-Wasm comparison or evidence of a universal VM speed ranking.

| Runtime / policy | 1 P-core, 1 worker, start + kernel + join, µs/round | 4 P-cores, 4 workers, µs/round |
| --- | ---: | ---: |
| Temurin OpenJDK 27+35 / G1 | 51.020 | 210.885 |
| GraalVM CE 25.4.4.1.1, JDK 25.0.4.1.1 / G1 | 183.177 | 253.895 |
| .NET SDK 10.0.401, runtime 10.0.12 / workstation GC | 41.992 | 125.596 |
| Node 26.10.0 / fresh Worker and V8 isolate | 8,872.151 | 9,307.551 |

Java heaps use `-Xms256m -Xmx1g -XX:+UseG1GC`; .NET uses a 1GiB hard heap
limit, workstation GC and tiered PGO; Node uses a 1GiB old-space limit. Timing
excludes process startup and contains each runtime's thread/kernel/join work.
Source, class/DLL, runtime-launcher hashes and exact commands are in metadata.
All 72 measured processes validate every worker checksum and requested thread
count. Requested counts are source-level assertions; only the native VM
qualification independently intercepts the OS creation entry point.

Graal's single-core direct/worker timings vary widely, consistent with ongoing
JIT/warmup effects; two warmup samples do not establish compiler convergence.
Package temperatures reached 71–100°C. Do not interpret that cell as mature
steady-state Graal performance. The `wait4` maximum RSS values are identical
across runtimes within each affinity run (169,268/169,316 KiB); fork/pre-exec
parent high-water RSS can dominate this measurement. They are preserved but
**not qualified as per-runtime working-set estimates**.

## Reproduction and raw evidence

Remote evidence root:
`/work/wasm3-resume-20260924/evidence/followup-c7f9-43a37-20260928/`.

| Evidence directory / summary | SHA-256 |
| --- | --- |
| `vm-thread-ros-env5/measurement-single-summary.json` | `6c0653316e34ac66701f7c23c4a32ca9d3aaf8b45832c8b6c3683989300f491f` |
| `vm-thread-ros-env5/measurement-four-summary.json` | `47f9d949c7db9c86d53509d52ef1fa26c0b16f9622592cdd0e48a0ba52ada327` |
| `vm-thread-ordinary-env5/measurement-single-summary.json` | `58b570b451b645f7912a7e53f501d48e1f5cb0dbf04caefe3fb191c9fc16a7f3` |
| `vm-thread-ordinary-env5/measurement-four-summary.json` | `d0efe583a9c76b4307f8ff221c61a7c06af75ab6e10a5bdeace80eec2b3d0241` |
| `wake-ros-env5/summary.json` | `954f17e7dad6cf031d15c9cb247ed5918b7ae9fdac216c9ae5bbaed6c6685919` |
| `wake-ordinary-env5/summary.json` | `86b31822125bf2dc6af1d29a327430330678a76a44e4004a4934934fa83b183b` |
| `parked-wake-ros-env5/summary.json` | `5adfee6d98074168e71cff8b2c8e3b02bf2c2ca055463b94b975090954671206` |
| `parked-wake-ordinary-env5/summary.json` | `c0d6b66dee3113d7e5033d7fa2533fd56425088e85af0ee47df77d60ada4b3b6` |
| `managed-threads-single-p-env2/summary.json` | `c2047783463683e2d28d609828f1e57f3b5e9fb0dfaaa8ca4229ecac49dbfab8` |
| `managed-threads-four-p-env2/summary.json` | `9df59d09a991d9eb4d034c59acc69c27bfc8d6eb6857de310f7a85004e9c93e2` |

Exact invocations are the saved `*.command`, driver JSON and metadata records.
The fixtures/runners are `test/0017.runtime/run_wasm_thread_performance.py`,
`run_wasm_wait_notify_performance.py`,
`run_wasm_parked_wait_notify_performance.py`, and
`benchmark/0004.wasm3-core/run_managed_threads.py`. Use `--run-only` after their
same-source O3 build/qualification step, pass each product's exact
`--expected-source-id`, and retain `--wasm-tools` validation. The managed runner
requires the matching `build_managed_threads.py` manifest. No product source or
runtime object was modified for this batch.
