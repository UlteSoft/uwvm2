# Multi-workload debug execution — 2026-10-10

Both products were built with xmake -j3 on SSH Linux in the actual 64 GiB,
zero-swap cgroup. All runtime, PTY, native reference and profiler work ran there;
no local executable tests were run. Target affinity is E cores 16–31 on the
Core i9-14900HX. This qualification does not cover macOS or Windows/QEMU.

## What changed

Hardware sampling of call-heavy kernels found repeated canonical publication
lookups, shared-plan ownership traffic and typed-packet copying. Entry still
validates the canonical registry, publication, plan control block and generation.
The fixed-packet and generated-return hooks now borrow that same retained plan
from the actual live TLS leaf, as the dynamic-packet hook already did. They check
activation count, incarnation, runtime epoch and function generation; no callback
or publication can intervene during this borrow. Stopped-state inspection keeps
its original canonical checks. Copied IDs, pointers and cached storage grant no
pause, root, register or executable-resume capability.

Fixed packets validate every reference/uninitialized slot before updating the
activation's owned values, avoiding a fresh detached vector per packet. Both
value vectors and operand workspaces can reuse empty allocations after exact
retirement. New frame identities, plans, materialized state and values never
carry over. Workspace resize constructs fresh zero bytes, including all flags.
Each pool retains at most 32 allocations of 16 KiB payload capacity: at most
1 MiB combined per worker, plus metadata/allocator overhead. Larger or excess
buffers still work and free normally. Code-generation policy stays **22** and
resumable policy 17; this is a runtime/storage change, with unchanged IR/ABI.

## Workloads and measurements

The new generator emits ten Wasm modules plus equivalent native C kernels.
WASI CLOCK_MONOTONIC measures only each kernel, after initializing its working
set. Native/Wasm and before/after checksums must agree. Five serial samples per
ordinary, no-breakpoint, cold and disabled case; before/after order alternates.
Cold/disabled matrix cases check a genuine unused breakpoint with hit count zero.
Startup, input and formatting are outside the kernel. No builds/profilers compete
with these timing runs. Native C uses O2; debug JIT retains pipeline/codegen None.
The native/GDB reference therefore uses its own ordinary baseline and is not an
instruction-for-instruction comparison.

The tested E-core cache topology reports 32 KiB L1 data, 4 MiB shared L2 per
four-core group and 36 MiB shared L3. Sequential read/write kernels use 32 KiB,
256 KiB and 8 MiB working sets. The 8 MiB pointer chase follows a dependent
full-period permutation; integer/f64 recurrences, random nested branches,
br_table dispatch, 300,000 direct calls and a nested call chain cover other
execution patterns.

Median debug continuation, milliseconds:

| Kernel | uwvm2 before → after | ROS before → after |
|---|---:|---:|
| integer | 3.829 → 3.831 | 3.834 → 3.832 |
| branches | 7.554 → 7.544 | 7.580 → 7.557 |
| dispatch | 6.779 → 6.750 | 6.716 → 6.740 |
| scan32k | 36.269 → 36.274 | 36.276 → 36.270 |
| scan256k | 36.297 → 36.285 | 36.284 → 36.283 |
| scan8m | 36.314 → 36.327 | 36.344 → 36.323 |
| chase8m | 37.090 → 35.869 | 35.159 → 36.992 |
| calls | 136.614 → 88.352 | 135.619 → 87.432 |
| callchain | 314.943 → 197.893 | 315.590 → 197.873 |
| float | 2.797 → 2.799 | 2.799 → 2.802 |

Direct-call time fell **35.33% / 35.53%** (primary/ROS); nested calls fell
**37.17% / 37.30%**. Arithmetic, branch and sequential-memory timing is essentially
unchanged. Pointer-chase variation goes in opposite directions across products;
these samples do not establish a memory-access improvement.

Current primary debug/own-ordinary ratios are approximately 8.4× integer, 6.3×
f64, 16.4× branches, 3.9× dispatch, 44.3–44.5× sequential read/write, 1.2× pointer
chase, and **201× / 209× calls / call chain**. GDB/native no-breakpoint ratios
are 1.00–1.02× in this matrix. Execution remains far from GDB's no-hit efficiency.
The remaining gap includes different optimization policies and observation
machinery; these elapsed times do not separate their contributions.

Complete trace, using the original full-size call modules (three samples):

| Kernel | uwvm2 before → after (ms) | ROS before → after (ms) |
|---|---:|---:|
| calls | 713.020 → 656.164 | 708.427 → 646.416 |
| callchain | 892.974 → 760.874 | 892.394 → 761.836 |

This is about **8–9%** less time for direct calls and **15%** for the call chain
while requesting every opcode. All ten algorithms also have small complete-trace
qualification variants; their kernel sizes/working sets differ, so they are not
used as performance evidence. A contiguous 512-event retained trace window is
checked; completeness independently uses the exact seven-opcode fixture and
per-opcode value/control/reference tests.

## Qualification

Both products passed twelve real PTY groups: infinite-loop pause/restart/quit,
observation policies, memory freshness, nine typed/control/reference scenarios,
pane markers, registers, Linux native stepping/cancellation/breakpoints,
symbols/help/layout, 32 mixed-mode restarts and exact opcode trace. Five component
checks passed, including recycled zero flags, exact LIFO cleanup, stale-identity
rejection, fresh frame values and rejected-packet preservation. Both products
passed 84 genuine terminal trap stops and ten no-catchpoint SIGILL sessions.

## Breakpoint and FFT measurements

Fresh paired five-run tests use the original 10,000-call fixture. Cold and
disabled tables contain 256 genuine breakpoints. Ignored-hit and visible-hit
cases check counters; visible-hit timing includes actual PTY round trips.

| Breakpoint policy | uwvm2 before → after (ms) | ROS before → after (ms) |
|---|---:|---:|
| none | 4.111 → 2.587 | 4.097 → 2.569 |
| cold256 | 4.122 → 2.589 | 4.093 → 2.553 |
| disabled256 | 4.132 → 2.589 | 4.097 → 2.573 |
| ignore9999 | 5.986 → 4.072 | 5.635 → 4.222 |
| hits32 | 7.765 → 6.105 | 7.735 → 6.319 |

The primary no-breakpoint cost fell 37.08%; cold/disabled tables add little cost
in this fixture. Current ordinary Wasm is 0.010701 ms, so debug still costs about
242× its ordinary backend. GDB/native is 0.007397 versus 0.007412 ms ordinary;
256 cold/disabled native breakpoints are also near that baseline. Ignored native
traps and uwvm's in-process filtering are different mechanisms, so their timings
are not a general debugger ranking.

FFT stays essentially unchanged: primary **1.489023 → 1.489123 ms**, ROS
**1.488084 → 1.489671 ms** (debug, five samples). Ordinary Wasm stays near
0.074 ms. The change benefits call boundaries rather than its numeric kernel.


## Hardware evidence

Fourteen serial primary collections use the exact same Wasm modules and
before/after binary hashes. VTune attaches at an exact _start offset after
initialization, BEFORE the guest kernel clock starts; the breakpoint is disabled
before collecting/continuing. JIT startup and working-set initialization are
excluded. Counting/sampling still includes clock/output wrappers and real
quit/destruction, so hardware totals are not exclusively the measured kernel.
Profiler timings are separate from the unprofiled samples above. The runtime
itself is compiled with O3 in both products; generated debug code retains None.

Actual CLI arguments follow Intel's [collection guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/collect.html)
and [hardware-hotspots guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-0/run-basic-hotspots-analysis-command-line.html).

| Kernel | Retired instructions before → after | Clockticks before → after |
|---|---:|---:|
| calls | 1,898,008,075 → 1,374,371,153 | 567,629,610 → 371,565,575 |
| callchain | 4,193,159,725 → 2,950,441,558 | 1,297,177,964 → 823,176,696 |
| branches | 101,031,056 → 101,066,702 | 37,796,003 → 39,128,282 |
| scan32k | 685,308,529 → 685,175,136 | 158,065,798 → 158,379,404 |
| chase8m | 469,668,559 → 469,674,953 | 197,416,817 → 193,320,400 |

Direct calls retire **27.59%** fewer instructions and **34.54%** fewer clockticks;
nested calls retire **29.64%** fewer instructions and **36.54%** fewer clockticks.
CPI improves 0.299 → 0.270 and 0.309 → 0.279 respectively. Those counting runs
have MUX reliability about 0.976–0.999. The short branch collections have lower
MUX reliability (0.82–0.84); their small differences are not reliable gain evidence.
Memory instruction counts are essentially unchanged. Pointer-chase cycles vary
by about 2%, alongside opposing elapsed-time variation across products.

After-call hotspots still include typed packet decoding, actual-leaf borrowing,
entry publication validation and safe-point bookkeeping. Unknown-module samples
can include anonymous JIT code but are not exclusively proven JIT attribution.
Short function samples and normalized top-down percentages are not absolute-cost
proof. No GDB parity or instrumentation-free execution is established.

The sampler's temporary cpuset expansion was restored to `0,2,4,6,16-31`.
OOM counters remain zero; no execution/build/profiler workers remain. The cgroup's
recorded peak memory is about **9.76 GiB**. The receipt contains 1,378 paired/native/
trace/FFT/breakpoint samples, in addition to screen/component/interaction checks.


## Reproduction and evidence

[Machine-readable receipt](debug_performance_matrix_20261010.json) retains sample
values, immutable before/after hashes, fixture/source digests, build/PTY/trap
receipts, exact CLI arguments and hardware reports. Raw sessions remain under
`/home/macromodel/Documents/uwvm-tui-20261009/perf-20261010`. Two setup failures
(path quoting/concatenation) and an overly strict breakpoint-text assertion are
kept as excluded runs; repaired runs use fresh result directories.

Generate fixtures with `make_debug_workload_matrix.py --out DIR --wasm-tools TOOL
--cc CLANG`. Run `run_debug_workload_matrix.py --uwvm AFTER --baseline BEFORE
--fixtures DIR --out RESULTS --native --repeats 5`. Use `--ros` for ROS; add
`--case trace --only calls --only callchain` for full-size call tracing. Use the
same generator with `--tiny` for the ten small trace qualification variants.

General instrumentation-free Wasm breakpoints remain unimplemented. The official
[GDB software-breakpoint description](https://sourceware.org/gdb/5.1.1/onlinedocs/gdbint_3.html)
uses traps; achieving comparable no-hit execution also requires complete typed
Wasm state recovery and actual code/worker ownership, rather than replacing the
current packets with guessed state.
