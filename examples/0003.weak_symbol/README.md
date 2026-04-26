# `0003.weak_symbol` Example

This directory mirrors the preload-DL example, but packages the plugin as a **weak-symbol module**.

## What it demonstrates

Exactly the same host-API flow as `examples/0002.dl`:

- ordinary imported functions;
- preload memory host API;
- optional plugin-facing WASI Preview 1 host API.

The difference is only **how the module is registered**:

- DL example: runtime loads a shared library at startup.
- Weak example: the module table is linked directly into the final `uwvm` binary via `uwvm_weak_symbol_module()`.

## Platform support

Weak-symbol preload modules are currently supported only on **ELF** targets.
That means this example is intended for Linux / other ELF platforms and is **not runnable on macOS** or Windows in the current implementation.

## Files

- `weak_symbol.c` — reference C implementation.
- `weak_symbol_cxx.cc` — reference C++ implementation.
- `main.wat` — verification module importing `weak.example`.
- `../0002.dl/interface.h` — shared ABI header used by both example families.

## Build and link (ELF only)

```sh
clang -std=c17 -c weak_symbol.c -o weak_symbol.o
wat2wasm main.wat -o main.wasm
xmake f --ldflags="$(pwd)/weak_symbol.o"
xmake build uwvm
```

## Run (ELF only)

```sh
./build/.../uwvm -Rcc int -Rcm full \
  --wasip1-expose-host-api \
  --wasm-set-preload-module-attribute weak.example copy all \
  --run ./main.wasm hello world
```

Module-specific exposure works the same way:

```sh
./build/.../uwvm -Rcc int -Rcm full \
  --wasip1-module weak.example expose-host-api \
  --wasm-set-preload-module-attribute weak.example copy all \
  --run ./main.wasm hello world
```

## Notes

- `--wasm-set-preload-module-attribute` applies to weak-symbol modules exactly like preload DL modules.
- If you omit `--wasip1-expose-host-api`, `probe_host_apis` returns a failure code and the verification wasm traps.
- The module-specific form `--wasip1-module weak.example hide-host-api` can explicitly turn the API back off even when the global default is on.
- The C and C++ examples intentionally mirror the DL example so you can compare only the registration layer.
