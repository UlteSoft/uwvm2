# WebAssembly Commands

Source focus:

- `src/uwvm2/uwvm/cmdline/params/wasm_*.h`
- `src/uwvm2/uwvm/cmdline/callback/wasm_*.h`
- `src/uwvm2/uwvm/wasm/loader/wasm_file.h`
- `src/uwvm2/uwvm/wasm/loader/load_and_check_modules.h`
- `src/uwvm2/uwvm/wasm/loader/dl.h`
- `src/uwvm2/uwvm/wasm/storage/**`
- `src/uwvm2/uwvm/run/run.h`
- `src/uwvm2/uwvm/runtime/initializer/init.h`
- `src/uwvm2/uwvm/runtime/initializer/init_limit.h`

## Command Table

| Command | Aliases | Arguments | Repeatability | Availability | Behavior |
| --- | --- | --- | --- | --- | --- |
| `--wasm-set-main-module-name` | `-Wname` | `<name:str>` | Once | Core Wasm | Override the internal name used for the main module. |
| `--wasm-set-start-func` | `-Wstart` | `<local-func:u32-dec> [[i32|i64|f32|f64] ...]` | Once | Core Wasm | Run a selected main-module local-defined function as the runtime entry. |
| `--wasm-preload-library` | `-Wpre` | `<wasm:path> (<rename:str>)` | Repeatable | Core Wasm | Load a Wasm file immediately as a preload dynamic library. |
| `--wasm-register-dl` | `-Wdl` | `<dl:path> (<rename:str>)` | Repeatable | `UWVM_SUPPORT_PRELOAD_DL` | Load a native dynamic library and register its C API exports as a Wasm module. |
| `--wasm-reset-import` | `-Wresimp` | `<module:str> <import-module:str> <import-extern:str> <new-import-module:str> <new-import-extern:str>` | Repeatable | Core Wasm | Rewrite a Wasm file module's import binding during runtime initialization. |
| `--wasm-set-preload-module-attribute` | `-Wpreattr` | `<module:str> <memory-access:none|copy|mmap> (<memory_index:list>)` | Repeatable | Core Wasm; `mmap` needs `UWVM_SUPPORT_MMAP` | Configure memory access behavior for preloaded modules. |
| `--wasm-depend-recursion-limit` | `-Wdeplim` | `<depth:size_t>` | Once | Core Wasm | Set dependency-check recursion depth. `0` means unlimited. |
| `--wasm-set-memory-limit` | `-Wmemlim` | `<module:str> [<index:size_t>|all] <min:size_t> (<max:size_t>)` | Repeatable | Core Wasm | Override runtime page limits for selected local-defined memories. |
| `--wasm-set-parser-limit` | `-Wplim` | `<type:str> <limit:size_t>` | Repeatable | Core Wasm | Override one parser or custom-name parser limit. |
| `--wasm-set-initializer-limit` | `-Wilim` | `<type:str> <limit:size_t>` | Repeatable | Core Wasm | Override one runtime initializer reserve/check limit. |
| `--wasm-list-weak-symbol-module` | `-Wlsweak` | None | Once | `UWVM_SUPPORT_WEAK_SYMBOL` | Load and print registered weak-symbol modules, then exit. |
| `--wasm-memory-grow-strict` | `-Wmemstrict` | None | Once | Core Wasm | Enable strict runtime memory-growth behavior. |

## Module Names

Several commands key configuration by WebAssembly module name:

- `--wasm-set-main-module-name`
- `--wasm-preload-library <wasm> <rename>`
- `--wasm-register-dl <dl> <rename>`
- `--wasm-reset-import <module> ...`
- `--wasm-set-memory-limit <module> ...`
- `--wasm-set-preload-module-attribute <module> ...`
- WASI single/group commands documented in [WASI Commands](wasi.md)

For memory-limit and preload-attribute configuration, the module name must be non-empty and valid according to the Wasm text-format name handling used by the source. Invalid UTF-8 is rejected during command-line processing.

For import-reset configuration, all five name arguments are validated as Wasm UTF-8 names during command-line processing.

`--wasm-set-main-module-name` changes the VM's internal main-module name. It does not change WASI `argv[0]`. Use `--wasip1-global-set-argv0`, `--wasip1-single-set-argv0`, or `--wasip1-group-set-argv0` for guest-visible `argv[0]`.

## Wasm File Loading

The main module selected by `--run` and every `--wasm-preload-library` module use the shared Wasm loader.

Loader behavior:

- The host file name is stored on the module record.
- The file is opened through `native_file_loader` with input and follow semantics.
- On Windows NT, `::NT::` paths use the NT/kernel opener after stripping the prefix.
- A `nt-path` warning is emitted by the Wasm loader for `::NT::` paths when enabled.
- The loader detects the binary format version.
- Currently supported binary format version is Wasm binary format version 1.
- The whole file is advised as `willneed` after detection.
- On platforms where `CHAR_BIT > 8`, the loader masks file bytes down to the low 8 bits before parsing.
- Parser errors are reported through the parser diagnostic path and cause the load command to fail.

Verbose mode (`--log-verbose`) adds progress messages for loading, format detection, and parsing.

## `--wasm-set-start-func`

Syntax:

```bash
uwvm --wasm-set-start-func <local-func:u32-dec> [arg ...] --run app.wasm
uwvm -Wstart <local-func:u32-dec> [arg ...] -r app.wasm
```

Behavior:

- Selects a local-defined function in the main module and runs it as the runtime entry function.
- `<local-func:u32-dec>` is a zero-based local-defined function index, not an import-inclusive Wasm function index.
- The local-function index is parsed only as decimal `u32`. Prefix forms such as `0x0`, `0b0`, and `0o0` are rejected for the function index.
- The option is guarded by `is_exist`; a second occurrence is a duplicate-parameter error.
- The selected function may have parameters and results only from the currently supported scalar set: `i32`, `i64`, `f32`, and `f64`.
- The number of supplied argument tokens must exactly match the selected function's parameter count.
- Argument parsing happens after the main module is loaded and initialized enough for runtime entry resolution, so arity, type, and range errors are runtime-entry fatal diagnostics.
- With `--log-verbose`, the resolved local index, import-inclusive Wasm function index, import count, function type, input tokens, and parsed values are printed before invocation. Integer verbose lines use `bin`, `oct`, `sdec`, `udec`, and `hex`; floating-point `bitfloat(hex)` is printed with the full bit width, so f32 uses 8 hex digits and f64 uses 16 hex digits after `0x`.

Argument token collection:

- After the decimal local-function index, preprocessing marks following tokens as `--wasm-set-start-func` arguments until the next host-parameter-looking token.
- A host-parameter-looking token is one whose first byte is `-` and whose next byte is not a C digit.
- This keeps negative decimal values such as `-1`, `-2`, `-3.0e4`, and `-4.5` attached to the command.
- It stops before `--log-verbose`, `--run`, `-r`, `--`, and ordinary unknown options such as `-foo`.
- Tokens beginning with `+` are not treated specially. They are collected as argument tokens and then accepted or rejected by the runtime-entry parser.
- Malformed positive-looking tokens are still collected. For example, `0xfffffffgff` becomes a function argument and is reported as an invalid `i32` argument if the selected parameter is `i32`, not as an unconsumed host option.

Integer arguments:

| Parameter type | Accepted forms | Notes |
| --- | --- | --- |
| `i32` | signed decimal; unsigned decimal; `0x`/`0X` hexadecimal; `0b`/`0B` binary; `0o`/`0O` octal | Unsigned forms are bit-cast to `i32`, so `4294967295` and `0xffffffff` mean `-1`. Prefixed forms do not accept a leading sign. |
| `i64` | signed decimal; unsigned decimal; `0x`/`0X` hexadecimal; `0b`/`0B` binary; `0o`/`0O` octal | Unsigned forms are bit-cast to `i64`, so `18446744073709551615` and `0xffffffffffffffff` mean `-1`. Prefixed forms do not accept a leading sign. |

Floating-point arguments:

| Parameter type | Accepted forms | Notes |
| --- | --- | --- |
| `f32` | ordinary decimal/general float; optional trailing `f`/`F`; C-style hexadecimal float beginning with `0x`/`0X` and using a `p`/`P` exponent | Examples: `3.5`, `-3.0e4`, `3.5f`, `0x1.8p0`. Binary-prefixed float forms such as `0b1.001p1` are rejected. Leading `+` is rejected. |
| `f64` | ordinary decimal/general float; optional trailing `f`/`F`; C-style hexadecimal float beginning with `0x`/`0X` and using a `p`/`P` exponent | Examples: `4.5`, `-4.5`, `0X1.2P1`. Binary-prefixed float forms such as `0b1.001p1` are rejected. Leading `+` is rejected. |

Examples:

```bash
uwvm --wasm-set-start-func 0 --run app.wasm
uwvm --wasm-set-start-func 3 -1 0xffffffffffffffff 3.5 0x1.2p1 --log-verbose --run app.wasm
uwvm -Wstart 353 0xfffffffgff -r app.wasm
```

In the last example, `0xfffffffgff` is collected as an entry-function argument. If local function `353` expects `i32`, the runtime-entry parser reports it as an invalid `i32` argument.

## `--wasm-preload-library`

Syntax:

```bash
uwvm --wasm-preload-library <wasm:path> [rename]
```

Behavior:

- Requires one Wasm path.
- Optionally consumes the next plain argument as a rename.
- Loads the module immediately during command-line processing.
- Appends the loaded module to `preloaded_wasm`.
- Repeating the option accumulates preloaded modules in command-line order.

Optional rename rules:

- If present, the rename is used as the module name override for that preloaded module.
- If omitted, the loader derives or preserves the module name according to the Wasm loader/custom-name path.
- Because the rename is optional and consumed when the next token is a plain argument, place the next host option immediately after the path if no rename is intended.

Examples:

```bash
uwvm --wasm-preload-library lib.wasm --run app.wasm
uwvm --wasm-preload-library lib.wasm lib.math --run app.wasm
```

## `--wasm-register-dl`

Syntax:

```bash
uwvm --wasm-register-dl <dl:path> [rename]
```

Behavior:

- Requires `UWVM_SUPPORT_PRELOAD_DL`.
- Requires one native dynamic-library path.
- Optionally consumes the next plain argument as a module rename.
- Loads the library immediately through `native_dll_file`.
- On success, appends the library to `preloaded_dl`.
- Registers preload-DL C API functions for the loaded entry.
- Repeating the option accumulates native preload modules.

Path notes:

- Dynamic libraries are loaded lazily by the platform loader path.
- On Windows NT, `::NT::` is detected and the `nt-path` warning may be emitted, but the loader then terminates fatally because loading DLLs from NT paths is explicitly unsupported.

Security notes:

- Native dynamic libraries execute host code. Treat them as trusted host extensions, not sandboxed guest code.
- `untrusted-dl` and `dl` warning classes are controlled by the logging commands when compiled.

## `--wasm-list-weak-symbol-module`

This is an immediate inspection command available with `UWVM_SUPPORT_WEAK_SYMBOL`.

Behavior:

- Loads weak-symbol modules.
- Prints registered module names.
- Exits instead of continuing to normal execution.
- Load failures are fatal for this command path.

Use it to inspect weak-symbol registration in the current binary/runtime environment.

## `--wasm-reset-import`

Syntax:

```bash
uwvm --wasm-reset-import <module> <import-module> <import-extern> <new-import-module> <new-import-extern>
```

Behavior:

- Requires exactly five name arguments.
- All five arguments must be valid Wasm UTF-8 names.
- `<module>` selects the Wasm file module instance whose imports should be rebound.
- `<import-module>` and `<import-extern>` select an import in that module by its original Wasm import module/name pair.
- `<new-import-module>` and `<new-import-extern>` are the effective import module/name pair used by runtime initialization and dependency checks.
- The parser-owned Wasm import section is not mutated.
- The runtime initializer creates runtime-owned import descriptors for matching imports and points runtime import records at those descriptors.
- Export names are not rewritten. This option does not rename Wasm exports, preload-DL exports, or weak-symbol plugin exports.
- To expose a new export-shaped interface, add an explicit bridge/adaptor module instead of renaming an existing export.
- The target `<module>` must resolve to a runtime-supported Wasm file module.
- Each configured rule must match at least one import in the selected module; otherwise runtime initialization terminates fatally.
- With `--log-verbose`, each applied binding rewrite is logged during runtime initialization.

Repeatability and replacement:

- The option is repeatable.
- Repeating the same `<module> <import-module> <import-extern>` replaces that rule's destination pair.
- Different source import pairs on the same module accumulate as independent rules.

Examples:

```bash
uwvm \
  --wasm-preload-library host.wasm host.v2 \
  --wasm-set-main-module-name app \
  --wasm-reset-import app env clock_ms host.v2 clock_ms \
  --run app.wasm
```

The `app` module still has its original parsed import in the parser result. Runtime initialization binds the import that was written as `env.clock_ms` to `host.v2.clock_ms`.

## `--wasm-depend-recursion-limit`

Syntax:

```bash
uwvm --wasm-depend-recursion-limit <depth:size_t>
```

Behavior:

- Parses exactly one `size_t`.
- Writes `uwvm::utils::depend::recursion_depth_limit`.
- `0` means unlimited.
- The command has an `is_exist` guard; a second occurrence is a duplicate-parameter error.

This limit applies to dependency traversal/checking, not to Wasm call-stack depth or runtime recursion.

## `--wasm-set-memory-limit`

Syntax:

```bash
uwvm --wasm-set-memory-limit <module> all <min> [max]
uwvm --wasm-set-memory-limit <module> <index> <min> [max]
```

Behavior:

- `<module>` is required, non-empty, and must be a valid Wasm UTF-8 name.
- The selector is either `all` or one local-defined memory index parsed as `size_t`.
- `<min>` is required and parsed as `size_t`.
- `[max]` is optional and parsed as `size_t` only if the next token parses completely.
- If `[max]` is present, `max >= min` is required.
- The parsed limit is stored in the configured module memory-limit table.
- `all` sets a module-wide override for all local-defined memories.
- A numeric selector sets or replaces the override for that local-defined memory index.

Combination rules:

- The command is repeatable.
- Repeating `all` for the same module replaces the module-wide all-memory limit.
- Repeating a numeric index for the same module replaces that index's configured limit.
- `all` and numeric index overrides can both be configured; resolution happens later when the module graph and local memories are known.
- These overrides target local-defined memories, not imported memories.

Examples:

```bash
uwvm --wasm-set-memory-limit app all 1 256 --run app.wasm
uwvm --wasm-set-memory-limit app 0 4 --wasm-set-memory-limit app 1 16 64 --run app.wasm
```

## `--wasm-set-preload-module-attribute`

Syntax:

```bash
uwvm --wasm-set-preload-module-attribute <module> none [all|0,1,2]
uwvm --wasm-set-preload-module-attribute <module> copy [all|0,1,2]
uwvm --wasm-set-preload-module-attribute <module> mmap [all|0,1,2]
```

Behavior:

- `<module>` is required, non-empty, and must be a valid Wasm UTF-8 name.
- Memory access mode accepts `none` and `copy`.
- `mmap` is accepted only when `UWVM_SUPPORT_MMAP` is compiled.
- If the memory list is omitted, the setting applies to all memories.
- `all` explicitly applies to all memories.
- A comma-separated list applies to selected memory indexes.
- Empty list items are rejected.
- Trailing commas are rejected.
- Each list item must parse completely as `size_t`.
- Duplicates in the list collapse naturally because storage is an unordered set.
- The callback immediately applies the configured attribute to already loaded modules with the same name.

Access-mode meaning:

- `none`: do not expose a preloaded memory through a special access mechanism.
- `copy`: copy memory content/metadata according to the preload memory implementation.
- `mmap`: use memory-mapped preload memory support where available.

Order matters when modules are already loaded:

```bash
uwvm --wasm-preload-library lib.wasm lib --wasm-set-preload-module-attribute lib copy
```

The attribute is applied to the already loaded `lib`.

```bash
uwvm --wasm-set-preload-module-attribute lib copy --wasm-preload-library lib.wasm lib
```

The attribute is stored first and picked up when `lib` is loaded.

## `--wasm-set-parser-limit`

Syntax:

```bash
uwvm --wasm-set-parser-limit <type> <limit:size_t>
```

Behavior:

- The option is repeatable.
- `<type>` selects one parser/custom-name parser limit.
- `<limit>` must parse completely as `size_t`.
- Unknown type names are rejected and the binary prints the available names and defaults.
- Repeating the same type overwrites that limit for later Wasm parsing.

Accepted type names:

| Type | Affected limit |
| --- | --- |
| `codesec_codes` | Maximum code-section function bodies. |
| `code_locals` | Maximum local declarations per code body. |
| `datasec_entries` | Maximum data-section entries. |
| `elemsec_funcidx` | Maximum function indexes in element-section parsing. |
| `elemsec_elems` | Maximum element entries. |
| `exportsec_exports` | Maximum exports. |
| `globalsec_globals` | Maximum globals. |
| `importsec_imports` | Maximum imports. |
| `memorysec_memories` | Maximum memories. |
| `tablesec_tables` | Maximum tables. |
| `typesec_types` | Maximum type-section entries. |
| `custom_name_funcnames` | Maximum names in the custom name function subsection. |
| `custom_name_codelocal_funcs` | Maximum functions in the custom name code-local subsection. |
| `custom_name_codelocal_name_per_funcs` | Maximum local names per function in the custom name code-local subsection. |

Place parser-limit overrides before any command that loads a Wasm module if you want them to affect that module. `--wasm-preload-library` loads immediately, so limits after it do not affect that already loaded preloaded module.

## `--wasm-set-initializer-limit`

Syntax:

```bash
uwvm --wasm-set-initializer-limit <type> <limit:size_t>
```

Behavior:

- The option is repeatable.
- `<type>` selects one runtime initializer limit field.
- `<limit>` must parse completely as `size_t`.
- Unknown type names are rejected and the binary prints available names and defaults.
- Repeating the same type overwrites that initializer limit.

Accepted type names:

| Type | Affected runtime initializer limit |
| --- | --- |
| `runtime_modules` | Maximum runtime modules. |
| `imported_functions` | Maximum imported functions. |
| `imported_tables` | Maximum imported tables. |
| `imported_memories` | Maximum imported memories. |
| `imported_globals` | Maximum imported globals. |
| `local_defined_functions` | Maximum local-defined functions. |
| `local_defined_codes` | Maximum local-defined code entries. |
| `local_defined_tables` | Maximum local-defined tables. |
| `local_defined_memories` | Maximum local-defined memories. |
| `local_defined_globals` | Maximum local-defined globals. |
| `local_defined_elements` | Maximum local-defined elements. |
| `local_defined_datas` | Maximum local-defined data entries. |

Initializer limits are used when the parsed module graph is transformed into runtime storage. They are distinct from parser limits.

## `--wasm-memory-grow-strict`

This flag sets the global strict memory-grow flag.

Behavior:

- No arguments are consumed.
- The flag has an `is_exist` guard through the underlying global bool, so repeating it is a duplicate-parameter error once the bool is set.
- It affects runtime memory growth semantics, especially how failed growth is reported where the runtime can handle it.
- It does not make host OOM behavior fully recoverable on every platform.

## Ordering Examples

Parser limit before preload:

```bash
uwvm --wasm-set-parser-limit codesec_codes 100000 --wasm-preload-library lib.wasm lib --run app.wasm
```

Parser limit after preload affects later loads only:

```bash
uwvm --wasm-preload-library lib.wasm lib --wasm-set-parser-limit codesec_codes 100000 --run app.wasm
```

Preload with module-specific WASI configuration:

```bash
uwvm \
  --wasip1-single-create lib \
  --wasip1-single-enable lib \
  --wasm-preload-library lib.wasm lib \
  --run app.wasm
```

NT path on Windows NT:

```bash
uwvm --wasm-preload-library ::NT::\??\C:\mods\lib.wasm lib --run app.wasm
```

Native DLL NT-path rejection on Windows NT:

```bash
uwvm --wasm-register-dl ::NT::\??\C:\mods\lib.dll lib
```

This enters the NT-path warning path and then fatally rejects DLL loading from NT paths.
