# Debug snapshot performance follow-up — 2026-10-10

This follows the [breakpoint/native-step qualification](README.debug-performance-20261010.md).
It addresses the remaining packet-decoding and generated snapshot-copy costs.
Every supported opcode still records the complete current typed packet. There
is no instruction sampling, omitted value update or stale operand substitution.
The final observation code-cache policy is **16**; resumable policy remains 17.

## Implementation

- Sealed compiler metadata derives the exact reference-slot indices and whether
  every executed local flag must be one. Numeric carrier bits remain unrestricted;
  all reference envelopes and all executed flags are still validated.
- Reusable activation-owned storage receives every slot's full 16-byte payload
  at every opcode. Local declaration types can be retained only for the same
  already materialized activation and the same retained immutable plan, after
  sealing compares the local prefix at every site. Dynamic initialized flags
  and operand declarations always refresh.
  When operand height grows, already complete typed cells are appended after
  reserving storage. This avoids clearing new cells and then immediately
  overwriting them. Existing-cell updates and growth use separate loops so the
  stable local prefix incurs no growth branch per cell. All flag/reference
  checks and allocation finish before changing the prior values; incomplete
  cells are never exposed.
- A nonempty numeric public-local prefix borrows the actual current opcode's
  owned packet. Its checkpoint recording is fused into the genuine safepoint
  host-call boundary, removing a second generated bridge call and local copy.
  The bridge still joins the live activation, generation, module, function,
  opcode and actual top-of-stack workspace before copying or calling observers.
  Reference prefixes and zero-local functions retain the original bridge path.
  The public five-argument native bridge ABI is unchanged.
- Numeric copy helpers load the actual 4/8/16-byte integer carrier and write a
  complete zero-padded 16-byte slot once. This replaces clearing a slot then
  overwriting its prefix, preserves signaling NaN bits and uses a big-endian
  shift where required. Absent flags select the immutable zero carrier before
  any local load. References retain the original format and checks.
- The actual LIFO workspace supplies bounded value/flag spans only after its
  payload base and layout match; an interior, stale, retired or overflowing
  payload label cannot nominate storage. This storage check grants no pause,
  native-root or continuation permission.
- Canonical x86-64 `vzeroupper` now permits a single native step only after its
  exact encoding and full LLVM MC effect shape match. Other zero/control forms
  and added prefixes remain refused; it does not grant branch, call, relocation
  or native-owner permission. The real current JIT owner is still required.

An intermediate Linux `CodeGenOptLevel::Less` experiment and zero-local fusion
caused a repeatable short-function regression. Both were withdrawn. The final
Linux debug machine policy remains `None`, with the IR pipeline `none`; the
existing bounded macOS policy is unchanged. The numerical FFT also used `None`
in the intermediate run, so its gain did not come from skipping IR safepoints.
Outlining the fused bridge restored its original 120-byte hot stack frame
(the inline version used 184 bytes), but added call overhead and worsened both
benchmarks. That experiment was also withdrawn.
Routing tiny zero-local packets through the updated packet factory also made
the short-function benchmark worse and was withdrawn.

## Controlled execution measurements

Final measurements and hardware evidence are recorded in the
[machine-readable receipt](debug_performance_snapshot_20261010.json).

Linux target: i9-14900HX, E-core affinity 16–31, the existing 64 GiB cgroup with
swap disabled. Each execution row is a three-run median; the native GDB FFT
uses five runs. Builds and profilers are stopped during these samples. Guest
`CLOCK_MONOTONIC` timing excludes JIT startup and breakpoint installation; hit
cases include real PTY continue/stop round trips inside the guest's timer.

The retained pre-change primary binary is the previous qualification's final
Linux binary. GDB 16.3 executes the same C++ workload as a native executable;
each debugger must be compared to its own ordinary backend baseline. Ignored
GDB hits cause real native traps whereas uwvm evaluates Wasm hit policy in
process. That case does not establish general GDB-equivalent performance.

| FFT workload, milliseconds | Ordinary | Debugger |
| --- | ---: | ---: |
| Retained primary binary | 0.073909 | 608.505762 |
| Final primary binary | 0.073800 | 284.690281 |
| Final ROS binary | 0.074129 | 279.068004 |
| Native executable / GDB | 0.046332 | 0.045727 |

The primary debug FFT takes **53.2% less time (2.14× faster)** than the retained
binary. It still takes about **3,858× its ordinary backend time**; GDB's
no-breakpoint native workload remains near its ordinary baseline. Complete
per-opcode snapshots are much more expensive than GDB's uninstrumented native
continue. This result is a snapshot optimization, not GDB execution parity.

| 10,000-call breakpoint workload, milliseconds | Final primary | Final ROS | GDB |
| --- | ---: | ---: | ---: |
| No breakpoint | 12.208394 | 12.113273 | 0.007390 |
| 256 installed, never hit | 12.494998 | 12.446320 | 0.007383 |
| 256 disabled | 12.163480 | 12.150117 | 0.007408 |
| Ignore first 9,999 hits | 14.262418 | 13.405782 | 954.440561 |
| 32 visible stops | 16.397414 | 15.569402 | 10.004361 |

Ordinary Wasm/native timings are 0.010677/0.007397 ms. The fresh retained-binary
measurement is 11.620584 ms without breakpoints and 11.918974 ms with 256 cold
breakpoints. Thus the final primary short-function case regresses **5.1%/4.8%**
respectively, despite the FFT improvement. The change does not resolve this
regression. Ignored-hit and visible-stop cases have different trap/interaction
costs and must not be used to conceal the no-hit gap.

## Qualification scope

Both final repository builds are tested through actual Linux PTYs: nine value
fixtures (numeric/reference/GC/loops/multivalue/tail/EH/nondefaultable/large
locals), three-level markers, register panes, native stepping, cancellation,
fast native breakpoints, symbols, restart and exact opcode traces. The finite
return loop tests Linux timeout/Ctrl+C; the unchanged unconditionally infinite
macOS fixture is not counted as a passing Linux continuation test.

Additional component tests exercise 16,000 complete declaration/flag/byte
transitions, dynamic native packet reuse, ledger validation, real LLVM MC,
signaling NaNs/SIMD/reference copy formats, full zero padding and 10,000-local
prefix behavior. Native copy execution is x86-64. Four 32/64-bit little/big
endian layouts pass LLVM IR verification; this does not qualify execution on
those other architectures or an executable checkpoint restore.

The ROS compiler retains its distinct preexisting full-module structure; only
the debug snapshot section is ported. An accidental whole-file sync brought in
primary-only tiered dependencies and failed compilation. The retained ROS
source payload restored that structure before its passing final build. Failed
intermediate runs and harness setup attempts are retained separately and are
not counted as final qualification passes.

All further compilation, execution and profiling after the user's local-memory
restriction occurred on SSH Linux. This source revision has not been compiled
or executed on macOS, or retested on Windows/QEMU. Earlier platform results do
not qualify this revision. PTY interaction is not a claim of new human GUI
validation.

## Equal-work hardware collection

`run_debug_vtune_workload.py` starts the real debugger, submits `startw`, attaches
Intel VTune hardware sampling, then submits `c`. The same complete longer FFT
(`fixtures/debug_fft_f64.cc`, `UWVM_DEBUG_FFT_REPS=120`) finishes in both binaries
with the same checksum. Collection ends on actual
debugger-process retirement. The counters therefore include the small quit
tail, but exclude startup and compilation. This avoids comparing unequal guest
work in fixed-duration windows.

The long fixture is compiled on Linux using system Clang 22's Wasm backend,
the retained WASIp1 sysroot and LLD 23. A scoped `-B` linker alias supplies the
Debian driver name `wasm-ld-22`; it changes no installed compiler. The build
receipt contains the exact command. The original longer `fft.wasm` lacks a
checksum and is excluded from qualification. Earlier failed logger, fixture
and cross-compiler setup attempts remain recorded separately.

```sh
vtune -collect hotspots -knob sampling-mode=hw \
  -knob enable-stack-collection=true -result-dir RESULT -target-pid PID
vtune -collect uarch-exploration -knob pmu-collection-mode=summary \
  -result-dir RESULT -target-pid PID
vtune -report summary -r RESULT -format csv
vtune -report hotspots -r RESULT -format csv -group-by function -limit 40
```

The cgroup's CPU eligibility is temporarily expanded for the per-CPU sampler;
the actual target remains pinned to E cores and retains the 64 GiB/no-swap
limits. The root driver restores the exact original CPU eligibility in a
`finally` block, including failed collection. Raw results, commands, binary and
fixture hashes, transcripts, resource receipts and restored CPU eligibility are
retained on Linux and referenced by the receipt. PMU totals are sampled
estimates and their multiplex reliability must accompany interpretation.

Hardware hotspots sampling completes the same 120-pair FFT once in each binary:

| Sampled metric, same complete work | Retained binary | Final binary |
| --- | ---: | ---: |
| Guest kernel elapsed, seconds | 81.961044 | 40.001938 |
| CPU time, seconds | 80.597217 | 39.321426 |
| Instructions retired | 892,432,800,000 | 433,238,400,000 |
| Snapshot decoder CPU time, seconds | 25.189482 | 9.543650 |
| Outside known modules CPU time, seconds | 40.987100 | 11.510912 |
| CPI | 0.369168 | 0.370842 |

CPU time falls **51.2%**, sampled retired instructions **51.5%**, and the
decoder's own sampled CPU time **62.1%**. CPI is almost unchanged: fewer
instructions, rather than a higher instruction issue rate, explain this gain.
The remaining decoder is still 24.3% of sampled CPU time; the fused safepoint
is 9.8%, retained frame-plan lookup 5.3%, and mutex locking 4.6%. The 29.3%
outside-known-modules category can include generated JIT code, but this
unresolved category alone cannot prove exclusive JIT attribution. Counters and
function times are estimates, and this one profile pair is separate from the
three-run unprofiled timing medians.

The separate microarchitecture counting run confirms the same workload result:

| Counting metric, E-core target | Retained binary | Final binary |
| --- | ---: | ---: |
| Instructions retired | 891,349,212,363 | 432,684,726,033 |
| Clockticks | 308,841,138,220 | 143,192,563,878 |
| CPI | 0.346487 | 0.330940 |
| MUX reliability | 0.999772 | 0.999573 |
| Retiring share | 58.4% | 62.5% |
| Front-end bound share | 8.0% | 7.0% |
| Back-end bound share | 31.2% | 30.0% |
| Store bound share | 22.3% | 6.5% |

Counting reports **51.5% fewer retired instructions** and **53.6% fewer
clockticks**. The lower store-bound share is consistent with removing repeated
slot clearing and public-local copies. Only active E-core metrics are used:
the target retires zero P-core instructions, so apparent P-core ratios are not
interpreted. Counting has no per-function attribution; its `[Unknown]` row is
the entire process and must not be called a JIT hotspot. The separate sampling
and counting modes impose different overhead and need not report identical CPI.

Final Linux binary SHA-256:

- Primary: `d0cdf370a87ce663aa040f24728fd4abe4f40614b236c9bb5ed23fb0e94c6470`
- ROS: `673d3cb03d3e92e7b28d026f16d827a94496973301a5a3f2fcb16a2e68c4bfd9`

Both clean rebuilds use `xmake -r -j3` within the existing 64 GiB/no-swap cgroup;
both sets of nine PTY groups and six component units pass. The final CPU
eligibility is restored to `0,2,4,6,16-31`; `oom` and `oom_kill` remain zero.

Official Intel references: [collection CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-4/collect.html),
[microarchitecture CLI tutorial](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2023-2/run-general-exploration-analysis-command-line.html),
[instruction reference](https://cdrdv2-public.intel.com/774492/325383-sdm-vol-2abcd.pdf).
