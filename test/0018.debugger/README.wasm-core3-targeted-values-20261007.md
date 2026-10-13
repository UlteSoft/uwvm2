# Wasm debugger: scalar atomics, relaxed SIMD and two participants

This is a targeted Wasm-level qualification. It uses no WASIp1 imports,
source-language DWARF, native register display or host/VM ASM stepping.
The two repositories carry the same fixtures and harness corrections.
The closed proof is `wasm_core3_targeted_values_qualification_20261007.json`.
All compilers, validators, LLVM/QEMU tools and tests ran in the verified SSH
Linux 64 GiB/swap-zero cgroup. Native products were freshly built from paired
source135; the RV64 extension uses the separately qualified immutable source111
products and exact ros.11 split-probe-CFA backport SDK/sysroot. Later concurrent
source changes are outside these cuts.

The batch passes 304 positive VM sessions, 9,772 exact-value pauses and 32
actual two-participant pause episodes (16 with atomic shared-memory workers).
Eight additional real-VM sessions test deliberately abnormal wrapper closure:
the VM first exits0, then the wrapper exits23 or misses the managed-quit
deadline. The corrected harness rejects every case. These negatives are not
included in the 304 positive sessions.

60 selected build/validation/test commands and a subsequent full closure audit
pass. The audit rehashes 46,710 inputs and 5,517 actual compile dependencies.
Observed owned RSS peak is 6,891,909,120 bytes; folder high-water is
16,946,155,520 bytes, below16 GiB. All max/OOM counters remain0. Cleanup retires
only this task's redundant transferred overlay and eight superseded failed
compiler objects; their hashes and failures remain recorded.

## Exact atomic stacks

`wasm_operand_atomic_cases.py` generates two independently assembled/validated
modules: shared memory with i32 addresses and shared memory64 with i64 addresses.
Each covers all 67 scalar atomic opcodes: seven loads, seven stores, seven
widths of each add/sub/and/or/xor/xchg/cmpxchg, wait32, wait64, notify and fence.

The actual CLI checks operand types/values before and after each operation,
preserves an i64 caller prefix, and checks memory readback after stores/RMW.
Signed full-width values, narrow zero extension, truncation, wrapping and both
successful/unsuccessful comparisons have concrete expected values. Each module
has 217 exact-value pauses. Wait cases cover mismatch and zero-timeout; notify
has no live waiters. These cases do not qualify blocking-wait debugging.

Example invocation, run inside the established Linux 64 GiB test cgroup:

```sh
python3 test/0018.debugger/run_wasm_operand_preview_cli.py \
  --source-root "$PWD" --binary /absolute/path/to/fresh/uwvm \
  --wasm-tools /absolute/path/to/wasm-tools --out /absolute/new/results \
  --case atomic-all-widths-i32-address \
  --case atomic-all-widths-i64-address --jit-policy max
```

Both instruction/unwind policies run by default. For a target product, add
`--runner-prefix-json /absolute/path/to/qualified-qemu-prefix.json` and retain
the actual target SDK/sysroot/product proof. A host SDK or a component-only
decoder build cannot replace that target VM qualification.

## All 20 relaxed SIMD opcodes

`wasm_operand_relaxed_simd_cases.py` checks the complete v128 operand bytes
before/after all 20 relaxed SIMD instructions, using uniquely determined
permitted results. Inputs avoid NaNs, signed-zero ties, invalid conversion or
swizzle ranges, partial lane masks and exceptional Q15/dot values. This makes
the expected bytes valid across different permitted target implementations.
The case has 42 exact-value pauses, including the preserved caller prefix and
the final empty stack.

Use `--case relaxed-simd-all-values` with the same runner. Default and max JIT
policies are separate runs. This does not constrain implementation-dependent
results for the deliberately excluded ambiguous inputs.

## Actual shared-memory participants and reference state

`debug_gc_state_two_participants_runtime.cc` has two additional opt-in modes:
`observe-atomic` and `checkpoint-atomic`. Use them with
`fixtures/debug_gc_state_two_atomic_participants.wat`. Two real native workers
enter one genuine full-LLVM instance and execute RMW on the same Wasm shared
memory. Each keeps its actual returned old value in original local[5].

The manager requires two current participants and two real pause episodes;
it queries both workers independently and checks the distinct consecutive
RMW values. It also checks typed operands/locals, GC cycles/aliases, arrays,
exception tag/payload, extern wrappers, member pagination and reference paths.
Incomplete/duplicate/forged capture cohorts and stale/resumed/retired stops
are rejected. Scheduler misses are bounded and cannot count as a successful
two-participant census. Threads actually join before reset or state reuse.

The baseline `observe` and default checkpoint-recording modes remain controls.
Checkpoint recording here supplies a compilation/capture contract; these
read-only state queries are not whole-world restore or replay qualification.
The fixture uses FastIO for file images, text output and numeric byte parsing.

## Harness correctness and scope

The Core 3 smoke matrix now requires real process exit 0 after managed quit.
A deadline/kill/nonzero exit fails the row and remains in the transcript.
Prompt failures retain a reachable child for cleanup; cleanup exceptions are
recorded instead of allowing a success marker to survive. The smoke matrix
still tests entry-state queries, three opcode steps and trace, rather than
exhaustive state support for every opcode in each feature family.

This change adds tests and corrects their success accounting. It changes no
production guest execution instructions. Neither these tests nor the snapshot
notice grant access to VM/host internals through ASM debugging.
