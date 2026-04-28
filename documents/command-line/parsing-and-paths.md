# Parsing And Paths

Source focus:

- `src/uwvm2/uwvm/cmdline/parser.h`
- `src/uwvm2/uwvm/cmdline/params.h`
- `src/uwvm2/utils/cmdline/**`
- `src/uwvm2/uwvm/wasm/loader/wasm_file.h`
- `src/uwvm2/uwvm/wasm/loader/dl.h`
- `src/uwvm2/uwvm/cmdline/callback/log_output.h`
- `src/uwvm2/uwvm/cmdline/callback/runtime_compiler_log.h`
- `src/uwvm2/uwvm/cmdline/callback/wasip1_global_mount_dir.h`
- `src/uwvm2/uwvm/imported/wasi/wasip1/storage/env.h`

## Parser Lifecycle

The parser first builds a token table, then validates invalid/duplicate parameters, then invokes callbacks, then rejects any unconsumed host-side argument.

During preprocessing:

- `argv[0]` is stored as a `dir` token and saved as `argv0_dir`.
- Empty command-line entries are skipped.
- A token whose first byte is `-` is treated as a candidate parameter.
- Candidate parameters are resolved through the generated command hash table. The table includes primary names and aliases.
- Unknown parameters are recorded as invalid. Later the parser prints `invalid parameter` and may suggest the closest known parameter by edit distance.
- Known parameters with an `is_exist` guard are checked immediately. A second occurrence is recorded as a duplicate before callbacks run.
- Known parameters without an `is_exist` guard are parser-repeatable, although their callback may reject repeated or conflicting target state.
- Plain tokens before `--run` are recorded as `arg` and must be consumed by a previous option callback.

After callbacks run, any remaining `arg` token before `--run` is an `invalid option`. This is why optional arguments matter: a callback that decides not to consume a token leaves it available for the final unconsumed-argument check.

## `--run` Stops Host Parsing

`--run`, `-r`, and `--` are the same parameter. It is special-cased during preprocessing, not only during callback execution.

When the parser sees `--run`:

- It records the `--run` token as a parameter.
- The next token is required and becomes the Wasm file path.
- Every following token becomes an occupied guest argument.
- Preprocessing stops immediately.
- Later tokens are never interpreted as `uwvm` parameters, even if they begin with `-`.

Examples:

```bash
uwvm --runtime-jit --run app.wasm --guest-flag -x
```

`--guest-flag` and `-x` are guest arguments.

```bash
uwvm --run app.wasm --runtime-jit
```

`--runtime-jit` is not a runtime selector here; it is guest `argv[1]`.

`--run` with no following token is a usage error. A host option that appears after `--run` cannot fix that, because host parsing has already ended.

## Negative Values And Pretreatment

Most callbacks can only consume tokens that preprocessing classified as `arg`. Because tokens beginning with `-` are normally classified as parameters, a value like `-1` would usually be treated as an option.

`--runtime-compile-threads` is the exception. Its parameter definition has a `pretreatment` hook. If the next raw token starts with `-` followed by a digit, preprocessing pre-marks it as an occupied value, so the callback can parse it as a signed `ssize_t`.

Supported:

```bash
uwvm --runtime-compile-threads -1 --run app.wasm
```

Do not assume other numeric options accept negative-looking tokens in the same way. `size_t` options are unsigned anyway, and options without pretreatment cannot consume a `-...` token as data.

## Strict Value Parsing

The source uses strict scanners for numeric and enum-like values:

- The whole token must be consumed.
- Trailing bytes reject the value.
- Empty required values reject the value.
- A token such as `123x` is invalid for `size_t`, `ssize_t`, and `i32`.
- Optional numeric values are consumed only if they parse completely. If an optional numeric token is present but malformed, the callback may leave it unconsumed, and the final parser pass then reports it as an invalid host option.

## Duplicate And Conflict Model

There are two layers of repeatability:

- Parser-level duplicate checks use the parameter's `is_exist` pointer. These options fail as `duplicate parameter` on the second spelling, including aliases.
- Callback-level conflict checks inspect semantic state. Many WASI single/group commands do not use `is_exist`, because the same command can target different modules or groups. The callback rejects conflicts inside one target.

Examples:

- `--mode run --mode validation` is a parser-level duplicate error.
- `--wasip1-single-set-argv0 app a --wasip1-single-set-argv0 app b` is a callback-level conflict for target `app`.
- `--wasip1-single-set-argv0 app a --wasip1-single-set-argv0 other b` is structurally allowed because the targets differ.

## Immediate Commands

Some callbacks stop normal execution:

- `--help` prints help and returns success immediately.
- `--version` prints the build report and returns success immediately.
- `--debug-test` is debug-only and exits through the debug hook path.
- `--wasm-list-weak-symbol-module` loads weak-symbol modules, prints names, and exits.

When an immediate command appears with other options before it, callbacks are still processed left-to-right until that command returns an immediate result. Do not rely on later host options being applied after an immediate command.

## Host Path Categories

The command line has several host-side path categories:

| Category | Used by | Open mode |
| --- | --- | --- |
| Wasm input file | `--run`, `--wasm-preload-library` | `native_file_loader`, input, follows symlinks |
| Native dynamic library | `--wasm-register-dl` | `native_dll_file`, lazy dynamic loading; follows loader behavior |
| Log file | `--log-output file` | output file, follows symlinks |
| Runtime compiler log file | `--runtime-compiler-log file` | output file, follows symlinks |
| WASI trace file | `--wasip1-*-trace file` | output file, follows symlinks |
| WASI mounted system directory | `--wasip1-*-mount-dir` | directory handle, follows symlinks |
| Unix-domain socket path | WASI socket options with `unix <path>` | socket path passed to socket callback |

Path arguments are not normalized by the generic parser. Each command's callback decides how to open or validate the path.

## Windows NT `::NT::` Path Prefix

On Windows NT builds, defined by `_WIN32` and not `_WIN32_WINDOWS`, several callbacks recognize a path beginning with `::NT::`. This is a UWVM command-line escape hatch, not a Windows path prefix by itself. The callback strips the first six bytes and passes the remainder to fast_io's NT/kernel path branch.

Two NT path forms matter most:

| Form | Example after `::NT::` | Meaning |
| --- | --- | --- |
| NT mapped path | `\??\C:\path\file.wasm` | Uses the NT object-manager DOS-devices directory, usually `\??`, to resolve a DOS drive mapping such as `C:`. This is the NT-level form behind many Win32 drive paths. |
| NT absolute path | `\Device\HarddiskVolume3\path\file.wasm` | Names the NT object directly under `\Device`. This bypasses the DOS drive-letter mapping layer and depends on the actual device object names on that system. |

Full UWVM examples:

```text
::NT::\??\C:\path\file.wasm
::NT::\Device\HarddiskVolume3\path\file.wasm
```

The source behavior is:

- `path.starts_with(u8"::NT::")` selects the NT branch.
- `path.subview(6uz)` is the path handed to the NT/kernel opener.
- The remaining string is expected to be an NT object-manager path, such as `\??\C:\...` or `\Device\...`.
- A warning class `nt-path` is emitted by most NT-aware call sites before opening.
- `--log-disable-warning nt-path` suppresses that warning where the call site checks it.
- `--log-convert-warn-to-fatal nt-path` turns the warning into a fatal termination where the call site checks it.

NT-aware path support appears in these command paths:

- Main Wasm load: `--run`.
- Preloaded Wasm load: `--wasm-preload-library`.
- Main log output: `--log-output file`.
- Runtime compiler log output: `--runtime-compiler-log file`.
- WASI system directory mounts: `--wasip1-global-mount-dir`, and single/group mounts through the shared mount callback.
- WASI trace output files through `reopen_wasip1_trace_output_file`; this helper uses the NT branch but does not emit the same `nt-path` warning in the helper itself.

Important exceptions:

- Native dynamic library loading with `--wasm-register-dl` detects `::NT::`, emits the `nt-path` warning when enabled, and then terminates fatally because the source explicitly does not support loading DLLs from NT paths.
- On non-Windows-NT builds, `::NT::...` is not special and is treated as an ordinary path string by the relevant platform opener.
- A normal Win32 path such as `C:\path\file.wasm` does not enter this branch. It is passed to the ordinary platform opener.

## Windows Host Path Forms

UWVM does not have one generic path normalizer. Before `--run`, the command-line parser only classifies tokens. Each callback later passes the path to the relevant fast_io file, directory, DLL, log, trace, or socket-opening API. On Windows, that means the same textual path can be interpreted by different Windows namespace layers depending on whether it enters the ordinary Win32 opener or the UWVM `::NT::` NT branch.

Use this section as a path-shape guide, not as a promise that every path shape is valid for every command. For example, `--wasm-register-dl` uses the native dynamic loader path and explicitly rejects `::NT::`.

### NT Object Paths With `::NT::`

Use `::NT::` only when you want the UWVM callback to bypass the ordinary Win32 path conversion and pass an NT object path to the NT/kernel opener.

NT mapped path:

```text
::NT::\??\C:\tmp\app.wasm
::NT::\??\D:\work\data
```

Notes:

- `\??` is the NT DOS-devices lookup directory used for drive-letter and DOS-device mappings.
- `C:` and `D:` here are resolved as NT object-manager symbolic links.
- This is still an NT path after UWVM strips `::NT::`; it is not parsed by the Win32 path layer.

NT absolute path:

```text
::NT::\Device\HarddiskVolume3\tmp\app.wasm
::NT::\Device\Mup\server\share\dir\file.wasm
```

Notes:

- `\Device\HarddiskVolume3` names a device object directly. The volume number is machine-specific.
- `\Device\Mup\server\share\...` is the NT object-manager path commonly involved in UNC provider resolution.
- These paths are powerful but less portable because object names depend on the running Windows installation and mounted devices.

### Win32 Drive Paths

Ordinary Win32 paths do not need `::NT::`.

```text
C:\tmp\app.wasm
D:\work\data
C:/tmp/app.wasm
```

Notes:

- These are passed to the ordinary platform opener.
- Windows normally converts them through the Win32/DOS path layer before reaching NT.
- Forward slashes can be accepted by many Win32 file APIs, but backslashes are the canonical Windows form.
- Drive letters are per-system mappings. `C:` is not an NT device name; it is a DOS-device mapping that ultimately points somewhere in the NT object namespace.

### UNC And Win32 Device Paths

UNC and Win32 device-style paths are also ordinary Windows path strings unless you explicitly prefix them with `::NT::`.

Classic UNC share:

```text
\\server\share\dir\file.wasm
```

Extended-length DOS path:

```text
\\?\C:\very\long\path\file.wasm
```

Extended-length UNC path:

```text
\\?\UNC\server\share\dir\file.wasm
```

Win32 device namespace path:

```text
\\.\C:\tmp\file.wasm
\\.\PhysicalDrive0
\\.\COM1
```

Notes:

- `\\server\share\...` is the normal UNC form for network shares.
- `\\?\...` asks the Win32 layer to use extended path handling and reduce some legacy normalization rules.
- `\\?\UNC\server\share\...` is the extended form of a UNC path.
- `\\.\...` enters the Win32 device namespace, often used for devices such as `COM1`, volumes, or physical drives.
- These are not the same as UWVM `::NT::\Device\...` paths. They still go through the Win32 path API selected by the ordinary opener.
- Whether a specific command can open a device path depends on that command's open mode. A command expecting a regular input file or directory can still fail on a device object.

### WSL Provider Paths

From a Windows process, WSL file systems are usually exposed as UNC provider paths:

```text
\\wsl$\Ubuntu\home\user\project\app.wasm
\\wsl.localhost\Ubuntu\home\user\project\app.wasm
```

Notes:

- These are Win32 UNC provider paths, not Linux paths.
- They should be treated like UNC paths by the ordinary Windows opener.
- UWVM does not translate `/mnt/c/...` to Windows or translate `\\wsl$...` to Linux. It passes the string to the platform opener used by the callback.
- If UWVM itself is running as a Linux binary inside WSL, then `/mnt/c/...` and `/home/user/...` are POSIX paths from that Linux process, not Win32 paths.

### DOS Path Forms

DOS path syntax has several forms that look similar but resolve differently.

Plain relative path:

```text
app.wasm
dir\app.wasm
..\app.wasm
```

Meaning:

- Relative to the process current directory.
- No drive root is specified.

Drive-absolute path:

```text
D:\apps\app.wasm
```

Meaning:

- Absolute path on drive `D:`.
- Does not depend on the current directory, except that the drive mapping itself must exist.

Current-drive rooted path:

```text
\apps\app.wasm
```

Meaning:

- Absolute from the root of the current drive.
- If the process current drive is `C:`, this means `C:\apps\app.wasm`.
- If the current drive is `D:`, this means `D:\apps\app.wasm`.

Drive-relative path:

```text
D:apps\app.wasm
D:..\app.wasm
```

Meaning:

- Relative to the current directory associated with drive `D:`.
- This is the surprising DOS form: `D:apps\app.wasm` is not the same as `D:\apps\app.wasm`.
- The per-drive current directory is a process/environment concept. Avoid this form in scripts if reproducibility matters.

Practical recommendation:

- Use ordinary absolute Win32 paths such as `C:\path\file.wasm` for normal Windows use.
- Use UNC forms such as `\\server\share\file.wasm` for network shares.
- Use `\\?\...` only when extended Win32 path handling is needed.
- Use `::NT::\??\...` or `::NT::\Device\...` only when you intentionally need UWVM's NT opener branch and accept the `nt-path` warning behavior.

## Symlink And Follow Behavior

The command-line file and directory open paths generally use follow semantics:

- Wasm files use `open_mode::in | open_mode::follow`.
- Log and trace files use `open_mode::out | open_mode::follow`.
- WASI mounted directories use directory open plus follow semantics.
- Native dynamic libraries follow the platform dynamic loader path behavior and are documented as following symlinks in the loader comments.

This means a host-side path argument is resolved at command-line processing or load time with symlink-following behavior unless the underlying platform or loader imposes a stricter rule.

## Windows 9x TOCTOU Warning For Mounts

On `_WIN32_WINDOWS` builds, `--wasip1-*-mount-dir` can emit a `toctou` warning. The source warns because Windows 9x cannot provide the same directory-handle safety used by POSIX/Windows NT style paths, and the mount can be vulnerable to time-of-check/time-of-use races.

Controls:

- `--log-disable-warning toctou` suppresses this warning.
- `--log-convert-warn-to-fatal toctou` turns it into a fatal termination.

## Practical Parser Rules

- Put every host option before `--run`.
- Put optional arguments immediately after the option that consumes them.
- Use `--runtime-compile-threads -1` only for that specific signed option; do not generalize negative values to `size_t` options.
- When passing a guest argument that looks like a host option, place it after `--run`.
- If an option accepts a file target as `file <path>`, the literal `file` is part of the host option syntax and must appear before the path unless that option explicitly documents a bare-path convenience.
