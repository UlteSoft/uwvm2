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
  transitions into native code. Tier 2 is requested after a small native-switch
  threshold, and the request has background priority so it cannot overtake a
  demand LLVM lazy request.

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

Tier 1 demand compilation uses priority 1 or higher. Tier 2 full compilation is
queued at priority 0, so it stays behind demand work that is already visible to
the scheduler. The Tier 2 translation itself runs with zero nested compile
threads because it already occupies a runtime compile worker; this avoids
oversubscribing the machine while the main program is executing.

The default Tier 2 codegen optimization level is `Less` (LLVM O1), matching the
lazy tier. `-Rllvm-opt` still overrides this, so experiments can compare O0,
O1, O2, and O3 without changing the tiered implementation.

## Runtime Logging

With `--runtime-compile-log`, tiered summaries include:

- interpreter fallback counts and timing;
- loop OSR callback, ready, miss, and stall timing;
- Tier 2 request, ready, failed, and publish counts;
- Tier 2 translation, materialization, and publication time in nanoseconds.

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

The current trigger is intentionally modest: after 64 successful switches into
native tiered code, the module enqueues Tier 2. This avoids forcing a full LLVM
materialization for very short programs while still promoting workloads that
return to tiered native code repeatedly. If long single-entry kernels need faster
promotion, the trigger should become a combined condition based on native
execution time, switch count, function count, and remaining lazy queue depth.
