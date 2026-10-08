# Separate r10/r11 hardware sampling and GC regions

This is a read-only plan. It adds no collector execution, profiler wrapper,
instrumentation, native build, production change or candidate qualification.
The approved four-build counter wrapper `22c21d09…`, checker `ae293454…`,
host runner `673d961b…`, counter protocol `266a56b3…` and component `25f9c710…`
remain unchanged. Actual B products and component executables are still inputs
to be supplied by the sole Linux keeper after cold qualification.

## First small hardware profile

After the unprofiled and pure-counter families, select the same preserved r10
A and strictly two-GC-header B builds from the [A/B plan](R11_GC_AB_PLAN_20261002.md).
Use the same immutable GC512M fixture, exact 1024 roots, checksum, allocation,
125000 collection and reclaim counts. Begin with one A/B hardware
Microarchitecture Exploration pair per repository: four whole-guest profiled
runs, sequentially, in fresh result directories. This is attribution evidence,
not the two-pair elapsed/count effect estimate. Repeat or expand only when its
actual sample/running quality and observed change justify it.

Use the keeper's separately reviewed existing VTune owned-process-tree
launcher in the same actual 64GiB/swap-zero scope. Its descendants and worker
threads need their own explicit birth/UID/cgroup/CPU admission; the pure-counter
wrapper's two-child roster must not be extended by inserting VTune. The guest
stays one TID on P0; controller/profiler use E16. Compilation, Windows, QEMU,
another VM and another profile must fully retire first. Temperature is observed
only; frequency distribution, throttle counters and independent host P0/SMT
activity are retained. No global permissions, software fallback or foreign
signals are requested.

Bind actual VTune ELF/version and runtime library closure, current boot/PMU
identity, source/compiler/runtime/ELF/cold receipts and before/after canonical
source diffs. The profiler had actually reported 2026.4.0 build632893; a reboot
does not substitute that historical version or permission receipt for a
current inventory. Installed `-help collect uarch-exploration`, `-help collect-with
runsa`, `-help report hw-events`, `-help report hotspots`, `-help time-filter`
and version outputs must precede any new configuration. This document does
not claim those new help calls or candidate profiles have run.

The argument cells for the separate admitted VTune launcher are:

```text
taskset -c 16 ACTUAL_VTUNE -collect uarch-exploration -knob pmu-collection-mode=summary -result-dir NEW_RESULT_DIRECTORY -- ACTUAL_PINNED_TASKSET_P0_GUEST_ARGV
```

Use the actually supported per-process driverless knobs, CPU mask and
`--perf-threads=none` control only after checking the current installed help and
the exact previous successful collector recipe. These are not unguarded shell
commands. Save actual resolved collector/event configuration and every
descendant/PMU attribution receipt; a tool returning zero is insufficient if
it counted another domain/process or silently selected all host CPUs.

Intel documents `uarch-exploration` and its summary/detailed and submetric
knobs; it also directs users to the installed help for current settings.
Detailed mode adds code context, while summary is useful for a first aggregate
comparison. [Official Microarchitecture Exploration CLI](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/run-general-exploration-analysis-command-line.html)

The old r9 summary profiles already had MUX Reliability about0.56. Merely
choosing summary again does not solve that limitation. Report running/enabled
or MUX, actual event domains/precision, raw/scaled counts, event sample totals
and skipped/lost records. If the new summary remains poorly multiplexed, retain
it as an observation and choose a smaller explicitly supported event set in a
separate `runsa` hardware-sampling pass, or an explicitly declared repeated-run
configuration. Do not merge differently timed event passes into an exact
single-run IPC. Multiple-run analysis can launch the target repeatedly; every
launch therefore needs actual admission and its own semantics/source/frequency
receipt. [Official multiplexing and multiple-run guide](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/allow-multiple-runs-or-multiplex-events.html)

For code attribution, optionally follow with one detailed uarch or hardware
Hotspots pair on the repository showing the measured regression/gain. Current
actual ELF symbols, section/address ranges, native finalized objects, maps and
exact DSO identities must establish the collector function. Never transplant
r9 lambda ordinals to r11. Unregistered anonymous JIT samples stay unknown.
Function self samples, inclusive ancestor samples and whole-guest totals are
different quantities; do not sum inclusive and self records twice. Low sample
counts or unresolved inline frames block a function-level microarchitecture
conclusion. Hardware skid particularly limits attribution to individual
instructions/basic blocks. [Official event-skid limitations](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/hardware-event-skid.html)

## ROI meanings that must remain separate

| Region/evidence | Actual boundary | Interpretation |
| --- | --- | --- |
| Unprofiled internal Wasm time | CLI's real execution timer | Whole GC ring workload; allocation/table/cast/field/arithmetic and collections. |
| Pure grouped hardware counts | Actual perf enable ACK before stopped guest GO through guest retirement | Whole executable including startup/init/JIT; exact grouped cycles/instructions. |
| Whole-guest uarch profile | Actual collector analysis interval | Sampled/scaled event explanation; profile overhead and MUX apply. |
| Native component `collection_ns` | Clock immediately before/after the real collector API inside the already-held exclusive admission | Native API collection work only, including final drop; excludes admission acquisition/root snapshot preparation. |
| Native component `readback_ns` | Authenticated compact-reader calls, expected payload checks and checksum accumulation | Separate safe readback cost; cannot be counted as collection. |
| Report `time-filter` | Selected portion of the profiler's recorded Elapsed Time | View of already-recorded data; does not change PMU collection or erase its overhead. |

Intel's time-filter acts on reports and uses begin:end seconds in the result's
Elapsed Time clock. An external guest monotonic timestamp is not automatically
that clock; establish actual alignment markers and uncertainty before using a
reported execution envelope. No current row has an exact Wasm hardware-counter
ROI. [Official report time-filter](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/time-filter.html)

Do not divide GC512M elapsed time or whole-guest hardware counts by125000 and
call the result collector latency or cycles per collection. That division
includes all ring and VM work. Likewise native component `collection_ns`
cannot qualify VM automatic root enumeration, native/guest frame scanning,
parked-mutator pause time or exception ownership. The component has4096 ring
collections plus one final drop, and reclaims all its16M allocations; the VM
has125000 allocation-triggered collections with1025 objects unreclaimed at
the fixture end. Their work and lifetimes differ.

The current component source was read again: root snapshot construction is
outside all four clocks; genuine exclusive admission begins before
`collect_begin`; its lease ends before authenticated readback. The final
empty-root API call contributes to `collection_ns`, while owner/cohort/lease
destruction belongs to `teardown_ns`. No ITT call exists in the frozen source.
Its real compact proof, duplicate-root cell, checksums and exact reclaim
oracle must pass on both bound native executables before interpreting a timed
component. No native build or execution PASS has been produced here.

## Optional finer region experiment, after a separate source review

A minimal first attribution pass needs no production hook: profile the actual
native component executable, with its current object/symbol/disassembly and
all four timers retained. If O3 inlines the API so completely that collection
PCs cannot be separated from allocation/readback, record that limitation.
Do not convert the enclosing `run` function's samples into pure collection.

Only if finer evidence is needed, prepare a separate profiling component
variant with an identifiable noinline collector-call wrapper applied equally
to A and B. The wrapper receives the same live canonical cohort and bounded
snapshot, calls the unchanged actual API once, and returns the status/reclaim
count. It adds no production ABI or guest capability. Admission, snapshot
creation, status/accounting checks and compact readback keep their existing
boundaries. The variant has its own source/object/ELF/compiler receipt; its
extra call and code layout are overhead to quantify, not a timing result from
the original component. This is a proposal, not an implementation.

Another separately instrumented variant could create cached ITT domain/string
handles outside clocks and bracket a selected region with Task API calls.
Tasks describe same-thread logical work in a running collection; they are not
exact event gates. Use a task label to filter genuine samples only if the
installed hardware collector actually records that task/data association.
Compare annotated and unannotated variants for timing perturbation and retain
annotation-call/data-loss overhead. Existing task APIs are open-source ITT
components rather than a new production dependency. [Intel's current API
support statement](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/api-support.html),
[official ITT header](https://raw.githubusercontent.com/intel/ittapi/master/include/ittnotify.h).

Do not add125000 collector-level pause/resume operations to the VM and claim
the resulting profile preserves its uninstrumented performance. CLI control
requests and per-collection PMU transitions add latency and uncertain
boundaries. ITT pause/resume controls the process and spawned processes, not
just a private GC timer. The2026 start-paused reference labels its modified
collect actions as user-mode analyses; collect-with lists that option, so
current hardware acceptance and actual pause/resume effects need a separate
verified receipt before any claimed hardware ROI. Hardware-only policy stays
in force; do not fall back to software tracing because a pause path is
unsupported. [Official start-paused reference](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/start-paused.html),
[official collect-with options](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/collect-with.html).

A profiled region also needs the collector's actual pause/loss records. Intel
documents that hardware stack collection can temporarily stop target
scheduling to spill buffered data; elapsed and sampled CPU time then differ.
No timer or report filtering corrects that into an unprofiled runtime value.
[Official profiler-inserted paused-time explanation](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2025-1/problem-unexpected-paused-time.html)

## Receipt and next action

The sole keeper first binds four real builds/cold/source diffs, retires all
other work, and completes the already-approved unprofiled/pure-counter and
native-component qualifications. The four whole-guest uarch cells above then
form a separate explanatory family. Save actual argv/config/version, target
PID/TID/address mapping, source/ELF/DSO closure, raw result DB, reports,
frequency/noise/resource receipts, checksum/counters, sample/MUX/loss/stack
quality and execution/collector/finalization exits. Raw result databases must
remain available; a curated CSV mirror is not their replacement.

If collector attribution or event quality is missing, the outcome is an
unresolved profile, not a zero-cost collector or an accepted optimization.
There is no new timing, permission change, source mutation or hardware-profile
PASS in this document.
