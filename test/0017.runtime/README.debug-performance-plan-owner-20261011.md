# Debug performance: Deferred generation plan ownership (2026-10-11)

This follow-up defers generation-plan ownership acquisition until authentication succeeds. The immediate baseline is the qualified selected-source comparison runtime (owner-v39). All builds, executable tests, real PTYs, timings and VTune ran on SSH Linux in the actual 64 GiB/no-swap cgroup; no local executable testing was performed.

## Implementation and retained guarantees

The generation-plan lookup formerly copied the compiled record shared owner before authentication and returned that const local as another owning result. It now borrows the exact record-owned shared_ptr under the actual execution/publication lease, performs every authentication check synchronously, then copies the admitted owner once into the return value. The caller and shadow ledger retain their genuine owning plan. No callback, park, mutation, cached admission or borrowed return is introduced.

Every canonical source, initializer, current epoch, compiled generation, plan/profile controlblock, selected native entry, registry membership, real code owner and replacement admission condition remains intact. No cached affirmative result or caller-provided identity is introduced. All requested before-opcode, awaiting-caller and returned-caller packets retain complete synchronous copying and reference validation. No observer, safepoint or JIT instrumentation was removed. Observation codegen policy remains 28 and resumable remains 17; this changes host authentication cost, not generated IR/cache identity.

The exact primary generation-plan authentication function has two locked increments before the change and one afterward. Its temporary plan destructor call is removed (one call before, zero afterward); the admitted return still acquires an owner. Both listings are bound to qualified binary hashes. This verifies removal of the temporary ownership acquisition/release path, without claiming that JIT instrumentation disappeared.

## Paired kernel measurements

Ten unchanged checksummed inputs cover integer, float, branches, dispatch, cache scan, dependent pointer chase, calls and call chains. Main kernels use three alternating before/after repetitions in ordinary, no-hit, cold and disabled-breakpoint modes. Kernel time excludes JIT initialization and breakpoint installation. Native C/GDB uses an equivalent algorithm and its own ordinary baseline. The preliminary primary call-only five-repeat trial is recorded separately. Percentages below are median reductions; negative values are regressions, and small changes are not claims of statistical significance.

| Kernel | uwvm2 before → after (ms) | Reduction | ROS before → after (ms) | Reduction |
|---|---:|---:|---:|---:|
| integer | 2.35569 → 2.35570 | -0.00% | 2.35583 → 2.35640 | -0.02% |
| branches | 5.06396 → 5.07229 | -0.16% | 5.05777 → 5.06329 | -0.11% |
| dispatch | 4.38255 → 4.38607 | -0.08% | 4.38816 → 4.39032 | -0.05% |
| scan32k | 24.12776 → 24.09838 | 0.12% | 24.09877 → 24.09452 | 0.02% |
| scan256k | 24.09350 → 24.09573 | -0.01% | 24.09811 → 24.09668 | 0.01% |
| scan8m | 24.10529 → 24.10753 | -0.01% | 24.10877 → 24.10787 | 0.00% |
| chase8m | 30.08718 → 31.77359 | -5.61% | 29.93852 → 29.94055 | -0.01% |
| calls | 46.73487 → 45.12574 | 3.44% | 46.71345 → 44.90888 | 3.86% |
| callchain | 100.66693 → 97.10158 | 3.54% | 99.85699 → 95.93878 | 3.92% |
| float | 2.28269 → 2.28288 | -0.01% | 2.28073 → 2.27972 | 0.04% |

Four kernels also receive ten serial alternating repetitions pinned to CPU 16, retaining every result:

| Product / kernel | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / calls | 46.86880 → 45.15372 | 3.66% |
| uwvm2 / callchain | 100.81199 → 97.03698 | 3.74% |
| uwvm2 / dispatch | 4.40341 → 4.39593 | 0.17% |
| uwvm2 / chase8m | 30.17868 → 30.21583 | -0.12% |
| uwvm2-ros / calls | 46.89006 → 45.11541 | 3.78% |
| uwvm2-ros / callchain | 100.05729 → 96.41550 | 3.64% |
| uwvm2-ros / dispatch | 4.40493 → 4.39189 | 0.30% |
| uwvm2-ros / chase8m | 30.16548 → 30.20849 | -0.14% |

### Complete trace and FFT

| Product | Calls complete trace before → after (ms) | Call chain complete trace before → after (ms) | FFT debug before → after (ms) | FFT after debug / ordinary |
|---|---:|---:|---:|---:|
| uwvm2 | 542.177 → 540.208 | 618.442 → 615.879 | 0.978512 → 0.977950 | 13.22× |
| uwvm2-ros | 534.177 → 531.437 | 613.471 → 606.730 | 0.976862 → 0.976972 | 13.17× |

Full trace uses two paired full-sized repetitions with a contiguous 512-event retained window. Independent opcode completeness and exact caller data are checked by real PTY fixtures. FFT uses the unchanged checked 2048-point Wasm input with five repetitions per mode; its runner groups modes rather than alternating them.

### GDB and breakpoint reference

| Kernel | uwvm2 debug / own ordinary | GDB / native ordinary |
|---|---:|---:|
| integer | 5.16× | 0.993× |
| branches | 11.01× | 0.951× |
| dispatch | 2.56× | 0.999× |
| scan32k | 29.52× | 1.002× |
| scan256k | 29.57× | 1.002× |
| scan8m | 29.45× | 0.968× |
| chase8m | 1.01× | 0.978× |
| calls | 102.42× | 1.012× |
| callchain | 103.51× | 0.992× |
| float | 5.14× | 1.001× |

| Breakpoint case | uwvm2 (ms) | ROS (ms) | GDB native (ms) |
|---|---:|---:|---:|
| none | 1.385166 | 1.390729 | 0.007408 |
| cold256 | 1.390442 | 1.372289 | 0.007388 |
| disabled256 | 1.382150 | 1.380631 | 0.007400 |
| ignore9999 | 2.781408 | 2.946388 | 933.546377 |
| hits32 | 5.546423 | 5.224812 | 10.440203 |

These are fresh two-repeat real PTYs; visible stops include command/continue transport. Ignored hits still encounter real breakpoints. Native C and Wasm code generation differ; ignored-hit results do not imply GDB no-hit execution parity.

## Hardware evidence

Sixteen before/after VTune CLI hardware Hotspots and Microarchitecture Exploration collections use unchanged inputs. Sampling starts at a genuine pre-clock Wasm stop and ends at guest exit before debugger quit/LLVM teardown. Totals include output/exit and collection-control latency and are not pure kernel counts. Actual targets use E cores 16–31; sampler cpuset temporarily becomes 0–31 and restores 16–31.

| Input / analysis | Instructions retired before → after | Reduction |
|---|---:|---:|
| calls/hotspots | 792,000,000 → 784,800,000 | 0.91% |
| calls/uarch-exploration | 792,024,902 → 785,794,030 | 0.79% |
| branches/uarch-exploration | 68,098,648 → 68,202,096 | -0.15% |
| dispatch/uarch-exploration | 57,221,862 → 57,216,521 | 0.01% |
| scan32k/uarch-exploration | 488,774,071 → 488,807,661 | -0.01% |
| chase8m/uarch-exploration | 324,980,201 → 325,006,085 | -0.01% |
| fft120/hotspots | 2,332,800,000 → 2,332,800,000 | 0.00% |
| fft120/uarch-exploration | 2,332,335,218 → 2,332,189,346 | 0.01% |

| Input / uarch | Clockticks before → after | Reduction |
|---|---:|---:|
| calls | 273,275,530 → 187,750,351 | 31.30% |
| branches | 23,035,119 → 23,524,661 | -2.13% |
| dispatch | 20,694,828 → 20,754,265 | -0.29% |
| scan32k | 101,351,391 → 101,395,084 | -0.04% |
| chase8m | 177,010,786 → 168,950,917 | 4.55% |
| fft120 | 488,674,883 → 489,673,562 | -0.20% |

Hardware clockticks are one before/after window per analysis and can differ substantially from the ten-pair fixed-CPU kernel timings. The window also includes collection-control/output/exit work; a large clocktick reduction is not a claim of the same kernel wall-time speedup. All positive and negative hardware comparisons are retained.

Remaining named call hotspots are sampling estimates; anonymous JIT samples cannot be attributed to an opcode:

| Function | Sampled CPU seconds | Estimated retired instructions |
|---|---:|---:|
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge | 0.012897 | 57600000 |
| uwvm2::runtime::checkpoint::shadow_ledger::leave | 0.004960 | 16800000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_root_generation_publication_bridge::matches_current_root | 0.004960 | 31200000 |
| __memmove_avx_unaligned_erms | 0.003968 | 12000000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize | 0.003968 | 28800000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_root_generation_publication_bridge::matches_record | 0.003968 | 21600000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet(uwvm2::runtime::checkpoint::activation_identity, std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const> const&, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long)::$_0::operator() | 0.002976 | 16800000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_activation_leave_abi_bridge | 0.002976 | 38400000 |

## Correctness and synchronization

Both release builds passed xmake -r -j3. Each product passed fifteen real PTY groups covering shutdown, probes, memory, typed values, native markers/registers/stepping, cancellation, fast breakpoints, symbols, restart, code generation, numeric SSA, source binding and local calls. Seven native component tests per product passed cache identity, packet shape, shadow ledger, packed numeric packet and LLVM IR/actual MCJIT checks with active assertions. Existing tests retain protected-page extents, invalid flags, numeric exact bits/signaling NaNs/v128, anti-alias controlblocks, references, callers and fallback paths.

The existing selected-owner native component exercises serialized source selection, an empty root, exact owner identity, the same address under a foreign controlblock, a different address under the same controlblock, an unreadable foreign alias, retirement and replacement. The comparison retains no extra owner. Existing frame tests continue to cover owner release, mixed-type depth reuse, exact references, rejected packets and sticky failure. Real multi-module PTYs exercise exact suspended callers, replacement generation 2 and restart; stale and active replacement admission remains rejected.

Both products passed 34 caught real Wasm traps plus eight memory64 boundary traps with exact before-trap values and terminal execution rejected. Ten unobserved traps exited with expected SIGILL and core dumps disabled. All final memory event counters remain zero, sampler cpuset is restored and no execution/profiling workers remain.

The [machine receipt](debug_performance_plan_owner_20261011.json) binds all 92 source hashes to completed guarded runs and immutable binary hashes. Receipt SHA-256: `165d85eb46dc23a3d87cefd35578c7a0f314aa9546d597ad877dd57d3e34c0ce`. The [sync audit](debug_performance_plan_owner_20261011_sync.json) checks both local products and corresponding SSH copies. Only the current Linux release configuration was executed; macOS/Windows, exception-disabled configurations and other-platform UI behavior were not requalified.

## Architecture research and remaining limits

[GDB software breakpoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Breaks.html) patch selected native instructions with traps; [displaced stepping](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Maintenance-Commands.html) executes an out-of-line original instruction while retaining the breakpoint. [LLVM StackMaps](https://llvm.org/docs/StackMaps.html) associate requested live register/spill/constant values with code offsets. Inference: complete Wasm typed locals/operand/control/caller/reference recovery and authenticated loaded-code ownership are still necessary at such stops.

[LLVM zero-shadow StackMaps](https://llvm.org/docs/StackMaps.html) can record code offsets and live-value locations without emitting in-place code. Their default memory effects may constrain optimization; backend support is limited. Inference: supplying every required Wasm value and resolving its exact typed state must be tested before replacing existing complete snapshots, including optimized native stops and suspended callers.

[Linux static keys](https://docs.kernel.org/staging/static-keys.html) reduce disabled branch checks through patching; [LLVM dynamic debugging](https://llvm.org/docs/DynamicDebugging.html) describes optimized/debugging body detours. Inference: these approaches require all-worker patch coordination, loaded-code lifetime/W^X/cache synchronization and precise active-frame transitions here. No jump-label patching, dual bodies or general instrumentation-free Wasm breakpoint recovery was implemented in this iteration. GDB no-hit execution efficiency remains unmet. This iteration removes temporary plan-owner acquisition during root authentication while retaining every requested state capture.

## Reproduction

Raw evidence is retained under /home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/plan-v40-*. Use guard-ssa-v25.py and immutable source/binary manifests. Baselines are uwvm-owner-v39(-ros), candidates uwvm-plan-v40(-ros); both use native Less/local-SSA and host O3. Preserve prior receipts and immutable binaries.
