# Matching LLVM 23.1.1 runtime configuration for ordinary uwvm2

`paired-static.cmake` is byte-identical to the ROS vendored runtime profile. It is a reviewable build configuration; it does not copy a complete LLVM tree into this repository or change the default external toolchain automatically.

Use the official LLVM 23.1.1 source archive, commit `6dfe1677ab8dffbc6ec13d53a1e0215d75147689`, archive SHA-256 `ebe9be46fe8756d58c5b198ffad0fa2a766257add81a4dc52179bfacc7888ee6` (179168672 bytes), and its `runtimes` CMake entry. Alternatively, an explicitly pinned external source directory may use the exact ROS subset and its per-file manifest. Bootstrap compiler/target/sysroot, generated configuration and all paired archives must be bound independently. The ROS source importer and manifest are in `uwvm2-ros/third-parties/llvm-runtimes`; neither user toolchain files nor the ordinary source tree are patched by this configuration.

Example configure shape (only the sole Linux keeper executes builds under the existing 64GiB scope/E-core affinity):

```sh
cmake -S "$task_runtime_source/runtimes" -B "$task_runtime_build" -G Ninja \
  -C tools/ci/llvm-runtimes-23.1.1/paired-static.cmake \
  -DCMAKE_C_COMPILER="$task_clang" -DCMAKE_CXX_COMPILER="$task_clangxx" \
  -DCMAKE_INSTALL_PREFIX="$task_runtime_prefix"
cmake --build "$task_runtime_build" --target cxx_static cxxabi_static unwind_static generate-cxx-headers --parallel 4
```

Use fresh private directories and record actual CMake cache/File API, compiler argv, headers, archive hashes and final symbol/loader closure. The LLVM/app/runtime objects need the same generated config-site and headers. Do not combine one static unwinder with dynamic libc++abi from a second provider. The Linux final ELF must not depend on external libc++.so/libc++abi.so/libunwind.so, and compiler driver defaults must not introduce a competing EH provider. Host libc and compiler builtins remain required inputs. No global installation or toolchain replacement is authorized by this recipe.

For an isolated shared-unwinder assertion comparison, use the same pinned source and two fresh builds with `LLVM_ENABLE_RUNTIMES=libunwind`, shared ON/static OFF, tests/docs OFF, Release, threads ON and identical frame/cache/architecture flags; change only `LIBUNWIND_ENABLE_ASSERTIONS=ON` versus OFF. Keep the private SONAME-compatible DSO prefixes separate from the paired-static product build. Validate `_Unwind_*`, `unw_*`, C++ throw/catch and actual dynamically registered JIT CFI using the existing cold fixtures before any P0 measurement. Record actual loader identity/CFI and retained exception trace/catch_ref/rethrow behavior. Existing installed unwinder provenance is unknown, so it is not the same-source assertion-ON baseline.

The configuration alone does not establish ABI/CFI or performance acceptance. The Linux qualification below applies only to the recorded matched build. Windows/Darwin/BSD providers and cross-target sysroots require separate qualification.

The local `EHHeaderParser.hpp` patch specializes the usual ELF `datarel|sdata4` FDE table. It bounds the complete fixed-stride extent before binary search, preserves signed offsets and the final FDE PC-range check, and retains upstream fallback for every other encoding. Exact original/patched hashes and patch identity are in `eh-fixed-encoding.json`. Apply it to a fresh private source tree with `python3 apply-eh-fixed-encoding.py "$task_runtime_source"`; `--check` verifies identity without writing. Build all three paired archives and matching headers together. Actual installation targets are `install-cxx-headers`, `install-cxx`, `install-cxxabi`, `install-unwind`, and `install-unwind-headers`.

On Linux x86-64, a fresh paired-static build in the 64 GiB cgroup passed actual JIT landing-pad tests (18,313 checks, 1,152 executions) and four-reader concurrent CFI publication/retirement tests (48 registrations and eight failed-publication rollbacks). Its ELF had no dynamic libc++/libc++abi/libunwind/libgcc EH provider. These results qualify the tested Linux build; other targets need separate tests. The patch changes table lookup, not cursor ABI, frame registration or CFI interpretation.
