# Live bulk-memory and table state debugging

The paired fixtures check real Wasm operand types and values before/after
bulk operations and read back the resulting memory bytes and function
references through actual guest instructions. They use no WASIp1 imports or
source-language/native-frame inspection. The new runner reuses the existing
strict CLI process lifecycle and exact operand assertions.

Each family has four cases: source and destination indices independently use
i32 or i64. A mutable i64 global (-991) and f32 global (-0, bits 0x80000000)
remain beneath every operation. The cases exercise both instruction/unwind
call-stack strategies and default/max LLVM policies.

| Family | Checked operations and results |
| --- | --- |
| Memory segments | Passive `memory.init`, `data.drop`, zero-length initialization at the one-past memory address after drop |
| Memory copying | Four source/destination width pairs and overlapping self-copy; exact byte readback |
| Memory filling | i32 fill value 511 truncates to byte 255; destination/count retain the memory's address type |
| Memory growth | Correct old page count, new size, failed growth result -1 and unchanged size |
| Table segments | Passive `table.init`, `elem.drop`, zero-length initialization at the one-past table index after drop |
| Table copying/filling | Four width pairs, overlapping self-copy, nullable function references and exact table.get readback |
| Table growth | Correct old size, function-filled new slot, failed growth result -1 and unchanged size |
| Table dispatch | Null table.set/get and a genuine indirect call through the grown slot returning 17 |

Mixed-width memory/table copies use a count of i32; copies between two i64
index spaces use an i64 count. Segment initialization keeps i32 segment offset
and count even when its destination address/index is i64. The independent
Wasm validator checks the generated modules before actual VM execution.

The memory fixture checks initial bytes [22, 33, 44, 55], overlapping-copy
bytes [22, 22, 33, 44] and filled bytes [22, 22, 255, 255]. Table fixtures use
two real Wasm functions with distinct indices/results. Known layouts determine
the expected values; the implementation under test does not generate the oracle.

## Run the matrix

Run inside the established SSH Linux 64 GiB/swap-zero cgroup and resource
supervisor, with a fresh results directory:

```sh
python3 test/0018.debugger/run_wasm_bulk_state_cli.py \
  --source-root "$PWD" --binary "$UWVM_BINARY" \
  --wasm-tools "$WASM_TOOLS" --out "$BULK_RESULTS" --jit-policy max
```

All eight cases and both call-stack strategies run by default. For a focused
mixed-width check, add `--case bulk-memory-dst-i64-src-i32` and
`--case bulk-table-dst-i32-src-i64`. To test one stack strategy, add
`--call-stack-policy instruction` or `--call-stack-policy unwind`.
For a qualified target VM, add `--runner-prefix-json "$QEMU_PREFIX"` and retain
that target's SDK, provider and full-product identities.

These are live operand checks plus guest-side state readback. They do not
qualify the debugger's table enumeration UI, traps after invalid/nonzero
access to a dropped segment, every typed GC table subtype, all Wasm 3.0
state combinations, whole-instance restore or debug-off performance.

## Qualified result: 2026-10-07

Both repositories pass 192 matrix VM sessions on Linux x86_64, QEMU RISC-V64
and QEMU AArch64, plus two focused mixed-width smoke sessions. There are
7,104 exact-value pauses in the matrix and 74 in the smoke, for 7,178 total.
All 16 paired generated modules pass independent assembly/validation.
The matrix has eight cases, two call-stack strategies, two JIT policies,
three architectures and two repositories. Every guest naturally exits 0;
the existing console lifecycle also requires real managed process exit 0.

All validators, QEMU and VM executions run inside the verified SSH Linux
64 GiB/swap-zero cgroup. The closure rehashes 30,200 pinned inputs and checks
the exact operand records, Wasm image bytes, root PIDFD retirement/reaping
and unchanged zero max/OOM counters. The sampled owned RSS peak is
312,025,088 bytes. Final observed owned-folder high-water is
14,397,915,136 bytes, below the 16 GiB admission/monitoring limit, and final
Linux free space is 67,885,056,000 bytes, above the 24 GiB floor. Foreign
processes were neither signalled nor adopted and their files were preserved.

This batch reuses separately qualified source212 full products and their
exact target-host ros.11 SDK/providers, rechecking their hashes before and
after the matrix. Its new source change adds paired tests; it changes no
production C++ execution path. ROS receives no basic modes. Later concurrent
production or helper changes are outside the qualified cut. Source212 IDs
and parent proofs are recorded in
[`wasm-bulk-state-qualification-20261007.json`](../../documents/runtime/wasm-bulk-state-qualification-20261007.json).

`QUALIFIED346.json` SHA256:
`3b82f92fa6eaa4e655fa53c1cdecb649579463b44c98f560ac4351864a5a8cfd`.
`delivery347.tar.gz` contains 642 source/proof/log files, is 7,740,857 bytes
and has SHA256
`c009f5b3717d73489ba636ed0235d12406dce2cb82fe2c1ebe9a8a4836330047`.
Every archive member was hash/size verified on Linux and after transport to
Mac; `DELIVERY348.json` separately closes the archive process after actual
retirement/reaping. Compiled products, objects and SDK/provider payloads
are not duplicated in the archive. Exact plans/profiles, frozen scripts,
module bytes and logs remain in the recorded Linux root.

## Direct debugger query follow-up

The separate [live-container matrix](README.wasm-live-containers.md) now
qualifies direct table pages and targeted GC members on the same source212
products across native x86_64, QEMU RISC-V64 and QEMU AArch64. Its scope and
evidence are recorded independently from the guest-readback matrix above.
