# 0016 Backend Fuzzer

This test is a shell-driven differential backend fuzzer for core Wasm MVP modules.
It generates self-checking `.wat` files, compiles them with WABT `wat2wasm`, runs
them with WABT `wasm-interp` as the oracle, and compares the success/trap outcome
against UWVM backends:

- `uwvm-int` lazy
- `uwvm-int` full
- `llvm-jit` lazy
- `llvm-jit` full

The modules export a void `main` entrypoint. Successful modules validate their own
computed values and trap on mismatch, so the process result is enough to catch many
runtime codegen errors without adding a C++ harness.

Run through Xmake:

```bash
xmake f -m debug --execution-int=uwvm-int --execution-jit=llvm --enable-test-backend-fuzzer=y
xmake test -g backend_fuzzer
```

Run directly after building `uwvm`:

```bash
test/0016.backend_fuzzer/run_backend_fuzzer.sh --uwvm build/linux/x86_64/debug/uwvm
```

Useful environment variables:

- `UWVM_BACKEND_FUZZ_CASES`: generated pseudo-random i32 cases, default `32`.
- `UWVM_BACKEND_FUZZER_INCLUDE_TRAPS=0`: skip fixed trap cases for a success-only smoke run.
- `UWVM_BACKEND_FUZZER_WABT_ROOT`: WABT checkout/build root, default `build/test/third-parties/wabt`.
- `UWVM_BACKEND_FUZZER_WORK_DIR`: generated corpus/log directory, default `build/test/0016.backend_fuzzer`.
- `UWVM_BACKEND_FUZZER_WABT_PULL=1`: update an existing WABT checkout before building tools.
