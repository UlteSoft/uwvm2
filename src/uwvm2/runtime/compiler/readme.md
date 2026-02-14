# Runtime compilers

This directory contains UWVM2 “runtime compiler” components: implementations that translate validated WebAssembly into an **in-memory executable form** (interpreter bytecode and/or native code), without producing a standalone executable on disk.

## Contents

- `uwvm_int/`: high-performance threaded interpreter pipeline (“u2”), including the register-ring stack-top cache design. See `uwvm_int/readme.md`.
- `debug_int/`: debug interpreter focused on full semantic coverage and observability. See `debug_int/readme.md`.
- `llvm_jit/`: JIT-related work (in-memory native code generation; currently sparse / work-in-progress).

## Notes

During compilation/translation, opcode metadata and feature constraints can be resolved via templates to enforce correctness at compile time. The runtime execution path can then remain independent of that metadata to preserve compatibility and keep hot paths minimal.
