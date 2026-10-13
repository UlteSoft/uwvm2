# Concrete same-source GC A/B/C recipe, 2026-10-02

`prepare_gc_nativeTLS_ABC.py` prepares build and cold command lists for the sole
Linux keeper. It launches no compiler, guest, profiler or SSH and adds no guard.
Actual build, cold, signed-cache and performance results are pending. Run every
listed command through the existing reviewed keeper supervisor in the current
64GiB/swap0 scope; compile on the permitted E cores. Use fresh boot/init identity,
not a historical PID. No timing overlaps compilation, another VM or profiler.

The common source starts from each own root-reviewed R5 plus13 frozen overlays.
Original32 source records (26production +6test) are pinned by
`gc_nativeTLS_ABC_source_contract.json` and source archive
`ef3bdb9d8227be8405d1c72c5e14eef1e040ec29eb2654fbb3bcb192886f521e`.
The original review packet2cac8767 contains diffs/provenance; it is not a complete
source payload. The full original candidate remains
`B/candidates/r5-nativeTLS-combined-gc-membership-v2-20261002-r1`.
The producer verifies the root194-record manifest, every original source pin,
complete src/vendor baseline delta exactly13paths, and unchanged runtime/header
API owner bytes. It does not accept source ID alone as a macro/ABI witness.

One common private source per repository is created at
`B/candidates/gc-nativeTLS-ABC-common-20261002-r1`. Source files are hardlinked to
avoid another LLVM source copy; the environment header is atomically replaced
in that private tree, never written in place. The sole additional production
change is the approved six-line precise metadata cache key. Its OFF projection
must reproduce the actual13-overlay environment byte for byte. This yields
ordinary ENV43652B/SHA381a8ad81599cf33e71367b3f7fe016ee4a16ab1b3fd6ca4463aace8359ecf1a
and ROS43061B/SHA c13c0c4635f5f83acf41b550b534f1b31552a4a51f06348d089b844c14d0646d.
These are intentionally different from later live609a/02c files. The approved
applied manifest2ae326cf records the key's provenance; it does not authorize
unrelated live environment blocks. The common whole-source fingerprint is
computed from actual bytes and shared by A/B/C within each repository.

| Profile | Exact independent experiment vector in all native TUs |
|---|---|
| A | All UWVM_EXPERIMENTAL_* macros omitted |
| B | Only UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP=1 |
| C | Only UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA=1 |

All use UWVM_USE_THREAD_LOCAL, the actual R5 LLVM23/libc++/O3/exception/unwind/
SSL prefix and CAPTURE=1 compilation support. The capture environment is cleared
for actual guests; no JIT object capture enters timed measurements. Value0/2 are
not the omitted default control. No six compact experiments, setter or array
proof is enabled. R5's old measured product has different source, so is not A.

Every profile fresh-compiles runtime, main and host API. Each TU has an actual
`-dM -E` macro witness, actual MD, before-pinned complete dependencies, fresh
object SHA and successful original command/retirement receipt. No RT1/host3
reuse occurs. The metadata h/.cppm and storage impl h/.cppm plus wasm_module
imports are physically in the pinned source. Header MD qualification requires
gc_object.h and gc_trace_metadata.h for each TU. The `.cppm` sources are closed
but this default-C++ recipe is not a compiled module-interface test.

LLVM SDK/static dependency reuse is explicitly separate from fresh product
objects: original RSP08974aa9 and mapped RSPb3ad1de4 retain all63 archive paths in
order. Complete library/reference bytes are ef76117d; target __config_site and
actual tools/headers/DSOs are pinned before/after and from each actual MD.
Whole-file RSP identity permits its already reviewed NDEBUG and exact
`-Xclang -fno-pch-timestamp`; nested RSP/profile override remains rejected.
Loaded DSO maps are not invented from static ldd/reference pins.

Place the producer, source_contract JSON and immutable
`prepare_general_gc_R5_nativeTLS_long_cold.py`2afc5478 together. Keep the actual
static-library-reference.json available. With B set to the fixed workspace path,
the keeper's supervised preparation command is:

```sh
python3 prepare_gc_nativeTLS_ABC.py --static-reference \
  "$B/builds/debug-joint-r5-nativeTLS-current-cli-cold-20261002-r2/static-library-reference.json" prepare
```

This prepares `B/builds/gc-nativeTLS-ABC-20261002-r1/plan.json` plus, for each
`uwvm2|uwvm2-ros / A|B|C`, independent build-commands.json (13stages) and
cold-commands.json (34stages). Output evidence must be rooted at
`B/evidence/gc-nativeTLS-ABC-20261002-r1/<repo>/<profile>/build|cold`, with actual
receipts.json and per-label log files. Keeper adapts only the reviewed
supervisor's fixed source/output/command paths and fresh admission; no producer
branch emulates CG/PIDFD protection or silently authorizes an unreviewed guard.
Run ordinary A→B→C first, then ROS A→B→C, serially. Before reuse or performance,
require each closure-after.json and cold-summary.json really passed.

Cold commands use the16 actual1M/2M four-family allocate/mutate Wasm files, each
under instruction/unwind, cache disabled, verbose and `-Rclog err`. Only existing
official validation/byte-roundtrip and Wasmtime49 copying `run` checksum/_start
provenance is reused; no old product PASS is imported. `_start` independently
rejects wrong scalar/readback checksum. New allocation cells require positive
attempts/collections/reclaimed, exact planned allocations, roots1/disabled0/
reason0. Mutation requires the exact1024/2048/1024/3072 initialization allocations,
no attempt/collection/reclaim, roots1/disabled0/reason phase_pending2. Mutation
is field/lookup qualification and never collector throughput proof.

Existing meaningful native membership/metadata/graph/8,32-store controls remain
separate source-bound cold qualifications. The signed-cache follow-up should
reuse `cache_product_isolation.cc` (write/load own context, reject foreign context
in both directions) and `llvm_jit_cache_integration.cc` actual hit fixtures with
fresh A/B/C bindings. Macro witnesses do not constitute signed-cache acceptance;
this recipe adds no test that merely mirrors the key-construction implementation.
The six-line key conservatively isolates host collector profiles; no confirmed
cached-IR type-layout overrun or public ABI repair is claimed.

After actual cold closure, reuse the already reviewed measurement code/protocol,
binding the new actual executable/source/MD/profile receipts rather than replacing
old fixed R5 IDs. P0 unprofiled A-B-B-A and A-C-C-A initially target reference-array
allocation2M and reference-cycle allocation2M with mutable/numeric controls.
At least two process pairs expose frequency/noise variation. Internal Wasm timer
excludes startup/JIT but includes setup, allocation/fields/collection/readback/
checksum. Parent wall and wait4 CPU/RSS are separate, with stopped-bootstrap
scope kept explicit. The native8/32 component's collection timer alone is a
collector ROI and must not be replaced by whole-VM time divided by collections.

Pure HW cpu_core cycles/instructions group is a distinct whole-guest single-TID
measurement with actual successful FDs, enabled/running100%, ACK-before-GO and
owned PIDFD completion. Do not silently add reference-cycles and allow multiplex.
VTune HW is another set of guest executions, actual PID/TID/cpu_0 filters,
sampled-self hotspots and MUX limits; use current installed help, no unsupported
cpu-mask option and no SW fallback. Temperature is raw observation only.
Inherited/capture environment and Intel loader injection limitations remain
explicit. None of these measurement families substitutes for another's ROI.

No profile is promoted to default and no industry ranking follows from this
source-only preparation. Actual current default nativeTLS evidence is distinct
from older S6e compatibility TLS-map percentages. Wasmtime copying is the pinned
cycle-reclamation oracle; DRC cannot serve as a cyclic graph collector baseline.

Preparation review corrections: before each build, all three objects/MD/macro
outputs, executable and response file must be absent, and actual compilation
disk headroom must be at least16GiB. The selected original receipt row is pinned
by canonical JSON hash; appending later stages to receipts.json does not change
that original proof. An immutable actual receipt-prefix copy is retained separately before the final
helper returns; it excludes that helper's own receipt. Keeper must close/hash the
complete13/34-stage raw receipt file after the supervisor finishes. The prefix
copy never qualifies the final supervisor outcome.
Explicit linked libssl/libcrypto and z/zstd paths have historical exact SHA/size
checks in addition to the DSO reference and63 LLVM archives. Every actual -I root
is resolved and inventoried before compile; actual MD spellings and resolved paths
are both retained. No missing before dependency is treated as qualified.

## Source recipe revision 2

The only generated-command correction selects each product's actual CLI: ordinary full uses `-Rcc jit -Rcm full`, while ROS LLVM full uses `-Raot`. The three build profiles, common source contract, compiler prefixes, source/MD/SDK closure and resource supervisor are unchanged. The original revision 1 snapshot is preserved; no revision 1 native execution is claimed. The pure checker now asserts the exact mode token shape in every planned cold cell.
