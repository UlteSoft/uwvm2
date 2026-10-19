# Demand-driven debug observation — 2026-10-10

This follows [the snapshot-copy qualification](README.debug-performance-snapshot-20261010.md).
The final observation code-generation policy is **19**; resumable policy remains
17. All compilation, executable tests, GDB sessions and VTune collection in this
cut ran on SSH Linux in the existing 64 GiB, zero-swap cgroup. No local compilation
or executable testing was performed. macOS and Windows/QEMU remain unqualified
for this cut.

## Implementation and boundaries

The controller opts into a process-lifetime, lock-free 4096-bit observation hint
table. Eligible Linux x86-64 numeric opcode sites check their constant bucket and
the existing shutdown request. With neither request present, they skip packet
serialization, decoding and the cooperative observer call. Collisions only cause
extra collection; the controller still compares the actual breakpoint identity.
A hint supplies no native read, register, stop, root or continuation authority.

Every requested stop still uses the original complete typed packet, activation,
generation, function, opcode and owned LIFO workspace checks. Calls, potentially
trapping instructions, memory/table operations, prefixed instructions and sites
with a reference or uninitialized slot retain full collection. Numeric entries,
ends and returns may skip collection during ordinary continuation; initial entry,
source/Wasm/native stepping, native breakpoints, catchpoints and enabled Wasm trace
publish full demand. Ordinary non-debug code gets no hint or packet IR.

Pause publishes full demand before requesting the actual domain pause. Resume
publishes demand before waking workers. Runtime close restores conservative full
collection before closing control. The shutdown flag also selects the cold path,
so fast numeric loops cannot indefinitely bypass genuine cleanup/retirement.
Publication is serialized against actual observer context replacement; retired
publishers cannot clear another observer's demand. Custom observers default to
full observation unless they explicitly opt into the publication contract.

The conditional capture edge invalidates packet-store reuse: those stores need
not have executed. Pre-probe semantic numeric SSA witnesses may survive only when
the original block and append-only position match; their definitions dominate
both private successors. Wasm CFG/owner changes still invalidate that reuse.
These witnesses grant no availability for a retained register prefix spilled by
a host call. The observation profile still provides no executable Wasm restore.

## Architecture research

GDB's [software-breakpoint protocol](https://www.sourceware.org/gdb/current/onlinedocs/gdb.html/Packets.html)
uses instruction replacement with a trap. Such a breakpoint avoids polling every original instruction when it is not hit. LLVM's
[stack maps and patch points](https://llvm.org/docs/StackMaps.html) describe live
value locations and reserved patch regions; zero-shadow stack maps emit no shadow
bytes. Those records must still be interpreted against the actual stopped native
context and the compiler's exact typed Wasm/control layout.

This cut implements conditional observation, **not INT3 patching or an
instrumentation-free debugger**. A real replacement needs owned JIT code and
lifetime proofs, exact recovery of live values before the opcode, saved control/call state,
precise references, synchronized patches while threads stop, target instruction-cache
handling and correct restore/single-step/re-arm behavior. Native instructions
outside the current LLVM JIT owner remain prohibited. LLVM's
[JIT debugger registration](https://llvm.org/docs/DebuggingJITedCode.html) is useful
for code/symbol visibility but does not by itself reconstruct a Wasm operand stack.

## Validation

Four shared-component checks passed. Each final binary passed ten real PTY groups:
conditional numeric-loop values/policies, mixed typed values/references/control,
pane markers, native registers, Linux SI/NI/step-out, cancellation, bounded native
breakpoint continuation, symbols/colour/layout help, repeated restart and the
fixed complete opcode trace. The numeric-loop test verifies fresh locals and
operands after skipped opcodes and checks ignore, disable/enable, delete,
condition-unavailable retention and restart. The fixed trace includes all seven
expected opcodes, including a constant with no native instruction and end.

Full-trace FFT timing separately checks checksum and contiguous retained trace
pages. Its complete-program event total includes value-dependent formatting of
the measured time, so totals are recorded rather than required to be identical.
The bounded trace retains its newest 512 events; that retention limit is not
instruction sampling. Failed harness assumptions and premature pre-link launches
are retained in the JSON exclusions and are not final qualifications.

## Results and practical limits

Guest CLOCK_MONOTONIC kernel medians, three runs per Wasm case. The same fixture
and checksum are required; launch, compilation and breakpoint installation are
outside the measured kernel. No compiler or collector ran during these timings.

| Workload | Before (ms) | After (ms) | Change |
|---|---:|---:|---:|
| FFT, ordinary debug continuation | 283.692252 | 40.990631 | 6.92× faster |
| 10,000 calls, no breakpoint | 12.164768 | 5.655077 | 2.15× faster |
| 10,000 calls, 256 unhit Wasm breakpoints | 12.607564 | 5.695293 | 2.21× faster |
| 10,000 calls, 256 disabled Wasm breakpoints | 12.181599 | 5.590201 | 2.18× faster |
| FFT, complete Wasm trace enabled | 310.871865 | 331.528527 | 6.64% slower |

**The full-trace throughput regression remains.** The semantic-SSA witness change
did not remove it. This tradeoff is reported separately from the fast continuation
result. Trace still records every opcode; packets are fully collected at requested
stops. Remaining extra branches and invalidated packet-store reuse need further
work before this trace mode can match the previous throughput.

Ordinary Wasm FFT remains 0.073771 ms; debug continuation is still about
556× its own ordinary backend. GDB/native FFT was
0.046082 ms versus its native ordinary baseline
0.046312 ms (five runs). These backend baselines differ. No GDB parity is
claimed. In the call fixture, GDB no-hit cost was 0.007383 ms versus
0.007391 ms ordinary native.

uwvm2-ros FFT measured 41.261447 ms;
no-breakpoint calls 5.557193 ms.
Ignored-hit and visible-hit cases also passed with unchanged counters/checksums.
The visible-hit timing includes PTY round trips; GDB's ignored software traps and
in-process callback filtering have different mechanisms, so those timings do not
establish a general debugger speed comparison.

## Hardware evidence

VTune CLI hardware attach started after startw and JIT completion, then collected
the same 120-repetition FFT through genuine debugger quit/retirement. Both binaries
used the identical Wasm hash and checksum. The target remained on E cores 16–31;
collector setup temporarily broadened the cgroup cpuset and restored it afterward.
The official [CLI collection guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/collect.html)
and [microarchitecture CLI guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-2/run-general-exploration-analysis-command-line.html)
are recorded with the exact arguments and result hashes.

| Counter/report | Before | After |
|---|---:|---:|
| Hotspots: CPU Time | 39.136902 | 5.711309 |
| Hotspots: Instructions Retired | 433231200000 | 61147200000 |
| Hotspots: CPI Rate | 0.369248 | 0.380132 |
| Counting: Clockticks | 143532945332 | 21250187540 |
| Counting: Instructions Retired | 432694371381 | 61073435832 |
| Counting: CPI Rate | 0.331719 | 0.347945 |
| Counting: MUX Reliability | 0.999767 | 0.995962 |

Counting instructions decreased 85.89%;
clockticks decreased 85.19%. CPI rose slightly,
0.331719 → 0.347945; this gain comes from doing less work.
The E-core retiring share was 62.4 → 59.4%, and store-bound share
6.5 → 11.3%; these relative shares do not describe
absolute work. P-core target instruction counts were zero.

Packet decoder sampling time fell 9.436507 → 1.179563 seconds.
The remaining decoder, safepoint, plan-borrow and mutex costs remain material.
“Outside any known module” can include JIT code but is not exclusively proven JIT
work. Counting-only “[Unknown]” totals have no per-function attribution.

## Reproduction and saved evidence

[The complete machine-readable receipt](debug_performance_probes_20261010.json)
contains source/binary hashes, both builds, all interaction/component outcomes,
raw timing samples, collector arguments, parsed reports, exclusions and resource
restoration. Raw evidence remains on SSH Linux at:

```text
/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/
  probe-carry-qualified-checks.py
  probe-carry-timings.py
  probe-carry-components.py
  probe-hardware-v2-root.py
  probe-hardware-v2-{hotspots,uarch-exploration}-{before,after}/
```

Final binaries:

```text
uwvm2 679ceb279e8e20756b67599352447e9383bbd0b46ff383d52fe4f88ff44ca7ae
uwvm2-ros ee1a9343a752765e717462352e9407e9868b6961b74d052dac1eb8a9db2d4ed1
```

The original cpuset is restored to `0,2,4,6,16-31`; memory.max is 64 GiB,
swap.max is zero, and all OOM counters remain zero. Source is synchronized with
uwvm2-ros while retaining its different runtime bridge and emitter implementation.
