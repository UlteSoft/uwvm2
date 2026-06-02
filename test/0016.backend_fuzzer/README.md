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

WABT is cloned and built automatically under `build/test/third-parties/wabt` if
the required tools are missing. The WABT oracle is invoked with post-MVP features
disabled, so generated cases are checked as Wasm MVP/1.0.

The matrix runner defaults to the Linux CI sanitizer policy
`build.sanitizer.address,build.sanitizer.leak,build.sanitizer.undefined` and
falls back to `build.sanitizer.address,build.sanitizer.undefined` on macOS.

Run through Xmake:

```bash
xmake f -m debug --execution-int=uwvm-int --execution-jit=llvm --enable-test-backend-fuzzer=y
xmake test -g backend_fuzzer
```

Run the compile-time `uwvm-int` matrix directly. This reconfigures Xmake for each
`--enable-uwvm-int-combine-ops` and `--enable-uwvm-int-delay-local` pair, builds
`uwvm`, then runs the same four runtime modes. It does not run tiered mode.

The matrix script intentionally runs nested `xmake f` and `xmake b`, so it should
not be wrapped in `xmake test`.

```bash
UWVM_BACKEND_MATRIX_COMBINE_MODES="none soft heavy extra" \
UWVM_BACKEND_MATRIX_DELAY_MODES="none soft heavy" \
UWVM_BACKEND_MATRIX_POLICIES=build.sanitizer.address,build.sanitizer.leak,build.sanitizer.undefined \
test/0016.backend_fuzzer/run_backend_fuzzer_matrix.sh
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
- `UWVM_BACKEND_MATRIX_COMBINE_MODES`: space- or comma-separated combine modes, default `none soft heavy extra`.
- `UWVM_BACKEND_MATRIX_DELAY_MODES`: space- or comma-separated delay-local modes, default `none soft heavy`.
- `UWVM_BACKEND_MATRIX_BUILD_MODE`: Xmake build mode for matrix runs, default `debug`.
- `UWVM_BACKEND_MATRIX_POLICIES`: Xmake sanitizer policy list for matrix runs, default CI-style ASan/LSan/UBSan on Linux.
- `UWVM_XMAKE_JOBS`: optional Xmake build job limit for matrix runs.
