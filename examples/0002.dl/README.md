# `0002.dl` Example

This directory contains practical preload-DL examples for UWVM2 in both C and C++.

## What the example demonstrates

- how to export `uwvm_get_module_name()`;
- how to export `uwvm_function()` with ordinary imported functions;
- how to export `uwvm_get_custom_handler()` for custom sections;
- how to export `uwvm_set_preload_host_api_v1()` and use the stable preload host API;
- how to handle `copy`, `mmap_full_protection`, `mmap_partial_protection`, and `mmap_dynamic_bounds` delivery states.

The demo function `inspect_memory` performs these steps:

1. locate `memory[0]` through `memory_descriptor_count()` + `memory_descriptor_at()`;
2. read the first four bytes of memory;
3. return the sum of `A`, `B`, `C`, and `D` (`266`);
4. replace the second byte with `Z`.

The companion module `main.wat` verifies all of this in its start function.

## Files

- `interface.h`
  Minimal standalone ABI header for external example code.
- `regdl.c`
  Full C preload-DL example.
- `regdl_cxx.cc`
  Full C++ preload-DL example.
- `main.wat`
  WebAssembly test module importing `add_i32` and `inspect_memory`.

## Build commands

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

## Run commands

Use full compilation with the interpreter backend:

```sh
uwvm -Rcc int -Rcm full --wasm-register-dl ./libregdl_c.dylib dl.example --wasm-set-preload-module-attribute dl.example copy all --run ./main.wasm
```

When the active platform supports mmap-backed native memory, the same example can be run with direct mmap delivery:

```sh
uwvm -Rcc int -Rcm full --wasm-register-dl ./libregdl_c.dylib dl.example --wasm-set-preload-module-attribute dl.example mmap all --run ./main.wasm
```

On platforms without mmap support, use `copy`.

## Expected result

The process exits successfully. The preload DL also emits a diagnostic line to `stderr`, for example:

```text
example-dl-c: state=copy backend=0 bytes=[65,66,67,68] sum=266 ...
```

or:

```text
example-dl-cxx: state=mmap_full_protection backend=0 bytes=[65,66,67,68] sum=266 ...
```

The exact state depends on the configured preload attribute and the runtime memory backend.
