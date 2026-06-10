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

In addition to random arithmetic/memory cases, the generator emits fixed
strategy cases by default:

- `tiered_hot_direct_call`: repeatedly calls a tiny local helper enough times to
  force tiered entry-hot replacement, then keeps executing through the
  interpreter-to-JIT call edge.
- `tiered_osr_loop`: emits a large-enough loop body to exercise tiered loop OSR
  polling and reentry generation.
- `tiered_win32_abi_direct_matrix`: stresses local-defined typed/raw tiered
  calls with enough arguments to expose Win64 SysV vs host-ABI mixups.
- `tiered_win32_abi_osr_call_matrix`: combines tiered OSR reentry with a
  local-defined JIT call inside the loop body.

On Windows, `run_win32_abi_probe.py` also builds a small standalone C++ probe
that directly exercises host-to-raw-entry, host-to-typed-entry, generated
Wasm-ABI-to-host-bridge, table typed/raw dispatch, OSR-style reentry, and
interpreter callback function-pointer boundaries.  It also checks the source for
the pointer-sized atomic-load alignment guard that prevents COFF/MCJIT from
emitting unresolved `__atomic_load` calls.

The low-level `uwvm-int-ring-matrix` runner only exercises single-function
translator ABI shapes, so cases that require runtime call bridges are marked
and skipped there while still running in lazy/full JIT and tiered modes.

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

Run the Windows ABI probe:

```bash
python test/0015.backend_fuzzer/run_win32_abi_probe.py
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

Run the LLVM libFuzzer target:

```bash
CXX="$(command -v clang++)" \
SYSROOT="$(xcrun --sdk macosx --show-sdk-path)" \
UWVM_BACKEND_LIBFUZZER_MODES="uwvm-int-ring-matrix llvm-jit-lazy tiered" \
test/0015.backend_fuzzer/run_backend_libfuzzer.sh \
  --memory-model single-thread-alloc \
  --no-use-thread-local \
  -- \
  -max_total_time=300
```

`backend_libfuzzer.cc` maps arbitrary input bytes to a valid Wasm code body
instead of treating bytes as a raw Wasm module. The generated code is
self-checking: wrong codegen reaches `unreachable`, which libFuzzer records as
a crash. The launcher runs libFuzzer in a single process by default. When
`SYSROOT` is set, the build uses `--sysroot=$SYSROOT` and `-fuse-ld=lld`.

Useful environment variables:

- `UWVM_BACKEND_FUZZ_CASES`: random non-trap cases, default `32`.
- `UWVM_BACKEND_FUZZ_SEED`: deterministic seed. If unset, a random seed is
  generated and printed; generated corpora also write it to `cases/seed.txt`.
- `UWVM_BACKEND_FUZZER_INCLUDE_TRAPS=0`: skip fixed trap cases.
- `UWVM_BACKEND_FUZZER_INCLUDE_STRATEGY=0`: skip fixed tiered strategy cases.
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
- `UWVM_BACKEND_LIBFUZZER_MODES`: space- or comma-separated mode list for the
  libFuzzer target. Defaults to all backend modes.
- `UWVM_BACKEND_LIBFUZZER_TRACE=1`: print input size and backend mode before
  each execution. This is useful when reducing a crash.
- `UWVM_BACKEND_LIBFUZZER_STRESS_TIERED_OSR=1`: add a long self-checking loop to
  each generated libFuzzer module so tiered OSR polling/reentry paths are hit
  more aggressively.
- `UWVM_BACKEND_LIBFUZZER_SANITIZERS`: default `fuzzer,address,undefined`.
- `UWVM_BACKEND_LIBFUZZER_SYSROOT`: sysroot path; `SYSROOT` and `SDKROOT` are
  also honored.
- `UWVM_BACKEND_LIBFUZZER_USE_LLD=0`: disable `-fuse-ld=lld`.
- `UWVM_BACKEND_LIBFUZZER_PROGRESS=0`: silence libFuzzer target build
  heartbeat.
- `UWVM_BACKEND_LIBFUZZER_PROGRESS_INTERVAL`: heartbeat interval in seconds,
  default `10`.
- `LLVM_CONFIG`: llvm-config path for building the JIT-enabled runner.
- `CXX`: C++ compiler used by `build_runner.py`.

`run_backend_fuzzer.sh` also accepts repeated `--define`, `--extra-cxxflag`,
and `--extra-ldflag` arguments. These are intended for targeted llvm-jit or
runtime macro probes that are not worth making first-class matrix dimensions.
Known crashes can be skipped during long libFuzzer runs by passing
`-ignore_crashes=1` after `--`.
