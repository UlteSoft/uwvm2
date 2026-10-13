# Source-bound exception performance, 2026-09-28

These measurements use the immutable Linux O3 baseline, not the newer r2
product batch. Ordinary source ID is
`sha256:c7f97991ba559981a71543aa4c692ec3b9ccd1cbd68057ce14469f7723062841`,
ELF SHA-256 `098f64a48cfe9261fef16bb42a4d593b2a390538eaf2505c1d678c6a213724fb`.
ROS source ID is
`sha256:43a37bcffcae8dc85dba5f7e9896b0046da00bce8cdb1ea35a625f04a83a1c1a`,
ELF SHA-256 `842935987d5654f0a6d72f1f3e27fa0530fa92a1b4de45eccabaafd021d03b6a`.
Wasmtime 49.0.1 CLI SHA-256 is
`c36ef7e6548600b09bcb34cb3ee4c138f3e7b01893347ec7f48fdb8e6050aa94`.
The follow-up owned-payload implementation was not present in these binaries;
its future A/B requires a fresh source-bound runtime.

The official [Core 3 instruction specification](https://webassembly.github.io/spec/core/syntax/instructions.html)
is Release 3.0 dated 2026-09-21. This fixture uses a real tag and `try_table`
with a numeric catch in a different function. It does not use `catch_ref` or
`catch_all_ref`; this particular workload does not retain permanent exnref
roots. Each Wasm file checks the final state and expected throw count.

## Caught exceptions and frequent calls

Each profile ran nine samples in rotated/reversed profile and case order,
pinned to P core 0. The cgroup was exactly 64 GiB, swap-free, with cpuset
`0,2,4,6,16-31`; P cores are `0,2,4,6`. All compiles completed before the
measurement. Cgroup OOM, OOM-kill and CPU-throttling counters stayed zero.
The package temperature ranged approximately 66–95 °C with the powersave
governor; saved telemetry records sampled frequencies. Order rotation does
not prove a thermally invariant ranking.

Plain and EH-enabled no-throw cases each perform 200,000,000 calls. The
throw case performs 8,000,000 steps and exactly 500,000 cross-function
throw/catch operations. Times below are **whole-process medians in seconds**,
including startup/JIT, rather than an isolated per-throw cost. These cases
have different iteration counts and must not be subtracted from one another.

| Product/profile | Plain calls | EH enabled, no throw | 500,000 actual throws |
| --- | ---: | ---: | ---: |
| ROS int full | 5.463925 | 5.510216 | 4.176354 |
| ROS JIT full, unwind | 0.159177 | 0.158509 | 4.414034 |
| ROS JIT full, instruction | 6.936859 | 6.918666 | 2.821977 |
| Ordinary int full | 5.576254 | 5.633403 | 4.311590 |
| Ordinary JIT full, unwind | 0.157485 | 0.159862 | 4.686444 |
| Ordinary JIT full, instruction | 6.909187 | 6.891358 | 2.844874 |
| Wasmtime, ROS batch | 0.154802 | 0.161764 | 0.096420 |
| Wasmtime, ordinary batch | 0.154467 | 0.160615 | 0.095714 |

ROS completed 108/108 timed executions; ordinary completed 162/162 including
two CLI-selected tiered configurations. Their actual T0/T1/T2 marker
qualification is pending, so those additional timings are retained in raw
results but are not presented as a particular tier's performance. Unwind
substantially reduces the no-throw call bookkeeping in this fixture, but real
throw/catch is slower than instruction recording in the current implementation.
This is evidence for an exception-path optimization target, not an estimate
of the fraction spent in CFI or allocation. The source audit separately finds
native diagnostic stack walking before C++ unwinding and payload copying.

All nine-sample p99 estimates are explicitly unqualified. Wasmtime's throw
samples are below 100 ms; use them as exploratory comparisons and increase
counts before asserting a precise speed ratio.

The same-bytecode hashes in both products are:

| Case | Wasm SHA-256 |
| --- | --- |
| Plain | `03c56518a1d2ed9d0149791e3a241ddff773395e51bd48aeb67a9160d5bdba01` |
| EH/no throw | `f2b9df696835bdd0f759885b77862e4a3de56b506a96f3edc41e54b20a63be8a` |
| Actual throws | `560243a56c6b29dbe0337548fa96626db2c168e56ea69d88208cffa4dfc48560` |

## Uncaught diagnostic latency

Seven profiles (both products' int, JIT instruction, JIT unwind, and Wasmtime)
ran two payload cases, nine timed processes each, plus fourteen warmups.
All 126 timed processes passed the required exception payload, readable stack
and colored diagnostic checks. Each process throws once; this measurement
includes startup/JIT and formatted error output and is **not per-throw
throughput**. All medians are below 100 ms and p99 is unqualified.

| Profile | Struct payload, ms | Exn payload, ms |
| --- | ---: | ---: |
| Ordinary int | 62.563 | 68.173 |
| Ordinary unwind | 71.019 | 69.418 |
| Ordinary instruction | 69.729 | 68.655 |
| ROS int | 68.537 | 63.399 |
| ROS unwind | 70.305 | 70.457 |
| ROS instruction | 69.556 | 67.956 |
| Wasmtime | 3.467 | 3.041 |

## Raw evidence and reproduction

Remote root: `/work/wasm3-resume-20260924/evidence/followup-c7f9-43a37-20260928`.
The exact command files, all raw samples, self-checking Wasm/WAT, telemetry,
compiler/runtime/source fingerprints and logs are kept there. Small files
have also been copied to both repositories under
`build/wasm3-evidence/linux-current-{c7f9,43a37}-o3/performance-20260928/managed-and-eh/`.
The large complete source inventories remain remotely available and hash-bound.

| Remote subdirectory | Summary SHA-256 |
| --- | --- |
| `eh-caught-ros-env2` | `9bf8c48c1fea47471be60e36741926193f00f8f335035cca6d4f62a9aa071e81` |
| `eh-caught-ordinary-env2` | `eadccea6f9ddc42985bc7915b9023d35c8425eb8253b0ab4d0fc9015044ea43c` |
| `eh-uncaught-env2` | `f86e2ad34b403892cf0ac6217fa4847e91dcdf881e096687b8f49f0632f5a770` |

The caught runner is `test/0017.runtime/run_wasm_exception_performance.py`,
SHA-256 `af6c5b3e97c99c7d0cda7665c7ba8aabb0b0329fc0a1a76eb350dbdaa90bbfee`.
Use the [command sheet](FOLLOWUP_C7F9_43A37.md) with the explicit source IDs;
for these actual caught runs select `--samples 9 --normal-count 200000000
--throw-count 8000000 --compare-wasmtime --cpu 0`. Use the env2 loader path
including `/work/deps/usr/lib/x86_64-linux-gnu`. Earlier missing-env/loader
failures were retained and were not included in timing.

GC collection and RSS gates still fail for these products. Correct exception
semantics or a faster no-throw call path does not satisfy the collector gate.
