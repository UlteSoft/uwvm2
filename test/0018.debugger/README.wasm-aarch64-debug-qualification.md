# AArch64 full-LLVM Wasm debugger qualification

Both repositories pass 366 actual VM sessions on Linux under QEMU AArch64:
360 in the base batch and six in a separate post-reboot continuation.
All compilers, LLVM tools, validators, QEMU and VM tests ran in the verified
SSH Linux 64 GiB, swap-zero cgroup. This qualifies the immutable source212
production cut plus the separately pinned EH runner correction. Concurrent
checkout changes are outside this qualification. ROS retains full interpreter
and full LLVM modes; this task adds no basic modes or production C++ changes.

The machine-readable record is
[`wasm-aarch64-debug-qualification-20261007.json`](../../documents/runtime/wasm-aarch64-debug-qualification-20261007.json).

The following table records the 360-session base batch.

| Actual VM sessions | Count | Verified behavior |
| --- | ---: | --- |
| Two-worker blocked waits | 96 | 192 actual workers, 192 cohort pause episodes, 384 worker pause observations; typed prefixes, GC locals, nested frames, notify and private shutdown |
| Debug CLI waits | 64 | Resume, quit and EH private cancellation; four address/compare-width combinations and two stack policies |
| Ordinary execution without `-Rdbg` | 16 | Actual wait results 0/1/2 and exit 0; these are functional controls, not a performance benchmark |
| Typed value cases | 160 | 4,336 exact-value pauses across 20 cases, two stack policies, default/max JIT and both repositories |
| Deep activations | 24 | 63, 64, 65, 128, 1,024 and 10,000 real frames, both stack policies and repositories |

The value cases cover all 67 scalar atomic opcodes with i32/i64 addresses and
all 20 relaxed SIMD opcodes using inputs with uniquely permitted results.
They also check scalar bit patterns, v128 bytes, GC/reference kinds, branch
and loop prefixes, multivalue calls, tail calls, exception payload/rethrow,
nondefaultable locals, memory64, multiple memories, table64, GC casts/fields,
300-value pagination and 72-frame identity previews. This is a targeted finite
matrix, not exhaustive state coverage of every Core 3 feature.

Deep activation tests query live typed values in leaf and ancestor frames,
page past the old 64-frame boundary, refuse replacement of active ancestors
and the active root, accept a validated inactive-function replacement, and
continue after Wasm over/out. The base batch excluded the 4,097-frame native
instruction variant. A separate continuation now passes this depth with
instruction, unwind and instruction-native policies in both repositories.
The two actual native pauses refuse stale Wasm operands, display the last
Wasm safepoint notice and continue to real exit 0. Full physical ASM parity
remains outside these deep-stack tests.

During a real blocked wait, the original compare value and deadline survive
pause and memory editing. A one-second wait expires during a longer debugger
pause and returns timeout 2 after resume. Infinite-wait quit uses private
native cancellation, bypasses lexical Wasm `catch_all`, joins workers and
reaches actual quiescence without fabricating a fourth Wasm wait result.

ASM operations can disassemble generated Wasm at the pre-opcode pause.
When execution is in the VM wait helper, disassembly, native step and native
finish are refused while retaining the same stop/thread. The debugger emits:

```text
Note: Last Wasm safepoint snapshot; may differ from current native state.
```

This snapshot represents the last Wasm safepoint. It does not authorize access
to VM/host code or claim an exact current native operand stack.

## EH regression entry correction

`run_wasm_blocked_wait_eh_cli.py` requires an infinite wait and terminal private
cancellation. Its old `resume` default failed its own assertion before a VM
could start. Its default and only accepted scenario are now `quit` in both
repositories. Two frozen old-default invocations reproduce that failure;
16 actual new-default VM runs pass. Existing explicit-quit runs remain valid.

## Reproduction

Execute these commands only inside the established Linux test cgroup and
resource supervisor. Use fresh output directories and the recorded target
SDK, providers, products and QEMU prefix. The archive retains the exact plans,
profiles and supervisor; do not replace its target SDK with a host SDK.

```sh
DBG_ROOT=/home/macromodel/Documents/uwvm-debug-recovery-20261006-r2/wasm-core3-debug-20261007-r1
python3 "$DBG_ROOT/test146/uwvm2/run_wasm_operand_preview_cli.py" \
  --source-root "$DBG_ROOT/source212/uwvm2" \
  --binary "$DBG_ROOT/aarch289/products/uwvm2/uwvm" \
  --wasm-tools "$DBG_ROOT/../assets/wasm-tools/wasm-tools" \
  --runner-prefix-json "$DBG_ROOT/aarch289/qemu-prefix.json" \
  --out "$DBG_ROOT/repro-aarch64-values-new" --jit-policy max \
  --case atomic-all-widths-i32-address --case atomic-all-widths-i64-address \
  --case relaxed-simd-all-values

python3 "$DBG_ROOT/source212/uwvm2/test/0018.debugger/run_wasm_deep_stack_cli.py" \
  --source-root "$DBG_ROOT/source212/uwvm2" \
  --binary "$DBG_ROOT/aarch289/products/uwvm2/uwvm" \
  --wasm-tools "$DBG_ROOT/../assets/wasm-tools/wasm-tools" \
  --runner-prefix-json "$DBG_ROOT/aarch289/qemu-prefix.json" \
  --out "$DBG_ROOT/repro-aarch64-deep-new" --total 65 --total 4097 --total 10000

python3 "$DBG_ROOT/test299/uwvm2/test/0018.debugger/run_wasm_blocked_wait_eh_cli.py" \
  --source-root "$DBG_ROOT/source212/uwvm2" \
  --binary "$DBG_ROOT/aarch289/products/uwvm2/uwvm" \
  --wasm-tools "$DBG_ROOT/../assets/wasm-tools/wasm-tools" \
  --runner-prefix-json "$DBG_ROOT/aarch289/qemu-prefix.json" \
  --out "$DBG_ROOT/repro-aarch64-eh-new"
```

Replace the repository component and product with `uwvm2-ros` for the paired
ROS test. Both instruction/unwind stack policies run by default.

## Evidence and limits

The target-host LLVM SDK is freshly built ros.11, with 65 static archives and
1,663 verified AArch64 ELF members. Both VM products and all runtime/host/main
and cohort objects are fresh AArch64 builds. The closure audit checks 39,600
pinned inputs and 4,527 unique actual compiler dependencies.

Audit307 found five generated LLVM Config `.def` files missing from the
observed SDK manifest and exited 1. Audit308 exactly regenerates their bytes
from pre-pinned templates and AArch64 CMake configuration. These five identities
are source-derived, not observed pre-compilation generated-file hashes. The
failed audit, original files and successful successor evidence are retained.

`QUALIFIED308.json` SHA256:
`02d5199dd68adac2dec4e9a9d25711d8e6827e93e788ab79aca0bbddea15df58`.
`delivery318.tar.gz` contains 7,519 source/proof/log files, is 18,677,544 bytes,
and has SHA256
`e68ca4bd2e27ab8ddb98f9750cbb3d026955b224e021b12a053469cc3060edf4`.
Every member was hash/size verified on Linux and after transfer to Mac. It
excludes duplicated SDK archives, providers, object and VM payloads.
`DELIVERY319.json` separately records the real exit/reaping of the package root.

Sampled owned RSS peaks at 8,052,838,400 bytes. The final observed owned-folder
high-water is 14,355,587,072 bytes, below the 16 GiB admission/monitoring limit.
Final Linux free space is 67,946,020,864 bytes, above the 24 GiB floor.
Cgroup max/OOM counters remain zero. These are sampled RSS/cgroup and disk
checks; the wrapper's address-space parameter is not an enforced QEMU VM cap.
No foreign process was adopted/signalled or foreign folder removed.

Full native ASM parity, exhaustive Core 3 debugging, other ISA full-VM
qualification, whole-instance restoration/external-IO rollback and debug-off
performance qualification remain outside this result.

## Post-reboot continuation

Linux rebooted before the additional deep-stack tests. The old birth/boot
bound supervisor refused before creating a test child; that failure is retained.
The existing container was already restored. A successor changes only the
boot/keeper binding, retains all resource/ownership rules, and reuses the
64 GiB/swap-zero cgroup with keeper PID 11,166, birth 22,661 and boot ID
`c8d3550f-3a19-40d7-8507-a76c2045ace5`. It rehashes 26,250 inputs before
execution and again at closure. No compiler or VM ran outside the cgroup.

`QUALIFIED326.json` records six additional actual VM sessions, including two
real native instruction stops at 4,097 frames. Its SHA256 is
`be36c44fe2f7fc1c41792e7b043258568cd9dba24efb9599f57452c5647bf8dd`.
`delivery329.tar.gz` contains 64 additional proof/log/script files, is
1,299,752 bytes and has SHA256
`6cd001072d1cbf060f881add213ca80de3eb3c4d087c32c59d8ad0734cffe41d`.
Linux and Mac verified every member; `DELIVERY330.json` closes the package
root after actual retirement/reaping. The final observed owned folder is
14,371,196,928 bytes and final Linux free space is 70,073,245,696 bytes.
This continuation has zero max/OOM counters and no foreign signal/adoption.
It preserves the base qualification and does not qualify later peer changes.
