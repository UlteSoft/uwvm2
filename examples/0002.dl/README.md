# `0002.dl` Example

This directory now shows a **complete preload-DL plugin lifecycle** instead of only a toy function.
It demonstrates three things together:

- ordinary imported functions (`add_i32`);
- the stable preload memory host API (`inspect_memory`);
- the optional WASI Preview 1 host API exposed to plugins (`probe_host_apis`).

## Exported symbols

The C and C++ examples both export:

- `uwvm_get_module_name()`
- `uwvm_function()`
- `uwvm_get_custom_handler()`
- `uwvm_set_preload_host_api_v1()`
- `uwvm_set_wasip1_host_api_v1()`

## What the plugin does

### `add_i32`
A plain imported function used to show the normal C ABI registration path.

### `inspect_memory`
Uses the stable preload host API to:

1. locate `memory[0]`;
2. read the first four bytes (`A`, `B`, `C`, `D`);
3. return their sum (`266`);
4. overwrite byte `1` with `Z`.

### `probe_host_apis`
Uses **both** host APIs together:

1. receives two guest memory offsets as parameters;
2. calls `args_sizes_get()` through the plugin-facing WASI P1 API;
3. reads the written `argc` and `argv_buf_size` values back through the preload memory API;
4. returns `0` on success or a negative diagnostic code on failure.

This is the important part of the new example: it shows that a preload plugin can call a WASI function using the **guest's own linear memory**.

## Files

- `interface.h` — standalone example ABI header shared by DL and weak-symbol examples.
- `regdl.c` — reference C implementation.
- `regdl_cxx.cc` — reference C++ implementation.
- `main.wat` — verification module that imports all three functions.

## Build

### macOS

```sh
clang -std=c17 -dynamiclib regdl.c -o libregdl_c.dylib
clang++ -std=c++20 -dynamiclib regdl_cxx.cc -o libregdl_cxx.dylib
wat2wasm main.wat -o main.wasm
```

### Linux

```sh
cc -std=c17 -shared -fPIC regdl.c -o libregdl_c.so
c++ -std=c++20 -shared -fPIC regdl_cxx.cc -o libregdl_cxx.so
wat2wasm main.wat -o main.wasm
```

## Run

### Positive path: expose WASI P1 to the plugin

```sh
uwvm -Rcc int -Rcm full \
  --wasip1-expose-host-api \
  --wasm-register-dl ./libregdl_c.dylib dl.example \
  --wasm-set-preload-module-attribute dl.example copy all \
  --run ./main.wasm hello world
```

### Optional: override WASI `argv[0]`

If you want the guest-visible program name to differ from the wasm file path, add `--wasip1-set-argv0` **before** `--run`:

```sh
uwvm -Rcc int -Rcm full \
  --wasip1-expose-host-api \
  --wasip1-set-argv0 main \
  --wasm-register-dl ./libregdl_c.dylib dl.example \
  --wasm-set-preload-module-attribute dl.example copy all \
  --run ./main.wasm hello world
```

With that override, the guest sees `argv = ["main", "hello", "world"]`, and the plugin-side `args_sizes_get()` result changes accordingly.

### Negative path: do **not** expose WASI P1

If you remove `--wasip1-expose-host-api`, the plugin still loads, but `probe_host_apis` returns a failure code and the verification wasm traps in its start function.

```sh
uwvm -Rcc int -Rcm full \
  --wasm-register-dl ./libregdl_c.dylib dl.example \
  --wasm-set-preload-module-attribute dl.example copy all \
  --run ./main.wasm hello world
```

## Expected output

The process succeeds in the positive path and prints diagnostics similar to:

```text
example-dl-c: state=copy backend=0 bytes=[65,66,67,68] sum=266
example-dl-c: wasi argc=3 argv_buf_size=24
```

`argc` includes `argv[0]` (the wasm path passed after `--run`), so the exact buffer size also depends on that path.
