# Focused debugger controller qualification

Run `run_controller.py --out ABSOLUTE_NEW_DIRECTORY` only on SSH Linux inside the
`uwvm3-implementation` cgroup, limited to 64 GiB and CPUs `0,2,4,6,16-31`.
The runner checks that environment before compiling and retains commands, logs,
source hashes and exact changed source files.
For the ROS bundled LLVM build, also pass `--llvm-build-include` with its
matching generated `include` directory. The runner records the bundled LLVM
source manifest and generated configuration-header hashes.

Both repositories are tested independently. Final r221-r4 qualification includes:

- O3, ASan/UBSan with leak detection, and `-fno-exceptions`: each has 715 controller
  assertions and 16 rounds with two real native participants. Prepared entry blocks
  admission; exact breakpoints park the domain; a chosen participant steps to its
  next real callback offset; explicit pause, continue, deletion, bounded capacity,
  timeout cancellation, guest completion and console parsing are exercised.
- Each profile also passes 3828 new protocol checks: every split/truncation point
  for four new operations, all incorrect payload lengths, missing grants, missing
  debug capability, replay, incorrect host completion and asynchronous state changes.
- Each profile reruns the existing 9062 session checks without removing cases.
- `__SINGLE_THREAD__` textual inclusion compiles and runs without a controller.
  The actual four control module units and the console command partition precompile
  in both native and single-thread configurations. Scoped dependency audits cover
  all new debugger and control header/module pairs.

These tests use a deliberately named test-only diagnostic provider. They verify
cold callback ownership and terminal-control escaping, not a native runtime
backtrace. Actual VM safe points, instruction mappings, CLI mode errors, guest I/O
isolation and native unwinding require the separate runtime/CLI fixtures. The
runtime-dependent controller/console BMI graph is statically audited here; this
runner does not claim to rebuild that complete named-module graph.
The controller unit fixture supplies a 256-offset safe-point bitmap and aborting
LLVM disassembler symbols so accidental native decoding fails loudly. Its
replacement callback simulates an unsupported prepare result. Real LLVM target
safe-point maps, disassembly, and function replacement are qualified against the
source-bound VM by the DAP and hot-replacement product tests.

The R30 private WASIp1 preparation check uses two genuine FastIO native workers
before engine preparation. Both visit every import-visible module and validate
the actual scoped environment/memory selection, an explicit null binding,
nested switching and LIFO restoration. Where two modules share one environment
but have different memories, the workers hold these contexts concurrently.
Both workers join before private environment/store destruction; the success
result reports `verified_wasip1_dispatch_workers`,
`verified_wasip1_dispatch_module_visits` and `wasip1_dispatch_tls_restored`.
`maximum_private_wasip1_workers` defaults to 2; a smaller bound refuses before
native thread creation. Unknown custom memory resolvers are refused without
invocation rather than replaced silently. These are private preparation checks;
complete world publication and restored guest startup remain unfinished. A Wasm
checkpoint still needs WASIp1 capture at the same cooperative stop to include
FD and environment state. See `wasip1_private_dispatch_r30_test_report.md` for
the four OS matrix, immutable source qualification and bounded recovery paths.
