# Independent entry adaptation review

Reviewed on 2026-10-17. Scope: the new default-entry resolver and main-module
start/command sequencing blocks in `src/uwvm2/uwvm/run/run.h` across `uwvm2`,
`uwvm2-ros`, and `uwvm2-stable`. Other pre-existing ROS changes were outside
this review. This was a static review; it does not claim new execution evidence.

No blocking correctness finding was established in the changed blocks. The
main-module graph block is textually identical in all three repositories.

The resolver now prefers a valid `() -> ()` `_start` or `main` command and keeps
the validated start section as a fallback. Imported command candidates still
have to resolve through the bounded alias chain to Wasm. Start-section
dispatch retains its separate `module_start` authorization for a validated
void host import; ordinary commands do not inherit that authorization.

The initializer uses a fresh, empty `RunConfig`, so an explicit `-Wstart`
command's scalar ABI buffers do not leak into `() -> ()` initialization.
The command's original buffer pointers remain owned by the caller's
`runtime_entry_invocation`. The runtime creates and retires separate entry
RAII scopes while retaining the same initialized modules, globals, memories,
tables, compiled publication, and outer CLI GC scope. No reset was added
between initialization and command dispatch.

The raw import-inclusive index comparison runs a pure start module, or a
command selecting that same index, once. It intentionally compares module
function indices, not resolved provider identities: two different imported
slots pointing at one provider remain two distinct selected entries. The
existing import bridge resolves each slot's provider/module context.

For managed full LLVM execution, accepted `proc_exit` sets the registered
worker's persistent `guest_exit_` before entry RAII returns. The new stopping
check observes that state and suppresses the command. A stop arriving after
that check is still rejected by the next runtime entry's admission/owned-worker
refusal path. The actual worker's outer execution lease pins its generation
across the gap. Ordinary unregistered WASI `proc_exit` terminates the process;
it cannot return to dispatch the command. Traps and uncaught guest exceptions
also retain the existing terminating entry behavior.

The existing seven-case `assemblyscript/start_contract.py` covers distinct
start/command, pure start, same index, explicit selection of either function,
and preloaded starts. Recommended additional execution regressions:

1. Start writes a mutable global, then an explicit `-Wstart` function with
   nonempty i32/i64 parameters checks both that global and its arguments before
   printing success. This checks the original command ABI survives initialization.
2. Main start resolves through a preloaded Wasm import alias; test both a
   distinct exported command and `_start` selecting the same imported index.
3. Main start invokes `proc_exit(17)` and the command prints a forbidden marker.
   Require no marker in ordinary and managed debugger runs, preserve the exit
   code, and verify managed shutdown actually joins the worker.
4. Stop a managed guest while its initializer is parked and immediately after
   it returns; require no command-side guest/host effect and no leaked worker.

Execution follow-up: `run_entry_edges.py` implemented items 1–3 and was run in
the original Linux 64 GiB cgroup. Each candidate repository passed all five
UWVM ordinary/debugger cases; Wasmtime passed all four independent reference
cases. Three pre-patch binaries each reproduced the skipped initializer for
typed explicit entry and the skipped distinct command after an import-alias
start. Same-index and both proc-exit cases passed before and after. Full
structured results and debugger actions are preserved in
`entry_edges_results.json`. Item 4 remains an additional suggested concurrency
regression, not an execution claim from this review.
