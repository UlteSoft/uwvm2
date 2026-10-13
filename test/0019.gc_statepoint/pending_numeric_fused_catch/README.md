# Numeric exception catch regression cases

These six modern `try_table` cases test raw numeric payload bits, empty tuples, distinct tags with equal signatures, lexical handler order, consecutive throws, and catch-all fallback. `oracles.json` records the expected `run` export and mixed payload bit patterns.

The pending fused-catch compiler candidate is enabled only by `UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH=1`; it remains off by default. Run both instruction and unwind policies, with and without a genuine admitted pending plan. Native component checks do not qualify the LLVM lowering or complete VM.

The exact six WAT inputs were encoded on Linux and their `run` exports returned 1, 31415, 1037, 41, 33, and 1 under Wasmtime 49.0.1. Retained raw receipts are under `build/wasm3-evidence/linux-r2-native-stage-20260928/pending-fused-tiny-tag-San-baseline-completion-qualified-small-raw-r1c/`; those are reference results, not UWVM results.

Specification: https://webassembly.github.io/spec/core/exec/instructions.html#exception-instructions
