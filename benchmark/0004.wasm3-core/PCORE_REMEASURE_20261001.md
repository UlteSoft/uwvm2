# Current-source P-core GC and exception remeasurement

This is a measurement plan. No new timing result is claimed. The full Core 3,
thread, debugger, platform and collector acceptance tasks remain open.

## Evidence that can actually be used

| Evidence | What it establishes | What it does not establish |
| --- | --- | --- |
| [2026-09-28 formal report](RESULTS_20260928_CURRENT_CANDIDATE.md) | The old c7f9/43a37 products took 117.991/116.906 ns per ring step and retained about 2 GiB at 16M allocations. The same-bytecode Wasmtime 49.0.1 copying baseline was 11.161 ns/step. | Current sealed compact throughput, current reclamation, or a thermally invariant ranking. The old GC products did not collect. |
| [2026-09-28 EH report](EXCEPTION_PERFORMANCE_20260928.md) | Actual 500K cross-function catches and separate 200M plain/no-throw call cases, including both diagnostic policies. | Current pending-numeric throughput. Wasmtime's original throwing sample was below 100 ms. |
| [Current design record](../../documents/runtime/gc-and-exception-fast-path-design.md) | Source and functional qualification for progressively newer allocation/EH candidates, kept separate from timing. | Complete current release qualification or an industry-best collector. |
| `sealed-r3-original-ring-long-P-actual-small-raw-r1/window/entry/raw.jsonl` | A preserved incomplete older long-ring attempt contains 26 raw executions: four warmups and 22 timed rows of the instruction subset. | A closed nine-pair result. CPU/package readings during these samples reached roughly 97–100 °C, above the recorded 90 °C quality threshold. These rows must not be upgraded to an accepted speedup. |

Wasmtime **49.0.1**, rather than the lost temporary 48.0.2 executable, is the
already measured and persistent comparator. Its ELF SHA-256 is
`c36ef7e6548600b09bcb34cb3ee4c138f3e7b01893347ec7f48fdb8e6050aa94`.
Wasmtime copying is the primary real-GC comparison. DRC is a different collector
cell; null is a non-reclaiming lower bound. Other VMs must pass the actual long
fixture before being timed: the old WasmEdge GC JIT crashed, while Wasmer/WAVM
rejected that GC input. Old exclusions are not inherited by a new version.

## Minimal first window

Wait for both current O3 products and the fused new-syntax regressions to pass.
The sole Linux keeper then closes every build/test/QEMU/Windows workload before
opening the existing P-core quiet window. Use CPU 0 for the guest and CPU 16 for
the controller. Keep exactly 64 GiB, swap zero and `0,2,4,6,16-31` admission.
Do not change the cgroup guard or host cooling/frequency settings to obtain a pass.

Both actual RT and CLI currently request these six opt-in experiments:
`COMPACT_NUMERIC`, `MANAGED_NUMERIC_PAGE`, `SEALED_COMPACT_CURSOR`,
`SEALED_LOCAL_TABLE`, `PENDING_NUMERIC_FUSED_CATCH`, `PACKED_NUMERIC_ARRAYS`.
All are one. The combine/heavy/extra-heavy and soft/heavy delay flags also
match. This is a specifically enabled candidate; it is not a default-off build
performance qualification. A later default build needs its own measurement.

1. Run the original 2M/16M, 1024-root allocation ring once per product and
   instruction/unwind policy with cold runtime logging. Check the original
   guest self-check, 16M attempted/committed allocations, 3906 collections,
   15,997,951 reclaimed objects and zero unsettled slots against real receipts.
   A changed count must be investigated, not accepted because exit status is zero.
2. Run the original numeric EH fixture once per policy, confirming actual
   owning-source pending activation. Separately force `native-unwind` dispatch
   for the same original module. Dispatch and diagnostic stack policy are
   independent axes; a native fallback cannot establish numeric-path performance.
   The no-throw cases are necessary controls for the user's frequent-call requirement.
3. Under the unchanged keeper guard, collect nine rotated/reversed profile
   pairs with the same validated Wasm: ordinary/ROS full JIT instruction and
   unwind, plus Wasmtime copying for GC and native EH for exceptions. Save every
   argv, ELF/runtime/source/fixture/tool hash, log, raw wall/RSS/CPU sample and
   before/during/after temperature/frequency/cgroup counters. Keep instrumentation
   out of timed loops; runtime receipt logging belongs to the separate functional run.
4. Reject OOM, throttling, missing frequency or process ownership, and the
   existing 75 °C start / 90 °C peak / 5 °C pair-start / 10% frequency limits.
   Preserve rejected rows. Any timed process sample below 100 ms or a negative slope is
   inconclusive. Do not replace a failed gate with a more lenient threshold.
5. If the 16M ring or 500K-catch module is too short, retain that original
   functional result and prepare a larger sibling with the same graph and
   independent checksum/catch oracle. The already prepared 128M/512M ring
   siblings provide an existing long-GC route. For EH, 32M steps give exactly
   2M catches; all participants must use the identical newly validated binary.
   Do not subtract the 200M no-throw case from the differently sized throwing case.

The first measure is the current candidate versus the accepted Wasmtime peer.
It is not an optimization A/B against the old monotonic store. Running that
historical store on 512M allocations would also violate the bounded-memory goal.
Keep the original 5M/50M memory64 random-store slope as the smallest memory
regression control; use the matched generated object for assembly inspection.
Packed/mutable/reference GC paths, cross-store cycles, the 1M→10M RSS release
gate, interpreter/lazy/tiered coverage and managed-language analogues remain
separate acceptance work. The interpreter path must not receive a collection
PASS from full-JIT receipts.

## Binding current build receipts

The historical `run.py` requires build JSON keys `runtime_command` and
`cli_command`; current fresh builds store the actual argv in
`runtime.command.json` and `cli.command.json` instead. Its GC summary also
contains historical hardcoded `absent-release-blocker` labels. Do not silently
feed it a fabricated old-shaped build receipt or treat those labels as current
collector proof.

[prepare_current_pcore_plan.py](prepare_current_pcore_plan.py) reads the actual
new receipt and both command sidecars. It checks fresh O3 runtime linkage,
matching experiment/combine/delay settings, embedded source IDs, actual ELF and
runtime hashes, and the original seven fixture hashes. It launches no compiler,
WAT tool, VM, timer or SSH session. `execute_ready` stays false: after-build
source fingerprints, semantic receipts and keeper process/thermal guard remain
independent prerequisites.

Run the plan producer inside the established Linux cgroup, using actual paths:

```sh
python3 benchmark/0004.wasm3-core/prepare_current_pcore_plan.py \
  --ordinary-build "$CORE3_ORDINARY_BUILD" --ros-build "$CORE3_ROS_BUILD" \
  --ordinary-source-id "$CORE3_ORDINARY_ID" --ros-source-id "$CORE3_ROS_ID" \
  --gc-fixture-dir "$CORE3_ORIGINAL_GC_FIXTURES" \
  --eh-fixture-dir "$CORE3_ORIGINAL_EH_FIXTURES" \
  --memory-fixture-dir "$CORE3_ORIGINAL_MEMORY64_FIXTURES" \
  --wasmtime /work/wasm3-resume-20260924/tools/wasmtime49-unpacked/wasmtime-v49.0.1-x86_64-linux/wasmtime \
  --include-native-eh --out "$CORE3_OUT/current-pcore-plan.json"
```

All variables must resolve to actual frozen inputs before invoking the command.
The r7 builds subsequently completed and their original 2M/16M ring and numeric
EH cold receipts passed on CPU 16. Those receipts are functional evidence and
contain `timed=false`. New legal cross-module subtyping cases then required
further validation/linking changes; the r8 products must get their own green
functional matrix, cold receipts, source IDs and new plan before measurement.

Add `--long-gc-fixture-dir "$CORE3_LONG_GC_FIXTURES"` explicitly when binding
the archived 128M/512M, 1024-root siblings. The original 2M/16M commands remain
in the plan, while `timing.gc_fixture_pair` selects the longer measurement pair.
The archived Wasm SHA-256 values are
`5eb8cb2ab852164f339636ac21fb7b0284b3d6dee0236fe9cd11e2f04e27fd76`
and `4344aec55a95e51d86433ab6d57305528866f18583947a1def97e81b69d26d79`;
their independent checksums are 2259414293 and 1687964949. Official assembly
and validation passed for those bytes in the archived preparation packet.
This does not establish execution under r8. The keeper must verify the exact
new-product checksum, collection/reclamation and memory receipts before using
them, and every participant must consume the same bytes.

The plan producer does not supply a measurement guard. A new guard must cover
every path after child creation with owned PIDFD kill/reap cleanup, including
bootstrap, admission, monitor startup and interruption failures. Its acceptance
must require successful before/after source/ELF/object/command-sidecar/fixture
and plan closure, process retirement, both valid low/high samples and positive
slopes. A closure exception must leave acceptance false. Preserve raw failures
and identify this new guard independently of the archived qualified host guard.

## Next optimization after the measurement

[Wasmtime's versioned copying lowering](https://github.com/bytecodealliance/wasmtime/blob/v49.0.1/crates/cranelift/src/func_environ/gc/copying.rs)
uses an inline bounded cursor with cold refill. It requires relocation-capable
roots; its barrier choices cannot be copied into a runtime with different
ownership. [HotSpot TLAB allocation](https://github.com/openjdk/jdk/blob/815ff4dc327fe17f2433c7d115a5a503af10f3c4/src/hotspot/share/gc/shared/threadLocalAllocBuffer.inline.hpp)
similarly separates locally owned allocation space from refill.
[Immix](https://www.steveblackburn.org/pubs/papers/immix-pldi-2008.pdf) provides
a different block/line reclamation design; it does not establish that the
current nonmoving numeric-cell store has equivalent fragmentation or pauses.

The current sealed emitters repeat entry authorization loads for allocation,
table set/get and cast/read. Inspect the actual optimized loop before attributing
cost to those loads. A possible next compiler improvement is a short dominating
authorization witness within one straight-line success region. Invalidate it
before a poll/refill, native/guest call, grow, EH edge, debug boundary, merge or
backedge. Keep token/type/range/live-bit and table index checks. The collector
must still wait for real participants to park before reclaiming storage.
This is a design candidate, not a source change or a measured improvement.

An adjacent same-index `table.set`→`table.get` can also be considered for value
forwarding only with the genuine exclusive entry and no intervening callback,
poll or table mutation. Preserve the table write as a real liveness root,
allocation/OOM/collection ordering, null/type traps and foreign fallback. Do
not fold the entire ring into a scalar loop or suppress the requested
allocations to improve the score. Correctness stress, real IR/assembly and
source-bound paired measurements are required before adopting either candidate.


## Independent long workload development diagnostic

`run_current_pcore_diagnostic.py` is a separate, deliberately unqualified
runner. It leaves the earlier thermal watchdog unchanged. It consumes the
current plan's pinned 128M/512M pair and uses product `diagnostic_argv` to
capture actual retirement counters and `Total WASM execution time` as well as
whole-process wall time, CPU time and maximum RSS. The guest's pinned `_start`
traps on a wrong checksum; exit zero is that checksum proof, not a printed
checksum measurement. Wasmtime has no product GC-counter or guest-time receipt.

The exact 64GiB/swap-zero cgroup, P0 guest/E16 controller affinity, process
ownership, PIDFD kill/reap, memory headroom, output limit and 90-second
sample deadline remain hard constraints. This guest-only diagnostic explicitly
requires 1GiB free disk, with each guest log bounded to 4MiB; it performs no
compilation or download. The qualified watchdog's 16GiB disk floor is unchanged.
Under the latest user criteria, temperature is an observation only: it does
not reject a sample or pair. Frequency, throttling, minimum duration and other
pair quality remain visible in raw evidence; all complete guests are retained. `formal_acceptance` is always false. Preserve
external host-load observation; this container runner cannot establish that
P0 and its SMT sibling were quiet. Its raw slopes locate a development problem
and must not be used to rank engines or qualify a release.

After fresh source-bound builds, semantic receipts for both long fixtures and
an idle build cgroup, the sole Linux keeper can execute one ten-guest pair:

```sh
taskset -c 16 python3 benchmark/0004.wasm3-core/run_current_pcore_diagnostic.py \
  --plan "$CORE3_OUT/current-pcore-plan.json" \
  --out "$CORE3_OUT/long-development-diagnostic" --pairs 1 --execute
```

The r8 short pilot preserved 90 raw samples: 80 exited via SIGKILL and ten
exited zero (eight Wasmtime 2M runs and two product instruction 2M runs). No
16M run completed and all samples were shorter than 100ms. None qualified.
The later r9 cold checks verified both long modules in both repositories and
both call-stack policies: 128M performed 31,250 collections and reclaimed
127,998,975 allocations; 512M performed 125,000 collections and reclaimed
511,998,975. Those cold counts are functional evidence, not current P0 timing.
Inspect generated loop assembly and the diagnostic counters before attributing
a delay to the sealed table/cast path or enabling additional experiments.
