# Runtime Commands

Source focus:

- `src/uwvm2/uwvm/cmdline/params/runtime_*.h`
- `src/uwvm2/uwvm/cmdline/callback/runtime_*.h`
- `src/uwvm2/uwvm/runtime/runtime_mode/storage.h`
- `src/uwvm2/uwvm/run/run.h`
- `src/uwvm2/runtime/compiler/**/compile_all_from_uwvm/translate/single_func.h`

## Command Table

| Command | Aliases | Arguments | Repeatability | Availability | Behavior |
| --- | --- | --- | --- | --- | --- |
| `--runtime-custom-mode` | `-Rcm` | `[lazy|lazy+verification|full]` | Once | Runtime backend support | Set only the runtime compilation mode axis. |
| `--runtime-custom-compiler` | `-Rcc` | `[int|tiered|jit|debug-int]` | Once | Compiled backend dependent | Set the runtime compiler backend axis; `int` defaults to uwvm-int auto lazy/full when no custom mode is specified. `debug-int` appears only when `--debug-int=y` is configured. |
| `--runtime-debug-int` | `-RDint` | None | Once | `UWVM_RUNTIME_DEBUG_INTERPRETER` (`--debug-int=y`; disabled by default) | Shortcut: selects full compilation with debug interpreter; executable dispatch currently reports this backend as unsupported. |
| `--runtime-int` | `-Rint` | None | Once | `UWVM_RUNTIME_UWVM_INTERPRETER` | Shortcut: auto lazy/full selection with UWVM interpreter. |
| `--runtime-jit` | `-Rjit` | None | Once | `UWVM_RUNTIME_LLVM_JIT` | Shortcut: lazy compilation with LLVM JIT. |
| `--runtime-tiered` | `-Rtiered` | None | Once | `UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED` | Shortcut: lazy tiered interpreter plus LLVM JIT. |
| `--runtime-aot` | `-Raot` | None | Once | `UWVM_RUNTIME_LLVM_JIT` | Shortcut: full compilation with LLVM JIT. |
| `--runtime-compiler-log` | `-Rclog` | `[out|err|file <file:path>]` | Once | Runtime backend support | Route runtime compiler logs. |
| `--runtime-uwvm-int-disable-loop-unwind` | `-Rint-no-loop-unwind` | None | Once | `UWVM_RUNTIME_UWVM_INTERPRETER` | Disable uwvm-int loop-unwind translation at runtime. |
| `--runtime-uwvm-int-disable-opcode-conbination` | `-Rint-no-op-conbine` | None | Once | `UWVM_RUNTIME_UWVM_INTERPRETER` | Disable uwvm-int opcode conbination peepholes at runtime. |
| `--runtime-uwvm-int-disable-delay-local` | `-Rint-no-delay-local` | None | Once | `UWVM_RUNTIME_UWVM_INTERPRETER` | Disable uwvm-int delay-local peepholes at runtime. |
| `--runtime-uwvm-int-loop-unwind-max-size` | `-Rint-loop-unwind-size` | `<bytes:size_t>` | Once | `UWVM_RUNTIME_UWVM_INTERPRETER` | Set the per-loop Wasm body byte budget used by loop-unwind decisions. |
| `--runtime-llvm-jit-policy` | `-Rllvm-policy` | `[debug|default|fast-compile|balanced|max]` | Once | `UWVM_RUNTIME_LLVM_JIT` or `UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED` | Select the high-level LLVM JIT strategy policy. |
| `--runtime-llvm-jit-lazy-policy` | `-Rllvm-lazy-policy` | `[auto|debug|light|balanced]` | Once | `UWVM_RUNTIME_LLVM_JIT` or `UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED` | Select the lazy/tier-1 LLVM JIT strategy. |
| `--runtime-llvm-jit-full-policy` | `-Rllvm-full-policy` | `[auto|debug|legacy-light|pb-o1|pb-o2|pb-o3]` | Once | `UWVM_RUNTIME_LLVM_JIT` or `UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED` | Select the full/tier-2 LLVM JIT strategy. |
| `--runtime-llvm-jit-call-stack` | `-Rllvm-call-stack` | `[auto|instruction|none|unwind|unwind-uncheck]` | Once | `UWVM_RUNTIME_LLVM_JIT` or `UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED` | Select LLVM-JIT call-stack tracking mode. |
| `--runtime-llvm-jit-disable-ir-verifaction` | `-Rllvm-noverify` | None | Once | `UWVM_RUNTIME_LLVM_JIT` or `UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED` | Disable LLVM IR verification in LLVM-JIT runtime paths. |
| `--runtime-compile-threads` | `-Rct` | `[default|aggressive|<count:ssize_t>]` | Once | Runtime backend support | Set compile-thread policy or numeric thread count. |
| `--runtime-scheduling-policy` | `-Rsp` | `[func_count <count:size_t>|code_size <bytes:size_t>]` | Once | Runtime backend support | Set full-compile task splitting policy. |

## Runtime Selection Model

Runtime selection has two independent axes in storage:

- Mode: `auto_compile`, `lazy_compile`, `lazy_compile_with_full_code_verification`, or `full_compile`.
- Compiler: `uwvm_interpreter_only`, `uwvm_interpreter_llvm_jit_tiered`, `llvm_jit_only`, `debug_interpreter`, or `none_backend` depending on compiled support.

Default storage values:

- Mode defaults to `lazy_compile`.
- Compiler defaults to `uwvm_interpreter_llvm_jit_tiered` when tiered support is compiled, otherwise to `llvm_jit_only` when LLVM JIT support is compiled,
  otherwise to `uwvm_interpreter_only` when interpreter support is compiled, otherwise to `none_backend`.

The shortcuts write both axes. The custom commands normally write one axis each, with one interpreter-specific default:

- `--runtime-custom-compiler int` also default-fills the mode axis with `auto_compile` when `--runtime-custom-mode` is omitted.
- `--runtime-custom-compiler jit` and `tiered` do not use `auto_compile`; when `--runtime-custom-mode` is omitted they use the current mode value, normally the storage default `lazy_compile`.
- `--runtime-custom-compiler debug-int` is compiled only when Xmake config `--debug-int=y` is used. It does not use `auto_compile`; if its inherited mode is not `full_compile`, runtime normalization forces `full_compile` and uses the runtime-warning policy. Executable dispatch currently reports debug interpreter as unsupported.
- `--runtime-custom-mode` remains explicit in either argument order. `--runtime-custom-mode full --runtime-custom-compiler int` and `--runtime-custom-compiler int --runtime-custom-mode full` both run concrete `full_compile`, not auto.

## Runtime Shortcut Commands

Shortcut mapping:

| Shortcut | Mode | Compiler |
| --- | --- | --- |
| `--runtime-int` | `auto_compile` resolved to `lazy_compile` or `full_compile` before execution | `uwvm_interpreter_only` |
| `--runtime-jit` | `lazy_compile` | `llvm_jit_only` |
| `--runtime-tiered` | `lazy_compile` | `uwvm_interpreter_llvm_jit_tiered` |
| `--runtime-aot` | `full_compile` | `llvm_jit_only` |
| `--runtime-debug-int` | `full_compile` | `debug_interpreter` when compiled with `--debug-int=y`; currently rejected by executable dispatch |

UWVM interpreter auto policy:

- `auto_compile` is used by `--runtime-int`, and by `--runtime-custom-compiler int` when no explicit `--runtime-custom-mode` is supplied.
- `auto_compile` is not a public custom-mode argument; it is resolved internally before runtime dispatch.
- LLVM-JIT and tiered backends never use `auto_compile`. Use `--runtime-custom-mode lazy|lazy+verification|full` when their mode must be controlled explicitly.
- Without preloaded Wasm modules, select `full_compile` when the executable Wasm file is at most 1 MiB; otherwise select `lazy_compile`.
- With preloaded Wasm modules, select `full_compile` when executable plus preloaded Wasm bytes total at most 256 KiB; otherwise select `lazy_compile`.
- `--log-verbose` prints `uwvm-int auto selected lazy|full compile for uwvm-int` with executable Wasm bytes, preloaded Wasm bytes, total Wasm bytes, threshold, and policy.

Verbose example:

```text
uwvm: [info]  uwvm-int auto selected full compile for uwvm-int (main-wasm-bytes=241035, preload-wasm-bytes=0, total-wasm-bytes=241035, threshold=1048576, policy=main-only). [timestamp] (verbose)
```

Supported execution combinations:

- `auto_compile`: uwvm-int only, from `--runtime-int` or `--runtime-custom-compiler int` without `--runtime-custom-mode`.
- `lazy_compile` and `lazy_compile_with_full_code_verification`: `int`, `jit`, and `tiered`.
- `full_compile`: `int` and `jit`.
- `full_compile + tiered` is rejected because tiered execution is a lazy/tiered strategy.
- `debug-int` is accepted by command-line parsing only when configured with `--debug-int=y`, but executable dispatch currently reports the debug interpreter backend as unsupported. The default Xmake configuration keeps it disabled.

Conflict rules enforced by callbacks:

- Only one shortcut may be used.
- A shortcut conflicts with `--runtime-custom-mode`.
- A shortcut conflicts with `--runtime-custom-compiler`.
- `--runtime-custom-mode` conflicts with any shortcut already seen.
- `--runtime-custom-compiler` conflicts with any shortcut already seen.
- Parser-level duplicate guards also reject repeating the same shortcut.

LLVM JIT policy defaults:

- Effective `full_compile + llvm_jit_only` execution, including `--runtime-aot` and `--runtime-custom-mode full --runtime-custom-compiler jit`, defaults to the full/tier-2 max policy when no explicit LLVM JIT policy is supplied.
- Tiered execution keeps its existing default tier-2 policy unless an LLVM JIT policy is explicitly supplied.
- Explicit `--runtime-llvm-jit-policy` or `--runtime-llvm-jit-full-policy` still takes precedence.

Valid:

```bash
uwvm --runtime-custom-mode lazy --runtime-custom-compiler jit --run app.wasm
uwvm --runtime-custom-compiler int --log-verbose --run app.wasm
uwvm --runtime-custom-compiler int --runtime-custom-mode lazy --run app.wasm
```

Invalid:

```bash
uwvm --runtime-jit --runtime-aot --run app.wasm
uwvm --runtime-jit --runtime-custom-mode full --run app.wasm
uwvm --runtime-custom-mode full --runtime-custom-compiler tiered --run app.wasm
```

## `--runtime-custom-mode`

Accepted values:

- `lazy`: functions are compiled or translated lazily when needed.
- `lazy+verification`: lazy compilation with full code verification.
- `full`: full compilation before execution.

Notes:

- The value is strict and case-sensitive.
- The option sets only the mode axis.
- It does not choose a backend; combine it with `--runtime-custom-compiler` when you need both axes.
- It has an `is_exist` guard.

Example:

```bash
uwvm --runtime-custom-mode lazy+verification --runtime-custom-compiler jit --run app.wasm
```

## `--runtime-custom-compiler`

Accepted values depend on compiled backends:

- `int`: UWVM interpreter backend.
- `tiered`: UWVM interpreter plus LLVM JIT tiered backend.
- `jit`: LLVM JIT backend.
- `debug-int`: debug interpreter backend; available only with Xmake `--debug-int=y`, disabled by default, and executable dispatch currently reports this backend as unsupported.

Notes:

- A value for a backend that was not compiled is not accepted, because the usage string and callback are built with feature macros.
- The option normally sets only the compiler axis.
- `int` defaults the mode axis to `auto_compile` when no explicit `--runtime-custom-mode` is supplied, matching `--runtime-int`.
- `jit` and `tiered` do not default to auto. If no custom mode is supplied, they inherit the current mode value, normally `lazy_compile`.
- `debug-int` does not default to auto. When compiled with `--debug-int=y`, if no custom mode is supplied it inherits `lazy_compile` and is then normalized to `full_compile` by the debug-interpreter runtime rule; executable dispatch then reports the backend as unsupported.
- An explicit `--runtime-custom-mode lazy|lazy+verification|full` before or after `--runtime-custom-compiler int` remains explicit and does not use auto.
- It has an `is_exist` guard.

Example:

```bash
uwvm --runtime-custom-compiler int --log-verbose --run app.wasm
uwvm --runtime-custom-mode full --runtime-custom-compiler int --run app.wasm
uwvm --runtime-custom-compiler int --runtime-custom-mode full --run app.wasm
```

## `--runtime-compiler-log`

Syntax:

```bash
uwvm --runtime-compiler-log out
uwvm --runtime-compiler-log err
uwvm --runtime-compiler-log file compiler.log
```

Behavior:

- `out` duplicates standard output.
- `err` duplicates standard error.
- `file <path>` opens a dedicated output file.
- The option has an `is_exist` guard.
- The file form must include the literal `file`; bare file paths are rejected for this command.
- On Windows NT, `::NT::` paths use the NT/kernel opener after stripping the prefix and can emit or fatalize the `nt-path` warning.
- Open failure is a command-line error.

This log is for runtime compiler internals. It is separate from main diagnostics configured by `--log-output`.

## `--runtime-llvm-jit-policy`

Syntax:

```bash
uwvm --runtime-llvm-jit-policy default
uwvm --runtime-llvm-jit-policy debug
uwvm --runtime-llvm-jit-policy fast-compile
uwvm --runtime-llvm-jit-policy balanced
uwvm --runtime-llvm-jit-policy max
uwvm -Rllvm-policy balanced
```

Behavior:

- `default`: preserve existing defaults. Lazy/tier-1 stays latency-oriented; full/tier-2 keeps the legacy lightweight pipeline.
- `debug`: disable LLVM optimization for both lazy/tier-1 and full/tier-2. This is intended for future debug-symbol and source/Wasm/native correlation work.
- `fast-compile`: keep lazy/tier-1 and full/tier-2 compile latency low. Full/tier-2 uses `legacy-light` with LLVM codegen `less`.
- `balanced`: keep lazy/tier-1 light, but make full/tier-2 use the tuned PassBuilder O1 pipeline. This is the preferred CPython long-run tradeoff found by local probes so far.
- `max`: keep lazy/tier-1 light, but make full/tier-2 use the tuned PassBuilder O3 pipeline with aggressive codegen.
- The policy is a strategy preset, not a wall-clock feedback input. Runtime tiering decisions must still be driven by counters and execution events.
- This high-level preset is mutually exclusive with scoped policies. Do not combine it with `--runtime-llvm-jit-lazy-policy` or `--runtime-llvm-jit-full-policy`.
- Use the scoped policy pair when you need a custom lazy/full combination.
- Call-stack tracking is controlled separately by `--runtime-llvm-jit-call-stack`.

## `--runtime-llvm-jit-lazy-policy`

Syntax:

```bash
uwvm --runtime-llvm-jit-lazy-policy auto
uwvm --runtime-llvm-jit-lazy-policy debug
uwvm --runtime-llvm-jit-lazy-policy light
uwvm --runtime-llvm-jit-lazy-policy balanced
uwvm -Rllvm-lazy-policy light
```

Behavior:

- `auto`: use the backend default.
- `debug`: force lazy/tier-1 LLVM codegen to `none` and avoid LLVM codegen optimization.
- `light`: force lazy/tier-1 LLVM codegen to `less`.
- `balanced`: force lazy/tier-1 LLVM codegen to `default`.
- Lazy/tier-1 intentionally does not select the full PassBuilder module pipeline; it remains the low-latency path for short tasks and tiered warmup.
- This scoped policy can be combined with `--runtime-llvm-jit-full-policy`, but it conflicts with `--runtime-llvm-jit-policy`.

## `--runtime-llvm-jit-full-policy`

Syntax:

```bash
uwvm --runtime-llvm-jit-full-policy auto
uwvm --runtime-llvm-jit-full-policy debug
uwvm --runtime-llvm-jit-full-policy legacy-light
uwvm --runtime-llvm-jit-full-policy pb-o1
uwvm --runtime-llvm-jit-full-policy pb-o2
uwvm --runtime-llvm-jit-full-policy pb-o3
uwvm -Rllvm-full-policy pb-o1
```

Behavior:

- `auto`: use the backend default.
- `debug`: use no full-module optimizer pipeline and LLVM codegen `none`.
- `legacy-light`: use the old lightweight full-module optimizer with the backend default codegen level.
- `pb-o1`: use the tuned PassBuilder O1 full-module pipeline and LLVM codegen `less`.
- `pb-o2`: use the tuned PassBuilder O2 full-module pipeline and LLVM codegen `default`.
- `pb-o3`: use the tuned PassBuilder O3 full-module pipeline and LLVM codegen `aggressive`.
- Runtime compiler logs include `policy=`, `pipeline=`, `codegen_opt=`, `call_stack=`, `unwind_backend=`, `unwind_check=`, `unwind_replace_frames=`, and `call_stack_frames=` on `[llvm-jit-full] optimize-start` lines.
- This scoped policy can be combined with `--runtime-llvm-jit-lazy-policy`, but it conflicts with `--runtime-llvm-jit-policy`.

## `--runtime-llvm-jit-call-stack`

Syntax:

```bash
uwvm --runtime-llvm-jit-call-stack auto
uwvm --runtime-llvm-jit-call-stack instruction
uwvm --runtime-llvm-jit-call-stack none
uwvm --runtime-llvm-jit-call-stack unwind
uwvm --runtime-llvm-jit-call-stack unwind-uncheck
uwvm -Rllvm-call-stack none
```

Behavior:

- `auto`: default. If an unwind backend is present, try `unwind`; if the generated-code live unwind probe fails, warn once and fall back to `instruction`. If runtime warnings are disabled, the fallback is silent; if runtime warnings are converted to fatal errors, the fallback warning is fatal.
- `instruction`: emit per-function LLVM JIT call-stack push/pop instructions. This preserves trap diagnostics without relying on generated-code unwind metadata.
- `none`: omit per-function LLVM JIT call-stack push/pop instructions. This can substantially improve hot Wasm-to-Wasm call workloads, but trap stack reports lose JIT body frames.
- `unwind`: force generated-code unwind metadata for LLVM JIT frames at trap-report time. This value is only accepted when the build can see an unwind backend. At runtime, uwvm runs a tiny generated-code live unwind probe once before replacing instruction frames; failure is fatal instead of falling back to `instruction`.
- `unwind-uncheck`: use the same generated-code unwind metadata as `unwind`, but skip the live unwind probe. This is intended for deployments that have already validated the JIT unwind path and want to avoid the check on startup.
- `unwind-unchecked`: accepted as an alias for `unwind-uncheck`.
- Default: `auto`.
- uwvm itself may still be built without unwind tables. Host-runtime unwindability is not a requirement for JIT unwind mode, and uwvm does not warn when ordinary host-runtime frames cannot be walked.
- On Apple targets, uwvm registers generated `.eh_frame` FDEs directly. On Win64 x86_64 targets, uwvm registers generated SEH function tables with the OS.
- This command is independent of `--runtime-llvm-jit-policy`, `--runtime-llvm-jit-lazy-policy`, and `--runtime-llvm-jit-full-policy`.
- Runtime compiler logs report the selected mode as `call_stack=`, the selected unwind backend as `unwind_backend=`, the live-probe decision as `unwind_check=live-jit|skipped|off`, whether generated-code unwind metadata is allowed to replace instruction frames as `unwind_replace_frames=`, and the actual instruction-frame emission as `call_stack_frames=`.

## `--runtime-llvm-jit-disable-ir-verifaction`

Behavior:

- LLVM IR verification is enabled by default for LLVM-JIT runtime compilation.
- This command disables the LLVM IR verifier for LLVM-JIT full, lazy, and tiered runtime paths.
- Alias: `-Rllvm-noverify`.
- It has an `is_exist` guard.
- It is compiled only when `UWVM_RUNTIME_LLVM_JIT` or `UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED` is enabled.
- It does not disable Wasm validation or lazy full-code verification selected by `--runtime-custom-mode lazy+verification`.

Failure model when verification is enabled:

- LLVM verifier failure means UWVM2 generated invalid LLVM IR.
- UWVM2 treats that as an internal UWVM2 bug, not as a guest Wasm validation error.
- Debug builds with detailed debug checks call the debug bug trap before termination; release builds terminate.

Example:

```bash
uwvm --runtime-aot --runtime-llvm-jit-disable-ir-verifaction --run app.wasm
```

## `--runtime-compile-threads`

Syntax:

```bash
uwvm --runtime-compile-threads default
uwvm --runtime-compile-threads aggressive
uwvm --runtime-compile-threads 0
uwvm --runtime-compile-threads 4
uwvm --runtime-compile-threads -1
```

Parse-time behavior:

- The option accepts `default`, `aggressive`, or a complete `ssize_t`.
- It has a pretreatment hook so negative numeric values such as `-1` are consumed as values, not parsed as options.
- Invalid strings are rejected immediately.
- The option has an `is_exist` guard.

Runtime resolution:

- If omitted, the runtime uses the default adaptive policy.
- On platforms with `fast_io::native_thread`, detected max threads come from `hardware_concurrency()`, with `0` normalized to `1`.
- Default policy resolves to roughly `floor(log2(max_threads))`, with a minimum of `1` when native threads are available.
- Aggressive policy resolves to `floor(max_threads * 2 / 3)`, and resolves to `0` if max threads is less than `2`.
- Numeric `0` means no extra compile threads.
- Numeric positive `N` means exactly `N` extra compile threads. If `N` is greater than detected max, UWVM warns but still uses it.
- Numeric negative `-K` means `detected_max_threads - K`.
- If `K` is greater than detected max, UWVM emits a fatal runtime compile-thread error.
- On platforms without `fast_io::native_thread`, adaptive policies resolve to `0`, and non-zero numeric requests warn and fall back to `0`.

Warnings:

- Warning class: `runtime-compile-threads`.
- Suppress with `--log-disable-warning runtime-compile-threads`.
- Fatalize with `--log-convert-warn-to-fatal runtime-compile-threads`.

Full-compile adjustment:

- The resolved value is an upper bound for adaptive policies.
- Per-module compilation may reduce the effective worker count for small modules.
- Explicit numeric settings are not described as adaptive upper-bound policies, but task scheduling can still clamp workers to useful task counts.

Examples:

```bash
uwvm --runtime-compile-threads default --run app.wasm
uwvm --runtime-compile-threads aggressive --runtime-aot --run app.wasm
uwvm --runtime-compile-threads -1 --runtime-custom-mode full --runtime-custom-compiler jit --run app.wasm
```

## `--runtime-scheduling-policy`

Syntax:

```bash
uwvm --runtime-scheduling-policy func_count 8
uwvm --runtime-scheduling-policy code_size 4096
```

Behavior:

- `func_count <count>` groups full-compile translation tasks by function count.
- `code_size <bytes>` groups full-compile translation tasks by cumulative Wasm code-body size.
- The size/count argument must parse completely as `size_t`.
- The size/count must be greater than zero.
- The option has an `is_exist` guard.
- Default storage is `code_size 4096`.

Runtime effect:

- It matters when full compilation can use extra compile threads.
- It has no practical effect when the runtime compiles serially.
- The compiler may adjust `code_size` thresholds for adaptive policies to avoid creating too many tiny tasks.
- Worker count is clamped to the number of task groups that actually exist.

Examples:

```bash
uwvm --runtime-aot --runtime-compile-threads 4 --runtime-scheduling-policy code_size 8192 --run app.wasm
uwvm --runtime-custom-mode full --runtime-custom-compiler int --runtime-scheduling-policy func_count 16 --run app.wasm
```

## Combination Patterns

Lazy JIT:

```bash
uwvm --runtime-jit --run app.wasm
```

Full LLVM JIT compilation with explicit scheduling:

```bash
uwvm \
  --runtime-aot \
  --runtime-compile-threads aggressive \
  --runtime-scheduling-policy code_size 8192 \
  --run app.wasm
```

Custom mode/backend pair:

```bash
uwvm \
  --runtime-custom-mode lazy+verification \
  --runtime-custom-compiler tiered \
  --run app.wasm
```

UWVM interpreter with automatic lazy/full selection:

```bash
uwvm \
  --runtime-custom-compiler int \
  --log-verbose \
  --run app.wasm
```

UWVM interpreter with explicit full compile:

```bash
uwvm \
  --runtime-custom-compiler int \
  --runtime-custom-mode full \
  --run app.wasm
```

Main diagnostics plus compiler log:

```bash
uwvm \
  --log-output file uwvm.log \
  --runtime-compiler-log file compiler.log \
  --runtime-aot \
  --run app.wasm
```

LLVM JIT without LLVM IR verification:

```bash
uwvm \
  --runtime-custom-mode full \
  --runtime-custom-compiler jit \
  --runtime-llvm-jit-disable-ir-verifaction \
  --run app.wasm
```
