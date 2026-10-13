# Wasm debugging while atomic.wait is blocked

This targets the Wasm-level threads feature, independently of WASIp1 and
source-language debugging. It exercises both `memory.atomic.wait32` and
`memory.atomic.wait64`, with i32 and i64 memory addresses.

The debugger can pause an already linked wait and read its real typed locals,
operand snapshot, caller frame and GC references. For example, the CLI fixture
retains `[i64=1234, i32=16, i32=7, i64=1000000000]`: caller prefix, address,
expected memory value and timeout. The console prints:

```text
Note: Last Wasm safepoint snapshot; may differ from current native state.
```

The wait has already begun. These are the last Wasm opcode's values, rather
than the VM helper's current registers or a replayable before-opcode state.
Disassembly, native stepping and native finish at that in-flight stop are
refused. The test requires those refusals to preserve the current stop and
thread. A pre-opcode disassembly header checks code ownership only; unavailable
instruction rows are not counted as successful native decoding.

Use the actual thread ID shown by `status`:

```text
pause
status
operands THREAD 0 0 8
set wasm memory 0 0 THREAD 16 bytes 09
status
continue
quit
```

A successful memory edit advances the public stop ID. Subsequent state/native
authority must use that new identity. Editing memory does not restart the wait,
repeat its comparison, replace its linked node or reset its monotonic deadline.
The CLI regression holds a real pause past the original timeout and requires
the real result2 after resume, even though memory changed from7 to9.

`quit` while waiting now uses authenticated managed cancellation. After the
actual wait node, callbacks and locks have retired, a debug-only throwing
bridge invokes the existing private shutdown poll. Generated native cleanup
unwinds the real Wasm activation and GC frames to the full-entry boundary.
Cancellation supplies no fourth Wasm wait result. Unauthenticated cancellation
retains the original trap behavior.

A resumed infinite wait also needs a durable close hint. The pause domain
publishes its atomic closed state before waking registered nodes; the debug
wait policy observes pause-or-close without taking the domain mutex under the
wait-shard mutex. A wake notification alone can be lost when close clears the
pause flag. The hint grants no capture or shutdown authority. Ordinary waits
install no debug policy. The paired cohort fixture tests both delayed close
after resume (`shutdown`) and immediate close (`shutdown-immediate`).

The ordinary bridge remains the compile-time `ManagedShutdown=false`
specialization. The compiler selects the throwing bridge only for a genuinely
enabled debug activation with native cleanup. This is a source-level exclusion
of the new cancellation poll from ordinary execution, not a whole-VM timing
benchmark or an absolute performance guarantee.

Run these harnesses only inside the verified SSH Linux test cgroup:

```sh
python3 test/0018.debugger/run_wasm_blocked_wait_cli.py \
  --source-root "$SOURCE" --binary "$VM" --wasm-tools "$WASM_TOOLS" \
  --out "$OUTPUT" --scenario resume
```

Use a distinct output directory and `--scenario quit` for infinite waits.
For a qualified target VM, add `--runner-prefix-json "$PREFIX"`; its JSON
array must identify the actual QEMU executable, target sysroot and target
library path. `run_wasm_atomic_wait_plain_cli.py` checks ordinary full LLVM
execution without `-Rdbg`; add `--ros` for the ROS `-Raot` selection.

`debug_wasm_blocked_wait_cohort_runtime.cc` additionally has two real workers
blocked in nested Wasm frames. Each retains a distinct dynamic operand prefix,
caller parameter and GC struct. Both observation and checkpoint-recording
profiles are tested under instruction/unwind policies. Two real pause episodes
must have unchanged opcode callback counts. Incomplete cohorts and stale
captures are refused. Normal notify returns2 then0, and both waits return0.
Its optional final `shutdown` argument requires both physical joins, genuine
runtime quiescence and unchanged result buffers99/99.

`run_wasm_blocked_wait_eh_cli.py` defaults to its only supported scenario,
`quit`, and puts the infinite wait inside
Wasm `try_table (catch_all ...)`. The handler ends in `unreachable`: native
managed cancellation must bypass that handler, clean up and exit successfully.
The same four width combinations and both stack policies remain covered.

In-flight wait instance snapshots and saved-execution retirement remain
refused. These tests do not qualify whole-world checkpoint restoration,
external-I/O rollback, arbitrary thread schedules, all ISAs or later source
changes. The accompanying qualification records the exact paired source cuts,
products, SDKs, successes and preserved failures.

Qualified paired source212 on Linux x86_64 and GNU/QEMU RISC-V64: 320 new
actual VM sessions passed (192 two-worker cases, 64 blocked CLI sessions,
32 ordinary CLI controls, 32 lexical-EH quit sessions). A separate source135
baseline has 64 passing actual VM sessions. The native utility regression has
80 passing two-worker cells. These counts are separate from the parent proof.
Exact source identities, products and limits are recorded in
[`wasm-blocked-wait-qualification-20261007.json`](../../documents/runtime/wasm-blocked-wait-qualification-20261007.json).
The full qualification SHA256 is
`d95fbce7e595652679db48a5e6b6a84d7cf42ce37c18ca88a48cbe447c5b27bc`.
Later concurrent checkout changes are outside this frozen source cut.

The subsequent paired AArch64 full-LLVM qualification also checks real blocked
waits, EH cancellation, typed values and deep activations. Its counts are
separate from the native/RV64 result above. See
[`README.wasm-aarch64-debug-qualification.md`](README.wasm-aarch64-debug-qualification.md).
