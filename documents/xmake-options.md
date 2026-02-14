# UWVM Xmake Project Options (`xmake/option.lua`)

This document describes the **project-specific** configuration options defined in `xmake/option.lua` and how they affect compilation/linking when using Xmake.
These flags are specific to this repository (they are not part of upstream Xmake).

To see the authoritative list as your local Xmake prints it, run:

```bash
xmake f --help
```

Project options appear under **Command options (Project Configuration)**.

## How to configure

- Configure (persisted in `.xmake/`): `xmake f [options...]`
- Clean cached configs/detections: `xmake f -c`
- Menu UI (interactive): `xmake f --menu`

Notes on syntax:

- Boolean options are shown as `--name=[y|n]` and accept `y`/`n`.
- String options are shown as `--name=VALUE`.

## Option reference (all options in `xmake/option.lua`)

### `--march=MARCH`

Controls the CPU instruction-set tuning by adding a `-march=...` compiler flag (GCC/Clang).

- **Default:** `default`
- **Values:**
  - `none`: Do not add `-march=...` (use the compiler/toolchain default).
  - `default`: Prefer `-march=native` if supported by the compiler; otherwise fall back to the toolchain default.
  - Any other string (e.g. `x86-64-v3`, `znver4`, `armv8-a`): Adds `-march=<value>`.
- **Impact:** Affects code generation, portability, and performance. `native` produces binaries optimized for the build machine and is usually **not portable** to older CPUs.
- **Example:**
  - `xmake f --march=default`
  - `xmake f --march=x86-64-v3`
  - `xmake f --march=none`

### `--sysroot=SYSROOT`

Controls `--sysroot=...` usage for GCC/Clang and (in some configurations) may also adjust include search paths for libc++-style layouts.

- **Default:** `detect`
- **Values:**
  - `none`: Do not add `--sysroot` (use the compiler/toolchain default sysroot).
  - `detect`: Auto-detect a sysroot for Clang toolchains when possible; use the toolchain default otherwise.
  - Any other string: Treated as a path and passed as `--sysroot=<path>`.
- **Impact:** Changes header/library resolution. Useful for cross-compilation and for LLVM toolchains that ship an external sysroot.
- **Example:**
  - `xmake f --sysroot=detect`
  - `xmake f --sysroot=/opt/llvm/sysroot`

### `--llvm-target=TRIPLET`

Controls the LLVM/Clang target triple when `--use-llvm=y` is enabled.

- **Default:** `detect`
- **Values:**
  - `none`: Do not override the toolchain target.
  - `detect`: Let the build scripts/toolchain choose an appropriate default.
  - Any other string: Treated as a target triple (e.g. `x86_64-w64-mingw32`, `aarch64-apple-darwin`).
- **Impact:** Influences codegen, predefined macros, and the selected runtime/ABI when using Clang.
- **Example:**
  - `xmake f --use-llvm=y --llvm-target=x86_64-w64-mingw32`

### `--rtlib=RTLIB`

Selects Clang’s runtime library via `-rtlib=...`.

- **Default:** `default`
- **Values:** `default`, `libgcc`, `compiler-rt`, `platform`
- **Impact:** Affects which runtime is used for low-level compiler builtins and related support. Typically relevant when using Clang with non-default runtime setups.
- **Example:**
  - `xmake f --rtlib=compiler-rt`

### `--unwindlib=UNWINDLIB`

Selects Clang’s unwind library via `-unwindlib=...`. This option is usually meaningful when `--rtlib=compiler-rt` (unless forced).

- **Default:** `default`
- **Values:**
  - `default`: Do not set `-unwindlib` (use Clang defaults).
  - `libgcc`, `libunwind`, `platform`: Set `-unwindlib=<value>` when appropriate.
  - `force-libgcc`, `force-libunwind`, `force-platform`: Always set `-unwindlib=<value>` regardless of `--rtlib`.
- **Impact:** Controls stack unwinding implementation. This can matter for exceptions, backtraces, and sanitizers depending on your toolchain.
- **Example:**
  - `xmake f --rtlib=compiler-rt --unwindlib=libunwind`
  - `xmake f --unwindlib=force-libgcc`

### `--stdlib=STDLIB`

Selects the C++ standard library implementation by injecting `-stdlib=...` for compilation and linking (toolchain-dependent).

- **Default:** `default`
- **Values:** `default`, `libstdc++`, `libc++`
- **Impact:** Changes the C++ standard library and ABI you link against. Mixed choices across dependencies can cause link/ABI issues.
- **Example:**
  - `xmake f --stdlib=libc++`

### `--strip=STRIP`

Controls symbol stripping behavior.

- **Default:** `default`
- **Values:**
  - `default`: Per-mode default.
    - `debug`/`release`/`releasedbg`: `none`
    - `minsizerel`: `ident`
  - `none`: Do not strip.
  - `symbol`: Strip symbols.
  - `ident`: Strip symbols and also disable compiler ident strings (adds `-fno-ident` in some modes/platforms).
- **Impact:** Affects binary size and debuggability.
- **Example:**
  - `xmake f --strip=symbol`

### `--enable-lto=[y|n]`

Enables/disables link-time optimization (LTO) policy in release-like modes (`release`, `minsizerel`, `releasedbg`).

- **Default:** `y`
- **Impact:** Can improve performance and/or reduce size, but may increase build time and can fail with mismatched compiler/linker combinations (e.g. compiling with Clang but linking with a linker that cannot consume LLVM bitcode).
- **Example:**
  - `xmake f --enable-lto=n`

### `--static=[y|n]`

Enables static linking by adding `-static` at link time (where supported).

- **Default:** `n`
- **Impact:** Produces more self-contained binaries but can significantly increase size and may be incompatible with some platforms/toolchains (e.g. macOS generally does not support fully static system linking in the same way as Linux).
- **Example:**
  - `xmake f --static=y`

### `--use-llvm=[y|n]`

Switches to Clang-based toolchains (Clang-CL on Windows, Clang elsewhere) and may select `lld` as the linker.

- **Default:** `n`
- **Impact:** Changes the compiler/linker frontend, warning behaviors, available flags, and runtime defaults. Combine with `--llvm-target` for cross-targeting.
- **Example:**
  - `xmake f --use-llvm=y`

### `--use-cxx-module=[y|n]`

Enables C++ Modules support in Xmake and defines `UWVM_MODULE`.

- **Default:** `n`
- **Impact:** Switches the build to a module-aware compilation model. Requires compiler support and may change build graph and incremental build behavior.
- **Example:**
  - `xmake f --use-cxx-module=y`

### `--enable-int=ENABLE-INT`

Controls the interpreter backend selection.

- **Default:** `default`
- **Values:**
  - `none`: Disable interpreter support (`UWVM_DISABLE_INT`).
  - `default`: Enable and use the default interpreter (`UWVM_USE_DEFAULT_INT`).
  - `uwvm-int`: Enable and use the UWVM interpreter (`UWVM_USE_UWVM_INT`).
- **Example:**
  - `xmake f --enable-int=uwvm-int`

### `--enable-jit=ENABLE-JIT`

Controls the JIT backend selection.

- **Default:** `default`
- **Values:**
  - `none`: Disable JIT support (`UWVM_DISABLE_JIT`).
  - `default`: Enable and use the default JIT (`UWVM_USE_DEFAULT_JIT`).
  - `llvm`: Enable and use LLVM as the JIT engine (`UWVM_USE_LLVM_JIT`).
- **Example:**
  - `xmake f --enable-jit=llvm --use-llvm=y`

### `--enable-debug-int=[y|n]`

Enables extra debug support for the interpreter (build-time define `UWVM_ENABLE_DEBUG_INT`).

- **Default:** `y`
- **Impact:** Intended for development/debug builds; may have runtime overhead or extra checks/logging depending on implementation.
- **Example:**
  - `xmake f --enable-debug-int=n`

### `--enable-uwvm-int-combine-ops=MODE`

Controls “combined opcode” optimizations for `uwvm-int`.

- **Default:** `heavy`
- **Values:**
  - `none`: Disable combine-op optimizations.
  - `soft`: Enable only soft/light combinations.
  - `heavy`: Enable soft + heavy combinations.
  - `extra`: Enable soft + heavy + extra-heavy combinations.
- **Impact:** Trades compiler/build complexity and code size for potential interpreter performance improvements.
- **Example:**
  - `xmake f --enable-int=uwvm-int --enable-uwvm-int-combine-ops=soft`

### `--detailed-debug-check=[y|n]`

Enables a more detailed debug checking mode in **debug** builds (defines `UWVM_ENABLE_DETAILED_DEBUG_CHECK` when `-m debug` is used).

- **Default:** `y`
- **Impact:** Adds extra checks in debug mode; may slow down execution.
- **Example:**
  - `xmake f -m debug --detailed-debug-check=y`

### `--mingw-min=MINGW-MIN`

Selects the minimum supported Windows version for MinGW targets by setting `_WIN32_WINNT`, `_WIN32_WINDOWS`, and/or `WINVER`.

- **Default:** `default` (effectively aligned with a modern Windows baseline)
- **Values:** `default` plus many specific Windows/Windows Server identifiers (e.g. `WIN10`, `WIN7`, `WS22`, `WINXP`, `WIN98`, `NT400`, …).
- **Impact:** Controls availability of Win32 APIs at compile-time and may influence subsystem/link settings. Lowering the minimum can require additional compatibility work and may limit available APIs.
- **Example:**
  - `xmake f -p mingw --mingw-min=WIN7`

### `--apple-platform=APPLE-PLATFORM`

Selects an Apple platform target and minimum OS version (macOS/iOS/tvOS/watchOS/visionOS). Intended to affect flags like `-mtargetos=...` and `-m*-version-min=...`.

- **Default:** `default` (use the compiler defaults)
- **Values:**
  - Predefined constants such as `MACOS_SONOMA`, `IOS_17`, `TVOS_16`, `WATCHOS_10`, `VISIONOS_1`, etc.
  - Custom format: `"platform:version"` (e.g. `macos:10.15`, `ios:13.0`).
- **Impact:** Controls deployment target and availability of platform APIs.
- **Example:**
  - `xmake f -p macosx --apple-platform=MACOS_SONOMA`
  - `xmake f -p iphoneos --apple-platform=ios:16.0`

### `--test-libfuzzer=[y|n]`

Enables a libFuzzer-oriented test mode (currently intended for LLVM environments).

- **Default:** `n`
- **Impact:** Alters how certain test targets are configured/built. Typically combined with `--use-llvm=y`.
- **Example:**
  - `xmake f --use-llvm=y --test-libfuzzer=y`

### `--debug-timer=[y|n]`

Enables timer functionality in modules (defines `UWVM_TIMER`).

- **Default:** `n`
- **Impact:** Useful for profiling and diagnostics; may add overhead.
- **Example:**
  - `xmake f --debug-timer=y`

### `--fno-exceptions=[y|n]`

Disables C++ exceptions for the project (sets Xmake exception policy `no-cxx`). This is explicitly documented as potentially unsafe for some UWVM/WASI behaviors.

- **Default:** `n`
- **Impact:** Reduces binary size and runtime overhead in some cases, but any code paths relying on exceptions will change behavior (including potential crashes if failures are handled via exceptions).
- **Example:**
  - `xmake f --fno-exceptions=y`

### `--use-multithread-allocator-memory=[y|n]`

Enables a multithread allocator memory mode (defines `UWVM_USE_MULTITHREAD_ALLOCATOR`). Intended for platforms that lack `mmap` but support multithreading.

- **Default:** `n`
- **Impact:** Changes allocator/memory strategy; platform-specific.
- **Example:**
  - `xmake f --use-multithread-allocator-memory=y`

### `--disable-local-imported-wasip1=[y|n]`

Disables importing the local WASI Preview 1 (wasip1) implementation during compilation (defines `UWVM_DISABLE_LOCAL_IMPORTED_WASIP1`). For platforms that do not support wasip1, the build scripts may ignore this option.

- **Default:** `n`
- **Impact:** Changes which WASI implementation is compiled/linked into the project.
- **Example:**
  - `xmake f --disable-local-imported-wasip1=y`

### `--enable-local-imported-wasip1-wasm64=[y|n]`

Enables wasip1 extensions for the wasm64 variant (defines `UWVM_ENABLE_LOCAL_IMPORTED_WASIP1_WASM64`).

- **Default:** `y`
- **Prerequisite:** Intended to be used only when `--disable-local-imported-wasip1=n`.
- **Example:**
  - `xmake f --enable-local-imported-wasip1-wasm64=y`

### `--enable-local-imported-wasip1-socket=MODE`

Controls socket-related extensions in wasip1.

- **Default:** `wasip1`
- **Prerequisite:** Intended to be used only when `--disable-local-imported-wasip1=n`.
- **Values:**
  - `none`: Disable socket extensions.
  - `wasip1`: Enable the official wasip1 socket extension (`UWVM_ENABLE_WASIP1_SOCKET`).
  - `wasix`: Enable the wasix-flavored extension (`UWVM_ENABLE_WASIX_VER_WASIP1_SOCKET`). One noted difference is `sock_accept` having an additional `ro_addr` parameter.
- **Example:**
  - `xmake f --enable-local-imported-wasip1-socket=wasix`
