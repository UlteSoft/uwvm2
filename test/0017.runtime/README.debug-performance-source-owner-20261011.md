# Debug performance: Selected source ownership comparison (2026-10-11)

This follow-up avoids temporary shared ownership solely for a selected-source identity comparison. The immediate baseline is the qualified retired frame storage runtime (frames-v38). All builds, executable tests, real PTYs, timings and VTune ran on SSH Linux in the actual 64 GiB/no-swap cgroup; no local executable testing was performed.

## Implementation and retained guarantees

Root authentication formerly obtained a temporary selected_full_source_owner_pin, compared its object and control block, then destroyed the temporary. The new selected_full_source_matches_owner performs the same non-null, object-address and both owner_before comparisons directly against the selected shared owner, inside the existing execution/publication or serialized maintenance scope. It does not dereference a candidate, return a borrowed pointer, retain an owner or grant independent lifetime/admission. Root sealing uses the same comparison under its actual publication scope.

Every canonical source, initializer, current epoch, compiled generation, plan/profile controlblock, selected native entry, registry membership, real code owner and replacement admission condition remains intact. No cached affirmative result or caller-provided identity is introduced. All requested before-opcode, awaiting-caller and returned-caller packets retain complete synchronous copying and reference validation. No observer, safepoint or JIT instrumentation was removed. Observation codegen policy remains 28 and resumable remains 17; this changes host authentication cost, not generated IR/cache identity.

The primary root comparison disassembly contains two locked instructions before the change and zero afterward, binding both listings to qualified binary hashes. This verifies the removed temporary reference-count traffic; it is not a claim that JIT instrumentation disappeared.

## Paired kernel measurements

Ten unchanged checksummed inputs cover integer, float, branches, dispatch, cache scan, dependent pointer chase, calls and call chains. Main kernels use three alternating before/after repetitions in ordinary, no-hit, cold and disabled-breakpoint modes. Kernel time excludes JIT initialization and breakpoint installation. Native C/GDB uses an equivalent algorithm and its own ordinary baseline. The preliminary primary call-only five-repeat trial is recorded separately. Percentages below are median reductions; negative values are regressions, and small changes are not claims of statistical significance.

| Kernel | uwvm2 before → after (ms) | Reduction | ROS before → after (ms) | Reduction |
|---|---:|---:|---:|---:|
| integer | 2.35459 → 2.35591 | -0.06% | 2.35674 → 2.35406 | 0.11% |
| branches | 5.07387 → 5.07486 | -0.02% | 5.07397 → 5.05830 | 0.31% |
| dispatch | 4.38870 → 4.39051 | -0.04% | 4.40159 → 4.37983 | 0.49% |
| scan32k | 24.11659 → 24.12068 | -0.02% | 24.10099 → 24.09485 | 0.03% |
| scan256k | 24.09733 → 24.10294 | -0.02% | 24.09977 → 24.09819 | 0.01% |
| scan8m | 24.11547 → 24.12128 | -0.02% | 24.11254 → 24.12619 | -0.06% |
| chase8m | 29.82698 → 30.01807 | -0.64% | 29.92572 → 29.91820 | 0.03% |
| calls | 48.03495 → 46.73574 | 2.70% | 48.16682 → 46.98638 | 2.45% |
| callchain | 103.71481 → 100.93396 | 2.68% | 103.33008 → 99.78215 | 3.43% |
| float | 2.28187 → 2.28220 | -0.01% | 2.28962 → 2.28269 | 0.30% |

Four kernels also receive ten serial alternating repetitions pinned to CPU 16, retaining every result:

| Product / kernel | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / calls | 48.00464 → 46.82865 | 2.45% |
| uwvm2 / callchain | 103.19625 → 100.73200 | 2.39% |
| uwvm2 / dispatch | 4.40472 → 4.39534 | 0.21% |
| uwvm2 / chase8m | 30.26232 → 30.15226 | 0.36% |
| uwvm2-ros / calls | 48.16284 → 46.91445 | 2.59% |
| uwvm2-ros / callchain | 103.08530 → 100.01168 | 2.98% |
| uwvm2-ros / dispatch | 4.40075 → 4.40219 | -0.03% |
| uwvm2-ros / chase8m | 30.18030 → 30.18457 | -0.01% |

### Complete trace and FFT

| Product | Calls complete trace before → after (ms) | Call chain complete trace before → after (ms) | FFT debug before → after (ms) | FFT after debug / ordinary |
|---|---:|---:|---:|---:|
| uwvm2 | 543.502 → 544.036 | 620.986 → 619.321 | 0.976866 → 0.978660 | 13.28× |
| uwvm2-ros | 534.185 → 536.054 | 618.686 → 611.437 | 0.977622 → 0.977923 | 13.20× |

Full trace uses two paired full-sized repetitions with a contiguous 512-event retained window. Independent opcode completeness and exact caller data are checked by real PTY fixtures. FFT uses the unchanged checked 2048-point Wasm input with five repetitions per mode; its runner groups modes rather than alternating them.

### GDB and breakpoint reference

| Kernel | uwvm2 debug / own ordinary | GDB / native ordinary |
|---|---:|---:|
| integer | 5.10× | 1.011× |
| branches | 10.93× | 1.002× |
| dispatch | 2.57× | 1.001× |
| scan32k | 29.64× | 0.997× |
| scan256k | 29.56× | 0.996× |
| scan8m | 28.66× | 0.959× |
| chase8m | 0.95× | 1.084× |
| calls | 106.10× | 1.001× |
| callchain | 107.52× | 0.991× |
| float | 5.14× | 0.991× |

| Breakpoint case | uwvm2 (ms) | ROS (ms) | GDB native (ms) |
|---|---:|---:|---:|
| none | 1.418982 | 1.406305 | 0.007399 |
| cold256 | 1.423262 | 1.419766 | 0.007402 |
| disabled256 | 1.412621 | 1.402985 | 0.007406 |
| ignore9999 | 3.485072 | 2.793699 | 922.259470 |
| hits32 | 5.298184 | 5.214050 | 10.686813 |

These are fresh two-repeat real PTYs; visible stops include command/continue transport. Ignored hits still encounter real breakpoints. Native C and Wasm code generation differ; ignored-hit results do not imply GDB no-hit execution parity.

## Hardware evidence

Sixteen before/after VTune CLI hardware Hotspots and Microarchitecture Exploration collections use unchanged inputs. Sampling starts at a genuine pre-clock Wasm stop and ends at guest exit before debugger quit/LLVM teardown. Totals include output/exit and collection-control latency and are not pure kernel counts. Actual targets use E cores 16–31; sampler cpuset temporarily becomes 0–31 and restores 16–31.

| Input / analysis | Instructions retired before → after | Reduction |
|---|---:|---:|
| calls/hotspots | 796,800,000 → 792,000,000 | 0.60% |
| calls/uarch-exploration | 796,794,921 → 791,836,392 | 0.62% |
| branches/uarch-exploration | 68,168,658 → 68,192,017 | -0.03% |
| dispatch/uarch-exploration | 57,194,332 → 57,257,800 | -0.11% |
| scan32k/uarch-exploration | 488,785,365 → 488,824,072 | -0.01% |
| chase8m/uarch-exploration | 325,021,357 → 324,977,050 | 0.01% |
| fft120/hotspots | 2,332,800,000 → 2,332,800,000 | 0.00% |
| fft120/uarch-exploration | 2,332,405,303 → 2,332,229,182 | 0.01% |

| Input / uarch | Clockticks before → after | Reduction |
|---|---:|---:|
| calls | 200,643,617 → 197,023,606 | 1.80% |
| branches | 23,078,459 → 23,648,895 | -2.47% |
| dispatch | 20,438,597 → 21,583,502 | -5.60% |
| scan32k | 101,116,668 → 101,508,231 | -0.39% |
| chase8m | 177,907,442 → 169,286,725 | 4.85% |
| fft120 | 489,224,789 → 487,608,278 | 0.33% |

Remaining named call hotspots are sampling estimates; anonymous JIT samples cannot be attributed to an opcode:

| Function | Sampled CPU seconds | Estimated retired instructions |
|---|---:|---:|
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge | 0.005952 | 67200000 |
| uwvm2::runtime::lib::checkpoint_current_generation_plan | 0.004960 | 43200000 |
| std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const>::~shared_ptr[abi:nqe230101] | 0.003968 | 7200000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_actual_frame | 0.003968 | 38400000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize_compiler_native_data | 0.002976 | 43200000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet(uwvm2::runtime::checkpoint::activation_identity, std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const> const&, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long)::$_0::operator() | 0.002976 | 12000000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_root_generation_publication_bridge::matches_current_root | 0.002976 | 43200000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_abi_bridge | 0.002976 | 52800000 |

## Correctness and synchronization

Both release builds passed xmake -r -j3. Each product passed fifteen real PTY groups covering shutdown, probes, memory, typed values, native markers/registers/stepping, cancellation, fast breakpoints, symbols, restart, code generation, numeric SSA, source binding and local calls. Seven native component tests per product passed cache identity, packet shape, shadow ledger, packed numeric packet and LLVM IR/actual MCJIT checks with active assertions. Existing tests retain protected-page extents, invalid flags, numeric exact bits/signaling NaNs/v128, anti-alias controlblocks, references, callers and fallback paths.

A new native component exercises serialized source selection, an empty root, exact owner identity, the same address under a foreign controlblock, a different address under the same controlblock, an unreadable foreign alias, retirement and replacement. The comparison retains no extra owner. Existing frame tests continue to cover owner release, mixed-type depth reuse, exact references, rejected packets and sticky failure. Real multi-module PTYs exercise exact suspended callers, replacement generation 2 and restart; stale and active replacement admission remains rejected.

Both products passed 34 caught real Wasm traps plus eight memory64 boundary traps with exact before-trap values and terminal execution rejected. Ten unobserved traps exited with expected SIGILL and core dumps disabled. All final memory event counters remain zero, sampler cpuset is restored and no execution/profiling workers remain.

The [machine receipt](debug_performance_source_owner_20261011.json) binds all 92 source hashes to completed guarded runs and immutable binary hashes. Receipt SHA-256: `64cd5c774880be60fc8084fe892327352a113b7ca0354550d8c8c9a92e2024e5`. The [sync audit](debug_performance_source_owner_20261011_sync.json) checks both local products and corresponding SSH copies. Only the current Linux release configuration was executed; macOS/Windows, exception-disabled configurations and other-platform UI behavior were not requalified.

## Architecture research and remaining limits

[GDB software breakpoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Breaks.html) patch selected native instructions with traps; [displaced stepping](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Maintenance-Commands.html) executes an out-of-line original instruction while retaining the breakpoint. [LLVM StackMaps](https://llvm.org/docs/StackMaps.html) associate requested live register/spill/constant values with code offsets. Inference: complete Wasm typed locals/operand/control/caller/reference recovery and authenticated loaded-code ownership are still necessary at such stops.

[LLVM zero-shadow StackMaps](https://llvm.org/docs/StackMaps.html) can record code offsets and live-value locations without emitting in-place code. Their default memory effects may constrain optimization; backend support is limited. Inference: supplying every required Wasm value and resolving its exact typed state must be tested before replacing existing complete snapshots, including optimized native stops and suspended callers.

[Linux static keys](https://docs.kernel.org/staging/static-keys.html) reduce disabled branch checks through patching; [LLVM dynamic debugging](https://llvm.org/docs/DynamicDebugging.html) describes optimized/debugging body detours. Inference: these approaches require all-worker patch coordination, loaded-code lifetime/W^X/cache synchronization and precise active-frame transitions here. No jump-label patching, dual bodies or general instrumentation-free Wasm breakpoint recovery was implemented in this iteration. GDB no-hit execution efficiency remains unmet. This iteration removes temporary source-owner acquisition during root authentication while retaining every requested state capture.

## Reproduction

Raw evidence is retained under /home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/owner-v39-*. Use guard-ssa-v25.py and immutable source/binary manifests. Baselines are uwvm-frames-v38(-ros), candidates uwvm-owner-v39(-ros); both use native Less/local-SSA and host O3. Preserve prior receipts and immutable binaries.
