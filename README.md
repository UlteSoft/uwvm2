<div align="center">
  <img src="documents/logo/uwvm2.svg" alt="uwvm2 logo" width="240"/>
  <br/>
  <h1>Ultimate WebAssembly Virtual Machine 2</h1>
  <p>
    <img src="https://img.shields.io/badge/Beta-2ea043?style=for-the-badge" alt="Beta"/>
    <img src="https://img.shields.io/badge/V2.0.1.1-ffffff?style=for-the-badge" alt="V2.0.1.1"/>
  </p>
</div>

<div style="text-align:center">
    <a href="https://github.com/UlteSoft/uwvm2/actions?query=workflow%3ACI+branch%3AV2.0.1.x">
        <img src="https://img.shields.io/github/actions/workflow/status/UlteSoft/uwvm2/ci.yml?branch=V2.0.1.x" alt="github-ci" />
    </a>
    <a href="LICENSE.md">
        <img src="https://img.shields.io/badge/License-Apache%202.0-green.svg" , alt="License" />
    </a>
    <a href="https://en.cppreference.com">
        <img src="https://img.shields.io/badge/language-c++26-blue.svg", alt="cppreference" />
    </a>
</div>

## Introduction
Ultimate WebAssembly Virtual Machine 2 (`uwvm2`) is a cross-platform WebAssembly runtime and toolchain project implemented in modern C++26, with a strong emphasis on standards compliance, portability, and interpreter-first execution.

## Changelog
See [CHANGELOG.md](CHANGELOG.md) for release history, distribution baseline commits, fixes, support tracking, and deprecation records.

## Features
- Direct-threaded `u2` interpreter with a register-ring stack-top cache and translation-time fusion.
- Very fast interpreter full-translation performance, with support for multi-threaded full translation on hosted builds.
- Spec-oriented WebAssembly parser and validation pipeline with structured diagnostics and extensive fuzz testing.
- Cross-platform WASI Preview 1 host integration, plus stable APIs for preload modules and native plugins.
- Multiple linear-memory backends and broad platform coverage across POSIX, Windows, DOS, BSD, WASI, Emscripten, and selected freestanding targets.

### WebAssembly feature coverage
UWVM2 supports a broad set of WebAssembly standards and extensions. See [features.md](documents/features.md). For the mapping between supported functionality and official WebAssembly release milestones, see [wasm-release.md](documents/wasm-release.md).

### Broad platform coverage
UWVM2 targets more than 100 platform triplets across DOS-family systems, POSIX-family systems, Windows 9x, Windows NT, hosted C library environments, and selected freestanding environments. See [support.md](documents/support.md) for details.

### Runtime execution backends
UWVM2 currently ships interpreter-oriented runtime compiler backends, including the high-performance `u2` interpreter and a debug interpreter for observability and correctness work. The `u2` pipeline emphasizes very fast full translation with low translation overhead, and hosted builds support multi-threaded full-translation scheduling. LLVM-based JIT integration exists as early groundwork and remains a work in progress. See [runtime compiler documentation](src/uwvm2/runtime/compiler/readme.md). For the `u2` interpreter architecture, see [u2 interpreter documentation](src/uwvm2/runtime/compiler/uwvm_int/readme.md).

### Standard parser and validation pipeline
UWVM2 includes a high-performance, spec-compliant WebAssembly binary parser and validation stack built on concept-oriented C++26, with SIMD-aware design and extensive fuzzing for safety and robustness. See [readme.md](src/uwvm2/parser/readme.md) for details.

### WASI Preview 1 host integration
WebAssembly System Interface Preview 1 (WASI P1) host bindings for `wasm32-wasip1` and `wasm64-wasip1` targets, built on the same cross-platform runtime as UWVM2 and exposing file-system and related services to WebAssembly modules. See [imported/readme.md](src/uwvm2/imported/readme.md) for details.

### Plugin and preload module integration
Preload modules (dynamic libraries / weak-symbol plugins) can consume a stable host-side API for linear-memory access and can optionally consume the UWVM2 WASI Preview 1 host API as well. The WASI table is disabled by default and must be explicitly enabled from the command line.

### Flexible linear-memory backends
UWVM2 provides three host-side models for implementing WebAssembly linear memory (`mmap`-based, multithreaded allocator-based, and single-thread allocator-based backends), allowing efficient execution on platforms with or without virtual-memory support. See [readme.md](src/uwvm2/object/memory/readme.md) for a detailed description.

## Command-Line Interface
* Get version information
```bash
$ uwvm --version
```
* Get a list of commands
```bash
$ uwvm --help
```
* Run the UWVM2 virtual machine
```bash
$ uwvm <param0> <param1> ... --run <wasm> <argv1> <argv2> ...
```
* Mount a WASI directory (`wasip1`)
```bash
$ uwvm --wasip1-mount-dir <wasi dir> <system dir> ... --run ...
```
* Expose WASI Preview 1 to preload plugins
```bash
$ uwvm --wasm-expose-wasip1-host-api --wasm-register-dl <plugin> <module> --run ...
```

## How to Build
* Windows (aka. unknown-windows-msvc). See [windows.md](documents/how-to-build/windows.md)
* MinGW (aka. unknown-windows-gnu, unknown-w64-mingw32). See [mingw.md](documents/how-to-build/mingw.md)
* Linux (aka. unknown-linux-unknown). See [linux.md](documents/how-to-build/linux.md)
* Darwin (aka. unknown-apple-darwin). See [darwin.md](documents/how-to-build/darwin.md)
* FreeBSD (aka. unknown-freebsd(Version)). See [freebsd.md](documents/how-to-build/freebsd.md)
* WASM-WASI (self-hosting) (aka. [wasm32|wasm64]-[wasip1|wasip2]-(threads)). See [wasm-wasi.md](documents/how-to-build/wasm-wasi.md)
* Other platforms: See [how-to-build](documents/how-to-build)

## Support / Donations

If you find this project useful, you can support its development with a crypto donation. See [donations.md](donations.md)
