# MoonBit native ROS1 Image processing checkpoint — 20 October 2026

This packet independently verifies **eight complete native ROS1 → MoonBit Wasm
→ native Image pipelines**, **24 zero/partial permission scenes**, and **97
naturally exited actor commands**, including the independently owned Master.
It covers four freshly compiled linear/GC × Debug/Release Wasms on two frozen
Release/Debug interpreter products. All compilation, module validation, native
execution and independent result validation ran through SSH Linux in the
original existing hard 64 GiB/swap0 cgroup. Mac operations were source editing,
inert transport, hashing and publication. No other agent was contacted.

This is a finite Noetic x86_64 loopback TCPROS fixture, not a physical-camera,
all-vendor, mixed-ROS, complete official-client, debugger or performance result.
The ROS2 Image pipeline and earlier codec timings/hardware result are separate
parent checkpoints, linked in [PARENT_LINKS.json](PARENT_LINKS.json). The source
SDK belongs to uwvm2-ros; this same frozen packet is published in all three
repositories without adding ROS imports to ordinary/stable products.

## Qualified behavior

An independent native roscpp executable publishes one complete padded 2 × 2
RGB Image. The Guest explicitly subscribes to its administrator-selected input
topic, validates every decoded field and pixel, converts padded RGB rows to
packed mono8 and publishes on a separately granted output topic. The native
receiver verifies the complete output and actual TCPROS publisher identity.
Local Guest success alone never qualifies native delivery.

- Native input: sequence 0; stamp seconds 1/nanoseconds 2; frame
  `camera/optical`; height/width 2/2; `rgb8`, little endian, step 8;
  complete bytes `ff000000ff00aabb0000ffffffffccdd`.
- Processing: bounded `poll_image(256)` and `Image.to_mono8(1)`, respecting
  padding and computing `(77R + 150G + 29B) >> 8` per pixel.
- Native output: same stamp/frame/extents, `mono8`, step 2 and `4c951cff`.
  Its native roscpp publisher assigns sequence **1** after warmup. Canonical
  header preservation does not override native publisher sequence management.
- Both attempts to open `ungranted-image` must return 13. Across the complete
  matrix, 88 unresolved-denial decisions are independently rechecked.
- Eight malformed Image submissions through the actual Worker return 22 before
  native publication. Each call produces its expected two Worker audit records;
  that is eight calls, not sixteen independent negative cases.
- Each full scene first performs an actual empty input poll before native send:
  eight error-11 polls total. Each full scene uses two external P credits,
  exactly one empty poll followed by one complete input consumption. The SDK
  does not retry, replay publication or resize consumed events implicitly.
- Zero, publish-only and subscribe-only scenes never send native input or
  produce processed output. Subscribe-only first demonstrates its actual native
  reader, then refuses output permission and retires the input owner.
- All endpoints retire, with matching native connect/disconnect counts and two
  continuous quiet seconds. Observed gate-to-natural-retirement intervals are
  2.167965685–2.392656239 seconds. All 97 commands exit naturally 0 with no
  cleanup signals; authenticated process ownership is closed.

ROS1 identity checks use native subscriber connection callbacks, MessageEvent
publisher identity and TCPROS callerid/type/MD5/latching headers. The official
Image identity is `sensor_msgs/Image`, MD5
`060021388200f6f0f447d0fcd9c64743`, latch 0. The owner checks the selected
Master's actual PID, initial/final empty graph, exact input/output native node
participants and published types through explicit phase snapshots. There are
32 distinct source node names, 16 authorized reader scenes and eight authorized
writer scenes. These are ROS1 observations, not ROS2 GIDs or authentication of
an arbitrary hostile remote ROS1 network.

## Actual producer and runtime matrix

| Guest target | Profile | Wasm bytes | SHA-256 |
| --- | --- | ---: | --- |
| linear Wasm | Debug | 73,111 | `b9c8ccba3f444a9063bee7e1dc105e54e312f4130f93c03ff3d3bdf8e0bfa76f` |
| linear Wasm | Release | 28,296 | `009fffd346cd4016f32de06d89c49385a91b733068f9b9a938b686d5fe07c4df` |
| WasmGC | Debug | 62,224 | `ac98b14daba9b343b42989ce05ce5814f67c01d5cbc12812d4c182a4cc01bffa` |
| WasmGC | Release | 24,402 | `3c03006ac17e45ae1df2ed618613ca7613de12c91f8c59d36f07844fe86401d3` |

All four actual modules pass whole-module validation and exact checks for all
38 numeric ROS1 imports. Only numeric WASIp1 fd_read/fd_write are additional
imports. Both linear products pass independent actual data/allocator/arena
placement inspection. Nine producer/validation commands pass: four builds,
four whole-module validations and one independent native peer build. The peer
has 844,368 bytes, SHA-256
`8a6e22340d360bdf17c82b829dc759d69106b3f6e267fa143e757f9114aab447`.
Its 1,272 native dependency rows and selected compiler/sysroot/loader provenance
are retained in the producer report.

The compiler is the pinned MoonBit 0.10.15+943464bfa toolchain. The native peer
uses Clang 21 with the selected Noetic sysroot and matching libc loader. The
native Gateway, Master, Python and their dependency provenance are explicitly
pinned. Every live native executable mapping is restricted to those selected
SDK/library roots or exact peer/Gateway products, then hashed after retirement.
No host/SDK libc mixture is accepted.

The frozen UWVM interpreter ELFs come from the earlier defaults-on restoration:
Release 15,442,000 bytes, SHA-256
`719e1cb683abeed7710d54ae705edc3e3ddd0048daf91cdbfe5a1ae6a9c26c19`;
Debug 242,820,864 bytes, SHA-256
`2bef37516ecc05a4f6e56124a1c5e8b011c6f725888ca529c4ecceb77936f084`.
Both have all fifteen language options enabled and both ROS imports. These
are exact frozen products, not fresh builds of the later dirty Host checkout.
[Runtime restoration](FROZEN_RUNTIME_RESTORATION.json) retains the original
build profiles and size provenance. Debug/Release compilation is not an actual
source-debugging workflow or a complete deployment size measurement.

## Resource failures retained separately

The first producer attempt failed its admission check and launched **zero**
child commands. The second producer attempt completed all nine commands.
The first native attempt completed three negative scenes. Its full scene
reached complete native receipt and successful Guest cleanup, then hit the
functional capacity guard while waiting for native quiet retirement. That full
scene is **not a completed pass**. All thirteen owned actor commands closed:
eleven exited naturally 0; the peer and Master were terminated through their
retained authenticated ownership records. No foreign process was signalled.
The first controller did not record its exact failing guard sample. Its later
raw-memory reading exceeds the stop line; no missing sample is reconstructed.
The subsequent controller records rejected samples before raising the guard.

A later 50-sample stricter admission observation started no actor. Only after
five further consecutive samples passed the conservative startup margin did the
second native attempt start the complete 32-scene matrix afresh. It does not
reuse the first attempt's partial positive scene or three negative passes.
All failed and refused reports, controller revisions, raw logs and policies are
retained. The earlier partial independent readback is a point-in-time result
before the second native attempt existed, not an alternative full-matrix pass.
The file named `readback-full-matrix-PENDING.py` is an authored, unexecuted
intermediate; `readback-r2.py` is the actual final full-matrix reader.

The original group is
`/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope`,
device/inode 29/19075, boot
`51c27aa1-d709-4a06-ba16-2ac3003fbcbd`. Hard memory/swap limits remain
68,719,476,736/0 bytes. Owner CPU15, native functional actors CPU19 and
compiler CPU18 are explicit. Parent AS is 512 MiB, child AS 128 GiB, aggregate
owned RSS bound 768 MiB; CPU/deadline/output caps are finite. The final native
matrix's sampled aggregate owned RSS peaks at 72,052,736 bytes. Its raw-memory
peak is 67,372,294,144 bytes; the functional estimate peaks at 55,900,614,656.
This estimate subtracts a bounded file-cache estimate and never supplies
performance admission credit. Functional admission remains estimate <54 GiB
and raw <63.25 GiB; running stop limits remain estimate <56 GiB and raw
<63.5 GiB. The successful restart additionally required five raw <62.5 GiB
startup samples. Memory events remain max2697, OOM0 and OOM-kill0 throughout
the accepted matrix; earlier shared counter changes are not attributed to it.

## Evidence and source associations

The [full independent readback](records/independent-readback-r2.json) and
[executed reader](records/readback-r2.py) rehash actual products, inputs, native
library mappings and raw logs. They independently parse protocol phases,
permissions, exact audit operations, Master graph snapshots, control-gate order
and closed ownership records. The [native owner result](records/native-result-r2.json)
contains the complete 32-profile matrix; [producer result](records/producer-result-r2.json)
contains actual argv, dependencies and outputs. The
[first partial readback](records/independent-readback-r1.json) and
[first native failure](records/native-result-r1.json) are retained explicitly.

[CURRENT_SOURCE_ASSOCIATION.json](CURRENT_SOURCE_ASSOCIATION.json) distinguishes
the frozen build tree, actual selected standalone controller and current
English documentation revisions. The source-r1 controller snapshot was not the
selected executable controller. Existing SDK, application and peer build inputs
are unchanged; current controller/guide revisions are separately identified.
Current policy code exactly matches the A13 proof of 48 previous profiles,
16 explicit split-topic profiles and 48 refusals. That proof is reused with
verified identical source, not counted as newly executed model cases.

[The complete English ROS1 tutorial](CURRENT_GUIDES/sdk/examples/moonbit_image_pipeline/ROS1.md)
explains build commands, sysroot/native dependency selection, separate grants,
SDK calls, the finite S/P/I/A/Q protocol, Master/TCPROS checks, sequence
semantics, existing native libraries and cleanup. The frozen
[Image reference](CURRENT_GUIDES/sdk/moonbit/IMAGE.md) describes strict canonical
layout and bounded ownership. Active ROS sources are in sdk/examples and tests;
archived C/C++ copies have `.source` suffixes so root xmake does not discover
them as test targets.

## Open original requirements

There is **no new formal timing, VTune collection, debugger workflow, QEMU or
cache/CPUID result** in this checkpoint. Raw memory still failed the strict
60 GiB performance prerequisite at its recorded observation; the benchmark lock
was not acquired and no profiler/benchmark target was launched. The parent
codec workload has fifteen timings and one Native hardware Hotspots profile;
its roughly 3.8× gap to Native and missing microarchitecture/other hardware
profiles remain open. Codec timings do not measure ROS pipeline latency.

Complete rcl/rclcpp/rclpy/rcljava semantics, every language and vendor,
physical devices, mixed bridges, native-relative ROS experience, full managed
lexical debugging and the complete architecture/cache/security matrix remain
unqualified. Small finite WasmGC scenes do not close the earlier large
imported-GC allocation/root-accounting limit. Keep Host/root admission intact.
The fifteen language debugger adapters remain default-on; disabling one changes
neither legitimate Wasm execution nor ROS/WASI authority. Keeping all enabled
is recommended. All fifteen original complete requirement bundles remain open.

After archive recovery and byte verification in all three repositories, only
the ended owner’s private policy/config copies are eligible for cleanup.
Primary reports, products, source snapshots and Linux proof records are kept.
