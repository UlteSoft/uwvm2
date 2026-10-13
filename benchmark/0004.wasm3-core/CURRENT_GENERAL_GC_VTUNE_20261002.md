# Current GeneralGC hardware VTune recipe

This is the third measurement family for the actual ordinary S6e product. It is a CLI recipe for the sole Linux keeper, not a new launcher or guard. The prepared plan must have schema `uwvm-current-s6e-general-gc-plan-v1`, source `sha256:6e05059af0b6c2a46ed4476d82920d91636dcda66dc6e59360e0560a4fe05131`, and product SHA `b5f39dfe70e08a308a0cc415dd3e904386dd069d24a0cf330c75f860c0026830`. It comes from the immutable [adapter](run_current_general_gc.py), SHA `0c6bba7e9826bcfafdd1e8da1a31f25be1d6a321d62311064b4a9110900915da`. No current VTune result is asserted here.

Use these two existing, byte-identical, officially validated and Wasmtime-cold-tested plan cells, each once for hardware Hotspots and once for hardware microarchitecture exploration:

| Plan cell | Intended attribution | Required dynamic classification |
| --- | --- | --- |
| `reference-array-allocate-2000000` | Allocator, root traversal, reference validation, collection and lookup mixed in one guest | 6,000,000 allocations, roots requested 1, disabled 0, reason 0, positive collections/reclaimed |
| `mutable-struct-mutate-2000000` | Field mutation, lookup and readback with initialization | 1,024 allocations, roots requested 1, disabled 0, attempts/collections/reclaimed 0, reason 2 (`phase_pending`); collector qualification remains false |

The original manifests' independent scalar checksum and trapping `_start` stay unchanged. Allocation counters are syntax-planned totals; collection/reclaimed counts must come from this profiled guest's real retirement log. Exit zero alone is insufficient. Apply the adapter's actual native-owner/cache-disabled/no-body-fallback, verbose guest/process times and managed-counter parser to the captured guest output. Preserve unknown or failed classification rather than treating mutation as a collector test.

Before launch, record the current boot, init birth/executable, exact cgroup and approved host-wrapper identity; check 64GiB, swap zero, approved cpuset and actual PMU/permissions. Recheck the prepared plan's actual source, ELF, runtime-origin, dependency, SDK, tools and fixture pins before and after. Preserve actual loader environment and `RAYON_NUM_THREADS=1`; clear neither errors nor provenance. Temperature is observed only. Keep frequency distributions, throttling and actual P0/SMT activity. Host noise is unknown when no independent observer exists.

Run serially after unprofiled and pure-counting retirement, with no compiler, other guest, profiler or Windows pipeline active. Use the already reviewed keeper-owned VTune process-tree admission, PIDFD cleanup and bounded deadline/output procedure inside the same scope. It explicitly admits the profiler's own descendants; do not insert a profiler child into either frozen counting or plain runner's closed roster. These commands do not authorize cgroup, capability, seccomp, sysctl or administrative changes. Preserve a failed hardware collection; do not fall back to software sampling.

The historically successful installed binary was `/opt/intel/oneapi/vtune/2026.4/bin64/vtune`, version 2026.4.0 build 632893. A fresh inventory records the actual ELF/SHA, loaded DSO closure, `-version`, `-help collect hotspots`, `-help collect uarch-exploration`, and report help. Historical success in an older scope does not qualify the current scope. Intel's [Hotspots CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/run-basic-hotspots-analysis-command-line.html) defaults to software sampling, so hardware is explicit; installed help determines supported knobs. Intel calls the second analysis [uarch-exploration](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/run-general-exploration-analysis-command-line.html).

The keeper obtains `GENERAL_PRODUCT`, `GENERAL_WASM`, `GENERAL_VTUNE` and fresh result paths from actual pinned files, not placeholder values. Verify that the following Bash array is exactly the selected plan command's argv; keep the real argv array in `commands.json`, not only a shell rendering. Do not replace the externally bound S6e product with a new shared-status or private-EH candidate.

```sh
GENERAL_GUEST_ARGV=(taskset -c 0 "$GENERAL_PRODUCT"
  -Rcc jit -Rcm full -Rllvm-full-policy pb-o3
  -Rllvm-call-stack unwind -Rllvm-exception-dispatch auto
  -Rllvm-cache-path disable -Rct 0 -WFE-gc
  --wasm-feature-enable-reference-types
  --wasm-feature-enable-function-references
  --log-verbose -Rclog err --run "$GENERAL_WASM")

taskset -c 16 "$GENERAL_VTUNE" -collect hotspots -cpu-mask=0 \
  -knob sampling-mode=hw -knob enable-stack-collection=true \
  -knob stack-size=1024 -knob enable-characterization-insights=false \
  -run-pass-thru=--perf-threads=none \
  -result-dir "$GENERAL_HOTSPOTS_RESULT" -- "${GENERAL_GUEST_ARGV[@]}"

taskset -c 16 "$GENERAL_VTUNE" -collect uarch-exploration -cpu-mask=0 \
  -knob pmu-collection-mode=summary \
  -result-dir "$GENERAL_UARCH_RESULT" -- "${GENERAL_GUEST_ARGV[@]}"
```

The installed-success Hotspots recipe already used hardware stacks, characterization disabled and `--perf-threads=none` for driverless per-process sampling. The new [CPU-mask option](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/cpu-mask.html) selects logical CPU number 0; it is not a bit-mask interpretation of zero. The guest remains explicitly pinned to P0 and the profiler/controller to E16. The fresh actual collector log must confirm hardware collection and actual target affiliation.

Create four unique result directories beneath a new `general-gc-current-S6e-vtune-20261002-r1` evidence directory, one per cell/collector. Capture real guest host PID/birth/UID/cgroup/executable/argv and every TID/affinity during execution, with profiler identity separately labelled. The report's PID/TID may use a namespace different from the host observer: reconcile the actual mapping rather than substituting the profiler PID or assuming TID equals PID. Report success is distinct from collection success; retain both raw return codes and logs.

After each collection, save unfiltered summary and reports first. Intel's [report](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/report.html), [grouping](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/group-by.html) and [filter](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/filter.html) references describe result-dependent columns. Query the actual result's supported groupings/filters before using them:

```sh
"$GENERAL_VTUNE" -report summary -r "$GENERAL_RESULT" \
  -report-output "$GENERAL_REPORTS/summary.txt"
"$GENERAL_VTUNE" -report hotspots -r "$GENERAL_RESULT" "group-by=?"
"$GENERAL_VTUNE" -report hotspots -r "$GENERAL_RESULT" "filter=?"
"$GENERAL_VTUNE" -report hw-events -r "$GENERAL_RESULT" "group-by=?"
"$GENERAL_VTUNE" -report hw-events -r "$GENERAL_RESULT" "filter=?"
"$GENERAL_VTUNE" -report callstacks -r "$GENERAL_HOTSPOTS_RESULT" "filter=?"
```

When those exact columns are supported, export unfiltered process/TID/CPU reports and then the target-only reports. `GENERAL_REPORT_PID` and `GENERAL_REPORT_TID` are actual observed report identities; record their host mapping. Examples below are conditional on installed result help, not permission to silently remove an unsupported filter:

```sh
"$GENERAL_VTUNE" -report hotspots -r "$GENERAL_HOTSPOTS_RESULT" \
  -group-by process-id,thread-id,cpuid,function -format csv \
  -report-output "$GENERAL_REPORTS/hotspots-all.csv"
"$GENERAL_VTUNE" -report hotspots -r "$GENERAL_HOTSPOTS_RESULT" \
  -filter "process-id=$GENERAL_REPORT_PID" \
  -filter "thread-id=$GENERAL_REPORT_TID" -filter cpuid=0 \
  -group-by function -format csv \
  -report-output "$GENERAL_REPORTS/hotspots-target.csv"
"$GENERAL_VTUNE" -report callstacks -r "$GENERAL_HOTSPOTS_RESULT" \
  -filter "process-id=$GENERAL_REPORT_PID" \
  -filter "thread-id=$GENERAL_REPORT_TID" -filter cpuid=0 \
  -group-by function-callstack -format csv \
  -report-output "$GENERAL_REPORTS/callstacks-target.csv"
"$GENERAL_VTUNE" -report hw-events -r "$GENERAL_UARCH_RESULT" \
  -group-by process-id,thread-id,cpuid -format csv \
  -report-output "$GENERAL_REPORTS/hw-events-all.csv"
"$GENERAL_VTUNE" -report hw-events -r "$GENERAL_UARCH_RESULT" \
  -filter "process-id=$GENERAL_REPORT_PID" \
  -filter "thread-id=$GENERAL_REPORT_TID" -filter cpuid=0 \
  -group-by function -format csv \
  -report-output "$GENERAL_REPORTS/hw-events-target.csv"
```

Each result has its own PID/TID; never reuse Hotspots identities in the separate uarch guest. An unsupported function grouping in summary-mode counting is an actual limitation: keep process/thread totals and its error receipt, without inventing function attribution. Preserve unfiltered data so filtering cannot hide extra processes/CPUs. Save actual collector type, event names/PMU instances, event coverage/MUX, sample counts, lost samples, stack-size/skipped-frame warnings, raw result database and CSV/log hashes. `pmu-collection-mode=summary` does not remove multiplexing: the earlier result's MUX about 0.561 remained a limitation.

Hotspots self time and inclusive call-stack time are separate. Anonymous JIT mappings such as `[ld.so.cache]` remain unknown until actual loaded PCs, finalized object symbols and profiler mapping bytes agree; old lambda numbering is not an identity for this product. These first samples locate native allocation, authentication, lookup, locks, collection and root traversal costs; they do not label all loop time as GC collection latency. The native component's separately measured collection region is also not this whole VM's collector ROI.

Profiler elapsed time, actual guest verbose time and whole-process costs are labelled as profiled observations; neither replaces unprofiled wait4 wall/user/system/RSS nor the separate non-sampling grouped counters' exact enabled/running interval. No industrial ranking follows from these four attribution samples. If the actual 2M guest produces insufficient target samples or MUX warnings, retain the entire result and first diagnose its coverage. Any larger body requires a new generator revision plus official/Wasmtime checksum and byte-roundtrip cold evidence; the frozen current generator and adapter accept at most 2M, so silently substituting 4M is forbidden.

Recipe v2 changes only the adapter provenance pin after its exact actual response-file prepare fix; all four collection argv templates and evidence boundaries are unchanged. The v1 recipe/source snapshot is retained.
