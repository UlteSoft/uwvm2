# Numeric exception CLI and performance runners

Run these scripts only on the remote Linux test machine inside the existing
`uwvm3-implementation` cgroup (64 GiB, no swap, allowed CPUs
`0,2,4,6,16-31`). Use a binary with a recorded production source fingerprint and
compiler flags. Selecting a backend does not imply that backend has passed the
suite. A failed case is a failure; unsupported EH, missing unwind support and
missing original stacks are not skipped.

`run_exception_cross_cli.py` contains 31 numeric `throw`/`try_table` cases.
They cover ordered and outer mismatch handling, imported tag aliases, tail-call
handler bypass, mixed/v128 payload bits, exceptional-only stack capacity, loops,
traps that must stay uncatchable, and uncaught original-stack ordering. Reference
payloads, exception references, the debugger and complete Core 3 conformance are
outside this suite.

## Backend selection

Existing invocations retain their behavior: the default backend is `int`, with
ordinary full/lazy/lazy+verification modes and ROS full. Existing interpreter log
names and `interpreter_modes` summary data remain compatible. `--all-combine-delay`
still selects all eight interpreter combinations; it is rejected for LLVM.

LLVM is explicit: `--backend llvm`. It defaults to one profile, full with
instruction tracing. Repeat `--mode full --mode lazy --mode lazy+verification`
or `--trace instruction --trace unwind` only when that specific matrix is wanted.
ROS accepts full only. LLVM lazy+verification is an explicit selection; the runner
does not silently substitute lazy for verification mode. `--trace` is rejected
for the interpreter, so it cannot silently mislabel an interpreter measurement.

The runner mappings come from the actual CLI parameter declarations:

| Runner selection | Ordinary CLI | ROS CLI |
| --- | --- | --- |
| `--backend int --mode full` | `-Rcc int -Rcm full` | `-Rint` |
| `--backend llvm --mode full` | `-Rcc jit -Rcm full` | `-Raot` |
| `--backend llvm --mode lazy` | `-Rcc jit -Rcm lazy` | Unsupported |
| `--backend llvm --mode lazy+verification` | `-Rcc jit -Rcm lazy+verification` | Unsupported |
| `--trace instruction/unwind/none` | `-Rllvm-call-stack instruction/unwind/none` | Same |

The source declarations are `runtime_custom_compiler.h`, `runtime_custom_mode.h`,
`runtime_llvm_jit_call_stack.h`, and ROS `runtime_aot.h` under
`src/uwvm2/uwvm/cmdline/params`. `unwind` requests the checked platform mode;
the scripts do not substitute `unwind-uncheck` or instruction mode on failure.

For uncaught exceptions, instruction and unwind profiles require the exact
`Uncaught WebAssembly exception` fatal class, the throw-site capture heading,
and exact innermost-to-outermost function indexes. Truncated, reordered or absent
stacks fail. `none` instead requires an explicit unavailable-snapshot report and
zero fabricated frame rows; it qualifies the fatal class and EH execution, not
original-stack capture. Wasmtime must specifically report a thrown Wasm exception,
while trap cases must retain their distinct trap categories. Uncaught payloads are
checked field by field, including negative integers, a signaling-NaN bit pattern,
negative zero and all 16 v128 bytes. `--force-color` additionally requires ANSI
colors and reset codes in the preserved raw diagnostic. LLVM execution and timing
explicitly disable persistent caching; the separate alias-cache runner covers
cold/warm cache behavior.

## Focused functional commands

Set `UWVM_BIN` to the qualified remote executable and run from the source/runner
checkout inside the cgroup. Each output directory must be new.

```sh
python3 test/0014.llvm_jit/run_exception_cross_cli.py \
  --uwvm "$UWVM_BIN" \
  --wasm-tools /dev/shm/uwvm-tools/wasm-tools-1.259.0 \
  --wasmtime /work/artifacts/wasmtime-v48.0.2-x86_64-linux/wasmtime \
  --backend llvm --mode full --trace instruction --trace unwind \
  --out /dev/shm/uwvm-builds/exception-cross-llvm-full
```

This explicitly runs 31 cases in two full-JIT stack policies. For a first narrow
check, add `--case caller-prefix --case uncaught-direct --case uncaught-mismatch`.
For ordinary LLVM lazy, replace `--mode full` with `--mode lazy`. For ROS full,
add `--ros` and select the ROS executable. To test disabled diagnostic frames,
use a separate output directory and `--trace none`.

Omitting `--uwvm` permits Wasmtime fixture qualification only. Such a report is
not UWVM execution evidence.

## Paired performance commands

The benchmark selects exactly one backend, mode and trace policy. LLVM full uses
explicit `pb-o3` generated-code optimization; host `--build-optimization O3` is
recorded separately. Lazy mode keeps the runtime lazy optimization policy. Both A/B
variants retain that policy. In the unused-feature comparison, the exact same
Wasm and executable are run with `-WFD-exceptions` and `-WFE-exceptions`; only the
feature switch changes. The other comparisons measure protected normal calls,
local throw-to-branch lowering, and throwing through one versus eight callees.
Every guest checks its final state. Wasmtime separately checks the same generated
workload with at most `--reference-max-iterations` (default 200,000); its count is
recorded and may differ from the calibrated UWVM count. The affine endpoint
checker validates every larger UWVM workload independently. This avoids expanding
a native local throw-to-branch optimization into billions of reference throws.

```sh
python3 test/0014.llvm_jit/bench_exception_cross_cli.py \
  --uwvm "$UWVM_BIN" \
  --wasm-tools /dev/shm/uwvm-tools/wasm-tools-1.259.0 \
  --wasmtime /work/artifacts/wasmtime-v48.0.2-x86_64-linux/wasmtime \
  --backend llvm --mode full --trace unwind \
  --build-optimization O3 --build-manifest "$UWVM_BUILD_MANIFEST" \
  --pairs 9 --out /dev/shm/uwvm-builds/exception-cross-llvm-full-unwind-perf
```

Use separate output directories to compare instruction/unwind/none. Add `--ros`
for ROS. Retain exact build manifests, binary hashes, raw samples, affinity,
cgroup limits and background-process observations. Run products sequentially
without other compilation or tests during sampling. Record CPU affinity when
pinning to a permitted core; preserve noisy initial samples rather than deleting
them. The runner calibrates samples to at least 100 ms and 20 times startup and
alternates AB/BA for at least nine pairs. Startup and compilation are included.

The `trace_policy` report states whether default interpreter tracing or an
explicit LLVM diagnostic policy was requested. Throw timings include value
construction/allocation, enabled trace frame/name copying, propagation, matching
and cleanup. They do not isolate the native unwinder. The historically named
`native-unwind-depth` comparison always uses the same diagnostic policy for both
depths; it is not an instruction-versus-unwind comparison. `none` disables
requested diagnostic frames, not native guest exception propagation. O1 is only
a development observation; O3 timings still need code-generation review and do
not qualify Wasm memory access or execution with guest threads enabled.

## Diagnostic policy comparison

`bench_exception_trace_policies.py` consumes completed O3/full/pb-o3 instruction
and unwind baseline summaries. It then alternates both policies on the exact same
Wasm bytes and loop count for nine pairs, with CPU 0 affinity and identical
startup-amortization requirements. Use `--workload cross-depth-1 --workload
cross-depth-8` for numeric throw propagation; default selection also measures
ordinary calls. Baseline manifests and binary hashes must match. All actual
diagnostic capture/allocation/cleanup costs remain included. This is not an
isolated measurement of the native unwinder.

`bench_exception_cross_cli.py --comparison NAME` selects individual timing paths.
Native local static throw lowering has a separate exact executable-section and
relocation comparison; it need not be timed when aggressive optimization makes
a synthetic loop too short to amortize process startup. Keep failed calibration
records and report selected comparisons rather than treating an omitted timing
as a pass.
