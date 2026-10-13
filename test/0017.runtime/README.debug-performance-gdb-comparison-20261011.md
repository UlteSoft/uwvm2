# Current debugger versus GDB: fresh sampling (2026-10-11)

Current immutable numeric-v44 binaries are measured without another production change. All compilation of profiling fixtures, executable measurements and VTune collections run only on SSH Linux in the actual 64 GiB / zero-swap cgroup. No local runtime or build is started.

## Method

Original workload timings are pinned to CPU 16, serial, with no profiler or competing builds. Ten unchanged checked inputs cover integer/floating-point arithmetic, branches, dispatch, cache scans, dependent memory access, direct calls and nested calls. Main uses seven alternating repetitions of ordinary / no-breakpoint / cold / disabled modes, plus native ordinary and a real PTY GDB run. ROS uses five repetitions of its own four modes. Kernel CLOCK_MONOTONIC excludes launch, JIT compilation, breakpoint installation and final output. All samples, including variation, remain in the JSON.

Each debugger is normalized to its own ordinary backend. Native C (O2 with debug information), ordinary Wasm JIT and debug Wasm Less/local-SSA generate different machine code; an absolute C-versus-Wasm time ratio is not solely debugger overhead. Small GDB ratios below one are measurement/code-layout/frequency variation, not evidence that GDB accelerates the kernel.

## No breakpoint hit

| Input | Main ordinary → debug (ms) | Main factor | ROS factor | GDB / own native baseline |
|---|---:|---:|---:|---:|
| integer | 0.460724 → 2.357186 | 5.12× | 5.11× | 0.977× |
| float | 0.444311 → 2.282716 | 5.14× | 5.15× | 0.974× |
| branches | 0.460563 → 5.080020 | 11.03× | 10.70× | 0.981× |
| dispatch | 1.714385 → 4.388614 | 2.56× | 2.57× | 0.993× |
| scan32k | 0.812809 → 24.116293 | 29.67× | 29.66× | 0.991× |
| scan256k | 0.813931 → 24.120365 | 29.63× | 29.64× | 0.991× |
| scan8m | 0.825077 → 24.121753 | 29.24× | 29.23× | 1.001× |
| chase8m | 29.677940 → 30.145574 | 1.02× | 1.02× | 1.030× |
| calls | 0.443895 → 45.128108 | 101.66× | 102.31× | 0.975× |
| callchain | 0.827442 → 96.893746 | 117.10× | 116.12× | 0.967× |

GDB is close to ordinary native execution when no breakpoint is encountered. Current -Rdbg remains much slower on calls and cache scans; dependent-memory stalls can hide its overhead.

| Input | Main no breakpoint (ms) | Main cold breakpoint (ms) | Main disabled breakpoint (ms) | No-breakpoint sample range (ms) |
|---|---:|---:|---:|---:|
| calls | 45.128108 | 45.192526 | 45.177096 | 45.021901–45.308900 |
| callchain | 96.893746 | 96.762865 | 96.790686 | 96.374550–96.963011 |
| scan32k | 24.116293 | 24.125564 | 24.119670 | 24.110815–24.135261 |
| chase8m | 30.145574 | 30.179487 | 30.160428 | 30.076599–30.238583 |

### Longer GDB calibration

Native direct-call and chase kernels are repeated 1000 and 30 times respectively for longer measurements. Seven alternating ordinary / GDB pairs are collected before any profiler starts, with profile gates disabled and CPU 16 fixed. These results are separate from the original input table.

| Extended input | Native ordinary (ms) | GDB inferior (ms) | GDB factor |
|---|---:|---:|---:|
| calls | 387.315737 | 387.291154 | 0.9999× |
| chase8m | 865.034438 | 865.158546 | 1.0001× |

## FFT

The same 2048-point forward/inverse FFT algorithm uses five samples per mode. Wasm and native floating-point checksums agree within 1e-12, not necessarily byte-for-byte. This runner groups modes, unlike the alternating matrix.

| Backend | Ordinary median (ms) | Debugger median (ms) | Own-baseline factor |
|---|---:|---:|---:|
| uwvm2 | 0.073708 | 0.980754 | 13.31× |
| uwvm2-ros | 0.074273 | 0.978215 | 13.17× |
| GDB / native | 0.046088 | 0.047214 | 1.024× |

## Breakpoint cost

Three repeats per case use the same original breakpoint fixture algorithm and real PTYs. Each cold/disabled case installs 256 genuine breakpoints before the clock. Ignored hits encounter the breakpoint 10000 times, with 9999 ignored; the last hit is visible. The 32-hit case includes actual command/prompt/continue round trips and the final disable operation. Results are not an isolated CPU trap latency.

| Case | Main (ms) | ROS (ms) | GDB/native (ms) |
|---|---:|---:|---:|
| none | 1.383607 | 1.375649 | 0.007407 |
| cold256 | 1.379967 | 1.369749 | 0.007408 |
| disabled256 | 1.380344 | 1.374277 | 0.007405 |
| ignore9999 | 2.489971 | 2.555703 | 1146.957562 |
| hits32 | 5.520477 | 5.473483 | 10.738241 |

### Complete breakpoint test sessions

These medians use the full process/test session clock, including launch, compilation, setup commands, execution and quit. They are not pure JIT startup times. Native ordinary is a subprocess; debug modes include real console transport. Breakpoint installation is included here, although excluded from the kernel table above.

| Case | Main complete session (s) | ROS complete session (s) | GDB complete session (s) |
|---|---:|---:|---:|
| none | 19.522224 | 19.500705 | 0.271025 |
| cold256 | 19.557337 | 19.523859 | 0.360677 |
| disabled256 | 19.544059 | 19.539937 | 0.365755 |

Breakpoint-fixture ordinary medians: Wasm 0.010652 ms; native 0.007390 ms. Ignoring a GDB software breakpoint still causes real traps; lower current uwvm ignored-hit cost does not establish GDB no-hit efficiency.

## VTune hardware sampling

Twelve fresh hardware Hotspots / Microarchitecture Exploration collections cover repeated calls and dependent memory access, each under ordinary native, GDB inferior and current main -Rdbg. Repetition length makes native PMU measurements substantial. Native ordinary and GDB run the same binary. The native fixture stops with SIGSTOP before its clock and after reporting results; the GDB case samples the actual inferior PID, never the GDB controller. Wasm starts collection at a genuine pre-clock breakpoint after fixture initialization, disables that breakpoint and stops collection after guest exit while the debugger is alive. JIT startup and LLVM teardown are excluded. The PMU window still includes resume/control, result output and exit or the final native signal stop.

Targets remain on E cores 16–31. VTune needs its sampler cpuset temporarily expanded to 0–31; it is restored to 16–31 in a finally block. Commands follow Intel's [VTune CLI control guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/command.html): hardware collection, attach to the exact target PID, then `-command stop -r RESULT`. Hotspots uses hardware sampling with stacks; Microarchitecture uses PMU summary counting.

| Input | Mode | Retired instructions | Clockticks | CPI | MUX reliability |
|---|---|---:|---:|---:|
| calls | native-ordinary | 3002713842 | 1585112753 | 0.527893 | 0.999222 |
| calls | gdb-inferior | 3002831270 | 1585000699 | 0.527835 | 0.999868 |
| calls | uwvm2 | 784572367541 | 185157075469 | 0.235997 | 0.999807 |
| chase8m | native-ordinary | 162415188 | 3822004235 | 23.532308 | 0.999983 |
| chase8m | gdb-inferior | 162509404 | 3825097276 | 23.537698 | 0.999879 |
| chase8m | uwvm2 | 9728724969 | 3897778126 | 0.400646 | 0.997643 |

Hardware-profiled kernel times are retained in the JSON but not mixed into the unprofiled slowdown table. Stack sampling can noticeably lengthen the heavily instrumented call kernel. These cross-backend raw counts include code-generation differences. The ordinary-native versus GDB-inferior comparison has identical code; the Wasm-versus-C counts must not be presented as an isolated debugger multiplier.

Current main direct-call Hotspots (CPU time is a sampling estimate; anonymous JIT addresses lack opcode attribution):

| Function | CPU seconds | Estimated retired instructions |
|---|---:|---:|
| [Outside any known module] | 8.679563 | 106615200000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge | 6.148809 | 60720000000 |
| uwvm2::runtime::checkpoint::shadow_ledger::materialize_compiler_native_data | 3.945436 | 58041600000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_abi_bridge | 3.708333 | 38328000000 |
| uwvm2::runtime::lib::checkpoint_current_generation_plan | 3.441468 | 32205600000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet(uwvm2::runtime::checkpoint::activation_identity, std::__1::shared_ptr<uwvm2::runtime::checkpoint::sealed_function_plan const> const&, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long)::$_0::operator() | 3.234127 | 15962400000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_materialize_actual_packet | 3.099206 | 46010400000 |
| uwvm2::runtime::lib::details::llvm_jit_checkpoint_activation_leave_abi_bridge | 2.958333 | 29436000000 |
| uwvm2::runtime::checkpoint::shadow_ledger::leave | 2.871032 | 20200800000 |
| uwvm2::runtime::lib::(anonymous namespace)::checkpoint_root_generation_publication_bridge::matches_row | 2.519841 | 39026400000 |

## Evidence and limits

The [machine receipt](debug_performance_gdb_comparison_20261011.json) retains all 732 unprofiled timing samples, twelve hardware receipts, parsed metrics/hotspots, source and immutable binary hashes, actual cgroup/affinity, tool versions and completed guard metadata. The [four-copy sync audit](debug_performance_gdb_comparison_20261011_sync.json) checks both products locally and on SSH Linux. Hardware raw results and all transcripts remain under `/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/gdb-comparison-v45-*`. Memory events are all zero and no execution/profile workers remain.

No production runtime changes or new full correctness/platform qualification occur in this measurement task. macOS and Windows/QEMU are not executed. GDB no-hit efficiency is still unmet. The ordinary GDB comparison does not promise the same Wasm operand/control/caller snapshots that current -Rdbg retains. The named snapshot/materialization hotspots and the much larger call-heavy slowdown identify continued execution-path overhead; they do not justify skipping required state or treating GDB ignored-hit latency as evidence of general parity.
