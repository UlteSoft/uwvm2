# Complete-instance census runtime fixture (SOURCE only)

`debug_checkpoint_complete_instance_runtime.cc` is a new actual-runtime input,
not a rerun of the old debugger VIEW test. Both products use byte-identical
fixture sources. R2 adds real retained first-catch/throw_ref/re-catch exception
identity assertions; frozen R1 remains unchanged. It needs the complete-census
API/manager/resource/reference/frame producer, schema 5 exception diagnostic payload, and checkpoint standalone-module
seam composed with the current observation 12, genuine generation and owned
preload source changes. Do not compile this fixture against an older runtime or
replace current sources with a stale whole packet.

The fixture selects LLVM JIT full and the explicit `observe_values` recording
purpose before compilation. Original parsing and fused validation/lowering use
the same immutable image adopted before parsing. One ordinary setup invocation
creates the resources; two later genuine native guest participants only read
them. Each capture comes from its actual generated before-park callback. The
complete state API must authenticate the real complete cohort, close actual host
admission, exclude all extra GC entries, and retain current publication/typed
source owners before it copies any graph. Neither the recording label, a wire ID
nor the returned const DATA graph authorizes executable restoration.

The fresh Core 3.0 WAT exercises:

- memory64 with current/minimum size 3 and maximum 5 standard pages; two separate
  nonzero pages with an omitted all-zero middle page and exact byte assertions;
- table64 with a real non-default GC-reference run, plus a zero-length
  nonnullable typed-function table whose explicit initializer is `ref.func`;
- 65 structures, including a real self-cycle, packed i8 fields and v128 values;
  four arrays with 1024 numeric values, 64 distinct structure identities, and
  1024 aliases of the same structure;
- a real thrown/caught exception followed by throw_ref and a second catch_ref;
  both retained global roots and both initialized frame-local roots must use ONE
  exception object and ONE original diagnostic trace, without assuming native
  carrier-token equality; linked tag identity, typed payload, original immutable
  trace, and an extern wrapper retain the same GC object aliases;
- all eleven globals, three functions, two tables, one memory, one tag, and both
  live and dropped passive data and typed element segments;
- the actual two native participants' complete captured locals and operands.

The table default is its real slot 0 value. Slot 1 differs and must be represented
by a table chunk; a duplicate slot would correctly be omitted by the sparse
format. The native diagnostic producer currently has no original instruction
PC or generation: the graph records explicit UNKNOWN PC/generation, preserving
origin instance/function and owned names instead of manufacturing offset 0.

Admission negatives use real extra GC entries, incomplete/duplicate capture
sets, forged shared-owner aliases (different pointee and different control block),
a zero recording label, a deliberately insufficient quota, old owners/tickets
from a previous genuine pause episode, and resumed/reset execution. All must
return no graph. Reversing a valid complete capture set must still produce a valid
complete graph. Detached logical DATA must remain readable after runtime reset.

Scheduling is bounded: at most 8 actual pairs of guest entries, each pause wait
has a 20-second deadline, and exactly 2 complete two-participant snapshots in the
real long-nop operand window must pass. A valid pause at the final drop/return
sequence remains valid DATA but does not count toward that window coverage.
External keeper timeouts must also cover joins and native compilation. Guest
memory is 192 KiB; fixture quotas are 2 MiB encoded DATA, 1 MiB payload, 1024 objects,
8192 links, 16384 values, 2 threads and 2 frames. These are test quotas, not evidence
of the LLVM compiler's maximum RSS.

Qualification is pending: no official WAT assembly, C++ compilation, LLVM
verification, native execution, endian or platform test has run for these new
files. The only completed checks are source review, paired content equality,
manifest hashes and private textual patch admission. A const copied graph is
not whole-instance save/load, rewind, replay or newly issued execution authority.
The stdout field `whole_restore=0` deliberately preserves that distinction.
Active caught-native continuation, nested frame/control restart, persisted
identity binding and host/WASIp1 recovery require their separate actual producers
and execution transactions; this fixture makes no claim for them.

The sole Linux keeper should first assemble and validate the new WAT with the
current official wasm-tools build, using Core 3.0 features (including memory64,
nonnullable function references and modern try_table), in the existing 64 GiB
cgroup. For example, with paths resolved by the keeper:

```sh
wasm-tools parse test/0017.runtime/fixtures/debug_checkpoint_complete_instance.wat -o /tmp/debug_checkpoint_complete_instance.wasm
wasm-tools validate --features all /tmp/debug_checkpoint_complete_instance.wasm
```

Build/link the new C++ fixture with three fresh matching object roles: full-LLVM
runtime, production host glue, and this fixture (which defines its own main).
Compile production_main separately as a fresh compile-only .o/.d/log check;
NEVER link that production_main object into this fixture executable. All three
linked roles and the separate compile-only check use the exact composed source.
Run the resulting binary
with `<assembled-wasm-path> instruction` and `<assembled-wasm-path> unwind`, in
each product and under an external finite timeout in the same cgroup. Use the
keeper's existing actual-runtime link procedure: basename discovery of a
`debug_*` test alone does not establish runtime/host/fixture dependencies.
Require actual runtime output and inspect failing cases before calling either
strategy passed. Do not execute this new fixture on macOS.

Primary normative references: the [Core 3.0 table, memory, data and element module
syntax](https://webassembly.github.io/spec/core/text/modules.html),
[Core 3.0 instruction validation](https://webassembly.github.io/spec/core/valid/instructions.html),
the [Core 3.0 throw_ref execution rules](https://webassembly.github.io/spec/core/exec/instructions.html),
and [Core 3.0 store/frame runtime structure](https://webassembly.github.io/spec/core/exec/runtime.html).
Sparse page/run encoding, quotas and shared-owner admission are implementation
contracts, not additional WebAssembly semantics.
