# R5 native TLS General GC P0 measurement binding — 2026-10-02

Status: source implemented; P0 plain/HW execution pending. The current ordinary
R5 long functional cold round is actual, not a planned pass: 34 stages, 32
product executions, 16 allocation and 16 mutation rows all passed. This adapter
does not modify the original S6e runner, the frozen measurement/guard code, the
source used by the product, or any historical raw result.

## Actual baseline qualification

Current main is 94,107,376 bytes, SHA-256
4aba5943567bbd216eb8e00382a6fee6ef5765b6d02f056f02b310abe0879632.
External source ID is
sha256:a97ff26b2da9dc6f227201bf7c36a73a371ee2885b710a188111f3d41472734a.
All TUs have native TLS. All GC/SET32/membership/array-auth experiment defines
are omitted. CAPTURE=1 support is compiled; sampling strips its runtime
environment variable and verifies the actual stopped-child environment.

The actual CLI8 closure uses one fresh main and same-task actual RT1/host3
objects. This is not an all-fresh claim. The original component schema stays
intact, including original_binding, actual argv/log/MD, actual dependency maps,
and objects. The adapter verifies shared compile prefixes, nativeTLS, no
experiment defines, MD/output identities and exact release response bytes,
then binds the full 3137-source-dependency union plus tools/libraries/objects.
The original static library/DSO inventories are checked; contemporaneous
loaded guest DSO maps are not qualified.

Actual long cold fixed pins:

- summary: 112044 bytes, SHA-256
  9da5e9d032c0aa551888883af3e6403b6aa16196cb5f853d74f13f368f9d0580.
- 34 actual receipts: SHA-256
  7d4690397319afcacd9a3523f0e9418e3b42504f0a5def8b4cf4fa6b1e780a5d.
- commands: SHA-256
  b28bec8d942f46acaff190928fd0c2f34dfe628ba63744338d6f2dff2ef58562.
- source-only long helper: SHA-256
  2afc5478b9b0d7d8bd94d29d92ea07e33d4f8ccf7de36104606f90d5453d23aa.

Prepare rechecks every actual 32-product row, argv/log, stopped capture absence,
retired owned PIDFD, strict GC counters and no-fallback/native witness. Both
instruction and unwind cold policies must pass. The measured product policy
is unwind. Exit zero alone never qualifies a collector.

The original sixteen 1M/2M Wasm files are unchanged. Official validate/WAT
roundtrip and Wasmtime copying run checksum/_start evidence is reused only for
those exact bytes. Old S6e product results do not qualify this product.

## Reused immutable implementations

The new run_general_gc_R5_nativeTLS.py loads only reviewed code:

- run_current_general_gc.py SHA-256
  91ee4854dcf7f176427f5943895bd9f933a5bfa2819c263c30e78774c8beada8:
  observed_guard and plain_sample, not its old S6e main or old source schema.
- run_current_host_pcore_hw_counting.py SHA-256
  673d961bb4fd37bd6ed98cc6c486cac3d2dc7fe256230795468ea58db7b42cd5:
  exact host admission/PMU/group attributes/math/owned SIGINT completion.
- run_current_pcore_hw_counting.py SHA-256
  266a56b3389ca5e120a757bd2a7a9b101b834810e5ba53f053b50f536aa425f8:
  unchanged stopped spawn and measurement functions.
- run_current_pcore_diagnostic.py SHA-256
  0ab44317fea5c7cddcf236d79be232e24237386d17795684fa2f0fa51c487f89:
  unchanged process identity and parser support.

No sampler/HostGuard check is removed. Clear-MM terminal observation uses the
same original PIDFD/birth/UID/PPID/PGID/CG/P0 checks and bounded two-second
retirement logic. Actual live argv/identity mismatches remain hard failures.
Missing short WT loaded-exec observations remain explicitly unqualified.
Plain Wasmtime may have multiple individually checked P0/UID/CG TIDs;
hardware products remain strictly single-TID.

The only spawn value adaptation is an explicit Popen child environment:
capture is stripped without modifying the controller environment. Before GO,
the original stopped child is checked for PIDFD/birth/state/UID/CG, actual
/proc/environ capture absence and actual loader/RAYON values. Failures before
returning the entry retire that exact owned PIDFD child with kill/wait4/reap.
Audit allocation occurs before Popen so failure cannot orphan an unreturned
child. Original bootstrap EOF/ownership protocol is unchanged.

## Actual keeper commands

After source review, stage the new driver and its pinned dependencies together.
Prepare cannot run before the actual fixed long summary/receipts exist:

    python3 run_general_gc_R5_nativeTLS.py prepare --out-plan /home/macromodel/Documents/uwvm3-implementation/wasm3-resume-20260924/builds/current-R5-nativeTLS-general-gc-measurement-20261002-r1/plan.json

Use the existing trusted wrapper to place the controller at UID1000/E16 in the
current exact 64GiB/swap0 scope. Keeper must produce a fresh real host-init
PID/birth/exe/boot/cgroup admission and real perf/tool/DSO inventory, not an old
container PID or mocked guard. The CLI below names those freshly produced
files symbolically; they must be actual paths before execution.

    python3 run_general_gc_R5_nativeTLS.py run --plan PLAN --admission ACTUAL_ADMISSION --perf-libraries ACTUAL_PERF_DSO_CLOSURE --out NEW_PLAIN_OUTPUT --measurement unprofiled --pairs 1 --execute

It runs 32 independent new-process samples: original sixteen fixture bytes,
current R5 and same-byte Wasmtime copying. A second pair is optional after the
first actual diagnostic round; pair two reverses order.

    python3 run_general_gc_R5_nativeTLS.py run --plan PLAN --admission ACTUAL_ADMISSION --perf-libraries ACTUAL_PERF_DSO_CLOSURE --out NEW_HW_OUTPUT --measurement hardware --pairs 1 --execute

It runs eight 2M product family/phase samples, serially. There are no Wasmtime
single-TID counts and no software events. Actual cpu_core group raw
{cpu_core/event=0x3c/,cpu_core/event=0xc0/} is unchanged; type4/config/group FD,
real enable ACK before GO, original-owned SIGINT after guest retirement, exact
raw time_enabled/time_running and JSON/raw crosschecks must all pass.
No REF_CPU_CYCLES event is added; a future capability check must be separate
and retain failures rather than introducing multiplexing silently.

No compiler, VM, managed port or profiler executes concurrently. The original
guard retains 64GiB/swap0, target memory headroom, exact P0 guest/E16 profiler/
controller, birth/UID/cgroup/argv ownership, PIDFD retirement, deadline and
bounded output checks. Temperature is raw observation only, not a start,
peak or pair rejection condition. The adapter does not use sudo or alter
sysctl/seccomp/cgroup settings.

## Distinct evidence and qualification

Each raw row contains source/product identity, effective argv, environment
receipts, original guest executable observation, actual wait4 resource usage,
frequency points and GC counters.

| Evidence | Scope |
| --- | --- |
| Internal Total WASM execution | Excludes startup/JIT; includes setup, allocation/collection, fields, root-ring readback and checksums |
| Internal Total process time | Product's own complete-process report |
| Parent wall/user/sys/RSS | New process including startup/JIT; Wasmtime accounting includes all its checked guest TIDs |
| Pure hardware cycles/instructions | Whole product single TID including startup/JIT; actual enabled/running/group protocol |
| Later VTune HW Hotspots/uarch | Separate collection with target filters, actual sampling/MUX/stack limitations |

Allocation qualification requires exact planned count, roots1, disabled0,
positive attempts/collections/reclaimed and reason0. Mutation qualification
requires exact initial 1024/2048/1024/3072 allocations, no attempts/collections/
reclaimed, roots1/disabled0 and reason2. Mutation is field/lookup evidence only;
collector_qualified is always false.

Whole workload timing is never a pure collector latency. Profiles/ROIs cannot
replace one another or prove industry ranking. Short samples retain missing
frequency/exec quality failures; host SMT activity remains unknown without
an independent actual observer. Two repeated products could be frequency
matched only on actual in-window medians within10%; that flag does not mean
complete host quietness or formal acceptance.

The previous S6e named get_thread_state percentage belongs to the actual
non-native-TLS map fallback profile. It is not the current nativeTLS baseline.
A subsequent independent VTune recipe should bind these same bytes/current
pins, select actual PID/TID/P-Core/cpu_0 report fields and retain anonymous
mapping as Unknown until actual JIT PC/object/source binding exists.

Pure Python checker covers schema/argv/profile negatives and isolated mock
environment/cleanup contracts. It performs no native execution and cannot
claim current P0 performance, collector-wide release, ROS, EH or thread
performance acceptance.

## Source revision v2: preserve the immutable spawn namespace

The actual first v1 plain attempt stopped before Popen/guest admission with
AttributeError for subprocess.STDOUT. No guest executed; the original failure
and v1 source freeze remain unchanged. This v2 changes only the private cloned
spawn's subprocess namespace: it copies the original module namespace and
overrides Popen. STDOUT, PIPE, DEVNULL and every other original constant remain
available to the unchanged spawn code. The checker explicitly reads these
three constants through its fake spawn; 65 pure Python checks pass.

The measurement plan, actual source/product/cold qualification, original
guard, sampler and PIDFD/ACK hardware protocol are unchanged. This source
revision is not a measurement pass.

## Source revision v3: bounded real retirement observation

The actual v2 plain attempt completed 18 semantic rows before its short
Wasmtime numeric-array1M child was observed with empty argv/RSS0 and actual
zero-MM stat Z, while the original PIDFD was not yet readable. Its loaded
executable witness remained null. The separate product HW attempt's second
row instead lost /proc/exe after a prior authenticated full argv observation;
that raw record has no second stat and does not prove it was terminal.

This v3 leaves all five frozen measurement/guard dependencies unchanged.
It records the original failed proc observation before considering either
exact error. Only a plain reference with no loaded witness and the pinned
Wasmtime path can recover Owned command changed; its initial empty argv,
all12 real zero-MM fields and birth/UID/P0/CG/PPID/PGID must already agree.
A product's Missing executable is not proved retired can be rechecked only
with its original complete loaded executable/argv identity, expected product
path and an initial legal argv. Any observed live mismatch remains hard.

Fresh observations must independently retain the original PIDFD/birth/
ancestry/UID/P0/CG and actual empty argv with all12 zero-MM fields. The2s
maximum wait neither signals nor authorizes a live child. Only real Z or
absent stat plus the original readable PIDFD, or the unchanged sampler's
actual same-child wait4 reaped/status receipt plus readable PIDFD, permit
re-running the entire original check. No cached executable/argv, synthetic
terminal state or fabricated field is returned. Perf never uses this branch.
The original failure record and new bounded observation audit are preserved.

Every final row still requires actual wait4, original semantic contract and
closure. A short reference with null loaded identity stays unqualified,
cannot participate in rankings and never receives a manufactured witness.
Product HW remains single-TID and retains actual ACK-beforeGO, event/FD/
ENA-RUN math and owned-SIGINT-after-retirement checks unchanged. 115 pure
Python checks include positive terminal cases and20 ownership/live-MM/
unknown-product/perf/timeout counterexamples. These are mocked protocol
checks, not a new native result.

## Source revision v4: verify deadline at actual terminal proof

The v3 source-only freeze remains immutable. V4 adds one post-observation
check of the original FD and2s deadline before recording terminal_confirmed.
A single proc read that crosses the deadline cannot be reported as a bounded
success. A late-terminal-proof pure mock negative joins the previous tests;
117 pure Python checks pass. No source/product/plan/guard protocol changes
or new native result accompany this source revision.
