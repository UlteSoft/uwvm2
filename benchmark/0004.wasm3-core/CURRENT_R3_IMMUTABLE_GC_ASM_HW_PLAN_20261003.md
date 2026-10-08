# Current immutable GC ring: next assembly and hardware cells

This is a finite source plan for the sole Linux keeper. It creates no launcher,
guardian, profiler binding, native build or production change. `execute_ready`
is false until the keeper supplies the actual successful parent-owned R3
three-TU source/compiler/provider/ELF/cold closure. A source packet, a prior
component executable, or an older product is not that closure.

The fixed target is the original `gc-allocation-ring`: Wasm SHA-256
`66874f9a4a0a5977b4ace1cc702f949c6c2627311dce0bcbe87c818c55516e3e`,
16,000,000 iterations, 1,024 exported table roots and returned state
`493211925`. Its immutable i32 struct creation, table set/get, ref cast and
struct get are the performance target. The eighteen native mutable-header
plain rows and two new mutable-field hardware rows have completed; this plan
does not repeat them or relabel them as this Wasm.

## First actual assembly

After fresh product cold success, use the existing compiled test capture hook
in separate untimed instruction and unwind runs for each repository. Use a
fresh private exclusive capture path for every run, cache disabled and `-Rct
0`. Ordinary uses `-Rcc jit -Rcm full`; ROS uses its actual full-JIT shortcut
`-Raot`. Preserve the selected `-Rllvm-full-policy pb-o3`, call-stack strategy,
`-Rllvm-exception-dispatch auto`, GC feature switch and exact fixed Wasm in the
actual argument array. Capture is absent from every timed/profiled child.

Bind the actual compiled source, runtime/provider origin, actual product and
captured object bytes, object sections/relocations/CFI and symbol listing.
Inspect the real loop and the matching actual ELF's table-get, retention and
immutable-cast callees. Process-local helper addresses must resolve through
that run's actual executable/map/symbol evidence. There is no public IR-dump
switch to assume. The existing cast/get witness already permits one native
field load; verify it rather than proposing it again.

The immediate question is whether local membership authentication actually
executes once in table-get caller retention and again in the adjacent cast.
If both walks are present, the reviewed
[get/cast proposal](GC_IMMUTABLE_TABLE_CAST_SOURCE_PROPOSAL_20261003.md)
describes the bounded next candidate. If the host optimizer has eliminated
them, retain that result and choose a different measured cost. Static source
alone, lambda ordinals, or disassembly from a different ELF do not select a
candidate. No authentication or lifetime check is removed here.

## Unprofiled and pure hardware families

Use the keeper's original approved plain/HW samplers with their actual owned
child identities and this new product closure. Do not extend the existing
native-component binding by substituting a product argv. Start with one
instruction/unwind pair per repository for this fixed Wasm; an additional
reverse-order pair follows only if the actual timing/frequency comparison
warrants it. Parent controls the exact final cell count and source namespace.

Internal Wasm execution, GO-to-reap process wall time, wait4 user/system/RSS
and grouped whole-guest cycles/instructions remain separate. Pure counting
needs actual final-guest PMU attribution, raw enabled/running, non-sampling,
no scale, original ACK-before-GO, original PIDFD retirement and full source
closure. Do not copy the native 16M mutable result or its ELF into this family.
No terminal-classifier actual-positive result is inferred from the two new
hardware rows: neither exercised that branch.

## One initial hardware profile pair

After ordinary and pure-counting retirement, begin with only the ordinary R3
unwind product on the same fixed Wasm: one hardware Hotspots run and one
Microarchitecture Exploration summary run in separate fresh result paths.
The initial goal is native helper self samples and whole-guest microarchitecture,
not another elapsed-time estimate. Add the instruction or ROS comparison only
when the real assembly or those two results identify a strategy/repository
question. These are profiled executions, never unprofiled timing rows.

Reuse the keeper's existing separately reviewed owned-process-tree supervisor
inside the same actual 64-GiB/swap-zero cgroup. Compiler, QEMU, Windows,
another VM and another profiler must retire before the window. The guest and
every actual guest TID stay on P0; profiler/controller use admitted E-cores.
There is no new supervisor, PID exception, global permission change or software
fallback. Temperature is an observation, not a rejection; actual sampled
frequency, throttling/OOM and available noise evidence stay in the receipt.

Record the actual profiler ELF/version, injected loader/helper/DSO closure and
fresh installed collect/report help before execution. Replace only actual
source-bound target/result paths in the existing successful command arrays:

```text
ACTUAL_VTUNE -collect hotspots -knob sampling-mode=hw
  -knob enable-stack-collection=true -knob stack-size=1024
  -knob enable-characterization-insights=false
  -run-pass-thru=--perf-threads=none
  -result-dir ACTUAL_NEW_HOTSPOTS_RESULT -- ACTUAL_PINNED_P0_GUEST_ARGV

ACTUAL_VTUNE -collect uarch-exploration
  -knob pmu-collection-mode=summary
  -result-dir ACTUAL_NEW_UARCH_RESULT -- ACTUAL_PINNED_P0_GUEST_ARGV
```

These are supervisor inputs, not standalone host launches. Fresh installed
help determines availability of each previously successful knob. Do not add
the previously unsupported `-cpu-mask=0`; actual guest admission and report
CPU partitions establish P0. Intel documents the explicit hardware Hotspots
selection and describes summary as whole-run counting with lower overhead;
neither guarantees full event coverage or function-level uarch attribution.
[Official knob reference](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-1/knob.html)

Save raw collection/finalization exit statuses, real guest semantic output,
internal timers and original identities separately from profiler exit status.
First save unfiltered summary and process/thread/CPU partitions. Query the
actual result's grouping/filter help; then use its own real PID/TID and
`cpuid=cpu_0` where supported. Reconcile namespace identities rather than
substituting the profiler PID. Preserve errors, lost samples, MUX/coverage,
hardware event configuration and skipped-stack warnings. Self samples and
inclusive stack samples cannot be added as disjoint cost; missing frames do
not describe complete call stacks.

## JIT attribution boundary

The current MCJIT listeners record loaded code/function ranges for runtime
ownership, and the selected debugger listener can capture native metadata.
They do not currently register ordinary JIT methods with Intel's profiler.
An object captured in a different untimed process proves its machine code,
not a profiled process's anonymous PC. Unknown anonymous JIT samples remain
unknown unless that exact collection supplies loaded object, PC range,
publication generation and sample-interval identity. Native ELF/DSO samples
can still locate the matching actual table, authentication, publication and
collector helpers.

The vendored LLVM `createIntelJITEventListener()` is conditional on
`LLVM_USE_INTEL_JITEVENTS`; its disabled definition returns null. Its presence
in a header does not prove the current SDK/provider includes that component.
A later source proposal may add an explicitly selected cold registration
listener with genuine load/unload lifetime, owned method identity and actual
profiler library pins. It must occur before execution and be separately
qualified; no executed ordinary access hook, ambient guest-triggered loader,
cross-generation reuse or hot-path guard is justified by profiling. Intel's
API requires reporting after compilation and before entry, and invalidates
updated code through its explicit event protocol.
[Official JIT notification API](https://www.intel.com/content/www/us/en/docs/vtune-profiler/user-guide/2026-0/ijit-notifyevent.html)

For the first two collections, no production registration change is required.
The previous hardware recipes' anonymous-JIT and injected-environment limits
remain explicit. A report-level time filter needs a real mapping to the
profiler's elapsed clock; it cannot manufacture an exact Wasm PMU ROI from the
program's internal monotonic timer. No claim of industry-leading performance
or a fix for the old 117-ns observation follows until the same actual Wasm
and product candidate have been measured.
