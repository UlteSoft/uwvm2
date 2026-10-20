# TinyGo self-contained example matrix — 19 October 2026

This is an **incomplete, source-bound checkpoint**, not complete language or
ROS-client acceptance. The independent inert reader qualifies 121 of 135
planned compiler products and 34 closed ordinary observations. The original
fifteen-language requirement bundle remains incomplete for every language.

All producer, validator, runtime, negative-control and reader execution took
place through SSH Linux in the original hard 64 GiB, swap-zero cgroup. macOS
was used for reading/editing source, inert hashing, transport and publication.
No other agent was contacted. Active SDK code belongs to the ROS checkout;
the identical packets in ordinary/stable are evidence, not ROS dependencies.

## Accepted scope

| Observation | Actual completed scope |
| --- | --- |
| Compiler products | 121 TinyGo Wasm files, each built, validated with wasm-tools and inspected for actual binary imports |
| Backend/optimization coverage | ROS2: all 14 entries × five profiles; ROS1: 13 entries × profiles 0/1/2, plus 12 entries at s |
| Qualified compiler/inspection/control rows | 370: 363 build/validate/import rows, four ROS1 excluded-entry refusals, one independent Go file-selection observation and two Go invalid-backend refusals |
| Complete ABI-retention fixtures | Nine unexecuted coverage modules, retaining 38 ROS1 or 43 ROS2 imports each |
| Default-disabled Guest executions | 24 on the frozen release interpreter only: 18 checked legacy application traps, four pure SDK/local successes and two successful resource default-denial commands |
| Argument refusals | Eight actual exit-2 controls with empty stdout/stderr |
| Missing-Gateway controls | Two exit-126 preflight refusals; Wasm has not started |
| New source-debug sessions | Zero; the planned five-optimization × three-product cohort was not admitted |
| New native ROS delivery / native-relative performance / QEMU / cache-update qualification | Zero |

The metadata identifies both frozen release/debug interpreters, but the accepted
34 observations execute **release only**. Metadata, a prepared command and a
successful binary hash check do not establish execution on the other product.

TinyGo 0.40.1 uses LLVM 20.1.1 and the selected Go 1.25.14 installation. The
explicit profile is wasip1, scheduler=none, panic=trap, one compiler job, and
optimization 0/1/2/s/z. Neither native Go model tests nor DWARF presence replaces
these actual producer/consumer observations.

The actual modules' imported function signatures are independently decoded by
readback-r3.py, rather than trusting the producer import report alone. The
three pure entries—scheduling, sdk_codec_debug and codec_benchmark—must import
only wasi_snapshot_preview1; they receive no ROS import/delivery credit.
coverage is never executed: its zero-placeholder export is only an import
retention fixture.

## Exact evidence and source association

The accepted producer parent is producer-result-r2.json. Its whole-cohort status
is FAILED_RETAINED because the next compiler was stopped by the shared raw
memory guard. Only its individually completed, hash-bound positive products
and valid controls are reused. producer-result-r1.json retains the earlier
excluded-file diagnostic matcher failure. Neither failed parent is relabelled
as a complete pass.

ordinary-partial-result-r4.json closes 34 individually complete observations
with status STOPPED_CAPACITY_WITH_COMPLETED_OBSERVATIONS. Its logs originate
from the explicit log_directory in each row, chiefly ordinary-partial-runs-r2
and ordinary-partial-runs-r3. Reused records retain their actual argv, PID,
start identity, process group, cgroup, exit, complete output hashes and source
product identity. A reused record is not counted as a fresh target invocation.

readback-checkpoint-r1.json records
INDEPENDENT_INERT_TINYGO_PARTIAL_EXAMPLE_READBACK_PASSED.
Its executed reader, readback-r3.py, has SHA-256
9936c86bf875589a681752b56d6f323bd580bb0ee1c10f29eb1ff1691030b574.
The reader rechecks all 65 frozen source inputs, actual Wasm imports,
qualified compiler logs, ordinary logs, runtime identities and classification.

source-current-association-r1.json associates those frozen inputs with the
active ROS checkout: all 63 other files match, and only the two declared
English README files changed. No SDK producer source code changed in this
checkpoint. The new BUILD_MATRIX.md and those README updates are separately
frozen under current-docs-r2 with their own manifest. The source-r1 copies
remain the actual original compiler inputs.

The Wasm validator was recovered after its earlier workspace path
disappeared. validation-tools/wasm-tools is byte-identical to the installed
1.262.0 binary: 20431992 bytes, SHA-256
fe6a056dc03269b06f53e0cff926101c17d4f4ea37fbaa077d049c0d18acb211.
validator-restoration-r1.json records that relocation. No substitute validator
or network upgrade was used.

## Retained failures and interpretation corrections

- TinyGo excludes the ROS2-only main on ROS1 with a package-only diagnostic.
  An independent Go file-selection observation verifies that main.go is ignored;
  a compiler exit alone is not silently counted as the intended refusal.
- Optimized TinyGo can inline main.main into _start. The original oracle
  incorrectly required main.main in every trap stack. Corrected checks retain
  the actual SIGILL, catch-unreachable stack and exact checked permission error;
  opt0 still requires its original source frame.
- Enabling ROS without an authenticated Gateway returns exit 126 before Wasm.
  The earlier enabled-zero assumption is retained as a failed harness attempt.
  These controls are not guest error13, zero-grant native sessions or delivery.
- The first independent reader assumed an older reused-log layout. Its source
  is retained as readback-r1.py; readback-r2/r3 explicitly preserve original
  reused log directories. The failing reader run is recorded in the task tool
  transcript, not represented as an archived successful result.

The [earlier resource checkpoint](../tinygo-sdk-resources-20261019/README.md)
retains genuine five-mode ROS2 native exchanges, wrong-value controls,
partial-construction rollback and three finite opt0 source-value workflows.
Those are separate compiler products with separate hashes. This checkpoint
does not reassign earlier native/DBG/performance evidence to its new binaries.

## Resource admission and remaining work

Cgroup:
 /system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope
Boot: 51c27aa1-d709-4a06-ba16-2ac3003fbcbd; device/inode 29/19075.
memory.max remains 68719476736; memory.swap.max remains 0.
No subcgroup, global cache purge, kernel change or foreign cleanup was used.

The shared group later fluctuated around 61.5–63.5 GiB. The remaining compiler
cohort waited 1200 seconds and ended NOT_RUN_CAPACITY with zero compiler
children. Idle ordinary/debug waiters were verified by exact argv/start/cgroup,
stopped, checked to have no children, and retired. No background waiter from
this checkpoint is left running. Retirement receipts preserve that distinction.

The shared memory.max event counter advanced from the older 641 baseline to
880 during the broader shared-host period. OOM and OOM-kill stayed zero.
Those shared max events are not attributed to this cohort. The independent
checkpoint reader itself observed unchanged max880/OOM0/OOM-kill0.

performance-admission-r1.json records both insufficient raw reserve for the
strict 60 GiB profiling admission and the busy existing benchmark lock. It
started zero target/collector processes. The lock was neither removed nor
bypassed. No timing or PMU sample is inferred from a functional codec loop.

The current remaining plan is:

- 14 producer products: ROS1 opt=s codec_benchmark and all 13 ROS1 opt=z entries.
- 356 ordinary commands to complete the 390-command plan: 226 more default
  Guest executions, 112 argument refusals and 18 missing-Gateway controls.
- 15 actual LLVM source-debug capability observations across three repositories.
  Optimized source locations/values must be reported unavailable when absent.
- Real source-pair-generated owned-RMW service inputs and independent native
  control/ROS closure. The missing customtypes/bounded_records.go is not
  fabricated or qualified by the 135-product self-contained matrix.
- A properly admitted same-source native/TinyGo/engine benchmark cohort with
  actual qualified hardware-counter, Hotspots and Microarchitecture results.
  The broader vendor/mixed-ROS/platform/cache and complete SDK gaps remain open.

resume-plan-r1.json identifies the prepared producer-r5.py, ordinary-r6.py and
debug-r6.py drivers. Their Python syntax was checked on Linux; **they were not
executed**. Run producers first and require the complete result before running
the ordinary completion driver. That driver preserves the 34 closed rows and
runs only the remaining 356. Resource, original-cgroup and input checks remain.

## Build, application and debugger instructions

The active ROS [complete build guide](../../../../uwvm2-ros/sdk/examples/tinygo/BUILD_MATRIX.md)
covers all fourteen entries, five optimization profiles, exact output/argument
checks, Gateway preflight semantics and the optional generated-source route.
The [resource tutorial](../../../../uwvm2-ros/sdk/examples/tinygo/native_resources/README.md)
documents actual native peers, complete policies, finite input credits,
ownership, cleanup and independently observable complete values.

All fifteen language debugger options remain enabled by default. Keeping them
enabled is recommended, including mixed-language runtime frames. Turning off
--debug-language-tinygo via --debug-languages=individual trims only implemented
TinyGo debugger adaptation; valid TinyGo-produced Wasm remains executable.
See the [common option/macro guide](../../../docs/debugger/language-options.md).
A switch, source map or debug section alone does not qualify usable language
debugging.

## Packet integrity and replay limits

MANIFEST.json lists every payload file's relative path, bytes and SHA-256.
The transport receipt covers the compressed archive, uncompressed tar stream
and full gzip EOF. All three published trees must have identical manifests.

Source and producer Wasm files, import reports, original logs, result JSON,
drivers and admission/retirement receipts are included. Compiler installations
and large UWVM/native SDK binaries remain external, identified inputs; they
are not copied into this packet. Original result paths describe the actual
Linux execution, not a promise that an arbitrary extracted directory has
those external installations.

The independent reader requires the original retained Linux stage and matching
external files/cgroup/boot. To run a new test after an update or on another
platform, create a new source/tool/runtime association and collect new logs;
never rewrite old hashes or treat an extracted receipt as a new test pass.
