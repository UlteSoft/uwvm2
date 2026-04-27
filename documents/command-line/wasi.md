# WASI Commands

Source focus:

- `src/uwvm2/uwvm/cmdline/params/wasi_disable_utf8_check.h`
- `src/uwvm2/uwvm/cmdline/params/wasip1_*.h`
- `src/uwvm2/uwvm/cmdline/callback/wasi_disable_utf8_check.h`
- `src/uwvm2/uwvm/cmdline/callback/wasip1_*.h`
- `src/uwvm2/uwvm/imported/wasi/wasip1/storage/env.h`
- `src/uwvm2/uwvm/imported/wasi/wasip1/init/init_env.h`
- `src/uwvm2/uwvm/run/loader.h`
- `src/uwvm2/imported/wasi/wasip1/environment/environment.h`

## Availability

The WASI command category is compiled only when WASI support is enabled.

- `--wasi-disable-utf8-check` requires `UWVM_IMPORT_WASI`.
- WASI Preview 1 commands require local WASI Preview 1 support, `UWVM_IMPORT_WASI_WASIP1`, and no `UWVM_DISABLE_LOCAL_IMPORTED_WASIP1`.
- Socket preopen commands require `UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET`.
- Unix-domain socket syntax additionally depends on `UWVM_SUPPORT_UNIX_PATH_SOCKET` and runtime platform support for local sockets.

If a command is not compiled into the binary, the parser treats its name as an invalid parameter.

## Target Model

WASI Preview 1 configuration has three layers:

| Layer | Purpose | How to configure |
| --- | --- | --- |
| Global default | Default environment and default import visibility for modules that do not have an override. | `--wasip1-global-*` commands and their short aliases. |
| Single target | One anonymous per-module override group, keyed by a Wasm module name. | `--wasip1-single-create <module>`, then `--wasip1-single-* <module> ...`. |
| Named group | One named shared override group that can be bound to multiple modules. | `--wasip1-group-create <group>`, `--wasip1-group-add-module <group> <module>`, then `--wasip1-group-* <group> ...`. |

The module names used by single/group bindings are WebAssembly module names, not host file names. They are affected by main/preload module naming commands such as `--wasm-set-main-module-name`, the optional rename of `--wasm-preload-library`, and the optional rename of `--wasm-register-dl`.

Loaded modules are checked under these target kinds:

- `main_wasm`
- `preload_wasm`
- `preloaded_dl`
- `weak_symbol`

One module name cannot be configured as both a single target and a member of a named group. The command-line callbacks prevent most conflicts immediately, and `run/loader.h` validates loaded module bindings again before initializing WASI environments.

## Core WASI Command

| Command | Alias | Arguments | Repeatability | Behavior |
| --- | --- | --- | --- | --- |
| `--wasi-disable-utf8-check` | `-Iu8relax` | None | Once | Disable WASI path/name UTF-8 checks for command-line and runtime WASI handling. |

`--wasi-disable-utf8-check` sets the global WASI disable flag and also sets the default WASI Preview 1 environment's `disable_utf8_check` flag. It does not relax WebAssembly binary parsing or module-name validation. Module names used by single/group commands must still be valid Wasm UTF-8 names.

For WASI mount paths, this flag changes command-line validation from "valid UTF-8 and no embedded zero" to "no embedded zero". Target-specific `--wasip1-*-enable-utf8-check` can set the target runtime environment back to checked mode, but it does not retroactively revalidate command-line mount paths that were parsed while the global relaxed flag was active.

## Global Command Table

| Command | Aliases | Arguments | Repeatability | Behavior |
| --- | --- | --- | --- | --- |
| `--wasip1-global-trace` | `--wasip1-trace`, `-I1trace` | `[none|out|err|file <file:path>]` | Once | Route global-default WASI call trace output. |
| `--wasip1-global-expose-host-api` | `--wasip1-expose-host-api`, `-I1exportapi` | None | Once | Make the stable WASI Preview 1 preload host API visible globally by default. |
| `--wasip1-global-disable` | `--wasip1-disable`, `-I1disable` | None | Once | Disable the global-default built-in WASI Preview 1 module unless a target override re-enables it. |
| `--wasip1-global-set-fd-limit` | `--wasip1-set-fd-limit`, `-I1fdlim` | `<limit:size_t>` | Once | Set the default WASI fd limit. `0` maps to the maximum WASI fd value. |
| `--wasip1-global-mount-dir` | `--wasip1-mount-dir`, `-I1dir` | `<wasi dir:str> <system dir:path>` | Repeatable | Mount a host directory into the default WASI preopen set. |
| `--wasip1-global-set-argv0` | `--wasip1-set-argv0`, `-I1argv0` | `<argv0:str>` | Once | Override global-default WASI `argv[0]`. |
| `--wasip1-global-noinherit-system-environment` | `--wasip1-noinherit-system-environment`, `-I1nosysenv` | None | Once | Start the global-default WASI environment without inheriting host environment variables. |
| `--wasip1-global-delete-system-environment` | `--wasip1-delete-system-environment`, `-I1delsysenv` | `<env:str>` | Repeatable | Remove one inherited host environment variable by name. |
| `--wasip1-global-add-environment` | `--wasip1-add-environment`, `-I1addenv` | `<env:str> <value:str>` | Repeatable | Add or replace one WASI environment variable. |
| `--wasip1-global-socket-tcp-listen` | `--wasip1-socket-tcp-listen`, `-I1tcplisten` | `<fd:i32> [<ipv4|ipv6>:<port>|unix <path>]` | Repeatable | Create a default TCP listening socket. |
| `--wasip1-global-socket-tcp-connect` | `--wasip1-socket-tcp-connect`, `-I1tcpcon` | `<fd:i32> [<ipv4|ipv6|dns>:<port>|unix <path>]` | Repeatable | Create a default connected TCP client socket. |
| `--wasip1-global-socket-udp-bind` | `--wasip1-socket-udp-bind`, `-I1udpbind` | `<fd:i32> [<ipv4|ipv6>:<port>|unix <path>]` | Repeatable | Create a default bound UDP socket. |
| `--wasip1-global-socket-udp-connect` | `--wasip1-socket-udp-connect`, `-I1udpcon` | `<fd:i32> [<ipv4|ipv6|dns>:<port>|unix <path>]` | Repeatable | Create a default connected UDP socket. |

The socket aliases above are available only when socket support is compiled. The exact alias strings come from the socket parameter headers; use `uwvm --help wasi` on the built binary if you need the authoritative compiled list.

## Single Target Command Table

All single commands require a module name. The module name must be non-empty and valid according to the Wasm text-format name validation path.

| Command | Alias | Arguments | Repeatability | Behavior |
| --- | --- | --- | --- | --- |
| `--wasip1-single-create` | `-I1Screate` | `<module:str>` | Once per module | Create an anonymous per-module WASI target. Must appear before other single commands for that module. |
| `--wasip1-single-enable` | `-I1Senable` | `<module:str>` | Once per target with disable conflict | Enable WASI Preview 1 for that module. |
| `--wasip1-single-disable` | `-I1Sdisable` | `<module:str>` | Once per target with enable conflict | Disable WASI Preview 1 for that module. |
| `--wasip1-single-expose-host-api` | `-I1Sexportapi` | `<module:str>` | Once per target with hide conflict | Expose the Preview 1 preload host API for that module. |
| `--wasip1-single-hide-host-api` | `-I1Shideapi` | `<module:str>` | Once per target with expose conflict | Hide the Preview 1 preload host API for that module. |
| `--wasip1-single-inherit-system-environment` | `-I1Ssysenv` | `<module:str>` | Once per target with noinherit conflict | Force host environment inheritance for that module. |
| `--wasip1-single-noinherit-system-environment` | `-I1Snosysenv` | `<module:str>` | Once per target with inherit conflict | Disable host environment inheritance for that module. |
| `--wasip1-single-enable-utf8-check` | `-I1Su8strict` | `<module:str>` | Once per target with disable conflict | Enable runtime WASI UTF-8 checks for that module. |
| `--wasip1-single-disable-utf8-check` | `-I1Su8relax` | `<module:str>` | Once per target with enable conflict | Disable runtime WASI UTF-8 checks for that module. |
| `--wasip1-single-trace` | `-I1Strace` | `<module:str> <none|out|err|file <file:path>>` | Once per target | Route that module's WASI trace output. A bare path is also accepted as file output. |
| `--wasip1-single-set-argv0` | `-I1Sargv0` | `<module:str> <argv0:str>` | Once per target | Override WASI `argv[0]` for that module. |
| `--wasip1-single-set-fd-limit` | `-I1Sfdlim` | `<module:str> <limit:size_t>` | Once per target | Override the fd limit for that module. |
| `--wasip1-single-add-environment` | `-I1Saddenv` | `<module:str> <env:str> <value:str>` | Repeatable with per-name conflict checks | Add or replace one variable for that module. |
| `--wasip1-single-delete-system-environment` | `-I1Sdelsysenv` | `<module:str> <env:str>` | Repeatable | Delete one inherited variable for that module. |
| `--wasip1-single-mount-dir` | `-I1Sdir` | `<module:str> <wasi dir:str> <system dir:path>` | Repeatable with mount-overlap checks | Add one module-specific directory mount. |
| `--wasip1-single-socket-tcp-listen` | `-I1Stcplisten` | `<module:str> <fd:i32> [<ipv4|ipv6>:<port>|unix <path>]` | Repeatable, no duplicate fd inside target | Add one TCP listening socket for the target. |
| `--wasip1-single-socket-tcp-connect` | `-I1Stcpcon` | `<module:str> <fd:i32> [<ipv4|ipv6|dns>:<port>|unix <path>]` | Repeatable, no duplicate fd inside target | Add one connected TCP socket for the target. |
| `--wasip1-single-socket-udp-bind` | `-I1Sudpbind` | `<module:str> <fd:i32> [<ipv4|ipv6>:<port>|unix <path>]` | Repeatable, no duplicate fd inside target | Add one bound UDP socket for the target. |
| `--wasip1-single-socket-udp-connect` | `-I1Sudpcon` | `<module:str> <fd:i32> [<ipv4|ipv6|dns>:<port>|unix <path>]` | Repeatable, no duplicate fd inside target | Add one connected UDP socket for the target. |

The single action callbacks first look up an existing single target. If `--wasip1-single-create <module>` has not already succeeded, later `--wasip1-single-* <module>` commands fail with a "single module does not exist" style error.

## Group Command Table

Group names must be non-empty. Unlike module names, group names are not validated through the Wasm UTF-8 name parser. Module names added to a group must be non-empty and valid Wasm UTF-8 names.

| Command | Alias | Arguments | Repeatability | Behavior |
| --- | --- | --- | --- | --- |
| `--wasip1-group-create` | `-I1Gcreate` | `<group:str>` | Once per group | Create a named shared WASI target. |
| `--wasip1-group-add-module` | `-I1Gaddmod` | `<group:str> <module:str>` | Repeatable | Bind one module name to an existing group. A module can be bound only once. |
| `--wasip1-group-enable` | `-I1Genable` | `<group:str>` | Once per group with disable conflict | Enable WASI Preview 1 for the group. |
| `--wasip1-group-disable` | `-I1Gdisable` | `<group:str>` | Once per group with enable conflict | Disable WASI Preview 1 for the group. |
| `--wasip1-group-expose-host-api` | `-I1Gexportapi` | `<group:str>` | Once per group with hide conflict | Expose the Preview 1 preload host API for the group. |
| `--wasip1-group-hide-host-api` | `-I1Ghideapi` | `<group:str>` | Once per group with expose conflict | Hide the Preview 1 preload host API for the group. |
| `--wasip1-group-inherit-system-environment` | `-I1Gsysenv` | `<group:str>` | Once per group with noinherit conflict | Force host environment inheritance for the group. |
| `--wasip1-group-noinherit-system-environment` | `-I1Gnosysenv` | `<group:str>` | Once per group with inherit conflict | Disable host environment inheritance for the group. |
| `--wasip1-group-enable-utf8-check` | `-I1Gu8strict` | `<group:str>` | Once per group with disable conflict | Enable runtime WASI UTF-8 checks for the group. |
| `--wasip1-group-disable-utf8-check` | `-I1Gu8relax` | `<group:str>` | Once per group with enable conflict | Disable runtime WASI UTF-8 checks for the group. |
| `--wasip1-group-trace` | `-I1Gtrace` | `<group:str> <none|out|err|file <file:path>>` | Once per group | Route the group's WASI trace output. A bare path is also accepted as file output. |
| `--wasip1-group-set-argv0` | `-I1Gargv0` | `<group:str> <argv0:str>` | Once per group | Override WASI `argv[0]` for the group. |
| `--wasip1-group-set-fd-limit` | `-I1Gfdlim` | `<group:str> <limit:size_t>` | Once per group | Override the fd limit for the group. |
| `--wasip1-group-add-environment` | `-I1Gaddenv` | `<group:str> <env:str> <value:str>` | Repeatable with per-name conflict checks | Add or replace one variable for the group. |
| `--wasip1-group-delete-system-environment` | `-I1Gdelsysenv` | `<group:str> <env:str>` | Repeatable | Delete one inherited variable for the group. |
| `--wasip1-group-mount-dir` | `-I1Gdir` | `<group:str> <wasi dir:str> <system dir:path>` | Repeatable with mount-overlap checks | Add one group-specific directory mount. |
| `--wasip1-group-socket-tcp-listen` | `-I1Gtcplisten` | `<group:str> <fd:i32> [<ipv4|ipv6>:<port>|unix <path>]` | Repeatable, no duplicate fd inside group | Add one TCP listening socket for the group. |
| `--wasip1-group-socket-tcp-connect` | `-I1Gtcpcon` | `<group:str> <fd:i32> [<ipv4|ipv6|dns>:<port>|unix <path>]` | Repeatable, no duplicate fd inside group | Add one connected TCP socket for the group. |
| `--wasip1-group-socket-udp-bind` | `-I1Gudpbind` | `<group:str> <fd:i32> [<ipv4|ipv6>:<port>|unix <path>]` | Repeatable, no duplicate fd inside group | Add one bound UDP socket for the group. |
| `--wasip1-group-socket-udp-connect` | `-I1Gudpcon` | `<group:str> <fd:i32> [<ipv4|ipv6|dns>:<port>|unix <path>]` | Repeatable, no duplicate fd inside group | Add one connected UDP socket for the group. |

Group action commands fail if the group does not already exist. Use `--wasip1-group-create <group>` before `--wasip1-group-* <group> ...`.

## Global Defaults And Overrides

The built-in Preview 1 module is globally enabled by default. `--wasip1-global-disable` sets `local_preload_wasip1` to false. A single or group target can then re-enable WASI for selected modules with `--wasip1-single-enable` or `--wasip1-group-enable`.

Import visibility resolution is:

- If the loaded module has a single/group override and that override explicitly set enable/disable, use the target value.
- Otherwise use the global `local_preload_wasip1` value.

Host API exposure has a similar default/override shape:

- `--wasip1-global-expose-host-api` sets the global exposure default and refreshes already loaded preload-DL and weak-symbol module host API state when those features are compiled.
- Target `expose-host-api` and `hide-host-api` set a target-specific boolean and conflict with each other.
- Native DL and weak-symbol loaders consult target overrides first; otherwise they use the global exposure default.

The existence of any target override can cause the WASI environment initialization path to run even when the global built-in module is disabled.

## Target Creation And Binding

Single target sequence:

```bash
uwvm \
  --wasip1-single-create app \
  --wasip1-single-enable app \
  --wasip1-single-mount-dir app /data ./data \
  --run app.wasm
```

Rules:

- `app` must be a valid Wasm UTF-8 module name.
- Creating the same single target twice is an error.
- Creating a single target for a module already bound to a named group is an error.
- Applying a single action before `--wasip1-single-create` is an error.

Named group sequence:

```bash
uwvm \
  --wasip1-group-create workers \
  --wasip1-group-add-module workers worker_a \
  --wasip1-group-add-module workers worker_b \
  --wasip1-group-set-fd-limit workers 256 \
  --run app.wasm
```

Rules:

- `workers` must be non-empty.
- Creating the same group twice is an error.
- `--wasip1-group-add-module` requires an existing group.
- A module can be added to only one named group.
- A module already configured as a single target cannot be added to a named group.

## Shared Target Conflict Rules

Single and group actions share the same target-action engine.

For one target, these pairs are mutually exclusive and cannot be repeated:

- `enable` and `disable`
- `expose-host-api` and `hide-host-api`
- `inherit-system-environment` and `noinherit-system-environment`
- `enable-utf8-check` and `disable-utf8-check`

Other single-assignment actions:

- `set-argv0` can be set once per target.
- `set-fd-limit` can be set once per target.
- `trace` can be set once per target, even if the second trace setting is identical.

Environment action rules inside one target:

- Environment names must be non-empty.
- Environment names must not contain `=`.
- `add-environment NAME VALUE` conflicts with `delete-system-environment NAME` in the same target.
- `delete-system-environment NAME` conflicts with a later `add-environment NAME VALUE` in the same target.
- Repeating `add-environment NAME VALUE` with the same value is accepted by the callback.
- Repeating `add-environment NAME OTHER_VALUE` with a different value is rejected.
- Repeating `delete-system-environment NAME` is accepted by the callback and has the same final effect.

Socket action rules inside one target:

- Target-specific socket commands reject duplicate fd numbers inside that same target.
- They do not reject a target fd that duplicates a global preopened socket fd during command parsing.
- Global plus target fd conflicts are detected later during WASI environment initialization as duplicate preopened fds.

## Environment Variable Semantics

Global environment initialization order:

1. If inheritance is enabled, read the host process environment.
2. Filter out host entries whose key is empty.
3. If inheritance is enabled, apply global delete rules.
4. Apply global add/replace rules. Add wins over delete.
5. Store final owned strings and expose string views to the WASI environment.

`--wasip1-global-noinherit-system-environment` disables step 1. Global delete rules then have nothing inherited to delete, but global add rules still create variables in the WASI environment.

Target environment initialization starts from the global command-line settings, then combines target settings:

- Target noinherit/inherit overrides the global inheritance mode when explicitly set.
- Global delete list and target delete list are concatenated.
- Global add list and target add list are concatenated.
- Target `argv0` replaces global `argv0` when explicitly set.
- Target mounts are appended after global mounts.
- Target sockets are appended after global sockets.

Because add rules are applied in list order and replace existing keys, target `add-environment` can override a global `add-environment` for the same variable. Target delete rules can remove inherited host variables, but they cannot remove a global add rule for the same name because all add rules run after all delete rules. Target callbacks reject conflicting add/delete pairs inside one target, but they do not reject a target action that interacts with global defaults.

Examples:

```bash
uwvm \
  --wasip1-global-noinherit-system-environment \
  --wasip1-global-add-environment MODE global \
  --wasip1-single-create app \
  --wasip1-single-inherit-system-environment app \
  --wasip1-single-add-environment MODE app \
  --run app.wasm
```

For module `app`, host environment inheritance is re-enabled and `MODE=app` wins over the global `MODE=global`.

```bash
uwvm \
  --wasip1-global-delete-system-environment SECRET \
  --wasip1-global-add-environment SECRET public \
  --run app.wasm
```

The final WASI environment contains `SECRET=public` because add/replace is applied after delete.

## `argv[0]` Semantics

By default, WASI argv is derived from `--run`:

- The Wasm path selected by `--run` participates in the guest argument list.
- Remaining tokens after `--run` become guest argv entries.

When a non-empty `wasip1_argv0_storage` is set, initialization inserts it as the first WASI argument and skips the original `--run` path entry. Target `set-argv0` overrides the global value for that target.

An empty string can be passed through the shell as `<argv0>`, and the callback will store it. During initialization, however, the argv code checks for a non-empty custom argv0; an empty global or target value therefore behaves like no custom `argv[0]`.

Use `--wasm-set-main-module-name` for internal module naming. Use WASI `set-argv0` only for the guest-visible argument vector.

## fd Limit Semantics

`--wasip1-global-set-fd-limit`, `--wasip1-single-set-fd-limit`, and `--wasip1-group-set-fd-limit` parse a `size_t`.

Rules:

- The whole token must parse as `size_t`.
- If `size_t` can represent values larger than the WASI fd type, values above the WASI fd maximum are rejected.
- A value of `0` is translated by the command-line callback to the maximum WASI fd value.
- If no fd limit is set, initialization treats internal `0` as default `1024`.
- Initialization requires room for stdio fds 0, 1, and 2. A configured limit below 3 parses successfully but fails during WASI environment initialization.
- Preopened sockets and directories count against the fd limit.

Directory preopens are assigned from fd 3 upward, skipping explicit socket fds. Explicit socket fds 0, 1, or 2 conflict with stdio during initialization.

Practical pattern:

```bash
uwvm \
  --wasip1-global-set-fd-limit 256 \
  --wasip1-global-socket-tcp-listen 10 127.0.0.1:8080 \
  --wasip1-global-mount-dir /data ./data \
  --run app.wasm
```

The socket is assigned fd 10. Directory mounts use the next available fd starting at 3.

## Trace Output

Global trace syntax:

```bash
uwvm --wasip1-global-trace none
uwvm --wasip1-global-trace out
uwvm --wasip1-global-trace err
uwvm --wasip1-global-trace file wasi.trace
```

Single/group trace syntax:

```bash
uwvm --wasip1-single-trace app none
uwvm --wasip1-single-trace app out
uwvm --wasip1-single-trace app err
uwvm --wasip1-single-trace app file app.trace
uwvm --wasip1-single-trace app app.trace
```

Differences:

- Global trace accepts only `none`, `out`, `err`, or `file <path>`.
- Target trace accepts those forms and also accepts a bare path as a convenience for file output.
- A trace file is opened during command-line processing to verify it can be opened.
- File trace is reopened during environment initialization.
- On Windows NT, trace file opening supports the `::NT::` branch through `reopen_wasip1_trace_output_file`, but that helper does not emit the same `nt-path` warning that Wasm loading, log output, compiler log output, and mounts emit.

`none` explicitly disables trace for that layer. A target trace setting overrides the global trace setting for that target. If a target does not set trace, it inherits the global trace configuration.

## Mount Directory Semantics

Syntax:

```bash
uwvm --wasip1-global-mount-dir <wasi-dir> <system-dir>
uwvm --wasip1-single-mount-dir <module> <wasi-dir> <system-dir>
uwvm --wasip1-group-mount-dir <group> <wasi-dir> <system-dir>
```

Mount parsing opens the host system directory immediately and stores a directory handle. It follows symlinks when opening. Runtime environment initialization later duplicates or observes that handle depending on the platform.

Host `system-dir` behavior:

- On Windows NT builds, a path beginning with `::NT::` enters the NT/kernel opener branch after stripping the first six bytes.
- The mount callback emits the `nt-path` warning for `::NT::` system directories when warning output is enabled.
- `--log-disable-warning nt-path` suppresses that warning.
- `--log-convert-warn-to-fatal nt-path` makes the warning fatal before opening.
- On non-Windows-NT builds, `::NT::...` is just an ordinary path string for the platform opener.
- On Windows 9x builds, mounts can emit a `toctou` warning because safe directory-handle semantics are unavailable there.

Guest `wasi-dir` rules:

- The value must be non-empty.
- A path starting with `/` is treated as absolute.
- Any other path is treated as relative.
- Relative `.` is allowed and means the relative root.
- Relative `./name`, `name/.`, `name/..`, and dot-only segments are rejected.
- Absolute `.` and `..` segments are rejected.
- `//` is rejected in both absolute and relative forms.
- Trailing `/` is removed for comparison and storage except for absolute root `/`.
- Relative trailing `/` is also removed.
- With UTF-8 checks enabled, `wasi-dir` must be valid RFC 3629 UTF-8 and contain no embedded zero.
- With UTF-8 checks disabled, embedded zero is still rejected.

Overlap checks:

- Exact duplicate mount points are rejected.
- Prefix overlaps are rejected in both directions.
- `/` conflicts with any other absolute mount.
- Relative `.` conflicts with any other relative mount.
- `/data` conflicts with `/data/cache`.
- `data` conflicts with `data/cache`.
- Absolute and relative namespaces are compared according to their own prefix rules; `/data` and `data` are not the same namespace.

Target mounts are validated by temporarily combining global mounts and the target's current mounts, so a target mount cannot overlap a global mount either.

Examples:

```bash
uwvm --wasip1-global-mount-dir /data ./data --run app.wasm
uwvm --wasip1-global-mount-dir data ./data --run app.wasm
uwvm --wasip1-global-mount-dir . ./sandbox --run app.wasm
```

Invalid combinations:

```bash
uwvm --wasip1-global-mount-dir /data ./data --wasip1-global-mount-dir /data/cache ./cache
uwvm --wasip1-global-mount-dir ./data ./data
uwvm --wasip1-global-mount-dir /data/../secret ./secret
uwvm --wasip1-global-mount-dir /data//cache ./cache
```

## Socket Semantics

Socket commands parse socket descriptions during command-line processing and open/connect/bind/listen sockets during WASI environment initialization.

Common fd rules:

- `<fd:i32>` is parsed through the WASI fd underlying type.
- The token must parse completely.
- Values greater than the WASI fd maximum are rejected during command-line processing.
- Negative-looking values normally cannot be consumed because these commands do not have parser pretreatment for `-1`; such tokens are treated as parameters before the callback sees them.
- Even when a negative fd reaches initialization through some future path, initialization rejects negative preopened socket fds.
- Fds 0, 1, and 2 conflict with stdio during initialization.
- Duplicate fds in global socket commands are not rejected during parsing, but initialization fails with duplicate preopened fd.
- Duplicate fds inside one single/group target are rejected immediately by the target wrapper.
- Duplicate fds between global and target sockets are detected during target environment initialization.

Address forms:

| Command kind | IP forms | DNS form | Unix form | Runtime behavior |
| --- | --- | --- | --- | --- |
| TCP listen | `<ipv4>:<port>`, `<ipv6>:<port>` | No | `unix <path>` when compiled | `bind`, then `listen` backlog 128. |
| TCP connect | `<ipv4>:<port>`, `<ipv6>:<port>` | `<dns>:<port>` | `unix <path>` when compiled | `connect`. |
| UDP bind | `<ipv4>:<port>`, `<ipv6>:<port>` | No | `unix <path>` when compiled | `bind`. |
| UDP connect | `<ipv4>:<port>`, `<ipv6>:<port>` | `<dns>:<port>` | `unix <path>` when compiled | `connect`. |

IP/DNS parsing order:

1. Try IPv4 with port.
2. If that fails, try IPv6 with port.
3. For connect commands only, if IPv6 also fails, try DNS with port.

DNS details:

- DNS is split at the first `:` after IP parsing fails.
- The DNS name is resolved during command-line processing.
- The first resolved address is stored.
- The port is parsed as an unsigned 16-bit value.
- If DNS lookup returns no address, the command fails.

Unix-domain details:

- The `unix <path>` form is available only when compiled.
- For global socket callbacks, `unix` without a following path falls through into normal address parsing and usually fails as a malformed address or missing DNS port.
- For single/group socket wrappers, `unix` without a following path is rejected as missing Unix socket path before the shared global parser is called.
- Filesystem Unix socket paths reject embedded NUL at initialization.
- Linux abstract namespace is supported by using a path that starts with `@`; this maps to a leading NUL in `sockaddr_un`.
- For Unix bind/listen in filesystem namespace, initialization tries to unlink an existing socket file before binding.
- Unix listen uses backlog 128.
- Empty Unix paths fail during initialization.
- Overlong Unix paths fail during initialization.

Protocol mapping:

- TCP commands create stream sockets with TCP protocol.
- UDP commands create datagram sockets with UDP protocol.
- Unix local sockets use local socket family and are stored with the requested connect/bind/listen handle type.

Examples:

```bash
uwvm --wasip1-global-socket-tcp-listen 3 127.0.0.1:8080 --run server.wasm
uwvm --wasip1-global-socket-tcp-connect 3 example.com:443 --run client.wasm
uwvm --wasip1-global-socket-udp-bind 5 0.0.0.0:5353 --run dns.wasm
uwvm --wasip1-global-socket-udp-connect 6 example.com:53 --run dns-client.wasm
```

Unix socket examples when compiled:

```bash
uwvm --wasip1-global-socket-tcp-listen 3 unix ./server.sock --run server.wasm
uwvm --wasip1-global-socket-tcp-connect 3 unix ./server.sock --run client.wasm
uwvm --wasip1-global-socket-udp-bind 4 unix @abstract-name --run app.wasm
```

## Initialization And Combination Order

WASI environment initialization happens after Wasm/preload modules have been loaded enough for module names and target bindings to be known.

The loader initializes the default environment when any of these are true:

- Global local WASI Preview 1 preload is enabled.
- Global host API exposure is enabled.
- Any single/group target override exists.

Then it initializes every configured target group that has an override.

For one target environment:

1. Copy or override trace from global.
2. Copy or override UTF-8 check flag from global.
3. Copy selected WASI callback pointers from global.
4. Copy or override fd limit from global.
5. Copy global sockets, then append target sockets.
6. Combine global and target environment delete/add rules.
7. Use target `argv0` when set; otherwise use global `argv0`.
8. Copy global mounts, then append target mounts.
9. Run the normal environment initialization flow to produce argv/env/fd state.

This means a target inherits global mounts, sockets, environment variables, trace, fd limit, and UTF-8 behavior unless that target explicitly overrides the relevant field. It also means target mounts and sockets are additive rather than replacements.

## Ordering Notes

Because callbacks run from left to right, order matters when a command performs immediate work.

Examples:

```bash
uwvm --wasi-disable-utf8-check --wasip1-global-mount-dir data ./data --run app.wasm
```

The mount's guest path is parsed under relaxed UTF-8 checking.

```bash
uwvm --wasip1-global-mount-dir data ./data --wasi-disable-utf8-check --run app.wasm
```

The mount was already validated before UTF-8 relaxation was enabled.

```bash
uwvm --wasip1-global-trace file wasi.trace --run app.wasm
```

The trace file is opened during parsing. If it cannot be opened, normal execution never starts.

```bash
uwvm --wasip1-single-enable app --wasip1-single-create app --run app.wasm
```

Invalid: the single target does not exist when the enable command is processed.

```bash
uwvm --wasip1-single-create app --wasip1-single-enable app --run app.wasm
```

Valid: the target exists before the action.

## Practical Patterns

Global sandbox with mounted data:

```bash
uwvm \
  --wasip1-global-noinherit-system-environment \
  --wasip1-global-add-environment PATH /bin \
  --wasip1-global-mount-dir /data ./data \
  --run app.wasm
```

Disable WASI by default, then enable only one module:

```bash
uwvm \
  --wasip1-global-disable \
  --wasip1-single-create plugin \
  --wasip1-single-enable plugin \
  --wasm-preload-library plugin.wasm plugin \
  --run app.wasm
```

Shared group for multiple preloaded modules:

```bash
uwvm \
  --wasip1-group-create workers \
  --wasip1-group-add-module workers worker_a \
  --wasip1-group-add-module workers worker_b \
  --wasip1-group-mount-dir workers /work ./work \
  --wasm-preload-library worker-a.wasm worker_a \
  --wasm-preload-library worker-b.wasm worker_b \
  --run app.wasm
```

Per-module trace file:

```bash
uwvm \
  --wasip1-single-create app \
  --wasip1-single-trace app file app.wasi.trace \
  --run app.wasm
```

Windows NT mount path:

```bash
uwvm --wasip1-global-mount-dir /data ::NT::\??\C:\sandbox\data --run app.wasm
```

On Windows NT, this strips `::NT::`, uses the NT opener, and can emit or fatalize the `nt-path` warning according to logging configuration.
