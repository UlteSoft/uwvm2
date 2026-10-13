# Live Wasm tables and GC members

`run_wasm_live_containers_cli.py` checks debugger queries at genuine Wasm
opcode stops after guest mutations. It also retains the strict typed operand
checks and requires natural guest exit 0 and managed console process exit 0.
No source-language information, WASIp1 imports or native addresses are needed.

The seven cases cover four independent i32/i64 source/destination table index
pairs, packed i8/i16 arrays, and an array with two references to one mutable
structure. Expected values come from fixed layouts in
`wasm_live_container_cases.py`, rather than another VM execution.

The table cases directly query both tables before/after `table.init`,
`table.copy` (including overlapping copy), `table.fill`, `table.set`, successful
and failed `table.grow`, and zero-length initialization after `elem.drop`.
Queries check original element indices, current total, a full page, an interior
page, the final element and an empty page beginning at the current total.

The GC cases check `array.new_fixed`, `array.set`, `array.fill`, overlapping
`array.copy`, `array.new_data`, `array.init_data`, and zero-length initialization
after `data.drop`. Both the local root and the typed table root must show the
current array contents. The packed member display is unsigned storage data
(`packed-u8` / `packed-u16`); separate `array.get_s` and `array.get_u` stops
check the signed and unsigned Wasm results. The reference-array case mutates
one structure and expands both array paths to that same structure, then checks
null filling and restoring a reference. Every member query checks the exact
object kind, runtime type, member indices, current values and mutability.
Member pages include the full page, final member and empty end page. All reads
must preserve the real stop ID.

At a pause, obtain the thread and module from `status`. For these particular
fixtures, thread 1/module 0 give these example commands:

```text
table 1 0 1 0 64
members locals 1 0 0 0 0 0 64
members table 1 0 0 0 0 0 64
members locals 1 0 0 0 1 0 64 2
```

The last command follows element 2 of the array in local 1 and reads that
structure's fields. Member grammar is
`members SELECTION THREAD MODULE FRAME TABLE ROOT FIRST COUNT [PATH...]`;
use zero for FRAME/TABLE where unused, and original indices for ROOT/PATH.
Object numbers printed in one response are response-local labels; use the
original root/path for subsequent requests.

Run inside the verified SSH Linux test cgroup with the resource supervisor:

```sh
python3 test/0018.debugger/run_wasm_live_containers_cli.py \
  --source-root "$PWD" --binary "$UWVM_BINARY" \
  --wasm-tools "$WASM_TOOLS" --out "$CONTAINER_RESULTS" --jit-policy max
```

Both instruction/unwind call-stack policies run by default. `--case` selects
a named case, `--call-stack-policy` selects one stack policy, and
`--runner-prefix-json` supplies a qualified QEMU prefix for a target VM.

This scope does not include every GC subtype/opcode combination, arbitrary
GC cycles, invalid-path/stale-handle refusals, table pagination past index
2^32, whole-instance checkpoint restoration or debug-off performance.

## Qualified result: 2026-10-07

Both repositories pass all 168 matrix VM sessions (seven cases, two stack
policies, two JIT policies, three architectures), plus four native smoke
sessions. Linux x86_64 and QEMU RISC-V64/AArch64 run the same exact-value and
query oracles on full LLVM JIT products. There are 4,248 matrix and 63 smoke
pauses, 29,488 table pages and 6,000 GC member pages in total. All 14 paired
modules pass independent assembly and validation. Every guest exits naturally
with status 0, and all managed VM processes retire and are reaped with status 0.

All validators, VM and QEMU execution use the existing verified SSH Linux
64 GiB/swap-zero cgroup. The closure checks 30,264 preexecution bindings,
complete query records, exact Wasm image bytes and process retirement. One
generated preparation-status file was originally pinned while running; the
successor closure reconstructs that exact original hash and verifies its
normal completed-status transition. The first failed metadata closure and its
receipt remain in the evidence; no source/product change or VM retry was
needed. Immutable input hashes are rechecked normally.

The observed owned RSS peak is 316,936,192 bytes. Final owned-folder high-water
is 14,441,992,192 bytes, below 16 GiB; Linux free space is 66,154,160,128 bytes,
above the 24 GiB floor. All max/OOM counters stay zero. Foreign processes and
files are preserved. The compact evidence archive is 8,131,386 bytes.

This qualifies the frozen source212 products, not later working-tree changes.
Production C++ was unchanged: the tested implementation already provides the
expected behavior. The main/ROS fixture, runner, documentation and result
record are synchronized.

- Result: `QUALIFIED357.json`, SHA-256
  `21cb1b3150f0fbf24e08130a5aa03401e9bd8e1adb12bfe9c9407e1e697b5646`.
- Evidence: `delivery358.tar.gz`, SHA-256
  `9dee1a63d01b3de15c52b3003ca28bad78084d56bce4968a1c862ecc199dbbca`.
- Paired compact record:
  `documents/runtime/wasm-live-containers-qualification-20261007.json`.

## Empty-array and element-segment follow-up

The [array element-segment matrix](README.wasm-array-elements.md) qualifies
array.new_elem/array.init_elem with generic and typed function references,
including non-null zero-member arrays after elem.drop. The shared member
harness now avoids a negative last-member index on empty arrays; an actual
old-harness VM control demonstrates the original failure.

Follow-up: [empty tables, dropped-element traps and PIDFD lifecycle checks](README.wasm-container-boundaries.md) are separately qualified on frozen source212. Actual after-failure trap pause remains outside that result.
