# Eight-object partition regression

`fixture.wat` is a real Wasm3 module with 26 retained function definitions,
including sixteen exported marker functions. It checks mutable global and
linear-memory state across those calls, active data, a mutated function table,
direct/indirect/typed `call_ref`, and a cross-function `throw` caught by
`try_table`. The checksum is 42; the sixteen marker cells sum to 248.

Run `run.py` only in the existing SSH Linux 64 GiB cgroup. It uses the installed
official `wasm-tools 1.262.0 parse`, first validates the exact module with
official Wasmtime 42.0.1, then uses `-Rct 7` and disables the LLVM
cache for each UWVM product. Every ordinary and real debugger run must contain
`object-emit-end` with at least sixteen definitions and exactly eight objects.
Fallback, skip and cache-hit paths fail this check.

The debugger enters at the actual command, steps, stops at the cross-function
thrower breakpoint with its catcher present in the backtrace, reads Wasm locals,
continues through catch/oracle validation, and performs normal quit with real
process retirement and terminal restoration through the existing Console helper.
No forced cleanup is accepted as a successful debugger result.

Pass `--arena`, a fresh `--out`, `--wasmtime`, and `--expected-cgroup`. Optional
`--runtime-tests` selects the existing `test/0017.runtime` Console source;
`--reference-only` builds and checks Wasmtime without starting UWVM. Compiler
and product identities, actual commands, object logs, and all console actions
are retained in the result directory. This Linux regression does not qualify
native Windows unwind execution.

`bootstrap_wasm_tools.py /absolute/arena/tools` installs the pinned official
[wasm-tools v1.262.0 release](https://github.com/bytecodealliance/wasm-tools/releases/tag/v1.262.0).
It requires both the release asset's SHA-256 metadata and the downloaded archive
to equal `655b8df67d54c3ef9b52fc237c4e94bbfbb5f5f43825245cb0940a7002a9ed65`,
records a binary/version receipt, and removes the archive. Wasmtime requires
explicit `exceptions=y` and `function-references=y` for this fixture.

`results.json` records the final latest-product qualification: Wasmtime plus
all six UWVM ordinary/debugger runs passed. Every ordinary run emitted 52
retained native definitions into eight objects, and every debugger run emitted
59 definitions into eight objects. The 26 Wasm definitions include the sixteen
exported marker functions. These Linux results do not qualify Windows execution.
`reference-feature-configuration-attempt.json` retains the initial Wasmtime
invocation that omitted its typed-function-reference opt-in; no UWVM ran in
that attempt. `results.json` records the corrected explicit feature settings.
