# Numeric debug observation — 2026-10-10

This continues [the policy 19 qualification](README.debug-performance-probes-20261010.md).
Observation code-generation policy is now **21**; resumable policy remains 17.
Source is synchronized with uwvm2-ros, preserving its different activation bridge
and emitter. All builds, executable tests, GDB sessions and profiling ran on SSH
Linux in a 64 GiB, zero-swap cgroup. No local executable testing was performed.
These Linux results do not qualify macOS or Windows/QEMU.

## Implementation

Numeric loads/stores, division/remainder and trapping conversions now use the
existing demand bitmap. With no requested observation or shutdown, eligible
sites skip packet serialization, decoding and the observer callback. Their
actual typed plan must contain only initialized numeric slots. Calls, host
waits, memory size/grow, table/GC/prefixed operations and reference-containing
sites retain full collection. Custom observers default to complete observation.

The controller publishes full demand before stepping, tracing, pausing, native
breakpoints and all catchpoints, including terminal traps. Requested failures
therefore still expose their original exact pre-opcode inputs. Unobserved
terminal traps still terminate with the actual runtime diagnostic.

An activation-private stamp guards reuse of unchanged numeric packet cells.
Every publication invalidates the previous token before changing bitmap words;
a fresh odd token is published only after all words request collection. Reuse
requires that same odd token and the compiler's exact private block chain.
Skipped captures, policy changes, resume, native unpark and Wasm CFG/owner changes
force refresh. References, locals and initialization flags remain fresh. Tokens
never wrap into a reused value, and give no register, root or resume authority.
The skipped-capture path receives no extra stores.

Mandatory callbacks without requested observations avoid the controller mutex.
The runtime still maintains its actual wait point, cancellation and pause poll.
The fused packet/observer path also reuses its synchronously authenticated plan
borrow instead of joining the same native leaf twice. Complete extent, flag,
type, reference, owner and generation checks remain before detached copying.
Ordinary non-debug JIT receives none of this observation IR.

## Execution measurements

Guest CLOCK_MONOTONIC kernel medians, three runs per Wasm case; compilation,
command entry and output formatting are outside the kernel timer. Before/after
FFT samples use the same fixture, checksum, debug optimization policy and remote
boot. No compiler or collector ran during these timing tests.

| Workload | Policy 19 (ms) | Policy 21 (ms) | Change |
|---|---:|---:|---:|
| uwvm2 FFT, debug continuation | 41.010199 | 2.175732 | 18.85× faster |
| uwvm2-ros FFT, debug continuation | 40.941974 | 2.173726 | 18.83× faster |
| uwvm2 FFT, complete Wasm trace | 332.906058 | 315.731680 | 5.16% less time |

Current ordinary Wasm FFT is 0.073810 ms. Debug continuation remains about 29.5×
its own ordinary backend. GDB/native FFT is 0.045829 ms versus 0.046034 ms ordinary
native, five runs each. The ordinary backends and optimization policies differ;
these results do not establish GDB execution parity. Full trace requests every
opcode and is much more expensive than continuation without observations.
Historical policy 16 full trace was 310.871865 ms on a different run/boot; this
cut does not demonstrate an improvement over that historical result.

There is a startup tradeoff: median time to the initial uwvm2 prompt rose from
8.580 to 9.228 seconds (uwvm2-ros: 8.686 to 9.287). The extra cold packet IR still
costs compilation time. The execution speedup does not imply faster launch.

Current 10,000-call fixture medians:

| Breakpoint policy | uwvm2 (ms) | uwvm2-ros (ms) |
|---|---:|---:|
| None | 4.149429 | 4.127523 |
| 256 enabled, never hit | 4.182648 | 4.149608 |
| 256 disabled | 4.155644 | 4.125699 |
| 9,999 ignored hits, one visible stop | 5.638720 | 6.395886 |
| 32 visible stops, then disable | 7.748620 | 8.213352 |

Actual breakpoint counters and checksums passed. Visible stops include PTY
round trips. In this fixture GDB/native no-breakpoint time is 0.007399 ms and
ordinary native is 0.007394 ms. GDB's ignored software traps and uwvm's in-process
filtering are different mechanisms; ignored-hit timings are not a general
debugger comparison.

## Hardware evidence and remaining cost

VTune CLI attached after startw/JIT completion and collected the same 120-repeat
FFT through real debugger retirement. Hardware runs include the quit/destruction
tail and are separate from unprofiled kernel timing. Target affinity remained
E cores 16–31. Collector setup temporarily broadened the cgroup cpuset and
restored it afterward. Exact arguments follow Intel's
[hardware collection guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/collect.html)
and [microarchitecture CLI guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-1/run-general-exploration-analysis-command-line.html).

| Counting report | Policy 19 | Policy 21 |
|---|---:|---:|
| Instructions Retired | 61073452104 | 5718580216 |
| Clockticks | 21185921129 | 1590021978 |
| CPI Rate | 0.346892 | 0.278045 |
| MUX Reliability | 0.998292 | 0.999285 |

Instructions decreased 90.64%; clockticks decreased 92.49%. CPI also improved.
Hotspots sampled CPU time is 5.704364 → 0.483135 seconds. The earlier decoder,
plan-borrow and mutex functions no longer appear in the after report's top 40.
This is not proof they never run. The shorter after run makes fine-grained
sampling less stable, and LLVM object destruction occupies part of its measured
retirement tail. “Outside any known module” dominates the after report and can
include anonymous JIT code; it is not exclusively proven JIT attribution.
Counting-only totals do not identify individual functions.

## Breakpoint architecture research

[GDB's software breakpoint algorithm](https://sourceware.org/gdb/5.1.1/onlinedocs/gdbint_3.html)
restores, single-steps and rearms a replaced instruction. Intel's
[breakpoint exception description](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf)
specifies the one-byte INT3 and its saved instruction pointer. LLVM
[stack maps](https://llvm.org/docs/StackMaps.html) describe explicitly retained
live values in registers, stack locations or constants. A zero-shadow record
emits no shadow padding; retaining additional values can still affect generated
code. Patchpoints reserve code space and require replacing the whole region.

Our architectural conclusion is that a general Wasm trap producer needs actual
pinned JIT code, complete typed local/operand/control/reference recovery from the
stopped kernel context, patch-aware instruction qualification, synchronized
worker stopping, and correct restore/step/rearm. Constant Wasm opcodes may have
no native instruction, so exact breakwasm/stepwasm can require distinct anchors.
Existing bounded native-step adapters do not supply all of that Wasm recovery.
[Linux uprobes](https://docs.kernel.org/trace/uprobetracer.html) use executable
file paths and offsets, so their documented interface is not a direct replacement
for arbitrary anonymous JIT addresses. Arm additionally requires
[cross-thread instruction synchronization](https://developer.arm.com/community/arm-community-blogs/b/architectures-and-processors-blog/posts/caches-self-modifying-code-working-with-threads).

This cut still emits demand checks. It does not implement a new general INT3
Wasm breakpoint engine or instrumentation-free execution. Native instructions
outside the authenticated LLVM JIT owner remain excluded.

## Qualification and reproduction

Both final binaries built successfully with xmake -j3. Four component checks
passed. Each binary passed eleven real PTY groups: numeric demand/policies,
memory-load freshness, typed values/references/control, pane markers, registers,
Linux native stepping, finite-return cancellation, bounded native breakpoints,
symbols/help/layout, repeated restart and exact opcode tracing.

Both binaries additionally passed 84 actual terminal trap stops, covering two
stack policies, memory32/64 boundaries, division, conversion, references, bulk
operations and preserved caller prefixes. Exact original inputs were verified;
invalid terminal native commands were refused and continue genuinely ended in
SIGILL. Ten additional no-catchpoint sessions verified real fatal diagnostics
and retirement without creating a spurious debugger stop.

The fixed opcode fixture verifies every expected opcode, including a constant
without native code and end. FFT trace checks checksum and contiguous retained
pages. Its newest 512 events are retained; this bound is not instruction sampling.
Whole-program event totals can vary with formatting the measured clock value.

Linux rebooted during the first timing driver. That incomplete receipt and
interrupted partial samples are excluded. All final timing cases reran on the
unchanged binary hashes in a replacement 64 GiB, zero-swap cgroup. Earlier harness
fixture/remapping/argument mistakes are retained as exclusions, not passing
qualifications. OOM counters remained zero and cpuset returned to
`0,2,4,6,16-31`.

[The machine-readable receipt](debug_performance_numeric_20261010.json) contains
source/binary hashes, successful guards, raw samples, trap outcomes, exact VTune
arguments, parsed counter/hotspot reports, exclusions and resource restoration.
Raw evidence and reproduction drivers remain on SSH Linux under:

```text
/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010/
  demand-v21-final-checks.py
  demand-v21-components.py
  demand-v21-trap-checks.py
  demand-v21-unobserved-traps.py
  demand-v21r-timings.py
  demand-v21-hardware-root.py
  demand-v21-hardware-{hotspots,uarch-exploration}-{before,after}/
```

Final binary SHA-256:

```text
uwvm2     d5a60100890ae27f961a18c50054bbf738e2e751300a1b11b4a39f2b81f9a037
uwvm2-ros 765e52ddd229e401995e977aa028b9ed38d9423bfa69987683eead16d7f4594e
```
