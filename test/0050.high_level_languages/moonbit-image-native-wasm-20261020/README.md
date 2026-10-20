# MoonBit Image native/Wasm checkpoint — 20 October 2026

This identical three-repository packet contains a finite same-source codec
checkpoint. The active example and English build/run guide live only in the ROS
checkout under `sdk/examples/moonbit_image_benchmark`. Ordinary/stable receive
this evidence without an active ROS SDK dependency. This checkpoint does not
complete the original fifteen-language requirement bundle.

## Accepted scope

The matching official MoonBit 0.10.15+943464bfa SDK produced three actual
applications: native C, linear Wasm and WasmGC. Both packaged compiler hashes
match the pinned compilers; all 19 selected native-runtime files and all 49
actual project inputs are retained and independently rehashed. The complete
100,080,035-byte official compressed SDK was streamed to EOF and hashed;
selected runtime sources/headers/objects were retained, rather than storing a
second full compiler archive. The older Git runtime mirror did not match this
compiler and its failed build attempts remain visible.

Forty positive finite cases check the entire READY/DONE transcript against an
independent direct-iteration oracle, normal zero exit, empty stderr, actual
product hashes and closed owned process trees:

| Engine selection | Positive cases |
| --- | ---: |
| MoonBit native C | 3 |
| Release UWVM interpreter, both backends | 5 |
| Debug UWVM interpreter, both backends | 6 |
| Ordinary/stable/ROS-all/ROS-none frozen LLVM products | 20 |
| Wasmtime, both backends | 6 |

The selected inputs are 1, 17 and 65,537 iterations, except debug interpreter
uses 1, 17 and 257 and large UWVM GC rows are excluded as described below.
These are correctness cases; functional watchdog durations are not benchmarks.
The ROS-none selection demonstrates execution with language debugger adapters
disabled for these frozen products. No new full host/xmake build matrix is
claimed. The preceding Image packet retains separate source-map debugger
workflows; this packet adds no lexical managed-object debugger qualification.

## Retained failures and performance boundary

The release interpreter's imported-GC workload at 65,537 iterations crossed its
2 GiB sampled owned-RSS guard (2,149,392,384 bytes). Its authenticated target
was stopped and the process group closed; no cap was raised. Five other large
UWVM GC rows were not run; debug-GC 65,537 is also outside the selected small
profile. Wasmtime's separate 65,537-iteration GC case passed. Current managed
collection deliberately excludes imported modules/preloads; these workloads
import WASI. The retained current and frozen source snapshots explain this
admission boundary without weakening root or Host-escape checks.

The earlier debug linear 65,537 case exceeded 90 seconds. Its raw logs remain,
but its complete JSON was accidentally overwritten by the next controller's
report-path replacement. The explicit disclosure identifies the affected path.
The next actual report and all original raw logs are retained. No lost record
is reconstructed or counted as positive. The final reader independently checks
40 positive rows from the correctly named final report and exact closed logs.
An initial warmup expected-seed typo and an LLVM GC command missing feature
flags also remain as failed predecessors, with corrected source/commands bound
to later products and observations.

One performance cohort completed five calibrations and only two first-replicate
observations before raw cgroup memory crossed 60 GiB. A second controller made
37 bounded admission probes over 180 seconds and started zero new targets or
collectors. It required 59.5 GiB for additional headroom; active performance
protection remained the original raw 60 GiB threshold. **There is no complete
three-replicate comparison, no new VTune collection and no hardware Hotspots or
microarchitecture qualification.** The partial timings are retained as raw
observations and are not a comparative performance conclusion. Functional
inactive-file estimates never substitute for performance admission.

The existing benchmark lock was used without stealing or unlinking it. Own
process retirement uses authenticated pidfds. Output used a dedicated bounded
tmpfs directory because the underlying root filesystem had zero available
space; this tmpfs still consumes the same cgroup budget. Advisory cold-cache
release touched only identified ended own inputs/evidence, never R136 timed
runtime dependencies, global cache controls, cgroup limits or foreign files.
Shared memory changes are not exclusively attributed to those advisory calls.
The shared cumulative max-event counter is 1962 and OOM/OOM-kill counters remain
zero in the recorded final reader observation; this is not a claim that every
memory event was caused by this cohort or that it saw no prior events.

## Reproduction and evidence

`CURRENT_GUIDES` contains the exact maintained English SDK/example guides with
build commands, gates, bounds, backend caveats and debugger option links.
`stage/producer-result-r8.json`, `stage/functional-result-r5.json`, and
`stage/independent-readback-r1.json` bind products and functional observations.
`stage/readback-r1.py` contains the independent direct finite oracle.
`stage/perf-source-r1` contains the prepared guarded timing/VTune controllers;
read their exact plans and failed admission receipts before resuming.
`perf` preserves actual partial timing transcripts and plan hashes.

The source and runtime paths in historical receipts identify their original
Linux locations. `SOURCE_PATHS.json` maps every archived role to that location.
C/C++ sources are stored with `.source` suffixes to avoid becoming xmake tests
through repository source globbing. `MANIFEST.json` hashes every payload role.
Transport checks the complete gzip stream and every role; final publication
checks identical payloads and preserves every prior CONTINUATION entry.

This pure Image codec benchmark starts no native ROS middleware. It establishes
no positive native Image delivery, vendor/ROS1+ROS2 interoperability, official
rcl/rclcpp/rclpy/rcljava parity, sandbox-wide audit, cache-update/CPUID acceptance,
QEMU/Windows platform coverage or all-language completion. Those requirements
remain separately open.
