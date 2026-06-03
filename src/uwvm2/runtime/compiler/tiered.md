# uwvm2 Tiered Runtime Compiler

This document describes the three-tier runtime compiler used by
`uwvm_interpreter_llvm_jit_tiered`. The design goal is to keep startup latency
close to LLVM lazy JIT, avoid blocking when native code is not ready, and still
provide a whole-module native tier for long-running workloads.

When the tiered backend is compiled in, it is the default lazy runtime backend.
`--runtime-jit` remains available as an explicit opt-in for pure LLVM lazy JIT.

## Tier Overview

The tiered backend has three execution tiers:

1. **Tier 0: `uwvm-int` lazy interpreter**

   Tier 0 is the latency tier. When execution reaches a function whose LLVM
   native entry is not ready, the runtime compiles or reuses the `uwvm-int`
   lazy execution unit and immediately runs the interpreter result. It must not
   wait for LLVM. Loop OSR polls are intentionally lightweight: the interpreter
   reads the smallest required state, exits immediately on a miss, and only
   transfers to native code when a ready reentry is already published.

2. **Tier 1: LLVM lazy JIT**

   Tier 1 is the normal native tier. It uses the same lazy LLVM strategy as
   `llvm_jit_only`: compile units are materialized on demand, demand requests
   use higher scheduler priority than background work, and successful
   materialization publishes raw and typed entry targets. While a target is not
   ready, the published raw target points at the tiered runtime bridge so the
   interpreter can execute the call without blocking.

3. **Tier 2: background full-module LLVM JIT**

   Tier 2 is the steady-state tier. After tiered execution has successfully
   switched into native code, a low-priority background request translates the
   same module through the full LLVM pipeline without the tiered lazy call
   bridge. The resulting native module uses direct intra-module calls and is
   published over the Tier 1 targets. This removes the lazy target-table
   barrier from later calls and gives long-running programs the full-module
   layout used by LLVM JIT full mode.

## State Transitions

Each module owns independent tiered state:

- `tiered_full_compile_state` is a scheduler unit with the normal lazy states:
  `uncompiled`, `queued`, `compiling`, `compiled`, and `failed`.
- `tiered_full_ready` is an atomic byte published with release semantics after
  Tier 2 entries have been materialized and written into the target tables.
- `tiered_switch_count` and `tiered_direct_switch_count` count successful
  transitions into native code. Tier 2 is requested only after a sustained
  native-switch threshold, and the request has background priority so it cannot
  overtake a demand LLVM lazy request.

The expected progression is:

```text
Tier 0 interpreter -> Tier 1 lazy native -> Tier 2 full native
```

Tier 1 is mandatory. It is the center of the tiered backend and cannot be
disabled. Tier 0 and Tier 2 are controlled independently:

- `--runtime-tiered-disable-uwvm-int-lazy-interpreter`
  (`-Rtiered-disable-t0`) disables the Tier 0 interpreter fallback. In this
  mode, a miss in the tiered entry path synchronously materializes the LLVM lazy
  function and executes that native entry; `uwvm-int` lazy storage is not
  initialized for tiered execution.
- `--runtime-tiered-disable-llvm-full-jit` (`-Rtiered-disable-t2`) disables the
  background full-module LLVM JIT request path. Tier 1 LLVM lazy JIT remains
  active.
- Disabling both Tier 0 and Tier 2 leaves the tiered shortcut as LLVM lazy JIT:
  the initial raw targets, lazy publication path, and background lazy scheduler
  use the same policy as `llvm_jit_only` lazy mode.

Failure is non-fatal for tiered execution. If Tier 2 translation or
materialization fails, the state becomes `failed` and execution continues with
Tier 0/Tier 1. This is important because Tier 2 is an optimization, not a
correctness requirement.

## Publication Protocol

Tiered execution uses two target families:

- raw call targets, used by runtime bridges and call-indirect tables;
- typed entry targets, used by generated LLVM lazy code when a matching native
  function body is already available.

All tiered target publication uses atomic stores. Tier 1 lazy code also loads
these targets atomically in tiered mode so the background Tier 2 publisher can
replace entries safely while generated code is running.

Tier 2 publication is deliberately two-phase:

1. publish all full-module raw and typed entries while `tiered_full_ready` is
   still false;
2. set `tiered_full_ready` with release ordering;
3. publish all entries again.

The second pass closes the race where a late Tier 1 materialization callback
could publish a lazy entry after the first Tier 2 pass. Once
`tiered_full_ready` is visible, later lazy materialization callbacks avoid
overwriting Tier 2 and re-publish the full entry for that function if needed.

Call-indirect tables are repaired through the same publication function. Every
table element whose context points at the corresponding defined function is
updated to the new raw native entry.

## Isolation From Pure Modes

The Tier 2 machinery is guarded by
`UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED` and only becomes active when the
runtime compiler is `uwvm_interpreter_llvm_jit_tiered`.

Pure modes do not request or observe the Tier 2 scheduler unit:

- `uwvm-int` full and lazy do not use LLVM lazy target tables.
- `llvm-jit` lazy keeps its existing lazy materialization strategy.
- `llvm-jit` full keeps its existing eager full-module materialization path.
- The full-mode `llvm_jit_ready` flag remains a non-concurrent full-mode flag;
  tiered background readiness is tracked separately by `tiered_full_ready` to
  avoid data races.

## Scheduling Policy

Tier 0 interpreter lazy translation is not enqueued in the shared lazy compile
scheduler in tiered mode. The executing thread claims the interpreter lazy unit
directly, translates it, publishes it, and continues with the interpreter. This
keeps the scheduler available for Tier 1 LLVM lazy demand work and Tier 2
background work.

Tier 1 demand compilation uses priority 1 or higher. Tier 2 full compilation is
queued at priority 0, so it stays behind demand work that is already visible to
the scheduler. The Tier 2 translation itself runs with zero nested compile
threads because it already occupies a runtime compile worker; this avoids
oversubscribing the machine while the main program is executing.

Tiered loop OSR can use a separate one-worker urgent scheduler when the module
is large enough for OSR polling, currently at least 512 local functions. Small
modules keep loop OSR on the normal scheduler to avoid adding another worker to
short benchmarks. Urgent requests are still counter-gated: each request carries
the function's OSR activity counter and a global urgent ticket. If later urgent
tickets overtake a request and that function has not refreshed activity beyond
one queued poll, the urgent worker demotes the request back to normal priority
1. This lets long-running loops keep the urgent lane while short loops that end
soon after requesting LLVM give the lane back.

The default Tier 2 codegen optimization level is `Less` (LLVM O1), matching the
lazy tier. `-Rllvm-opt` still overrides this, so experiments can compare O0,
O1, O2, and O3 without changing the tiered implementation.

## Runtime Logging

With `--runtime-compiler-log`, tiered summaries include:

- interpreter fallback counts and timing;
- sampled interpreter fallback and entry miss counters used by the large-module
  long-run gate;
- Tier 0 direct interpreter compile counts;
- loop OSR callback, ready, miss, and stall timing;
- loop OSR deferral counts from the counter-only request gate;
- urgent OSR request, demotion, ticket, and scheduler counters;
- Tier 2 request, ready, failed, and publish counts;
- Tier 2 translation, materialization, and publication time in nanoseconds.
- per-module counter summaries, including entry/OSR counters, thresholds, lazy
  materialization states, and compile-unit size buckets.

The log names use the `tiered_full_*` prefix for full-tier data and
`tiered_osr_*` / `tiered_int_*` for interpreter and OSR data.

## Correctness Rules

- The interpreter result is always used when LLVM native code is not ready.
- Native target tables are the only handoff mechanism between tiers; code does
  not patch machine code in place.
- Tier 2 never routes Wasm calls through the tiered bridge. It emits the normal
  full-module LLVM code path so direct calls stay direct.
- Tier 2 publication must happen only after all raw and typed function
  addresses are resolved and the LLVM execution engine ownership is stored in
  the module record.
- Lazy callbacks must not overwrite full-tier entries after
  `tiered_full_ready` is published.

## Tuning Notes

The current trigger waits for 65536 successful switches into native tiered code
before enqueueing Tier 2 for small modules. Modules with at least 512 local
functions require 1048576 switches, and modules with at least 1024 local
functions are kept out of full-module Tier 2. This keeps Tier 2 from stealing
CPU from hot large modules that are still benefiting from targeted lazy Tier 1
functions, while still promoting small workloads that repeatedly return to
native tiered code.

Tier 1 policy is counter-only: elapsed time is reported in logs, but it does
not decide when tiering happens. Ordinary function-entry misses require 4096
misses on the same local function before the bridge enqueues LLVM work for
small modules. The threshold scales up for large modules and large functions:
modules with at least 512 local functions require 16384 entry misses, modules
with at least 1024 local functions require 65536 entry misses, and modules with
at least 8192 local functions require 1048576 entry misses until the module is
classified as a large long-running module. Compile units of up to 128/512/1024
bytes can lower that large-module threshold back to 8192/16384/32768 misses
while the module has fewer than 4096 local functions, because tiny helpers are
cheap to materialize and often sit on hot indirect-call paths.

Modules at CPython scale, currently modules with at least 8192 local functions,
use sampled module-level counters to separate startup noise from true long
runs. A module enters the large-long-run state only after sampled interpreter
fallbacks or sampled entry misses reach 4194304, or after successful native
tiered switches reach 1048576. Once active, small top-hot compile units lower
their entry thresholds to 65536/131072/262144/524288 misses for code sizes up
to 128/512/1024/4096 bytes. Larger compile units keep the conservative
large-module thresholds. Compile units of at least 4096/8192 bytes require at
least 65536/262144 entry misses, and compile units of at least 32768 bytes are
kept out of entry-triggered lazy LLVM. This keeps short CPython startup scripts
from starting LLVM work, while allowing long-running Python loops to promote a
small set of top-hot functions without using elapsed time as a policy input.

Entry hotness counters are sampled for larger modules to keep the miss path
cheap while preserving a counter-based policy. Modules with fewer than 512 local
functions count every miss, modules with at least 512/1024/8192 local functions
use entry probe strides of 2/4/16 respectively. The counter is advanced by the
stride on sampled misses, so thresholds continue to be expressed as estimated
miss counts rather than elapsed time.

Loop and block OSR use mutable bytecode immediates as local counters. Loop
headers in functions of at least 4096 bytes poll after 4 iterations and then
retry every 64 missed polls, and require 512 expired polls before reaching the
runtime OSR gate. Functions of at least 1024 bytes poll after 16 iterations,
retry every 128 missed polls, and require 512 expired polls before reaching the
gate. Smaller functions poll every 1024 iterations and require 2048 expired
polls before reaching the gate. Block polls keep the older 8192-block cadence,
but use request-count thresholds of 512, 512, or 64 expired polls for large,
medium, and small functions respectively. Functions of at least 32768 bytes do
not get OSR polls, because their LLVM compile cost is too large to pay without
much stronger evidence than benchmark-sized loops provide.

After the bytecode-local OSR counter expires, a per-function runtime counter
decides whether to request LLVM. Small and medium modules request on the first
runtime OSR signal. Modules at CPython scale, currently modules with at least
8192 local functions, do not get Tier 0 OSR polls during interpreter bytecode
translation and cannot request LLVM from OSR; they rely on entry counters
instead. This avoids both the hot-loop poll overhead and the expensive
single-function LLVM materialization from Python bytecode loops that are shorter
than the compile cost. This is still counter-only policy; elapsed time is only
reported for offline calibration.

When a loop OSR request is queued, the interpreter continues to poll the
runtime through the same countdown path while the LLVM unit remains in the
`queued` state. These queued polls update the per-function OSR activity counter
but do not enqueue duplicate LLVM work. The urgent scheduler uses that activity
counter only as an ordering signal: a queued urgent request is considered stale
when at least four newer urgent tickets exist and the function has refreshed
the activity counter no more than once since enqueue. Stale urgent requests are
requeued on the normal lazy scheduler, so the LLVM work can still complete if
the program keeps running, but it no longer blocks the urgent lane.
