# WebAssembly Specification Releases

## Changes by Release

### WebAssembly 1.0 (2019-07-20) — Changes since previous Working Draft

- Syntax and notation
  - Function reference notation: `anyfunc` → `funcref`
  - Variable instructions renamed: `get_local` → `local.get`, `set_local` → `local.set`, `tee_local` → `local.tee`
  - Global instructions renamed: `get_global` → `global.get`, `set_global` → `global.set`
  - Memory instructions renamed: `grow_memory` → `memory.grow`, `current_memory` → `memory.size`
  - Numeric function names normalized (e.g., `i64.extend_sx/i32` → `i64.extend_i32_sx`)
  - Notation for numeric conversions clarified; conversion names normalized (e.g., `extend_u` family normalized)
- Validation and limits
  - Tables limited to 2^32 elements (spec §3.2.3)
  - Memory limited to 2^16 pages (64KiB pages) (spec §3.2.4)
  - Validation of external types clarified (spec §3.2.6)
- Endianness and semantics fixes
  - Fix little-endian definition (spec §4.3.1.3)
  - Semantics for growing tables (spec §4.5.3.6) and memory (spec §4.5.3.7) made explicit
  - Stack behavior: push a dummy frame instead of raw values (spec §4.5.5)
- Text encoding and Unicode
  - UTF-8 validation fixed to disallow surrogate code points (spec §5.2.4)
  - Use term “scalar values” instead of “code points”
- Memory instruction immediates
  - Explicit `align` and `offset` immediates; default alignment 0
  - Alignment must be a power of two and ≤ the instruction’s natural alignment (range 0..3)
- Appendix changes
  - Add pre- and post-conditions section (Appx A.1)
  - Rename helper functions to noun_verb form (e.g., `decode_module` → `module_decode`) (Appx A.1)
  - Each subsection may occur at most once, in order of increasing id (Appx A.4)

### WebAssembly 1.1 (Draft 2021-11-16) — Integrated proposals

- Sign-extension operations
  - New numeric instructions: `i32.extend8_s`, `i32.extend16_s`, `i64.extend8_s`, `i64.extend16_s`, `i64.extend32_s`
- Non-trapping float-to-int conversions
  - New numeric instructions: `i32.trunc_sat_f32_s`, `i32.trunc_sat_f32_u`, `i32.trunc_sat_f64_s`, `i32.trunc_sat_f64_u`, `i64.trunc_sat_f32_s`, `i64.trunc_sat_f32_u`, `i64.trunc_sat_f64_s`, `i64.trunc_sat_f64_u`
- Multiple values
  - Function types allow more than one result; blocks can have parameters and arbitrary function types
- Reference types
  - New value types: `funcref`, `externref`
  - New reference instructions: `ref.null`, `ref.func`, `ref.is_null`
  - Parametric `select` enriched with optional type immediate
  - New declarative element segment form
- Table instructions
  - New instructions: `table.get`, `table.set`, `table.size`, `table.grow`; table element type can be any reference type
- Multiple tables
  - Modules may define/import/export multiple tables; table index immediates for `table.*` and `call_indirect`; element segments take a table index
- Bulk memory and table operations
  - Memory: `memory.fill`, `memory.init`, `memory.copy`, `data.drop`
  - Table: `table.fill`, `table.init`, `table.copy`, `elem.drop`
  - Passive data/element segments; new data count section; segment boundary checks may trap at runtime
- Fixed-width SIMD (128-bit)
  - New type: `v128`; loads/stores, constants, lane ops; arithmetic, logical, compare, shuffle/swizzle; conversions; tests and bitselect

### WebAssembly 2.0 (Draft 2025-01-30) — Core changes

- Incorporates Release 1.1 features: sign-extension ops, non-trapping float-to-int conversions, multiple values
- Reference types: `funcref`, `externref`; `ref.null`, `ref.func`, `ref.is_null`; typed `select`; declarative element segments
- Table instructions: `table.get`, `table.set`, `table.size`, `table.grow`; tables over any reference type
- Multiple tables per module; table index immediates; element segments take a table index
- Bulk memory and table operations as listed above
- Fixed-width SIMD (v128) with the instruction families listed above

### WebAssembly 2.0 + tracks — Deltas relative to 2.0 core

- 2.0 + threads (Draft 2023-10-10) (Phase 4 - Standardize the Feature)
  - Shared memories; memory is declared `shared`
  - Atomic instructions: `memory.atomic.wait32/wait64`, `memory.atomic.notify`, `atomic.fence`
  - Atomic loads/stores and RMW ops: `i32/64.atomic.load/store`, `atomic.rmw.add/sub/and/or/xor/xchg/cmpxchg`
- 2.0 + multi_memory (Draft 2024-09-04) (Phase 5 - The Feature is Standardized, merged into wasm3.0)
  - Multiple linear memories per module (define/import/export)
  - Memory index immediates for memory instructions: `memory.size`, `memory.grow`, `memory.fill`, `memory.copy`, `memory.init`
  - Data segments carry a memory index; loads/stores use memory index immediates
- 2.0 + tail_calls (Draft 2023-03-01) (Phase 5 - The Feature is Standardized, merged into wasm3.0)
  - New control instructions: `return_call`, `return_call_indirect`
- 2.0 + tail_calls + function_references (Draft 2024-12-09) (Phase 5 - The Feature is Standardized, merged into wasm3.0)
  - Typed function references and `call_ref`; refined typing of `ref.func`
  - `br_on_null`/`br_on_non_null`; tracking initialization of non-defaultable locals
- 2.0 + tail_calls + function_references + gc (Draft 2024-12-09) (Phase 5 - The Feature is Standardized, merged into wasm3.0)
  - Integration with managed reference types and subtyping introduced by GC

### WebAssembly 3.0 (Release 2026-09-21) — Changes

This list follows the [official Core 3.0 change history](https://webassembly.github.io/spec/core/appendix/changes.html#release-3-0) and its [constant-expression validation rules](https://webassembly.github.io/spec/core/valid/instructions.html#constant-expressions).

- Extended constant expressions
  - Allow basic numeric computations in const expr: `iN.add`, `iN.sub`, `iN.mul`, and `global.get` of prior immutable globals
- Tail calls
  - Control instructions: `return_call`, `return_call_indirect`
- Exception handling
  - Tags (define/import/export); new heap types: `exn`, `noexn`; short-hands: `exnref`, `nullexnref`
  - Control: `throw`, `throw_ref`, `try_table`; new tag section in binary
- Multiple memories
  - Multiple memories per module; memory index immediates for memory ops (incl. loads/stores and SIMD loads/stores)
  - Data segments take a memory index
- 64-bit address space
  - Memories and tables may declare an `i64` address type; their instruction operands, limits, and active-segment offsets use that address type
- Typeful references (function references)
  - Generalized reference types: `(ref null? heaptype)`; heap types: `func`, `extern`, `typeidx`
  - New/refined instructions: `ref.as_non_null`, `br_on_null`, `br_on_non_null`, `call_ref`; refined `ref.func`
  - Track initialization for non-defaultable locals; extended table init with optional initializer expr
- Garbage collection (managed references)
  - Heap types: `any`, `eq`, `i31`, `struct`, `array`, `none`, `nofunc`, `noextern`
  - Shorthands: `anyref`, `eqref`, `i31ref`, `structref`, `arrayref`, `nullref`, `nullfuncref`, `nullexternref`
  - Type defs: `struct`, `array`, subtypes, recursive types; enriched subtyping
  - Generic ref ops: `ref.eq`, `ref.test`, `ref.cast`, `br_on_cast`, `br_on_cast_fail`
  - i31 ops: `ref.i31`, `i31.get_s`, `i31.get_u`
  - Struct ops: `struct.new`, `struct.new_default`, `struct.get_s`, `struct.get_u`, `struct.set`
  - Array ops: `array.new`, `array.new_default`, `array.new_fixed`, `array.new_data`, `array.new_elem`, `array.get_s`, `array.get_u`, `array.set`, `array.len`, `array.fill`, `array.copy`, `array.init_data`, `array.init_elem`
  - External conversion: `any.convert_extern`, `extern.convert_any`
  - GC constant instructions: `ref.i31`, `struct.new`, `struct.new_default`, `array.new`, `array.new_default`, `array.new_fixed`, `any.convert_extern`, `extern.convert_any`; these do not require the separate numeric extended-const feature
- Relaxed SIMD
  - New relaxed min/max, multiply-add, truncation, swizzle, lane selection, q15 multiply and dot-product instructions; the standard permits implementation-dependent results
- Profiles
  - The specification introduces a deterministic execution profile
- Custom annotations (text format)
  - Generic annotations `(@id ...)`; escaped identifiers `@"..."`
  - Built-in name annotations: `(@name "...")` for module/type/func/local/field; `(@custom "...")` for arbitrary custom sections

### Implementation release gate

Core 3.0 instruction and validation support does not establish that the managed-object runtime is ready for long-lived programs. The current GC object store retains every allocated struct and array until module teardown. The [GC release blocker and executable memory-growth gate](../benchmark/0004.wasm3-core/RELEASE_BLOCKER_GC.md) record the source-bound Linux measurements, the missing reachability collection, and the root, thread, cross-module, and reclamation requirements that must pass before a production release. Performance comparisons for the other Core 3.0 features are tracked separately in the [Core 3 benchmark](../benchmark/0004.wasm3-core/README.md).

For an Xmake release build, refresh `--build-source-id` with the current source fingerprint during `xmake f` while preserving the other build options. `xmake -r` recompiles but does not refresh Xmake's cached configuration. Check that the finished executable embeds the expected fingerprint before treating runtime results as source-bound evidence; the earlier macOS v3 evidence is explicitly invalidated in `build/wasm3-evidence/final-20260925/macos-v3-provenance-invalidated.json` for this reason.
