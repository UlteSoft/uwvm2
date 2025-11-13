# WASM‑Emscripten Build Guide

Scope and limitations
- Purpose: self‑hosting to demonstrate uwvm2 portability. Usable but not polished.
- Targets: `wasm32` and `wasm64` via Emscripten toolchain.
- Threads: not supported. Do not enable WASM threads or shared memory.
- Web runtimes: this repo does not provide JS glue or HTML. Prefer standalone WASM builds.

Prerequisites
- Emscripten SDK (emsdk) ≥ 2.0.18, with LLVM/Clang ≥ 21.
- Activate environment before building:
  ```sh
  # install and activate (examples)
  git clone https://github.com/emscripten-core/emsdk.git
  cd emsdk
  ./emsdk install latest
  ./emsdk activate latest
  source ./emsdk_env.sh   # or emsdk_env.bat on Windows
  ```
- Verify tools:
  ```sh
  emcc -v
  em++ -v
  wasm-objdump --version
  ```

Build with xmake (recommended)
1) Install xmake and emsdk; activate emsdk env.
2) Configure target platform and architecture:
   ```sh
   # wasm32
   xmake f -p wasm-emscripten -a wasm32 \
     --cxflags="-sSTANDALONE_WASM=1 -sWASI=1 -sBULK_MEMORY=0" \
     --ldflags="-sSTANDALONE_WASM=1 -sWASI=1 -sBULK_MEMORY=0"
   xmake

   # wasm64
   xmake f -p wasm-emscripten -a wasm64 \
     --cxflags="-sSTANDALONE_WASM=1 -sWASI=1 -sMEMORY64=1 -sBULK_MEMORY=0" \
     --ldflags="-sSTANDALONE_WASM=1 -sWASI=1 -sMEMORY64=1 -sBULK_MEMORY=0"
   xmake
   ```
   Notes:
   - `-s STANDALONE_WASM=1` produces a pure `.wasm` module without JS glue.
   - `-s WASI=1` makes the module import `wasi_snapshot_preview1` syscalls (suited to WASI runtimes).
   - `-s BULK_MEMORY=0` avoids emitting the DataCount section (section id 12). This prevents uwvm2 parser errors like: Illegal Section ID: "12".
   - For `wasm64`, add `-s MEMORY64=1`.

3) Optional: install
   ```sh
   xmake i -o <install_path>
   ```

Feature profile and compatibility
- Keep atomics/pthreads disabled: no `-pthread`, no `-s USE_PTHREADS=1`.
- If you still hit a parser incompatibility:
  - Disable reference types: add `-sREFERENCE_TYPES=0`.
  - Alternatively, disable features via Clang attributes: `-mattr=-bulk-memory` (less preferred with Emscripten; prefer `-s` flags).

Exports and entry
- Standalone WASM with `-sSTANDALONE_WASM=1 -sWASI=1` provides `_start` by default for command-style execution.
- If you need specific exports, use e.g.:
  ```sh
  --ldflags="-sEXPORTED_FUNCTIONS=['_uwvm_main','_main']"
  ```

Manual em++ examples
- wasm32 (standalone command-style):
  ```sh
  em++ -std=c++26 -O2 -DNDEBUG \
    -sSTANDALONE_WASM=1 -sWASI=1 -sBULK_MEMORY=0 \
    --target=wasm32-unknown-emscripten -fuse-ld=lld \
    @objects.rsp -o uwvm.wasm
  ```
- wasm64 (standalone command-style):
  ```sh
  em++ -std=c++26 -O2 -DNDEBUG \
    -sSTANDALONE_WASM=1 -sWASI=1 -sMEMORY64=1 -sBULK_MEMORY=0 \
    --target=wasm64-unknown-emscripten -fuse-ld=lld \
    @objects.rsp -o uwvm.wasm
  ```

Tips
- Prefer DWARF exceptions for portability; wasm EH is optional (`-fwasm-exceptions`).
- If a flag is unrecognized by your emsdk version, drop it or run:
  ```sh
  emcc --help
  wasm-objdump --help
  ```

Additional options
- `--use-cxx-module=y` Use C++ modules if your compiler supports them.
