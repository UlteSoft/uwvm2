# Effective byte dispatch — 2026-10-10

This continues the policy 21 numeric observation qualification. Observation
code-generation policy is **22**, with resumable policy still 17. Both products
built with xmake -j3 on SSH Linux in the existing 64 GiB, zero-swap cgroup. No
local executable testing was performed. These results do not qualify macOS or
Windows/QEMU.

## Implementation

An eligible numeric observation gate now loads one real atomic uint8_t and
branches directly on its value. The preceding gate loaded a bitmap word,
masked its selected bit, loaded the actual shutdown flag separately, and
combined the results. The controller still constructs the same 4096-bit demand
bitmap; command publication expands it into 4096 distinct atomic byte objects.
There are no mixed-size aliasing atomic accesses. Hash collisions remain
conservative and every actual breakpoint uses its exact identity comparison.

Managed shutdown first authenticates the actual runtime, control owner,
maintenance transition and pinned code generations. It then latches full
collection under the demand publisher mutex before publishing the actual
shutdown request or waking guests. Current and retired selective publishers
cannot clear that latch. Only actual quiescent configuration resets it; null
configuration leaves every byte enabled and refuses retired publishers. The
hint never supplies shutdown/unwind, root, register or executable resume
authority. The original cold shutdown poll still authenticates the request.

Every publication invalidates the previous full-collection token before any
byte change. An odd token appears only after all bytes are enabled. Existing
activation-private packet reuse therefore retains its original continuous-full
publication requirement. Cold full typed packets, control/reference validation,
catchpoints and every requested trace opcode remain. Ordinary non-debug JIT
receives no observation gate. Unsupported targets retain full collection.

## Qualification

Both binaries passed twelve real PTY groups: new infinite-loop managed shutdown,
numeric demand/policies, memory freshness, typed values/references/control,
pane markers, registers, Linux native stepping, finite-return cancellation,
bounded native breakpoints, symbols/help/layout, restart and exact opcode trace.
The new test performs six restarts of a genuinely running numeric loop without
breakpoints, checks nonzero actual counters after pauses, and quits the final
still-running loop with a finite real guest join and restored terminal.

Four assert-enabled component checks passed. The dispatch test checks every
byte against the bitmap, retired publishers, concurrent current publishers
around forced shutdown, fresh configuration and nonreused collection tokens.
Both binaries also passed 84 actual terminal trap stops and 10 no-catchpoint
fatal sessions. Exact pretrap inputs, caller prefixes, real SIGILL retirement
and rejection of unsupported terminal native commands were checked.

The fixed seven-opcode trace remains exactly identical. In its four native
transitions, JIT instruction-step commands fell from **70/40/61/92** to
**61/31/53/84** in both products. These are actual owned-JIT NI commands for
this fixture, not a universal instruction count per Wasm opcode. Constant
opcodes with no native instruction remain present in the complete trace.

## Execution measurements

FFT uses the same 2048-point fixture and checksum, before/after binary hashes,
boot, E-core target affinity and debug optimization policy. Guest CLOCK_MONOTONIC
measures only the kernel; compilation and command formatting are outside it.
Five runs per ordinary/debug case, three per complete-trace case. No build or
profiler ran during these elapsed-time measurements.

| Workload | Policy 21 (ms) | Policy 22 (ms) | Observed change |
|---|---:|---:|---:|
| uwvm2 FFT, debug continuation | 2.176091 | 1.489267 | 31.56% less time |
| uwvm2-ros FFT, debug continuation | 2.175959 | 1.488179 | 31.61% less time |
| uwvm2 FFT, complete Wasm trace | 315.291181 | 312.103236 | 1.01% less time |

Complete tracing still requests every opcode; its measured change is small.
Ordinary FFT stayed near 0.074 ms. Current debug continuation remains about
20.2 times its own ordinary Wasm backend. GDB/native FFT is 0.045807 ms
versus 0.045777 ms ordinary native. These backends and optimization policies
differ; the native reference does not establish a fair instruction-for-instruction
comparison or GDB parity.

Median initial-prompt times fell from 9.155 to 8.857 seconds in uwvm2 and from
9.223 to 8.924 seconds in ROS. Startup and guest execution are separate costs.

Current 10,000-call benchmark, five runs per case:

| Breakpoint policy | uwvm2 (ms) | uwvm2-ros (ms) |
|---|---:|---:|
| Ordinary Wasm | 0.010667 | 0.010660 |
| None | 4.117587 | 4.077673 |
| 256 enabled, never hit | 4.092617 | 4.090148 |
| 256 disabled | 4.098876 | 4.077515 |
| 9,999 ignored hits, one visible stop | 6.083693 | 5.892684 |
| 32 visible stops, then disable | 7.691866 | 7.867890 |

Breakpoint counters and checksum 10000 passed. Cold/disabled tables add little
cost in this fixture, but calling-heavy debug execution remains expensive.
GDB/native no-breakpoint time is 0.007395 ms versus 0.007402 ms ordinary native.
Visible-hit timing includes actual PTY round trips. Ignored native traps and
uwvm's in-process filtering use different mechanisms, so ignored-hit timings
are not a general debugger ranking. Breakpoint before/after results from the
previous qualification are historical, rather than a fresh paired comparison.

## Hardware evidence

VTune CLI attached after startw/JIT completion to the same 120-repeat FFT. These
hardware runs include real quit/destruction and are separate from the unprofiled
kernel samples above. Target affinity remained E cores 16–31. The sampler's
cpuset expansion was restored to `0,2,4,6,16-31`; OOM counters remained zero.
Exact collection/report arguments follow Intel's
[hardware collection guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/collect.html)
and [microarchitecture CLI guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-1/run-general-exploration-analysis-command-line.html).

| Counting metric | Policy 21 | Policy 22 |
|---|---:|---:|
| Instructions Retired | 5718736021 | 3649461772 |
| Clockticks | 1595282768 | 1242059361 |
| CPI Rate | 0.278957 | 0.340340 |
| MUX Reliability | 0.999693 | 0.999792 |

Instructions decreased 36.18%; clockticks decreased
22.14%. CPI rose despite less total work; it does not mean total execution
regressed. Hotspots sampled CPU time fell from 0.485119 to 0.348214 seconds.
The largest entry remains outside known modules (0.329365 → 0.205357 seconds).
It can include anonymous JIT code, but is not exclusively proven JIT attribution.
Allocator and LLVM reference destruction occupy part of the retirement tail.
Shorter sampling makes individual function attribution less stable. Full
reports retain both hybrid CPU categories; empty-category top-down percentages
are not used as workload evidence.

## Architecture research and limits

The official [GDB fast tracepoint documentation](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Create-and-Delete-Tracepoints.html)
describes target-dependent alternatives such as jumps instead of traps, with
location restrictions. [Software breakpoints](https://sourceware.org/gdb/5.1.1/onlinedocs/gdbint_3.html)
restore, step and rearm modified instructions. LLVM
[stack maps and patchpoints](https://llvm.org/docs/StackMaps.html) expose explicit
live-value locations; zero-shadow maps add no reserved padding, but live values
can affect register allocation. Patchpoints reserve space and prohibit partial
replacement of their reserved region.

Our implementation conclusion is that general patchable Wasm stops also need
complete typed local/operand/control/reference recovery, owned code and pinned
generations, synchronized workers, and patch-aware native instruction views.
The existing bounded native software-breakpoint adapter does not by itself
supply that general recovery. This cut keeps demand checks and does not add a
general INT3 Wasm engine or instrumentation-free execution. Calls, host waits,
GC/table/prefixed operations and reference-containing sites retain their full
path. Calling-heavy execution remains expensive; these measurements do not
establish GDB execution parity.

## Evidence and reproduction

[The machine-readable receipt](debug_performance_bytes_20261010.json) includes
source/binary hashes, completed guard exits, raw timing samples, genuine trap
outcomes, shutdown counters, native transitions, VTune commands/reports and
resource restoration. All qualification phases completed successfully; no
partial or failed runs are counted as successes. Original raw evidence remains
under `/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/` on SSH Linux:

```text
byte-v22-final-checks.py
byte-v22-components.py
byte-v22-trap-checks.py
byte-v22-unobserved-traps.py
byte-v22-timings.py
byte-v22-hardware-root.py
collect-byte-receipt.py
```

Final binary SHA-256:

```text
uwvm2      40620b9e90b245ed07226910fb8da20bad4e7e01b3d6535b9c371ba0ba66723c
uwvm2-ros  900db87aab136f04a5594be90732cce8d4960e93477706f6fc938aac57a9aa70
```
