# Debug performance: Retired frame storage (2026-10-11)

This follow-up reuses constructed logical-frame storage at actual activation depth. The immediate baseline is the qualified local typed-call demand runtime (calls-v36). All builds, executable tests, real PTYs, timings and VTune ran on SSH Linux in the actual 64 GiB/no-swap cgroup; no local executable testing was performed.

## Implementation and retained guarantees

The shadow ledger retains physical slots up to the deepest actual activation and tracks a separate active count. Entry grows storage only when it exceeds that prior depth. Return clears the exact plan/controlblock, identity, site, availability, pending state and value cells before excluding the retired slot. Empty numeric byte storage can be reused; its previous contents are never treated as initialized values. Large values or a full capacity cache release their allocation immediately. Zero-capacity value vectors no longer occupy the bounded empty-value cache. The existing 32 × 16 KiB cache bound is retained.

Quota, parent/epoch/continuation admission, exact current-plan lookup, completeness checks, stopped views, expansion and reference visitation all use only the active range. A new activation starts unmaterialized with no values even when the slot was previously used by a different function/type. All requested before-opcode, awaiting-caller and returned-caller packets remain fully copied and validated. References retain synchronous validation and retirement. Sticky failure, terminal discard, tail handling, native ownership/TF admission and code replacement rules are unchanged. No observer, safepoint or JIT instrumentation was removed. Observation codegen policy remains 28 and resumable remains 17; this change affects host storage, not generated IR/cache identity.

An earlier inlined-root-check trial changed calls and callchain timings by about 0.7%, below an actionable benefit. Its two root headers were restored before both final builds. That experiment and its initial executable-mode copy failure are retained separately and do not qualify the final candidate.

## Paired kernel measurements

Ten unchanged checksummed inputs cover integer, float, branches, dispatch, cache scan, dependent pointer chase, calls and call chains. Main kernels use three alternating before/after repetitions in ordinary, no-hit, cold and disabled-breakpoint modes. Kernel time excludes JIT initialization and breakpoint installation. Native C/GDB uses an equivalent algorithm and its own ordinary baseline. The preliminary primary call-only five-repeat trial is recorded separately. Percentages below are median reductions; negative values are regressions, and small changes are not claims of statistical significance.

| Kernel | uwvm2 before → after (ms) | Reduction | ROS before → after (ms) | Reduction |
|---|---:|---:|---:|---:|
| integer | 2.35571 → 2.35396 | 0.07% | 2.35648 → 2.35361 | 0.12% |
| branches | 5.05214 → 5.07967 | -0.54% | 5.05517 → 5.06488 | -0.19% |
| dispatch | 4.38770 → 4.38944 | -0.04% | 4.40120 → 4.38482 | 0.37% |
| scan32k | 24.09956 → 24.09573 | 0.02% | 24.09558 → 24.10432 | -0.04% |
| scan256k | 24.10019 → 24.09523 | 0.02% | 24.09350 → 24.09680 | -0.01% |
| scan8m | 24.10728 → 24.10356 | 0.02% | 24.10383 → 24.11681 | -0.05% |
| chase8m | 31.80177 → 31.67602 | 0.40% | 30.03317 → 29.60590 | 1.42% |
| calls | 49.07605 → 48.03828 | 2.11% | 48.66285 → 48.11195 | 1.13% |
| callchain | 105.43989 → 102.95136 | 2.36% | 105.11537 → 103.08970 | 1.93% |
| float | 2.29538 → 2.27967 | 0.68% | 2.28011 → 2.27996 | 0.01% |

Four kernels also receive ten serial alternating repetitions pinned to CPU 16, retaining every result:

| Product / kernel | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / calls | 49.06433 → 47.99655 | 2.18% |
| uwvm2 / callchain | 105.85586 → 103.17479 | 2.53% |
| uwvm2 / dispatch | 4.40524 → 4.39640 | 0.20% |
| uwvm2 / chase8m | 30.22248 → 30.21940 | 0.01% |
| uwvm2-ros / calls | 48.74359 → 48.11299 | 1.29% |
| uwvm2-ros / callchain | 105.70504 → 103.42716 | 2.15% |
| uwvm2-ros / dispatch | 4.39817 → 4.39811 | 0.00% |
| uwvm2-ros / chase8m | 30.21477 → 30.19462 | 0.07% |

### Complete trace and FFT

| Product | Calls complete trace before → after (ms) | Call chain complete trace before → after (ms) | FFT debug before → after (ms) | FFT after debug / ordinary |
|---|---:|---:|---:|---:|
| uwvm2 | 537.278 → 540.882 | 617.799 → 620.659 | 0.977874 → 0.977272 | 13.25× |
| uwvm2-ros | 530.566 → 535.249 | 611.499 → 615.814 | 0.979080 → 0.977689 | 13.17× |

Full trace uses two paired full-sized repetitions with a contiguous 512-event retained window. Independent opcode completeness and exact caller data are checked by real PTY fixtures. FFT uses the unchanged checked 2048-point Wasm input with five repetitions per mode; its runner groups modes rather than alternating them.

### GDB and breakpoint reference

| Kernel | uwvm2 debug / own ordinary | GDB / native ordinary |
|---|---:|---:|
| integer | 5.16× | 0.991× |
| branches | 10.94× | 1.012× |
| dispatch | 2.57× | 1.001× |
| scan32k | 29.61× | 1.000× |
| scan256k | 29.53× | 1.006× |
| scan8m | 29.24× | 0.979× |
| chase8m | 1.01× | 1.041× |
| calls | 109.04× | 1.000× |
| callchain | 124.42× | 0.986× |
| float | 5.14× | 0.996× |

| Breakpoint case | uwvm2 (ms) | ROS (ms) | GDB native (ms) |
|---|---:|---:|---:|
| none | 1.451049 | 1.458942 | 0.007397 |
| cold256 | 1.446530 | 1.450905 | 0.007392 |
| disabled256 | 1.450155 | 1.445634 | 0.007393 |
| ignore9999 | 3.199187 | 3.579796 | 928.627144 |
| hits32 | 5.335606 | 5.311970 | 10.997724 |

These are fresh two-repeat real PTYs; visible stops include command/continue transport. Ignored hits still encounter real breakpoints. Native C and Wasm code generation differ; ignored-hit results do not imply GDB no-hit execution parity.

## Hardware evidence

Sixteen before/after VTune CLI hardware Hotspots and Microarchitecture Exploration collections use unchanged inputs. Sampling starts at a genuine pre-clock Wasm stop and ends at guest exit before debugger quit/LLVM teardown. Totals include output/exit and collection-control latency and are not pure kernel counts. Actual targets use E cores 16–31; sampler cpuset temporarily becomes 0–31 and restores 16–31.

| Input / analysis | Instructions retired before → after | Reduction |
|---|---:|---:|
| calls/hotspots | 796,800,000 → 796,800,000 | 0.00% |
| calls/uarch-exploration | 796,931,257 → 796,634,273 | 0.04% |
| branches/uarch-exploration | 68,162,284 → 68,209,660 | -0.07% |
| dispatch/uarch-exploration | 57,220,336 → 57,221,630 | -0.00% |
| scan32k/uarch-exploration | 488,818,309 → 488,822,052 | -0.00% |
| chase8m/uarch-exploration | 324,984,586 → 324,995,679 | -0.00% |
| fft120/hotspots | 2,332,800,000 → 2,332,800,000 | 0.00% |
| fft120/uarch-exploration | 2,332,091,722 → 2,332,506,538 | -0.02% |

| Input / uarch | Clockticks before → after | Reduction |
|---|---:|---:|
| calls | 203,157,541 → 199,481,174 | 1.81% |
| branches | 23,587,249 → 23,945,332 | -1.52% |
| dispatch | 20,997,537 → 20,854,886 | 0.68% |
| scan32k | 101,771,073 → 101,749,826 | 0.02% |
| chase8m | 177,786,867 → 175,660,642 | 1.20% |
| fft120 | 488,707,782 → 489,840,771 | -0.23% |

Remaining named call hotspots are sampling estimates; anonymous JIT samples cannot be attributed to an opcode:

| Function | Sampled CPU seconds | Estimated retired instructions |
|---|---:|---:|
| uwvm2::runtime::lib::checkpoint_current_generation_plan | 0.005952 | 38400000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_abi_bridge | 0.005952 | 45600000 |
| uwvm2::runtime::checkpoint::shadow_ledger::leave | 0.004960 | 12000000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_activation_leave_abi_bridge | 0.004960 | 26400000 |
| std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const>::~shared_ptr[abi:nqe230101] | 0.003968 | 2400000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet(uwvm2::runtime::checkpoint::activation_identity, std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const> const&, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long)::$_0::operator() | 0.003968 | 19200000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize_compiler_native_data | 0.002976 | 55200000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_actual_frame | 0.002976 | 50400000 |

## Correctness and synchronization

Both release builds passed xmake -r -j3. Each product passed fifteen real PTY groups covering shutdown, probes, memory, typed values, native markers/registers/stepping, cancellation, fast breakpoints, symbols, restart, code generation, numeric SSA, source binding and local calls. Six native component tests per product passed cache identity, packet shape, shadow ledger, packed numeric packet and LLVM IR/actual MCJIT checks with active assertions. Existing tests retain protected-page extents, invalid flags, numeric exact bits/signaling NaNs/v128, anti-alias controlblocks, references, callers and fallback paths.

New component checks release an ephemeral plan and prove its weak controlblock expires at retirement. Empty views and reference census exclude retired storage. 128 repeated parent/child activations reuse depths with different i32/f64 declarations, validate empty/unmaterialized entry, exact active counts, retired-child rejection and one remaining live parent reference. Existing reuse checks retain fresh values/flags and unchanged prior frame data after packet rejection. Deep observation, tail continuation, sticky failure and discard remain tested.

Both products passed 34 caught real Wasm traps plus eight memory64 boundary traps with exact before-trap values and terminal execution rejected. Ten unobserved traps exited with expected SIGILL and core dumps disabled. All final memory event counters remain zero, sampler cpuset is restored and no execution/profiling workers remain.

The [machine receipt](debug_performance_frame_storage_20261011.json) binds all 88 source hashes to completed guarded runs and immutable binary hashes. Receipt SHA-256: `48597b1fe2b400b7889959cea90662674a0a04ede2c1067367e474b3fd385549`. The [sync audit](debug_performance_frame_storage_20261011_sync.json) checks both local products and corresponding SSH copies. Only the current Linux release configuration was executed; macOS/Windows, exception-disabled configurations and other-platform UI behavior were not requalified.

## Architecture research and remaining limits

[GDB software breakpoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Breaks.html) patch selected native instructions with traps; [displaced stepping](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Maintenance-Commands.html) executes an out-of-line original instruction while retaining the breakpoint. [LLVM StackMaps](https://llvm.org/docs/StackMaps.html) associate requested live register/spill/constant values with code offsets. Inference: complete Wasm typed locals/operand/control/caller/reference recovery and authenticated loaded-code ownership are still necessary at such stops.

[Linux static keys](https://docs.kernel.org/staging/static-keys.html) reduce disabled branch checks through patching; [LLVM dynamic debugging](https://llvm.org/docs/DynamicDebugging.html) describes optimized/debugging body detours. Inference: these approaches require all-worker patch coordination, loaded-code lifetime/W^X/cache synchronization and precise active-frame transitions here. No jump-label patching, dual bodies or general instrumentation-free Wasm breakpoint recovery was implemented in this iteration. GDB no-hit execution efficiency remains unmet. This iteration reduces repeated host frame construction while retaining every requested state capture.

## Reproduction

Raw evidence is retained under /home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/frames-v38-*. Use guard-ssa-v25.py and immutable source/binary manifests. Baselines are uwvm-calls-v36(-ros), candidates uwvm-frames-v38(-ros); both use native Less/local-SSA and host O3. The discarded root-inline-v37 experiment has separate guards and samples. Preserve prior receipts and immutable binaries.
