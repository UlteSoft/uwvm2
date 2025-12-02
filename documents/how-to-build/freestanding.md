# Freestanding Environment Build Guide

> Note: this guide targets **freestanding C++26 environments** where you embed the uwvm2 **parser** (and the future **interpreter**) into your own runtime. It assumes you provide your own global heap implementation and low‑level runtime.

The freestanding environment is described at a high level in [`documents/support.md`](../support.md) under **“Freestanding Environment”**. This page focuses on how to actually build and integrate uwvm2 components in such an environment.

## Scope and limitations

- **Supported components**
  - ✅ WebAssembly parser (`src/uwvm2/parser/**`)
  - 🚧 Future interpreter (INT) – planned; see [`documents/development-status.md`](../development-status.md)
- **Not supported in freestanding mode**
  - CLI front‑end (`uwvm` binary)
  - WASI host / OS integration
  - Dynamic loading, threads, processes, filesystems, etc.
- **You are responsible for**
  - Providing a **global heap** that satisfies the C++ heap model.
  - Implementing `fast_io::custom_global_allocator` and enabling it via compile‑time macros.
  - Providing any required startup code (CRT0, vector tables, linker script, etc.).

## Environment assumptions

Summarizing `support.md` and the parser documentation:

- **C++ standard**
  - Full **ISO C++26** is required by uwvm2.
- **Compilers**
  - GCC ≥ 16
  - LLVM Clang ≥ 21
- **Targets / ABI examples**
  - ELF: `unknown-unknown-elf` (no OS, freestanding)
  - PE: `unknown-windows-gnu` with `-ffreestanding`
- **`CHAR_BIT` and byte layout**
  - Platforms where `CHAR_BIT != 8` are allowed.
  - You **must normalize** the WebAssembly byte stream: clear any bits above the least‑significant 8 bits in each addressable byte before passing it to the parser. Violating this is undefined behaviour.
- **Exceptions and dynamic memory**
  - The parser is designed for environments where **exceptions and dynamic memory are available**.
  - For very restricted targets you may disable exceptions, but then **parse failures will typically become hard‑fail paths**; see the parser README for trade‑offs.

For more background, see:

- [`src/uwvm2/parser/readme.md`](../../src/uwvm2/parser/readme.md) – especially the section *“Heap and allocator requirements”*.
- [`documents/support.md`](../support.md) – section *“Freestanding Environment”*.

## Custom global heap / allocator

In a freestanding build, **uwvm2 does not provide a default global allocator**. Instead, all heap behaviour is delegated to user code via `fast_io`:

- `fast_io` exposes a **custom global allocator hook**:
  - `fast_io::custom_global_allocator` (forward‑declared in `fast_io_core_impl/allocation/custom.h`).
- uwvm2 selects the allocator through the macro `FAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR`:
  - When this macro is defined, `fast_io` routes **all global allocations** used by uwvm2 through `custom_global_allocator`.

### 1. Implement `fast_io::custom_global_allocator`

You must provide a definition of `fast_io::custom_global_allocator` in your own code, typically in a freestanding runtime or platform layer:

```cpp
// Example *shape* only – consult fast_io's documentation for the exact concept.
namespace fast_io {

class custom_global_allocator {
  // Implement the interface required by fast_io's allocator abstraction.
  // This usually includes allocating and deallocating raw memory regions
  // in a way that is safe for multi-module use.
};

} // namespace fast_io
```

Important:

- The precise interface is defined by **fast_io** itself.
- Your implementation should:
  - Allocate memory from your **global heap** (static arena, custom MMU, RTOS heap, etc.).
  - Be safe for all allocations that uwvm2 may perform (parser + future interpreter).
  - Respect any alignment and size requirements enforced by fast_io.

### 2. Enable the custom allocator

When compiling uwvm2 sources for a freestanding target, define:

- `FAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR`

This can be done via your build system, for example:

```sh
# Clang example (generic, not tied to xmake)
clang++ -std=c++26 -O2 -DFAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR \
  -ffreestanding --target=unknown-unknown-elf \
  -I/path/to/uwvm2/src \
  -I/path/to/uwvm2/third-parties/fast_io/include \
  ...
```

Without this macro, fast_io will fall back to its own native allocator (or other configured back‑ends such as mimalloc), which may **not** exist or be suitable in your freestanding environment.

## Building only the parser (and future interpreter)

In freestanding mode you normally do **not** build the full `uwvm` CLI. Instead, you embed only the components you need:

- **Today**
  - Embed the **parser** as a library (headers/modules under `src/uwvm2/parser/**` plus supporting utilities under `src/uwvm2/utils/**` and `src/uwvm2/object/**`).
- **Future**
  - When the interpreter (INT) is implemented, you can embed its execution engine in a similar way.

Typical integration pattern:

1. **Add uwvm2 as a submodule** (or vendor the relevant directories).
2. **Include the parser headers and modules** in your build:
   - For classic includes: include from `src/uwvm2/parser/**` and `src/uwvm2/utils/**`.
   - For C++ modules: add the `*.cppm` files under `src/uwvm2/parser/**` and its dependencies, similar to how `xmake.lua` does it when `--use-cxx-module=y` is set.
3. **Do not link** the CLI entry points (`src/uwvm2/uwvm/main.*`) or WASI integration layers into your freestanding binary.
4. Implement `fast_io::custom_global_allocator` and compile with `-DFAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR`.

## Using xmake as a helper (optional)

Currently there is **no dedicated `freestanding` platform** in `xmake.lua`. Instead, you typically:

- Use xmake on a **hosted platform** (Linux, macOS, etc.) for development and testing.
- For the **actual freestanding target**, build uwvm2 as part of your own project with your cross toolchain.

However, some existing xmake options and platform helpers are still informative:

- `xmake/option.lua`
  - `--enable-int=[none|default|uwvm-int]` controls whether the interpreter is built when it is available.
  - `--use-llvm=y` switches to the LLVM/Clang toolchain.
  - `--sysroot=...` and `--target=...` configure cross‑compilation.
- `xmake/platform/none.lua`
  - Shows how `--sysroot` and `--target` flags are forwarded to the compiler and linker.

For a custom freestanding build you can mirror this behaviour in your own build system:

```sh
# Example: cross‑compiling parser to ELF freestanding
TARGET=unknown-unknown-elf
SYSROOT=/path/to/your/sysroot

clang++ -std=c++26 -O2 -DNDEBUG -ffreestanding \
  --target=${TARGET} --sysroot=${SYSROOT} \
  -DFAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR \
  -I/path/to/uwvm2/src \
  -I/path/to/uwvm2/third-parties/fast_io/include \
  -c your_parser_entry.cpp
```

On Windows‑style freestanding targets you would instead use a MinGW‑like triplet with `-ffreestanding`, for example `x86_64-unknown-windows-gnu`.

## Example high‑level integration flow

1. **Define your heap**
   - Set aside a contiguous memory region (static array, linker script section, or RTOS heap).
   - Provide allocation primitives (allocate, free, optionally reallocate) on top of that region.
2. **Implement `fast_io::custom_global_allocator`**
   - Use your primitives to satisfy the interface required by fast_io.
   - Ensure the implementation is safe in the presence of all uwvm2 allocations.
3. **Compile uwvm2 parser for your target**
   - Use your cross compiler with `-ffreestanding`, `--target=...`, and C++26.
   - Add include paths for `src/` and `third-parties/fast_io/include`.
   - Define `FAST_IO_USE_CUSTOM_GLOBAL_ALLOCATOR`.
4. **Link into your freestanding runtime**
   - Provide your own `main` (or equivalent entry) that:
     - Initializes your heap (if required).
     - Initializes any device I/O you will use for diagnostics.
     - Calls into uwvm2 parser APIs to parse and validate WebAssembly modules.

## Caveats and recommendations

- **Testing on hosted platforms first**
  - It is recommended to first build and test the parser on a hosted environment (Linux, macOS, Windows) using the regular build guides, then move the same configuration to a freestanding build.
- **Diagnostics and logging**
  - In a minimal environment, you may only have a UART or memory buffer for diagnostics. Ensure that any I/O layer you rely on is compatible with fast_io’s expectations.
- **Upgrades**
  - As the interpreter and additional proposals are implemented, this document will be extended to describe their freestanding integration details.
