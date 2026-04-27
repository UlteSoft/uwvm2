# UWVM Command-Line Reference

This directory is the source-driven command-line reference for the `uwvm` executable. It expands the compact `uwvm --help` output into behavior notes, parameter interaction rules, platform gates, path handling details, and practical combinations.

The command registry and callbacks used for this reference are primarily in:

- `src/uwvm2/uwvm/cmdline/params.h`
- `src/uwvm2/uwvm/cmdline/parser.h`
- `src/uwvm2/uwvm/cmdline/params/**`
- `src/uwvm2/uwvm/cmdline/callback/**`
- selected runtime, Wasm loader, logging, and WASI storage headers referenced by those callbacks

Build-time `xmake f ...` switches are documented separately in [`../xmake-options.md`](../xmake-options.md).

## Documents

- [Parsing And Paths](parsing-and-paths.md): argument preprocessing, duplicate handling, `--run`, strict parsing, file path classes, and Windows NT `::NT::` paths.
- [Global And Debug Commands](global-debug.md): `--help`, `--mode`, `--run`, `--version`, and `--debug-test`.
- [WebAssembly Commands](wasm.md): module loading, preload modules, native dynamic libraries, weak symbols, parser limits, initializer limits, memory limits, preload memory attributes, and strict memory growth.
- [Runtime Commands](runtime.md): runtime mode/compiler selection, shortcut conflicts, compile-thread resolution, scheduling, and compiler logs.
- [Logging Commands](logging.md): diagnostic output routing, warning enable/disable/fatal conversion, verbose mode, and Windows ANSI mode.
- [WASI Commands](wasi.md): core WASI option, WASI Preview 1 global/single/group targets, environment variables, mounts, trace output, sockets, and target override rules.

## Quick Availability Model

The command set is compiled conditionally. A binary may omit a command entirely when its feature is disabled. Important gates include:

- `_DEBUG` for `--debug-test`.
- `UWVM_SUPPORT_PRELOAD_DL` for native preload dynamic libraries.
- `UWVM_SUPPORT_WEAK_SYMBOL` for weak-symbol listing.
- `UWVM_SUPPORT_MMAP` for preload memory access mode `mmap`.
- `UWVM_RUNTIME_*` backend macros for interpreter, LLVM JIT, tiered, AOT-style full JIT, and debug interpreter commands.
- `UWVM_IMPORT_WASI` and `UWVM_IMPORT_WASI_WASIP1` for WASI commands.
- `UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET` for WASI socket preopen commands.
- Windows platform macros for `::NT::` path handling, Windows 9x TOCTOU warnings, and legacy Win32 color mode.

The binary itself is always authoritative:

```bash
uwvm --help all
uwvm --help global
uwvm --help debug
uwvm --help wasm
uwvm --help runtime
uwvm --help log
uwvm --help wasi
```

## Syntax Conventions

- `None` means no argument is consumed.
- `<name:type>` means one required argument is consumed.
- `(<name:type>)` means an optional argument may be consumed when the next token is still a plain argument.
- `[a|b|c]` means the slot accepts one of the listed literal forms.
- `[a|<int>]` means the slot accepts either a literal or a value parsed as the stated type.
- `...` means all later guest tokens belong to `--run` and are not parsed as host options.
- `path` values are host-side paths and can have platform-specific behavior.
- `str` values are raw command-line strings unless a command-specific rule validates them.
- `size_t`, `ssize_t`, and `i32` are parsed strictly: the whole token must parse and fit the target type.

## Reading Order

Read [Parsing And Paths](parsing-and-paths.md) first if you need exact behavior. Many surprising cases, especially `-1` as a value, optional arguments, `--run`, and `::NT::` paths, are parser-level behavior rather than individual command behavior.
