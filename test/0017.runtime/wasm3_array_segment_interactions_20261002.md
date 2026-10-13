# Core 3 typed array segments and polymorphic fixed counts

This source-only group adds eleven cases to the existing fused-validation
runner. It changes no production source and performs no local assembly or VM
execution. The exact previous runner is preserved in the source snapshot.

The [Core 3 aggregate instruction rules](https://webassembly.github.io/spec/core/valid/instructions.html#aggregate-reference-instructions)
require a segment's reference type to match the array's reference storage for
`array.new_elem` and `array.init_elem`. A non-null initializer does not narrow a
nullable segment's declared type. Initialization requires mutable storage;
construction also permits immutable storage. Data construction/initialization
accept numeric or vector storage, including unpacked i16, and operate on byte
offsets in the segment. Array copy uses storage matching: i8 and i16 are
incompatible even though both unpack to i32. These immediate/declaration rules
remain in force for unreachable instructions.

The [validation algorithm](https://webassembly.github.io/spec/core/appendix/algorithm.html)
permits missing Bot operands at an unreachable control-frame height and still
checks known pushed operands. The 65536-element v128 pair materializes Bot via
`select`; its control supplies a known i32 in the required v128 sequence.
Called probes with a false condition force real lazy compilation without
executing a huge allocation. This does not cover the full u32 count maximum,
actual allocation limits, physical OOM, or concurrent collection.

Five valid cases execute function-subtype dispatch, null/readback checks,
immutable packed data construction with an unaligned byte offset, vector
construction/initialization and a called polymorphic probe. Six invalid cases
must report code-validation errors, including unreachable mutation/storage
mismatches. The three vector cases explicitly enable SIMD. No parser, unrelated
runtime failure or timeout can substitute for a validation error.

The sole Linux keeper should run the eleven `gc-array-*` cases through the
existing runner in the current 64 GiB/swap-zero cgroup: official wasm-tools
assembly/validation and ordinary pure validation/interpreter full+lazy/LLVM
full+lazy (and supplied lazy verification), then ROS pure validation and both
full modes. The runner itself does not launch Wasmtime. Add five independent
Wasmtime valid-case executions in the outer keeper recipe, using the exact
assembled bytes; runner success cannot substitute for those separate receipts. Exact fixtures/product/dependency
pins and actual source closure are required. Keep this small functional group
separate from P-core measurement. Source review is not an execution pass.
