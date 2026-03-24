# UWVM2 Command-Line Reference

This document describes the command-line interface of the `uwvm` executable.

It is intentionally separate from build-system documentation:

- Runtime CLI options for the `uwvm` binary are documented here.
- Build-time `xmake f ...` configuration switches are documented in [`xmake-options.md`](xmake-options.md).

## Scope

This reference covers the command-line parameters defined in the current source tree under `src/uwvm2/uwvm/cmdline/params/**`.

Some parameters are conditionally available:

- Some options exist only in debug builds.
- Some options depend on runtime backends enabled at build time.
- Some options depend on WASI, preload-DL, weak-symbol, socket, or platform support.

When an option is not available in your build, `uwvm --help` will not list it.

## Syntax Conventions

- Long options use the GNU-style form `--option-name`.
- Many options also provide a short project-specific alias such as `-Rjit` or `-Wpre`.
- Angle brackets such as `<file:path>` indicate required values.
- Parentheses such as `(<rename:str>)` indicate optional values.
- Bracketed alternatives such as `[run|validation]` indicate a required choice from a fixed set.
- `--run` also accepts the shorthand form `--`.

## Discoverability

Use the built-in help system to inspect the CLI from a concrete binary:

```bash
uwvm --help
uwvm --help global
uwvm --help debug
uwvm --help wasm
uwvm --help runtime
uwvm --help log
uwvm --help wasi
```

## Global Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--help` | `-h` | `([all\|global\|debug\|wasm\|log\|runtime\|wasi])` | Show general help or help for a specific category. | Always available. |
| `--mode` | `-m` | `[section-details\|validation\|run]` | Select the operation mode. The default is `run`. | Always available. |
| `--run` | `-r`, `--` | `<file argv[0]:path> <argv[1]:str> <argv[2]:str> ...` | Run a WebAssembly module and forward remaining arguments to the guest program. | Always available. |
| `--version` | `-v`, `-ver` | None | Print version, build, compiler, and feature information. | Always available. |

## Debug Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--debug-test` | None | None | Custom internal debug test entry point. | Debug builds only. |

## WebAssembly Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--wasm-depend-recursion-limit` | `-Wdeplim` | `<depth:size_t>` | Set the recursion-depth limit used by dependency checking. `0` means unlimited; the default is `2048`. | Available when the standard CLI front end is built. |
| `--wasm-expose-wasip1-host-api` | `-Wexi1api` | None | Expose the stable WASI Preview 1 host API to preload modules, including DL and weak-symbol modules. Disabled by default. | Requires local imported WASI Preview 1 support. |
| `--wasm-list-weak-symbol-module` | `-Wlsweak` | None | List all registered WASM weak-symbol modules. | Requires weak-symbol support. |
| `--wasm-memory-grow-strict` | `-Wmemstrict` | None | Enable strict `memory.grow` behavior. In strict mode, allocation failure returns false when overcommit is disabled; with overcommit enabled, OOM still aborts the process. | Available when memory-growth policy controls are built. |
| `--wasm-preload-library` | `-Wpre` | `<wasm:path> (<rename:str>)` | Preload a WebAssembly file as a dynamic library. A custom module name may be supplied. | Available in hosted builds. |
| `--wasm-register-dl` | `-Wdl` | `<dl:path> (<rename:str>)` | Load a native dynamic library and register its exported functions as a WebAssembly module. A custom module name may be supplied. | Requires preload dynamic-library support. |
| `--wasm-set-initializer-limit` | `-Wilim` | `<type:str> <limit:size_t>` | Set an initializer reserve limit for the WASM runtime. | Available in hosted builds. |
| `--wasm-set-main-module-name` | `-Wname` | `<name:str>` | Override the main module name. By default, the file-derived name is used. | Available in hosted builds. |
| `--wasm-set-parser-limit` | `-Wplim` | `<type:str> <limit:size_t>` | Set parser limits for selected WebAssembly parser subsystems. | Available in hosted builds. |
| `--wasm-set-preload-module-attribute` | `-Wpreattr` | `<module:str> <memory-access:none\|copy\|mmap> (<memory-list:str>)` | Configure memory-access attributes for a preload module, including preloaded dynamic libraries and weak-symbol modules. | Requires preload-module attribute support. |

## Runtime Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--runtime-aot` | `-Raot` | None | Select the AOT shortcut runtime: full compile plus LLVM-JIT-only backend. | Requires the LLVM JIT backend. |
| `--runtime-compile-threads` | `-Rct` | `<count:int>` | Set the runtime compile thread count. | Available in hosted runtime builds. |
| `--runtime-compiler-log` | `-Rclog` | `<file:path>` | Write runtime compiler logs to a file. | Available in hosted runtime builds. |
| `--runtime-custom-compiler` | `-Rcc` | `[int\|tiered\|jit\|debug-int]` | Select the runtime compiler explicitly instead of using a shortcut mode option. | Available when runtime compiler selection is supported. Individual choices depend on enabled backends. |
| `--runtime-custom-mode` | `-Rcm` | `[lazy\|lazy+verification\|full]` | Select the runtime mode explicitly. | Available in hosted runtime builds. |
| `--runtime-debug` | `-Rdebug` | None | Select the debug interpreter shortcut runtime: full compile plus debug interpreter. | Requires the debug interpreter backend. |
| `--runtime-int` | `-Rint` | None | Select the interpreter shortcut runtime: lazy compile plus interpreter-only backend. | Requires the UWVM interpreter backend. |
| `--runtime-jit` | `-Rjit` | None | Select the JIT shortcut runtime: lazy compile plus LLVM-JIT-only backend. | Requires the LLVM JIT backend. |
| `--runtime-tiered` | `-Rtiered` | None | Select the tiered shortcut runtime: lazy compile plus interpreter/LLVM-JIT tiered backend. | Requires the tiered backend. |

## Logging Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--log-convert-warn-to-fatal` | `-log-wfatal` | `[all\|vm\|parser\|untrusted-dl\|dl\|depend\|runtime]` | Promote selected warning classes to fatal errors. | Available in hosted builds. |
| `--log-disable-warning` | `-log-dw` | `[all\|vm\|untrusted-dl\|dl\|depend\|runtime]` | Disable selected warning classes. | Available in hosted builds. |
| `--log-enable-warning` | `-log-ew` | `[all\|parser]` | Re-enable selected warning classes. | Available in hosted builds. |
| `--log-output` | `-log` | `[out\|err\|file <file:path>]` | Select the main log destination. The default is standard error. | Available in hosted builds. |
| `--log-verbose` | `-log-vb` | None | Enable more verbose log output. | Available in hosted builds. |
| `--log-win32-use-ansi` | `-log-ansi` | None | Use ANSI escape sequences for log coloring on legacy Win32 targets instead of the default console-color API path. | Only available on supported Win32 configurations. |

## WASI Commands

The options below configure the hosted WASI integration path, especially the built-in WASI Preview 1 environment.

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--wasi-disable-utf8-check` | `-Iu8relax` | None | Disable UTF-8 validation for WASI-facing command-line processing and runtime behavior. | Requires WASI support. |
| `--wasip1-add-environment` | `-I1addenv` | `<env:str> <value:str>` | Add or replace a WASI Preview 1 environment variable. Still applies when host environment inheritance is disabled. | Requires local imported WASI Preview 1 support. |
| `--wasip1-delete-system-environment` | `-I1delsysenv` | `<env:str>` | Remove a host environment variable from the inherited WASI Preview 1 environment. | Requires local imported WASI Preview 1 support. |
| `--wasip1-disable` | `-I1disable` | None | Disable preloading of the local WASI Preview 1 module. | Requires local imported WASI Preview 1 support. |
| `--wasip1-mount-dir` | `-I1dir` | `<wasi dir:str> <system dir:path>` | Mount a host directory into the WASI Preview 1 sandbox at a fixed mount point. Absolute POSIX-style and relative mount points are supported under the documented path restrictions. | Requires local imported WASI Preview 1 support. |
| `--wasip1-noinherit-system-environment` | `-I1nosysenv` | None | Disable inheritance of host environment variables for WASI Preview 1. | Requires local imported WASI Preview 1 support. |
| `--wasip1-set-argv0` | `-I1argv0` | `<argv0:str>` | Override WASI Preview 1 `argv[0]` while preserving subsequent guest arguments from `--run`. | Requires local imported WASI Preview 1 support. |
| `--wasip1-set-fd-limit` | `-I1fdlim` | `<limit:size_t>` | Set the WASI Preview 1 file descriptor limit. `0` means the maximum representable `fd_t` value. | Requires local imported WASI Preview 1 support. |
| `--wasip1-socket-tcp-connect` | `-I1tcpcon` | `<fd:i32> [<ipv4\|ipv6\|dns>:<port>\|unix <path>]` | Configure a WASI Preview 1 TCP client connection from the command line. | Requires WASI Preview 1 socket support. |
| `--wasip1-socket-tcp-listen` | `-I1tcplisten` | `<fd:i32> [<ipv4\|ipv6>:<port>\|unix <path>]` | Configure a WASI Preview 1 TCP listening socket from the command line. | Requires WASI Preview 1 socket support. |
| `--wasip1-socket-udp-bind` | `-I1udpbind` | `<fd:i32> [<ipv4\|ipv6>:<port>\|unix <path>]` | Configure a WASI Preview 1 UDP bind operation from the command line. | Requires WASI Preview 1 socket support. |
| `--wasip1-socket-udp-connect` | `-I1udpcon` | `<fd:i32> [<ipv4\|ipv6\|dns>:<port>\|unix <path>]` | Configure a WASI Preview 1 UDP client connection from the command line. | Requires WASI Preview 1 socket support. |

## Practical Examples

### Run a WebAssembly program

```bash
uwvm --run hello.wasm
uwvm -- hello.wasm arg1 arg2
```

### Validate instead of running

```bash
uwvm --mode validation --run hello.wasm
```

### Use a specific runtime backend

```bash
uwvm --runtime-tiered --run app.wasm
uwvm --runtime-jit --runtime-compiler-log jit.log --run app.wasm
```

### Configure WASI arguments and sandbox paths

```bash
uwvm \
  --wasip1-set-argv0 my-app \
  --wasip1-mount-dir /data ./data \
  --wasip1-add-environment MODE production \
  --run app.wasm --config /data/config.toml
```

### Preload a supporting WebAssembly library

```bash
uwvm \
  --wasm-preload-library ./libmath.wasm math \
  --run app.wasm
```

## Notes for Maintainers

- This file documents the runtime CLI of `uwvm`, not the build configuration interface.
- If a parameter is added, renamed, removed, or made conditional, this document should be updated together with the corresponding files in `src/uwvm2/uwvm/cmdline/params/**`.
- The built-in `uwvm --help` output remains the authoritative source for a specific binary and build configuration.
