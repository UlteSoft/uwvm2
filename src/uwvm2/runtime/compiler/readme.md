# Runtime compilers

This directory contains the two retained UWVM2 translation components. Both translate a complete validated module before execution; demand compilation and tier switching have been removed.

## Contents

- `uwvm_int/`: high-performance threaded interpreter pipeline (“u2”), using full-module translation and the register-ring stack-top cache design. See `uwvm_int/readme.md`.
- `llvm_jit/`: full-module LLVM AOT translation and native materialization. Its historical directory name is retained temporarily; no lazy LLVM compiler, tiered runtime, or per-function interpreter fallback is built.

## Notes

During compilation/translation, opcode metadata and feature constraints can be resolved via templates to enforce correctness at compile time. The runtime execution path can then remain independent of that metadata to preserve compatibility and keep hot paths minimal.
