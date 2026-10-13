# Debug performance: owned numeric packets with cold expansion (2026-10-11)

This follow-up targets the remaining packet-decoding hotspot after the qualified SSA iteration. All executable work ran on SSH Linux in the actual **64 GiB / no-swap** cgroup with E-core target affinity 16–31. No local builds or executable tests were performed. General instrumentation-free Wasm breakpoints and GDB performance parity remain unimplemented.

## Implementation

An observational activation may retain a small, completely initialized numeric packet in its own bounded 128-byte buffer. Its immutable sealed site determines all local, operand and saved-control-parameter types. The current compiler flags are checked immediately; every sixteen-byte value slot is copied before a call, workspace reuse or native retirement. Only numeric declaration expansion is delayed until a cold frame reader requests it. Frame entry constructs directly in its vector, avoiding an extra buffer copy; cold allocation failure refreshes the reported recording status. Signaling NaNs and v128 values retain exact bits.

The activation identity, shared-plan control block, epoch/generation, native producer extent, initialization flags, call phase and sticky error checks stay in place. A failed packet cannot replace an earlier valid pending packet. Expansion allocates before changing the owned view and fails closed on allocation failure. Caller packets remain complete across nested calls and exact LIFO retirement. Pending numeric packets contain no GC references: root visitation does not expand them or treat their bits as references.

Reference-bearing, larger-than-eight-slot and resumable packets retain synchronous decoding. This changes host ledger storage only; native observation IR, input packet ABI, and cache policy **27 / 17** remain unchanged. Repository-specific ROS bridge names are preserved. Other platforms were not compiled or executed in this qualification.

## Paired kernel measurements

Before is the immutable, previously qualified SSA runtime; after is the packed-packet runtime. Both use the same native Less/local-SSA policy and host O3 build. Each of ten unchanged checksummed Wasm workloads uses five alternating before/after repetitions for ordinary, no-hit, cold and disabled breakpoint cases. Guest CLOCK_MONOTONIC kernel time excludes JIT startup and breakpoint installation. Native C O2/GDB uses an independent equivalent algorithm and its own ordinary baseline.

| Kernel | uwvm2 before → after (ms) | Reduction | ROS before → after (ms) | Reduction |
|---|---:|---:|---:|---:|
| integer | 2.3588 → 2.3551 | 0.15% | 2.3584 → 2.3561 | 0.10% |
| branches | 5.0367 → 5.0595 | -0.45% | 5.0630 → 5.0462 | 0.33% |
| dispatch | 4.3601 → 4.3546 | 0.12% | 4.3893 → 4.3553 | 0.77% |
| scan32k | 24.0991 → 24.1043 | -0.02% | 24.1001 → 24.0995 | 0.00% |
| scan256k | 24.0956 → 24.0968 | -0.00% | 24.1010 → 24.0982 | 0.01% |
| scan8m | 24.1025 → 24.1108 | -0.03% | 24.1161 → 24.1053 | 0.04% |
| chase8m | 29.9993 → 30.0089 | -0.03% | 31.8010 → 31.9700 | -0.53% |
| calls | 84.6505 → 75.3765 | 10.96% | 83.5791 → 74.2496 | 11.16% |
| callchain | 186.8908 → 163.8882 | 12.31% | 183.7406 → 161.8724 | 11.90% |
| float | 2.2832 → 2.2830 | 0.01% | 2.2870 → 2.2812 | 0.25% |

Dependent pointer-chase results include memory-latency variance. The receipt retains every sample and cold/disabled result; zero-hit assertions are checked in real PTY sessions.

### Complete trace and FFT

| Product | Calls complete trace before → after (ms) | Call chain complete trace before → after (ms) | FFT debug before → after (ms) | FFT after debug / ordinary |
|---|---:|---:|---:|---:|
| uwvm2 | 621.606 → 562.872 | 744.783 → 654.274 | 0.987207 → 0.987010 | 13.38× |
| uwvm2-ros | 616.889 → 556.245 | 725.090 → 645.148 | 0.986298 → 0.987563 | 13.32× |

Full-sized trace timing uses two paired repetitions per product, retains a contiguous 512-event window, and checks guest checksums. Exact opcode completeness is independently checked by the seven-opcode interaction fixture. FFT uses the same checked 2048-point Wasm input, five repetitions per mode; its runner groups modes rather than alternating them.

### Current GDB reference and breakpoint costs

| Kernel | uwvm2 debug / own ordinary | GDB / native ordinary |
|---|---:|---:|
| integer | 5.12× | 1.000× |
| branches | 10.98× | 0.992× |
| dispatch | 2.54× | 1.001× |
| scan32k | 29.59× | 1.005× |
| scan256k | 29.54× | 1.000× |
| scan8m | 29.16× | 0.996× |
| chase8m | 1.01× | 1.087× |
| calls | 171.11× | 1.000× |
| callchain | 172.94× | 1.008× |
| float | 5.15× | 0.999× |

| Breakpoint case | uwvm2 (ms) | ROS (ms) | GDB native (ms) |
|---|---:|---:|---:|
| none | 2.326039 | 2.257868 | 0.007393 |
| cold256 | 2.330490 | 2.283491 | 0.007400 |
| disabled256 | 2.333301 | 2.248592 | 0.007392 |
| ignore9999 | 4.529631 | 3.541408 | 946.778992 |
| hits32 | 6.734630 | 6.272610 | 10.408706 |

These are fresh two-repeat real PTY measurements, including a new native/GDB reference. Visible-hit timing includes command/continue transport; ignored-hit timing includes genuine repeated breakpoint encounters. Ordinary code generation differs between native C and Wasm. Faster ignored-hit results do not imply GDB parity or an instrumentation-free Wasm engine.

## Hardware qualification

VTune 2026.4 CLI hardware Hotspots and Microarchitecture Exploration collected fourteen completed before/after runs on the same checked inputs. Collection begins at the real pre-clock Wasm stop and stops at the actual guest-exit prompt, before explicit quit/LLVM teardown. Therefore hardware totals include guest output/exit and collection-control latency, rather than pure kernel-only PMU counts. Targets run on E cores 16–31; the sampling coordinator temporarily allows CPUs 0–31 and restores the original cpuset. Empty P-core summary categories are not evidence about these E-core targets.

| Input / analysis | Instructions retired before → after | Reduction |
|---|---:|---:|
| calls/hotspots | 1,317,600,000 → 1,106,400,000 | 16.03% |
| calls/uarch-exploration | 1,316,607,505 → 1,105,605,137 | 16.03% |
| branches/uarch-exploration | 68,187,047 → 68,189,859 | -0.00% |
| scan32k/uarch-exploration | 488,848,124 → 488,769,738 | 0.02% |
| chase8m/uarch-exploration | 325,053,746 → 324,999,414 | 0.02% |
| fft120/hotspots | 2,356,800,000 → 2,354,400,000 | 0.10% |
| fft120/uarch-exploration | 2,356,582,748 → 2,353,840,959 | 0.12% |

Named after-version call hotspots (sampling estimates; anonymous JIT samples cannot be assigned to a particular opcode):

| Function | Sampled CPU seconds | Estimated retired instructions |
|---|---:|---:|
| uwvm2_native_step_safepoint_impl | 0.009921 | 52800000 |
| __memcmp_avx2_movbe | 0.006944 | 36000000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_borrow_actual_frame_plan | 0.006944 | 132000000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize_compiler_native_data | 0.005952 | 84000000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_root_generation_publication_bridge::matches_row | 0.004960 | 40800000 |
| uwvm2::runtime::checkpoint::shadow_ledger::enter | 0.003968 | 43200000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize | 0.003968 | 33600000 |

The remaining leading named hotspots are native safepoint processing, authenticating the current borrowed frame plan, packet materialization, and publication-row checks. The prior standalone decoder is no longer a leading named row in this sample. These are sampling observations, not a proof that reference/large-packet decoding disappeared.

## Correctness and retained evidence

Both release builds passed `xmake -r -j3`. Each product passed thirteen real PTY regression groups covering shutdown, probes, numeric memory, typed values, three-level markers/registers/native stepping, cancellation, fast breakpoints, symbols, restart, code generation and numeric SSA. Six native component tests per product include 32,000 eager/deferred byte/type/flag comparisons, packed caller/ownership/overwrite/rejection checks, cache identity, legacy copier IR layout verification and native MCJIT numeric SSA. Both final native packed-packet tests include explicit f32/f64 signaling-NaN and zero/exact-eight-slot edge cases.

Both products passed 34 caught actual Wasm traps and eight memory64 boundary traps; all asserted exact pre-trap inputs and rejected terminal execution commands. Ten additional unobserved traps exited with the expected SIGILL, with core dumps disabled. Memory event counters stayed zero; no execution or profiling workers remain. An initial ROS header synchronization compile failure is retained and excluded; the repository-specific bridge names were restored against prior qualified source hashes before the successful rebuild.

The [machine receipt](debug_performance_packed_20261011.json) binds all 76 source hashes and final binary hashes to completed guarded runs. Receipt SHA-256: `14c16f9d243d95412e6b4c3c553a4bf7016342963d606734fc0460c60f18996f`. The [synchronization audit](debug_performance_packed_20261011_sync.json) verifies final sources and reports across both local and remote checkouts. Neither platform/UI parity nor general instrumentation-free Wasm breakpoints is established by these Linux results.

## Architecture research and remaining work

[LLVM stack maps](https://llvm.org/docs/StackMaps.html) describe native register, stack and constant locations of selected live values. A zero-shadow stack map need not emit inline instructions; patchpoints reserve replacement space and require whole-region replacement. Inference: using those records to replace repeated snapshots can remove work, but preserving all values as live can still affect register pressure and spills.

[GDB fast tracepoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Tracepoints.html) may install a jump instead of a trap, with target/location restrictions; their feature set differs from normal breakpoints. [Linux uprobes](https://www.kernel.org/doc/html/latest/trace/uprobetracer.html) describe executable/library file offsets. They do not by themselves provide the complete anonymous-JIT Wasm typed-state reconstruction required here.

General inactive Wasm breakpoints still need canonical loaded-site ownership, accurate live locals/operands/control/caller/reference maps, parked-worker patching with W^X and instruction-cache rules, and correct hit/restore/rearm/pause/shutdown behavior. The existing native instruction breakpoint backend is not a replacement for that Wasm state engine. This iteration reduces repeated numeric expansion; mandatory activation/entry/exit hooks and per-opcode demand gates remain measurable work.

## Reproduction

Ignored orchestration and raw evidence remain in `/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/packed-v30-*`; source tests are tracked under `test/0017.runtime`. The 64-GiB guard is `guard-ssa-v25.py`. Use the final source manifest, successful build guards, frozen binaries and explicit `--baseline-codegen less --baseline-pipeline debug-local-ssa` when reproducing the paired workload matrix. Preserve the completed prior SSA receipt and immutable baseline binaries.
