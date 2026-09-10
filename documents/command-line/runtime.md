# Runtime Commands

The production runtime has two eager, full-module execution backends:

- `--runtime-int` (`-Rint`): translate the complete module to the UWVM interpreter representation before execution.
- `--runtime-aot` (`-Raot`): translate and materialize the complete module with LLVM before execution.

There is no lazy interpreter, demand LLVM compiler, tiered/OSR runtime, or public custom mode/compiler axis. The historical `llvm_jit` names in source paths and LLVM-specific option names are implementation names; `--runtime-aot` is the only production entry to that backend.

LLVM AOT never routes an unsupported function through uwvm-int. If a validated Wasm type or opcode has no native LLVM lowering, `--runtime-aot` rejects compilation; the independent `--runtime-int` backend remains available for interpreter coverage and diagnosis.

Source focus:

- `src/uwvm2/uwvm/cmdline/params/runtime_*.h`
- `src/uwvm2/uwvm/cmdline/callback/runtime_*.h`
- `src/uwvm2/uwvm/runtime/runtime_mode/storage.h`
- `src/uwvm2/uwvm/run/run.h`
- `src/uwvm2/runtime/compiler/**/compile_all_from_uwvm/`

## Command Table

| Command | Alias | Arguments | Availability | Behavior |
| --- | --- | --- | --- | --- |
| `--runtime-int` | `-Rint` | None | UWVM interpreter | Select the full-module interpreter. |
| `--runtime-aot` | `-Raot` | None | LLVM backend | Select full-module LLVM AOT compilation. |
| `--runtime-compiler-log` | `-Rclog` | `[out\|err\|file <path>]` | Runtime backend support | Route compiler diagnostics. |
| `--runtime-compile-threads` | `-Rct` | `[default\|aggressive\|<count>]` | Runtime backend support | Configure full-module translation workers. |
| `--runtime-scheduling-policy` | `-Rsp` | `[func_count <count>\|code_size <bytes>]` | Runtime backend support | Split full-module translation work. |
| `--runtime-llvm-jit-policy` | `-Rllvm-policy` | `[debug\|default\|fast-compile\|balanced\|max]` | LLVM backend | Select an AOT optimization preset. |
| `--runtime-llvm-jit-full-policy` | `-Rllvm-full-policy` | `[auto\|debug\|legacy-light\|pb-o1\|pb-o2\|pb-o3]` | LLVM backend | Select the concrete full-module optimizer pipeline. |
| `--runtime-llvm-jit-call-stack` | `-Rllvm-call-stack` | Platform dependent | LLVM backend | Select generated-code stack tracking. |
| `--runtime-llvm-jit-cache-path` | `-Rllvm-cache-path` | `[disable\|default\|path <path>]` | LLVM backend | Configure the AOT object cache. |

The UWVM interpreter also exposes translation tuning options when their corresponding build features are available:

- `--runtime-uwvm-int-disable-loop-unwind` (`-Rint-no-loop-unwind`)
- `--runtime-uwvm-int-set-opcode-conbination-level` (`-Rint-op-conbine-level`)
- `--runtime-uwvm-int-disable-delay-local` (`-Rint-no-delay-local`)
- `--runtime-uwvm-int-enable-instruction-reorder` (`-Rint-reorder`)
- `--runtime-uwvm-int-loop-unwind-max-size` (`-Rint-loop-unwind-size`)

## Runtime Selection

`runtime_mode_t` contains only `full_compile`. Runtime selection therefore chooses a backend, not a compilation lifecycle:

| Shortcut | Backend | Preparation rule |
| --- | --- | --- |
| `--runtime-int` | UWVM interpreter | Translate every validated function before entering the module. |
| `--runtime-aot` | LLVM AOT | Generate, optimize, and materialize the complete module before entering it. |

When multiple production backends are compiled, LLVM AOT is the default; otherwise the full interpreter is used. Runtime shortcuts are guarded against duplicates and conflicting selections.

Examples:

```bash
uwvm --runtime-int --run app.wasm
uwvm --runtime-aot --run app.wasm
```

Removed commands such as `--runtime-jit`, `--runtime-tiered`, `--runtime-custom-mode`, and `--runtime-custom-compiler` are command-line errors rather than aliases for AOT.

## Compiler Log

```bash
uwvm --runtime-compiler-log out --runtime-int --run app.wasm
uwvm --runtime-compiler-log err --runtime-aot --run app.wasm
uwvm --runtime-compiler-log file compiler.log --runtime-aot --run app.wasm
```

The compiler log is independent of the main diagnostic destination selected by `--log-output`. The file form requires the literal `file` argument.

## LLVM AOT Policies

The retained high-level presets are:

- `debug`: no LLVM optimization.
- `default`: use the normal full-module default.
- `fast-compile`: favor compilation latency.
- `balanced`: use the tuned PassBuilder O1 path.
- `max`: use the tuned PassBuilder O3 path and aggressive code generation.

The scoped full-module policies map directly to `auto`, `debug`, `legacy-light`, `pb-o1`, `pb-o2`, and `pb-o3`. The high-level and scoped policy commands are mutually exclusive.

```bash
uwvm --runtime-aot --runtime-llvm-jit-policy max --run app.wasm
uwvm --runtime-aot --runtime-llvm-jit-full-policy pb-o2 --run app.wasm
```

All generated Wasm functions carry LLVM `NoInline`, including under `max`/`pb-o3`. Optimizers may still transform code inside each function, but they do not merge one Wasm function body into another. This keeps patchable AOT boundaries and native stack identities stable.

## LLVM AOT Call Stacks

`--runtime-llvm-jit-call-stack` accepts `auto`, `instruction`, and `none` everywhere. Auxiliary-only POSIX native-unwind targets additionally expose `unwind-uncheck`; checked `unwind` is exposed only where Win64 SEH supplies an authoritative generated-caller context.

- `auto`: use native unwind only when the platform supplies an explicit generated-caller context that can replace logical frames; otherwise use instruction tracking.
- `instruction`: emit explicit per-function stack push/pop operations.
- `none`: omit generated Wasm body frames from trap diagnostics.
- `unwind`: require an authoritative native replacement for generated logical frames. The CLI exposes this only for the supported Win64 SEH caller-context path; unsupported programmatic selection fails closed.
- `unwind-uncheck` (alias `unwind-unchecked`): enable the compiled native unwind path as auxiliary information. On POSIX, logical frames remain emitted, printed first, and authoritative.

The native-unwind path reports concrete generated functions from registered code ranges. It does not emit synthetic DWARF inline scopes, seeded POSIX cursors, frame-pointer scans, raw-stack scans, or inline call-chain expansion. POSIX uses an ordinary `<unwind.h>` backtrace after the authoritative logical stack. Win64 SEH alone receives explicit generated frame/stack context. AOT functions emit asynchronous unwind tables when native unwind is selected; fixed frame pointers are limited to the Win64 SEH bridge.

## LLVM IR Verification and Cache

LLVM IR verification is mandatory in this reduced runtime and cannot be disabled from the command line. This is independent of the earlier Wasm validation step; both must succeed.

The full-module backend can read and write native object-cache entries. Cache path selection is `default`, `disable`, or `path <directory>`. When caching is enabled, every newly written object is signed and every loaded object must have a valid signature. There are no command-line overrides for signing or verification.

Persistent native-object caching requires a clean Git commit identity or a verified complete-source manifest ID. Source-archive packagers can pass `--build-source-id=sha256:<64 lowercase hex digits>`; the value must be regenerated whenever any packaged source input changes. Official release archives should carry the stable hash of a normalized, complete source manifest rather than a tag, version string, timestamp, path, or build artifact.

Dirty Git worktrees and builds with neither identity fail closed even when an explicit cache path is requested. A Wasm-derived LLVM module hash does not cover host bridge/runtime/unwind semantics, and the cache signature provides integrity and context binding rather than source provenance. Developers may separately accept these risks with `UWVM2_ALLOW_UNSAFE_DIRTY_LLVM_JIT_CACHE` or `UWVM2_ALLOW_UNSAFE_UNPROVENANCED_LLVM_JIT_CACHE`; release builds must not define either macro.

The signing identity is deterministically derived from the local execution context. It detects damaged entries, incompatible-context substitution, and accidental cache reuse; it is not a secret-key trust boundary and does not protect against an attacker who can act as the same OS account. Put the cache in an account-private directory, and use `disable` whenever that ownership boundary is not sufficient.

```bash
uwvm --runtime-aot --runtime-llvm-jit-cache-path default --run app.wasm
uwvm --runtime-aot --runtime-llvm-jit-cache-path path ./uwvm-cache --run app.wasm
```

## Compile Threads and Scheduling

`--runtime-compile-threads` accepts:

- `default`: an adaptive worker count based on hardware concurrency.
- `aggressive`: roughly two thirds of detected hardware threads.
- `0`: serial translation.
- positive `N`: request exactly `N` extra workers, subject to useful task counts.
- negative `-K`: detected hardware threads minus `K`.

`--runtime-scheduling-policy` groups full-module tasks either by function count or cumulative Wasm code-body bytes. The default is `code_size 4096`. Worker counts can be reduced when the module does not contain enough useful task groups.

```bash
uwvm \
  --runtime-aot \
  --runtime-compile-threads aggressive \
  --runtime-scheduling-policy code_size 8192 \
  --run app.wasm

uwvm \
  --runtime-int \
  --runtime-compile-threads 4 \
  --runtime-scheduling-policy func_count 16 \
  --run app.wasm
```
