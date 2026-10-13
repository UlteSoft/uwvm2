# Same-pass atomic memory candidate

This PRIVATE R2 candidate has not been applied, compiled, or run. It is a narrow descendant of immutable atomic R1, rebased onto the actual joint page/SIMD, Stage4 and common typed kernel SOURCE changes. It extends the
immutable page/SIMD joint R2 with a fourth mutually exclusive normalized LLVM
dispatch specialization. All 67 FE instructions use the first bounded atomic
decoder's owned result. The pure Core3 validator, integer translator, and LLVM
translator use one fixed-arity typed operand sequence helper. Reachable total
arity is checked before any pop; known reference heap Bot cannot satisfy a
numeric operand, while value Bot and the unreachable synthetic remainder retain
their distinct meanings.

The common sequence kernel is now present in the actual working tree with the exact R3 bytes, not a locally recreated implementation. This candidate preserves all current shared kernel/GC/EH changes outside the atomic-owned hunks. Further changes still require a narrow rebase. The
ancestral pure validator and entire LLVM emitter must not overwrite those later
changes. No old pure wasm1p1/wasm2 validator is changed.

The LLVM emitter consumes a checked owned event, with exact stack byte effects
(memory64 wait32=20, wait64=24, notify=12, i32 store=12). Its original LLVM IR and
runtime bridge body was moved byte-for-byte to a normal `.h`. Atomic guards,
sequential consistency, alignment and runtime traps remain in that body. The
raw FE decoder is removed from the generic emitter. The frontend marks each
successfully typed FE instruction inline before attempting emission; failure
disables that emission path rather than replaying raw FE bytes. Integer
register-ring emission after the typed transition is byte-for-byte unchanged.

Rules were checked against the official [threads validation rules](https://webassembly.github.io/threads/core/valid/instructions.html)
and the official [memory64 proposal](https://github.com/WebAssembly/memory64/blob/main/proposals/memory64/Overview.md).
The threads document is a threads extension draft, not the Core3 document.
Selected memories determine address width; wait timeout remains i64 and notify
count remains i32. Packed compare-exchange operands are narrowed to field width.

The paired `test/0034.fused_atomic_memory` leaves contain 263 return assertions
covering every FE opcode on both selected memory widths, eight invalid WAST
modules, six malformed binary encodings, an unused invalid function beyond the
lazy warm-up range, and a constexpr descriptor/typed-sequence component. Tests
allocate only one page per shared memory and waits use a zero timeout. Source
counts and binary pins are checked independently; no assembler or VM result is
claimed here.

The finite Linux runner pins actual tools/products/helpers, invokes the existing
cgroup gate, and accepts only real semantic return/rejection results. Its
`--configuration full` selects full modes and the validator; `all` includes
lazy/tiered configurations to expose their remaining body-admission gaps. Root
and the sole Linux keeper must run it in the existing 64GiB sandbox with a fresh
coherent source/product closure. C++ compilation, oracle results, generated
LLVM IR/assembly, musttail behavior, and performance are all pending. This slice
does not complete all Core3 lowering, retained lazy plans, tiered reuse, or
pre-initializer complete body validation.
