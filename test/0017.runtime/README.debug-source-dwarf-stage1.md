# Embedded Wasm DWARF metadata, Stage1

This independent component produces copied scope, concrete inline, variable,
scalar-type and finite location-plan metadata. Its standalone tests establish
only this parser component. A separate source-only candidate installs it in
`run.h`/controller for current physical-frame inline display using the runtime's
actual source/publication/stop binding; that integration needs its own fresh
production DWARF link closure and runtime tests. DAP/value queries are not
implemented by this component. It does not recover variable
values, read memory, access native registers, open source/DWO files, or expand
physical callers whose true Wasm PCs were not captured.

`source_dwarf::index::parse(input, out, limits)` clears `out` first, copies accepted
embedded payloads and constructs an immutable heap owner. Diagnostic storage and
the `MemoryBuffer` map precede `DWARFContext`; nothing moves, and context is
destroyed while callbacks/buffers remain live. Only a complete successful parse
publishes output. Recoverable exceptions/allocation failures discard all partial
data. Fatal LLVM allocator failures remain process failures; these budgets do
not impose an exact LLVM internal heap limit.

The parser admits DWARF32/64 versions 4/5 with matching Wasm address sizes 4/8,
independent of native host width, and always little endian. It rejects external,
split/skeleton/DWO, supplementary/altlink metadata, external/type-unit reference
forms and unknown debug extensions. It never calls autoload, symbolizer,
ObjectFile or general expression APIs. LLVM diagnostics are bounded private kind
records, not default stderr messages.

Defaults: 64 sections, 1 MiB per section, 8 MiB total, 256 CUs/256 KiB per CU,
65,536 actual parsed DIEs, depth 64, 4 KiB per string/path and 1 MiB copied strings,
16 reference hops, 1,048,576 attributes, 65,536 ranges/locations/line rows, 256 bytes
per expression and 1 MiB expressions total. Byte/CU-header preflight precedes
LLVM parsing. DIE/row/location counts are checked after parsing bounded inputs,
not before LLVM allocation. The test process still requires the controlled
64 GiB/no-swap/verified-CPU-set Linux cgroup.

Ranges/locations are concrete-DIE-only. Bounded abstract-origin/specification
walks supply descriptive names, types and declarations. File indices use the
attribute's own CU line table; raw filenames are copied without filesystem/path
resolution. Names/paths containing control bytes are rejected. These untrusted
records authenticate no source, execution epoch or JIT generation.

Finite plans recognize local plus terminal `stack_value`, scalar constant plus
`stack_value`, small `implicit_value`, and signed `fbreg` displacement metadata.
Plain local locations are unavailable as values: LLVM's indirect-local encoding
uses the same local opcode without terminal `stack_value`. Frame-base role may
record that plain local as a base plan. Globals, operand stack, native
registers/CFA, calls, dereferences, pieces and unsupported operations are
explicitly unavailable. No plan executes or supplies a host pointer. Code offsets
are bounded, but not certified as all decoded opcode boundaries. The separate
metadata-display candidate obtains the actual current Code position from a live
execution lease, unique native code owner, source validation epoch, function
generation and matching parked participant/safe-point bitmap. The separate
[Stage2 direct numeric candidate](README.debug-source-dwarf-values.md) binds actual
captured scalar slots before using a restricted plan; these Stage1 plans alone
grant no value/read authority. Explicitly declared empty lexical ranges are
retained separately from absent range attributes and exclude nested metadata.

The standalone runner links the SDK's `debuginfodwarf` explicitly. The separate
production integration has a new DWARF dependency contract candidate, qualified
independently; no test summary authorizes changing frozen Windows archives. It builds real C/C++/Rust locals
and forced inline fixtures for DWARF4/5 O0/O1, requires `wasm-tools validate`,
official `llvm-dwarfdump --verify`, actual parameter/local DIEs and optimized
inline DIEs, then tests metadata. Synthetic tests cover both DWARF formats,
versions/address widths, budgets, reference cycles, known truncations, duplicate
and external sections, and complete failure cleanup.

Run only in the keeper's existing remote Linux cgroup with matching SDK/toolchain:

```sh
python3 "$ROOT/test/0017.runtime/run_debug_source_dwarf_stage1.py" \
  --source-root "$ROOT" --out "$EVIDENCE/dwarf-stage1" \
  --cxx "$CXX" --llvm-config "$LLVM_CONFIG" --llvm-dwarfdump "$DWARFDUMP" \
  --wasm-tools "$WASM_TOOLS" --wasm-clang "$WASM_CLANG" --wasm-ld "$WASM_LD" \
  --rustc "$RUSTC"
```

This source candidate was not compiled/run on the development machine. A future
`summary.json` applies only to its exact component/tool hashes and proves no
runtime variable or full-debugger capability.

Primary references: [Wasm DWARF conventions](https://github.com/WebAssembly/tool-conventions/blob/main/Dwarf.md),
[LLVM DWARF APIs](https://llvm.org/doxygen/classllvm_1_1DWARFContext.html),
[llvm-dwarfdump](https://llvm.org/docs/CommandGuide/llvm-dwarfdump.html),
[Rust codegen options](https://doc.rust-lang.org/rustc/codegen-options/).
