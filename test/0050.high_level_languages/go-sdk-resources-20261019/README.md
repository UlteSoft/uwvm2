# Official-Go ROS SDK resource continuation — 19 October 2026

Official Go now has bounded resource conveniences beyond its original String
publisher. The final source produces twelve actual Wasms, completes24 ordinary
Runtime workflows and four independently witnessed native ROS2 publications.
This is a concrete SDK increment; the original fifteen-language requirements
remain incomplete. The preceding [full audit](../REQUIREMENTS_AUDIT_20261019.md)
and [ABI integration repair](../abi-integration-20261019/README.md) retain their
original evidence and conclusions.

## Implemented behavior

- Owner copies share retirement and a nonblocking operation lease. Overlapping
  calls return17, with rollback when the second of two owner leases is busy.
  Close/revoke retire every copy before the one possibly effectful import.
- Official-Go backing objects are pinned before address-to-integer conversion
  and retained through each synchronous import. All offsets, sizes, handles
  and request IDs preserve their u64 ABI width.
- Topic publication/subscription, legacy/versioned polling, logging, service
  advertise, parameter-event activation, event watch/readiness and asynchronous
  service submission have explicit methods. Pair methods reject the same owner.
- `Request` names33 RPC operations:25 exist on ROS1 and29 on ROS2. An absent
  backend operation returns95 before allocation/import. The Host still checks
  role, type, resource, ownership, quotas and audit policy.
- Failed output is never exposed as data. Error90 distinguishes a genuinely
  written required length, including zero, from an untouched all-ones sentinel.
  No implicit growth, effect replay, timeout retry or finalizer is introduced.
- A bounded Session reserves ownership slots, rolls back busy close preflight,
  attempts every live close in reverse slot order and preserves all per-slot
  errors alongside the primary result.
- Finite codecs cover strict canonical String, Trigger/SetBool responses,
  nonzero full-width Trigger request IDs and Trigger responses. Complete
  versioned-event association validation and other typed records remain work.
- A real official-Go application adds subscription, Trigger/SetBool clients,
  Trigger server and String parameter lessons. Its policy tool selects exact
  aliases using the shared bounded SDK policy implementation. English API,
  build, permission, native-peer, scheduling and cleanup tutorials are included.

Implementation and SDK documentation live in the ROS checkout. This acceptance
packet is synchronized identically into uwvm2, uwvm2-stable and uwvm2-ros; it
does not add a ROS dependency to ordinary/stable builds. No VM C++ code or
language-debug option changes are part of this continuation. All15 debug
adapters remain default-on. Trimming an adapter must still leave legal Wasm
executable; keep all adapters enabled unless there is a demonstrated need to
trim debugging support. Backend Go build tags select ROS imports and do not
grant authority or replace those Runtime debug options.

## Actual SSH Linux results

Every compiler, validator, policy CLI, target execution and native actor ran
inside the original existing Linux cgroup:

`/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope`

Each bracket verifies boot
`51c27aa1-d709-4a06-ba16-2ac3003fbcbd`, cgroup device/inode29/19075,
hard68719476736-byte maximum and zero swap. Controllers/readers use CPU15,
Go/native compilation CPU18 and ordinary target/native actors CPU20. All work
is serial within each controller. Shared-host execution is not a performance
isolation claim. Mac operations were source authoring, hashing, transport and
publication only. No other agent was contacted.

| Check | Actual outcome |
| --- | --- |
| Final official-Go builds | 12 PASS: ROS1/ROS2 × Release/Debug × SDK tests/publisher/resource application |
| Full validation | All12 pass wasm-tools1.262.0 `validate --features all` |
| Actual binary import signatures | All12 pass; SDK test products contain all38 ROS1 or43 ROS2 functions; other apps use subsets |
| SDK tests in actual UWVM | Eight runs ×11 complete groups =88 group executions, all pass |
| Original publisher default denial | Eight actual runs, exact marker and errno13 |
| Resource application default denial | Eight actual runs, all six aliases refused13 and exact marker |
| Resource CLI errors | 16 actual runs reject unknown mode/extra argument with exit2 |
| Invalid Go backend selection | No tag and both tags each fail actual compilation; no Wasm output |
| Explicit resource policies | 10 actual generator CLIs and10 Runtime syntax checks pass; five lessons ×two backends |
| Policy input errors | Three actual CLIs reject unknown lesson, invalid deadline and extra alias with exit2 |
| All-language source ABI integration | PASS:891 declarations/22 binding-backend rows plus separate Python source-call checks on the frozen repaired Host/catalog with final Go overlaid |
| Actual native ROS2 publication | Four PASS: both Go producer profiles ×both frozen Runtime build modes |
| Native actor retirement | All12 processes exit0 with complete logs, no forced signals, no retained pidfds; private sockets and short scratch directory removed |
| Cgroup memory events | Unchanged in every passing bracket; existing max1, OOM0, OOM-kill0 |
| Independent inert readback | PASS over final source/product/log/command/policy/native evidence |

The final24 workflows comprise eight SDK test runs and sixteen default-denial
application runs. The88 groups belong to those eight runs; do not count them
again as independent Runtime workflows. Earlier eight-build/sixteen-workflow
results are retained on Linux but are superseded by the final12/24 matrix;
do not add the earlier matrix to inflate acceptance.

The test groups check actual ABI/default denial, alias validation, canonical
codecs and bounds, full-width IDs, output quarantine/sentinels, zero/busy owners,
actual Host refusal of deliberately invalid synthetic handles, all RPC/backend
branches, pair rollback,32-goroutine lease exclusion and session bookkeeping/
retirement. Synthetic handles occur only in tests; their refusal is not proof
of a positive native resource. Codec/sentinel/local-state cases exercise SDK
logic inside the actual Wasm test binary and are not native middleware cases.

The two explicit WASI clock grants used here are monotonic-clock and
realtime-clock. They grant no ROS alias, filesystem or network access. These
are selected fixture permissions, not Runtime defaults or a claim that every
Go application needs both. Preserve the existing zero-grant startup-refusal
qualification separately.

## Native witness and producer boundaries

The original publisher executes its actual business logic inside Go Wasm.
The owner binds the authenticated native Gateway to the actual held Worker
PID/UID, a private0600 socket, a unique per-case namespace, explicit domain193,
`roswasm_language_gateway`, exact String identity, reliable/volatile depth4 QoS
and finite byte/rate/queue/audit budgets. The disabled-test ROS2 profile is
explicit; it is not SROS2 Enforce qualification.

The distinct warmup materializes the Gateway publisher. An independently
compiled native rclcpp subscriber verifies node, namespace, type, QoS and a
nonzero writer GID before the trusted owner sends `P`. It then requires exactly
`bounded WasmROS telemetry` from that same GID before `A` allows Guest cleanup.
The peer requires two seconds quiet and publisher count zero. Every case also
checks the Guest's ungranted alias refusal13, complete stdout/audit output and
normal process/socket retirement. The native library mappings are inspected
against the selected SDK; file hashes are rechecked afterward.

Five native-witness compiler failures are retained before the final passing
build. The fixes restrict the include set to actual ROS package roots, include
the required C String declarations before Humble headers, and supply explicit
SDK library search/direct dependencies. No assertion was removed and no
unresolved symbol or foreign library was allowed to manufacture success.
The witness uses the pinned Humble SDK loader; the selected historical Gateway
origin retains its original overall FAIL. This continuation independently
qualifies only the actual selected String path, not that old whole cohort.

The final native witness build's sampled compiler-group RSS peak was about
722MB. Initial Go SDK compilation sampled about214MB; the final cached builds
were smaller. The native actors sampled under100MB combined. Sampling can miss
short-lived helpers or peaks, so these are observations rather than exact
maximum-memory guarantees. Native/ordinary outcomes are not VTune results.

## Runtime identities and size

These are restored and rehashed frozen default-all interpreter products,
not a new build of the current dirty VM checkout. Their original build receipts
record native ROS1/ROS2 imports, uwvm-int, all15 adapters enabled, no LLVM JIT,
no WSEA and no LTO. Restoration checks every143 retained archive object and
the complete gzip stream. Current SDK Go source is compiled separately.

| Executed Runtime | Raw ELF bytes | SHA-256 |
| --- | --- | --- |
| uwvm2-ros Release | 15442000 | `719e1cb683abeed7710d54ae705edc3e3ddd0048daf91cdbfe5a1ae6a9c26c19` |
| uwvm2-ros Debug | 242820864 | `2bef37516ecc05a4f6e56124a1c5e8b011c6f725888ca529c4ecceb77936f084` |

The Go resource application is2509438/3005907 bytes for ROS1 Release/Debug and
2512565/3009989 bytes for ROS2. Full identities for all12 products are in
`producer-final-result.json`. These Wasms are external Guest applications;
their bytes are not added to the VM ELF.

## Debugging and remaining acceptance

Actual binary parsing finds only `go:buildid`, `producers` and `name` custom
sections in all12 Go1.24.2 products. There is **no `.debug_info` or `.debug_line`**.
`-gcflags='all=-N -l'` and a Debug Runtime therefore establish a build/runtime
configuration, not usable Go source-language debugging. This is recorded in
the Go API documentation and `custom-section-inventory.json`.

The new resource application's subscribe/Trigger/SetBool/server/parameter
positive native modes remain unqualified. Their source, builds, default denial
and policy syntax pass; the four native passes apply to the original publisher
only. ROS1 native Go delivery, typed Action/executor/event validation, complete
custom vendor records and upstream source-compatibility layers remain work.
The Go SDK is not complete rcl/rclcpp/rclpy/rcljava parity.

This continuation adds no source DBG, hardware Hotspots/microarchitecture,
native/Wasmtime/WAVM/WasmEdge performance, cache/CPUID/Wasm3 compatibility or
QEMU/Windows/other-architecture credit. The full fifteen-language option/build
matrix and outstanding queues retain their previous separate status. Existing
binary-SHA cache work is not requalified by a successful SDK test.

## Evidence and reproduction

The authoritative Linux cohort is:

`/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-go-sdk-20261019-a3`

The packet contains exact executed final drivers, their complete raw outputs,
policy JSON/configuration, binary inspection reports, current source manifest,
native compile dependencies/failures, selected Runtime origins and independent
readback. Large compiler prefixes, Wasm/ELF products and private reusable
compiler cache remain on the Linux backing volume. No foreign files, process
queues, cgroup settings or system caches were removed/changed.

Use the current ROS checkout's `sdk/go/README.md` and
`sdk/examples/go/resources/README.md` for build/API/native-peer recipes.
`verify_go_sdk_resources.py --root <Linux-cohort>` is an inert readback: it
requires the original Linux cgroup and starts no compiler, target or peer.
The final readback follows a documentation-only clarification about missing
Go DWARF; its predecessor and original source snapshot remain retained.
`packet-manifest.json` binds every published evidence file. Preservation is
not a replacement for repeating native/DBG/performance acceptance after a
future compiler, VM, Host/Gateway, policy or SDK change.
