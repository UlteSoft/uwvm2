# Debug performance: Bounded operand workspace storage reuse (2026-10-11)

This follow-up retains operand-workspace metadata at real call depth. The immediate baseline is the qualified deferred plan-owner runtime (plan-v40). All builds, executable tests, real PTYs, timings and VTune ran on SSH Linux in the actual 64 GiB/no-swap cgroup; no local executable testing was performed.

## Implementation and retained guarantees

The old workspace created and removed a vector descriptor on every real call/return and swapped capacity through a fixed freelist. The new workspace keeps empty descriptors at each actual call depth, resizes the current inactive descriptor, and publishes active depth only after successful initialization. The same depth can reuse its byte capacity without descriptor construction or freelist swaps. Full byte zero initialization remains mandatory, including every local initialization flag and payload/padding byte.

At most 32 inactive slots may retain byte capacity at any real depth, each at most 16 KiB (at most 512 KiB total inactive byte capacity). Oversized buffers and buffers exceeding the inactive-slot quota release at the real LIFO return. Peak vector metadata may persist, as the existing outer vector allocation did; it supplies no active-frame authority. Size and cold current-local snapshot use actual active depth, excluding every inactive descriptor. A failed allocation leaves the active LIFO unchanged. The exact top buffer still must match before retirement or a cold snapshot exposes any pointer.

No canonical source, initializer, epoch, compiled generation, plan/profile controlblock, native entry, code owner, breakpoint admission or replacement check changes. All requested before-opcode, awaiting-caller and returned-caller packets retain complete copying and reference validation. No observer, safepoint or JIT instrumentation is removed. Observation codegen policy remains 28 and resumable remains 17; this changes host storage cost, not generated IR/cache identity. Actual enter/leave bridge disassembly for both baseline and candidate products is retained in the receipt without assigning static instruction counts to dynamic call costs.

## Paired kernel measurements

Ten unchanged checksummed inputs cover integer, float, branches, dispatch, cache scan, dependent pointer chase, calls and call chains. Main kernels use three alternating before/after repetitions in ordinary, no-hit, cold and disabled-breakpoint modes. Kernel time excludes JIT initialization and breakpoint installation. Native C/GDB uses an equivalent algorithm and its own ordinary baseline. The preliminary primary call-only five-repeat trial is recorded separately. Percentages below are median reductions; negative values are regressions, and small changes are not claims of statistical significance.

| Kernel | uwvm2 before → after (ms) | Reduction | ROS before → after (ms) | Reduction |
|---|---:|---:|---:|---:|
| integer | 2.35594 → 2.35556 | 0.02% | 2.35707 → 2.35694 | 0.01% |
| branches | 5.09671 → 5.06870 | 0.55% | 5.07513 → 5.05867 | 0.32% |
| dispatch | 4.37788 → 4.38613 | -0.19% | 4.38897 → 4.40158 | -0.29% |
| scan32k | 24.10264 → 24.09859 | 0.02% | 24.10068 → 24.10038 | 0.00% |
| scan256k | 24.10013 → 24.09920 | 0.00% | 24.14455 → 24.09899 | 0.19% |
| scan8m | 24.10504 → 24.10873 | -0.02% | 24.11232 → 24.10784 | 0.02% |
| chase8m | 30.08043 → 31.79101 | -5.69% | 30.05900 → 31.82553 | -5.88% |
| calls | 45.12470 → 44.61528 | 1.13% | 45.12317 → 44.25338 | 1.93% |
| callchain | 97.23155 → 94.69756 | 2.61% | 96.36744 → 95.12073 | 1.29% |
| float | 2.28262 → 2.33642 | -2.36% | 2.28234 → 2.28374 | -0.06% |

Four kernels also receive ten serial alternating repetitions pinned to CPU 16, retaining every result:

| Product / kernel | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / calls | 45.10361 → 44.63655 | 1.04% |
| uwvm2 / callchain | 96.94172 → 94.96643 | 2.04% |
| uwvm2 / dispatch | 4.39847 → 4.39541 | 0.07% |
| uwvm2 / chase8m | 30.19013 → 30.20449 | -0.05% |
| uwvm2-ros / calls | 45.13881 → 44.38919 | 1.66% |
| uwvm2-ros / callchain | 96.49433 → 94.67419 | 1.89% |
| uwvm2-ros / dispatch | 4.39695 → 4.39750 | -0.01% |
| uwvm2-ros / chase8m | 30.31043 → 30.25576 | 0.18% |

The initial primary float median also showed a negative result. A separate ten-pair float confirmation pins both products to CPU 16 and retains all samples:

| Product / float | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / float | 2.28155 → 2.28184 | -0.01% |
| uwvm2-ros / float | 2.28179 → 2.28146 | 0.01% |

Two additional checked Wasm/native workloads run with five alternating repetitions on CPU 16: a 64-level recursive call chain and a busy leaf loop under 64 suspended callers. Their generation is optional (`make_debug_workload_matrix.py --deep-calls`); the existing ten inputs remain unchanged. Native C checksums are cross-checked with both Wasm versions.

| Product / deep input | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / deepcallchain | 101.72970 → 99.71394 | 1.98% |
| uwvm2 / deepbusy | 45.31918 → 44.40501 | 2.02% |
| uwvm2-ros / deepcallchain | 101.80835 → 100.03837 | 1.74% |
| uwvm2-ros / deepbusy | 45.29632 → 44.56352 | 1.62% |

### Complete trace and FFT

| Product | Calls complete trace before → after (ms) | Call chain complete trace before → after (ms) | FFT debug before → after (ms) | FFT after debug / ordinary |
|---|---:|---:|---:|---:|
| uwvm2 | 538.867 → 550.498 | 616.022 → 622.243 | 0.977927 → 0.978738 | 13.26× |
| uwvm2-ros | 539.563 → 541.456 | 606.916 → 613.296 | 0.977676 → 0.978088 | 13.20× |

Full trace uses two paired full-sized repetitions with a contiguous 512-event retained window. Independent opcode completeness and exact caller data are checked by real PTY fixtures. FFT uses the unchanged checked 2048-point Wasm input with five repetitions per mode; its runner groups modes rather than alternating them.

### GDB and breakpoint reference

| Kernel | uwvm2 debug / own ordinary | GDB / native ordinary |
|---|---:|---:|
| integer | 5.12× | 1.000× |
| branches | 11.00× | 0.999× |
| dispatch | 2.57× | 0.998× |
| scan32k | 29.61× | 1.004× |
| scan256k | 29.62× | 1.002× |
| scan8m | 29.29× | 0.979× |
| chase8m | 1.01× | 1.028× |
| calls | 101.27× | 1.003× |
| callchain | 100.92× | 1.010× |
| float | 5.21× | 0.996× |

| Breakpoint case | uwvm2 (ms) | ROS (ms) | GDB native (ms) |
|---|---:|---:|---:|
| none | 1.361575 | 1.369266 | 0.007385 |
| cold256 | 1.358828 | 1.363430 | 0.007395 |
| disabled256 | 1.361120 | 1.358658 | 0.007399 |
| ignore9999 | 2.957042 | 3.051122 | 953.117722 |
| hits32 | 5.726517 | 4.878273 | 10.661631 |

These are fresh two-repeat real PTYs; visible stops include command/continue transport. Ignored hits still encounter real breakpoints. Native C and Wasm code generation differ; ignored-hit results do not imply GDB no-hit execution parity.

## Hardware evidence

Sixteen before/after VTune CLI hardware Hotspots and Microarchitecture Exploration collections use unchanged inputs. Sampling starts at a genuine pre-clock Wasm stop and ends at guest exit before debugger quit/LLVM teardown. Totals include output/exit and collection-control latency and are not pure kernel counts. Actual targets use E cores 16–31; sampler cpuset temporarily becomes 0–31 and restores 16–31.

| Input / analysis | Instructions retired before → after | Reduction |
|---|---:|---:|
| calls/hotspots | 784,800,000 → 775,200,000 | 1.22% |
| calls/uarch-exploration | 785,807,455 → 775,562,204 | 1.30% |
| branches/uarch-exploration | 68,189,212 → 68,233,058 | -0.06% |
| dispatch/uarch-exploration | 57,248,186 → 57,234,024 | 0.02% |
| scan32k/uarch-exploration | 488,793,586 → 488,784,088 | 0.00% |
| chase8m/uarch-exploration | 324,977,115 → 325,089,853 | -0.03% |
| fft120/hotspots | 2,332,800,000 → 2,332,800,000 | 0.00% |
| fft120/uarch-exploration | 2,332,174,439 → 2,331,982,061 | 0.01% |

| Input / uarch | Clockticks before → after | Reduction |
|---|---:|---:|
| calls | 187,894,170 → 184,769,862 | 1.66% |
| branches | 24,079,699 → 23,358,295 | 3.00% |
| dispatch | 21,060,558 → 20,972,691 | 0.42% |
| scan32k | 101,420,110 → 101,290,657 | 0.13% |
| chase8m | 177,837,402 → 198,682,102 | -11.72% |
| fft120 | 489,365,944 → 489,372,538 | -0.00% |

Hardware clockticks are one before/after window per analysis and can differ substantially from the ten-pair fixed-CPU kernel timings. The window also includes collection-control/output/exit work; a large clocktick reduction is not a claim of the same kernel wall-time speedup. All positive and negative hardware comparisons are retained.

Remaining named call hotspots are sampling estimates; anonymous JIT samples cannot be attributed to an opcode:

| Function | Sampled CPU seconds | Estimated retired instructions |
|---|---:|---:|
| uwvm2::runtime::checkpoint::shadow_ledger::materialize_compiler_native_data | 0.007937 | 84000000 |
| uwvm2::runtime::lib::checkpoint_current_generation_plan | 0.007937 | 9600000 |
| uwvm2::runtime::checkpoint::shadow_ledger::leave | 0.005952 | 16800000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet | 0.004960 | 48000000 |
| uwvm2::runtime::checkpoint::shadow_ledger::enter | 0.002976 | 26400000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet(uwvm2::runtime::checkpoint::activation_identity, std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const> const&, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long)::$_0::operator() | 0.002976 | 26400000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge | 0.002976 | 57600000 |
| std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const>::~shared_ptr[abi:nqe230101] | 0.001984 | 9600000 |

## Correctness and synchronization

Both release builds passed xmake -r -j3. Each product passed fifteen real PTY groups covering shutdown, probes, memory, typed values, native markers/registers/stepping, cancellation, fast breakpoints, symbols, restart, code generation, numeric SSA, source binding and local calls. Eight native component tests per product passed cache identity, packet shape, shadow ledger, packed numeric packet and LLVM IR/actual MCJIT checks with active assertions. Existing tests retain protected-page extents, invalid flags, numeric exact bits/signaling NaNs/v128, anti-alias controlblocks, references, callers and fallback paths.

The extended operand-storage component exercises 80 nested large frames, stable live buffer addresses, rejected non-top retirement, fresh flags/payload/padding after reuse, invalid metadata/extent/overflow labels, extra operand slots beyond the local prefix, suspended-parent snapshot rejection, real child return, inactive snapshot rejection. Four 40-depth waves and 128 leaf calls with 64 live callers check arbitrary-depth reuse, full zeroing and preserved callers without exposing inactive frames. Existing source-owner/frame/packed-packet tests continue to cover anti-alias controlblocks, owner release, mixed-type depth reuse, exact references, rejected packets and sticky failure. Real multi-module PTYs exercise exact suspended callers, replacement generation 2 and restart; stale and active replacement admission remains rejected.

Four additional real deep-stop PTYs (both versions of both products) stop at the first busy leaf call, inspect exact typed locals in 66 frames (leaf, busy caller and all 64 suspended recursive callers), then disable the breakpoint and verify the independently computed checksum. Their deliberate stop/inspection time is excluded from performance results.

Both products passed 34 caught real Wasm traps plus eight memory64 boundary traps with exact before-trap values and terminal execution rejected. Ten unobserved traps exited with expected SIGILL and core dumps disabled. All final memory event counters remain zero, sampler cpuset is restored and no execution/profiling workers remain.

The [machine receipt](debug_performance_operand_storage_20261011.json) binds all 92 source hashes to completed guarded runs and immutable binary hashes. Receipt SHA-256: `24bb40bc67b9f4483b32a73bfbee660c66b82320719cd5f0f2bf73718789ea3e`. The [sync audit](debug_performance_operand_storage_20261011_sync.json) checks both local products and corresponding SSH copies. Only the current Linux release configuration was executed; macOS/Windows, exception-disabled configurations and other-platform UI behavior were not requalified.

## Architecture research and remaining limits

[GDB software breakpoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Breaks.html) patch selected native instructions with traps; [displaced stepping](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Maintenance-Commands.html) executes an out-of-line original instruction while retaining the breakpoint. [LLVM StackMaps](https://llvm.org/docs/StackMaps.html) associate requested live register/spill/constant values with code offsets. Inference: complete Wasm typed locals/operand/control/caller/reference recovery and authenticated loaded-code ownership are still necessary at such stops.

[LLVM zero-shadow StackMaps](https://llvm.org/docs/StackMaps.html) can record code offsets and live-value locations without emitting in-place code. Their default memory effects may constrain optimization; backend support is limited. Inference: supplying every required Wasm value and resolving its exact typed state must be tested before replacing existing complete snapshots, including optimized native stops and suspended callers.

[Linux static keys](https://docs.kernel.org/staging/static-keys.html) reduce disabled branch checks through patching; [LLVM dynamic debugging](https://llvm.org/docs/DynamicDebugging.html) describes optimized/debugging body detours. Inference: these approaches require all-worker patch coordination, loaded-code lifetime/W^X/cache synchronization and precise active-frame transitions here. No jump-label patching, dual bodies or general instrumentation-free Wasm breakpoint recovery was implemented in this iteration. GDB no-hit execution efficiency remains unmet. This iteration reduces repeated operand-storage metadata work while retaining every requested state capture.

A further research candidate is an asm-goto / [LLVM callbr](https://llvm.org/docs/LangRef.html#callbr-instruction) edge whose normal native path contains reserved NOP bytes and whose patched path enters the existing complete cold producer. [GCC asm goto](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html#Goto-Labels) explicitly models fallthrough and label destinations. Inference: such an edge might remove repeated demand-byte loads while keeping complete capture available; it is not a complete instrumentation-free state-recovery engine and has not been implemented or benchmarked here. LLVM may split indirect edges, so a raw blockaddress is not sufficient proof of the loaded destination. A prototype would require real relocation-bound site ownership, all-worker quiescence including asynchronous pause and tight loops, W^X/instruction-cache coordination, exact preservation of memory-operation ordering and live values, cancellation/step correctness and generation retirement.

## Reproduction

Raw evidence is retained under /home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/workspace-v42-*. Use guard-ssa-v25.py and immutable source/binary manifests. Baselines are uwvm-plan-v40(-ros), candidates uwvm-workspace-v42(-ros); both use native Less/local-SSA and host O3. Preserve prior receipts and immutable binaries.
