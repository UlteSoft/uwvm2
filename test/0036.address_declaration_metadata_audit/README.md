# Address/shared/count declaration admission

This source-only fixture loads real official-tool binaries, parses and initializes with every control enabled, copies the actual parsed module, and selects one stricter immutable compilation policy per observation. Its consumer has no local/imported functions. A real preload provider supplies imports; no runtime record, opcode span or source policy is fabricated.

The independent CLI mask must agree with actual decoded limits and imported/local cardinality; AFTER also verifies original/deep-copy/runtime requirement bits. `memory64`, `table64`, `threads` and `multi-memory` are separately toggled. Shared memory64 needs both 1 and 4; MVP/no-memory remain valid with every selected control off. Multi-memory error value 2 identifies the minimum cardinality requiring the feature.

Run the first three negative cases as a short iteration before the complete finite matrix. The official wasm-tools version/hash/help and each parse+validate invocation/input/binary must be recorded. `shared_memory64.wat` is an intersection of Core3 memory64 and the separate Threads extension; actual oracle acceptance is pending and cannot be inferred from a WAT name.

All IO uses native_file_loader, parse_by_scan(dec), and formatted fast_io. All modes and the three extra public LLVM validation/convenience paths are explicit. Mandatory profile rows and observation counts in plan.json prevent int-only masquerading as LLVM. No function exists to invoke, so guest_execution remains 0.

Official Core3 (2026-10-02): https://webassembly.github.io/spec/core/binary/types.html#limits ; https://webassembly.github.io/spec/core/valid/modules.html#memories . Threads encoding/max: https://github.com/WebAssembly/threads/blob/main/proposals/threads/Overview.md#spec-changes . Old wasm1p1/wasm2 pure validators are untouched.
