# MoonBit native Image pipeline and codec performance — 2026-10-20

Independent readback verified eight complete native ROS2 → MoonBit Wasm → native
ROS2 Image processing scenes, 24 zero/partial-permission scenes and 96 naturally
exited scene actors. Four fresh Guest products ran on both frozen interpreter
modes. The three repositories contain identical frozen evidence; active ROS
SDK/example/fixture sources belong to the ROS checkout.

A separate pure Image-codec benchmark completed 15 fresh formal timings across
Native, ordinary UWVM, stable UWVM, ROS UWVM and Wasmtime. One Native hardware
Hotspots profile passed actual target sample qualification. The following
Microarchitecture profile stopped at the strict raw-memory limit; the complete
ten-profile hardware matrix remains unfinished. The original fifteen-language,
full official client API, complete DBG and platform/cache requirements are not
all achieved by this checkpoint.

## Actual native input, processing and output

An independent native `rcl` process creates a reliable/volatile Image publisher
on an explicit input topic and observes a separate output topic. MoonBit opens
its separately granted subscription, receives a complete padded 2×2 RGB frame,
checks every header/layout field and pixel, converts it with the SDK to packed
mono8, and publishes the processed result. The native receiver checks the exact
output fields and `4c951cff` pixels, the actual Gateway writer GID and one final
receipt. It permits at most one complete output warmup with that same writer.

The native witness verifies the input source publisher, Gateway input reader
and output writer GIDs, exact node/namespace/type and actual native QoS. It
refuses foreign source endpoints, unexpected/duplicate values and any publisher
or data on `/forbidden`. The independent readback found 32 distinct native
source GIDs, 16 authorized input-reader GIDs and eight output-writer GIDs.

The owner first requires a real empty input poll before allowing the native
source to send anything. Each external `P` byte permits exactly one
`poll_image(256)` call, with at most 32 total credits. Each accepted full scene
used two credits: one error 11, then one complete input. The SDK does not silently
retry, resize or replay an effect. One native `I` gate sends one RGB Image.
The Guest's final `A` gate is released only after complete native output receipt.
Explicit publisher/subscriber/session cleanup must succeed. All Guest endpoints
must disappear continuously for two seconds before native cleanup and exit 0.
Observed post-gate quiet-to-exit intervals were 2.111–2.151 seconds.

The 32-scene matrix is linear/GC × Debug/Release Wasm × Release/Debug interpreter
× four policy profiles:

| Profile | Native behavior required |
| --- | --- |
| Zero grants | Input open refused 13; no Guest input reader or output effect |
| Publish only | Input open refused 13; publication permission adds no input right |
| Subscribe only | Authorized native reader actually matches; output refused 13; input closes without an output effect |
| Full | Exact native RGB input, SDK conversion and complete native mono8 receipt |

Every scene also refuses an ungranted publisher and subscription alias. The
reader verified 88 actual unresolved error 13 decisions, eight malformed Image calls
refused error 22, eight real empty polls and exact Gateway operation sequences. Each
malformed call has two related Worker audit entries; they are not 16 calls.
Malformed frames never reach a successful native publish. The fixture's native
source publisher remains an independent native endpoint until its own cleanup;
negative cases assert absent Guest endpoints/output rather than an empty DDS
domain.

## Policy and production evidence

`example_policy.py --subscribe-resource` selects a separate reviewed input
resource only when subscribe was separately requested. It changes `sub` and an
explicitly requested `subEvents` observation together, without adding operations,
limits or grants. This pipeline selects `pub` and `sub` and does not request
`subEvents`. The model checker verified 48 exact prior String/Image policies
(omitted/None field), 16 explicit split-topic profiles and 48 invalid-resource
or implicit-subscription refusals across ROS1/ROS2. These policy models do not
supply native ROS1 input-pipeline execution credit.

The producer has ten passed bounded commands: one policy regression, four fresh
MoonBit builds, four whole-module validations and one native peer build. All 43
actual ROS2 import signatures were checked on each product; the only extra
imports are numeric WASIp1 `fd_read` and `fd_write`. Two actual linear placement
inspections verify memory/data/globals and the pinned lazy allocator shape.

| Actual Guest | Bytes |
| --- | ---: |
| Linear Debug | 73930 |
| Linear stripped Release | 28827 |
| Wasm-GC Debug | 63822 |
| Wasm-GC stripped Release | 24943 |

MoonBit is pinned 0.10.15+943464bfa. The independent C++17 native witness uses
official `rcl`, `rmw`, `rosidl_runtime_c` and generated C Image interfaces with
the selected Humble SDK, Clang 21, explicit sysroot/GCC 11 libraries and
`-O1 -g -pthread`. Its 322 actual dependencies and linker map are retained.
The peer is 187232 bytes, SHA-256
`46f5830146f896217b41719d473bc2e1a90da7c365a4615645fe2d1c62338e07`.
The selected Gateway/SDK/loader are the frozen products already exercised by the
preceding native ROS2 Image checkpoint; this is not a fresh Gateway/host rebuild.

All compilers, validators, models, controllers, native witnesses and independent
readers ran through SSH Linux in the original existing hard 64 GiB/swap 0 cgroup,
with unchanged identity/boot/mount and no global-cache, sysctl or cgroup changes.
The accepted native controller sampled aggregate owned RSS up to 65236992 bytes
and raw shared memory up to 64465514496 bytes. Its exact memory.events interval
remained `max 2356, oom 0, oom_kill 0, oom_group_kill 0`.
Functional admission uses the documented file-cache estimate and raw 63.25/63.5 GiB
admission/running limits, finite deadlines/output and 768 MiB sampled aggregate
RSS. It never supplies hardware-performance admission. Retained pidfds constrain
cleanup to authenticated ownership; the helper's sampled discovery limit is
explicitly retained. All 96 scene processes exited naturally with status 0 with no cleanup
signals. No failed native scene or producer is promoted into this pass.

An initial transport script was rejected by the Linux Python parser before any
producer or scene started. Its placeholder substitution was corrected using a
separate JSON data envelope. The full initial stderr was not separately retained;
the private transport note and tool transcript disclose that limitation. The
accepted producer and native scene transcripts are complete.

## Separate same-source codec timings

These measurements use the preceding unchanged Image codec benchmark kernel,
with the exact same source/SDK work for Native and linear Wasm. Every iteration
checks full RGB and mono canonical round trips and complete output pixels.
An independent iterative Python oracle rechecks the actual LCG seed/checksum;
it does not reuse the cohort's closed-form exponentiation oracle.

Each engine completed three fresh formal runs with exactly 999637 iterations,
on selected target CPU 21. Run order reverses in the middle replication. Five
explicitly identified old closed calibrations choose the common workload size;
none of the 15 new formal observations is reused. Supervisor monotonic time runs
from the external `G` gate to complete `DONE`, including stdin/stdout and bounded
polling. Startup, parsing/JIT and two 17-iteration warmups precede that interval.
Formal timings have no attached collector. The host is shared with unrelated
work; these are three observations of one small fixed codec workload.

| Engine | Median seconds | Min–max seconds | Time / Native | Time / Wasmtime |
| --- | ---: | ---: | ---: | ---: |
| Native | 1.9684 | 1.8826–2.1056 | 1.000 | 0.230 |
| uwvm2 | 7.4157 | 7.2423–7.5596 | 3.767 | 0.865 |
| uwvm2-stable | 7.5825 | 7.4276–7.6064 | 3.852 | 0.884 |
| uwvm2-ros | 7.4071 | 7.1966–7.4772 | 3.763 | 0.864 |
| Wasmtime | 8.5743 | 8.3111–8.6190 | 4.356 | 1.000 |

All three UWVM products take less time than the selected Wasmtime 42.0.1 in this
finite workload. They still take roughly 3.8× Native time. That measured Native
gap remains open; these data do not establish negligible robot latency impact
or general superiority over Wasmtime. WAVM/WasmEdge have no new accepted rows.
The selected UWVM LLVM binaries are the frozen all 15 adapter R136 products;
their native ROS imports are disabled. They run the pure codec, while the native
ROS pipeline above runs the two separately frozen A3 interpreters. This is no
fresh host xmake compilation, debugger qualification or current-source size
measurement. Their selected cache modes/argv and input hashes are retained.

## One qualified hardware Hotspots profile; microarchitecture incomplete

The new Native Hotspots run executed 10156864 iterations. VTune 2026.4 collected
actual target PID/TID 231497 on CPU 21 / `cpu_atom`, with 12491
`INST_RETIRED.ANY` and 2226 `CPU_CLK_UNHALTED.REF_TSC` samples. Offline readback
reopens the PID/TID/CPU/PMU/address sample CSV, checks every dimension, excludes
other processes, verifies the configuration/report hashes and requires at least
ten actual target samples for each required event. Scaled event counts are not
substituted for samples. Raw result data, summary/XML/events/hotspot reports and
the sample qualification remain included.

The next native Microarchitecture attempt observed the target's exact DONE line,
then its enclosing owner stopped at raw shared memory 64555495424 bytes, above
the strict 64424509440-byte (60 GiB) active threshold. The partial report has no
completed timing, target exit status or hardware qualification. Its authenticated
controller/target/collector ownership closed, with that failed interruption
retained separately from the naturally exited native scenes and completed
profiles. No microarchitecture metric is qualified. Nine of the intended ten
hardware profiles, including every UWVM/Wasmtime profile, remain unfinished.
The batch status stays FAILED_RETAINED; only the separately reverified complete
15-run timing lane and one Hotspots profile receive credit.

## Reviewing and using the checkpoint

The detailed English [application guide](CURRENT_GUIDES/sdk/examples/moonbit_image_pipeline/README.md)
provides build, SDK interface, policy, native package/library and bounded gate
recipes. `MANIFEST.json` hashes every payload; `SOURCE_PATHS.json` maps original
Linux paths, and current guide/source associations distinguish documentation
changes from compiled inputs. Raw logs/reports remain byte-for-byte records.
Native C/C++ files use a `.source` suffix in the packet so root xmake discovery
does not create targets for archived translation units. Restore original names
only in a new owned review workspace. The retained controllers require new
resource/input admission before reuse; a packet is not a safe unattended launch.

Language debugger options/macros remain default ON. The active option guide
explains individual switches; disabling an adapter still permits valid Wasm
from that language and changes no ROS/WASI authority. Keeping the other language
adapters enabled is recommended. New Debug/Release products alone do not add
source/value DBG acceptance. Native Image input on ROS1, mixed/vendor bridges,
physical cameras, full client APIs, new QEMU/Windows/cache/CPUID matrices and the
remaining hardware profiles are still open. Small GC scene success does not qualify sustained imported-GC allocation: the earlier large workload stopped at its 2 GiB owned-memory bound; runtime root/Host-escape admission remains in force. The complete original all-language
requirements remain unfulfilled.
