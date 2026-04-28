# Global And Debug Commands

Source focus:

- `src/uwvm2/uwvm/cmdline/params/{help,mode,run,version,debug_test}.h`
- `src/uwvm2/uwvm/cmdline/callback/{help,mode,version,debug_test}.h`
- `src/uwvm2/uwvm/cmdline/parser.h`

## Command Table

| Command | Aliases | Arguments | Repeatability | Availability | Behavior |
| --- | --- | --- | --- | --- | --- |
| `--help` | `-h` | `([all|global|debug|wasm|runtime|log|wasi])` | Once | Core | Print category help and return success immediately. |
| `--mode` | `-m` | `[section-details|validation|run]` | Once | Core | Select the top-level operation mode. |
| `--run` | `-r`, `--` | `<file argv[0]:path> <argv[1]:str> ...` | One parser boundary | Core | Set the guest Wasm file and stop host option parsing. |
| `--version` | `-v`, `-ver` | None | Once | Core | Print the build and feature report, then return success immediately. |
| `--debug-test` | None | None | Once | `_DEBUG` only | Run an internal debug-only test hook. |

## `--help`

`--help` is an immediate inspection command. Without an argument, it prints global commands plus pointers to category-specific help.

Accepted extra help names:

- `all`: every compiled command in the current binary.
- `global`: global commands only.
- `debug`: debug commands only.
- `wasm`: WebAssembly commands only.
- `runtime`: runtime commands only.
- `log`: logging commands only.
- `wasi`: WASI commands only, when WASI support is compiled.

The extra help name is a normal argument and is marked occupied by the callback. Unknown names fail with an "Invalid Extra Help Name" error and show usage. Because the command returns immediately, do not place options after it expecting normal execution to continue.

Examples:

```bash
uwvm --help
uwvm --help all
uwvm -h runtime
```

## `--mode`

`--mode` selects `uwvm::wasm::storage::execute_wasm_mode`.

Accepted values:

- `run`: load, validate/initialize as required, and execute the selected entry.
- `validation`: validate the module graph without executing it.
- `section-details`: inspect and print section-level details instead of running the guest.

Notes:

- The default is `run`.
- The value is strict and case-sensitive.
- The option has an `is_exist` guard. Repeating it through any spelling is a duplicate-parameter error before callback behavior matters.
- The mode does not itself supply a module path. Normal runs still need `--run <file>`.

Examples:

```bash
uwvm --mode validation --run app.wasm
uwvm -m section-details -r app.wasm
```

## `--run`

`--run` is not implemented as a normal callback. The parser special-cases it during preprocessing.

Behavior:

- The next token is required and becomes the Wasm file path.
- All following tokens are guest arguments.
- No following token is parsed as a host option.
- `-r` and `--` behave exactly like `--run`.

The first token after `--run` is documented as `<file argv[0]:path>` because it both identifies the host Wasm file and becomes the natural guest argument root for the command line. WASI `argv[0]` can later be overridden with `--wasip1-*-set-argv0`, and the internal module name can be overridden with `--wasm-set-main-module-name`; those are separate concepts.

Examples:

```bash
uwvm --run app.wasm
uwvm -r app.wasm hello world
uwvm --runtime-tiered -- app.wasm --guest-option
```

Host option placement:

```bash
uwvm --runtime-jit --run app.wasm
```

This selects JIT.

```bash
uwvm --run app.wasm --runtime-jit
```

This passes `--runtime-jit` to the guest.

## `--version`

`--version` prints a formatted build report and exits successfully. It is not just a semantic version string.

The report can include:

- UWVM logo and version information.
- Git/build metadata when compiled in.
- Build mode and compiler information.
- Platform and ISA details.
- Allocator and install-path information where available.
- Runtime backend support, including LLVM version when JIT support is compiled.
- Compiled WebAssembly feature support.
- WASI-related support when compiled.

Because it is immediate, it is best used alone or after only output-routing options such as `--log-output`.

Example:

```bash
uwvm --version
uwvm --log-output file version.txt --version
```

## `--debug-test`

`--debug-test` exists only in `_DEBUG` builds. It is a developer hook, not a stable end-user command.

Behavior:

- No arguments are consumed.
- The command has an `is_exist` duplicate guard.
- In release builds it is not registered. The same token is then an invalid parameter.

Use it only when working on the VM itself.
