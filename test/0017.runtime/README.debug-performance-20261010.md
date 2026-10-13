# Debugger performance qualification — 2026-10-10

The changes preserve per-opcode value capture, breakpoint policy and JIT-only
native execution. They do not claim GDB-equivalent uninstrumented throughput.

## Changes

- Source and Wasm breakpoints use a fixed-size candidate hash index. Exact keys,
  enable/disable, hit/ignore counts, conditions, temporary points and their
  original slot ordering are still checked under the controller mutex.
- The compiler checkpoint hook borrows the sealed plan retained by the actual
  activation. Entry still validates canonical publication and ownership. Each
  opcode still verifies the live TLS activation, epoch and generation and copies
  all typed value bits. It avoids another registry walk and shared-owner copy.
- ASM continue proves a bounded straight-line prefix of the current immutable
  JIT owner, ending before control transfers, stack/frame/link/TLS writes or the
  nearest enabled breakpoint. macOS uses an execution breakpoint at the endpoint.
  Linux retains TF within the proved prefix and services intermediate traps on
  the worker, avoiding manager round trips and repeated MC/owner reconstruction.
  Both stop on the actual kernel PC and preserve cancellation and owner checks.
- Linux native call continuations account for the uniform `RET imm16` cleanup
  emitted by LLVM fastcc. The adjustment comes from forward-decoded copies of
  the actual published typed callee, never a user-supplied stack address. Opaque
  SysV calls and straight-line runs retain zero adjustment. Unknown or conflicting
  return cleanup refuses the native continuation.
- AArch64 MC now explicitly rejects unaligned instruction PCs even when LLVM's
  generic assembler metadata reports a weaker alignment.

Linux execution-event creation/retirement cost more than two ordinary steps in
our initial experiment (about 21 ms versus 1.66 ms). Therefore the Linux linear
path uses worker-side TF batching instead of creating a perf descriptor for every
small interval. Actual call/return continuations retain their existing sealed
hardware event path. Stop IDs count published manager stops, not every TF signal.

## Controlled execution measurements

Linux: i9-14900HX, target affinity CPUs 16–31 (E cores), existing Docker cgroup
with `memory.max=68719476736`, `memory.swap.max=0`, O3 release/no-LTO host build.
No profiler or compiler was running during the following timing samples.
Each row is the median of three repetitions, except the five-run GDB FFT rows.
Kernel timing excludes debugger startup and breakpoint installation. Visible-hit
cases include the PTY continue/stop round trips within the guest's timer.

| Test | Before | After | Comparison |
|---|---:|---:|---:|
| Same FFT Wasm, ordinary JIT | 0.073781 ms | 0.073909 ms | baseline |
| Same FFT Wasm, `-Rdbg` | 954.412370 ms | 606.079093 ms | 1.57× faster |
| 10,000 calls, dbg without breakpoints | 36.028573 ms | 11.507759 ms | 3.13× faster |
| 256 unhit Wasm breakpoints | 75.136263 ms | 11.795830 ms | 6.37× faster |
| 256 disabled targets | 65.395244 ms | 11.547374 ms | 5.66× faster |
| 9,999 ignored hits, one visible hit | 38.169962 ms | 13.236270 ms | 2.88× faster |
| 32 visible breakpoint hits | 39.630621 ms | 15.578631 ms | 2.54× faster |

The breakpoint fixture uses numeric Wasm locations versus equivalent native C
function symbols in GDB. Both execute the same source-level hot/cold function
workload and check its checksum. They have separate native/Wasm baselines.

| GDB 16.3 comparison | Ordinary native | GDB |
|---|---:|---:|
| FFT, same C++ source | 0.045868 ms | 0.045742 ms |
| 10,000 calls, no breakpoints | 0.007408 ms | 0.007401 ms |
| 256 unhit breakpoints | — | 0.007405 ms |
| 9,999 ignored hits, one visible hit | — | 913.881837 ms |
| 32 visible breakpoint hits | — | 11.273896 ms |

GDB's native execution runs without our per-Wasm-opcode materialization. Its
unhit breakpoints therefore have almost no execution overhead. The ignored-hit
comparison measures a different cost: GDB receives native breakpoint traps,
whereas uwvm evaluates hit/ignore policy in process. It does **not** establish a
general speed advantage over GDB. The FFT remains roughly 8,200× its ordinary
Wasm kernel baseline in this short, compute-heavy fixture; startup is separate.

macOS Apple M4, existing O0 host test configuration, three repetitions: the same
Wasm FFT `-Rdbg` median fell from 3131.538 ms to 1429.808 ms. This is a comparison
within the local test build, not an optimized-release or cross-platform ranking.

## Final binary checks

Both repositories compiled on macOS and Linux. The final Linux primary FFT
median is **606.082414 ms** (ordinary **0.074156 ms**); the ROS build is
**614.365997 ms** (ordinary **0.074282 ms**), three repetitions each. This
confirms the later native stepping changes retained the checkpoint/index gain.
For the short two-instruction native breakpoint interval, five alternating PTY
runs measured **2.055165 ms before / 1.819067 ms after**. This small interaction
latency measurement includes terminal and scheduling overhead; it does not claim
the same gain for all machine instructions. Each run verifies both ordered
breakpoints and exact PCs. A separate RX backend test verifies 254 instructions
with one published revision and 32 cancellation races.

The final Linux builds passed native stepping, breakpoint intervals, all nine
value-capture fixtures, 14 three-level marker checks, register panes and compact
symbol/help/layout tests in both repositories. The long-loop physical finish
and Ctrl+C test passed separately with `--finite-return`. Final macOS native
interaction, breakpoint intervals and cancellation passed in both repositories;
the earlier full matrix also passed. The qualified primary macOS binary was
installed at `~/.local/bin/uwvm` with its hash recorded in the receipt.

## Intel VTune hardware observations

Used the installed official CLI, attaching after `startw` and then continuing a
longer repetition of the same FFT. The target remained running for the full
15-second collection. No compiler ran during the retained controlled profiles.
The cgroup's CPU eligibility was temporarily expanded for VTune's per-CPU sampler;
the target's explicit affinity remained 16–31, and the 64 GiB/swap-zero limits
remained in force. The original cgroup CPU eligibility is restored after profiling.

```sh
vtune -collect hotspots -knob sampling-mode=hw \
  -knob enable-stack-collection=true -duration 15 \
  -result-dir RESULT -target-pid PID
vtune -collect uarch-exploration -knob pmu-collection-mode=summary \
  -duration 15 -result-dir RESULT -target-pid PID
vtune -report hotspots -r RESULT -format csv -group-by function -limit 40
vtune -report summary -r RESULT -format csv
```

The breakpoint controller's sampled CPU time fell from 2.426 s to 0.271 s per
15-second window. Repeated canonical-plan lookup and shared-owner operations
also disappeared from the leading hot functions. Since the faster binary does
more guest work in a fixed-duration sample, these are hotspot shares, not
per-FFT instruction counts.

The remaining leading host function is `decode_compiler_packet` (4.551 s of the
15-second optimized sample). Approximately 7.447 s is unresolved JIT code; it is
not silently attributed to a named Wasm function. The E-core microarchitecture
report shows CPI 0.342, 178.786 billion retired instructions, multiplex reliability
0.999986, front-end bound 7.7%, bad speculation 2.1%, back-end bound 31.0%, and
store bound 21.9%. P-core top-down rows have no target work and are not interpreted.
These observations point to continuing snapshot/code-generation work rather than
only optimizing breakpoint lookup. Complete state is still materialized for every
opcode; this change introduces no sampling, omitted locals or stale operands.

Official Intel references:
[collect](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/collect.html),
[microarchitecture CLI tutorial](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-2/run-general-exploration-analysis-command-line.html).

## Reproduction and evidence

- `run_debug_fft_benchmark.py`: same Wasm, ordinary/debug, checksum and separate
  startup/kernel measurements.
- `run_debug_gdb_fft.py`: actual GDB PTY, same native FFT source.
- `run_debug_breakpoint_benchmark.py`: no/cold/disabled/ignored/visible breakpoints
  through real debugger command submissions, including exact hit counts.
- `run_debug_vtune.py`: cgroup-checked hardware collection and retained reports.
- `run_debug_native_fast_breakpoint.py`: use
  `fixtures/debug_native_fast_breakpoint.wat` compiled with wasm-tools. Two ordered
  native breakpoints, exact PC, shorter published stop sequence, forbidden targets
  and native-to-Wasm handoff. `--baseline` records the old per-instruction behavior.
- `debug_breakpoint_index.cc`: 1,000 randomized tables compared to the ordered
  linear oracle, including collisions and duplicate keys.
- `native_owned_instruction_semantics.cc`: real LLVM MC ordinary/branch/call,
  implicit/explicit SP/frame writes, stack reads and alignment checks.
- `native_step_linux_x86_64.cc`: 254 real RX instructions with one manager
  revision, exact endpoint/counter and 32 races cancelling a linear interval.
- `run_debug_native_macos_cancel.py --finite-return`: physical Linux finish
  timeout and Ctrl+C with a 2**32-iteration conditional loop that retains an
  actual return instruction. The original unconditionally infinite fixture is
  retained for macOS; Linux refuses its unavailable native unwind continuation
  without moving the stop. This is recorded separately, not counted as a passing
  Linux finish-timeout test.

The [machine-readable receipt](debug_performance_20261010.json) records binary
hashes, test results, profile commands and artifact locations. Native Windows/QEMU has not been rerun for this
performance change; successful macOS/Linux tests do not qualify it.
