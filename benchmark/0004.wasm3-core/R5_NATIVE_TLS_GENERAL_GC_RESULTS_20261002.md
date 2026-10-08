# R5 native-TLS general-GC actual measurements, 2026-10-02

These are completed development measurements from boot
`b0907744-e3a0-4e1b-a1c5-3c80472a5e48`. They do not qualify a later boot,
an industry ranking, the complete collector, other architectures or ROS.
Temperature was observed only. SMT/foreign-host activity was not qualified.
The current R5 ordinary profile used native TLS in every translation unit;
all six compact/numeric experiments, SET32, trace-metadata and local-membership
experiments were omitted. The older S6e map-backed TLS profile is separate.

The persistent review is
[the root review](../../build/wasm3-evidence/current-R5-nativeTLS-P0-plain32-HW8-complete-20261002-r1/root-review.json),
SHA256 `5b3bd84dc5f95d682c92e0d921922e5d67107181ac995fd3c6c03b7fb866f94d`.
It references raw packets retained remotely; local packet origins are
`/tmp/uwvm-R5-nativeTLS-general-gc-plain32-complete-small-packet-20261002-r1/compact-metadata-raw.tar.gz`
(483527 bytes, SHA256 `583c5a350744fa82f3d43f904f65a5cf7ea16acbdc5b6bfb98768a0b9e68f26c`)
and
`/tmp/uwvm-R5-nativeTLS-general-gc-hardware8-complete-small-packet-20261002-r1/compact-metadata-raw.tar.gz`
(438088 bytes, SHA256 `d2d8d66174f80a1ecab36d81922f6be025513290c02bf02d9b9daf6aa9728f11`).
This document is a small derived analysis, not a replacement for original logs.

Actual plain32 and HW8 summaries completed with before/after closures true,
32/32 and 8/8 self-checking semantics, and 8/8 hardware math/attribution proofs.
The plain summary SHA256 is `f5554e475beb8602ce870103c1dfb200ad0a102d49f8b39614287c8a078a50d4`;
hardware is `04f5fdd0b21126871b6815291a71796bb3a2dbf07a257e758e0b33c583b949ce`.
Actual execution used frozen measurement v3 SHA256
`34fb87a80e6e1abc8a27d07928585aa129236fae20df754e4b8dd600174fa1cf`.
Approved v4 was not substituted retrospectively and does not require rerunning
these samples. The prepared plan SHA256 was
`c6e08fa3595470094b6c97db55c3ddd20e45440862e014a4003be4c03ba63f35`.
Actual product SHA256 was
`4aba5943567bbd216eb8e00382a6fee6ef5765b6d02f056f02b310abe0879632`;
source ID was `sha256:a97ff26b2da9dc6f227201bf7c36a73a371ee2885b710a188111f3d41472734a`.

The table reports one unprofiled sample and a separate hardware-counted sample
for each 2M-step workload. Its times are not paired A/B medians.

| Family/phase | Plain internal ns/step | Plain parent wall ms | Plain wait4 user/sys s | HW internal ns/step | Whole-guest cycles | Whole-guest instructions | Exact enabled=running ns | Plain allocations/collections/reclaimed |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| mutable-struct-allocate | 82.207 | 179.249 | 0.181607/0.006984 | 89.107 | 1039049321 | 3372706571 | 193100017 | 2000000/488/1997823 |
| mutable-struct-mutate | 22.258 | 58.746 | 0.062279/0.005931 | 23.196 | 332399097 | 1105325177 | 60343527 | 1024/0/0 |
| reference-cycle-allocate | 265.888 | 558.833 | 0.561782/0.005976 | 280.527 | 3144593566 | 11050988589 | 587472212 | 4000000/976/3995646 |
| reference-cycle-mutate | 100.564 | 227.188 | 0.229828/0.006994 | 103.215 | 1266697703 | 3759747529 | 230626033 | 2048/0/0 |
| numeric-array-allocate | 187.715 | 402.728 | 0.405133/0.006967 | 194.359 | 2242142805 | 7214725723 | 415450479 | 2000000/488/1997823 |
| numeric-array-mutate | 48.938 | 119.700 | 0.125129/0.003972 | 50.865 | 664810507 | 2140706470 | 122648810 | 1024/0/0 |
| reference-array-allocate | 607.226 | 1265.051 | 1.265279/0.008987 | 630.127 | 7138181843 | 22285424713 | 1309671200 | 6000000/1464/5993469 |
| reference-array-mutate | 108.878 | 258.538 | 0.260760/0.005994 | 112.695 | 1422187013 | 4571171141 | 262925334 | 3072/0/0 |

The internal Wasm timer excludes startup/JIT compilation. It includes fixture
setup, allocation or mutation, lookup, collection when triggered, readback and
checksum. It is not collector latency. The parent GO-to-reap wall is a separate
interval. `wait4` user/sys and peak RSS cover the entire original child lifetime,
including its stopped Python bootstrap before GO; these are not guest-only
CPU/RSS. Recorded peak RSS was 82292 KiB for all plain rows and 82276 KiB for all
HW rows, so it cannot distinguish these guests' actual peak memory use.
The HW group was enabled before guest GO and includes startup/JIT and Wasm
execution after that ACK. It excludes earlier stopped-bootstrap work. No hardware
counter in this table is restricted to the internal Wasm timer or collector ROI.
Allocations are not steps: reference-cycle allocates two objects per step and
reference-array three, while mutable struct/numeric array allocate one.

An independent reparse of both original stat-output and stderr streams reproduced
all eight raw counts, exact enabled/running times and alias mappings. Each final
guest had PMU type4, raw config0x3c/0xc0, CPU attachment-1, successful leader FD5
and member FD7 with group_fd5. The group was counting/no-inherit, with actual
enabled=running and 100% coverage for both events. Each original control ACK
preceded GO and actual guest retirement; the successful original-PIDFD SIGINT
occurred after reaped exit0. The recorded perf exit-2 was the owned SIGINT contract,
not an ignored failure. Guest/perf/stat bytes matched their recorded SHA256s.
Raw event strings remain `cpu_core/event=0x3c/` and `cpu_core/event=0xc0/`;
cycles/instructions dictionary names are explicitly derived mathematical aliases.

All product allocation rows had positive collection/reclamation, roots_requested1,
disabled0/reason0. Mutation rows had their exact 1024/2048/1024/3072 setup
allocations, attempts/collections/reclaimed0, roots_requested1/disabled0/reason2
(phase_pending before threshold). They qualify fields/lookup only, not collection.
The same-byte Wasmtime copying runs exit0, but 10 plain reference rows had no
observed loaded-executable witness and 9 had no in-window frequency. Short-row
qualification failures stay present; unknown identity stays null. Even the single
longer reference-array pair lacks qualified SMT/host-noise evidence. There is
no cross-engine ranking here.

The sole terminal audit preserved original PID223915/birth6003164/FD4 and
already-observed actual guest executable/argv. A fresh actual Z/MM-zero observation
and original PIDFD readiness completed after 1116639 ns, within the original 2s
bound; complete original guard checks then repeated. No cached executable, fake
Z or synthetic loaded identity was returned. This old-boot audit is not admission
for a restarted host.

Current VTune initial data is separate from both groups. Its 30-payload origin is
`/tmp/uwvm-R5-nativeTLS-general-gc-vtune-initial-small-reports-20261002-r1/compact-metadata-raw.tar.gz`,
373861 bytes, SHA256 `3648a1e767f06b666c7bb87b28382c131fa3f7112ef9959f519eb41d6f2a25df`.
It reports four actual hardware collections and 16 commands. The initial CSVs
are unfiltered, target-P0/PID/TID queries are pending, and the profiler inserted
two runtime-loader prefixes: strict environment qualification remains false.
Those prefixes were actually observed; this document does not replace the failed
strict contract with an assumed environment.

Unfiltered reference-array allocation samples name the typed struct_get32 bridge,
array_get, publish, struct_get and two typed collector visitor instantiations.
Mutable-struct mutation has only about 69 ms of sampled CPU time; struct_set,
struct_get32 bridge and ref_test appear, but the sample is too short for precise
hotspot percentages. Anonymous ld.so.cache/outside-module addresses remain
unknown. Current uarch summary MUX is .519/.562; it is not the 100%-running
pure-count evidence above. Its short mutation TopDown includes implausible
98.2% BadSpeculation and must not drive a branch redesign. Native-TLS function
attribution needs the actual filtered reports, not old S6e lambda labels.

Next controlled work is the same-source single-flag plan in
[the collector A/B/C plan](GC_NATIVE_TLS_SINGLE_FLAG_PLAN_20261002.md).
Current EH throwing/catching, actual thread creation and parked-wake performance
still need their independently bound current-source measurement series.

Actual filtered-report update, revision2 (report boot differs from collection boot)

The old result databases were subsequently reported under new boot
`83f582ec-ff7f-41ce-9631-2c0fbb865aeb`, using fresh reporting-process admission.
This did not rerun the four original profiled guests, and their old PIDs are DB
filters only, not authority over current host processes. The filtered packet is
`/tmp/uwvm-R5-nativeTLS-general-gc-vtune-filtered-complete-small-packet-20261002-r1/compact-metadata-raw.tar.gz`,
323020 bytes, SHA256 `fd7452a700f1a9a2b0d2c60fe01f38eb957bfb78fbe8c47bb06ac4a4fcc7105d`.
The actual query packet used prefixed '-group-by','?'/'-filter','?' options;
producer v3 now emits explicit prefixed query options. Original query/collection
raw data is retained and never edited to fit documentation.

All eight filtered commands completed. Each target thread table has exactly one
row with its historical collection PID==TID and Logical Core=cpu_0: refarray
Hotspots226479/uarch226650, mutable Hotspots226800/uarch226968. Functions were
reported using actual process-id/thread-id/cpuid filters; the reported module
path identifies the exact R5 main. CPU self-time function sums differ from target
thread totals only by CSV decimal rounding. These are reported sample associations,
not contemporaneous loaded-module maps or new native execution receipts.

| Reference-array allocation, P0 sampled self | Seconds | Share of1.732138s function sum | Reported start address |
|---|---:|---:|---|
| collector typed visitor instantiated with reported callback#3 |0.275794|15.922%|0x17ed210|
| llvm_jit_gc_struct_get32_bridge<false> |0.213294|12.314%|0x1949eb0|
| collector typed visitor instantiated with reported callback#4 |0.125000|7.217%|0x17ed6c0|
| gc_object_store::array_get |0.118056|6.816%|0x163b090|
| gc_object_store::publish |0.078373|4.525%|0x13fb920|
| gc_object_store::struct_get |0.054563|3.150%|0x1638cc0|

The two collector labels refer to the actual reported full typed instantiations
in this source; they are not the old S6e locate lambda numbers. A source/PC mapping
or clone of another version cannot be inferred from a lambda number. Anonymous
ld.so.cache81.349ms and outside-module25.794ms remain unknown. Initial action is
therefore the already-reviewed same-source membership/metadata A/B/C, with complete
preflight/mark safety retained, rather than a new collector justified by Unknown.

Mutable Hotspots sampled CPU only0.068452s. Its named struct_set8.929ms,
struct_get32 bridge7.937ms, ref_test6.944ms and reference_matches5.952ms are useful
operation clues, not enough samples for an exact micro-optimization ranking.
A longer same-semantic cold-qualified workload is needed for a stable field profile.

The filtered uarch function reports each contain one [Unknown] aggregate row:
no function-level uarch attribution is available. Refarray target226650 has
P-core clockticks7099069439 and instructions22287051085, identical to its generic
columns; mutable226968 has329119494/1110551403, also identical. This excludes
mixing an E-core count into these target rows. Reported average CPU frequencies
are4.352830676GHz and5.274131744GHz, derived in different profiled executions.
They do not replace the unprofiled frequency distribution or pure-HW raw counters.
MUX.519/.562 and the short mutation98.2% BadSpeculation limitation remain. Neither
CPU partitioning nor successful report generation turns summary multiplexed events
into exact100%-running counting or makes [Unknown] a known JIT function.

Strict actual loader-environment qualification and complete loaded-DSO-map
qualification both remain false in the actual report proof. These samples support
development attribution, not an industry ratio, complete release qualification or
an inferred harmlessness of unpinned profiler-injected DSOs. No sample is relabeled
as a fresh-boot measurement, and none validates a future A/B/C product build.

Revision3 attribution clarification: callback ordinals above reproduce the actual reported typed instantiations. They do not by themselves prove a specific preflight or mark source phase; final source/PC attribution must use the captured R5 source/object rather than current live lambda numbering. Revision2 remains frozen.
