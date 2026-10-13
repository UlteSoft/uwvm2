# Explicit aggregate sweep: native component qualification

This is a native component experiment with a complete array of explicit roots
and a caller-quiescent, canonically pinned cohort of all admitted stores. It is
not a WebAssembly workload, an allocation-triggered collector, or a VM release
qualification. Passing it must not clear `RELEASE_BLOCKER_GC.md` or the real VM
RSS/root-survival gate. The production activation roots, interpreter ring,
LLVM SSA/spills, parked mutators, host callbacks, exception and extern graphs
still require independent integration and collection-triggered tests.

The driver is `run_gc_explicit_sweep_component.py`; the native fixture is
`gc_explicit_sweep_ring.cc`. Both are test-only, mirrored between repositories.
Python `--self-test` checks accounting and rejects incomplete JSON; it neither
executes a collector nor provides a native pass. Native compilation,
ASan/UBSan/LSan, proof execution and P-core timing are pending until their raw
commands actually complete.

## Immutable inputs

Use the parent-reviewed owner-checked header, SHA-256
`16fcee47d451c614b222db978ff2fc2570d8c3ada94c782a0505b0ae1407b1f5`,
from an isolated directory containing only
`uwvm2/uwvm/runtime/storage/gc_object.h`. The prior `9134936f...` header is
historical and must not be substituted: it could resolve a store outside the
canonical pinned cohort during the admission/global-index teardown gap.

The baseline native component uses the unchanged monotonic product header,
SHA-256 `a24fa96e3e45eb1ec2e70936ce2da06d1439cf269a0d175bfa7ed38661017c49`.
The runner selects the candidate only for a direct native angle include,
checks its actual depfile and rejects inclusion of the baseline definition in
that candidate compile. This native include technique does not override the
relative include inside the LLVM emitter and cannot qualify the LLVM emitter.

The ready O3 binaries identify the r2 source contexts; these product binaries
are not the binaries timed by this native component experiment:

| Context | Exact source ID | Actual O3 product ELF SHA-256 |
| --- | --- | --- |
| ordinary r2 | `sha256:3921b6c3dee437775c219bd9d25293f89c063f7b2637061ec1bdb700716221fa` | `2a3cfa640699637dd9bb489aa35e39ef3752e47f973b5f61cedb695e2af368dd` |
| ROS r2 | `sha256:55246b7b19aad36e74eb5687c205aef0630bb629968f50fba962bb31edacacf7` | `2781300e7569c94f2b53d671d11ea43985c190fc696a27e68195a715a638daa2` |

The runner records the source and native/compiler/fixture/header hashes, real
runtime object provenance and exact compiler commands. It compiles native O3
and sanitizer binaries in each real repository context; it does not reuse an
overlay object as a product object. New source IDs require fresh manifests and
a reviewed header rebase, rather than moving qualification from r2 to r3.

## Correctness before timing

The candidate proof actually collects a graph spanning two stores and an
empty admitted third store. It checks:

- A cohort omitting the empty admitted store, a forged token and a swept token
  fail without reclamation or corruption of surviving data.
- A root in store A retains the exact child payload in store B; an unreachable
  A/B cycle, a scalar and a zero-length array are reclaimed exactly once.
- After the live field is overwritten with null, its former child is swept
  and the obsolete object-owned foreign lease no longer pins store B when
  explicit module lease roots are also retired.
- Later allocations never reuse swept identities. Dropping the final explicit
  root reclaims its object. Four successful collections reclaim eight objects.
- Sanitizer-only builds force one collector work-array allocation to return
  null, require `out_of_memory`/zero reclaimed and verify the unchanged graph.
  O3 binaries contain neither the allocator wrapper nor its fault-check branch.

The driver also builds/runs the actual
`test/0019.gc_statepoint/product_cohort_sweep.cc` companion in both contexts
under ASan/UBSan/LSan, requiring its seven successful collections and six
reclaimed objects. The conditional admission/destructor pause hook is not
enabled by this driver: the controlled teardown-gap test remains separate.
Neither proof accepts extern/exn wrappers as an implemented collector graph.

The monotonic baseline proof records zero collections and retains an object
after 4,096 subsequent allocations. Its success only qualifies the baseline
semantics; it is not a collecting runtime.

## Observable ring and independent checksums

One native mutator fills a 1,024-root ring with one-field mutable i32 structs.
A deterministic 32-bit LCG gives every allocation a different observable
payload. All root values are read back against independently tracked values
after each chunk. The Python oracle uses affine exponentiation to reproduce
the last 1,024 values independently of the collector.

The run continues for up to 10 million allocations. With the default 65,536
interval, chunks are split at each million-allocation RSS checkpoint; there
are 16 ring collections per million and one final collection after dropping
all roots. A 10-million run therefore has 161 real successful API calls and
reclaims exactly 10 million objects. Every ring collection retains exactly
1,024 objects; the final call retains zero. The baseline reports zero
collections and retains every allocated object until store teardown.

After allocation, one million state-derived accesses vary over all 1,024
retained references. Each `struct.set` is followed by a checked `struct.get`,
and both the read sum and final root sum are externally checked. This avoids
an unobservable store workload or a single constant object being folded into
a scalar. The collected and monotonic heaps have different allocator/cache
histories, so this comparison is a component diagnostic, not an isolated
same-Wasm local-field regression claim.

## Timing and memory interpretation

Allocation time, collection API time, root-validation time, dynamic field
set/get time and module teardown time are recorded separately. Collection
timers contain the collector call; their returned status/count checks and
printing occur afterward. Allocation timers include the common allocation
result checks. Ring wall time additionally includes root checks, logs and
checkpoint RSS reads; it is not substituted for allocation throughput.

The fixture reads actual resident pages from `/proc/self/statm` with FastIO at
the 1M, 2M, ... checkpoints. Linux supplies the actual page size. The native
component plateau gate requires at least two million allocations and bounds
the largest 1M-to-final RSS growth among all pairs in both contexts; the
default bound is 32 MiB. This bounds this explicit graph experiment only.
Allocator retention and process metadata remain in RSS. Peak `wait4` RSS may
include an inherited fork/exec floor; direct checkpoint self-RSS is the ring
memory measure.

The runner performs nine AB/BA pairs, alternating the product-context order.
Each raw sample records P0 affinity, the exact 4P/16E cgroup, 64 GiB limit,
swap disabled, governor/frequency/thermal telemetry and OOM/throttle counters.
Builds run on E16. A 60-billion-byte shared-cgroup guard and a 4-GiB native
process guard stop below the 64-GiB hard ceiling. An optional bounded initial
temperature gate rejects a run when a requested comparable starting range
cannot be reached; unavailable sensors are explicitly recorded.

Pause p50/p95/p99 are empirical quantiles of dependent serial native API calls,
not service percentiles or confidence bounds. The report marks p99 sample
counts below 100 and all allocation/field cells shorter than 100 ms. Minor
ratios in such cells do not justify a performance-regression or industry-rank
claim. The allocation ratio also omits collection cost, so the report includes
allocation-plus-collection time separately.

## Commands after an explicit resource handoff

Copy the single candidate header into a fresh remote overlay and rehash it;
the Mac `/tmp` path does not exist automatically on SSH Linux. Preserve every
failed attempt in a fresh output directory. The existing r2 O3 paths are:

```sh
python3 benchmark/0004.wasm3-core/run_gc_explicit_sweep_component.py \
  --ordinary /work/wasm3-resume-20260924/builds/ordinary-r2-3921-all-combine-delay-o3/uwvm \
  --ordinary-source-id sha256:3921b6c3dee437775c219bd9d25293f89c063f7b2637061ec1bdb700716221fa \
  --ros /work/wasm3-resume-20260924/builds/ros-r2-55246-all-combine-delay-o3/uwvm \
  --ros-source-id sha256:55246b7b19aad36e74eb5687c205aef0630bb629968f50fba962bb31edacacf7 \
  --overlay-root /work/wasm3-resume-20260924/overlays/gc-product-sweep-owner-checked \
  --out /work/wasm3-resume-20260924/evidence/gc-explicit-sweep-component-r2-env1 \
  --samples 9 --max-allocations 10000000 --collect-every 65536
```

`--build-only` separates sanitizer/semantic qualification from the quiet
P-core window. A later `--run-only` with the same arguments and output
directory requires every stored input and binary hash to remain exact. Both
phases retain commands, logs, independent checksums, pause records,
checkpoint RSS, manifest/source checks and final OOM checks. No phase resets
`HOME` or bypasses the shared resource handoff.

Only completed raw commands may be summarized as native component passes.
Even a component pass always reports `product_release_qualified=false`,
`full_vm_qualified=false` and `automatic_gc=false`. Production acceptance
still requires actual collections during WebAssembly execution, precise
root survival across all engines/modes and threads, complete EH/extern/host
reachability, safe admission/teardown, and the separate real VM RSS plateau.
