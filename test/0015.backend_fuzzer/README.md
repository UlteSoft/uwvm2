# 0015 Backend Fuzzer

This folder is a pure shell/Python differential fuzzer for uwvm2 backend codegen.
It does not use xmake and does not run the `uwvm` CLI.

`generate_cases.py` creates two views of the same random corpus:

- `.wasm` binaries for WABT validation and oracle execution.
- `generated/backend_fuzzer_cases.inc` for the C++ runner.

The C++ runner constructs `wasm_module_storage_t` directly from the generated
arrays, including function types, code bodies, local declarations, and memory
records. It bypasses parser and runtime initializer, then runs the real backend
entry points or the low-level uwvm-int single-function compiler.

The generated modules export a void `main`. Non-trap cases are self-checking:
they compute expected values and execute `unreachable` on mismatch. Trap cases
are also useful, because `unreachable`, divide-by-zero, and OOB memory accesses
are observable backend behavior.

Default compared modes:

- `uwvm-int-ring-matrix`
- `uwvm-int-lazy`
- `uwvm-int-full`
- `llvm-jit-lazy`
- `llvm-jit-full`
- `tiered`
- `tiered-no-t0`
- `tiered-no-t2`

Run one macro configuration:

```bash
test/0015.backend_fuzzer/run_backend_fuzzer.sh \
  --fuzz-cases 32 \
  --combine none \
  --delay none \
  --memory-model default
```

Run the compile-time macro matrix:

```bash
UWVM_BACKEND_MATRIX_COMBINE_MODES="none soft heavy extra" \
UWVM_BACKEND_MATRIX_DELAY_MODES="none soft heavy" \
test/0015.backend_fuzzer/run_backend_fuzzer_matrix.sh
```

The matrix defaults to only `memory=default` and `thread-local=0`. Add those
dimensions explicitly when needed:

```bash
UWVM_BACKEND_MATRIX_MEMORY_MODELS="default mmap single-thread-alloc multi-thread-alloc" \
UWVM_BACKEND_MATRIX_THREAD_LOCAL_MODES="0 1" \
test/0015.backend_fuzzer/run_backend_fuzzer_matrix.sh
```

WABT is cloned and built automatically under `build/test/third-parties/wabt` if
`wasm-validate` or `wasm-interp` are missing. WABT is invoked with post-MVP
features disabled, so cases are checked as Wasm MVP/1.0.

Useful environment variables:

- `UWVM_BACKEND_FUZZ_CASES`: random non-trap cases, default `32`.
- `UWVM_BACKEND_FUZZ_SEED`: deterministic seed, default `0xC0DEF00D`.
- `UWVM_BACKEND_FUZZER_INCLUDE_TRAPS=0`: skip fixed trap cases.
- `UWVM_BACKEND_FUZZER_MODES`: space- or comma-separated mode list.
- `UWVM_BACKEND_FUZZER_WABT_ROOT`: WABT checkout/build root.
- `UWVM_BACKEND_FUZZER_WORK_DIR`: corpus, runner, and log output root.
- `UWVM_BACKEND_FUZZER_WABT_PULL=1`: update an existing WABT checkout.
- `UWVM_BACKEND_FUZZER_COMBINE`: `none`, `soft`, `heavy`, or `extra`.
- `UWVM_BACKEND_FUZZER_DELAY`: `none`, `soft`, or `heavy`.
- `UWVM_BACKEND_FUZZER_MEMORY_MODEL`: `default`, `mmap`,
  `single-thread-alloc`, or `multi-thread-alloc`.
- `UWVM_BACKEND_FUZZER_USE_THREAD_LOCAL=1`: define `UWVM_USE_THREAD_LOCAL`.
- `UWVM_BACKEND_FUZZER_EXTRA_DEFINES`: extra build macros, space- or
  comma-separated. Entries may include or omit `-D`.
- `UWVM_BACKEND_FUZZER_EXTRA_CXXFLAGS`: extra compiler flags for the runner.
- `UWVM_BACKEND_FUZZER_EXTRA_LDFLAGS`: extra linker flags for the runner.
- `LLVM_CONFIG`: llvm-config path for building the JIT-enabled runner.
- `CXX`: C++ compiler used by `build_runner.py`.

`run_backend_fuzzer.sh` also accepts repeated `--define`, `--extra-cxxflag`,
and `--extra-ldflag` arguments. These are intended for targeted llvm-jit or
runtime macro probes that are not worth making first-class matrix dimensions.
