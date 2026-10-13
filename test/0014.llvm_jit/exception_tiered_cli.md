# Ordinary tiered numeric exception checks

Run only in the SSH Linux cgroup, with the ordinary UWVM executable.

```sh
python3 test/0014.llvm_jit/run_exception_tiered_cli.py \
  --uwvm "$UWVM_BIN" --wasm-tools /dev/shm/uwvm-tools/wasm-tools-1.259.0 \
  --wasmtime /work/artifacts/wasmtime-v48.0.2-x86_64-linux/wasmtime \
  --phase t1 --mode lazy --mode lazy+verification \
  --trace instruction --trace unwind --profile no-t0 --profile no-t0-no-t2 \
  --out /dev/shm/uwvm-builds/exception-tiered-t1
```

For actual interpreter-to-native loop continuation, use --phase osr and omit the
T0-disabling profiles. Normal warm cases require tiered_osr_ready > 0.
The first-activation escape and uncaught cases instead require tiered-osr-enter:
their native activation exits exceptionally and never increments the normal
return counter. Both stack policies require the exact original exception stack,
with retired tail activations absent.

For real Tier 2, use --phase t2 --trace instruction --trace unwind and omit profile overrides.
This enables two compiler workers by default. The fixture heats a short T0
driver, then throws through the same published native function target.
tiered-full-enter compares the dispatched raw address with the acquire-published
full-module entry. Compilation/publication by itself does not qualify execution.
T2 includes caught and uncaught direct/indirect/tail cases. Uncaught runs require
exact original frames (the tail caller must be absent), no truncation, and actual
T2 entry evidence before the fatal. A cold publication lock prevents late T1
callbacks from restoring old targets. The unwind policy keeps T1 demand inline;
only T2 uses the background worker. ROS has no tiered backend.

The frozen r216 ordinary O3 executable passed 32 actual T2 executions: eight
caught/uncaught direct/indirect/tail fixtures, both lazy validation modes and
both trace policies. Every run required actual full-entry dispatch and a
three-function full-module publication; uncaught stacks were exactly [0,1,2],
or [0,2] after a tail call. This is the first qualification of the new unwind T2
path; the earlier r214 results cover instruction T2 only.

The same executable passed a bounded T1 regression of six cases (24 executions)
and five OSR cases (20 executions), again with both modes and policies. T1 ran
with T0 and T2 disabled. OSR checks included a normally returning native loop,
first-activation direct/indirect exception escape, and an uncaught indirect tail
call. All 76 executions passed the Wasmtime semantic comparison. The first T1
and OSR attempts stopped on ENOSPC while copying the runner, before any test;
their directories remain in the archive separately from the successful reruns.

Source ID: 1d17c4d13e68ec1d6d0425d9b546b066dbdfc0f5c5afb4da862d678c63576dc2.
Executable SHA256: a539936c5d9891e122ee7a773daed5b1bc794a540a9e9abeceb554d8ce113030.
The archive build/wasm3-evidence/tiered-numeric-eh-r216.tar.gz contains 273
individually verified files; SHA256 is
dd52e629afb9d75bdd58a844c16a88cbbef44fe3df79c680bab7a7bb9a65018a.

Each output directory preserves runner sources, exact commands, Wasm fixtures,
compiler logs, actual tier evidence, and executable hashes. O1 is functional
qualification only. This logging-enabled runner does not measure performance.
The r216 checks used O3 but remain functional qualification, not timing results.

The separate tiered_entry_sampling.cc tests the real production helper against
the formerly starving fn2 -> (fn1 -> fn0)* pattern, regular call periods 1..1024,
strides 4/8/16 and uint32 wrapping. O3 assembly probes expose the sampling code:
one existing state load/write, add, shift and bit tests, without calls or locks.


`tiered_entry_publication.cc` forces the formerly broken late-T1 schedule,
checks both serialized writer orders, then stresses 80,000 T1 publications with
concurrent promotion. It uses the real production publication scope. This is
separate from actual CFI registration/unregistration and VM execution tests.
