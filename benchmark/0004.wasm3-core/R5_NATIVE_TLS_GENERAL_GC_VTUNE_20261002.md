# R5 native TLS: separate hardware VTune recipe

This command-only producer binds the actual R5 plan
c6e08fa3595470094b6c97db55c3ddd20e45440862e014a4003be4c03ba63f35,
current source a97ff26b2da9dc6f227201bf7c36a73a371ee2885b710a188111f3d41472734a
and CLI4aba5943567bbd216eb8e00382a6fee6ef5765b6d02f056f02b310abe0879632.
The actual long cold receipt is 34/34 stages, 32 products, summary
9da5e9d032c0aa551888883af3e6403b6aa16196cb5f853d74f13f368f9d0580.
No current R5 VTune result is claimed. Old S6e source/results stay immutable.

The sole Linux keeper prepares the new recipe after plain and pure hardware
counting have retired, then uses its narrowly derived existing all-tree
VTune supervisor. There is no new launcher, sampler or guardian here.
Fresh admission must independently prove the current boot/init birth/UID,
exact64GiB cgroup/swap0/cpuset, current tool/DSOs and owned original PIDFDs.
Compiler/Windows/VM/profiler jobs must not overlap these samples. Temperature
is raw observation only; actual frequency/PMU/noise evidence stays distinct.

Prepare only, under the keeper's existing resource admission:

```text
python3 prepare_general_gc_R5_nativeTLS_vtune.py \
  --plan B/builds/current-R5-nativeTLS-general-gc-measurement-20261002-r1/plan.json \
  --out B/builds/R5-nativeTLS-general-gc-vtune-20261002-r1/recipe.json
```

Here B is the keeper's real /home/macromodel/Documents/uwvm3-implementation/
wasm3-resume-20260924. The generated argv arrays contain exact paths; B is
only a readable abbreviation in this document, never a runnable placeholder.
The recipe's execute_ready remains false: actual guardian derivation,
before-closure and hardware inventory are separate requirements.

The two2M cells are reference-array allocate and mutable-struct mutate.
Each runs once with HW Hotspots and once with uarch summary. The former must
report actual6M allocations, roots1/disabled0/reason0 and positive attempts,
collections/reclaimed. The latter has exact1024 initialization allocations,
attempts/collections/reclaimed0, roots1/disabled0/reason2 and is field/lookup
evidence only. Parse the actual guest log with the frozen current R5 semantic
function; retain actual exit evidence, timers, native ownership, disabled
cache, no body fallback and independent same-byte checksum proof. Profiler
exit0 or summary report0 cannot stand in for guest semantic correctness.

The actual historical installed help was read again: VTune2026.4.0 build632893,
Hotspots supports sampling-mode=hw, stack collection/stack-size and disabled
characterization; uarch supports pmu-collection-mode=summary. Hashes are
recorded in the recipe as historical evidence, not fresh installation proof.
The keeper runs actual version/collect/report help first and binds all actual
profiler helpers/DSOs. Intel's [Hotspots CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/run-basic-hotspots-analysis-command-line.html)
requires explicit hardware sampling because software is the default;
[uarch CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-1/run-general-exploration-analysis-command-line.html)
supports the chosen low-overhead summary mode. No software fallback is used.

Hotspots argv retains actual successful knobs: HW sampling, stacks1024,
characterization false and --perf-threads=none. Uarch remains summary.
The old cpu-mask=0 was reported unsupported for this target type. The new
recipe omits that ineffective option; actual guest tasksetP0 and owned TID
affinity supply execution evidence. The supervisor's previous cpu-mask
requirement needs an explicit one-line derived whitelist change, not a false
claim that the old guardian accepted these new commands.

Use the recipe's explicit controller_environment with the already-owned
spawn. It inherits no ambient configuration: HOME/TMPDIR are new private
directories, PATH/LANG/LC_ALL/USER/LOGNAME are fixed, LD_LIBRARY_PATH equals
the plan and RAYON_NUM_THREADS=1. Every inherited environment key/length/
value hash is retained; values of unrelated credentials are not printed.
The actual stopped controller/profiler bootstrap environ must match this
explicit environment before GO. The target's actual /proc/environ must
independently show capture absent, expected loader/RAYON, no LD_PRELOAD/
LD_AUDIT. Any profiler-injected variables are recorded separately; do not
assume cleaned controller inheritance proves the loaded guest environment.
Never restore CAPTURE merely to obtain JIT objects during timing. This
environment adaptation preserves existing own-child/PIDFD/EOF/ACK protocols;
it does not authorize a new executable or process-tree exemption.

Before and after all four collections, call the unchanged current driver's
closure(sampler,cold,host,plan,libraries,out,stage) with its actual pinned
dependencies, and additionally close the actual VTune ELF/helper/DSO pins,
supervisor source and command-file bytes. Do not substitute old S6e closure
or a changed source. Existing tree admission still checks profiler E-core,
guest P0, each TID UID/cgroup/affinity, argv/executable/birth, deadlines,
output bounds and original owned PIDFD retirement. A race/missing witness
is recorded as unknown or failure according to that approved guard.

For each result save unfiltered summary and function/events reports first.
Query actual supported columns before adding filters. Intel's
[grouping](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-1/group-by.html)
and [filter](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-1/filter.html)
syntax defines -group-by/-filter options. Some official query examples omit the
prefix, while the actual installed keeper commands succeeded with separate
'-group-by','?' and '-filter','?' arguments. The v3 producer uses the explicit
option form -group-by=?/-filter=? for deferred queries:

```text
vtune -report hotspots -r ACTUAL_RESULT -group-by=?
vtune -report hotspots -r ACTUAL_RESULT -filter=?
vtune -report hw-events -r ACTUAL_RESULT -group-by=?
vtune -report hw-events -r ACTUAL_RESULT -filter=?
```

Use each collection's own actual guest PID/TID and reconcile report namespace
identity with the guardian's host birth/executable observation. The verified
previous target spelling is cpuid=cpu_0, not numeric0. A supported target
function report uses process-id,thread-id,cpuid,core-type,function,module-path,
address plus filters process-id=ACTUAL_PID,thread-id=ACTUAL_TID,cpuid=cpu_0.
These are documentation templates only; no unknown PID/TID is prefilled.
Keep actual grouping/filter errors and unfiltered data. Uarch summary may
lack function attribution: keep Unknown rather than inventing JIT names.

Retain raw sampling counts, lost/skipped stack warnings, actual PMU events,
MUX/coverage and frequency distribution. Previous summary MUX.574/.584
was not100%-running pure counting; summary mode does not remove multiplexing.
Native helper sampled self time differs from inclusive call-stack time.
Anonymous ld.so.cache/outside-module PCs stay unknown without contemporaneous
actual loaded object/PC/generation evidence; old lambda numbers do not map
this new source.

Internal WASM execution excludes startup/JIT, but includes setup, fields,
allocation/collection and checksum. Parent wall/user/sys/RSS includes the
whole new process. Pure HW grouped counters count the whole guest single TID.
VTune performs four different profiled executions. No family replaces
another, and none is a pure collector ROI or industry ranking. The old
get_thread_state map-fallback percentage belonged to non-native TLS S6e;
native TLS current attribution must come from these new actual samples.

Source revision v2 binds the unchanged command-only producer to the approved
R5 measurement driver v4, SHA256 d96e7eb2795d020ca5bf29800500b98b947db901eb7bf262eff1130da7b863cb.
The producer-only executable diff is the fixed dependency SHA; selected
product/source/plan, guest argv, four HW collections, environment contract
and all commands are unchanged. The v1 archive remains immutable and binds
measurement v3. Actual unprofiled32 and pure-HW8 completed with v3; this
source revision does not relabel or rerun their raw data. Current VTune
collections still require the keeper-owned current supervisor and actual
results; no collection is claimed here.

Source revision v3 changes only the deferred-query argument to '-' + query and
adds pure Python checks requiring16 explicit option-form queries with zero
positional query arguments. The immutable v1/v2 archives remain available.
Actual keeper10 query commands used the separate prefixed options and are not
changed or relabeled. This source correction is not a failed actual query.

Current actual VTune collection inserted two Intel runtime-loader prefixes.
The original strict LD-matches contract therefore remains unqualified. Keep
the original actual37-key guest environment and strictfalse fields; successful
HW collection is development attribution evidence, not an environment-contract
pass. No original raw receipt, PID/TID, result label or source policy is altered.
