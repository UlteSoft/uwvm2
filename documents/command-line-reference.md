# UWVM2 Command-Line Reference

This document is a source-driven reference for the `uwvm` executable.

Unlike a short `--help` listing, this page also explains:

- how the parser actually consumes arguments,
- which options are mutually exclusive,
- which values are accepted by each structured argument,
- what state each option modifies,
- which options only exist under specific build configurations.

This file documents the runtime CLI of the `uwvm` binary. Build-time `xmake f ...` configuration switches are documented separately in [`xmake-options.md`](xmake-options.md).

## Scope

This reference is derived from the current source tree, primarily:

- `src/uwvm2/uwvm/cmdline/params/**`
- `src/uwvm2/uwvm/cmdline/callback/**`
- selected runtime / WASI storage headers that define the affected state

As a result, this document may describe options that are compiled conditionally and therefore may not appear in every binary.

## Availability Model

Not every option exists in every build. Availability is controlled by compile-time feature flags and platform support.

Typical conditions include:

- debug-only options,
- interpreter / LLVM-JIT / tiered / debug-interpreter backend availability,
- WASI Preview 1 integration,
- preload dynamic-library support,
- weak-symbol support,
- socket support,
- Windows-specific warning or console behavior.

The authoritative view for a specific binary is always:

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

## CLI Processing Model

The command-line parser is not a generic POSIX parser. Several source-level rules are important when using or documenting the CLI.

### 1. Parsing is left-to-right

Options are processed in order. For many singleton-style options, repeating the same option triggers a duplicate-parameter error because the corresponding parameter has an `is_exist` guard.

Examples of options that are naturally repeatable include:

- `--wasm-preload-library`
- `--wasm-register-dl`
- `--wasm-set-preload-module-attribute`
- `--wasip1-mount-dir`
- `--wasip1-add-environment`
- socket preopen options

### 2. `--run` terminates host-side option parsing

`--run`, `-r`, and the shorthand `--` are special.

Once the parser sees `--run`, the next token is treated as the guest module path and **all remaining tokens are treated as guest arguments**. They are no longer parsed as `uwvm` options, even if they start with `-`.

Practical consequence:

- Put host-side options before `--run`.
- Do not expect `uwvm` to interpret flags placed after `--run`.

Example:

```bash
uwvm --runtime-jit --wasip1-set-argv0 myapp --run app.wasm --flag-for-guest
```

Here `--flag-for-guest` is passed to the guest program, not to `uwvm`.

### 3. Structured values are parsed strictly

When an option expects a number or an enum-like token:

- the entire token must match,
- trailing garbage is rejected,
- missing required values are rejected,
- invalid enum names produce immediate parse-time errors.

### 4. Some options pre-mark following tokens

The parser supports a `pretreatment` hook for options whose next argument might look like an option.

`--runtime-compile-threads` uses this mechanism so values such as `-1` are accepted as an `int` argument instead of being misclassified as a new parameter.

### 5. `--help` and `--version` are immediate-exit style commands

These commands are intended for introspection. They print information and then return immediately without continuing into normal program execution.

## Syntax Conventions

- Long options use `--option-name`.
- Many options also have short project-specific aliases such as `-Rjit` or `-Wpre`.
- `<name:type>` indicates a required argument.
- `(<name:type>)` indicates an optional argument.
- `[a|b|c]` indicates a required value chosen from a fixed set.
- `None` means the option is a pure flag.

## Global Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--help` | `-h` | `([all\|global\|debug\|wasm\|log\|runtime\|wasi])` | Show help. Without an extra argument, it prints global commands and category entry points. With `all`, it prints all registered parameters in the current build. | Always available |
| `--mode` | `-m` | `[section-details\|validation\|run]` | Select the top-level operation mode. | Always available |
| `--run` | `-r`, `--` | `<file argv[0]:path> <argv[1]:str> <argv[2]:str> ...` | Run a WebAssembly module and stop further host-side option parsing. | Always available |
| `--version` | `-v`, `-ver` | None | Print version, compiler, ISA, feature, allocator, runtime, and platform information. | Always available |

### `--help`

Source behavior:

- `uwvm --help` prints the global category plus category pointers for `debug`, `wasm`, `runtime`, `log`, and `wasi` when compiled.
- `uwvm --help all` prints every registered parameter.
- `uwvm --help <category>` prints only that category.

### `--mode`

Accepted values:

- `section-details`
- `validation`
- `run`

Source mapping:

- `section-details` selects section inspection mode,
- `validation` selects validation-only mode,
- `run` selects normal execution mode.

Invalid mode names are rejected immediately.

### `--run`

This is the single most important parsing rule in the CLI:

- the token immediately after `--run` is the guest module path,
- all remaining tokens are guest arguments,
- none of the remaining tokens are parsed as `uwvm` parameters.

## Debug Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--debug-test` | None | None | Internal debug test entry point. | Debug builds only |

Notes:

- This option exists only when the project is built in debug mode.
- It is meant for internal developer workflows rather than end-user use.

## WebAssembly Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--wasm-depend-recursion-limit` | `-Wdeplim` | `<depth:size_t>` | Set the recursion depth limit for dependency checking. `0` means unlimited. The source comment and help text indicate a default of `2048`. | Hosted builds |
| `--wasm-expose-wasip1-host-api` | `-Wexi1api` | None | Expose the stable WASI Preview 1 host API to preload modules such as DL and weak-symbol modules. | Requires local WASI Preview 1 support |
| `--wasm-list-weak-symbol-module` | `-Wlsweak` | None | Print the list of registered weak-symbol modules. | Requires weak-symbol support |
| `--wasm-memory-grow-strict` | `-Wmemstrict` | None | Enable strict `memory.grow` semantics. In strict mode, allocation failure returns false when overcommit is disabled. With overcommit enabled, OOM still aborts the process. | Builds with memory-growth policy support |
| `--wasm-preload-library` | `-Wpre` | `<wasm:path> (<rename:str>)` | Preload a WebAssembly module as a dynamic library, optionally under a custom module name. | Hosted builds |
| `--wasm-register-dl` | `-Wdl` | `<dl:path> (<rename:str>)` | Load a native dynamic library and register its exports as a WebAssembly module, optionally under a custom module name. | Requires preload-DL support |
| `--wasm-set-initializer-limit` | `-Wilim` | `<type:str> <limit:size_t>` | Override a specific runtime initializer reserve limit. | Hosted builds |
| `--wasm-set-main-module-name` | `-Wname` | `<name:str>` | Override the main module name used by `uwvm`. | Hosted builds |
| `--wasm-set-parser-limit` | `-Wplim` | `<type:str> <limit:size_t>` | Override a specific parser limit. | Hosted builds |
| `--wasm-set-preload-module-attribute` | `-Wpreattr` | `<module:str> <memory-access:none\|copy\|mmap> (<memory-list:str>)` | Configure preload-module memory access behavior, optionally for selected memory indices only. | Builds with preload-module attribute support |

### `--wasm-set-main-module-name`

Important source note:

- This changes the **module name** used by `uwvm`.
- It does **not** change WASI `argv[0]`.
- To override guest `argv[0]`, use `--wasip1-set-argv0`.

The value is stored as-is. UTF-8 validation is intentionally left to later parser/runtime stages.

### `--wasm-set-parser-limit`

The parser accepts a symbolic limit name plus a `size_t` value.

Supported `type` values in the current source:

- `codesec_codes`
- `code_locals`
- `datasec_entries`
- `elemsec_funcidx`
- `elemsec_elems`
- `exportsec_exports`
- `globalsec_globals`
- `importsec_imports`
- `memorysec_memories`
- `tablesec_tables`
- `typesec_types`
- `custom_name_funcnames`
- `custom_name_codelocal_funcs`
- `custom_name_codelocal_name_per_funcs`

Behavior:

- invalid type names are rejected,
- invalid numeric values are rejected,
- each accepted key writes directly into the active parser-limit structure,
- the source already has per-key default constants and prints them in the runtime error message when an invalid key is supplied.

### `--wasm-set-initializer-limit`

The initializer limit callback accepts a symbolic limit name plus a `size_t` value.

Supported `type` values in the current source:

- `runtime_modules`
- `imported_functions`
- `imported_tables`
- `imported_memories`
- `imported_globals`
- `local_defined_functions`
- `local_defined_codes`
- `local_defined_tables`
- `local_defined_memories`
- `local_defined_globals`
- `local_defined_elements`
- `local_defined_datas`

Behavior:

- invalid type names are rejected,
- invalid numeric values are rejected,
- accepted values are stored directly into `runtime::initializer::initializer_limit`.

### `--wasm-set-preload-module-attribute`

This option is more structured than the one-line `--help` output suggests.

Source-enforced rules:

- `<module>` must not be empty,
- `<memory-access>` must be one of `none`, `copy`, or `mmap`,
- the optional third value can be:
  - `all`, meaning all memories of that preload module,
  - or a comma-separated list of memory indices such as `0,1,2`.

Behavior:

- the configured attribute is stored in the preload-module attribute map,
- `apply_to_all_memories` is set by default,
- if a memory index list is provided, only the selected indices are targeted,
- after parsing, the attribute is immediately applied to already loaded modules via `apply_preload_module_memory_attribute_to_loaded_modules(...)`.

### `--wasm-memory-grow-strict`

This is a pure flag-style parameter. It toggles the global memory-growth policy flag and does not take a value.

## Runtime Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--runtime-aot` | `-Raot` | None | Shortcut runtime: full compile plus LLVM-JIT-only backend. | Requires LLVM JIT |
| `--runtime-compile-threads` | `-Rct` | `<count:int>` | Parse and store a runtime compile-thread count. | Hosted runtime builds |
| `--runtime-compiler-log` | `-Rclog` | `<file:path>` | Write runtime compiler logs to a file. | Hosted runtime builds |
| `--runtime-custom-compiler` | `-Rcc` | `[int\|tiered\|jit\|debug-int]` | Select the runtime compiler explicitly. | Depends on compiled backends |
| `--runtime-custom-mode` | `-Rcm` | `[lazy\|lazy+verification\|full]` | Select the runtime mode explicitly. | Hosted runtime builds |
| `--runtime-debug` | `-Rdebug` | None | Shortcut runtime: full compile plus debug interpreter. | Requires debug interpreter |
| `--runtime-int` | `-Rint` | None | Shortcut runtime: lazy compile plus interpreter-only backend. | Requires interpreter backend |
| `--runtime-jit` | `-Rjit` | None | Shortcut runtime: lazy compile plus LLVM-JIT-only backend. | Requires LLVM JIT |
| `--runtime-tiered` | `-Rtiered` | None | Shortcut runtime: lazy compile plus tiered interpreter/JIT backend. | Requires tiered backend |

## Runtime Model and Conflict Rules

Runtime selection is implemented as two pieces of state:

- `runtime_mode_t`
- `runtime_compiler_t`

The source-defined runtime mode enum currently contains:

- `lazy_compile`
- `lazy_compile_with_full_code_verification`
- `full_compile`

The runtime compiler enum is compiled conditionally and may contain:

- `uwvm_interpreter_only`
- `debug_interpreter`
- `uwvm_interpreter_llvm_jit_tiered`
- `llvm_jit_only`

### Shortcut Mapping

Shortcut runtime options are syntactic sugar over a concrete `(mode, compiler)` pair:

| Shortcut | Mode | Compiler |
| --- | --- | --- |
| `--runtime-int` | `lazy_compile` | `uwvm_interpreter_only` |
| `--runtime-jit` | `lazy_compile` | `llvm_jit_only` |
| `--runtime-tiered` | `lazy_compile` | `uwvm_interpreter_llvm_jit_tiered` |
| `--runtime-aot` | `full_compile` | `llvm_jit_only` |
| `--runtime-debug` | `full_compile` | `debug_interpreter` |

### Conflict Rules

The source enforces the following:

- Only one shortcut runtime option may be used at a time.
- Any shortcut runtime option conflicts with `--runtime-custom-mode`.
- Any shortcut runtime option conflicts with `--runtime-custom-compiler`.
- `--runtime-custom-mode` and `--runtime-custom-compiler` are intended to be used together when you want explicit control over both axes.

### `--runtime-custom-mode`

Accepted values:

- `lazy`
- `lazy+verification`
- `full`

Source mapping:

- `lazy` -> `lazy_compile`
- `lazy+verification` -> `lazy_compile_with_full_code_verification`
- `full` -> `full_compile`

### `--runtime-custom-compiler`

Accepted values depend on which backends were compiled in.

Possible values in the source:

- `int`
- `tiered`
- `jit`
- `debug-int`

Source mapping:

- `int` -> `uwvm_interpreter_only`
- `tiered` -> `uwvm_interpreter_llvm_jit_tiered`
- `jit` -> `llvm_jit_only`
- `debug-int` -> `debug_interpreter`

### `--runtime-compile-threads`

This option is intentionally more permissive than the generic parser because it accepts signed integers, including negative values.

Source behavior:

- the argument is parsed as an `int`,
- negative values such as `-1` are accepted,
- the value is stored in `runtime::runtime_mode::global_runtime_compile_threads`,
- the corresponding existence flag is `runtime_compile_threads_existed`.

Current implementation note:

- in the current source, this option is a **parse-and-store** setting only,
- the actual scheduling / execution semantics for full-compile vs lazy-compile are not implemented in this command-line layer.

### `--runtime-compiler-log`

Behavior:

- expects a path argument,
- opens the runtime compiler log file,
- uses `follow` semantics when opening the path,
- emits parse-time errors if the path cannot be opened.

On supported Windows NT-style path configurations:

- a path beginning with `::NT::` is treated specially,
- enabling `nt-path` warnings may emit an additional warning,
- converting warnings to fatal may escalate this path warning.

## Logging Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--log-convert-warn-to-fatal` | `-log-wfatal` | `[all\|vm\|parser\|untrusted-dl\|dl\|weak-symbol\|depend\|nt-path\|toctou\|runtime]` | Promote selected warnings to fatal errors. Some categories are platform- or feature-specific. | Hosted builds |
| `--log-disable-warning` | `-log-dw` | `[all\|vm\|untrusted-dl\|dl\|weak-symbol\|depend\|nt-path\|toctou\|runtime]` | Disable selected warning classes. Some categories are platform- or feature-specific. | Hosted builds |
| `--log-enable-warning` | `-log-ew` | `[all\|parser]` | Re-enable parser warnings. In the current source, `all` is effectively equivalent to `parser`. | Hosted builds |
| `--log-output` | `-log` | `[out\|err\|file <file:path>]` | Select the primary log sink. | Hosted builds |
| `--log-verbose` | `-log-vb` | None | Enable verbose log output. | Hosted builds |
| `--log-win32-use-ansi` | `-log-ansi` | None | Use ANSI escape sequences for log coloring on legacy Win32 targets instead of the default console-color API path. | Legacy Win32 configurations only |

## Warning Classes

The warning system is more granular than the brief help text suggests.

Warning classes referenced by the source include:

- `vm`
- `parser`
- `untrusted-dl`
- `dl` when preload-DL support is compiled
- `weak-symbol` when weak-symbol support is compiled
- `depend`
- `nt-path` on Windows NT-path-aware configurations
- `toctou` on Windows 9x style configurations
- `runtime`

### `--log-enable-warning`

Current source behavior:

- `parser` enables parser warnings,
- `all` also only enables parser warnings.

This is not a typo in this document; it reflects the current implementation.

### `--log-disable-warning`

Current source behavior:

- `all` disables every currently compiled warning class,
- platform- and feature-specific categories are only present when the related support exists.

### `--log-convert-warn-to-fatal`

Current source behavior:

- `all` promotes every currently compiled warning class to fatal,
- feature/platform-specific categories again only exist when the related support exists.

### `--log-output`

Accepted values:

- `out`
- `err`
- `file <file:path>`

Behavior:

- `out` duplicates standard output,
- `err` duplicates standard error,
- `file <path>` opens a file sink and marks the extra argument as consumed,
- file opening follows symlinks,
- on supported Windows NT-path-aware builds, `::NT::...` paths receive special handling and may emit `nt-path` warnings.

## WASI Commands

These options configure the hosted WASI integration, especially the built-in WASI Preview 1 environment.

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--wasi-disable-utf8-check` | `-Iu8relax` | None | Disable UTF-8 validation for WASI-related command-line and runtime behavior. | Requires WASI support |
| `--wasip1-add-environment` | `-I1addenv` | `<env:str> <value:str>` | Add or replace an environment variable in the Preview 1 environment. | Requires local imported WASI Preview 1 support |
| `--wasip1-delete-system-environment` | `-I1delsysenv` | `<env:str>` | Remove a host environment variable from inherited Preview 1 state. | Requires local imported WASI Preview 1 support |
| `--wasip1-disable` | `-I1disable` | None | Disable preloading of the local Preview 1 module. | Requires local imported WASI Preview 1 support |
| `--wasip1-mount-dir` | `-I1dir` | `<wasi dir:str> <system dir:path>` | Mount a host directory into the Preview 1 sandbox. | Requires local imported WASI Preview 1 support |
| `--wasip1-noinherit-system-environment` | `-I1nosysenv` | None | Disable host environment inheritance for Preview 1. | Requires local imported WASI Preview 1 support |
| `--wasip1-set-argv0` | `-I1argv0` | `<argv0:str>` | Override guest `argv[0]` for Preview 1. | Requires local imported WASI Preview 1 support |
| `--wasip1-set-fd-limit` | `-I1fdlim` | `<limit:size_t>` | Set the Preview 1 file descriptor limit. | Requires local imported WASI Preview 1 support |
| `--wasip1-socket-tcp-connect` | `-I1tcpcon` | `<fd:i32> [<ipv4\|ipv6\|dns>:<port>\|unix <path>]` | Preconfigure a TCP client socket. | Requires Preview 1 socket support |
| `--wasip1-socket-tcp-listen` | `-I1tcplisten` | `<fd:i32> [<ipv4\|ipv6>:<port>\|unix <path>]` | Preconfigure a TCP listening socket. | Requires Preview 1 socket support |
| `--wasip1-socket-udp-bind` | `-I1udpbind` | `<fd:i32> [<ipv4\|ipv6>:<port>\|unix <path>]` | Preconfigure a UDP bind socket. | Requires Preview 1 socket support |
| `--wasip1-socket-udp-connect` | `-I1udpcon` | `<fd:i32> [<ipv4\|ipv6\|dns>:<port>\|unix <path>]` | Preconfigure a UDP client socket. | Requires Preview 1 socket support |

### `--wasi-disable-utf8-check`

Current source behavior:

- when WASI Preview 1 is present, this sets `default_wasip1_env.disable_utf8_check = true`,
- it is implemented as a flag-style option.

### Environment Inheritance and Overrides

The environment-related options are designed to work together.

Rules implemented in the source and parameter descriptions:

- `--wasip1-noinherit-system-environment`
  - disables inheritance of host environment variables,
  - causes `--wasip1-delete-system-environment` to become irrelevant,
  - does **not** prevent `--wasip1-add-environment` from applying.

- `--wasip1-add-environment`
  - expects `<env> <value>`,
  - rejects an empty `<env>`,
  - rejects `<env>` names containing `=`,
  - stores the pair in the additive environment override list.

- `--wasip1-delete-system-environment`
  - expects `<env>`,
  - rejects an empty `<env>`,
  - rejects `<env>` names containing `=`,
  - stores the name in the delete list.

### `--wasip1-disable`

This option is implemented by clearing `wasm::storage::local_preload_wasip1`.

In other words:

- it disables the automatic preload path for the local WASI Preview 1 module,
- it does not take any additional arguments.

### `--wasip1-set-argv0`

This option stores a dedicated guest-facing `argv[0]` string.

Important distinction:

- `--wasm-set-main-module-name` changes the module name used by `uwvm`,
- `--wasip1-set-argv0` changes the guest process `argv[0]`.

These are different concepts.

### `--wasip1-set-fd-limit`

Source behavior:

- parses a `size_t`,
- rejects malformed values,
- rejects values larger than the maximum representable WASI file-descriptor type when necessary,
- treats `0` as “use the maximum representable fd value”.

### `--wasip1-mount-dir`

This option has important source-level validation beyond the brief help text.

Rules enforced by the callback:

- `<wasi dir>` must not be empty,
- a `<system dir>` must be provided and must be openable as a directory,
- mount points are canonicalized for stable comparison,
- trailing `/` is removed except for root,
- relative and absolute WASI mount points are normalized differently,
- duplicate mount points are rejected,
- overlapping mount prefixes are rejected in both directions.

Examples of rejected conflicts:

- mounting both `/data` and `/data/cache`
- mounting both `mods` and `mods/core`

Additional platform notes from the source:

- some Windows configurations may emit `nt-path` warnings,
- Windows 9x-style configurations may emit a TOCTOU warning for directory handling,
- when verbose logging is enabled, successful mounts are logged.

### Socket Options

The socket options all begin with an explicit `<fd:i32>` argument and then parse an address form.

Supported high-level patterns:

| Option | Supported address forms |
| --- | --- |
| `--wasip1-socket-tcp-listen` | `ipv4:port`, `ipv6:port`, or `unix <path>` |
| `--wasip1-socket-tcp-connect` | `ipv4:port`, `ipv6:port`, hostname-like `dns:port`, or `unix <path>` |
| `--wasip1-socket-udp-bind` | `ipv4:port`, `ipv6:port`, or `unix <path>` |
| `--wasip1-socket-udp-connect` | `ipv4:port`, `ipv6:port`, hostname-like `dns:port`, or `unix <path>` |

Implementation detail for connect-style options:

- the parser first tries direct IP parsing,
- if IPv6 parsing fails in the relevant code path, it then attempts host-name resolution,
- the resolved address is stored as a preopen socket entry.

## Practical Examples

### Show all currently compiled options

```bash
uwvm --help all
```

### Run a guest with host options before `--run`

```bash
uwvm \
  --runtime-tiered \
  --wasip1-set-argv0 myapp \
  --wasip1-add-environment MODE production \
  --run app.wasm --guest-flag
```

### Validate without executing

```bash
uwvm --mode validation --run app.wasm
```

### Select runtime mode and compiler explicitly

```bash
uwvm \
  --runtime-custom-mode lazy+verification \
  --runtime-custom-compiler jit \
  --run app.wasm
```

### Store a signed compile-thread setting

```bash
uwvm --runtime-compile-threads -1 --run app.wasm
```

### Redirect main log output to a file

```bash
uwvm --log-output file uwvm.log --run app.wasm
```

### Record runtime compiler output separately

```bash
uwvm --runtime-compiler-log jit.log --runtime-jit --run app.wasm
```

### Mount a host directory into the WASI sandbox

```bash
uwvm \
  --wasip1-mount-dir /data ./data \
  --run app.wasm
```

### Preconfigure a TCP client socket

```bash
uwvm \
  --wasip1-socket-tcp-connect 3 example.com:443 \
  --run app.wasm
```

## Maintenance Notes

- This file should be updated whenever a parameter is added, removed, renamed, or changes behavior.
- The most important source directories are:
  - `src/uwvm2/uwvm/cmdline/params/**`
  - `src/uwvm2/uwvm/cmdline/callback/**`
- If there is any discrepancy between this document and a concrete binary, trust the binary's `--help` output for availability and trust the source for implementation details.
