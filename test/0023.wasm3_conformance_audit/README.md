# Independent Core 3 validation probes

These fixtures were added by a read-only source audit on 2026-10-03. They are
test inputs, **not a record of executed success**. The audit did not compile or
run either product. Production sources and the old `wasm1p1` / `wasm2` pure
validator directories were not edited by this audit.

The authoritative specification consulted was
[WebAssembly 3.0, validation algorithm](https://webassembly.github.io/spec/core/appendix/algorithm.html)
(page version 2026-10-02), together with its
[instruction rules](https://webassembly.github.io/spec/core/valid/instructions.html),
[type matching](https://webassembly.github.io/spec/core/valid/matching.html), and
[binary instruction grammar](https://webassembly.github.io/spec/core/binary/instructions.html).
The consulted official test tree is pinned at commit
[`2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1`](https://github.com/WebAssembly/spec/commit/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1)
(2026-10-02). These are independently constructed, smaller probes; they do not
replace the full official test files.

| Probe | Distinction tested | Related official test at the pinned commit |
| --- | --- | --- |
| `reference_bottom` / first invalid case | A dead-stack `ref.as_non_null` creates a reference-only bottom, which permits `throw_ref` but cannot satisfy `i32.add` | [ref_as_non_null](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/ref_as_non_null.wast), [throw_ref](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/exceptions/throw_ref.wast) |
| `nondefaultable_local` / both-arm invalid case | Scoped initialization permits an in-block read; assignment in both if arms does not initialize the local after the frame ends | [local_init](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/local_init.wast) |
| `recursive_identity` / group identity invalid case | Matching includes the whole recursive group and projection, rather than just the projected function signature | [type-rec](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/type-rec.wast) |
| `common_branch_subtype` | `br_table` labels may differ when the actual branch argument is a subtype of every destination | [br_table](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/br_table.wast), [GC branch casts](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/gc/br_on_cast.wast) |
| `sibling_cast` | Cast validation uses the common heap hierarchy top; sibling struct types need not be related by a declared subtype edge | [GC ref_cast](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/gc/ref_cast.wast) |
| `aggregate_subtype` / mutable field invalid case | Struct prefix and immutable field covariance are permitted; mutable field types remain invariant | [GC type-subtyping](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/gc/type-subtyping.wast) |
| `immutable_packed_data` / packed get invalid case | `array.new_data` permits immutable packed arrays; packed field reads require a signed/unsigned extension opcode | [GC array](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/gc/array.wast) |
| `wide_storage` / mixed length and wrong-address invalid cases | Source and destination addresses follow their own declarations; copy length is i64 only when both addresses are i64 | [table_copy_mixed](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/memory64/table_copy_mixed.wast), [memory_copy64](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/memory64/memory_copy64.wast) |
| `typed_exception_payload` / wrong catch label and nullability invalid cases | Catch labels refer to the outer control stack; tagged values and the nonnull exception reference must match the complete destination tuple | [exceptions/try_table](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/exceptions/try_table.wast) |
| `tail_reference_covariance` | A tail callee's narrower reference result may satisfy the caller's broader reference result | [return_call_ref](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/return_call_ref.wast) |
| `unreachable_fixed_array` | A maximum u32 count in unreachable code must be handled without iterating over missing operands or allocating billions of entries | [GC array](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/gc/array.wast) |
| `relaxed_simd` | The FD subopcode above 0xff is bounded-decoded, feature gated, and has a well-defined deterministic allowed result in this chosen input | [relaxed swizzle](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/relaxed-simd/i8x16_relaxed_swizzle.wast) |
| Conditional-branch precision invalid case | Fallthrough reifies the label's declared prefix type, rather than retaining the original narrower value type | [br_on_null](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/br_on_null.wast) |
| `binary_edges.json` | Nonminimal u64 offsets, oversized memory32 offsets, truncated prefixes, literal flags, abstract-heap single bytes, and zero-count local run types | [binary_leb128_64](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/memory64/binary_leb128_64.wast), [binary-gc](https://github.com/WebAssembly/spec/blob/2e44bf79e68cc4fe689f43eea8e7d25b6fdfbce1/test/core/gc/binary-gc.wast) |

## Execution requirements

Run official-oracle validation first; a WAT text parser must preserve an
intentionally invalid module for `assert_invalid` or binary emission with
validation disabled. Diagnostic text in WAST follows the reference-interpreter
style and must not be compared verbatim to uwvm's formatted diagnostic.

Then validate each rejection and each valid module in ordinary int full/lazy,
LLVM full/lazy, and the supported tiered configurations. ROS has int full and
LLVM full. Trigger `_start` or explicitly invoke every probe with the indicated
arguments. A successful load of an unused lazy function is insufficient.
Trapping valid modules deliberately reach `unreachable` only after compilation;
their expected trap is not an invalid-module rejection. Avoid executing the
maximum-array probe with a modified entry point that bypasses `unreachable`.

The binary manifest contains complete modules and zero-based absolute module
offsets. `instruction_offset` is the instruction to investigate, while
`edge_offset` identifies the specific malformed/mismatched field. Hex is
lossless source data; no native executable is stored here. Each full module has
an exported `_start` so the lazy compilation path can be reached.

Feature checks must disable the specific feature independently, retaining
dependencies needed by the remaining syntax. In particular `wide_storage`
contains both memory64 and table64; use the small binary memory probes when
isolating memory64-independent Core 3 memarg grammar. `typed_exception_payload`
requires exceptions plus typed function references; `reference_bottom` also
requires function references even though it has no GC heap declaration.
Do not interpret a disabled dependency's earlier rejection as proof that the
intended opcode gate was reached.

## Source observations, not executed findings

One stricter-policy candidate was identified after the initial snapshot. In
`src/uwvm2/parser/wasm/standard/wasm1p1/features/types.h`,
`define_parse_core3_complete_type_section` takes the complete type section when
GC is enabled and unconditionally sets `section.requires_gc = true` at the end.
Thus even the direct `0x60 00 00` type of `gc_gate_baseline` is tagged as requiring
GC. The shared `require_gc_recursive_type_policy` then rejects it if an already
parsed module is validated/compiled with a stricter policy that only disables
GC. This is a source finding, not an executed failure. Parsing from scratch
with GC already disabled follows a different parser path, so this observation
must not be reported as a normal CLI rejection without actual execution.

Reproduce in the runtime-policy test API: first parse `gc_gate_baseline` with
all Core 3 features enabled, then call the pure validator and each fused
compiler/materializer with the same policy except `disable_gc = true`. It
should be accepted because neither its type section nor body uses GC syntax.
Also repeat with `(local anyref)` and with a nullable anyref global/table: those
must be rejected under the stricter policy. The current unconditional flag can
hide missing per-declaration checks, so the negative cases belong in the same
regression when correcting the baseline flag.

At the audit snapshot the two products' 54 Core 3 h/cppm files were identical,
and `impl.h` / `impl.cppm` imported/exported every file in that directory.
Normal full compilers fuse validation and translation; lazy materialization
reaches those fused compilers. The int and LLVM opcode validation remains
separately written, with shared Core 3 helpers for heap/type matching, GC stack
effects, catches, memargs, atomics, and feature policy. This observation does
not establish complete conformance or prove that every emitted path is safe.

Relevant paths, relative to each product root:

- Pure validator: `src/uwvm2/validation/standard/wasm3/validator.h`.
- int: `src/uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/translate/single_func.h`,
  `single_func_context.h`, and `opcode/{branch_cases,wasm1p1_cases,gc_cases,threads_cases}.h`.
- LLVM: `src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func.h`,
  `opcode/{branch_cases,wasm1p1_cases,gc_cases,threads_cases}.h`, and the secondary
  `single_func_emit.h` / `single_func_gc_emit.h` emission helpers.
- Lazy structural decoders:
  `src/uwvm2/runtime/compiler/{uwvm_int,llvm_jit}/compile_cu_from_lazy_validator/translate.h`.

The checked new immediate helpers perform bounded reads and commit bytecode
cursors only after successful decoding. The checked opcode cases first prove
the opcode byte exists; catch and type-index resolution happens before the
corresponding metadata lookup. This is a scoped source observation, not a
whole-repository pointer audit or a passed sanitizer/architecture matrix.
