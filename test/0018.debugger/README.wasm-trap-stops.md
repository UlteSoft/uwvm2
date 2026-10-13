# Actual Wasm trap stops

`catch wasm trap MODULE FUNCTION|all` selects a confirmed runtime failure.
Unlike opcode catchpoints such as `catch wasm gc`, it fires after the helper
has detected failure. `trace wasm on trap` records these events as
`after-failure`. `catch wasm all` retains its existing before-instruction
meaning; it does not implicitly enable terminal trap stops.

Start an instrumented full LLVM JIT session with `-Rdbg`. A representative
session for module 0 is:

```text
catch wasm trap 0 all
trace wasm on trap
continue
status
operands THREAD 0 0 64
frames wasm THREAD STOP_ID 0 16
trace wasm read
continue
```

Use the actual thread and stop IDs reported by `status`. At a caught failure,
the console reports:

```text
stopped: Wasm trap after failure; pre-trap operands; continue terminates
Note: Pre-trap Wasm inputs; no instruction result.
```

The operand preview contains the exact saved inputs at the failing Wasm
opcode, with their Wasm types. It contains no result for the failed instruction.
Each borrow must match the current activation incarnation, module, function,
runtime epoch and sealed before-opcode site. This prevents stale data from a
previous recursive invocation from being presented as current state.

This stop is terminal. Read-only Wasm frame, operand, table and GC queries are
available. Stepping, executable checkpoint restore, replacement and mutation
are rejected. Continuing reaches the original fatal termination. The VM
helper's native PC/registers are not exposed as an ASM debugging endpoint.
This facility does not resume execution past a failed instruction.

The paired DAP adapter recognizes this stop as `exception` and labels its
operand scope `Wasm pre-trap inputs (no instruction result)`. Typed operand
packets preserve both the pre-trap notice and the existing last-safepoint
notice. A known unavailable operand reply still retains its pre-trap notice;
it remains unavailable and does not become an empty successful stack preview.
These notices do not grant a native register or memory capability.
`test_dap_wasm_trap.py` checks detached protocol data, including stale copies;
it is separate from actual VM capture qualification.

Instrumented JIT code uses explicit memory bounds checks instead of parking
inside an mmap fault signal handler. The uninstrumented mmap strategy remains
unchanged; this source-level separation is not a measured performance result.
Fatal VM invariants, cancellation, unavailable waits, queue limits and uncaught
exception propagation are excluded from this catchpoint. Authenticating a
before-opcode leaf is required; this is not coverage of every possible trap
path or all Wasm 3.0 debugging.

`run_wasm_trap_stops_cli.py` constructs and independently validates fifteen
fixtures: four dropped-element GC failures, unreachable, divide by zero,
integer overflow, memory bounds, table32/table64 empty access, table fill,
null reference, 72 recursive frames, retired tail-call activation and unaligned
atomic load. Every actual VM must first reach an ordinary opcode breakpoint,
then a distinct real trap stop with unchanged exact inputs. Trace records and
forbidden native queries are checked before genuine fatal retirement.

The runner accepts `--source-root`, `--binary`, `--wasm-tools`, `--out`,
`--jit-policy default|max`, repeated `--call-stack-policy instruction|unwind`,
`--runner-prefix-json` and repeated `--case`. Run compilation, validators,
VMs, QEMU and the harness only in the verified SSH Linux 64 GiB test cgroup,
under the shared resource supervisor. Do not execute these tests locally.

`run_wasm_recursive_trap_identity_cli.py` strengthens the recursive oracle:
the leaf input is i64 -991 while the immediate caller input is i64 -990.
Borrowing an otherwise similar packet from another depth must fail. The
qualified matrix includes twelve such recursive trap sessions. Normal
execution, in-flight waits and hot-replacement regressions also passed.

`run_wasm_memory64_trap_boundary_cli.py` adds four real failures: a load beyond
a one-page memory64 maximum, a load from zero-capacity memory64, an unsigned
`-1` address and an unaligned memory64 atomic load. It imports the explicitly
selected frozen trap runner and preserves its real process, validator, stop,
typed-value and terminal-native-command checks.

## Validation status

The implementation and fixtures are synchronized in uwvm2 and uwvm2-ros.
The pinned DAP notice cut passed 40 finite protocol tests per repository,
80 total, in the verified SSH Linux 64 GiB/swap-zero cgroup. This includes
unavailable pre-trap notes, strict packet parsing, scope paging and stale-stop
rejection. The final-r16 pinned adapter cut also passed these eighty tests.
See `documents/runtime/wasm-trap-memory64-runtime-20261008.json` for that
completed prefix; `wasm-trap-dap-protocol-20261007.json` preserves the earlier
protocol cut. Detached protocol tests alone do not qualify runtime capture.

The final-r10 targeted cut compiled and linked on x86_64, RISC-V64 and
AArch64 in both repositories (24 successful compiler/link roots). All 360
actual trap sessions passed, with 720 typed-value pauses. Independent metadata
closure passed in the verified cgroup after actual parent retirement; see
`documents/runtime/wasm-trap-runtime-20261008.json`. The full batch failed later
in a normal regression.
The memory32 blocked-wait cases passed, while four memory64 cases terminated
before the initial debug prompt despite the explicit memory64 feature flag.
This failed batch is preserved and must not be reported as fully qualified.

A paired follow-up limits the mmap memory64 reservation on 64-bit hosts to the
module's declared maximum, instead of requesting the entire 1 TiB ceiling for
a one-page maximum under the test process's 12 GiB address-space limit.
`mmap_memory64_declared_max.cc` exercises real shared/unshared instantiation,
growth boundaries, zero maximum, move and reinitialization. Actual GDB reproduced SIGILL
in `mmap_memory_t::init_by_page_count` before the debug prompt and traced
a 1,099,511,693,376-byte mmap request returning ENOMEM (-12). The two paired
backend regressions compiled and passed in the verified cgroup. Memory32
keeps its full guard domain. This reservation fix covers bounded, explicitly
declared maxima; unbounded or much larger reservations are not qualified.

The final-r16 source cut completed twenty-four fresh compiler/link commands
and eighty DAP tests. Its whole batch subsequently failed because the
supervisor executable allowlist still named an older product directory;
that failure and actual forced retirement remain preserved. Final-r17 reused
the six authenticated products with an explicit allowlist and passed 360
real trap sessions plus 84 regressions: 72 zero exits and twelve distinct
recursive failures. A separate supplement passed 96 memory64 boundary traps
with 192 typed-value pauses. Each matrix covers both repositories, native
x86_64, QEMU RISC-V64 and QEMU AArch64; trap cases cover default/max JIT and
instruction/unwind stack policies. All 540 VM lifetimes and all four runtime
and metadata outer roots were genuinely retired and reaped. See
`documents/runtime/wasm-trap-memory64-runtime-20261008.json`. All functional
execution and qualification audits used the verified 64 GiB/swap-zero Linux
cgroup and shared lock; cgroup memory events stayed zero.

The targeted trap cut includes the FastIO console fixes, the ROS-specific
bridge guard and only the paired table-width bridge change. In debug mode,
known engine-owned table helpers preserve the actual Wasm activation; their
native helper PC never becomes an ASM endpoint. Ordinary-mode call targets
stay the same. The trap fixtures explicitly enable table64, send expected
rejection queries through the real pipe without the positive-only send
assertion, and bind frame pages to their actual stop and thread. `physical=0`
denotes the current frame's ordinal; `total=72` denotes the chain length.
Both values are checked. The twelve distinct recursive caller-value
regressions passed on the three qualified architectures.

The targeted source cut creates hardlinks to unchanged immutable inputs and
removes changed destination links before copying, preserving every original
inode and byte. Captured paired DAP files are also part of that source manifest;
later working-tree edits remain outside this qualification. The scoped
runtime qualification above includes completed in-cgroup evidence closure.
It does not qualify all Wasm 3.0 features, all architectures, debug-off
performance or uncaught exception propagation. Call-dispatch coverage is the
finite supplement below.
Only this task's successful linker input objects are
retired; authenticated executables are compressed and materialized on demand
to stay within the private 16 GiB folder budget. The shared filesystem retains
at least 24 GiB free space; shared read-only SDKs are independently hash pinned.


## Call-dispatch failure supplement

`run_wasm_call_dispatch_trap_cli.py` adds fourteen independently validated
failures: `call_indirect` and `return_call_indirect`, table32 and table64,
each with out-of-bounds, null-element and signature-mismatch cases;
`call_ref` and `return_call_ref` with null typed references. The table64
bounds case uses index 4294967296 into a one-element table.

Each ordinary opcode pause and actual failure pause preserves all nine typed
input values: an i64/f32 prefix, six call arguments (i32, i64, f32, f64, v128
and externref), and the table selector or function reference. NaN payload,
negative-zero bits and all sixteen vector bytes are checked exactly. Live
table pages are checked at both pauses. Frame pages prove that the callee
was not entered and that a failing tail call did not retire its current frame.
The actual after-failure trace, terminal native-query rejection and genuine
fatal retirement retain the base runner's checks.

All 336 actual VM sessions passed, with 672 typed-value pauses, across both
repositories, native x86_64, QEMU RISC-V64 and QEMU AArch64, default/max JIT
and instruction/unwind stack policies. Runtime and independent metadata
outer roots were both genuinely PIDFD-retired and reaped zero. All execution
and qualification used the existing verified 64 GiB/swap-zero Linux cgroup
and shared lock; all cgroup memory events stayed zero.

This supplement uses the six authenticated final-r16 compiled products and
the explicitly selected frozen source, without a production C++ change or
a new compile. Dispatch checks already precede awaiting-call publication
and tail-frame retirement. It qualifies this finite producer cut, without
qualifying later concurrent source edits, uncaught exception snapshots,
all Core 3 features, all architectures or debug-off performance. See
`documents/runtime/wasm-call-dispatch-debug-runtime-20261008.json` for
actual process proofs, exact source/product hashes and bounded evidence.


## Uncaught exception supplement

A later paired frozen cut adds `catch wasm uncaught MODULE FUNCTION|all` and
`trace wasm on uncaught`. It observes an actual thrown guest exception before
any generated Wasm frame unwinds, checks real handler tag identity, and retains
live typed operands, locals, reference roots and deep caller frames. The terminal
stop remains readonly and provides no VM/helper ASM endpoint.

All 600 VM sessions passed on that new cut across both repositories and the
same three architectures and four policy combinations: 144 uncaught throws,
96 handled zero-exit controls and 360 fresh trap regressions. The matrix contains
1,104 typed pauses and 48 beyond-64-frame previews; a separate producer smoke
adds 10 distinct VM lifetimes and 16 typed pauses. Both runtime and independent
metadata roots actually retired and were reaped zero. See
`README.wasm-uncaught-stops.md` and
`documents/runtime/wasm-uncaught-debug-runtime-20261008.json` for commands,
source/product hashes, bounded process evidence and the remaining scope.
