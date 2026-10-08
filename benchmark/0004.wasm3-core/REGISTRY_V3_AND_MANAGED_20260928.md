# Fixed v3 registry and managed analogues, 2026-09-28

This report binds historical immutable O3 products, not the later production
source batch containing the bridge-teardown fix. Ordinary source ID is
`sha256:c7f97991ba559981a71543aa4c692ec3b9ccd1cbd68057ce14469f7723062841`;
ROS is `sha256:43a37bcffcae8dc85dba5f7e9896b0046da00bce8cdb1ea35a625f04a83a1c1a`.
ELF hashes remain respectively `098f64a4…724fb` and `84293598…03b6a`.
The experiment never modifies either product's `src`.

## Native registry A/B

Baseline header SHA-256 is `21f90b8edb9427f12feb710ea643011d5fba3a672ccd5a0ac76b0e1e16802a2d`;
fixed isolated v3 is `1080f56b8b723002342ac7131aea4a28d9c2f7445694a4d96d5825d502ddaeeb`.
Linux address/undefined/leak sanitizers passed all eight cells across both
source contexts: forged/unissued/stale token and concurrent registration,
stripe/foreign-owner stress, and baseline/candidate unique-to-shared owner
transition. The leak tests do not include all cross-store object graphs;
known monotonic retention/cycles and bridge teardown remain separate blockers.

The shared-store workload allocates 4M objects, then performs 10M observable
set/get updates through 1024 dynamically selected references. It preserves
shared ownership. The switching workload allocates 1M objects across 64 stores
per worker. Nine pairs reverse baseline/candidate order; worker threads pin to
P cores 0/2/4/6 (1P uses 0). **The field phase is a single main thread after
allocation, even in the 4-worker row**; it is not 4P field throughput.

| Context / allocation workers | Single-store allocation baseline → v3 (ms) | Paired speedup | Single-thread field paired speedup | 64-store allocation baseline → v3 (ms) | Paired speedup |
| --- | --- | --- | --- | --- | --- |
| ROS / 1 | 312.656 → 302.654 | 1.035× | 0.983× | 98.356 → 80.461 | 1.216× |
| ROS / 4 | 190.651 → 176.986 | 1.073× | 0.996× | 46.971 → 37.760 | 1.257× |
| ordinary / 1 | 310.228 → 301.207 | 1.033× | 0.990× | 96.566 → 79.526 | 1.213× |
| ordinary / 4 | 189.368 → 178.216 | 1.067× | 0.980× | 49.672 → 40.183 | 1.200× |

Single-store RSS remains about 492 MiB. Switching-store RSS remains about
157 MiB (1P) / 254 MiB (4P). Registry unlink/teardown improves substantially:
ROS 1P single-store 133.60→104.36 ms; 64-store 190.58→55.35 ms. That is teardown
of an acyclic native graph, not reclaimed guest objects during execution.

All switching allocation intervals are below 100 ms and the 4P pairs vary
widely; these throughput estimates are exploratory. Package readings reached
80–100°C, governor was `powersave`; frequency samples between processes often
show idle clocks. Reverse pairs control coarse order bias but do not eliminate
thermal, turbo or scheduling variability. The small field regressions need a
cooler repeat and cache/layout analysis. **Do not merge v3 from these results.**
Its known last-foreign-bridge teardown hang must also be fixed without losing
the measured candidate's provenance. A nonreclaiming heap cannot pass release
qualification regardless of allocation or teardown speed.

The corrected token statistic reduces first and last reference from **every
store**, outside allocation time. Each store has one writer and issues tokens
monotonically. The 4P candidate all-issued span is 1,048,386; the old worker
endpoint reduction is 999,235 and is now emitted only as `token_endpoint_span`.
Historical values using the latter as `token_span` are invalid. The new sanity
check `token_span >= allocations` is not a uniqueness proof; sanitizer stress
checks identity separately. Corrected fixture SHA is `d79607c247aaa3adda9f3e2ff9b5c03507d7a586d848fb8f3f4fa709f6a9aece`, runner
`e8ea3d8a41ddf62ac1c5f52a734e02cd315f7f170713ba5536935e42bbcf312b`.

Raw remote root: `/work/wasm3-resume-20260924/evidence/followup-c7f9-43a37-20260928/`.
ROS summary SHA is `76273af35deb2fe4702d81124aa8cd326aa92e4b41e13c40aaf94e7013b7324e`;
ordinary is `5c28d3461adc6bf7989a1c058a72348eb7eb6b099f065a7c521bc130b09d3f95`.
Local small artifacts reside under each repository's
`build/wasm3-evidence/linux-current-{c7f9,43a37}-o3/performance-20260928/followup-registry-v3/`.
Full pre/post fingerprints and object cache remain remote and hashed.

## Actual GC guest/runtime assembly

Both immutable O3 products executed the self-checking 8192-step ring and
produced real signed JIT caches. Fixture Wasm SHA is
`d20970fb576326215a3eae0d9b9ccaed61872eae4099ccf246baa5872cf2c173`.
The unwind hot loop (0x40–0x177) has 76 instructions and **five bridge calls per
step**: struct.new, table.set, table.get, ref.cast and struct.get. The total
function has 150 instructions / 15 static call sites in unwind mode and
156 / 17 in instruction mode; these totals include cold trap blocks. The extra
two instruction-mode calls are entry/exit records, not per-iteration calls.

The exact `llvm_jit_gc_aggregate_bridge` function in each compiled O3 runtime.o
retains `xorps` plus **eight 16-byte movaps zero stores**, offsets 0x50 through
0xc0 from rsp. It also retains input-count copying and the opcode dispatch.
The ring calls this generic bridge twice per step, so it zeros 256 bytes of
small input arrays per step, in addition to the caller's 16-byte input-slot
initialization. This is an actionable code-generation cost; it is not a
sampled CPU-cycle attribution. Runtime allocation/publication, table access,
cast and get costs still cannot be separated by simply subtracting different
workloads' wall time. `perf_event_paranoid=4` restricts hardware sampling here.

Use only `gc-runtime-bridge-asm-{ordinary,ros}-env3` for exact bridge bodies.
The initial loader attempt lacked libc++; the env2 name-substring filter also
included symbol-name generation templates and overwrote listings. Those
attempts are retained but excluded. Env3 selects exactly the two code symbols
and asserts their emitted bodies. All tool/ELF/runtime/object hashes and
commands are stored in summaries. Ordinary runtime.o SHA is
`1348df78928075a16278fc6f9b569f79bed596962056eb3160b8707e769e75ef`;
ROS is `ee5e2e253be17282b297e29ce25dc1f91f9109fd3a1edb2d2b94b661e6938af0`.

## Managed analogues

This is a **cross-language analogue**, not identical Wasm bytecode or an engine
ranking. Each publishes a fresh one-field object into a static 1024-root array,
checks the LCG and each final root, and performs eight 250K JIT warmups. JVM G1
uses a 1 GiB heap cap; .NET workstation GC has a verified 1 GiB committed heap
limit; V8's 1 GiB old-space limit is not a total-heap cap. Java, C# and JS have
different object headers, barriers, type checks and optimizing compilers.

Nine P0 reversed 50M/200M process pairs give guest-loop slopes below. Every
200M guest interval exceeds 100 ms. Runtime and source/artifact hashes are in
`managed-ring-200m/{metadata,summary}.json`; summary SHA is
`cf5bbf9aea45072a2683be28b36fa9e2350ff71d8902a9f921fc97d070a44d3e`.

| Runtime | Collector / execution | Median slope (ns/step) | High-count peak RSS (MiB) |
| --- | --- | --- | --- |
| Temurin OpenJDK 27+35 | HotSpot G1 | 1.499 | 194.6 |
| GraalVM CE 25.4.4.1.1+1.1, JDK 25.0.4.1.1 | HotSpot G1, not Native Image | 3.026 | 241.5 |
| .NET runtime 10.0.12 / SDK 10.0.401 | Workstation GC / tiered PGO | 3.226 | 165.2 |
| Node 26.10.0 | V8 | 3.472 | 165.2 |

A separate traced 200M, nine-process diagnostic observed real collections.
Java allocation counters report about 3.2 GB (16 bytes per step), and .NET
about 4.8 GB (24 bytes per step), ruling out allocation removal for those
traced runs. V8 allocation bytes are unavailable here; trace events establish
actual collection. Trace overhead means these runs are **not** the untraced
throughput samples. Collection counts for JVM ZGC are sums of MXBean deltas
and may not equal unique major/minor cycles.

| Runtime / collector | Observed count over nine processes | Pause events | p50 / p95 / p99 estimate (ms) | Trace peak RSS (MiB) |
| --- | --- | --- | --- | --- |
| OpenJDK G1 | 180 | 180 | 0.308 / 0.483 / 0.542 | 203.1 |
| OpenJDK ZGC | 270 MXBean deltas | 216 | 0.003 / 0.008 / 0.012 | 854.4 |
| Graal HotSpot G1 | 180 | 180 | 0.568 / 0.812 / 0.832 | 250.9 |
| .NET workstation | 2295 Gen0 | per-event API unavailable | 11.424–11.920 ms total per process | 165.9 |
| V8 | 5787 traced GC events | 5787 | 0.060 / 0.070 / 0.080 | 165.9 |

These pooled event percentiles describe this fixed workload, heap and CPU
restriction; event dependence and workload duration limit generalization.
Current UWVM has zero collections, so it has no valid collector-pause success
claim. Its RSS/reclamation gate stays **FAIL**. Diagnostic summary SHA is
`76cce51bb35be516fc5e2ee7a8026a517688790f4e2cd385d73bb40135b6087d`.

The first .NET build version probe exited -6 because this minimal container
has no ICU. Both test-only build runners now explicitly use invariant
culture, as do the measurement commands; integer workload semantics are
unchanged. The failed preflight remains separate from `managed-build-env2`.

## Qualification environment

All Linux builds/runs occurred in the same 64 GiB cgroup with swap disabled,
exact cpuset `0,2,4,6,16-31`, recorded topology and P-core affinity. OOM,
OOM-kill, cgroup max events and CPU throttling remained zero in completed
phases. Each raw pair retains frequency/temperature/cgroup telemetry, commands
and checksums. No Windows VM, QEMU or product compilation overlapped timing.
Later product-source changes need new-SID qualification; this historical
baseline cannot be relabeled as the new production binary.
