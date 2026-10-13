# Imported tags and cross-module uncaught stops

`wasm_uncaught_import_cases.py` defines eight independent Wasm module graphs.
`run_wasm_uncaught_import_cli.py` executes the real LLVM-full debug console and
checks both instruction and unwind call-stack policies. Run it only in the
verified Linux test cgroup with the shared test/build lock and an owned process
supervisor.

Export `UWVM_TEST_CPUSET` with the actual verified effective cgroup CPU list
before invoking the harness. The guard checks the running process's membership,
memory/swap limits and exact CPU list.

The cases cover an imported tag, a re-exported tag, equal signatures with
different tag identities, ordinary cross-module calls, a forwarded tail call,
and matching `catch_ref` / `catch_all_ref` handlers. Every module is parsed and
validated independently before VM execution.

At the leaf `nop`, and again at an actual uncaught throw before unwind, the
runner checks each live frame's exact module/function, generation, operand
values and locals. It derives module IDs from actual console replies; registry
ordering is not part of the oracle. A forwarding module creates no defined
activation, and the tail-call middle frame must have retired. Readonly queries
must preserve the stop identity. The fatal diagnostic must identify the real
provider `P` as the tag owner. Handled controls must exit zero without an
uncaught stop or trace event.

```sh
python3 test/0018.debugger/run_wasm_uncaught_import_cli.py \
  --source-root /absolute/frozen/uwvm2 \
  --binary /absolute/qualified/uwvm \
  --wasm-tools /absolute/wasm-tools \
  --out /absolute/new-result-directory \
  --jit-policy default
```

QEMU runs additionally supply `--runner-prefix-json` containing the qualified
emulator and sysroot arguments. The harness records executable hashes, the
actual cgroup, and PIDFD admission/retirement evidence for every VM.

Tag-only and import-forwarding modules have no native LLVM engine. Their
actual validated source and GC store must still participate in value
observation. Only the empty native-code extent waives engine/context presence;
modules with definitions continue to require genuine native code ownership.
Authentic defined-Wasm import forwarding preserves the Wasm activation chain;
foreign providers keep the host boundary. An uncaught stop permits inspection
and continuation, and refuses ASM stepping/register/disassembly requests from
the native exception wrapper.

This finite suite does not establish host-origin or threaded exceptions,
quota exhaustion, every Core 3 feature, every architecture, or measured
debug-disabled performance. Qualification results are published separately
after actual process retirement and source/product binding have been checked.

The 2026-10-08 qualification completed 192 imported-tag/cross-module VM runs
across both repositories, x86_64, RISC-V64 and AArch64, default/max JIT and
instruction/unwind policies. Another 100 native exception/trap regressions
passed, including the 72-frame case. All 292 actual VM lifetimes and 472 typed
pause observations passed. The producer additionally completed 24 fresh
compiler/link commands and 100 detached DAP tests. All functional execution
and qualification audits ran in the shared verified 64GiB/swap0 Linux cgroup.

See [the qualification report](../../documents/runtime/wasm-imported-uncaught-runtime-20261008.json)
for the exact source identities, products, retired process roots, resource
limits and remaining scope. It qualifies the fixed source cut recorded there;
later concurrent working-tree changes are outside this result.
