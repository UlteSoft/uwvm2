# Official-Go native ROS resource matrix — 19 October 2026

The official-Go resource application now passes real ROS2 subscription,
Trigger/SetBool client, Trigger server and String parameter scenarios in all
four selected producer/Runtime build combinations. Four real native wrong-value
controls also pass. This closes the finite positive-native gap left by the
[preceding SDK continuation](../go-sdk-resources-20261019/README.md).
The original fifteen-language SDK, source-debugging, performance and platform
requirements remain incomplete.

## Changes and actual execution

The independent native witness is
`tests/ros_language_integration/go_resource_peer.cc`. It uses real rclcpp/DDS
resources and generated ROS types, complete values, native graph identity,
nonzero GUID/sequence or subscriber GID, one-operation counts, owner
acknowledgements, quiet time and Gateway retirement. It is never linked into a
Guest or substituted for a Host import.

The Go application now writes fixed control markers directly and checks the
complete write. A partial/failed success marker cannot yield success. Numeric
primary/cleanup diagnostics preserve their schema and error exit. The SDK
library implementation is unchanged from the earlier 88 test-group matrix;
its source identities are checked against that snapshot. The changed
application is newly compiled and validated.

Every compiler, validator, policy CLI, Runtime and native actor runs inside
the original existing 64 GiB, zero-swap Linux cgroup:

`/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope`

Brackets check boot `51c27aa1-d709-4a06-ba16-2ac3003fbcbd`, cgroup
device/inode 29/19075, memory.max 68719476736 and memory.swap.max 0.
Controllers/readers use CPU15, compilers CPU18, ordinary checks CPU19 and
native-scenario actors CPU20. No cgroup setting, system cache or foreign
process/file was changed. Mac operations are source editing, inert hashing,
transport and publication. No other agent was contacted.

| Check | Actual result |
| --- | --- |
| New Go builds | Four pass: ROS1/ROS2 × Release/Debug, Go1.24.2 |
| Full validation and import signatures | All four pass wasm-tools1.262.0 and exact selected-backend import checks |
| Native witness compile/link | Pass with Clang21, selected Humble sysroot/GCC11 runtime and recorded compiler dependencies |
| Positive native ROS2 matrix | 20 pass: five lessons × two Go profiles × two frozen Runtime modes |
| Native wrong-value controls | Four pass on Release/Release: subscription, Trigger, SetBool and parameter |
| Native scenario actors | All 72 normal expected exits, complete logs, closed pidfds and no forced signals |
| Policy generation | 24 actual CLIs pass, binding the actual held Worker PID/UID and exact aliases |
| Ordinary clock profiles | 32 checked: 16 actual ROS-default-denial passes, 16 WASI clock-startup refusals |
| Argument refusals | 16 actual runs exit2 with no success output |
| Independent inert readback | Pass across source/tool/product hashes, raw outputs, policies, native protocols and retirement |
| Memory events | Unchanged in each passing bracket; existing max1, OOM0, OOM-kill0 |

The 20 positives are the full Cartesian product of `subscribe`, `trigger`,
`set-bool`, `server`, `parameter`; Go Release/Debug; and Runtime Release/Debug.
Do not add smoke results, actor counts or the preceding publisher matrix to
inflate this total. The 72 scene processes are 24 Workers, 24 Gateways and 24
peers; policy generation adds 24 separate administrative processes.

Final compiler-group RSS samples peak at 625074176 bytes for the native
witness and 202620928 bytes for Go. Scene actors sample 95305728 bytes combined.
The final native compiler guard is 768 MiB, the combined scene-actor guard is
1 GiB, shared raw current stays below 62 GiB, and current-minus-inactive-file
below 56 GiB. Sampling can miss short helpers/peaks; these are observations,
not exact maxima, exclusive-host claims or performance measurements.

## Real native operations

| Lesson | Complete actual operation |
| --- | --- |
| Subscribe | One native String `bounded native Go telemetry`; selected subscriber node/namespace/type, reported reliable/volatile QoS and nonzero GID; Go decodes the full value |
| Trigger client | One empty native request with nonzero GUID/sequence and selected client graph; response success=true and `native-go-ready` |
| SetBool client | One native request data=true, GUID/sequence and graph checks; response success=true and `native-go-enabled` |
| Trigger server | Native client discovers the selected service, submits one real sequence/future and receives success=true plus `bounded go wasm service` through Go's separately granted responder |
| Parameter get | One generated GetParameters request for exactly `value`; actual declared native String returned as typed ParameterValue; Go decodes `bounded native Go parameter` |

Parameter mode disables automatic parameter services and provides one counted
generated GetParameters service over an actual declared String. This makes
name, type, value and request count independently checkable; it is not every
upstream parameter service.

Each case retains ungranted-alias denial13, complete Guest output and Host
audit checks, primary/cleanup outcomes, at least two seconds quiet and selected
Gateway-node disappearance. Subscription also requires subscriber count zero.
Private sockets and short owned scratch directories retire. Native service
graph/type, request GUID/sequence and future association are finite observations;
disabled-test DDS supplies no cryptographic peer-authentication claim.

The wrong-value controls send one bounded unexpected String or a SetBool
response with success=false. Guest must exit1, preserve exactly
`primary 22 cleanup-status 0`, print no success marker and close its resources.
Only then does native-owner byte `X` admit the peer's count/quiet/retirement
completion. No second message, different alias, fallback or synthetic Host
reply is substituted.

## Scheduling, QoS and original failures

The same held Worker PID is used for policy generation and Runtime exec.
Grants fix operation, resource, type identity, QoS and finite budgets.
Scenes use unique namespaces, explicit domain194, `go_resource_gateway`,
private 0600 sockets and an explicit disabled-test profile. Guest gains no
policy-management, debugger/update, host-network or directory authority.

Service/parameter grant budgets remain **3000ms**. The separate Gateway
frame/idle budget is 15000ms with max-sessions2 for preflight/business
retirement. Peer and owner actor bounds are 90s and 100s. These scheduling
bounds do not extend expired native requests, and no timeout is replayed.

The original compiler error and three failed smoke cohorts remain preserved.
The compiler rejected an ambiguous raw character pointer in test logging;
the correction uses the explicit OS-string wrapper. The first smoke wrongly
required remote discovery to report history/depth. Fast DDS
[RTPS QoS conversion](https://github.com/ros2/rmw_fastrtps/blob/humble/rmw_fastrtps_shared_cpp/include/rmw_fastrtps_shared_cpp/qos.hpp)
documents that those remote fields are absent. Final observed UNKNOWN3/depth0
is recorded as unavailable, never relabelled as observed depth4. Reported
reliable/volatile and nonzero subscriber GID are required. If history/depth
are reported, they must match; keep-last/depth4 stays explicit in policy.

The other failed smokes retain real service.respond110 results. Their owner
waited for native send-return observation before allowing Guest polling,
leaving an approximately five-second scheduling/observation gap. The corrected
owner issues native `S` only after Guest readiness/native discovery and drives
bounded Guest `P` credits alongside the send. Both actual Go reply and complete
native response are required before separate `A` acknowledgements. A selected
diagnostic measured the native send call itself at about 1.24ms; the earlier
gap must not be attributed wholesale to that API or Go. Remaining control and
discovery costs require profiling. The original three-second budget is retained.

Forced failed-run cleanup is not successful retirement credit. Original logs
remain unchanged; separate receipts remove only closed owned sockets and
empty scratch after recorded pidfds close. The final 24 scenes need no signals.

## Startup, size and remaining work

For the changed denial-mode app, monotonic-clock alone and both clocks pass
on all eight producer/Runtime pairs. Realtime-clock alone or no clocks refuse
at actual `clock_time_get` startup. These 16 startup outcomes are not ROS refusal
credit. Positive native fixtures explicitly retain both clocks; denial-mode
subtraction is not universal Go/GC permission qualification.

| Guest | Earlier raw bytes | New raw bytes |
| --- | --- | --- |
| ROS1 Release | 2509438 | 2114570 |
| ROS1 Debug | 3005907 | 2439464 |
| ROS2 Release | 2512565 | 2117726 |
| ROS2 Debug | 3009989 | 2443423 |

This saves about 15.7% Release and 18.8% Debug Guest bytes. Exact hashes are in
`size-comparison.json`. No VM executable code is added. Executed frozen
Runtime raw ELFs remain 15442000 bytes Release and 242820864 bytes Debug,
rehashed against their earlier origins. They include ROS1/ROS2 and all 15
default-on language adapters, use uwvm-int and have no LLVM JIT/WSEA/LTO.
They are not new builds of the current dirty VM tree.

The four new Wasms contain only `go:buildid`, `producers` and `name` custom
sections, with no DWARF source-debug sections. Debug configurations do not prove
Go source debugging. This cohort adds no VTune hardware Hotspots/microarchitecture,
native/Wasmtime/WAVM/WasmEdge speed, compiled-cache/CPUID/Wasm3 matrix,
ROS1 native, QEMU/Windows or other-architecture qualification. Full
rcl/rclcpp/rclpy/rcljava parity, all RPC/Action/vendor records and the original
fifteen-language requirements remain open. All 15 debug options stay default-on;
trimming an adapter must still permit otherwise valid Wasm. Keeping adapters
enabled is recommended.

## Evidence

The authoritative Linux root is:

`/var/tmp/uwvm-ros-complete-20261015-r1/acceptance-storage-r1/root-language-go-resource-native-20261019-a4`

The current ROS checkout's `sdk/examples/go/resources/README.md` provides
complete build, policy and two-owner-pipe protocols. This packet retains exact
executed drivers, final/failed complete logs, product/import identities,
native compiler dependencies, source snapshots and policies. Large compiler
prefixes, native SDK, Wasm/ELF binaries and reusable cache remain on the Linux
backing volume.

`verify_go_resource_native.py --root <Linux-root>` independently reads the
hashes, outputs, values, matrix, grants and normal retirement under the original
cgroup; it starts no test actor. `packet-manifest.json` binds the evidence copied
identically into uwvm2, uwvm2-stable and uwvm2-ros. ROS implementation stays in
the ROS checkout and adds no ordinary/stable build dependency. Earlier dated
reports remain immutable historical records.

Archived C++ payloads use `.cc.source` names with an exact byte/hash/original-path
map, preventing the repository's `test/**.cc` discovery from treating them as
unit targets. An actual Xmake3.0.5 Lua regression finds ten unintended archival
sources before packaging and none afterward, with a real `.cc` sentinel still
discovered. This corrects both this packet and the preceding Go packet; original
Linux publications/manifests and all target results remain preserved. Restore
logical source paths only into an isolated explicit source workspace.
