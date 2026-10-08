# Live GC array element-segment debugging

`run_wasm_array_elem_cli.py` checks `array.new_elem` and `array.init_elem`
through the real paused Wasm debugger. Its two fixtures independently cover
generic nullable funcref and nullable references to one declared function
type. Both reuse the strict operand/member query and process lifecycle checks.
There are no WASIp1 imports or source-language/native-memory queries.

The passive segment contains `[function 0, null, function 1, function 0]`.
Creation must preserve that layout in the operand preview and then through
local 0 and typed table element 0. A separate default array starts with four
nulls; initialization from segment offset 2 at destination 1 must produce
`[null, function 1, function 0, null]` without modifying the first array.
The fixture changes the last slot to function 1 and really calls the function
in slot 1 through `call_ref`, requiring result 29. Member queries check the
original function module/index and static nullable reference type throughout.

After `elem.drop`, zero-length initialization at the destination end succeeds
without changing the existing array. Zero-length `array.new_elem` produces a
non-null, correctly typed GC array with zero members, and `array.len` returns
0. It remains inspectable through the operand root and then local 2/table
element 2. Empty member pages query indices 0 with counts 64 and 1. The shared
CLI harness now handles this case without constructing a negative last-member
index. Nonempty arrays retain their full/last/end page checks.

Every query must preserve the genuine stop ID, every guest must naturally
exit 0, and the managed debug console process must exit and be reaped with 0.
Expected reference layouts and dispatch result are fixed fixture data, not
values obtained from a second implementation execution.

For these fixtures, after obtaining thread 1/module 0 from `status`, examples
at the appropriate opcode pauses are:

```text
members operands 1 0 0 0 2 0 64
members locals 1 0 0 0 1 0 64
members table 1 0 0 0 1 0 64
members locals 1 0 0 0 2 0 64
```

The first command reads the array freshly returned on operand index 2; the
last reads the stored empty array. Printed object numbers are local to one
response. Repeat queries using the original root indices.

Run only inside the verified SSH Linux test cgroup and resource supervisor:

```sh
python3 test/0018.debugger/run_wasm_array_elem_cli.py \
  --source-root "$PWD" --binary "$UWVM_BINARY" \
  --wasm-tools "$WASM_TOOLS" --out "$ARRAY_ELEM_RESULTS" --jit-policy max
```

Both instruction/unwind policies run by default. Select a single case with
`--case gc-array-elem-funcref` or `--case gc-array-elem-typed-funcref`. For a
qualified target VM, provide `--runner-prefix-json "$QEMU_PREFIX"`.

This matrix excludes nonzero access traps after dropping a segment, invalid
paths and stale handles, every GC subtype/cycle, other element reference
kinds, whole-instance checkpoint restoration and debug-off performance.

## Qualified result: 2026-10-07

Both repositories pass 48 matrix sessions plus two native smoke sessions,
using full LLVM JIT products on Linux x86_64 and QEMU RISC-V64/AArch64.
The matrix covers both instruction/unwind call-stack strategies and
default/max JIT policies. There are 750 positive exact-value pauses and
8,350 exact GC member pages. All four paired generated modules pass
independent assembly/validation. Every positive guest naturally exits 0;
the strict console lifecycle also requires managed process exit 0.

One additional real native VM uses the previously qualified member harness
as a negative control. The old harness actually sends
`members operands 1 0 0 0 2 -1 1` on the empty array and exits 1 as expected;
its managed debug VM still retires with 0. The corrected harness completes
the same case. Negative-control partial observations are separate from the
positive counts above.

All validators, VMs, QEMU and the old-helper control run in the verified
SSH Linux 64 GiB/swap-zero cgroup. The passing closure binds 30,311 inputs
and checks exact images/query records, genuine stop IDs and owned root
retirement/reaping. Max/OOM counters stay zero and foreign work is preserved.

This qualifies the frozen source212 products. Later working-tree changes
and full Wasm 3.0 coverage remain outside the result. Production C++ is
unchanged; the synchronized fix is the test harness's empty-member window.
The fixture, new runner, shared-harness fix, documentation and result record
are paired in uwvm2 and uwvm2-ros.

The 8,350 positive member pages include 700 exact empty-array pages. Final
owned-folder high-water is 14,662,692,864 bytes
(below 16 GiB), and Linux free space is 64,389,926,912 bytes
(above the 24 GiB floor). The observed owned RSS peak is
309,760,000 bytes.

- Passing closure: `QUALIFIED365.json`, SHA-256
  `e638beb30fa9fc325a81fff0e4896cbad45b2819f9a64937e3c262fd0e03804d`.
- Evidence: `delivery366.tar.gz`, 9,033,271 bytes/283 members,
  SHA-256 `77173f00ea1f9aa25669dababf7c01ba1b939db0cfe978999010eefb099df444`.
- Paired record:
  `documents/runtime/wasm-array-elements-qualification-20261007.json`.
- Archive transport: `DELIVERY370-local-verification.json`; all member sizes
  and hashes checked locally as metadata, with no local test execution.

Follow-up: [empty tables, dropped-element traps and PIDFD lifecycle checks](README.wasm-container-boundaries.md) are separately qualified on frozen source212. Actual after-failure trap pause remains outside that result.
