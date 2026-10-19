# External engine environment and correctness

All recorded commands ran through SSH Linux, inside the existing Docker
container and its original 64 GiB / swap-0 cgroup, using `chroot /host`.
Correctness smoke was pinned to CPU 23, run once per case, and was **not timed**.
No PMU/VTune collection or external engine source edit was performed here.

| Same producer artifact | Wasmtime 42.0.1 | Existing WAVM JIT | WasmEdge 0.18.0 JIT | WasmEdge interpreter | WasmEdge AOT |
|---|---|---|---|---|---|
| TeaVM Java full | PASS | LLVM codegen abort | PASS | not run | not run |
| .NET C# default | PASS | missing `sock_accept` | SIGSEGV | PASS | PASS |
| AssemblyScript debug | PASS | PASS | PASS | not run | not run |
| AssemblyScript release | PASS | PASS | PASS | not run | not run |
| Javy JavaScript 50 rounds | PASS | LLVM codegen abort | SIGSEGV | PASS | PASS |
| MoonBit linear Wasm | PASS | PASS | PASS | not run | not run |
| MoonBit WasmGC | PASS | GC type encoding rejected | PASS | not run | not run |

`support-matrix.json` records exact original module/binary hashes and failure
diagnostics. `environment.json` records executable versions and shared-library
hashes. The additional AOT report preserves both original Wasm and derivative
native artifact hashes. A failed engine/case is unavailable for a speed ratio;
it must not be presented as a slow run. Interpreter and compiled modes must be
separate rows. These fixtures exercise their actual features, including MoonBit
GC instructions; they do not establish full WebAssembly 3 conformance.

Wasmtime is available at `/home/macromodel/.wasmtime/bin/wasmtime`, version
`42.0.1 (6844a83b5 2026-02-25)`. Smoke explicitly disabled its object cache and
parallel compilation and enabled the relevant standard Wasm proposals.

WAVM is an existing local build at
`/home/macromodel/Documents/src/WAVM/build-clang/bin/wavm`, reporting
`0.0.0-prerelease`. Its source checkout is commit
`6f871e61c6e8fc54fa5317b5d61d128d681846f3` with tracked modifications in
`Lib/LLVMJIT/LLVMJIT.cpp` and `Lib/Runtime/WAVMIntrinsics.cpp`. The binary is
therefore identified by hash, not certified as an unmodified upstream release.
CMake records RelWithDebInfo, runtime/unwind enabled and LTO, release assertions,
ASAN, UBSAN and TSAN disabled. It links `libWAVM.so.0` from its build directory,
and needs the existing LLVM SDK's libc++, libc++abi and libunwind directories
in `LD_LIBRARY_PATH`. No global loader or Git configuration was changed.
Use `--enable all-proposed`, not `--enable all`, which also activates nonstandard
memory-tagging features. The CLI does not advertise GC/function-reference/tail
call/extended-constant switches. The recorded MoonGC rejection is concrete
evidence that this build cannot participate in that GC comparison.

WasmEdge had a source checkout but no installed binary or build directory.
The official **0.18.0** manylinux_2_28 x86-64 binary archive was downloaded into
the task's tmpfs directory and verified against the release metadata:

- API asset: `https://api.github.com/repos/WasmEdge/WasmEdge/releases/assets/611615391`
  with `Accept: application/octet-stream`.
- SHA-256: `b69973779b395c358dce2004d6b59740d385589a4fe10ae16531bbdd366d1ed7`.
- Executable: `/tmp/high-level-20261017/engines/wasmedge-0.18.0/bin/wasmedge`.
- Its relative RPATH resolves the packaged `lib64/libwasmedge.so.0`; host
  libstdc++/libgcc/glibc and basic system libraries satisfy the remaining links.

The release CLI defaults to `--wasm-3` and interpreter mode. JIT is selected
explicitly by `--run-mode jit`. C#/Javy both failed in that JIT path but passed
in the interpreter and in an independently compiled AOT `.so`, so the report
does not misclassify these failures as missing language support.

Commands and future comparison setup are in `recipes.json`. The AOT recipes
are validated for the C#/Javy smoke modules; compile work belongs in a separate
cold-build measurement. `smoke.py` can rerun one untimed correctness sample per
engine and manifest case. It preserves original modules and writes separate
stdout/stderr logs, kills only its own process group on timeout, and disables
core dumps.
It exits nonzero if any selected engine/case fails; inspect the recorded
diagnostic to distinguish an unsupported case from a runtime failure.

Official references: [WAVM](https://github.com/WAVM/WAVM),
[WasmEdge 0.18.0 release](https://github.com/WasmEdge/WasmEdge/releases/tag/0.18.0),
[WasmEdge proposal support](https://wasmedge.org/docs/start/wasmedge/extensions/proposals/),
[WasmEdge CLI](https://wasmedge.org/docs/start/build-and-run/cli/).
