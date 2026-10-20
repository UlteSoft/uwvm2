# Fifteen-language source ABI integration continuation — 19 October 2026

The original audit's **business/client source-association failure is repaired**.
The normal ABI extractor and expanded binding checker now pass on the exact
frozen current Host. The check covers the raw interfaces used by all fifteen
language routes, with shared bindings counted once.

This closes one source-integration gap. It does not implement the remaining
high-level SDKs, managed debugger bridges, real ROS applications, platform/cache
matrix or native performance requirements. The original fifteen-language
acceptance bundle remains incomplete.

## Changes

- Keep the complete historical `sdk/abi/abi-v1.json` unchanged.
- Associate `service-client-v1.json` with the current whole `host.h`; retain the
  immediately previous association. All other client-catalog fields stay equal.
- Add `sdk/tools/rebind_service_client_catalog.py`. Both input SHA-256 values
  are mandatory. The tool validates a candidate against unchanged original
  tuples/metadata and exact optional signatures in private scratch, rechecks
  source bytes and writes a fresh file. It never overwrites the checkout catalog.
- Extend the existing formal `check_bindings.py` entry point to official Go,
  Zig, Java/Kotlin, MoonBit, C#/F#/VB and the C# C forwarder.
- Check Python's generated ROS1/ROS2 dispatcher names, actual backend/callee,
  argument expression order and output-kind selection against the checked C
  headers. This is a finite source-call parser, not a CPython target type checker
  or buffer-lifetime proof.
- Add regression groups for narrowed offsets, malformed/duplicate imports,
  backend/symbol mismatches, reordered forwarding arguments, Python dispatch
  and output drift, and guarded catalog refresh.
- Correct the English source-update guide to preserve the original catalog,
  explain shared language routes and keep source results separate from target
  compilation and native/DBG qualification.

Only the ROS checkout contains these ROS SDK implementation files. This report
and its evidence packet are synchronized byte-for-byte to uwvm2, uwvm2-stable
and uwvm2-ros. No ROS SDK dependency is added to ordinary/stable VM builds.
The fifteen debugger options remain default-on; legitimate Wasm execution
remains independent of adapter trimming.

## Actual SSH Linux results

Every check and CLI subprocess ran inside the original existing cgroup:

`/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope`

The bracket verifies boot
`51c27aa1-d709-4a06-ba16-2ac3003fbcbd`, cgroup device/inode29/19075,
64GiB maximum, zero swap and CPU15. Source readers use a512MiB address-space
limit. No compiler, Wasm application, native ROS peer, debugger, VTune collector
or QEMU process was started by this source-only cohort. Mac operations were
inert source authoring, hashing, transport and publication.

| Check | Actual result |
| --- | --- |
| Stale catalog on the frozen current Host | Original failure reproduced and retained |
| Normal ABI extractor after repair | PASS; historical38/43 catalog bytes unchanged |
| Expanded normal binding CLI | PASS;891 declaration entries across22 backend/binding rows |
| Python generated dispatcher | PASS;38 ROS1 +43 ROS2 call expressions |
| Source association and all-language regression suite | 18 groups passed;20 recorded backend/path subcases; no failures/errors |
| Independent existing C dispatch mutation CLI | 170 cases passed, including refusal of wrong same-signature slots |
| Actual CLI processes | Seven passed their expected outcomes: normal checks/candidate generation plus overwrite/wrong-Host/wrong-catalog refusal |
| Cgroup memory events | Unchanged during both source and CLI brackets; existing max1, OOM0, OOM-kill0 |

The891 declarations include shared C/C++, Java/Kotlin, C#/F#/VB and the
C-forwarder. They are not891 target executions. Python's81 source call shapes
are separate. The normal entry point is already invoked by the SDK source
aggregate, so these new declarations no longer depend on an ad-hoc audit reader.

The20 subcases belong to the18 test groups; do not add them as independent
compiler/Runtime workflows. Likewise the170 C dispatch mutations are source
models, not170 real ROS indirect calls.

## Input and catalog identities

| Item | SHA-256 |
| --- | --- |
| Whole current Host | `e9363601b000098c3339194d39d9f7c2f5bfbb1bca77ca95a3fd56a3e344434a` |
| Complete unchanged historical ABI catalog | `0a13f6c49100173c156eb76acfd35ebe103a40b2a90c752d3d1302c31f875ec0` |
| Client catalog before repair | `84c1f6ae15bda8a0f5fcc19148535b8fb6b131823165c52437e3bdc313bbd784` |
| Validated/installed client catalog | `ffead68d969f02759d1fbf1327d88b112e727c3446a8bd813b12183051146bf6` |
| Source regression result | `e929ab4d8d259aa06b5fd4a539dfa6ae4fa9372390dde1b60521226c04fd4b5b` |

The original32-file snapshot and repaired tree are retained on Linux at:

`/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-abi-integration-20261019-a2`

The source regression driver and CLI driver retain their exact executed bytes.
The source-validation guide received one later documentation-only correction:
the actual standalone tools use `-S -B`, retaining their script-local imports,
rather than isolated `-I` which removes that path. This corrected guide and
the existing C dispatch peer script are separately bound in
`supplemental-inputs.json`. The original snapshot is not edited.

## Evidence and reproduction

- [Source inputs](inputs.json), [supplemental inputs](supplemental-inputs.json).
- [Normal binding result](bindings.json).
- [Source regression result](result.json) and [complete log](regressions.log).
- [Actual CLI records](cli-result.json) and [C dispatch cases](cli/C-dispatch-peer.json).
- [Old catalog](catalog.before.json) and [validated catalog](catalog.candidate.json).
- Exact [source driver](driver.py) and [CLI driver](cli-driver.py).
- [Independent inert readback](independent-readback.json).
- [Machine-readable continuation](continuation.json).

For a new source check, use an admitted Linux source snapshot and a new owned
output path:

```sh
python3 -S -B sdk/tools/abi_contract.py
python3 -S -B sdk/tools/check_bindings.py --report "$OWNED_OUTPUT/bindings.json"
```

Catalog refresh uses both explicitly reviewed input identities; see the
[source validation guide](../../../../uwvm2-ros/sdk/docs/source-validation.md) in the ROS
checkout. Its source-association candidate changes no imports or permissions.

The captured drivers deliberately use fresh fixed output paths and cannot be
rerun over their existing receipts. A new regression run needs a fresh owned
cohort directory and a newly recorded driver/input identity. The independent
readback reads the old evidence as data and never executes its archived sources.

## Capacity and remaining work

An inert resource inventory found about48.3GB shmem/tmpfs charged to the shared
group. The owned `/tmp/root-language-*` trees total1,687,670,784 allocated bytes;
948,277,248 bytes are the r136 products/dependencies required by active language
queues. Most remaining owned bytes are reports/receipts. Deleting those copies
would not supply the missing capacity and could break active provenance inputs.
No unrelated data, active dependency, process, cgroup limit or global cache
setting was changed.

The official Go, TinyGo/Kotlin and F#/VB large-DBG queues retain their own
resource admission and completion status. This source cohort grants them no
new success credit. No new native speed/size, hardware profiling, cache or
platform result is claimed.

The other optional RMW publisher/subscription/service catalogs still retain
their independent Host/implementation/raw-header associations and need their
own reviewed current-source closure. This business/client repair does not
silently update those hashes or qualify the complete SDK source aggregate.

The [original full requirements audit](../REQUIREMENTS_AUDIT_20261019.md) remains
an immutable dated snapshot of the pre-repair state. Read this continuation for
the repaired source-association/checker item. Its other findings remain open:
complete high-level Go/Zig and other client surfaces, original ROS compatibility,
managed language DBG, actual native ROS/vendor applications, complete consumer/
QEMU/cache/security matrices and reliable all-language native/engine profiling.
