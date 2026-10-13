# Empty tables and dropped-element traps

These runners inspect actual paused Wasm state through the CLI, independently
of language, ASM and WASIp1 debugging. Run them only in the verified SSH Linux
test cgroup under the shared resource supervisor.

`run_wasm_empty_table_cli.py` uses table32 and table64 with minimum 0 and maximum
2. It checks empty pages, `table.size`, zero-length `table.fill`/self-copy and
initialization from a dropped segment. Growth from 0 to 1 returns 0; an attempt
to exceed maximum 2 returns -1 and leaves the element unchanged; growth to 2
returns 1; zero growth returns 2. The function in element 0 is really dispatched
with `call_indirect`, requiring result 17. Every pause checks the scalar operand
prefix and current table elements. Empty pages use first 0/counts 64 and 1;
nonempty pages retain full/middle/last/end checks. Read-only queries must keep
the genuine stop ID, and successful guests and managed consoles must exit 0.

`run_wasm_dropped_elem_traps_cli.py` independently checks generic and typed
nullable function references with both `array.new_elem` and `array.init_elem`.
After dropping a one-element passive segment, accessing one element from offset
0 must fail. A breakpoint first inspects the operand stack, then a genuine GC
catchpoint pauses immediately before the failing opcode. Both pauses check the
exact types/values and the existing one-null array through local/table roots.
The second stop must have a new genuine stop ID. The catchpoint is disabled
before resuming to the actual runtime failure.

This is a before-instruction catchpoint, **not an after-failure trap pause**.
The qualified source212 trap path prints the Wasm diagnostic and invokes FastIO termination.
The runner requires `Runtime crash (array access out of bounds)` and the exact
target trap signal: SIGILL for these Clang x86_64/RISC-V64 products, SIGTRAP for
AArch64. A timeout, another signal, a successful guest exit or a missing/wrong
diagnostic fails. The process is genuinely waited/reaped; expected fatal exits
are reported separately from successful zero exits. Core dumps are disabled
to avoid filling the test folder. These cases do not establish post-trap state
inspection or recovery.

The common CLI console now holds an actual Linux PIDFD for each VM before
sending debugger commands. Admission binds the PID/birth, parent, UID, cgroup,
CPU affinity and running executable hash. Natural quit and expected fatal
termination both require that same PIDFD to become readable after the real
Popen wait/reap. The transcript ends with a `managed-vm-retirement` JSON proof;
this is harness metadata and is not output produced by the VM console. Timeout
cleanup signals the owned PIDFD. The batch driver independently stops each
fresh command root before its payload, acquires its PIDFD, verifies confinement,
then resumes it and checks retirement. This covers short-lived roots missed by
an outer periodic process scan.

For the generated dropped-segment fixtures, after reaching the first breakpoint
and obtaining the actual thread from `status`, representative commands are:

```text
catch wasm gc 0 1
continue
status
operands 1 0 0 64
members locals 1 0 0 0 0 0 64
members table 1 0 0 0 0 0 64
disable wasm-event 1
continue
```

The last continuation terminates the VM on the real out-of-bounds trap. The
thread/catchpoint IDs shown are fixture examples; use actual IDs from replies.

Both runners support `--source-root`, `--binary`, `--wasm-tools`, `--out`,
`--jit-policy default|max`, repeated `--call-stack-policy instruction|unwind`
and `--runner-prefix-json`. Select a fixture with repeated `--case` arguments.
Examples of case names are `empty-table-i32`, `empty-table-i64`,
`dropped-elem-new-generic`, `dropped-elem-new-typed`,
`dropped-elem-init-generic`, and `dropped-elem-init-typed`.

The shared empty-table window fix and both new runners are synchronized in
uwvm2 and uwvm2-ros. Production C++ is unchanged. This targeted work does not
establish all Wasm 3.0 debugger features, all Linux architectures, whole-instance
checkpoint restoration or debug-off performance.

## Qualified result: 2026-10-07

Both repositories pass 144 matrix sessions plus six native smoke sessions,
using the frozen source212 full LLVM JIT products on Linux x86_64 and QEMU
RISC-V64/AArch64. The matrix covers instruction/unwind call-stack strategies
and default/max JIT policies. Fifty guests naturally exit 0; 100 cases produce
the exact expected Wasm diagnostic and target fatal signal. There are 750
exact pre-opcode value pauses, 1,700 table pages (500 empty) and 1,200 GC member
pages. All twelve paired generated modules are independently validated.

One further real VM runs the unchanged old table-window logic with the new
PIDFD console as a negative control. Its first valid empty page succeeds; its
actual `table 1 0 0 1 2` request is rejected and the old runner exits 1. Its VM
is genuinely quit/reaped with 0. An earlier matrix passed functional checks
but failed lifecycle auditing because an outer periodic scan missed ten short
command roots. That failure is retained; the final rerun explicitly owns the
VM and command-root PIDFDs instead of depending on scan timing.

Two additional native recovery cases use `run_wasm_managed_timeout_cli.py`, one
per repository. Each actual prepared VM is stopped through its retained PIDFD,
then a real `close()` waits for its 30-second quit deadline. It raises the
expected timeout error, sends SIGKILL through the same PIDFD and genuinely
waits/reaps exit -9. Observed waits are 30.001 and 30.004 seconds. No guest
function executes. These expected recovery failures are separate from the
50 successful guests and 100 out-of-bounds traps. All 153 VMs have exact
PIDFD/birth/confinement/image and retirement proofs.

Every compiler/validator/VM/QEMU/test invocation runs in the verified SSH Linux
64 GiB/swap-zero cgroup. The final boundary closure checks 30,401 immutable
input hashes and the genuine stop IDs, query records and process lifetimes.
Max/OOM counters remain zero; foreign processes are neither adopted nor
signalled. Local archive/source verification is metadata only, with no local
compiler, validator or VM execution.

This follow-up changes test infrastructure only. It qualifies source212,
not subsequent working-tree edits or all Wasm 3.0 debugging. In particular,
the observed trap path terminates after continuation: actual after-failure
trap pause/state inspection is still unsupported by these products.

Final observed owned-folder high-water is 15,783,518,208 bytes (below 16 GiB);
Linux free space is 62,940,336,128 bytes (above the 24 GiB floor). The observed
owned RSS peak for the boundary qualification is 549,167,104 bytes.

- Passing boundary closure: `QUALIFIED.json`, SHA-256
  `631496fca0bc7d6952e27ea2251e84cc5c1cc9842f75265d4a84628b8763f574`.
- Main evidence: `evidence.tar.gz`, 9,267,903 bytes/1,161 members, SHA-256
  `6f850fbf7b490a63f1a2d883fb63e1b59148a3ca598412e11479f4799aeaf2a8`.
- Real timeout closure: `TIMEOUT-QUALIFIED.json`, SHA-256
  `42a40bbb04651f37b465d48e44a65280caba462eb4b4f95d7671204e8af47240`.
- Timeout evidence: `timeout-evidence.tar.gz`, 1,460,606 bytes/19 members,
  SHA-256 `54e811058ab403894c3fd59d24d31406010b0894c3d953672ac599b8d4693428`.
- All 1,180 archive member sizes/hashes and twelve paired executed source
  hashes are verified locally in `LOCAL-VERIFICATION-FINAL.json`.
- Paired result: `documents/runtime/wasm-container-boundaries-qualification-20261007.json`.
