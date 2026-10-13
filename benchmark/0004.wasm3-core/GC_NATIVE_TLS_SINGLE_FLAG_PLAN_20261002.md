# Same-source native-TLS collector A/B/C plan, 2026-10-02

This is a source-only plan for the existing pipeline; no build, native execution,
new guard or product default is authorized or claimed by this document. Keeper
linux_fused_resume is the sole Linux executor. New-boot tool/PMU/cgroup/init
admission must be read again; boot b0907744 evidence is historical.

The input source is one privately frozen, root-reviewed R5 plus 13-production-path
GC overlay, identified by
[the source review](../../build/wasm3-evidence/source-only-r5-nativeTLS-combined-gc-membership-v2-20261002-r1/root-review.json)
and its packet SHA256 `2cac876783c820fc2ee8508c0928bdb59378d75bc0fbca313719f96e42a05038`.
Keeper must recover that actual reviewed archive, canonical dependency bytes and
per-repository pins before deriving commands. Do not copy today's whole live
source, a later array-auth change, a different debugger/EH publisher or unrelated
vendor edits into this comparison. If an additional required metadata header/export
is missing from that archive, close and review it before building every profile.
This plan itself does not invent a resolved source ID or completed build receipt.

| Build | One same-source compilation profile |
|---|---|
| A | UWVM_USE_THREAD_LOCAL enabled in every TU; all experimental macros omitted |
| B | A plus joined `-DUWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP=1` only |
| C | A plus joined `-DUWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1` only |

A is the new same-source control, not the already-measured R5 product. Actual R5
main/runtime/host-api MDs pinned gc_object.h to SHA256
`209de02c372a7c082be0aada2760699b6a543631fa9e280b4263ebae528ea456`
(275849 bytes). Its runtime MD also pinned cache/environment.h to
`b5cd7b702b03ef679cb031a526b4f4bbd5d2a5784dda42198574fc0987e428b7`
(42139 bytes). The 13-path source contains additional reviewed collector/status
changes. Comparing R5 directly to B/C would mix those changes and would not
isolate membership or metadata. R5 remains a separately identified diagnostic.

Derive the existing actual 8-stage R5 main/runtime/host-api preparation and link
recipe three times into fresh directories. Preserve its compiler, nativeTLS,
LLVM23 SDK, optimization, target, ABI, SSL and response-file closure; change only
the listed joined profile define for B or C. All three source fingerprints must
be equal within each repository. Complete argv/RSP bytes and actual macros are
independent profile evidence; equal source ID alone does not prove macro equality.
Reject inherited/preprocessor flags that rewrite a candidate, hidden -U/-D in
RSP, forwarded options, extra experiments or an unmatched config_site. Emit actual
preprocessor macro witnesses for main, runtime and host API. Both value0 and value2
are useful cold projection controls but are not the omitted production control A.

All three major translation units need fresh compilation: every actual R5 MD
included gc_object.h. Membership adds inline collector/helper/cache behavior.
Metadata enlarges the private type_layout with a store-owned immutable plan and
changes construction/destruction/tracing; it cannot mix with an old runtime or
host-api TU. No legacy RT1/host3 object can be reused just because a build tag
matches. Private plan identity comes from canonical layout construction, never
from shape agreement. Existing locks, actor/exclusive admission, global token
ownership, generation, foreign-owner leases and whole-graph preflight stay intact.

LLVM/vendor/static SSL binaries may be reused only when their actual source/tool/
ABI/config-site/static-library SHA and compile profiles are unchanged and actual
MDs prove absence of the affected inline runtime/cache headers and candidate
macros. Reuse is an attested dependency fact, not a guessed exemption. Objects
that reference either affected header or transitively compiled storage definitions
must be rebuilt. Keep exact actual compile/link/MD/ELF/source-before-after receipts.

The changed-header dependency set to audit includes gc_object.h,
collection_local_membership.inc, gc_trace_metadata.h/.cppm and storage
impl.h/.cppm imports/exports, plus runtime/llvm_jit_cache/environment.h. Recover
all bytes from the reviewed source; do not substitute a declaration-only header
or a stale module import. Public bridge ABI, object layout and nativeTLS identity
must be checked independently from private type-layout construction.

Membership has the existing exact1 cache key
`gc-collection-local-membership=closed-canonical-local-v1`.
The initial read-only audit found no PRECISE_GC_TRACE_METADATA discriminator.
Root subsequently approved the separate exact1 six-line cache-profile addition;
its applied manifest SHA256 is
`2ae326cf1b3106661dbbe3cf929a46d88da153e85f49502c05b02c3a99aafca6`.
The approved ordinary environment before/after hashes begin7d740/609a, and ROS
490f/02c. This is conservative host collector-profile isolation, not a repair
of demonstrated cached-IR type-layout dereferencing or a proven cache overrun.

A/B/C preparation must now bind that actual applied manifest and its full hashes
plus the reviewed13-overlay base. The proposal's before environment can contain
later independent default-off identity blocks. Recover and enumerate all such
source deltas; do not silently substitute the whole live609a file for a different
archived13-overlay environment. Either derive the exact six lines on the actual
reviewed base with an independently reviewed off projection, or explicitly review
the complete resulting common source. All three builds use the same final ENV/GCO
and candidate dependencies. Only joined compiler flags differ. Source ID is not
a substitute for the actual per-TU macro profile or cache-context identity.

Initially A/B/C use `-Rllvm-cache-path disable`. After full current source/profile
binding, actual cache smoke must prove metadata and membership key separation,
correct symbols and hit/miss behavior with unchanged ABI/schema and exact1 gates.
Undefined/0/2 preserve their original default cache projection. No new flag or
cache implementation is invented by this plan.

Before performance, use existing source-bound component controls on A/B/C:

- Membership ordinary/foreign/stale/forged/wrong-kind/outside-cohort/alias/drop
  controls and the actual same-actor reserved fallback fixture. A real reserved
  operation must retain old-path late failure, zero reclaim and unchanged epoch.
- Metadata layout/function/array-zero/index overflow/invalid storage/index OOM
  unit/store controls; mixed numeric/reference/exn fields retain real validation.
- The existing current registered/native-value exception graph controls in both
  metadata states. Their external_exception_handles==1 refusal is preserved;
  they do not prove actual native-throw/external-handle integration.
- Actual 8/32-store components with scattered cross-store cyclic roots, meaningful
  warm/readback/final-drop checks and the exact same collection counts. No test
  probe, allocator hook or diagnostic instrumentation is compiled into timed CLI.

Prior native component passes are useful controls, not new A/B/C product passes.
The next cold manifest must pin the actual source/compiler/profile/dependencies
of the new ELF. ASan/OOM builds are separate from timed O3 and never silently
replace the product. Do not construct illegal admission to force a fallback.

For the first product cold stage, reuse the existing exact four-family Wasm
fixtures at1M/2M and official same-byte validation/Wasmtime oracle provenance.
Run all family/phase cells under instruction and unwind full-JIT policies with
cache disabled, `--log-verbose -Rclog err`, gc/ref-types/function-refs enabled.
Require actual scalar checksums, allocations and collector counters for each
fresh ELF. Allocation requires positive collects/reclaimed, roots_requested1,
disabled0/reason0. Mutation requires exact1024/2048/1024/3072 setup allocations,
no attempts/collections/reclaimed, roots_requested1/disabled0/phase_pending2;
it is field/lookup qualification only. No sealed-compact counter is required.
Whole-graph failure controls require zero reclaim and unchanged live garbage.

Then take a small serial P0 development comparison, using the already-reviewed
plain, HW and VTune measurement protocols with their real new-build receipts.
No compiler, Windows VM, QEMU, other product or profiler overlaps it. Start with
reference-array allocation2M and reference-cycle allocation2M, plus mutable and
numeric allocation controls. Separate A-B-B-A from A-C-C-A; at least two process
pairs per candidate are needed to inspect frequency distribution and noise.
Do not mix B+C or the six compact experiments in this initial attribution.
If a row remains below100ms or has missing loaded-executable/frequency evidence,
retain it unqualified and use a larger same-semantic officially validated fixture
for a later qualified comparison; do not turn a short row into a ranking.

Plain data keeps internal Wasm time, GO-to-reap wall and wait4 whole-child user/
sys/RSS separately. Internal time includes the whole GC workload, not pure
collector latency. Native 8/32-store component collection_total_ns is a separate
actual collector ROI and excludes setup/admission/readback. Pure-HW cpu_core
raw3c/c0 counts are whole guest single-TID after enable ACK, with actual successful
FD grouping and exact enabled/running proof. Preserve actual current PMU/kernel
inventory, not old-boot values. Optional refcycles capability probing is separate;
never add a third event and assume no multiplexing. VTune HW uses different
profiled guests, real loaded PID/TID/P0 report filters, sampled self versus
inclusive stack separation and actual MUX/lost/skipped-frame limits. Capture
JIT objects only in untimed codegen runs, then prove the actual object/function
ranges; clear capture in every timed guest and profiler process.

The IR/assembly review checks that the ordinary generated Wasm loop/bridge ABI
is unchanged between A/B/C and that only selected native collector/type-plan
paths differ. Inspect the actual final ELF collector helpers and loaded object
ranges, not only source or lambda numbering. Invalid canonical layout/reference,
foreign-owner/exn/native roots and mutable synchronization retain their original
trap, validation and locking behavior. No direct guest token dereference or
cross-operation authentication cache is introduced.

Initial success means measured same-source development attribution plus these
cold controls. It does not promote either experiment, prove threaded/external
exception integration or claim industry leadership. ROS needs its own actual
source and full3-TU closure; ordinary R5 measurements do not qualify it.
