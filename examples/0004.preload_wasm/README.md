# `0004.preload_wasm` Example

This directory demonstrates a **preloaded wasm module** using its own
module-specific WASI Preview 1 configuration.

It focuses on two behaviors:

- a preloaded wasm can keep `wasi_snapshot_preview1` enabled even when the
  global default is disabled;
- the preloaded wasm can use its **own** WASI environment override such as
  `argv[0]`, independently of the main wasm.

## Files

- `lib.wat` — the preloaded wasm module `lib.test`
- `main.wat` — the main wasm that imports functions from `lib.test`

## What the preloaded module does

`lib.test` imports `wasi_snapshot_preview1.args_sizes_get`, stores the result in
its own linear memory, and exports:

- `get_argc`
- `get_argv_buf_size`

The main wasm checks:

- `argc == 3`
- `argv_buf_size == 16`

That `16` corresponds to:

```text
argv = ["pre", "hello", "world"]
```

## Build

```sh
wat2wasm lib.wat -o lib.wasm
wat2wasm main.wat -o main.wasm
```

## Positive path

This run demonstrates both features together:

- global WASI P1 is disabled;
- `lib.test` is explicitly re-enabled with `--wasip1-module lib.test enable`;
- global `argv[0]` is set to `global-entry`;
- `lib.test` overrides its own `argv[0]` to `pre`.

```sh
uwvm -Rcc int -Rcm full \
  --wasip1-disable \
  --wasip1-set-argv0 global-entry \
  --wasip1-module lib.test enable \
  --wasip1-module lib.test set-argv0 pre \
  --wasm-preload-library ./lib.wasm lib.test \
  --run ./main.wasm hello world
```

The process should exit successfully.

When global WASI P1 remains enabled, the inverse form also works:

```sh
--wasip1-module lib.test disable
```

That disables WASI Preview 1 only for `lib.test` while leaving the global default unchanged.

## Negative path: omit module-level enable

If you remove:

```sh
--wasip1-module lib.test enable
```

then `lib.test` can no longer import `wasi_snapshot_preview1`, and uwvm aborts
during initialization.

## Negative path: omit module-level `argv[0]`

If you keep the module enabled but remove:

```sh
--wasip1-module lib.test set-argv0 pre
```

then `lib.test` falls back to the global default `argv[0] = "global-entry"`,
so `argv_buf_size` is no longer `16`, and the main module traps in `start`.
