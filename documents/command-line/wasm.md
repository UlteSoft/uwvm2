# WebAssembly Commands

Source focus:

- `src/uwvm2/uwvm/cmdline/params/wasm_*.h`
- `src/uwvm2/uwvm/cmdline/callback/wasm_*.h`
- `src/uwvm2/uwvm/wasm/loader/wasm_file.h`
- `src/uwvm2/uwvm/wasm/loader/dl.h`
- `src/uwvm2/uwvm/wasm/storage/**`
- `src/uwvm2/uwvm/runtime/initializer/init_limit.h`

## Command Table

| Command | Aliases | Arguments | Repeatability | Availability | Behavior |
| --- | --- | --- | --- | --- | --- |
| `--wasm-set-main-module-name` | `-Wname` | `<name:str>` | Once | Core Wasm | Override the internal name used for the main module. |
| `--wasm-preload-library` | `-Wpre` | `<wasm:path> (<rename:str>)` | Repeatable | Core Wasm | Load a Wasm file immediately as a preload dynamic library. |
| `--wasm-register-dl` | `-Wdl` | `<dl:path> (<rename:str>)` | Repeatable | `UWVM_SUPPORT_PRELOAD_DL` | Load a native dynamic library and register its C API exports as a Wasm module. |
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
- `--wasm-set-memory-limit <module> ...`
- `--wasm-set-preload-module-attribute <module> ...`
- WASI single/group commands documented in [WASI Commands](wasi.md)

For memory-limit and preload-attribute configuration, the module name must be non-empty and valid according to the Wasm text-format name handling used by the source. Invalid UTF-8 is rejected during command-line processing.

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
