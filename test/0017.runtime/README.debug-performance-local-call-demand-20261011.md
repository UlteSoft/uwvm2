# Debug performance: Local typed-call observation demand (2026-10-11)

This follow-up avoids an unrequested numeric before-call observation at actual full local typed Wasm calls, while retaining complete awaiting and returned caller packets. The immediate baseline is the qualified sealed-source-binding runtime. All compilation, executable tests, timings and VTune ran on SSH Linux in the actual 64 GiB/no-swap cgroup; no local executable testing was performed.

## Change and retained guarantees

Linux x86-64 observation-profile direct-call points are emitted after the one original immediate/signature decoder validates the target, but before either semantic or LLVM arguments are popped. A full local typed target can then select the existing one-byte acquire demand gate when the sealed complete site has no references or uninitialized values. No public function ID, cached token or pointer grants state access. The complete awaiting-caller packet is still generated and synchronously copied unconditionally before child entry; the complete returned-caller packet remains unconditional. A child breakpoint therefore retains exact caller locals, unconsumed operand prefix, controls, handlers and saved parameters. Full trace, stepping, pause, shutdown or requested call breakpoints still enter the complete before-call path with every argument present. Imports/host waits, indirect/tail calls, live references, unsupported profiles/platforms and non-full/routed targets retain their mandatory behavior.

The immutable observation codegen policy increases from 27 to 28 so existing cached IR cannot silently reuse the old producer policy. Resumable policy remains 17. Other platform wrappers were not changed or requalified. Actual publication/control-block/epoch/generation admission, signal-mask checks, caller/reference/GC ownership and exact state copies remain. This does not implement a general instrumentation-free Wasm breakpoint engine.

A preceding host-call-fusion candidate qualified but showed little primary timing gain in its call-only trial; its wider timing run was deliberately interrupted and not counted as a pass. The final candidate also retains that Linux x86-64 fusion: the private C leaf returns its authenticated arm decision in EAX after the real observer/poll and cancellation handling. The naked wrapper uses one host call/reservation rather than two, sets TF only at its final POPFQ/RET and retains the public generated five-argument void ABI. Exact source hashes distinguish the final combined candidate from the intermediate trial.

## Paired kernel measurements

Ten unchanged checksummed inputs cover integer, float, control flow, dispatch, cache scan, dependent pointer chase, calls and call chains. Each main input runs three alternating before/after repetitions in ordinary, no-hit, cold-breakpoint and disabled-breakpoint modes. Native C/GDB uses an independent equivalent algorithm and its own ordinary baseline. Guest monotonic kernel time excludes JIT initialization and breakpoint installation. Raw samples and exact hashes are retained in the receipt; an earlier 48-sample call-only trial is separate from the reported main matrix.

| Kernel | uwvm2 before → after (ms) | Reduction | ROS before → after (ms) | Reduction |
|---|---:|---:|---:|---:|
| integer | 2.35618 → 2.35305 | 0.13% | 2.35644 → 2.35621 | 0.01% |
| branches | 5.03475 → 5.07635 | -0.83% | 5.06548 → 5.06721 | -0.03% |
| dispatch | 4.35324 → 4.38243 | -0.67% | 4.40799 → 4.38578 | 0.50% |
| scan32k | 24.11379 → 24.10276 | 0.05% | 24.09960 → 24.09672 | 0.01% |
| scan256k | 24.09705 → 24.09688 | 0.00% | 24.09510 → 24.12054 | -0.11% |
| scan8m | 24.10718 → 24.10514 | 0.01% | 24.12010 → 24.14179 | -0.09% |
| chase8m | 32.03139 → 31.59802 | 1.35% | 31.82129 → 30.05804 | 5.54% |
| calls | 62.91497 → 49.04146 | 22.05% | 65.39815 → 48.92041 | 25.20% |
| callchain | 135.08484 → 105.81324 | 21.67% | 135.66843 → 105.11222 | 22.52% |
| float | 2.28484 → 2.28114 | 0.16% | 2.28070 → 2.28112 | -0.02% |

Negative reductions indicate a slowdown and are not counted as improvements. Kernel timings are medians, not claims of statistical significance. Dispatch and dependent pointer chase also receive a serial ten-repeat check pinned to CPU 16:

| Kernel / product | Fixed-CPU before → after (ms) | Reduction |
|---|---:|---:|
| dispatch / uwvm2 | 4.36930 → 4.40090 | -0.72% |
| chase8m / uwvm2 | 30.22895 → 30.20403 | 0.08% |
| dispatch / uwvm2-ros | 4.41859 → 4.40061 | 0.41% |
| chase8m / uwvm2-ros | 30.22855 → 30.18240 | 0.15% |

Fixed-CPU checks retain all results without discarding slower samples. Percentages are medians, not statistical-significance claims; reductions and regressions below a few percent need to be assessed against run variability.


### Complete trace and FFT

| Product | Calls complete trace before → after (ms) | Call chain complete trace before → after (ms) | FFT debug before → after (ms) | FFT after debug / ordinary |
|---|---:|---:|---:|---:|
| uwvm2 | 562.765 → 538.707 | 622.917 → 624.127 | 0.987869 → 0.977052 | 13.23× |
| uwvm2-ros | 538.600 → 533.063 | 618.652 → 612.036 | 0.992072 → 0.978162 | 13.19× |

Trace timings use two paired full-sized repetitions and a contiguous 512-event retained window; exact opcode completeness is independently checked by the seven-opcode PTY fixture. FFT uses the same checked 2048-point Wasm input and five repetitions per mode. Its runner groups modes rather than alternating them.

### GDB reference and breakpoint costs

| Kernel | uwvm2 debug / own ordinary | GDB / native ordinary |
|---|---:|---:|---:|
| integer | 5.07× | 1.000× |
| branches | 11.02× | 1.009× |
| dispatch | 2.56× | 1.000× |
| scan32k | 29.60× | 0.998× |
| scan256k | 29.62× | 1.002× |
| scan8m | 28.44× | 0.979× |
| chase8m | 1.08× | 1.054× |
| calls | 110.48× | 0.994× |
| callchain | 127.92× | 1.000× |
| float | 5.13× | 1.010× |

| Breakpoint case | uwvm2 (ms) | ROS (ms) | GDB native (ms) |
|---|---:|---:|---:|
| none | 1.521780 | 1.491212 | 0.007391 |
| cold256 | 1.495903 | 1.488792 | 0.007404 |
| disabled256 | 1.491834 | 1.485108 | 0.007408 |
| ignore9999 | 3.102173 | 3.443588 | 935.140357 |
| hits32 | 4.860620 | 5.246005 | 9.530075 |

Breakpoint results are fresh two-repeat real PTY measurements. Visible stops include command/continue transport; ignored hits still encounter real breakpoints. Native C and Wasm ordinary code generation differ. Faster ignored-hit results do not establish GDB no-hit parity.

## Hardware evidence

VTune CLI hardware Hotspots and Microarchitecture Exploration collected sixteen before/after runs on unchanged inputs. Collection starts at a real pre-clock Wasm stop and ends at the actual guest-exit prompt, before explicit quit/LLVM teardown. PMU totals therefore include output/exit and collection-control latency; they are not pure kernel-only counts. Targets use E cores 16–31; the sampling coordinator temporarily uses cpuset 0–31 and restores 16–31. Empty P-core categories are not target evidence.

| Input / analysis | Instructions retired before → after | Reduction |
|---|---:|---:|
| calls/hotspots | 1,003,200,000 → 796,800,000 | 20.57% |
| calls/uarch-exploration | 1,004,594,457 → 796,878,883 | 20.68% |
| branches/uarch-exploration | 68,215,006 → 68,058,288 | 0.23% |
| dispatch/uarch-exploration | 57,149,052 → 57,187,684 | -0.07% |
| scan32k/uarch-exploration | 488,857,915 → 488,825,062 | 0.01% |
| chase8m/uarch-exploration | 325,001,957 → 324,930,204 | 0.02% |
| fft120/hotspots | 2,352,000,000 → 2,332,800,000 | 0.82% |
| fft120/uarch-exploration | 2,352,155,544 → 2,332,179,315 | 0.85% |

| Input / uarch | Clockticks before → after | Reduction |
|---|---:|---:|
| calls | 260,579,656 → 203,328,408 | 21.97% |
| branches | 23,874,579 → 22,672,976 | 5.03% |
| dispatch | 20,525,875 → 20,603,342 | -0.38% |
| scan32k | 101,769,839 → 101,703,742 | 0.06% |
| chase8m | 177,233,320 → 168,522,568 | 4.91% |
| fft120 | 494,188,908 → 488,968,492 | 1.06% |

The actual compiled Linux x86-64 wrappers in both baseline binaries contain two host calls; both qualified candidates contain one. The receipt retains symbol-specific machine disassembly and exact binary hashes. Both candidates retain the final POPFQ/RET sequence; actual first-trap/native register and cancellation behavior is qualified separately by real PTYs.

Remaining named call hotspots are sampling estimates; anonymous JIT samples cannot be assigned to an opcode:

| Function | Sampled CPU seconds | Estimated retired instructions |
|---|---:|---:|
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge | 0.006944 | 74400000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_root_generation_publication_bridge::matches_current_root | 0.005952 | 26400000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet | 0.004960 | 31200000 |
| uwvm2::runtime::lib::checkpoint_current_generation_plan | 0.004960 | 38400000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_abi_bridge | 0.003968 | 33600000 |
| uwvm2::runtime::checkpoint::shadow_ledger::leave | 0.002976 | 36000000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize_compiler_native_data | 0.002976 | 69600000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet(uwvm2::runtime::checkpoint::activation_identity, std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const> const&, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long)::$_0::operator() | 0.002976 | 16800000 |

## Correctness and synchronization

Both release builds passed xmake -r -j3. Each product passed fifteen real PTY groups covering shutdown, probes, memory, typed values, markers/registers/native stepping, cancellation, fast breakpoints, symbols, restart, code generation and numeric SSA. Six native component tests per product retain byte/type/flag comparisons, caller ownership, sticky rejection, cache identity, LLVM IR verification and native MCJIT numeric SSA checks. The sealed-source PTY group executes main → relay → leaf across two actual preloaded modules, checks exact locals in the leaf and both suspended callers, steps to the computed leaf value and repeats three complete reloads. The third run installs and executes generation 2 in the preloaded leaf, rejects stale-generation and active-frame replacements, and verifies exact locals in the replacement and both suspended callers.

The packed-packet test retains 288 exact extent/alignment/producer combinations and eighteen protected-page payload cases, including the empty unreadable span. Source storage is overwritten before inspecting every owned copy. Existing tests retain 22,950 invalid-byte cases, protected flag boundaries, foreign-control-block rejection, signaling NaNs, vectors, references, GC, nested callers and fallback profiles.

The initial primary component suite retained a literal policy-27 expectation and failed that assertion after the intentional policy-28 change. The test now checks version 28 and proves prior-policy 27 cannot reuse the current metadata/path. The failed attempt is retained separately; successful reruns provide the qualified component evidence. No production source changed after the successful release builds for this test repair.

A new local-call PTY fixture checks exact before-call arguments (123, 35), leaf locals (35, 0), suspended caller local 9 and retained prefix 123 with disabled/ignored call probes, a real call breakpoint and full opcode trace. Every mode verifies the final checksum and normal exit. The same fixture also passes against the frozen baseline before candidate qualification.

The first receipt collection failed while selecting a wrapper symbol because a substring matched compiler template instantiations. The collector now resolves the exact five-argument runtime bridge ABI. The failed collection is recorded separately; production code, binaries and all measurements stayed unchanged.

Both products passed 34 caught real Wasm traps and eight memory64 boundary traps with exact pre-trap inputs and terminal execution rejected. Ten unobserved traps exited with expected SIGILL and core dumps disabled. Final memory event counters stayed zero and no execution/profiling workers remain.

The [machine receipt](debug_performance_local_call_demand_20261011.json) binds all 84 source hashes to completed guarded runs and final binary hashes. Receipt SHA-256: `ab921bdb0d1e0a922143d1805b6b40d6861a703d53a7f7b7a21dc53ad723ad0e`. The [sync audit](debug_performance_local_call_demand_20261011_sync.json) compares both local products against their qualified SSH checkouts. Only the current Linux release configuration was executed. macOS/Windows, exception-disabled configurations and other-platform UI behavior were not requalified in this iteration.

## Architecture research

[GDB software breakpoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Breaks.html) replace a selected native instruction with a trap. [LLVM StackMaps](https://llvm.org/docs/StackMaps.html) record specified values at code offsets, and zero-shadow maps need not emit inline instructions. Inference: a Wasm implementation still needs complete typed locals/operand/control/caller/reference maps, authentic loaded-code owners, precise register/spill recovery and safe all-worker patching; merely replacing an instruction does not reconstruct those states.

[Linux static keys](https://docs.kernel.org/staging/static-keys.html) use patchable branch sites to reduce disabled tracepoint checks. [LLVM dynamic debugging](https://llvm.org/docs/DynamicDebugging.html) describes switching optimized and debugging function bodies through detours. Inference: these offer possible future idle-mode reductions, but need loaded-code lifetime/W^X/cache synchronization, active-frame transition and exact stepping support in this runtime. This iteration does not implement jump-label patching, dual bodies or general stackmap breakpoint recovery. It extends demand collection to a compiler-qualified numeric local typed call with unconditional caller preservation, and removes an existing redundant host call. GDB no-hit execution efficiency remains unmet.

## Reproduction

Raw evidence and ignored orchestration remain under /home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/calls-v36-*. Use guard-ssa-v25.py, the final source manifest, successful release build guards and frozen binaries. The immediate baseline is uwvm-binding-v34(-ros); candidates are uwvm-calls-v36(-ros). Both use native Less/local-SSA and host O3. Preserve prior receipts and immutable binaries.
