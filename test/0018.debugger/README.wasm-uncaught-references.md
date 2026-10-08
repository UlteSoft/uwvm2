# Cross-module reference payloads at real Wasm debug stops

`wasm_uncaught_reference_cases.py` supplies eight module graphs to
`run_wasm_uncaught_reference_cli.py`. Every module is independently parsed and
validated before it executes. Run the harness only inside the verified Linux
test cgroup, under the shared test/build lock and owned process supervisor.
Set `UWVM_TEST_CPUSET` to that cgroup's verified effective CPU list.

The exception tuple includes a struct, a packed i16 array, a negative i31,
a null extern reference, a Wasm-created extern wrapper and a function reference.
The struct and wrapper share the same underlying struct; the struct's second
field points to the same array that appears directly in the tuple. The runner
checks exact declared types, module provenance, query-local alias identity,
packed values and mutability. An exception's payload is immutable even when
the objects it references have mutable fields.

The cases cover a tag-only provider, a re-exported tag, a different tag with
the same signature, ordinary cross-module calls, a forwarded tail call,
matching handlers and cross-module `throw_ref`. The caller has its own GC
struct with a different value, so substituting the callee's frame cannot pass.
The rethrow case keeps a provider-created exception and GC objects in the
consumer's operand stack and locals after the provider's activation returns.

At the leaf `nop`, and at the genuine uncaught stop before unwind, the runner
checks all live frames and expands original operand/local roots. Full member
pages, final-member pages and empty end pages are checked. Read-only commands
must preserve the exact stop identity. Handled controls must exit zero without
an uncaught stop or trace; unhandled controls must emit the actual provider's
tag identity and terminate with the target's fatal signal after continuation.
Native ASM/register/disassembly commands remain refused at the uncaught
exception wrapper.

```sh
python3 test/0018.debugger/run_wasm_uncaught_reference_cli.py \
  --source-root /absolute/frozen/uwvm2 \
  --binary /absolute/qualified/uwvm \
  --wasm-tools /absolute/wasm-tools \
  --out /absolute/new-result-directory \
  --jit-policy default
```

QEMU executions additionally use `--runner-prefix-json` with the qualified
emulator and sysroot arguments. The CLI records PIDFD admission before
debug commands, executable hashes and actual process retirement.

For interactive inspection, first obtain the actual module and frame IDs:

```text
frames wasm THREAD STOP 0 16
operands THREAD FRAME 0 64
locals wasm THREAD FRAME 0 64
members operands THREAD MODULE FRAME 0 ROOT 0 64
members operands THREAD MODULE FRAME 0 ROOT 0 64 0
members locals THREAD MODULE FRAME 0 ROOT 0 64
```

The member command's order is
`members SELECTION THREAD MODULE FRAME TABLE ROOT FIRST COUNT [PATH...]`.
`ROOT` is the original operand/local index, not a displayed `object #N`.
Object numbers are local to each reply. Module registry ordering is not fixed.
A path such as `0 1` follows payload field 0 and then struct field 1.

The finite matrix qualifies the recorded source/product cut and the added
test overlay. It does not establish threaded or host-origin exceptions, quota
exhaustion, every Core 3 feature, every full-LLVM target, debug-disabled
performance, or later concurrent source changes.


The 2026-10-08 finite qualification passed 192 actual LLVM-full VM runs,
312 exact typed pause observations and 10,224 original-root member pages across
both repositories, x86_64, QEMU RISC-V64 and QEMU AArch64, default/max JIT and
instruction/unwind policies. This reuses the previously qualified C++ products;
no production C++ change was required for the reference suite. All runtime and
independent qualification execution ran in the verified 64GiB/swap0 Linux cgroup.
See [the recorded report](../../documents/runtime/wasm-cross-module-reference-runtime-20261008.json)
for source/product bindings, the test overlay and remaining scope.
