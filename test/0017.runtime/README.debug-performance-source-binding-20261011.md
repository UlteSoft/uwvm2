# Debug performance: actual sealed source binding (2026-10-11)

This follow-up removes repeated display-name hashing from actual frame entry validation and eliminates an extra retained-plan reference-count copy. The baseline is the immediately preceding qualified short-flag runtime. All executable work ran on SSH Linux in the actual 64 GiB/no-swap cgroup. No local compilation or executable tests were performed.

## Change and retained guarantees

The canonical source owner seals the real main/preload registry members after actual initialization. Full compilation binds those actual module addresses to dense IDs and validation epochs. On every current TLS activation, the entry hook delegates source binding authentication to its existing current-generation plan lookup. The root lookup already checks the canonical owner, initialized/validated module/file binding and actual epoch. A committed replacement separately checks that canonical binding before its retained engine/context/source/generation/entry owners are admitted. Configurations without root identities also explicitly check canonical source binding. This removes a redundant name lookup without adding another linear scan for preloaded modules. Canonical control-block ownership and initialized/validated state are still required; no public ID, address or cached boolean grants execution authority. Current function generation, publication, profile, native leaf and full typed packet checks remain unchanged. The already validated plan owner moves into its destination rather than incrementing/decrementing an additional shared_ptr copy.

This changes host entry validation and ownership transfer only. Packet copying remains the previously qualified implementation. JIT observation IR, runtime input ABI, cache policy 27/17, actual publication/activation validation and native JIT code boundaries are unchanged. It does not implement a general instrumentation-free Wasm breakpoint engine. Both repositories contain the same incremental change; their existing product-specific runtime differences remain intact.

## Rejected experiment

A constant-size short-packet slot-copy candidate passed correctness qualification, but the three-repeat paired call-only trial slowed both products by approximately 0.4%–2%. That source change was reverted, its broader timing run was deliberately interrupted, and its binary is not the final candidate or baseline. The new exact extent/alignment/protected-page tests were retained. The rejected trial and interrupted guard are recorded in the final receipt; an interrupted run is not counted as a pass.

## Paired kernel measurements

Each unchanged checksummed workload runs three alternating before/after repetitions in ordinary, no-hit, cold-breakpoint and disabled-breakpoint modes. Native C/GDB uses an independent equivalent algorithm and its own ordinary baseline. Guest monotonic kernel timing excludes JIT initialization and breakpoint installation. Every raw sample and exact input/binary hash is retained in the receipt, including an earlier 48-sample call-only diagnostic. The reported main matrix uses a separate subsequent run.

| Kernel | uwvm2 before → after (ms) | Reduction | ROS before → after (ms) | Reduction |
|---|---:|---:|---:|---:|
| integer | 2.35998 → 2.35555 | 0.19% | 2.35628 → 2.35674 | -0.02% |
| branches | 5.06984 → 5.05731 | 0.25% | 5.07457 → 5.06794 | 0.13% |
| dispatch | 4.41058 → 4.36597 | 1.01% | 4.37663 → 4.42183 | -1.03% |
| scan32k | 24.10096 → 24.11268 | -0.05% | 24.09850 → 24.10334 | -0.02% |
| scan256k | 24.10590 → 24.09809 | 0.03% | 24.09884 → 24.09913 | -0.00% |
| scan8m | 24.10064 → 24.11885 | -0.08% | 24.10753 → 24.13986 | -0.13% |
| chase8m | 29.75574 → 31.72680 | -6.62% | 31.85671 → 31.81636 | 0.13% |
| calls | 72.80657 → 63.02392 | 13.44% | 72.20842 → 62.92928 | 12.85% |
| callchain | 157.00666 → 135.10285 | 13.95% | 156.27160 → 136.00431 | 12.97% |
| float | 2.28424 → 2.28228 | 0.09% | 2.28207 → 2.28366 | -0.07% |

Negative reductions indicate a slowdown and are not counted as improvements. Kernel timings are medians, not claims of statistical significance. The prior iteration had a product-specific dispatch difference, so dispatch and dependent pointer chase receive an additional serial ten-repeat check pinned to CPU 16:

| Kernel / product | Fixed-CPU before → after (ms) | Reduction |
|---|---:|---:|
| dispatch / uwvm2 | 4.41964 → 4.37751 | 0.95% |
| chase8m / uwvm2 | 30.18860 → 30.21340 | -0.08% |
| dispatch / uwvm2-ros | 4.37114 → 4.42232 | -1.17% |
| chase8m / uwvm2-ros | 30.16665 → 30.15512 | 0.04% |

The fixed-CPU pointer-chase medians differ by less than 0.1%. The uwvm2 dispatch result improves by about 1%, while ROS dispatch remains about 1.2% slower. The cause of the opposing product-specific dispatch differences is not established; the ROS slowdown is not dismissed as noise or counted as an improvement.


### Complete trace and FFT

| Product | Calls complete trace before → after (ms) | Call chain complete trace before → after (ms) | FFT debug before → after (ms) | FFT after debug / ordinary |
|---|---:|---:|---:|---:|
| uwvm2 | 555.854 → 542.697 | 646.829 → 622.549 | 0.992769 → 0.987111 | 13.37× |
| uwvm2-ros | 546.821 → 540.196 | 642.370 → 617.947 | 0.987451 → 0.992412 | 13.36× |

Trace timings use two paired full-sized repetitions and a contiguous 512-event retained window; exact opcode completeness is independently checked by the seven-opcode PTY fixture. FFT uses the same checked 2048-point Wasm input and five repetitions per mode. Its runner groups modes rather than alternating them.

### GDB reference and breakpoint costs

| Kernel | uwvm2 debug / own ordinary | GDB / native ordinary |
|---|---:|---:|---:|
| integer | 5.11× | 0.993× |
| branches | 10.98× | 0.993× |
| dispatch | 2.55× | 1.000× |
| scan32k | 29.64× | 0.997× |
| scan256k | 29.78× | 1.000× |
| scan8m | 29.37× | 0.991× |
| chase8m | 1.07× | 1.029× |
| calls | 142.51× | 1.000× |
| callchain | 163.23× | 1.011× |
| float | 5.13× | 0.993× |

| Breakpoint case | uwvm2 (ms) | ROS (ms) | GDB native (ms) |
|---|---:|---:|---:|
| none | 1.921851 | 1.928612 | 0.007399 |
| cold256 | 1.926467 | 1.942646 | 0.007392 |
| disabled256 | 1.922639 | 1.937086 | 0.007390 |
| ignore9999 | 3.317479 | 3.456576 | 952.118175 |
| hits32 | 5.389290 | 5.614095 | 9.904865 |

Breakpoint results are fresh two-repeat real PTY measurements. Visible stops include command/continue transport; ignored hits still encounter real breakpoints. Native C and Wasm ordinary code generation differ. Faster ignored-hit results do not establish GDB no-hit parity.

## Hardware evidence

VTune CLI hardware Hotspots and Microarchitecture Exploration collected sixteen before/after runs on unchanged inputs. Collection starts at a real pre-clock Wasm stop and ends at the actual guest-exit prompt, before explicit quit/LLVM teardown. PMU totals therefore include output/exit and collection-control latency; they are not pure kernel-only counts. Targets use E cores 16–31; the sampling coordinator temporarily uses cpuset 0–31 and restores 16–31. Empty P-core categories are not target evidence.

| Input / analysis | Instructions retired before → after | Reduction |
|---|---:|---:|
| calls/hotspots | 1,068,000,000 → 1,003,200,000 | 6.07% |
| calls/uarch-exploration | 1,068,285,627 → 1,004,697,827 | 5.95% |
| branches/uarch-exploration | 68,178,272 → 68,191,753 | -0.02% |
| dispatch/uarch-exploration | 57,241,697 → 57,224,894 | 0.03% |
| scan32k/uarch-exploration | 488,806,286 → 488,832,171 | -0.01% |
| chase8m/uarch-exploration | 324,950,923 → 324,996,136 | -0.01% |
| fft120/hotspots | 2,354,400,000 → 2,352,000,000 | 0.10% |
| fft120/uarch-exploration | 2,353,125,243 → 2,352,115,042 | 0.04% |

Remaining named call hotspots are sampling estimates; anonymous JIT samples cannot be assigned to an opcode:

| Function | Sampled CPU seconds | Estimated retired instructions |
|---|---:|---:|
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge | 0.011905 | 64800000 |
| uwvm2_native_step_safepoint_impl | 0.011905 | 91200000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize_compiler_native_data | 0.006944 | 81600000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet(uwvm2::runtime::checkpoint::activation_identity, std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const> const&, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long)::$_0::operator() | 0.004960 | 24000000 |
| __memmove_avx_unaligned_erms | 0.003968 | 19200000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_root_generation_publication_bridge::matches_row | 0.003968 | 40800000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_actual_frame | 0.002976 | 28800000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_root_generation_publication_bridge::matches_current_root | 0.002976 | 48000000 |

## Correctness and synchronization

Both release builds passed xmake -r -j3. Each product passed fourteen real PTY groups covering shutdown, probes, memory, typed values, markers/registers/native stepping, cancellation, fast breakpoints, symbols, restart, code generation and numeric SSA. Six native component tests per product retain byte/type/flag comparisons, caller ownership, sticky rejection, cache identity, LLVM IR verification and native MCJIT numeric SSA checks. The new sealed-source PTY group executes main → relay → leaf across two actual preloaded modules, checks exact locals in the leaf and both suspended callers, steps to the computed leaf value and repeats three complete reloads. The third run installs and executes generation 2 in the preloaded leaf, rejects stale-generation and active-frame replacements, and verifies exact locals in the replacement and both suspended callers.

The packed-packet test adds 288 exact extent/alignment/producer combinations and eighteen protected-page payload cases, including the empty unreadable span. Source storage is overwritten before inspecting every owned copy. Existing tests retain 22,950 invalid-byte cases, protected flag boundaries, foreign-control-block rejection, signaling NaNs, vectors, references, GC, nested callers and fallback profiles.

Both products passed 34 caught real Wasm traps and eight memory64 boundary traps with exact pre-trap inputs and terminal execution rejected. Ten unobserved traps exited with expected SIGILL and core dumps disabled. Final memory event counters stayed zero and no execution/profiling workers remain.

The [machine receipt](debug_performance_source_binding_20261011.json) binds all 78 source hashes to completed guarded runs and final binary hashes. Receipt SHA-256: `8d342c6a8fe4f7dc5e293f4e1f5c97da053dd8a872010f07453c1ea77374185a`. The [sync audit](debug_performance_source_binding_20261011_sync.json) compares both local products against their qualified SSH checkouts. Only the current Linux release configuration was executed. macOS/Windows, exception-disabled configurations and other-platform UI behavior were not requalified in this iteration.

## Architecture research

[GDB software breakpoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Breaks.html) replace a selected program instruction and restore it while stopped. [GDB displaced stepping](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Maintenance-Commands.html) can execute an out-of-line copy to step over a breakpoint without removing the trap.

[LLVM stack maps](https://llvm.org/docs/StackMaps.html) record selected live values in registers, stack slots or constants; a zero-shadow map need not emit inline instructions. Patchpoints reserve replacement space. Inference: these mechanisms could avoid repeated Wasm value copies, but the runtime still needs authenticated loaded-code ownership, accurate caller/control/reference/initialization maps, exact native register/spill recovery, and all-worker patching with W^X/cache-coherence rules. Keeping all values live may also increase spills.

[LLVM dynamic debugging](https://llvm.org/docs/DynamicDebugging.html) describes optimized/unoptimized function bodies with runtime detours. Inference: function-entry detours could help inactive execution, but do not alone solve already-active Wasm frames or exact all-opcode stepping. This iteration implements actual entry binding and ownership transfer, not those architectures. Native temporary breakpoints already present in this project are not a full Wasm state reconstruction engine. GDB execution efficiency remains unmet.

## Reproduction

Raw evidence and ignored orchestration remain under /home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/binding-v34-*. Use guard-ssa-v25.py, the final source manifest, successful release build guards and frozen binaries. The immediate baseline is uwvm-short-v31(-ros); candidates are uwvm-binding-v34(-ros). Both use native Less/local-SSA and host O3. Preserve prior receipts and immutable binaries.
