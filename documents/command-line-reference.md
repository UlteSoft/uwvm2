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
- `--wasip1-global-mount-dir`
- `--wasip1-global-add-environment`
- `--wasip1-single-create` for different module names
- `--wasip1-group-create` for different group names
- `--wasip1-group-add-module` for different module names
- socket preopen options

### 2. `--run` terminates host-side option parsing

`--run`, `-r`, and the shorthand `--` are special.

Once the parser sees `--run`, the next token is treated as the guest module path and **all remaining tokens are treated as guest arguments**. They are no longer parsed as `uwvm` options, even if they start with `-`.

Practical consequence:

- Put host-side options before `--run`.
- Do not expect `uwvm` to interpret flags placed after `--run`.

Example:

```bash
uwvm --runtime-jit --wasip1-global-set-argv0 myapp --run app.wasm --flag-for-guest
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
- `<name:type>` indicates a typed placeholder. The supplied token must satisfy that type on the current platform.
- `(<name:type>)` indicates an optional typed argument.
- `|` separates alternative accepted forms within the same usage slot.
- `[a|b|c]` indicates that the slot accepts one of the listed forms.
- `[a|<int>]` means the slot accepts either the literal `a` or any token that satisfies the current platform's `int` parsing rules.
- Whether a slot itself may be omitted is determined by the surrounding usage form, not by `[]` alone. For example, `<x>` is required, while `(<x>)` is optional.
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

### `--version`

`--version` is an immediate-exit inspection command.

Source behavior:

- it prints a formatted build report rather than a single semantic-version line,
- it includes build mode and, when compiled in, the detected install path,
- it reports compiler, allocator, platform, ISA, and runtime information,
- it enumerates compiled WebAssembly feature support,
- it returns immediately without entering validation or execution.

## Debug Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--debug-test` | None | None | Internal debug test entry point. | Debug builds only |

Notes:

- This option exists only when the project is built in debug mode.
- It is meant for internal developer workflows rather than end-user use.

### `--debug-test`

This is a developer-facing immediate command compiled only in debug builds.

Source behavior:

- it is a pure flag and does not consume additional arguments,
- it dispatches into an internal debug test path,
- it is not intended to be a stable end-user interface.

## WebAssembly Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--wasm-depend-recursion-limit` | `-Wdeplim` | `<depth:size_t>` | Set the recursion depth limit for dependency checking. `0` means unlimited. The source comment and help text indicate a default of `2048`. | Hosted builds |
| `--wasm-list-weak-symbol-module` | `-Wlsweak` | None | Print the list of registered weak-symbol modules. | Requires weak-symbol support |
| `--wasm-memory-grow-strict` | `-Wmemstrict` | None | Enable strict `memory.grow` semantics. In strict mode, allocation failure returns false when overcommit is disabled. With overcommit enabled, OOM still aborts the process. | Builds with memory-growth policy support |
| `--wasm-preload-library` | `-Wpre` | `<wasm:path> (<rename:str>)` | Preload a WebAssembly module as a dynamic library, optionally under a custom module name. | Hosted builds |
| `--wasm-register-dl` | `-Wdl` | `<dl:path> (<rename:str>)` | Load a native dynamic library and register its exports as a WebAssembly module, optionally under a custom module name. | Requires preload-DL support |
| `--wasm-set-initializer-limit` | `-Wilim` | `<type:str> <limit:size_t>` | Override a specific runtime initializer reserve limit. | Hosted builds |
| `--wasm-set-main-module-name` | `-Wname` | `<name:str>` | Override the main module name used by `uwvm`. | Hosted builds |
| `--wasm-set-memory-limit` | `-Wmemlim` | `<module:str> [<index:size_t>\|all] <min:size_t> (<max:size_t>)` | Override runtime page limits for selected local-defined Wasm memories. | Hosted builds |
| `--wasm-set-parser-limit` | `-Wplim` | `<type:str> <limit:size_t>` | Override a specific parser limit. | Hosted builds |
| `--wasm-set-preload-module-attribute` | `-Wpreattr` | `<module:str> <memory-access:none\|copy\|mmap> (<memory_index:list>)` | Set a preload module's memory access mode. Omitting `memory_index` applies the policy to all memories. | Builds with preload-module attribute support |

### `--wasm-depend-recursion-limit`

This option configures the recursion cap used by the dependency-resolution helper logic.

Source behavior:

- it parses exactly one `size_t` argument,
- it writes the result into `uwvm::utils::depend::recursion_depth_limit`,
- `0` means “unlimited”,
- malformed numeric input is rejected immediately.

### `--wasm-list-weak-symbol-module`

This is an immediate inspection command for weak-symbol integration.

Source behavior:

- it first triggers weak-symbol module loading through `run::load_weak_symbol_modules()`,
- on success, it prints each loaded weak-symbol module name,
- on failure, it terminates immediately with an error,
- it does not continue into normal execution after printing.

### `--wasm-preload-library`

This option loads a Wasm file during command-line processing and stores it in the preload-module set.

Source behavior:

- it expects a required Wasm file path and an optional rename token,
- each invocation appends a new entry to `wasm::storage::preloaded_wasm`,
- loading is performed immediately by `wasm::loader::load_wasm_file(...)`,
- the optional rename is forwarded to the loader as the module-name override,
- repeating the option is supported and accumulates multiple preloaded Wasm modules.

### `--wasm-register-dl`

This option is the native-DL counterpart of `--wasm-preload-library`.

Source behavior:

- it expects a required dynamic-library path and an optional rename token,
- each invocation appends a new entry to `wasm::storage::preloaded_dl`,
- loading is performed immediately by `wasm::loader::load_dl(...)`,
- the optional rename is forwarded to the loader as the module-name override,
- after a successful load, uwvm registers the preload-DL C-API functions for that entry,
- repeating the option is supported and accumulates multiple preloaded native modules.

### `--wasm-set-main-module-name`

Important source note:

- This changes the **module name** used by `uwvm`.
- It does **not** change WASI `argv[0]`.
- To override guest `argv[0]`, use `--wasip1-global-set-argv0`.

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

### `--wasm-set-memory-limit`

This option records runtime memory-limit overrides keyed by Wasm module name.

Source-enforced rules:

- `<module>` must not be empty,
- `<module>` must be a valid WebAssembly UTF-8 name,
- the selector slot accepts either `all` or a single `<index:size_t>`,
- `<min>` is required and must parse as `size_t`,
- `<max>` is optional, but when present it must parse as `size_t`,
- if `<max>` is present, it must be greater than or equal to `<min>`.

Behavior:

- the configuration is stored through `upsert_configured_module_memory_limit(module_name)`,
- `all` sets the module-wide override used for every local-defined memory in that module,
- a numeric selector updates only the chosen local-defined memory index,
- imported memories are not the target of this option,
- if `<max>` is omitted, the stored override records only the minimum and leaves the maximum in its default “not explicitly present” state,
- actual target resolution happens later during runtime initialization, when the loaded module graph is available.

### `--wasm-set-preload-module-attribute`

This option is more structured than the one-line `--help` output suggests.

Source-enforced rules:

- `<module>` must not be empty,
- `<memory-access>` must be one of `none`, `copy`, or `mmap`,
- the optional third value `<memory_index:list>` can be:
  - `all`, meaning all memories of that preload module,
  - or a comma-separated list of memory indices such as `0,1,2`.

Behavior:

- the configured attribute is stored in the preload-module attribute map,
- `apply_to_all_memories` is set by default, so omitting `<memory_index:list>` targets every memory,
- if a memory index list is provided, only the selected indices are targeted,
- after parsing, the attribute is immediately applied to already loaded modules via `apply_preload_module_memory_attribute_to_loaded_modules(...)`.

### `--wasm-memory-grow-strict`

This is a pure flag-style parameter.

Source behavior:

- it toggles the global `memory.grow` strictness flag,
- it does not take a value,
- it affects runtime allocation behavior rather than parser behavior.

## Runtime Commands

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--runtime-aot` | `-Raot` | None | Shortcut for full compile with the LLVM JIT backend. | Requires LLVM JIT |
| `--runtime-compile-threads` | `-Rct` | `[default\|aggressive\|count:ssize_t]` | Configure the runtime compile-thread count, or choose the adaptive `default` / `aggressive` policy upper bound. | Hosted runtime builds |
| `--runtime-scheduling-policy` | `-Rsp` | `[func_count <count:size_t>\|code_size <bytes:size_t>]` | Configure how full-compile groups local functions into translation tasks. Effective only when extra runtime compile threads are enabled. | Hosted runtime builds |
| `--runtime-compiler-log` | `-Rclog` | `<file:path>` | Write runtime compiler logs to a file. | Hosted runtime builds |
| `--runtime-custom-compiler` | `-Rcc` | `[int\|tiered\|jit\|debug-int]` | Select the runtime compiler explicitly. | Depends on compiled backends |
| `--runtime-custom-mode` | `-Rcm` | `[lazy\|lazy+verification\|full]` | Select the runtime mode explicitly. | Hosted runtime builds |
| `--runtime-debug-int` | `-RDint` | None | Shortcut for full compile with the debug interpreter backend. | Requires debug interpreter |
| `--runtime-int` | `-Rint` | None | Shortcut for lazy compile with the uwvm interpreter backend. | Requires interpreter backend |
| `--runtime-jit` | `-Rjit` | None | Shortcut for lazy compile with the LLVM JIT backend. | Requires LLVM JIT |
| `--runtime-tiered` | `-Rtiered` | None | Shortcut for lazy compile with the tiered interpreter/LLVM JIT backend. | Requires tiered backend |

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

When no runtime-selection parameter is provided, the current default is:

- mode = `full_compile`
- compiler = `llvm_jit_only` when LLVM JIT is compiled in; otherwise `uwvm_interpreter_only`

### Shortcut Runtime Options

These five parameters are pure flag-style selectors. Each one writes both runtime-selection axes at once: the runtime mode and the runtime compiler backend.

Shortcut runtime options are syntactic sugar over a concrete `(mode, compiler)` pair:

| Shortcut | Mode | Compiler |
| --- | --- | --- |
| `--runtime-int` | `lazy_compile` | `uwvm_interpreter_only` |
| `--runtime-jit` | `lazy_compile` | `llvm_jit_only` |
| `--runtime-tiered` | `lazy_compile` | `uwvm_interpreter_llvm_jit_tiered` |
| `--runtime-aot` | `full_compile` | `llvm_jit_only` |
| `--runtime-debug-int` | `full_compile` | `debug_interpreter` |

Operational notes:

- they do not consume extra arguments,
- they are intended for the common “pick a known runtime preset” workflow,
- they do not compose with each other,
- they also do not compose with `--runtime-custom-mode` or `--runtime-custom-compiler`.

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

This option is intentionally more permissive than the generic parser because it accepts signed integers, including negative values, and also supports named adaptive policies.

Source behavior:

- accepted forms are `default`, `aggressive`, or `ssize_t`,
- numeric values are stored in `runtime::runtime_mode::global_runtime_compile_threads`,
- named-policy values are stored in `runtime::runtime_mode::global_runtime_compile_threads_policy`,
- the corresponding existence flag is `runtime_compile_threads_existed`,
- the effective resolved value is stored later in `runtime::runtime_mode::global_runtime_compile_threads_resolved`,
- for `default` and `aggressive`, that resolved value is an adaptive upper bound rather than a guaranteed per-module thread count.

Runtime-time resolution behavior in `run.h`:

- if the option is omitted, uwvm selects the `default` adaptive policy upper bound using the `log2(N CPUs)` heuristic,
- if the value is `default`, uwvm explicitly uses that same conservative adaptive policy,
- if the value is `aggressive`, uwvm resolves an aggressive adaptive-policy upper bound to `floor(detected_max_threads * 2 / 3)`,
- if the value is non-negative, that exact value becomes the resolved compile-thread count,
- if the non-negative value is larger than the detected maximum hardware thread count, uwvm emits a dedicated `runtime-compile-threads` warning but still keeps the requested value,
- if the value is negative, uwvm interprets it as `detected_max_threads - abs(value)`,
- if the absolute value of a negative input is larger than the detected maximum, uwvm terminates with a fatal error,
- if `aggressive` is requested on a platform without `fast_io::native_thread`, uwvm emits a warning and falls back to `0` extra compile threads,
- during interpreter full-compile, `default` may reduce the per-module extra compile-thread count below that resolved upper bound when a small module would otherwise be overscheduled,
- during interpreter full-compile, `aggressive` is also adaptive, but uses a less conservative per-module reduction rule than `default`,
- numeric settings are not part of this adaptive-policy adjustment path,
- when verbose logging is enabled, uwvm prints the final resolved upper bound, and per-module verbose logs show the actual thread count used for full translation when adaptation is active.

### `--runtime-scheduling-policy`

This option controls how uwvm groups local functions into translation tasks during interpreter full-compile. It only has an effect when the resolved extra runtime compile thread count is greater than zero.

Source behavior:

- if the option is omitted, uwvm starts from `code_size 4096`,
- for small modules, the default policy may increase that `code_size` threshold automatically to reduce multithread scheduling overhead,
- accepted forms are:
  - `func_count <size_t>`: group by number of functions,
  - `code_size <size_t>`: group by cumulative wasm code-body size,
- the scheduling policy is stored in `runtime::runtime_mode::global_runtime_scheduling_policy`,
- the scheduling granularity is stored in `runtime::runtime_mode::global_runtime_scheduling_size`,
- the corresponding existence flag is `runtime_scheduling_policy_existed`.

Task-building behavior in `translate/single_func.h`:

- grouping always walks local functions in source order,
- `func_count <N>` emits a task whenever `N` functions have been accumulated,
- `code_size <N>` emits a task whenever the cumulative wasm code-body size reaches `N`,
- the measured code-body size is derived from each function's parsed wasm code section boundaries,
- the last incomplete group is emitted as the final task,
- if full translation runs on the main thread only, the configured scheduling policy is inactive.

Validation behavior:

- all numeric forms must be `size_t` and strictly greater than `0`,
- invalid modes or zero-sized thresholds are rejected at parse time.

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
| `--log-convert-warn-to-fatal` | `-log-wfatal` | `[all\|vm\|parser\|untrusted-dl\|dl\|weak-symbol\|depend\|nt-path\|toctou\|runtime\|runtime-compile-threads]` | Promote selected warnings to fatal errors. Some categories are platform- or feature-specific. | Hosted builds |
| `--log-disable-warning` | `-log-dw` | `[all\|vm\|untrusted-dl\|dl\|weak-symbol\|depend\|nt-path\|toctou\|runtime\|runtime-compile-threads]` | Disable selected warning classes. Some categories are platform- or feature-specific. | Hosted builds |
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
- `runtime-compile-threads`

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

### `--log-verbose`

This is a pure flag-style parameter that enables additional diagnostic output across multiple subsystems.

Source behavior:

- it toggles the global `uwvm::io::show_verbose` flag,
- loaders, runtime initialization, section-detail inspection, WASI environment setup, and socket preconfiguration all consult this flag,
- enabling it increases explanatory progress logging but does not change execution semantics.

### `--log-win32-use-ansi`

This is a platform-specific output-formatting flag.

Source behavior:

- it toggles `uwvm::utils::ansies::log_win32_use_ansi_b`,
- on supported Win32-oriented builds, diagnostics then prefer ANSI escape sequences for coloring,
- the option only exists in builds where that legacy console-color choice is relevant.

## WASI Commands

These options configure the hosted WASI integration, especially the built-in WASI Preview 1 environment.

| Option | Alias | Arguments | Description | Availability |
| --- | --- | --- | --- | --- |
| `--wasi-disable-utf8-check` | `-Iu8relax` | None | Disable WASI path/name UTF-8 checks for command-line and runtime behavior. | Requires WASI support |
| `--wasip1-global-add-environment` | `--wasip1-add-environment`, `-I1addenv` | `<env:str> <value:str>` | Add or replace a global-default environment variable in the Preview 1 environment. | Requires local imported WASI Preview 1 support |
| `--wasip1-global-delete-system-environment` | `--wasip1-delete-system-environment`, `-I1delsysenv` | `<env:str>` | Remove a host environment variable from the inherited global-default Preview 1 state. | Requires local imported WASI Preview 1 support |
| `--wasip1-global-disable` | `--wasip1-disable`, `-I1disable` | None | Disable the built-in Preview 1 module in the global default configuration. | Requires local imported WASI Preview 1 support |
| `--wasip1-global-expose-host-api` | `--wasip1-expose-host-api`, `-I1exportapi` | None | Expose the stable Preview 1 preload host API in the global default configuration. | Requires local imported WASI Preview 1 support |
| `--wasip1-global-mount-dir` | `--wasip1-mount-dir`, `-I1dir` | `<wasi dir:str> <system dir:path>` | Mount a host directory into the global-default Preview 1 sandbox. | Requires local imported WASI Preview 1 support |
| `--wasip1-global-noinherit-system-environment` | `--wasip1-noinherit-system-environment`, `-I1nosysenv` | None | Disable host environment inheritance in the global-default Preview 1 configuration. | Requires local imported WASI Preview 1 support |
| `--wasip1-global-set-argv0` | `--wasip1-set-argv0`, `-I1argv0` | `<argv0:str>` | Override guest `argv[0]` as the global default for Preview 1. | Requires local imported WASI Preview 1 support |
| `--wasip1-global-set-fd-limit` | `--wasip1-set-fd-limit`, `-I1fdlim` | `<limit:size_t>` | Set the global-default Preview 1 file descriptor limit. `0` means the representable maximum. | Requires local imported WASI Preview 1 support |
| `--wasip1-global-socket-tcp-connect` | `--wasip1-socket-tcp-connect`, `-I1tcpcon` | `<fd:i32> [<ipv4\|ipv6\|dns>:<port>\|unix <path>]` | Preconfigure a global-default Preview 1 TCP client socket. | Requires Preview 1 socket support |
| `--wasip1-global-socket-tcp-listen` | `--wasip1-socket-tcp-listen`, `-I1tcplisten` | `<fd:i32> [<ipv4\|ipv6>:<port>\|unix <path>]` | Preconfigure a global-default Preview 1 TCP listening socket. | Requires Preview 1 socket support |
| `--wasip1-global-socket-udp-bind` | `--wasip1-socket-udp-bind`, `-I1udpbind` | `<fd:i32> [<ipv4\|ipv6>:<port>\|unix <path>]` | Preconfigure a global-default Preview 1 UDP bound socket. | Requires Preview 1 socket support |
| `--wasip1-global-socket-udp-connect` | `--wasip1-socket-udp-connect`, `-I1udpcon` | `<fd:i32> [<ipv4\|ipv6\|dns>:<port>\|unix <path>]` | Preconfigure a global-default Preview 1 UDP client socket. | Requires Preview 1 socket support |
| `--wasip1-single-create` | None | `<module:str>` | Create one anonymous module-local Preview 1 group. Later `--wasip1-single-*` options for that module require this to have appeared first. | Requires local imported WASI Preview 1 support |
| `--wasip1-single-<action>` | None | `<module:str> (...)` | Configure an already-created single module. Actions include `enable`, `disable`, `expose-host-api`, `hide-host-api`, environment, mount, trace, fd-limit, argv0, and socket actions. | Requires local imported WASI Preview 1 support |
| `--wasip1-group-create` | None | `<group:str>` | Create one named shared Preview 1 group. The group name is not WebAssembly UTF-8 validated. | Requires local imported WASI Preview 1 support |
| `--wasip1-group-add-module` | None | `<group:str> <module:str>` | Bind one UTF-8-checked module name to an existing group. A module cannot also be a single module. | Requires local imported WASI Preview 1 support |
| `--wasip1-group-<action>` | None | `<group:str> (...)` | Configure an already-created group with the same action set used by `--wasip1-single-<action>`. | Requires local imported WASI Preview 1 support |

### `--wasi-disable-utf8-check`

Current source behavior:

- when WASI Preview 1 is present, this sets `default_wasip1_env.disable_utf8_check = true`,
- it is implemented as a flag-style option,
- during command-line processing, it affects guest-path validation in `--wasip1-global-mount-dir`,
- during Preview 1 runtime operation, it affects path/name UTF-8 validation in filesystem-style APIs,
- it does **not** relax Wasm parser UTF-8 validation or Wasm module-name validation.

### `--wasip1-global-add-environment`

This option records an explicit guest-side environment assignment in the global default Preview 1 environment.

Source behavior:

- it expects `<env> <value>`,
- `<env>` must not be empty,
- `<env>` must not contain `=`,
- the pair is stored in the additive environment override list,
- it still applies even when host environment inheritance is disabled.

### `--wasip1-global-delete-system-environment`

This option records a removal rule for inherited host environment variables in the global default Preview 1 environment.

Source behavior:

- it expects exactly one `<env>` token,
- `<env>` must not be empty,
- `<env>` must not contain `=`,
- the name is stored in the delete list used when building the inherited Preview 1 environment,
- it does not prevent a later explicit `--wasip1-global-add-environment` from providing the same name.

### `--wasip1-global-noinherit-system-environment`

This is a pure flag-style option that changes how the initial global-default Preview 1 environment is assembled.

Source behavior:

- it disables host environment inheritance for the built-in Preview 1 environment,
- explicit additions from `--wasip1-global-add-environment` still apply,
- delete rules become irrelevant when there is no inherited host environment to filter.

### Environment Inheritance and Overrides

The environment-related options are designed to work together.

Rules implemented in the source and parameter descriptions:

- `--wasip1-global-noinherit-system-environment`
  - disables inheritance of host environment variables,
  - causes `--wasip1-global-delete-system-environment` to become irrelevant,
  - does **not** prevent `--wasip1-global-add-environment` from applying.

- `--wasip1-global-add-environment`
  - expects `<env> <value>`,
  - rejects an empty `<env>`,
  - rejects `<env>` names containing `=`,
  - stores the pair in the additive environment override list.

- `--wasip1-global-delete-system-environment`
  - expects `<env>`,
  - rejects an empty `<env>`,
  - rejects `<env>` names containing `=`,
  - stores the name in the delete list.

### `--wasip1-global-disable`

This option is implemented by clearing `wasm::storage::local_preload_wasip1`.

In other words:

- it disables the automatic preload path for the local WASI Preview 1 module in the global default configuration,
- it does not take any additional arguments.

Module-specific `--wasip1-single-enable <name>` or `--wasip1-group-enable <group>` can still re-enable Preview 1 for selected modules after their single module or group has been created.

### `--wasip1-single-*`

Single-module configuration is explicit and left-to-right:

- first create the anonymous single-module group with `--wasip1-single-create <module:str>`,
- then configure it with `--wasip1-single-enable <module>`, `--wasip1-single-set-argv0 <module> <argv0>`, and the other `--wasip1-single-*` action parameters,
- a later action for a module that has not already been created is fatal, even if a create for the same name appears later,
- the module name is non-empty and WebAssembly UTF-8 validated.

The single action set is: `enable`, `disable`, `expose-host-api`, `hide-host-api`, `noinherit-system-environment`, `inherit-system-environment`, `disable-utf8-check`, `enable-utf8-check`, `trace`, `set-argv0`, `set-fd-limit`, `add-environment`, `delete-system-environment`, `mount-dir`, and the socket actions when socket support is enabled.

Example:

```bash
--wasip1-single-create app --wasip1-single-enable app --wasip1-single-set-argv0 app app-main
```

### `--wasip1-group-*`

Named group configuration is also explicit and left-to-right:

- create a raw named group with `--wasip1-group-create <group:str>`,
- configure it with `--wasip1-group-enable <group>`, `--wasip1-group-set-fd-limit <group> <limit>`, and the other `--wasip1-group-*` action parameters,
- bind modules with `--wasip1-group-add-module <group> <module>`,
- group names must be non-empty but are not WebAssembly UTF-8 validated,
- module names passed to `--wasip1-group-add-module` are non-empty and WebAssembly UTF-8 validated,
- a module can be configured as either a single module or a member of one named group, not both.

Example:

```bash
--wasip1-group-create shared --wasip1-group-expose-host-api shared --wasip1-group-add-module shared dl.alpha
```

### Target Action Behavior

| Action | Arguments after target | Effect |
| --- | --- | --- |
| `enable` | None | Re-enable built-in `wasi_snapshot_preview1` imports and expose the plugin-facing Preview 1 host API table. |
| `disable` | None | Disable built-in `wasi_snapshot_preview1` imports and hide the plugin-facing Preview 1 host API table. |
| `expose-host-api` | None | Expose the plugin-facing Preview 1 host API table. |
| `hide-host-api` | None | Hide the plugin-facing Preview 1 host API table. |
| `noinherit-system-environment` | None | Disable host environment inheritance for the selected target environment. |
| `inherit-system-environment` | None | Re-enable host environment inheritance for the selected target environment. |
| `disable-utf8-check` | None | Disable Preview 1 UTF-8 path/name checks for the selected target environment. |
| `enable-utf8-check` | None | Re-enable Preview 1 UTF-8 path/name checks for the selected target environment. |
| `trace` | `<none\|out\|err\|file <file:path>>` | Route WASI Preview 1 trace output for the selected target. |
| `set-argv0` | `<argv0:str>` | Override guest `argv[0]` for the selected target environment. |
| `set-fd-limit` | `<limit:size_t>` | Override the file-descriptor limit for the selected target environment. |
| `add-environment` | `<env:str> <value:str>` | Add or replace an environment variable for the selected target environment. |
| `delete-system-environment` | `<env:str>` | Remove one inherited host environment variable from the selected target environment. |
| `mount-dir` | `<wasi dir:str> <system dir:path>` | Add a target-specific mount on top of the global default mount set. |
| socket actions | socket-specific arguments | Add target-specific preopened sockets on top of the global default socket set. |

### `--wasip1-global-expose-host-api`

This option enables the stable Preview 1 preload host API for preload modules in the global default configuration.

Source behavior:

- it sets `wasm::storage::preload_expose_wasip1_host_api = true`,
- when preload-DL support is present, it refreshes that exposure for already loaded preload DL modules,
- when weak-symbol support is present, it also refreshes that exposure for already loaded weak-symbol modules,
- it is therefore both a forward-looking configuration flag and an immediate refresh trigger for existing preload-module state.

### `--wasip1-global-set-argv0`

This option stores a dedicated guest-facing `argv[0]` string in the global default Preview 1 environment.

Important distinction:

- `--wasm-set-main-module-name` changes the module name used by `uwvm`,
- `--wasip1-global-set-argv0` changes the guest process `argv[0]` in the global default Preview 1 environment.

These are different concepts.

Operational note:

- the override is stored in `wasip1_argv0_storage`,
- remaining guest arguments still come from the tokens placed after `--run`,
- `--wasip1-single-set-argv0` or `--wasip1-group-set-argv0` can replace that global default for selected targets.

### `--wasip1-global-set-fd-limit`

Source behavior:

- parses a `size_t`,
- rejects malformed values,
- rejects values larger than the maximum representable WASI file-descriptor type when necessary,
- treats `0` as “use the maximum representable fd value”,
- stores the resolved limit in `default_wasip1_env.fd_storage.fd_limit`,
- `--wasip1-single-set-fd-limit` or `--wasip1-group-set-fd-limit` can replace that global default for selected targets.

### `--wasip1-global-mount-dir`

This option has important source-level validation beyond the brief help text. It configures the global default mount set used by Preview 1.

Rules enforced by the callback:

- `<wasi dir>` must not be empty,
- a `<system dir>` must be provided and must be openable as a directory,
- unless `--wasi-disable-utf8-check` is active, the guest-visible `<wasi dir>` must also satisfy the current UTF-8 path rules,
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

Common source-level behavior:

- `<fd>` is parsed immediately as a WASI file descriptor,
- malformed or out-of-range fd values are rejected at parse time,
- duplicate-fd conflicts are intentionally deferred until later environment initialization,
- successful registrations may emit verbose logs when `--log-verbose` is enabled.

Supported high-level patterns:

| Option | Supported address forms |
| --- | --- |
| `--wasip1-global-socket-tcp-listen` | `ipv4:port`, `ipv6:port`, or `unix <path>` |
| `--wasip1-global-socket-tcp-connect` | `ipv4:port`, `ipv6:port`, hostname-like `dns:port`, or `unix <path>` |
| `--wasip1-global-socket-udp-bind` | `ipv4:port`, `ipv6:port`, or `unix <path>` |
| `--wasip1-global-socket-udp-connect` | `ipv4:port`, `ipv6:port`, hostname-like `dns:port`, or `unix <path>` |

Implementation detail for connect-style options:

- the parser first tries direct IP parsing,
- if IPv6 parsing fails in the relevant code path, it then attempts host-name resolution,
- the resolved address is stored as a preopen socket entry.

### `--wasip1-global-socket-tcp-listen`

This option preconfigures a global-default Preview 1 listening socket entry.

Source behavior:

- it expects an explicit `<fd:i32>` plus a listener address,
- it accepts `ipv4:port`, `ipv6:port`, and, when compiled in, `unix <path>`,
- it stores the resulting listener configuration in the default Preview 1 environment,
- `--wasip1-single-socket-tcp-listen` or `--wasip1-group-socket-tcp-listen` appends an additional entry for selected targets.

### `--wasip1-global-socket-tcp-connect`

This option preconfigures a global-default Preview 1 outbound TCP socket entry.

Source behavior:

- it expects an explicit `<fd:i32>` plus a remote address,
- it accepts `ipv4:port`, `ipv6:port`, hostname-like `dns:port`, and, when compiled in, `unix <path>`,
- it stores the resulting connect configuration in the default Preview 1 environment,
- `--wasip1-single-socket-tcp-connect` or `--wasip1-group-socket-tcp-connect` appends an additional entry for selected targets.

### `--wasip1-global-socket-udp-bind`

This option preconfigures a global-default Preview 1 UDP bind endpoint.

Source behavior:

- it expects an explicit `<fd:i32>` plus a bind address,
- it accepts `ipv4:port`, `ipv6:port`, and, when compiled in, `unix <path>`,
- it stores the resulting bind configuration in the default Preview 1 environment,
- `--wasip1-single-socket-udp-bind` or `--wasip1-group-socket-udp-bind` appends an additional entry for selected targets.

### `--wasip1-global-socket-udp-connect`

This option preconfigures a global-default Preview 1 outbound UDP socket entry.

Source behavior:

- it expects an explicit `<fd:i32>` plus a remote address,
- it accepts `ipv4:port`, `ipv6:port`, hostname-like `dns:port`, and, when compiled in, `unix <path>`,
- it stores the resulting connect configuration in the default Preview 1 environment,
- `--wasip1-single-socket-udp-connect` or `--wasip1-group-socket-udp-connect` appends an additional entry for selected targets.

## Practical Examples

### Show all currently compiled options

```bash
uwvm --help all
```

### Run a guest with host options before `--run`

```bash
uwvm \
  --runtime-tiered \
  --wasip1-global-set-argv0 myapp \
  --wasip1-global-add-environment MODE production \
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
  --wasip1-global-mount-dir /data ./data \
  --run app.wasm
```

### Re-enable Preview 1 only for the main wasm

```bash
uwvm \
  --wasip1-global-disable \
  --wasm-set-main-module-name app \
  --wasip1-single-create app \
  --wasip1-single-enable app \
  --run app.wasm
```

### Re-enable Preview 1 only for one preloaded wasm and give it its own `argv[0]`

```bash
uwvm \
  --wasip1-global-disable \
  --wasip1-global-set-argv0 global-entry \
  --wasip1-single-create lib.test \
  --wasip1-single-enable lib.test \
  --wasip1-single-set-argv0 lib.test pre \
  --wasm-preload-library ./lib.wasm lib.test \
  --run ./main.wasm hello world
```

### Expose Preview 1 host APIs only to one DL plugin

```bash
uwvm \
  --wasip1-single-create dl.example \
  --wasip1-single-expose-host-api dl.example \
  --wasm-register-dl ./libregdl.so dl.example \
  --wasm-set-preload-module-attribute dl.example copy all \
  --run ./main.wasm hello world
```

### Bind two modules to one shared Preview 1 group

```bash
uwvm \
  --wasip1-group-create shared-fd \
  --wasip1-group-expose-host-api shared-fd \
  --wasip1-group-add-module shared-fd dl.alpha \
  --wasip1-group-add-module shared-fd dl.beta \
  --wasm-register-dl ./libalpha.so dl.alpha \
  --wasm-register-dl ./libbeta.so dl.beta \
  --run ./main.wasm
```

### Preconfigure a TCP client socket

```bash
uwvm \
  --wasip1-global-socket-tcp-connect 3 example.com:443 \
  --run app.wasm
```

## Maintenance Notes

- This file should be updated whenever a parameter is added, removed, renamed, or changes behavior.
- The most important source directories are:
  - `src/uwvm2/uwvm/cmdline/params/**`
  - `src/uwvm2/uwvm/cmdline/callback/**`
- If there is any discrepancy between this document and a concrete binary, trust the binary's `--help` output for availability and trust the source for implementation details.
