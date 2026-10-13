# Uncaught Wasm exception stops

In an instrumented full LLVM JIT session (`-Rdbg`), use:

```text
catch wasm uncaught 0 all
trace wasm on uncaught
continue
status
operands THREAD 0 0 64
frames wasm THREAD STOP_ID 0 16
trace wasm read
continue
```

Use the actual thread and stop IDs from `status`. The stop reports:

```text
stopped: Uncaught Wasm exception before unwind; continue propagates
uncaught-thread THREAD
Note: Uncaught Wasm snapshot; stack has not unwound.
```

`catch wasm throw` observes a throw opcode before it executes. `catch wasm
uncaught` observes the actual constructed and thrown guest exception, before
any generated Wasm frame unwinds, after checking every authenticated active
frame for a matching handler. Matching uses actual tag identity, including
resolved imported tags; equal payload signatures do not imply equal tags.
A handled exception produces no uncaught event. `throw_ref` is attributed to
its current throwing activation, rather than an older stored throw trace.
`trace wasm on uncaught` labels the event `unhandled-before-unwind`.
`catch wasm all` keeps its existing opcode catchpoint behavior.

While parked, typed Wasm operand inputs, locals, caller frames and reference
roots remain alive. Read-only Wasm inspection is available. Continuing
propagates the original exception and reaches the normal unhandled-exception
termination. Stepping, replacement and Wasm mutation are rejected at this
terminal stop. The C++ exception wrapper is never an ASM endpoint: native
register and disassembly requests are rejected. The existing last-Wasm-
safepoint notice still distinguishes these Wasm values from native state.

The DAP adapter reports reason `exception`. Only the actual throwing thread
gets the `Wasm uncaught snapshot (before unwind)` operand scope title; other
threads retain their ordinary safepoint scope. Packet notes are retained for
unavailable views too, without converting them into successful empty stacks.
Stale stop IDs, duplicate notes and ambiguous participant identity are rejected.

The observer requires the complete current activation and typed-shadow
ledgers, current immutable plans, actual module/tag owners and a genuine
before-opcode leaf plus awaiting-call caller sites. An incomplete or stale
chain does not produce a claimed uncaught snapshot. Host-origin exception stops
remain unqualified. Defined-Wasm imports, quota refusal and real two-worker
embedding paths have separate, later frozen-cut qualifications; see
`README.wasm-imported-uncaught.md`, `README.wasm-debug-quotas.md` and
`README.wasm-threaded-uncaught.md` for their exact scope. Foreign native call
islands do not become complete Wasm captures from these qualifications.
With `-Rdbg` disabled, generated throws use the original ABI bridge targets;
this source-level separation is not a measured performance qualification.

`run_wasm_uncaught_cli.py` independently validates six uncaught fixtures:
exact numeric/SIMD payload bits, a live GC reference payload, 72 recursive
frames, a retired tail activation, a different tag with the same signature
in a caller, and `throw_ref` after an earlier handled throw. Four handled
leaf/caller tag and catch-all-reference fixtures must exit zero without an
uncaught stop or trace. Recursive inspection pages beyond ordinal 63 and
checks exact operands and locals at frames 64, 70 and 71. The runner checks
native-query rejection and actual process retirement, not only console text.

For a stop whose frame count exceeds 64, page the same live stop:

```text
frames wasm THREAD STOP_ID 64 8
operands THREAD 64 0 64
locals wasm THREAD 64 0 64
```

These are frame ordinals within that stop. Obtain a new `STOP_ID` after
continuing; a previous stop does not authorize access to later frame state.

All compiler, validator, VM, QEMU, protocol and qualification execution must
use the verified SSH Linux 64 GiB/swap-zero test cgroup and shared lock. Do
not run these tests locally. The runner accepts `--source-root`, `--binary`,
`--wasm-tools`, `--out`, `--jit-policy default|max`, repeated
`--call-stack-policy instruction|unwind`, `--runner-prefix-json` and `--case`.

## Validation status

The frozen uncaught-r6-build cut completed all 24 fresh compiler/link roots,
50 finite DAP tests per repository, and 10 real native smoke sessions in the
verified Linux cgroup. All actual compiler dependencies were hash bound before
compilation, including 2,674 additional standard/system headers. The protocol
tests use detached packets and do not qualify runtime capture.

The uncaught-r2-runtime continuation reused the six authenticated products.
All 600 real VM sessions passed across both repositories, native x86_64,
QEMU RISC-V64 and QEMU AArch64, default/max JIT, and instruction/unwind stack
policies: 144 actual uncaught exceptions, 96 handled zero-exit controls and
360 trap regressions. These contain 1,104 exact typed-value pauses and 48
deep previews beyond frame 63. The separate producer smoke adds 10 distinct
VM lifetimes and 16 typed pauses, for 610 lifetimes and 1,120 pauses in this cut.
Both producer roots and both runtime/independent-audit roots were genuinely
PIDFD-retired and reaped zero. All cgroup memory events remained zero; no
foreign process was adopted or signalled.

See `documents/runtime/wasm-uncaught-debug-runtime-20261008.json` for exact
source/product hashes, command and process evidence, and the bounded archive.
Earlier failed preparation, build and audit attempts remain recorded. The
runtime predecessor failed on a script quoting error before any VM launched;
the corrected continuation required no C++ rebuild. This separately frozen
cut does not qualify concurrent edits to either whole working tree, all Wasm
3.0 features, all architectures, imported/host-origin or threaded uncaught
paths, quota exhaustion, or debug-off performance.
