# Debug performance: short flag checks and actual-frame validation (2026-10-11)

This follow-up targets short initialization-flag comparisons and actual-frame validation in the previously qualified packed-packet runtime. All executable work ran on SSH Linux in the actual **64 GiB / no-swap** cgroup with E-core target affinity 16–31. No local builds or executable tests were performed. General instrumentation-free Wasm breakpoints and GDB performance parity remain unimplemented.

## Implementation

A sealed all-initialized site checks short flag ranges with exact 8/4/2/1-byte loads instead of calling the general libc comparator. Loads use memcpy for unaligned input and never extend beyond the already validated flag count. Every byte must still equal 1; larger ranges retain the complete comparator. Repeated 0x01 bytes make the short checks independent of native endianness.

The actual native leaf-plan validation is inlined, retaining every activation/count/epoch/module/function/generation check. The dynamic packet commit recognizes a reference to its exact frame-owned shared_ptr object; another shared_ptr still undergoes pointee and control-block checks. A matching raw address with a foreign control block is rejected. All complete 16-byte values, initialized states, operand/control state, caller packets, reference checks and sticky failures remain intact.

Reference-bearing, larger-than-eight-slot and resumable packets retain synchronous decoding; sealed all-required flag runs may use the same exact short checks. This changes host validation code only; native observation IR, input packet ABI, and cache policy **27 / 17** remain unchanged. Repository-specific ROS bridge names are preserved. Other platforms were not compiled or executed in this qualification.

## Paired kernel measurements

Before is the immutable, previously qualified packed-packet runtime; after is the short-flag runtime. Both use the same native Less/local-SSA policy and host O3 build. Each of ten unchanged checksummed Wasm workloads uses five alternating before/after repetitions for ordinary, no-hit, cold and disabled breakpoint cases. Guest CLOCK_MONOTONIC kernel time excludes JIT startup and breakpoint installation. Native C O2/GDB uses an independent equivalent algorithm and its own ordinary baseline.

| Kernel | uwvm2 before → after (ms) | Reduction | ROS before → after (ms) | Reduction |
|---|---:|---:|---:|---:|
| integer | 2.3563 → 2.3552 | 0.05% | 2.3567 → 2.3542 | 0.11% |
| branches | 5.0449 → 5.0704 | -0.51% | 5.0637 → 5.0668 | -0.06% |
| dispatch | 4.3596 → 4.4033 | -1.00% | 4.3716 → 4.3660 | 0.13% |
| scan32k | 24.0950 → 24.0983 | -0.01% | 24.0990 → 24.1050 | -0.03% |
| scan256k | 24.0996 → 24.1057 | -0.03% | 24.0980 → 24.0953 | 0.01% |
| scan8m | 24.1063 → 24.1181 | -0.05% | 24.1128 → 24.1208 | -0.03% |
| chase8m | 29.9077 → 31.7129 | -6.04% | 31.7725 → 31.8315 | -0.19% |
| calls | 75.3513 → 72.7560 | 3.44% | 74.5025 → 72.4579 | 2.74% |
| callchain | 163.7907 → 157.0256 | 4.13% | 161.2039 → 156.0738 | 3.18% |
| float | 2.2827 → 2.2816 | 0.05% | 2.2823 → 2.2815 | 0.04% |

The initial pointer-chase timing differed between versions. An additional serial check pins both products and both versions to CPU 16, with ten alternating repeats on dispatch and chase8m. These retain the same checksummed Wasm fixtures and exact binary hashes:

| Kernel / product | Fixed-CPU before → after (ms) | Reduction |
|---|---:|---:|
| dispatch / uwvm2 | 4.3709 → 4.4185 | -1.09% |
| chase8m / uwvm2 | 30.1871 → 30.2155 | -0.09% |
| dispatch / uwvm2-ros | 4.3711 → 4.3689 | 0.05% |
| chase8m / uwvm2-ros | 30.2394 → 30.2106 | 0.10% |

The fixed-CPU pointer-chase medians differ by about 0.1%, so the initial uwvm2 pointer-chase slowdown did not reproduce in that check. The uwvm2 dispatch result remains about 1.1% slower on fixed CPU 16; ROS dispatch is approximately unchanged. The cause of this small, product-specific dispatch difference is not established, and it is not counted as an improvement.

Dependent pointer-chase results include memory-latency variance. The receipt retains every sample and cold/disabled result; zero-hit assertions are checked in real PTY sessions.

### Complete trace and FFT

| Product | Calls complete trace before → after (ms) | Call chain complete trace before → after (ms) | FFT debug before → after (ms) | FFT after debug / ordinary |
|---|---:|---:|---:|---:|
| uwvm2 | 566.264 → 555.852 | 653.036 → 647.560 | 0.991851 → 0.988159 | 13.38× |
| uwvm2-ros | 557.687 → 548.063 | 646.688 → 640.548 | 0.987185 → 0.990158 | 13.29× |

Full-sized trace timing uses two paired repetitions per product, retains a contiguous 512-event window, and checks guest checksums. Exact opcode completeness is independently checked by the seven-opcode interaction fixture. FFT uses the same checked 2048-point Wasm input, five repetitions per mode; its runner groups modes rather than alternating them.

### Current GDB reference and breakpoint costs

| Kernel | uwvm2 debug / own ordinary | GDB / native ordinary |
|---|---:|---:|
| integer | 5.12× | 0.999× |
| branches | 10.90× | 1.000× |
| dispatch | 2.58× | 1.002× |
| scan32k | 29.75× | 0.997× |
| scan256k | 29.76× | 1.001× |
| scan8m | 29.21× | 0.998× |
| chase8m | 1.01× | 1.101× |
| calls | 165.17× | 1.000× |
| callchain | 167.47× | 1.011× |
| float | 5.10× | 0.992× |

| Breakpoint case | uwvm2 (ms) | ROS (ms) | GDB native (ms) |
|---|---:|---:|---:|
| none | 2.250346 | 2.232226 | 0.007408 |
| cold256 | 2.250851 | 2.245355 | 0.007393 |
| disabled256 | 2.255492 | 2.230195 | 0.007392 |
| ignore9999 | 4.324785 | 3.998412 | 947.370520 |
| hits32 | 6.643838 | 5.832542 | 10.967309 |

These are fresh two-repeat real PTY measurements, including a new native/GDB reference. Visible-hit timing includes command/continue transport; ignored-hit timing includes genuine repeated breakpoint encounters. Ordinary code generation differs between native C and Wasm. Faster ignored-hit results do not imply GDB parity or an instrumentation-free Wasm engine.

## Hardware qualification

VTune 2026.4 CLI hardware Hotspots and Microarchitecture Exploration collected fourteen completed before/after runs on the same checked inputs. Collection begins at the real pre-clock Wasm stop and stops at the actual guest-exit prompt, before explicit quit/LLVM teardown. Therefore hardware totals include guest output/exit and collection-control latency, rather than pure kernel-only PMU counts. Targets run on E cores 16–31; the sampling coordinator temporarily allows CPUs 0–31 and restores the original cpuset. Empty P-core summary categories are not evidence about these E-core targets.

| Input / analysis | Instructions retired before → after | Reduction |
|---|---:|---:|
| calls/hotspots | 1,104,000,000 → 1,068,000,000 | 3.26% |
| calls/uarch-exploration | 1,105,501,943 → 1,068,332,215 | 3.36% |
| branches/uarch-exploration | 68,124,300 → 68,239,253 | -0.17% |
| scan32k/uarch-exploration | 488,799,543 → 488,801,469 | -0.00% |
| chase8m/uarch-exploration | 324,960,296 → 324,976,148 | -0.00% |
| fft120/hotspots | 2,354,400,000 → 2,354,400,000 | 0.00% |
| fft120/uarch-exploration | 2,353,733,667 → 2,353,070,577 | 0.03% |

Named after-version call hotspots (sampling estimates; anonymous JIT samples cannot be assigned to a particular opcode):

| Function | Sampled CPU seconds | Estimated retired instructions |
|---|---:|---:|
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet | 0.008929 | 67200000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge | 0.008929 | 60000000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize_compiler_native_data | 0.007937 | 76800000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_actual_frame | 0.006944 | 74400000 |
| uwvm2::runtime::lib::checkpoint_current_generation_plan | 0.004960 | 38400000 |
| uwvm2_native_step_safepoint_impl | 0.004960 | 84000000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_root_generation_publication_bridge::matches_current_root | 0.003968 | 48000000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_activation_leave_abi_bridge | 0.003968 | 24000000 |

## Correctness and retained evidence

Both release builds passed `xmake -r -j3`. Each product passed thirteen real PTY regression groups covering shutdown, probes, numeric memory, typed values, three-level markers/registers/native stepping, cancellation, fast breakpoints, symbols, restart, code generation and numeric SSA. Six native component tests per product include 32,000 eager/deferred byte/type/flag comparisons, packed caller/ownership/overwrite/rejection checks, cache identity, legacy copier IR layout verification and native MCJIT numeric SSA. Both native packed-packet tests additionally cover 22,950 invalid-byte cases across lengths 1-9 in eager/deferred paths, exact frame-owner borrows and foreign-control-block rejection, protected-page boundaries for lengths 0-9, plus f32/f64 signaling-NaN and zero/exact-eight-slot edges.

Both products passed 34 caught actual Wasm traps and eight memory64 boundary traps; all asserted exact pre-trap inputs and rejected terminal execution commands. Ten additional unobserved traps exited with the expected SIGILL, with core dumps disabled. Memory event counters stayed zero; no execution or profiling workers remain.

The [machine receipt](debug_performance_short_flags_20261011.json) binds all 76 source hashes and final binary hashes to completed guarded runs. Receipt SHA-256: `c791ff0d2fb44bbc0f792e8945c60ef3a65957cdc8267d3fe21fa66ab40646cb`. The [synchronization audit](debug_performance_short_flags_20261011_sync.json) verifies final sources and reports across both local and remote checkouts. Neither platform/UI parity nor general instrumentation-free Wasm breakpoints is established by these Linux results.

## Architecture research and remaining work

[LLVM stack maps](https://llvm.org/docs/StackMaps.html) describe native register, stack and constant locations of selected live values. A zero-shadow stack map need not emit inline instructions; patchpoints reserve replacement space and require whole-region replacement. Inference: using those records to replace repeated snapshots can remove work, but preserving all values as live can still affect register pressure and spills.

[GDB fast tracepoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Tracepoints.html) may install a jump instead of a trap, with target/location restrictions; their feature set differs from normal breakpoints. [Linux uprobes](https://www.kernel.org/doc/html/latest/trace/uprobetracer.html) describe executable/library file offsets. They do not by themselves provide the complete anonymous-JIT Wasm typed-state reconstruction required here.

[GDB software breakpoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Breaks.html) replace a selected program instruction and restore original bytes while stopped. [LLVM dynamic debugging](https://llvm.org/docs/DynamicDebugging.html) describes detouring optimized functions into a separately loaded unoptimized body when a breakpoint is requested. Inference for this JIT: dual bodies are a possible way to remove inactive checks, but switching an already executing Wasm frame requires an authenticated state map and generation/lifetime handling; function-entry detours alone do not meet all-opcode stepping and inspection requirements.

[GDB displaced stepping](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Maintenance-Commands.html) can execute an out-of-line copy of the original instruction without removing the breakpoint. Inference: a concurrent JIT patching design would need equivalent instruction-relocation and ownership rules or an actual all-worker stop; otherwise other threads may cross a temporarily removed trap. Such a mechanism is not implemented by this flag-validation change.

General inactive Wasm breakpoints still need canonical loaded-site ownership, accurate live locals/operands/control/caller/reference maps, parked-worker patching with W^X and instruction-cache rules, and correct hit/restore/rearm/pause/shutdown behavior. The existing native instruction breakpoint backend is not a replacement for that Wasm state engine. This iteration reduces short comparisons and repeated native call overhead; mandatory activation/entry/exit hooks and per-opcode demand gates remain measurable work.

## Reproduction

Ignored orchestration and raw evidence remain in `/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/short-v31-*`; source tests are tracked under `test/0017.runtime`. The 64-GiB guard is `guard-ssa-v25.py`. Use the final source manifest, successful build guards, frozen binaries and explicit `--baseline-codegen less --baseline-pipeline debug-local-ssa` when reproducing the paired workload matrix. Preserve the completed prior packed-packet receipt and immutable baseline binaries.
