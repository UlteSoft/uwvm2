# TinyGo SDK resources: actual Linux qualification

This dated increment qualifies TinyGo0.40.1/LLVM20.1.1 with Go1.25.14 for
the finite SDK workflows below. Every compiler, validator, target, debugger
and native actor ran through SSH Linux in the original existing64GiB,
swap-zero cgroup. Local work is source authoring and inert transport only.
The original all-language requirements remain incomplete: this packet does
not change the original audit into a full15-language success claim.

## Actual completed matrix

| Scope | Completed evidence |
| --- | --- |
| Main producers | 30 actual TinyGo Wasms: ROS1/ROS2 × `0,1,2,s,z` × resource application, full raw ABI coverage and SDK tests |
| Validation | Every main Wasm passes full `wasm-tools validate --features all`; all ten coverage products retain exactly38/43 ROS imports with their complete scalar signatures |
| Ordinary execution | 40 workflows on two frozen interpreter products:20 resource default-denial and20 SDK-test executions |
| SDK tests | Each Wasm test execution passes25 top-level groups and57 nested wire-vector cases; the native Go model invocation passes36 top-level groups plus57 nested cases |
| Refusals | 60 exact exit2 argument refusals; two independently observed expected callback-panic traps |
| Startup subtraction | 16 actual observations: two runtime products × eight entropy/clock profiles; this exact resource application reaches default denial with no extra WASI grant |
| Real ROS2 | 50 positive scenes: two runtime products × five optimizations × subscribe, Trigger, SetBool, guest server and parameter |
| Native negative controls | Four real wrong-value/semantic-status cases; guest exit1 with primary22, cleanup0 and no success marker |
| Partial construction | Ten serve-only cases; missing respond13, one acquisition and one retirement, session remains usable until explicit shutdown |
| Source debugger | Three actual LLVM products: source breakpoint/backtrace, repeated `state.Code` field13, source step, stale-stop refusal and normal completion |
| Debug selection | Eight ordinary pure codec executions, including both backend-tag artifacts on the ROS product with all language adapters disabled |
| Current raw sources | All15 routes,891 backend-qualified declarations, complete C dispatch and Python generated call shapes pass the source-only contract check |

The selected native platform is x86_64 Linux with ROS2 Humble,
`rmw_fastrtps_cpp` and the independently compiled native witness. Native
scenes create162 business actors plus54 policy preparation processes.
The rollback matrix adds20 business actors plus10 policy preparations.
The initial five-scene smoke run is retained separately; it does not inflate
the50 final positive combinations. ROS1 compilation/default denial is
distinct from native ROS1 delivery.

## Changes exercised

Factory failures previously discarded rollback notification errors. The
TinyGo SDK now preserves the original operation failure and every failed
rollback outcome in `ConstructionError{Primary, Cleanup}`. Successful
rollback preserves the original error identity. Local owners retire before
their one notification; errors never trigger automatic cleanup replay.
Model tests exercise full-width handles, ordered failures and one-shot
retirement. Actual serve-only policy tests independently exercise successful
rollback after a denied second acquisition; they do not inject failed native
cleanup notifications.

TinyGo0.40.1 actually panicked inside reflective `errors.As` in the first
resource application. `ErrorCode` now extracts SDK statuses without reflection,
following at most32 SDK Primary links and failing22 for foreign/typed-nil/
cyclic or deeper error chains. Cleanup outcomes remain separately visible.
The revised application, tests and complete compiler matrix execute this
implementation. No ROS ABI or runtime authorization default changed.

The real application uses existing Session, Node, subscription, service and
parameter APIs. Its native server callback attempts Session.Close and checks
Conflict17 before any resource retires. It then sends the complete reply.
Every positive/negative path checks the ungranted alias, preserves business
errors, inspects all normal cleanup outcomes and rejects retained-node
factories after shutdown.

## Native evidence

The independent native C++ witness checks the complete String/Boolean value,
namespace, selected Gateway graph/type, one request/publication, nonzero
subscription GID, native-server writer GUID/sequence or native-client request
sequence, two seconds quiet and Gateway
retirement. Actual subscription QoS reliability/durability is recorded.
Fast DDS reports unavailable history/depth as UNKNOWN/0; this is retained
instead of fabricating KeepLast4 observation. The pinned witness source
enforces exposed values when available.

Each held Worker is bound by its actual UID/PID before its one deferred exec.
Policies grant only the selected aliases. The private socket is0600 in an
owned0700 directory. Native service/parameter deadlines remain3000ms and
the separate Gateway frame deadline15000ms. Wrong-value controls alter
complete native data or return success=false; no deadline is enlarged to
turn a failed scene into a pass. Native executable mappings, selected SDK
libraries, complete outputs and natural retirement remain in the receipts.
Process creation is sampled with retained pidfds; a detached helper escaping
every observed ownership anchor is a stated monitoring limitation.

## Debug and panic boundaries

The first minimal source probe hits its source breakpoint but its local
`code` value is unavailable. That original failure is retained. The final
probe passes the actual codec result through a state record to a separate
non-inlined function and qualifies `state.Code` inspection there. It proves
that finite field/source workflow, not arbitrary TinyGo locals, all optimized
locations, goroutine debugging or native ROS callback debugging.

The full native Go model suite includes a native-only panic/recover fixture.
TinyGo `-panic=trap` intentionally cannot recover it. The first unfiltered
Wasm run traps at that fixture; supported Wasm test executions explicitly
exclude it and two separate runs require its exact trap. A trap is never
counted as successful recovery or successful cleanup. The standard testing
runtime also requires a scheduler: `scheduler=none` is rejected by the
compiler, so the test harness uses `asyncify` with explicit timing grants.
The resource application stays caller-driven with `scheduler=none`.

All15 language debugger options default ON; keeping them ON is recommended.
They select host source-debug support and do not form an execution-language
allowlist. The exact xmake switches, macros and trimming examples are in
`source-current-guides/docs/debugger/language-options.md` and the active
repository guide.

## Resource application Wasm sizes

These are actual unstripped producer bytes, including retained metadata;
they are not the host UWVM executable size or performance measurements.

| Optimization | ROS1 bytes | ROS2 bytes |
| --- | ---: | ---: |
| `0` | 621400 | 622427 |
| `1` | 401652 | 401661 |
| `2` | 390511 | 390520 |
| `s` | 324960 | 324969 |
| `z` | 309484 | 309493 |

## Performance and remaining requirements

The same-source native Go/TinyGo SDK codec benchmark and four Wasm probe/
benchmark artifacts are actually compiled and validated. Its gated protocol
checks the full decoded data, iteration count and independent LCG seed.
The new timing/VTune cohort has not qualified a performance result. The
calibration obeys the existing serialized benchmark lock and the separate
strict60GiB memory admission guard. Occupied-lock failures/waits and the
subsequent capacity refusal are retained; no target/collector was started.
No lock holder is interrupted or bypassed and no guard is relaxed.
See `performance-coordination.json` for the publication-time state.
No new native ratio, hardware Hotspots, Microarchitecture, WAVM or WasmEdge
comparison is claimed by these functional results.

No new QEMU/platform matrix, cache-update/binary-SHA/CPUID test, managed SDK
parity, all-vendor integration or general sandbox proof is added. Existing
native `.so`/`.a` libraries still need source porting or an explicitly
authorized native Gateway boundary. The English resource tutorial explains
the build, API, policy, application protocol and these boundaries in detail.

## Verification and transport

`independent-readback-r2.json` comes from a separate inert reader. It parses
actual Wasm type/import sections, compares the raw ABI, reads complete logs,
checks exact Cartesian scene matrices/values and verifies process retirement.
It never imports archived Python or executes Wasm/native code. Its retained
Linux invocation is in `independent-readback-r2.log`. The first reader's
overbroad writer-GUID requirement for the native-client case is retained as
a reader failure; the corrected reader checks that case's actual positive
request sequence and explicitly unavailable writer GUID.

`publication-manifest.json` binds every public member by original relative
path, size and SHA256. Native C++ source payloads have `.cc.source` names so
the root xmake `test/**.cc` scan cannot create unintended build targets;
`source-transport-map.json` preserves their exact original names and bytes.
Large shared runtimes, native SDKs and compiler installations remain at their
pinned retained Linux locations, rather than being duplicated into three
repositories. Historical failed producer/guard/DBG/harness attempts remain
separate from accepted rows. No archive script runs during transport checks.
