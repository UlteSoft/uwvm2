# Core 3.0 Linux performance evidence — current candidate, partial matrix

This is a source-bound **partial** result, not Core 3.0 release qualification. It covers the GC allocation ring and three memory64 store workloads on 2026-09-28. The remaining cases, real exception throwing and VM-managed threads still require their own quiet P-core runs. The GC reclamation gate fails.

## Sources and environment

| Product | Source ID (`src` + vendor fingerprint) | O3 ELF SHA-256 |
|---|---|---|
| ordinary | `c7f97991ba559981a71543aa4c692ec3b9ccd1cbd68057ce14469f7723062841` | `098f64a48cfe9261fef16bb42a4d593b2a390538eaf2505c1d678c6a213724fb` |
| ROS | `43a37bcffcae8dc85dba5f7e9896b0046da00bce8cdb1ea35a625f04a83a1c1a` | `842935987d5654f0a6d72f1f3e27fa0530fa92a1b4de45eccabaafd021d03b6a` |

Both O3 products were built in the shared Docker cgroup with `memory.max=68719476736`, `memory.swap.max=0`, and `cpuset.cpus.effective=0,2,4,6,16-31`. Formal child processes were pinned to P-core CPU 0; the harness used E-core CPU 16. Nine reversed/rotated low/high pairs used the **same validated Wasm file** for each accepted engine. Source, binary, generated WAT/Wasm, engine, tool, command, per-pair CPU telemetry, raw wall/RSS samples and logs are recorded in the evidence directories below. OOM/OOM-kill and cgroup CPU throttling stayed at zero. Package temperature varied from roughly 63–100 °C across groups; relative results must retain this thermal limitation. With nine samples, p99 is descriptive only.

| Comparator executable | Binary SHA-256 |
|---|---|
| Wasmtime 49.0.1 | `c36ef7e6548600b09bcb34cb3ee4c138f3e7b01893347ec7f48fdb8e6050aa94` |
| WasmEdge 0.17.1 | `998ba3478bd1d6c0365cf2461608900b0356bfb2e37b71b7bdf587182ff8b365` |
| Wasmer 7.4.2 | `701ceeaa596f710a6fb097c462de9f9571177d7484662b871719ab66a08a2949` |
| WAVM nightly 2026-04-05 | `daf29ef8ebbd9eaa20a13115205b56fafd4b8cfd77b38333c647bb12851ecd23` |

The GC high fixture is the same SHA-256 `66874f9a4a0a5977b4ace1cc702f949c6c2627311dce0bcbe87c818c55516e3e` for every accepted VM; it makes 16 million allocations while replacing only 1,024 roots and checks its result. Memory64 high random-store SHA-256 is `322641f90c7b80225e942031988918da63e925e725a72dd2dd1f6c4b28cb36cc`; the unaligned high fixture is `fe939ab6f45d607d2c9dd7cb6cc034de48dcb33ffca29028583b855230531fe6`. The latter dynamically makes 195,313 actual cross-page stores in 50 million steps; its aligned control makes zero.

## Same-Wasm results

GC allocation ring, median incremental ns/step after subtracting the low-count run, nine pairs (MAD in parentheses):

| Engine | ns/step | 16M median peak RSS | Qualification |
|---|---:|---:|---|
| ordinary JIT full/unwind | 117.991 (0.601) | 2,000.5 MiB | semantics pass; **no collection** |
| ROS JIT full/unwind | 116.906 (0.682) | 1,999.9 MiB | semantics pass; **no collection** |
| ordinary interpreter full | 107.241 (0.457) | 1,984.3 MiB | semantics pass; **no collection** |
| ROS interpreter full | 107.443 (0.309) | 1,983.9 MiB | semantics pass; **no collection** |
| Wasmtime 49.0.1 copying | 11.161 (0.188) | 234.5 MiB | semantics pass |
| Wasmtime 49.0.1 DRC | 51.956 (0.454) | 234.5 MiB | semantics pass; cannot collect cycles |
| Wasmtime 49.0.1 null | 5.144 (0.048) | 266.0 MiB | non-collecting throughput bound; high sample below 100 ms, **timing inconclusive** |
| WasmEdge 0.17.1 interpreter | 177.343 (0.681) | 1,361.6 MiB | semantics pass |

WasmEdge JIT crashed at the 2M semantic run (signal 11). WAVM and Wasmer rejected this GC syntax. Those engines have no GC throughput score. The Wasmtime collector categories follow [its official Collector API](https://docs.wasmtime.dev/api/wasmtime/enum.Collector.html); the tested CLI/binary is separately fixed at 49.0.1 in the raw metadata. Its [version-pinned copying implementation](https://github.com/bytecodealliance/wasmtime/blob/v49.0.1/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs) uses a Cheney semispace heap and a JIT-accessible bump pointer, with no read/write barriers. That implementation and the executed CLI qualify this benchmark; the API page's lingering “not yet functional” sentence is inconsistent with the tested release.

Memory64, median incremental ns/step, nine pairs:

| Engine | random store | page-aligned store | cross-page unaligned store |
|---|---:|---:|---:|
| ordinary JIT full/unwind | 2.341 | 2.344 | 2.377 |
| ROS JIT full/unwind | 2.305 | 2.372 | 2.373 |
| ordinary interpreter full | 5.173 | 8.487 | 8.536 |
| ROS interpreter full | 5.269 | 8.231 | 8.334 |
| Wasmtime 49.0.1 | 3.199 | 3.055 | 3.051 |
| WasmEdge 0.17.1 JIT | 2.294 | 2.400 | 2.346 |
| WasmEdge 0.17.1 interpreter | 76.275 | 97.070 | 96.792 |
| WAVM nightly 2026-04-05 | 2.796 | 2.792 | 2.841 |

Wasmer 7.4.2 rejected all three memory64 fixtures at semantic qualification, so it is excluded rather than ranked. All accepted high samples exceeded 100 ms. Actual product JIT ELF cache objects for both products were decoded and disassembled: random/aligned unwind guest function: 60 instructions, two conditional branches, one call **only on checksum-failure exit**, zero `lock` prefixes; unaligned guest function: 48 instructions, four conditional branches, one failure-only call, zero `lock` prefixes. The extra cross-page preflight branch preserves all-or-nothing protected-store behavior. The normal random-store loop has direct guest memory loads/stores and no per-access software guard object, call, lock or fence. Instruction tracing adds seven instructions and two calls to those guest functions. Hardware mmap protection remains enabled.

## GC layout experiment and release gate

An **isolated header overlay**, not product code, combines object and value slots into one allocation (`gc_object.h` SHA-256 `7eca359a5d989c814967df49e9030e5e1402adb97a0154a82b2591a52ad9a602`). Current ordinary and ROS baseline header SHA-256 is `21f90b8edb9427f12feb710ea643011d5fba3a672ccd5a0ac76b0e1e16802a2d`. Source-bound ordinary/ROS baseline and candidate builds passed Linux ASan, UBSan and LSan edge probes (zero length, overflow, alignment, injected allocation failure and foreign lease).

For 4M native allocations and 10M indexed field set/get operations over 1,024 roots, nine reversed pairs gave baseline/candidate allocation speedup **1.268×/1.278×/1.012×** at 1/2/4 P cores in ordinary and **1.267×/1.199×/1.037×** in ROS. Median RSS fell from about 492 MiB to 431 MiB in both. The 1P indexed field path slowed about 2% in a supplemental reversed-order nine-pair retest (ordinary speedup 0.9816×, ROS 0.9807×). Baseline and candidate native `struct_get`/`struct_set` instruction sequences have equal counts and are identical after relocation-address normalization; the residual difference may reflect data layout/cache or thermal state. This candidate should not be described as a no-regression optimization without further review. It does not implement collection.

The opt-in GC release gate exited **2 (FAIL)** for ordinary JIT, ROS JIT, ordinary interpreter and ROS interpreter. With only 1,024 replaceable roots, median peak RSS rises roughly **1,098 MiB** between 1M and 10M allocations in each mode; the configured plateau limit is 64 MiB. Wasmtime copying's reference run stayed flat in this gate. Neither product has a validated positive `collection_count` in this gate; even a coincidental RSS plateau cannot pass without proven collection. This is a release blocker, independent of allocation throughput.

## Raw evidence in the Linux container

- GC ring: `/work/wasm3-resume-20260924/evidence/bench-core3-c7f9-43a37-gc-allocation-20260928/`; `gc-allocation/summary.json` SHA-256 `074ce9eb011af08c23ebdc1438623130e53b3f7a7e9adb20103313d4d869e9e2`.
- Memory64: `/work/wasm3-resume-20260924/evidence/bench-core3-c7f9-43a37-memory64-20260928/`; `memory64/summary.json` SHA-256 `df0aa32faab27018bcc07bda01b235d1f3a4cd727e3064314904fcf56c5c4557`.
- Actual product JIT objects: `/work/wasm3-resume-20260924/evidence/memory-codegen-ordinary-c7f9-20260928/` and `/work/wasm3-resume-20260924/evidence/memory-codegen-ros-43a37-env2-20260928/`; summary SHA-256 `e3d6f5c7a162dbdf737da82ca41df11446a6d0f52baf8e40583acae6e31f0b8f` and `8ddffc28f3df292d67811c11312a03a616031b215b6c5917b417bd6a30125648`. An earlier ROS attempt in `memory-codegen-ros-43a37-20260928` failed because LLVM tools lacked `libedit.so.0` in the loader path; it is not the passing evidence.
- Isolated GC layout ASan/UBSan/LSan: `/work/wasm3-resume-20260924/evidence/gc-single-block-edge-c7f9-43a37-20260928/`.
- Nine-pair layout A/B: `/work/wasm3-resume-20260924/evidence/gc-single-block-ab-ordinary-c7f9-20260928/` and `/work/wasm3-resume-20260924/evidence/gc-single-block-ab-ros-43a37-20260928/`.
- Supplemental 1P field retest: `/work/wasm3-resume-20260924/evidence/gc-field-one-recheck-c7f9-43a37-20260928/`.
- GC release gate: `/work/wasm3-resume-20260924/evidence/gc-release-gate-c7f9-43a37-20260928/`; `summary.json` SHA-256 `303f794e23a1f40d17f37c582da6229993e6a3e01cae6dc245bb94e99fc3cdb7`.

The [WebAssembly 3.0 change history](https://webassembly.github.io/spec/core/appendix/changes.html) covers more than these measured cases. This report does not claim the deterministic profile, real blocking wait/notify, exception throwing, VM-managed thread creation, other memory/table/GC operations, or full Core 3.0 conformance have been performance-qualified.
