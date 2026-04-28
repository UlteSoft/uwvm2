# Logging Commands

Source focus:

- `src/uwvm2/uwvm/cmdline/params/log_*.h`
- `src/uwvm2/uwvm/cmdline/callback/log_*.h`
- `src/uwvm2/uwvm_predefine/io/warn_control.h`
- `src/uwvm2/uwvm_predefine/io/runtime_log.h`

## Command Table

| Command | Aliases | Arguments | Repeatability | Availability | Behavior |
| --- | --- | --- | --- | --- | --- |
| `--log-output` | `-log` | `[out|err|file <file:path>]` | Once | Core; file target gated on constrained platforms | Route main diagnostics to stdout, stderr, or file. |
| `--log-enable-warning` | `-log-ew` | `[all|parser]` | Repeatable | Core | Enable selected warning categories. |
| `--log-disable-warning` | `-log-dw` | `[all|vm|untrusted-dl|dl|weak-symbol|depend|nt-path|toctou|runtime|runtime-compile-threads]` | Repeatable | Core; some categories gated | Disable selected warning categories. |
| `--log-convert-warn-to-fatal` | `-log-wfatal` | `[all|vm|parser|untrusted-dl|dl|weak-symbol|depend|nt-path|toctou|runtime|runtime-compile-threads]` | Repeatable | Core; some categories gated | Convert selected warning categories to fatal termination. |
| `--log-verbose` | `-log-vb` | None | Once | Core | Enable verbose progress diagnostics. |
| `--log-win32-use-ansi` | `-log-ansi` | None | Once | Legacy Win32 color configurations | Use ANSI escapes for colored logs instead of Win32 console attributes. |

## Main Output Routing: `--log-output`

Syntax:

```bash
uwvm --log-output out
uwvm --log-output err
uwvm --log-output file uwvm.log
```

Behavior:

- `out` duplicates stdout into the main diagnostic output object.
- `err` duplicates stderr into the main diagnostic output object.
- `file <path>` opens a file with output and follow semantics where the target platform supports it.
- The option has an `is_exist` guard. Any second spelling is a duplicate-parameter error.
- The `file` form requires a second argument. A missing path is a usage error.
- A bare path is not accepted; use the literal `file` before the path.
- On constrained targets such as AVR, Windows 9x style builds, DOS/DJGPP, some newlib/picolibc configurations, and wasm targets, only `out`/`err` routing is compiled by this callback.

Windows NT path behavior:

- `file ::NT::<path>` enters the NT path branch on Windows NT builds.
- The `::NT::` prefix is stripped before opening.
- A `nt-path` warning is emitted unless suppressed.
- If `nt-path` is converted to fatal, the process terminates before opening.
- Open failures are reported as command-line errors.

Examples:

```bash
uwvm --log-output err --run app.wasm
uwvm --log-output file uwvm.log --version
```

## Warning Categories

Warning categories referenced by the current command-line source:

| Category | Meaning |
| --- | --- |
| `vm` | General VM warnings. |
| `parser` | Parser warnings. Enabled separately. |
| `untrusted-dl` | Warnings about untrusted native dynamic-library loading contexts. |
| `dl` | Native dynamic-library warnings when preload-DL support is compiled. |
| `weak-symbol` | Weak-symbol warnings when weak-symbol support is compiled. |
| `depend` | Dependency traversal/checking warnings. |
| `nt-path` | Windows NT `::NT::` path resolution warnings. |
| `toctou` | Windows 9x mount TOCTOU warning. |
| `runtime` | General runtime warnings. |
| `runtime-compile-threads` | Runtime compile-thread policy/count warnings. |

Gated categories are only accepted when their compile-time feature/platform branch is present. Otherwise the same category string is invalid for that binary.

## `--log-enable-warning`

Accepted values:

- `parser`
- `all`

Current source behavior:

- `parser` enables parser warnings.
- `all` also enables parser warnings.
- It does not enable every warning class; most non-parser warning classes default to enabled and are controlled by `--log-disable-warning`.

The option is parser-repeatable. Repeating it with `parser` or `all` is harmless because it sets the same bool to `true`.

Example:

```bash
uwvm --log-enable-warning parser --run app.wasm
```

## `--log-disable-warning`

Accepted values:

- `all`
- `vm`
- `untrusted-dl`
- `dl` when compiled
- `weak-symbol` when compiled
- `depend`
- `nt-path` on Windows NT builds
- `toctou` on Windows 9x style builds
- `runtime`
- `runtime-compile-threads`

Behavior:

- `all` disables every compiled non-parser warning class handled by this callback.
- Parser warnings are not disabled by `all` in this callback, because parser warnings are enabled through `--log-enable-warning` and fatalized through `--log-convert-warn-to-fatal`.
- Repeating the command is allowed and simply sets the relevant show flag to `false`.
- Disabling a warning does not unset its fatal-conversion flag if you set fatal conversion separately; avoid contradictory configurations if you need predictable operator-facing behavior.

Examples:

```bash
uwvm --log-disable-warning runtime-compile-threads --runtime-compile-threads 999 --run app.wasm
uwvm --log-disable-warning nt-path --log-output file ::NT::\??\C:\tmp\uwvm.log --run app.wasm
```

## `--log-convert-warn-to-fatal`

Accepted values:

- `all`
- `vm`
- `parser`
- `untrusted-dl`
- `dl` when compiled
- `weak-symbol` when compiled
- `depend`
- `nt-path` on Windows NT builds
- `toctou` on Windows 9x style builds
- `runtime`
- `runtime-compile-threads`

Behavior:

- `all` sets every compiled fatal-conversion flag handled by this callback, including `parser`.
- When a later code path emits a warning in a fatalized category, the code path prints a fatal conversion message and terminates.
- This option does not by itself emit a warning. It changes behavior of later warnings.
- Repeating the command is allowed.

Order matters when a warning-producing option appears on the same command line:

```bash
uwvm --log-convert-warn-to-fatal nt-path --log-output file ::NT::\??\C:\tmp\uwvm.log
```

The log-output NT warning is fatal.

```bash
uwvm --log-output file ::NT::\??\C:\tmp\uwvm.log --log-convert-warn-to-fatal nt-path
```

The log-output NT warning has already happened before fatal conversion is set.

## `--log-verbose`

`--log-verbose` sets `uwvm::io::show_verbose`.

Effects include additional progress messages for:

- Wasm file loading and parsing.
- Binary format detection.
- Runtime compile-thread resolution.
- WASI mounts.
- Runtime initialization steps that consult the verbose flag.
- Section-detail mode and other diagnostic flows.

It does not change execution semantics. It only increases diagnostic output.

The command has an `is_exist` guard through the underlying bool, so repeating it is a duplicate-parameter error once set.

## `--log-win32-use-ansi`

This command is registered only on legacy Win32 color configurations:

```c
defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
```

Behavior:

- Sets the flag that chooses ANSI escape sequences for colored logs.
- No argument is consumed.
- It has an `is_exist` guard through the underlying bool.

On modern Windows NT builds where the console setup path already uses virtual terminal processing, this command may not be registered.

## Interactions With Other Command Groups

Path-related warnings:

- `nt-path` can be emitted by Wasm loading, log-output files, runtime compiler log files, and WASI mounts on Windows NT.
- `toctou` can be emitted by WASI mounts on Windows 9x style builds.

Runtime warnings:

- `runtime-compile-threads` is emitted by runtime compile-thread resolution, not by the parse callback itself.
- A too-large positive explicit thread count warns but is kept.
- A negative thread count whose absolute value exceeds the detected max is fatal directly, not merely a warning.

Output routing:

- `--log-output` affects where later main diagnostics are written.
- `--runtime-compiler-log` has its own output object and is documented in [Runtime Commands](runtime.md).
- `--wasip1-*-trace` has its own output handling and is documented in [WASI Commands](wasi.md).
