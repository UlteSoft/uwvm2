Tools

- `tools/ci/patch_llvm_libcxx_hash_memory.py`: CI helper script to work around an upstream LLVM libc++ issue in `__functional/hash.h` related to `_LIBCPP_AVAILABILITY_HAS_HASH_MEMORY`.
- `tools/wasm_opcode_counter/opcode_counter.py`: Count opcode occurrences in the Wasm code section (e.g. `i32.const` count).
- `tools/wasm_operand_stack_stats/stack_stats.py`: Count operand stack height after each opcode in the Wasm code section; reports `> threshold` vs `<= threshold`.
