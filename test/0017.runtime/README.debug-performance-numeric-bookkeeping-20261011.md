# Debug performance: Numeric packet representation bookkeeping (2026-10-11)

This follow-up avoids re-clearing already empty decoded numeric values and re-writing already set private packet flags during consecutive packed captures. The immediate baseline is the qualified deferred plan-owner runtime (plan-v40). All builds, executable tests, real PTYs, timings and VTune ran on SSH Linux in the actual 64 GiB/no-swap cgroup; no local executable testing was performed.

Ten paired CPU-16 runs show lower full call-chain trace medians by 0.88% in uwvm2 and 0.70% in ROS, with all twenty paired call-chain runs faster. Ordinary no-hit and FFT gains remain unestablished. The result is a small full-trace improvement, not GDB no-hit execution parity.

## Implementation and retained guarantees

Every current compiler payload is still authenticated, checked for exact extent/type/flags and copied in full, including all sixteen bytes of every slot. Only internal representation bookkeeping is conditional: an already pending packed numeric frame has empty typed values and is already materialized. Cold expansion, eager or reference commits and actual retirement reset that state. When packing starts again, the typed values are cleared and both flags set normally. The current site is assigned on every capture.

No source/epoch/generation/plan or profile owner/code owner/native-entry validation changes. No caller packets, references, root checks, event snapshots or JIT instrumentation are removed. Observation codegen policy remains 28 and resumable 17; this is a host bookkeeping change. The operand workspace retains its original bounded freelist implementation. Two descriptor-reuse candidates were rejected because complete trace became slower; their raw evidence is retained.

## Paired kernel measurements

Ten unchanged checksummed inputs cover integer, float, branches, dispatch, cache scan, dependent pointer chase, calls and call chains. Main kernels use three alternating before/after repetitions in ordinary, no-hit, cold and disabled-breakpoint modes. Kernel time excludes JIT initialization and breakpoint installation. Native C/GDB uses an equivalent algorithm and its own ordinary baseline. The preliminary primary call-only five-repeat trial is recorded separately. Percentages below are median reductions; negative values are regressions, and small changes are not claims of statistical significance.

| Kernel | uwvm2 before → after (ms) | Reduction | ROS before → after (ms) | Reduction |
|---|---:|---:|---:|---:|
| integer | 2.35720 → 2.35511 | 0.09% | 2.36167 → 2.35642 | 0.22% |
| branches | 5.06241 → 5.06801 | -0.11% | 5.08025 → 5.07149 | 0.17% |
| dispatch | 4.38765 → 4.38839 | -0.02% | 4.38575 → 4.38593 | -0.00% |
| scan32k | 24.09537 → 24.09672 | -0.01% | 24.09844 → 24.10280 | -0.02% |
| scan256k | 24.10411 → 24.12608 | -0.09% | 24.09494 → 24.09782 | -0.01% |
| scan8m | 24.13755 → 24.11053 | 0.11% | 24.11612 → 24.11100 | 0.02% |
| chase8m | 31.80753 → 31.93828 | -0.41% | 31.78060 → 31.80217 | -0.07% |
| calls | 44.97272 → 44.97062 | 0.00% | 44.98162 → 45.03149 | -0.11% |
| callchain | 96.93649 → 96.55705 | 0.39% | 96.47221 → 95.85690 | 0.64% |
| float | 2.28283 → 2.27950 | 0.15% | 2.28315 → 2.28029 | 0.13% |

Four kernels also receive ten serial alternating repetitions pinned to CPU 16, retaining every result:

| Product / kernel | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / calls | 45.07353 → 45.11807 | -0.10% |
| uwvm2 / callchain | 96.96725 → 96.92319 | 0.05% |
| uwvm2 / dispatch | 4.39758 → 4.39816 | -0.01% |
| uwvm2 / chase8m | 30.15915 → 30.20341 | -0.15% |
| uwvm2-ros / calls | 45.18758 → 45.08965 | 0.22% |
| uwvm2-ros / callchain | 96.45327 → 96.42478 | 0.03% |
| uwvm2-ros / dispatch | 4.39784 → 4.40418 | -0.14% |
| uwvm2-ros / chase8m | 30.18018 → 30.15578 | 0.08% |

A separate ten-pair float confirmation pins both products to CPU 16 and retains all samples:

| Product / float | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / float | 2.28272 → 2.28183 | 0.04% |
| uwvm2-ros / float | 2.28194 → 2.28184 | 0.00% |

Two additional checked Wasm/native workloads run with five alternating repetitions on CPU 16: a 64-level recursive call chain and a busy leaf loop under 64 suspended callers. Their generation is optional (`make_debug_workload_matrix.py --deep-calls`); the existing ten inputs remain unchanged. Native C checksums are cross-checked with both Wasm versions.

| Product / deep input | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / deepcallchain | 101.85761 → 101.76049 | 0.10% |
| uwvm2 / deepbusy | 45.29044 → 45.23840 | 0.11% |
| uwvm2-ros / deepcallchain | 102.04188 → 101.44918 | 0.58% |
| uwvm2-ros / deepbusy | 45.41967 → 45.04632 | 0.82% |

### Complete trace and FFT

| Product | Calls complete trace before → after (ms) | Call chain complete trace before → after (ms) | FFT debug before → after (ms) | FFT after debug / ordinary |
|---|---:|---:|---:|---:|
| uwvm2 | 541.986 → 536.880 | 614.408 → 612.309 | 0.978199 → 0.977468 | 13.27× |
| uwvm2-ros | 531.741 → 533.795 | 608.072 → 607.099 | 0.978293 → 0.978706 | 13.18× |

The initial two-pair complete trace result is also checked with ten alternating repetitions pinned to CPU 16, including the exact retained 512-event window:

| Product / full trace | Fixed CPU before → after (ms) | Reduction |
|---|---:|---:|
| uwvm2 / calls | 540.344 → 538.293 | 0.38% |
| uwvm2 / callchain | 615.733 → 610.341 | 0.88% |
| uwvm2-ros / calls | 531.915 → 531.584 | 0.06% |
| uwvm2-ros / callchain | 606.478 → 602.259 | 0.70% |

Paired full-trace differences preserve variation; no sample is discarded. This table compares each after run to its matching before run (it differs slightly from ratios of separate medians):

| Product / input | Median paired reduction | Paired range | Faster pairs |
|---|---:|---:|---:|
| uwvm2 / calls | 0.32% | -0.09% to 1.05% | 8/10 |
| uwvm2 / callchain | 0.81% | 0.22% to 1.82% | 10/10 |
| uwvm2-ros / calls | 0.12% | -1.54% to 0.54% | 7/10 |
| uwvm2-ros / callchain | 0.67% | 0.45% to 0.90% | 10/10 |

Full trace uses two paired full-sized repetitions with a contiguous 512-event retained window. Independent opcode completeness and exact caller data are checked by real PTY fixtures. FFT uses the unchanged checked 2048-point Wasm input with five repetitions per mode; its runner groups modes rather than alternating them.

### GDB and breakpoint reference

| Kernel | uwvm2 debug / own ordinary | GDB / native ordinary |
|---|---:|---:|
| integer | 5.16× | 0.996× |
| branches | 10.91× | 0.992× |
| dispatch | 2.56× | 1.002× |
| scan32k | 29.89× | 1.001× |
| scan256k | 29.56× | 0.995× |
| scan8m | 29.19× | 0.988× |
| chase8m | 1.01× | 1.048× |
| calls | 101.22× | 1.002× |
| callchain | 103.07× | 0.999× |
| float | 5.11× | 1.005× |

| Breakpoint case | uwvm2 (ms) | ROS (ms) | GDB native (ms) |
|---|---:|---:|---:|
| none | 1.382384 | 1.372275 | 0.007393 |
| cold256 | 1.376246 | 1.373695 | 0.007408 |
| disabled256 | 1.380135 | 1.376713 | 0.007392 |
| ignore9999 | 2.729540 | 2.884373 | 936.939400 |
| hits32 | 5.823131 | 5.369567 | 10.413131 |

These are fresh two-repeat real PTYs; visible stops include command/continue transport. Ignored hits still encounter real breakpoints. Native C and Wasm code generation differ; ignored-hit results do not imply GDB no-hit execution parity.

## Rejected candidates

The retained-descriptor workspace (v42) and its alignment variant (v43) are excluded from the final production code. Their full-trace regressions are preserved below rather than treating their faster no-trace calls as an accepted optimization:

| Rejected stage / product / trace input | Before → after (ms) | Reduction |
|---|---:|---:|
| workspace-v42 / uwvm2 / calls | 539.284 → 548.685 | -1.74% |
| workspace-v42 / uwvm2 / callchain | 613.477 → 621.766 | -1.35% |
| workspace-v42 / uwvm2-ros / calls | 531.972 → 541.567 | -1.80% |
| workspace-v42 / uwvm2-ros / callchain | 608.521 → 613.289 | -0.78% |
| workspace-v43 / uwvm2 / calls | 538.804 → 548.033 | -1.71% |
| workspace-v43 / uwvm2 / callchain | 619.782 → 623.854 | -0.66% |

v42 uses ten paired CPU-16 repetitions per product. v43 uses a five-pair primary trial on the allowed E-core cpuset. Immutable inputs, versions, all samples and completed guards are bound in the machine receipt. These candidates are not credited as gains in this final implementation.

## Hardware evidence

Sixteen before/after VTune CLI hardware Hotspots and Microarchitecture Exploration collections use unchanged inputs. Sampling starts at a genuine pre-clock Wasm stop and ends at guest exit before debugger quit/LLVM teardown. Totals include output/exit and collection-control latency and are not pure kernel counts. Actual targets use E cores 16–31; sampler cpuset temporarily becomes 0–31 and restores 16–31.

| Input / analysis | Instructions retired before → after | Reduction |
|---|---:|---:|
| calls/hotspots | 784,800,000 → 784,800,000 | 0.00% |
| calls/uarch-exploration | 785,801,713 → 785,101,547 | 0.09% |
| branches/uarch-exploration | 68,169,827 → 68,185,488 | -0.02% |
| dispatch/uarch-exploration | 57,152,011 → 57,196,028 | -0.08% |
| scan32k/uarch-exploration | 488,835,292 → 488,837,431 | -0.00% |
| chase8m/uarch-exploration | 324,953,398 → 324,964,331 | -0.00% |
| fft120/hotspots | 2,332,800,000 → 2,332,800,000 | 0.00% |
| fft120/uarch-exploration | 2,332,212,086 → 2,332,225,161 | -0.00% |

| Input / uarch | Clockticks before → after | Reduction |
|---|---:|---:|
| calls | 187,929,217 → 187,615,921 | 0.17% |
| branches | 23,363,070 → 24,114,281 | -3.22% |
| dispatch | 20,641,006 → 20,517,785 | 0.60% |
| scan32k | 102,188,553 → 101,340,127 | 0.83% |
| chase8m | 178,698,406 → 169,132,821 | 5.35% |
| fft120 | 488,744,400 → 489,674,869 | -0.19% |

Hardware clockticks are one before/after window per analysis and can differ substantially from the ten-pair fixed-CPU kernel timings. The window also includes collection-control/output/exit work; a large clocktick reduction is not a claim of the same kernel wall-time speedup. All positive and negative hardware comparisons are retained.

Remaining named call hotspots are sampling estimates; anonymous JIT samples cannot be attributed to an opcode:

| Function | Sampled CPU seconds | Estimated retired instructions |
|---|---:|---:|
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet(uwvm2::runtime::checkpoint::activation_identity, std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const> const&, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long)::$_0::operator() | 0.006944 | 21600000 |
| uwvm2::runtime::checkpoint::shadow_ledger::leave | 0.005952 | 9600000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize_compiler_native_data | 0.003968 | 45600000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_abi_bridge | 0.003968 | 36000000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_observer_workspace_leave_abi_bridge | 0.003968 | 7200000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize | 0.002976 | 26400000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_actual_frame | 0.002976 | 36000000 |
| uwvm2::runtime::lib::checkpoint_current_generation_plan | 0.002976 | 26400000 |

## Correctness and synchronization

The final evidence includes 1,368 timing samples, sixteen hardware collections, thirty standard real PTY groups plus four deep-stop PTYs, sixteen current native component runs and ninety-four real trap scenarios. Both release builds passed xmake -r -j3. Each product passed fifteen real PTY groups covering shutdown, probes, memory, typed values, native markers/registers/stepping, cancellation, fast breakpoints, symbols, restart, code generation, numeric SSA, source binding and local calls. Eight native component tests per product passed cache identity, packet shape, shadow ledger, packed numeric packet and LLVM IR/actual MCJIT checks with active assertions. Existing tests retain protected-page extents, invalid flags, numeric exact bits/signaling NaNs/v128, anti-alias controlblocks, references, callers and fallback paths.

The extended operand-storage component exercises 80 nested large frames, stable live buffer addresses, rejected non-top retirement, fresh flags/payload/padding after reuse, invalid metadata/extent/overflow labels, extra operand slots beyond the local prefix, suspended-parent snapshot rejection, real child return, inactive snapshot rejection. Four 40-depth waves and 128 leaf calls with 64 live callers check bounded-pool reuse, full zeroing and preserved callers. The numeric-packet test adds 48 fresh activation cycles alternating pending replacement, removed operands, stop-time expansion, eager capture and packed capture, including zero locals. Existing source-owner/frame/packed-packet tests continue to cover anti-alias controlblocks, owner release, mixed-type depth reuse, exact references, rejected packets and sticky failure. Real multi-module PTYs exercise exact suspended callers, replacement generation 2 and restart; stale and active replacement admission remains rejected.

Four additional real deep-stop PTYs (both versions of both products) stop at the first busy leaf call, inspect exact typed locals in 66 frames (leaf, busy caller and all 64 suspended recursive callers), then disable the breakpoint and verify the independently computed checksum. Their deliberate stop/inspection time is excluded from performance results.

Both products passed 34 caught real Wasm traps plus eight memory64 boundary traps with exact before-trap values and terminal execution rejected. Ten unobserved traps exited with expected SIGILL and core dumps disabled. All final memory event counters remain zero, sampler cpuset is restored and no execution/profiling workers remain.

The [machine receipt](debug_performance_numeric_bookkeeping_20261011.json) binds all 92 source hashes to completed guarded runs and immutable binary hashes. Receipt SHA-256: `8174aa9faf862640a5847654af32b7d07f236f442328b521a41cc7aaa3ec7803`. The [sync audit](debug_performance_numeric_bookkeeping_20261011_sync.json) checks both local products and corresponding SSH copies. Only the current Linux release configuration was executed; macOS/Windows, exception-disabled configurations and other-platform UI behavior were not requalified.

## Architecture research and remaining limits

[GDB software breakpoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Breaks.html) patch selected native instructions with traps; [displaced stepping](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Maintenance-Commands.html) executes an out-of-line original instruction while retaining the breakpoint. [LLVM StackMaps](https://llvm.org/docs/StackMaps.html) associate requested live register/spill/constant values with code offsets. Inference: complete Wasm typed locals/operand/control/caller/reference recovery and authenticated loaded-code ownership are still necessary at such stops.

[LLVM zero-shadow StackMaps](https://llvm.org/docs/StackMaps.html) can record code offsets and live-value locations without emitting in-place code. Their default memory effects may constrain optimization; backend support is limited. Inference: supplying every required Wasm value and resolving its exact typed state must be tested before replacing existing complete snapshots, including optimized native stops and suspended callers.

[Linux static keys](https://docs.kernel.org/staging/static-keys.html) reduce disabled branch checks through patching; [LLVM dynamic debugging](https://llvm.org/docs/DynamicDebugging.html) describes optimized/debugging body detours. Inference: these approaches require all-worker patch coordination, loaded-code lifetime/W^X/cache synchronization and precise active-frame transitions here. No jump-label patching, dual bodies or general instrumentation-free Wasm breakpoint recovery was implemented in this iteration. GDB no-hit execution efficiency remains unmet. This iteration reduces repeated private packet bookkeeping while retaining every requested state capture. The gain is small and does not approach GDB no-hit execution efficiency.

A further research candidate is an asm-goto / [LLVM callbr](https://llvm.org/docs/LangRef.html#callbr-instruction) edge whose normal native path contains reserved NOP bytes and whose patched path enters the existing complete cold producer. [GCC asm goto](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html#Goto-Labels) explicitly models fallthrough and label destinations. Inference: such an edge might remove repeated demand-byte loads while keeping complete capture available; it is not a complete instrumentation-free state-recovery engine and has not been implemented or benchmarked here. LLVM may split indirect edges, so a raw blockaddress is not sufficient proof of the loaded destination. A prototype would require real relocation-bound site ownership, all-worker quiescence including asynchronous pause and tight loops, W^X/instruction-cache coordination, exact preservation of memory-operation ordering and live values, cancellation/step correctness and generation retirement.

[LLVM XRay](https://llvm.org/docs/XRay.html) uses compiler-reserved NOP sleds that can enable or disable function tracing through runtime patching, including individual functions. Inference: this supports researching patchable dormant observation paths, but function entry/exit tracing alone cannot recover Wasm operands, locals, references, controls and suspended callers. A JIT integration must bind sled locations to the actual loaded code and coordinate active frames and all workers. XRay was researched, not integrated or benchmarked in this iteration.

## Reproduction

Raw evidence is retained under /home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/numeric-v44-*. Use guard-ssa-v25.py and immutable source/binary manifests. Baselines are uwvm-plan-v40(-ros), candidates uwvm-numeric-v44(-ros); both use native Less/local-SSA and host O3. Preserve prior receipts and immutable binaries.
