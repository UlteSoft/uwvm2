# WASM‑WASI Build Guide

Scope and limitations
- Purpose: self‑hosting to demonstrate uwvm2 portability. Usable but not polished.
- Targets: `wasm32-unknown-wasi` and `wasm64-unknown-wasi`, with WASI levels p1–p3 depending on your sysroot.
- Threads: not supported. Do not enable WASM threads or shared memory.
- Web runtimes: wasm‑JS is not provided; browsers require an adapter/translation layer.

Prerequisites
- LLVM/Clang with WebAssembly target and LLD (Clang ≥ 21 recommended).
- A compatible WASI sysroot (wasip1/wasip2/wasip3).
- Optional: a WASI runtime to run the resulting module (e.g., wasmtime/wasmer/wasmedge).

Build with xmake
1) Install xmake and LLVM.
2) Configure the target platform and architecture, providing your WASI sysroot.
   Example:
   ```sh
   xmake f -p [wasm-wasip1|wasm-wasip2] -a [wasm32|wasm64] \
     --sysroot=$HOME/sysroot/wasm-sysroots/wasm-sysroot/wasm32-wasip1 \
     --links=unwind
   xmake
   ```
   Notes:
   - Threaded variants (e.g., `wasm-wasip1-threads`) are not supported by uwvm2 and are for experimentation only.
   - Adjust `--sysroot` to match your WASI level and architecture.
3) Install:
   ```sh
   xmake i -o <install_path>
   ```

Build with clang + lld
- Select exec model and features as appropriate. Avoid threads/atomics.

Baseline feature set (clang `-mattr`; names may vary by LLVM version)
- `+mutable-globals`, `+nontrapping-fptoint`, `+sign-ext`, `+multivalue`, `+extended-const`, `+bulk-memory`, `+reference-types`
- Optional: `+simd128` (portable), `+relaxed-simd` (newer runtimes), `+multimemory`, `+tail-call`
- `+memory64` is required for `wasm64`
- Keep atomics disabled: `-mattr=-atomics` (no `-pthread`, no shared memory)

Recommended `-mattr` sets
- wasm32 + wasip1 (baseline):
  ```
  -mattr=+mutable-globals,+nontrapping-fptoint,+sign-ext,+multivalue,+extended-const,+bulk-memory,+reference-types,-atomics
  ```
- wasm64 + wasip3 (advanced):
  ```
  -mattr=+mutable-globals,+nontrapping-fptoint,+sign-ext,+multivalue,+extended-const,+bulk-memory,+reference-types,+multimemory,+memory64,+simd128,+relaxed-simd,+tail-call,-atomics
  ```

Example commands
- Command-style entry (WASI command), wasm32 + wasip1:
  ```sh
  clang++ -std=c++26 --target=wasm32-unknown-wasi -O2 -DNDEBUG -fuse-ld=lld \
    -mexec-model=command \
    -mattr=+mutable-globals,+nontrapping-fptoint,+sign-ext,+multivalue,+extended-const,+bulk-memory,+reference-types,-atomics \
    --sysroot=/path/to/wasip1-sysroot \
    -Wl,--strip-all -o uwvm2.wasm @objects.rsp
  ```
- Reactor-style entry (exports callable from host), wasm64 + wasip3:
  ```sh
  clang++ -std=c++26 --target=wasm64-unknown-wasi -O2 -DNDEBUG -fuse-ld=lld \
    -mexec-model=reactor \
    -mattr=+mutable-globals,+nontrapping-fptoint,+sign-ext,+multivalue,+extended-const,+bulk-memory,+reference-types,+multimemory,+memory64,+simd128,+relaxed-simd,+tail-call,-atomics \
    --sysroot=/path/to/wasip3-sysroot \
    -Wl,--export=uwvm_main -Wl,--strip-all -o uwvm2.wasm @objects.rsp
  ```

Tips
- Pick the WASI level by pointing `--sysroot` to the corresponding sysroot.
- If a feature flag is unrecognized, drop it or query supported features:
  ```sh
  llc -mtriple=wasm32-unknown-wasi -mattr=help
  ```
- Prefer DWARF exceptions for portability; wasm EH is optional (`-fwasm-exceptions`).

Additional options
- `--use-cxx-module=y` Use C++ modules if your compiler supports them.
