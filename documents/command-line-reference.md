# UWVM2 Command-Line Reference

This page is the compact entry point for the source-driven `uwvm` command-line reference. The detailed reference now lives under [`command-line/`](command-line/) so each command group can be read independently without turning one document into an oversized manual.

Build-time `xmake f ...` switches are documented separately in [`xmake-options.md`](xmake-options.md).

## Short Index

Each entry here is intentionally short. Follow the file link for the full behavior, source notes, edge cases, and command combinations.

| Short introduction | Detailed reference |
| --- | --- |
| Overview of the split command-line manual, availability model, syntax conventions, and recommended reading order. | [README.md](command-line/README.md) |
| Parser lifecycle, `--run`, duplicate handling, negative values, host path classes, symlink-follow behavior, Windows NT `::NT::`, Win32 paths, UNC paths, WSL provider paths, and DOS path forms. | [parsing-and-paths.md](command-line/parsing-and-paths.md) |
| Global and debug commands: `--help`, `--mode`, `--run`, `--version`, and debug-only `--debug-test`. | [global-debug.md](command-line/global-debug.md) |
| WebAssembly commands: module loading, preloaded Wasm modules, native preload DLs, weak symbols, parser/initializer limits, memory limits, preload memory attributes, and strict memory growth. | [wasm.md](command-line/wasm.md) |
| Runtime commands: runtime mode/compiler selection, shortcut conflicts, compile-thread policies, scheduling, and compiler logs. | [runtime.md](command-line/runtime.md) |
| Logging commands: output routing, warning category enable/disable/fatal conversion, verbose mode, and Windows ANSI mode. | [logging.md](command-line/logging.md) |
| WASI commands: UTF-8 relaxation, WASI Preview 1 global/single/group targets, environment rules, mounts, trace output, sockets, target overrides, and initialization order. | [wasi.md](command-line/wasi.md) |

## Authoritative Source

The reference is derived from the current source tree, primarily:

- `src/uwvm2/uwvm/cmdline/params.h`
- `src/uwvm2/uwvm/cmdline/parser.h`
- `src/uwvm2/uwvm/cmdline/params/**`
- `src/uwvm2/uwvm/cmdline/callback/**`
- selected runtime, Wasm loader, logging, and WASI storage/initialization headers referenced by those callbacks

Some commands are conditionally compiled. The authoritative list for a particular binary is always the binary itself:

```bash
uwvm --help
uwvm --help all
uwvm --help global
uwvm --help debug
uwvm --help wasm
uwvm --help runtime
uwvm --help log
uwvm --help wasi
```

## Fast Orientation

- Put host-side options before `--run`, `-r`, or `--`; everything after that boundary is guest argv.
- Read [parsing-and-paths.md](command-line/parsing-and-paths.md) before debugging surprising argument behavior.
- On Windows NT builds, paths beginning with `::NT::` are special in specific file-opening callbacks; see [parsing-and-paths.md](command-line/parsing-and-paths.md#windows-nt-nt-path-prefix).
- WASI Preview 1 has three configuration layers: global defaults, single-module overrides, and named groups. See [wasi.md](command-line/wasi.md) for the full combination rules.
