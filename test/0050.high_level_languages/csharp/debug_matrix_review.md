# Read-only debugger matrix review

The recorded C# default matrix run reached `started` before the SSH Linux host
rebooted. There is no retained failing C# debugger assertion or console trace.
Its debugger qualification is therefore incomplete, rather than a reproduced
runtime failure. Historical native/Wasmtime and small entry-regression results
remain separate evidence.

`run_matrix.py:argv_for` preserves the same guest arguments and feature flags
between ordinary and debugger runs. It selects FULL/JIT for `uwvm2`, AOT for
stable/ROS, disables the LLVM disk cache, and adds `-Rdbg` for debugging. Ordinary
compilation uses `-Rct 0`; debugging defaults to `-Rct 7` unless a case explicitly
sets `debug_compile_threads`.

The initial implementation used a 600-second CLI-prompt deadline and a generic
240-second deadline for all commands. The current harness gives `startw` and
restart `run` a separate case-configurable `cold_command_timeout` (default 600),
records each action's elapsed time, and allows `initial_prompt_timeout` and
`debug_command_timeout` overrides. Compilation can occur after a command is
submitted, so the initial prompt deadline does not increase command deadlines.
The script changes the target's CPU affinity
after `startw` and after restart `run` return. Both commands therefore complete
their cold compilation using the admitted E cores. The restart deliberately
repeats entry compilation and checks a real offset-zero breakpoint.

Keep the existing seven-helper/E-core compilation plan for rebuilt C#
qualification. There is no C#-specific failure, memory-pressure measurement, or
concurrency fault supporting a change to serial compilation. Allow separate,
case-configurable cold-start/restart command timeouts of at least 600 seconds
(1200 if measured cold compilation requires it), and an initial-prompt timeout
of 1200 seconds. Preserve timings and compilation logs per action before
choosing smaller limits. A serial `debug_compile_threads: 0` run would only be a
diagnostic comparison after a reproducible compiler-concurrency failure. The
interruption does not establish that a debugger timeout caused the container
stop or host reboot. Do not start another matrix until ownership and retirement
of any previous matrix are established.

The rebuilt main product's first C# debugger cold start was inspected once with
GDB after verifying its exact command, product/module hashes and cgroup. It was
in `llvm::WriteBitcodeToFile`, called by the optimized-module serialization
before parallel object emission. A second debugger thread waited in the CLI
controller's condition variable. The short attach detached normally. Forty
seconds later the same process had nine threads, including seven runnable
compiler helpers, proving that it advanced beyond serialization. Its observed
RSS rose from about 17 GiB to 53.8 GiB. These are cold compilation observations,
not guest execution timings or a completed debugger result. Raw evidence is in
`recovery/cold-debug-pid42326/report.json`.

The remaining sequence meaningfully checks Wasm entry, stop IDs, backtrace,
Wasm locals, single stepping, breakpoints, restart, exact semantic output, normal
exit, CLI retirement, and terminal restoration. These checks do not establish
C# source breakpoints or managed locals: the retained DWARF describes Mono's
native C implementation, while the portable PDB needs a managed debugger bridge.

The timed fixture emits ticks and frequency together on one line. The benchmark
parser originally expected separate tick/frequency lines; its current ticks
branch reads the actual complete C# line and checks the iteration count and
positive reported frequency. `perf-timed-cases.json` preserves that real output
and full semantic checksum checks. No timer observations in the six historical
correctness references are a controlled engine-performance comparison.

`production_delta_review.json` records a read-only comparison of the complete
entry resolver, the new main start/command block, and exact-count table bounds in
the four WASI headers. Those task-specific blocks and the four unit-test files
match in all three repositories. No unrelated dirty changes were copied.
