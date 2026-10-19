# Debug performance: native selection and bounded numeric local SSA (2026-10-11)

Both products passed the Linux qualification below. All builds, native execution, PTY interaction, benchmarks and hardware collection ran on SSH Linux in the actual **64 GiB / zero-swap** cgroup. No local compilation or executable testing was performed. This change reduces debug execution work; general instrumentation-free Wasm breakpoints and GDB performance parity remain unimplemented.

## Implementation

Default Linux x86-64 debug observation now uses LLVM native `CodeGenOptLevel::Less`, favors the no-capture continuation in branch weights, and runs only bounded numeric-local promotion before the existing legalization/verifier. Explicit user optimization choices retain precedence. This does not enable broad CFG simplification, inlining or removal of observation callbacks. Other platforms retain their earlier policy and were not qualified by this run.

A complete numeric prefix of at most eight declared locals is copied inline only after validating its actual observation allocation, owner, layout, extent and typed local slots. Each executed capture still writes fresh initialization flags and all sixteen bytes of every slot, preserving signaling NaN and v128 bits. Large/reference/resumable packets retain the previous bounded copier and lifetime checks.

Only a function admitted by its complete numeric copier can use this SSA path. With at most two actual mutable GP locals, internal entry temporaries also participate; LLVM must prove that every promoted slot is nonescaping and promotable. More than two mutable GP locals triggers a pressure bound: nonconstant i32/i64 slots remain on the stack, while FP/vector/constant slots can still promote. The count uses actual Wasm parameters and nonconstant `local.set/local.tee`; cold ABI/checkpoint restores do not inflate it. Those restores still participate in LLVM's full dataflow proof. The metadata is a compiler optimization hint with no publication, native-address, root or restore authority.

Observation object-cache policy is **27**; resumable policy stays **17**. The cache identity test rejects the previous observation policy. Host runtime compilation remains O3.

## Kernel measurements

Same checksummed Wasm inputs, serial execution, CPU affinity 16–31 (E cores), three alternating paired samples for ordinary/no-hit/cold/disabled cases. Guest CLOCK_MONOTONIC measures kernels after initialization and excludes JIT startup, command entry and formatting. Native C O2/GDB is an independent reference with its own ordinary baseline, not an identical instruction stream. Cold/disabled cases assert zero breakpoint hits.

Median no-breakpoint debug continuation, milliseconds. Before is the immutable previous runtime with observation policy 22 and pipeline/codegen None; after is the final policy 27 candidate.

| Kernel | uwvm2 before → after | Reduction | ROS before → after | Reduction |
|---|---:|---:|---:|---:|
| integer | 3.8280 → 2.3564 | 38.44% | 3.8301 → 2.3646 | 38.26% |
| branches | 7.5758 → 5.0632 | 33.17% | 7.5639 → 5.0492 | 33.25% |
| dispatch | 6.7514 → 4.3660 | 35.33% | 6.7440 → 4.3975 | 34.79% |
| scan32k | 36.2687 → 24.1036 | 33.54% | 36.2695 → 24.1007 | 33.55% |
| scan256k | 36.2917 → 24.1001 | 33.59% | 36.2812 → 24.1038 | 33.56% |
| scan8m | 36.3311 → 24.1169 | 33.62% | 36.3224 → 24.1070 | 33.63% |
| chase8m | 34.9055 → 30.0022 | 14.05% | 35.2606 → 31.7483 | 9.96% |
| calls | 88.1396 → 84.6757 | 3.93% | 87.7821 → 83.6603 | 4.70% |
| callchain | 197.2658 → 186.1407 | 5.64% | 196.6200 → 184.4497 | 6.19% |
| float | 2.8019 → 2.2840 | 18.48% | 2.8016 → 2.2880 | 18.33% |

Pointer-chase timing is sensitive to dependent memory latency; a few samples do not establish a general cache-access improvement. Cold/disabled results and all individual samples remain in the receipt.

### Selection ablation

An earlier unrestricted mem2reg candidate improved the f64 recurrence but regressed integer and sequential-memory kernels by about 6–8% against native Less alone. A second candidate counted all nonconstant GP stores, including cold reloads: its complete primary matrix eliminated that regression but lost the float gain. A subsequent declared-local-only candidate still lost that gain: internal entry temporaries were omitted. Both remaining sampling runs were deliberately stopped; their partial results are excluded from final qualification.

The final five-repeat, alternating Less-only versus bounded-SSA comparison isolates the additional promotion choice (both sides use native Less). The table reports relative kernel time, so positive values are regressions.

| Kernel | uwvm2 final SSA vs Less only | ROS final SSA vs Less only |
|---|---:|---:|
| integer | -0.38% | -0.15% |
| branches | +0.12% | +0.67% |
| dispatch | -0.52% | +0.64% |
| scan32k | +0.07% | -0.05% |
| scan256k | +0.36% | +0.40% |
| scan8m | +0.03% | +0.01% |
| chase8m | -0.13% | +0.91% |
| calls | -0.90% | -1.06% |
| callchain | -0.29% | -0.86% |
| float | -22.47% | -22.60% |

### FFT and complete trace

| Workload | uwvm2 before → after (ms) | ROS before → after (ms) |
|---|---:|---:|
| FFT 2048 × 1 | 1.4882 → 0.9882 | 1.4908 → 0.9875 |
| 300,000 calls, every opcode | 655.7957 → 626.0834 | 646.2005 → 618.3691 |
| 20,000 nested call chains, every opcode | 762.1670 → 737.6898 | 778.7718 → 730.2538 |

FFT debug/ordinary ratios: uwvm2 **13.39×**, uwvm2-ros **13.33×**. Ten small complete-trace variants qualify algorithm/value behavior; their different sizes are not performance evidence. A contiguous retained 512-event window is checked; exact completeness independently checks the unchanged seven-opcode fixture.

### Native stepping and GDB gap

uwvm2: the four exact Wasm transitions require `ni` command counts [61, 31, 53, 84] → [33, 26, 29, 37]. Both retain the same exact opcode sequence. These are debugger next-command counts, not retired CPU instruction counts.

uwvm2-ros: the four exact Wasm transitions require `ni` command counts [61, 31, 53, 84] → [33, 26, 29, 37]. Both retain the same exact opcode sequence. These are debugger next-command counts, not retired CPU instruction counts.

| Kernel | Final debug / own ordinary | GDB native / native ordinary |
|---|---:|---:|
| integer | 5.06× | 1.000× |
| branches | 10.99× | 1.001× |
| dispatch | 2.55× | 1.001× |
| scan32k | 29.69× | 1.005× |
| scan256k | 29.56× | 1.001× |
| scan8m | 29.28× | 1.004× |
| chase8m | 0.95× | 1.003× |
| calls | 192.20× | 1.000× |
| callchain | 225.01× | 1.000× |
| float | 5.13× | 0.988× |

Call-entry/caller-state materialization and per-opcode demand checks still dominate the gap. This result does not establish GDB no-hit efficiency.

### Breakpoint workload

Final candidate: one fresh repeat of all five cases per product (plus ordinary control). Baseline/native/GDB references use the unchanged immutable inputs from the completed three-repeat run on this same boot; they were not rerun here. A 256-function cold/disabled fixture has substantial uncached JIT startup, excluded from the kernel measurements. Hit/ignore cases include real stop/resume handling and command transport.

| Case | uwvm2 final (ms) | ROS final (ms) | GDB/native reference (ms) |
|---|---:|---:|---:|
| none | 2.543086 | 2.491498 | 0.007394 |
| cold256 | 2.541767 | 2.487444 | 0.007392 |
| disabled256 | 2.555867 | 2.501019 | 0.007392 |
| ignore9999 | 4.395767 | 4.380372 | 918.402539 |
| hits32 | 7.624245 | 6.027186 | 10.894739 |

## Hardware evidence

VTune CLI hardware Hotspots and Microarchitecture Exploration: fourteen completed collections covering direct calls, branches, sequential memory, dependent memory and FFT 2048 × 120. Collection starts after real `startw` (the matrix additionally stops exactly before its clock/after initialization), and explicitly stops after genuine guest exit while the debugger remains alive. The subsequent `quit` and any teardown after it are excluded. Guest output/exit (including any cleanup before its prompt) and collection-control latency remain included, so PMU totals are not a pure isolated kernel count.

| Workload / collection | Before retired instructions | After retired instructions | Change |
|---|---:|---:|---:|
| calls / hotspots | 1,368,000,000 | 1,317,600,000 | -3.68% |
| calls / uarch-exploration | 1,368,500,351 | 1,316,550,697 | -3.80% |
| branches / uarch-exploration | 95,112,163 | 68,268,164 | -28.22% |
| scan32k / uarch-exploration | 678,954,149 | 488,768,915 | -28.01% |
| chase8m / uarch-exploration | 461,077,211 | 324,960,334 | -29.52% |
| fft120 / hotspots | 3,199,200,000 | 2,356,800,000 | -26.33% |
| fft120 / uarch-exploration | 3,197,214,358 | 2,356,549,831 | -26.29% |

All targets were verified in the actual memory cgroup and pinned to E cores 16–31. Only the sampler temporarily received CPU set 0–31; it was restored to 16–31. Hybrid reports can contain both core-class metrics; empty P-core rows do not describe these E-core targets. Anonymous JIT samples can appear as unknown/outside known module; they cannot be assigned to a specific generated function from this report alone. Raw summaries, function reports, exact argv, checksums, target identities and boundaries are bound in the receipt.

The final direct-call Hotspots report retains `shadow_ledger::decode_compiler_packet_impl` and packet materialization among its leading named functions. This supports continued work on caller-state capture/validation costs; unknown JIT samples are not assigned to individual opcodes.

## Qualification and retained evidence

Both products passed thirteen real PTY groups: pause/restart/quit and shutdown; observation and numeric-memory probes; exact typed values and control/reference state; pane markers; registers; native stepping/cancellation/fast breakpoints; symbols/layout/help; 32 mixed-mode restarts; exact opcode/native-next comparisons; numeric locals/loop PHIs/callee/suspended caller with exact sNaN/v128 bits.

Five components passed per product, including real native LLVM execution of the production numeric copier/promotion, hot-GP/cold-reload selection, all sixteen bytes/flags/fresh changed values, full packet/ledger/cache identity checks, and 32/64-bit little/big-endian legacy copier IR verification. The unchanged terminal semantics passed 84 real caught trap stops and ten uncaught SIGILL executions. Intentional trap exits are qualified as such, not reported as ordinary success.

Fresh final timing samples: 768, plus 400 selection-ablation samples. Earlier breakpoint references are explicitly marked as reused. All required guards completed with exit 0; source/binary hashes bind their receipts. Cgroup peak across the retained session was 11.82 GiB; OOM/max/high events stayed zero and actual test/build/profiler workers retired.

Mac and Windows/QEMU were not qualified by this Linux-only run. The previous host-reboot-interrupted native-selection timings, initial CPU-topology setup rejection, and both abandoned broad-cold-store/declared-only candidates remain recorded separately and do not count as final passes.

Machine receipt: [debug_performance_ssa_20261011.json](debug_performance_ssa_20261011.json). The receipt includes all paired timings, reusable reference identities, hardware metrics, complete guard exits, source/binary/fixture hashes, compiler flags and final resources. [Repository synchronization audit](debug_performance_ssa_20261011_sync.json) binds both local checkouts to the tested Linux copies. Linux raw evidence remains under `/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/ssa-v28-*`; earlier candidate data were retained.

## Architecture research and next boundary

[LLVM frontend guidance](https://llvm.org/docs/Frontend/PerformanceTips.html) explains entry-alloca promotion to SSA. Our bounded policy adds an empirically tested GP-pressure decision rather than indiscriminate promotion.

[GDB fast tracepoints](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Tracepoints.html) can insert jumps on supported targets, with location restrictions. [Intel architectural manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html) describe native breakpoint/debug exceptions. Native trapping alone does not reconstruct the complete typed Wasm locals, operand/control stacks, caller phases and GC roots required here.

[LLVM stackmaps/patchpoints](https://llvm.org/docs/StackMaps.html) can record explicit live-value locations and reserve patchable regions. A zero-shadow stackmap emits no inline code; keeping complete values live can still affect register pressure (inference). A patchpoint requires replacing the entire reserved region. A future general engine needs authenticated loaded JIT site ownership, complete typed/control/root mappings, parked workers, W^X/cache coherency and correct breakpoint resume/rearm. None of that is established by this local-SSA change.

[Linux uprobes](https://www.kernel.org/doc/html/latest/trace/uprobetracer.html) use executable/library file paths and offsets; this interface does not directly supply a complete anonymous-JIT Wasm state map. [Intel hardware collect](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/collect.html) and [collection stop](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/command.html) document the CLI used here.

## Reproduction

Use the same guarded Linux configuration and verified E-core topology. Build each repository serially with `xmake -r -j3`. The receipt stores exact commands for all component/interaction groups and the final coordinator scripts (`ssa-v28-builds.py`, `ssa-v28-final-checks.py`, `ssa-v28-timings.py`, `ssa-v28-hardware-root.py`, `ssa-v28-ablation.py`).

The repository drivers are `run_debug_workload_matrix.py`, `run_debug_codegen_pty.py`, `run_debug_numeric_local_ssa_pty.py`, `run_debug_fft_benchmark.py`, `run_debug_breakpoint_benchmark.py` and `run_debug_vtune_workload.py`. Generate full and tiny checked fixtures with `generate_debug_workloads.py`. Supply an immutable baseline binary and unique output directories; add `--ros` for ROS timings. Hardware expected policies are baseline `none/none` and final `debug-local-ssa/less`.
