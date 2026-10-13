# Next quiet Linux P-core qualification

This is a reproducible command sheet, **not** a result. Windows PE/Win11 or
Linux QEMU/Clang must first release the shared 64 GiB, swap-free cgroup. Run
inside `docker exec -it uwvm3-implementation sh` on SSH host `linux`; never
run these workloads on the macOS development host. The cgroup cpuset is
`0,2,4,6,16-31`; CPU `0,2,4,6` are P cores. Check `memory.events` and
`cpu.stat` before/after each phase. If another build/VM starts, discard that
phase. Record each pair's `scaling_cur_freq` and package temperature; the first
GC/memory64 run saw 68–100 °C drift, so near-throttle speed ranks require a
repeat after cooldown. Keep `memory.current` below 48 GiB before a managed
50-million-allocation process and confirm OOM/OOM-kill stay zero.

Exact current product IDs:

| Product | Source ID | O3 ELF SHA-256 |
| --- | --- | --- |
| ordinary | `sha256:c7f97991ba559981a71543aa4c692ec3b9ccd1cbd68057ce14469f7723062841` | `098f64a48cfe9261fef16bb42a4d593b2a390538eaf2505c1d678c6a213724fb` |
| ROS | `sha256:43a37bcffcae8dc85dba5f7e9896b0046da00bce8cdb1ea35a625f04a83a1c1a` | `842935987d5654f0a6d72f1f3e27fa0530fa92a1b4de45eccabaafd021d03b6a` |

The runner verifies source/build fingerprints independently. Copy the current
test-only runners/fixtures to the corresponding frozen candidate trees first;
the `src` and `third-parties` fingerprint must remain unchanged. Save `sha256sum`
of copied scripts and comparators. Distinct output directories are immutable.
The commands below use task-specific names and leave `HOME` unchanged.

```sh
export UWVM_TEST_CPUSET=0,2,4,6,16-31
CORE3_WORK=/work/wasm3-resume-20260924
export LD_LIBRARY_PATH=/toolchain/lib/x86_64-unknown-linux-gnu:/toolchain/lib:/work/deps/usr/lib/x86_64-linux-gnu:$CORE3_WORK/tools/host-libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
CORE3_ORDINARY_SRC=$CORE3_WORK/candidates/core3-c7f97991-ordinary-linux-test
CORE3_ROS_SRC=$CORE3_WORK/candidates/core3-43a37bcf-ros-linux-test
CORE3_ORDINARY_BUILD=$CORE3_WORK/builds/ordinary-c7f9-all-combine-delay-o3
CORE3_ROS_BUILD=$CORE3_WORK/builds/ros-43a37-all-combine-delay-o3
CORE3_ORDINARY_ID=sha256:c7f97991ba559981a71543aa4c692ec3b9ccd1cbd68057ce14469f7723062841
CORE3_ROS_ID=sha256:43a37bcffcae8dc85dba5f7e9896b0046da00bce8cdb1ea35a625f04a83a1c1a
CORE3_WASMTOOLS=$CORE3_WORK/tools/wasm-tools-1.259.0-x86_64-linux/wasm-tools
CORE3_WASMTIME=$CORE3_WORK/tools/wasmtime49-unpacked/wasmtime-v49.0.1-x86_64-linux/wasmtime
CORE3_JAVA=$CORE3_WORK/tools/temurin27-unpacked/jdk-27+35/bin/java
CORE3_JAVAC=$CORE3_WORK/tools/temurin27-unpacked/jdk-27+35/bin/javac
CORE3_GRAAL=$CORE3_WORK/tools/graalvm254-unpacked/graalvm-community-25.4.4.1.1+1.1/bin/java
CORE3_DOTNET=$CORE3_WORK/tools/dotnet-10.0.401/dotnet
CORE3_NODE=$CORE3_WORK/tools/node26-unpacked/node-v26.10.0-linux-x64/bin/node
CORE3_OUT=$CORE3_WORK/evidence/followup-c7f9-43a37-20260928
```

The selected comparator executables are Wasmtime 49.0.1 SHA-256
`c36ef7e6548600b09bcb34cb3ee4c138f3e7b01893347ec7f48fdb8e6050aa94`,
Temurin JDK 27 `306db8c7d74a1b2b8aadc0b82a52adfa564c4855d746a36b70c5304c45d23af6`,
GraalVM CE 25.4.4.1.1 Java `489bc8ef27770e9d222d8e55f0a8f5aa0800f821eb01ba2479e2ffd5882c0f8b`,
Node 26.10.0 `ab9c8eecf9f82d6693cdc3accced17034065c8d96213b0aa76a7e803d20ae1da`,
and .NET SDK 10.0.401 launcher
`01d89e0a0191052bfea616cd4ce624c8faf13b05bbddf7f64499c23e2a9d9269`
(runtime 10.0.12). Hash and version the actual binaries again at execution.

## Build-only stage on E core 16, outside the P-core timing interval

Run only after the Windows high-RSS window releases. The build scripts pin
`javac`, .NET compilation, and C++ host probes to E core 16; do these in a
separate low-contention window before formal timing. They save source, artifact,
compiler and exact O3 product hashes.

```sh
python3 "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/build_managed.py" \
  --out "$CORE3_OUT/managed-build" --javac "$CORE3_JAVAC" --dotnet "$CORE3_DOTNET"
python3 "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/build_managed_threads.py" \
  --out "$CORE3_OUT/managed-threads-build" --javac "$CORE3_JAVAC" --dotnet "$CORE3_DOTNET"
python3 "$CORE3_ORDINARY_SRC/test/0017.runtime/run_wasm_thread_performance.py" \
  --base-source-root "$CORE3_ORDINARY_SRC" --runtime-build "$CORE3_ORDINARY_BUILD" \
  --expected-source-id "$CORE3_ORDINARY_ID" --wasm-tools "$CORE3_WASMTOOLS" --out "$CORE3_OUT/vm-thread-ordinary" --variant all
python3 "$CORE3_ROS_SRC/test/0017.runtime/run_wasm_thread_performance.py" \
  --base-source-root "$CORE3_ROS_SRC" --runtime-build "$CORE3_ROS_BUILD" \
  --expected-source-id "$CORE3_ROS_ID" --wasm-tools "$CORE3_WASMTOOLS" --out "$CORE3_OUT/vm-thread-ros" --variant all
python3 "$CORE3_ORDINARY_SRC/test/0017.runtime/run_wasm_wait_notify_performance.py" \
  --base-source-root "$CORE3_ORDINARY_SRC" --runtime-build "$CORE3_ORDINARY_BUILD" \
  --wasm-tools "$CORE3_WASMTOOLS" --expected-source-id "$CORE3_ORDINARY_ID" \
  --out "$CORE3_OUT/wake-ordinary" --build-only
python3 "$CORE3_ROS_SRC/test/0017.runtime/run_wasm_wait_notify_performance.py" \
  --base-source-root "$CORE3_ROS_SRC" --runtime-build "$CORE3_ROS_BUILD" \
  --wasm-tools "$CORE3_WASMTOOLS" --expected-source-id "$CORE3_ROS_ID" \
  --out "$CORE3_OUT/wake-ros" --build-only
```

The thread runner's build phase performs a short semantic run; its default
output is **not** formal timing. The later `--run-only --measure` phase reuses
hash-verified host binaries. For the current isolated GC block-registry v3,
copy only the single header with SHA-256
`1080f56b8b723002342ac7131aea4a28d9c2f7445694a4d96d5825d502ddaeeb`
to a separate overlay. Then use `run_gc_block_registry_ab.py --build-only`
for Linux ASan/UBSan/LSan, a 1P/4P **shared-owner one-store** control, and
the 1P/4P 64-store-switch fixture; never mix
its output with the earlier single-block-layout candidate. The v3 overlay
retains v2's store-cookie TLS cache and owner-transition repair, then
distinguishes token-ID exhaustion from block-record allocation failure.
The sanitizer stage also compares a
source store that publishes an object under unique ownership and only later
moves into `shared_ptr`. The baseline must resolve this object from another
store and hold/release its foreign lease. The earlier v2 overlay SHA
`253835d1aa99e8df507e102b0480ebab4d4fa1fbfc09b8d2c029ff74b09ed1e8`
failed this exact case on Mac under ASan/UBSan: the record was not registered
while `weak_from_this()` was empty. V3 stages a raw owner under the stripe
and promotes it only during a foreign lookup; if it fails this case,
**stop before timing or merge**. Old v1/v2 results remain historical.

```sh
python3 "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/run_gc_block_registry_ab.py" \
  --baseline-product "$CORE3_ORDINARY_BUILD/uwvm" \
  --expected-source-id "$CORE3_ORDINARY_ID" \
  --overlay-root "$CORE3_WORK/overlays/gc-block-registry-v3" --candidate v3 \
  --out "$CORE3_OUT/block-registry-v3-ordinary" --build-only --pairs 9
python3 "$CORE3_ROS_SRC/benchmark/0004.wasm3-core/run_gc_block_registry_ab.py" \
  --baseline-product "$CORE3_ROS_BUILD/uwvm" \
  --expected-source-id "$CORE3_ROS_ID" \
  --overlay-root "$CORE3_WORK/overlays/gc-block-registry-v3" --candidate v3 \
  --out "$CORE3_OUT/block-registry-v3-ros" --build-only --pairs 9
```

The overlay root above was staged with only that header and its SHA verified
from both the SSH host and container; verify both again before use. Mac v3
ASan/UBSan plus the shared one-store semantic control passed under a 4 GiB
watcher; that is not Linux LSan or formal P-core timing evidence.

After both build-only runs pass sanitizer checks, run the A/B timing in
two separate, sequential quiet P-core windows. The one-store control holds
its store in a `shared_ptr`, so the candidate must actually register its
weak owner; it allocates four million objects and checks 10 million dynamic
field reads/writes. The switching fixture allocates one million objects
while changing among 64 stores per worker. Both modes alternate
baseline/candidate order over nine pairs, validate retained references, and
record allocation, teardown and RSS; the switching mode also records token
span. This is a native token-index experiment, not same-Wasm throughput or
proof of GC reclamation. Treat samples shorter than 100 ms as exploratory.

```sh
python3 "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/run_gc_block_registry_ab.py" \
  --baseline-product "$CORE3_ORDINARY_BUILD/uwvm" \
  --expected-source-id "$CORE3_ORDINARY_ID" \
  --overlay-root "$CORE3_WORK/overlays/gc-block-registry-v3" --candidate v3 \
  --out "$CORE3_OUT/block-registry-v3-ordinary" --run-only --pairs 9
python3 "$CORE3_ROS_SRC/benchmark/0004.wasm3-core/run_gc_block_registry_ab.py" \
  --baseline-product "$CORE3_ROS_BUILD/uwvm" \
  --expected-source-id "$CORE3_ROS_ID" \
  --overlay-root "$CORE3_WORK/overlays/gc-block-registry-v3" --candidate v3 \
  --out "$CORE3_OUT/block-registry-v3-ros" --run-only --pairs 9
```

Do not combine v1/v2/v3 samples or run them beside Clang, QEMU or the Windows
VM. Mac old-v2 semantics/ASan preflight found a
worker-endpoint span of 1,047,817 for one million 1P round-robin allocations,
compared with 1,023,998,977 for v1; the 4P old-v2 endpoint span was 999,235.
These historical values are **not valid all-issued-token ranges**: a worker
can restore an older TLS block when it switches stores, so its last token
need not be its greatest token. The old raw samples are retained. The revised
fixture reduces the first and last reference from every store after join,
outside the allocation interval. Each store has a single writer and issues
monotonically increasing tokens; the fixture checks `token_span >= allocations`
as a sanity condition, not as a uniqueness proof. It records the old reduction
separately as `token_endpoint_span`. Revised v3 Linux results must use the
new fixture hash and `all_store_first_last_single_writer` method.

## Quiet P-core nine-pair stage

Run these sequentially, preferably after package temperature returns below
the initial preflight range. The managed ring is **cross-language analogous**:
Java/Graal use HotSpot G1 with `-Xmx1g`, .NET uses tiered PGO with a
`DOTNET_GCHeapHardLimit=0x40000000` 1 GiB committed-GC-heap cap and explicit
`DOTNET_gcServer=0` workstation GC, and Node/V8
uses a 1 GiB **old-space** cap. The old-space cap is not a total-heap cap.
All implement a mutable 1024-root array and check the same 32-bit LCG/final
root checksum after eight 250,000-step JIT warmups, but they execute different
bytecode/native code and collector semantics. Do not put them in the same
ranking as Wasmtime or UWVM's **same-Wasm** `.wasm` measurements.

```sh
python3 "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/run_managed.py" \
  --out "$CORE3_OUT/managed-ring" --ordinary "$CORE3_ORDINARY_BUILD/uwvm" \
  --ros "$CORE3_ROS_BUILD/uwvm" --ordinary-source-id "$CORE3_ORDINARY_ID" \
  --ros-source-id "$CORE3_ROS_ID" --openjdk "$CORE3_JAVA" --graalvm "$CORE3_GRAAL" \
  --java-class-dir "$CORE3_OUT/managed-build/java" --dotnet "$CORE3_DOTNET" \
  --dotnet-dll "$CORE3_OUT/managed-build/dotnet/ManagedRing.dll" \
  --node "$CORE3_NODE" \
  --node-script "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/managed/node/managed_ring.mjs" \
  --build-manifest "$CORE3_OUT/managed-build/build.json" \
  --cpu 0 --low 5000000 --high 50000000 --pairs 9
python3 "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/run_managed_gc_diagnostics.py" \
  --out "$CORE3_OUT/managed-gc-events-one-p" --ordinary "$CORE3_ORDINARY_BUILD/uwvm" \
  --ros "$CORE3_ROS_BUILD/uwvm" --ordinary-source-id "$CORE3_ORDINARY_ID" \
  --ros-source-id "$CORE3_ROS_ID" --openjdk "$CORE3_JAVA" --graalvm "$CORE3_GRAAL" \
  --java-class-dir "$CORE3_OUT/managed-build/java" --dotnet "$CORE3_DOTNET" \
  --dotnet-dll "$CORE3_OUT/managed-build/dotnet/ManagedRing.dll" \
  --node "$CORE3_NODE" \
  --node-script "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/managed/node/managed_ring.mjs" \
  --build-manifest "$CORE3_OUT/managed-build/build.json" \
  --cpu 0 --affinity 0 --count 50000000 --samples 9
```

The traced diagnostic is a separate run. It requires an observed collection
in each managed runtime. It also checks Java and .NET allocation counters for
at least eight allocated bytes per measured step, to reject a scalar-replaced
object workload. The .NET program reports `GCSettings.IsServerGC`, the
configured `GCHeapHardLimit` and the GC budget at the last collection; the
first two must agree with the selected workstation mode and 1 GiB cap. The
last collection's budget may be zero if none has occurred yet. Java G1/ZGC and
GraalVM CE HotSpot G1 trace pause
events; Node traces V8 pauses; .NET's public API exposes collection counts,
allocated bytes and **total** pause duration, but not per-pause quantiles.
Only call p95 qualified after ≥20 pause events and p99 after ≥100; nine process
samples alone do not qualify a p99. If .NET records zero collections, treat
that as a failed diagnostic, not a zero-pause GC success. GC memory plateau
and `collection_count>0` still fail for current UWVM products.

```sh
python3 "$CORE3_ORDINARY_SRC/test/0017.runtime/run_wasm_exception_performance.py" \
  --source-root "$CORE3_ORDINARY_SRC" --expected-source-id "$CORE3_ORDINARY_ID" \
  --uwvm "$CORE3_ORDINARY_BUILD/uwvm" --wasm-tools "$CORE3_WASMTOOLS" \
  --wasmtime "$CORE3_WASMTIME" --out "$CORE3_OUT/eh-caught-ordinary" \
  --compare-wasmtime --samples 9 --normal-count 20000000 --throw-count 800000 --cpu 0
python3 "$CORE3_ROS_SRC/test/0017.runtime/run_wasm_exception_performance.py" \
  --source-root "$CORE3_ROS_SRC" --expected-source-id "$CORE3_ROS_ID" \
  --uwvm "$CORE3_ROS_BUILD/uwvm" --wasm-tools "$CORE3_WASMTOOLS" \
  --wasmtime "$CORE3_WASMTIME" --out "$CORE3_OUT/eh-caught-ros" \
  --compare-wasmtime --samples 9 --normal-count 20000000 --throw-count 800000 --cpu 0 --ros
python3 "$CORE3_ORDINARY_SRC/test/0017.runtime/run_wasm_uncaught_exception_performance.py" \
  --ordinary "$CORE3_ORDINARY_BUILD/uwvm" --ros "$CORE3_ROS_BUILD/uwvm" \
  --ordinary-source-id "$CORE3_ORDINARY_ID" --ros-source-id "$CORE3_ROS_ID" \
  --wasm-tools "$CORE3_WASMTOOLS" --wasmtime "$CORE3_WASMTIME" \
  --out "$CORE3_OUT/eh-uncaught" --pairs 9 --cpu 0
```

The caught fixture distinguishes plain calls, EH-enabled calls with no
throw, and one-in-sixteen real cross-function throw/catch. `instruction` and
`unwind` are separate stack-record policies. The uncaught fixture verifies
colored, detailed `structref` and `exnref` diagnostics each time; each process
throws **once**, so only full-process latency is reportable. Do not derive a
nanosecond-per-throw figure from that row.

```sh
python3 "$CORE3_ORDINARY_SRC/test/0017.runtime/run_wasm_thread_performance.py" \
  --base-source-root "$CORE3_ORDINARY_SRC" --runtime-build "$CORE3_ORDINARY_BUILD" \
  --expected-source-id "$CORE3_ORDINARY_ID" --wasm-tools "$CORE3_WASMTOOLS" --out "$CORE3_OUT/vm-thread-ordinary" \
  --run-only --measure --variant timed --affinity single --cpu 0
python3 "$CORE3_ORDINARY_SRC/test/0017.runtime/run_wasm_thread_performance.py" \
  --base-source-root "$CORE3_ORDINARY_SRC" --runtime-build "$CORE3_ORDINARY_BUILD" \
  --expected-source-id "$CORE3_ORDINARY_ID" --wasm-tools "$CORE3_WASMTOOLS" --out "$CORE3_OUT/vm-thread-ordinary" \
  --run-only --measure --variant timed --affinity four --cpu 0
python3 "$CORE3_ROS_SRC/test/0017.runtime/run_wasm_thread_performance.py" \
  --base-source-root "$CORE3_ROS_SRC" --runtime-build "$CORE3_ROS_BUILD" \
  --expected-source-id "$CORE3_ROS_ID" --wasm-tools "$CORE3_WASMTOOLS" --out "$CORE3_OUT/vm-thread-ros" \
  --run-only --measure --variant timed --affinity single --cpu 0
python3 "$CORE3_ROS_SRC/test/0017.runtime/run_wasm_thread_performance.py" \
  --base-source-root "$CORE3_ROS_SRC" --runtime-build "$CORE3_ROS_BUILD" \
  --expected-source-id "$CORE3_ROS_ID" --wasm-tools "$CORE3_WASMTOOLS" --out "$CORE3_OUT/vm-thread-ros" \
  --run-only --measure --variant timed --affinity four --cpu 0
python3 "$CORE3_ORDINARY_SRC/test/0017.runtime/run_wasm_wait_notify_performance.py" \
  --base-source-root "$CORE3_ORDINARY_SRC" --runtime-build "$CORE3_ORDINARY_BUILD" \
  --wasm-tools "$CORE3_WASMTOOLS" --expected-source-id "$CORE3_ORDINARY_ID" \
  --out "$CORE3_OUT/wake-ordinary" --run-only --pairs 9
python3 "$CORE3_ROS_SRC/test/0017.runtime/run_wasm_wait_notify_performance.py" \
  --base-source-root "$CORE3_ROS_SRC" --runtime-build "$CORE3_ROS_BUILD" \
  --wasm-tools "$CORE3_WASMTOOLS" --expected-source-id "$CORE3_ROS_ID" \
  --out "$CORE3_OUT/wake-ros" --run-only --pairs 9
```

The thread fixture checks native `pthread_create` counts and a matched
same-thread kernel, then times fresh VM-managed host entries with 1 or 4
workers; a four-worker threaded-minus-direct difference includes parallel
speedup. The guest wait/notify rendezvous fixture uses two genuine host threads,
one enrolled in guest `memory.atomic.wait32` and another performing guest
`memory.atomic.notify` with return value one. It does not qualify kernel sleep.
Per-wake latency excludes the one-time worker creation but includes host-entry
and host completion observation. The
single-thread mismatch and empty-notify CLI cases are separate **fast paths**,
not wake latency. The [threads proposal](https://github.com/WebAssembly/threads/blob/main/proposals/threads/Overview.md)
leaves thread creation/join to the embedder; it defines no guest spawn opcode.

The separate **Linux parked-wait** fixture pins the notifier to P core 0 and
the waiter to P core 2. Before each measured notify, it samples task state `S`,
reads a `futex` wchan, and samples state `S` again. The successful guest notify
must return one. It records qualification-read time/probe count separately
from wake latency; those reads are outside the timed interval. These are
sequential `/proc` observations, not an atomic kernel snapshot. Missing/zero
or non-futex wchan fails qualification rather than silently becoming a
rendezvous result. The interpretation follows the
[Linux kernel proc documentation](https://docs.kernel.org/filesystems/proc.html).
Build it after v3 A/B and before a separate quiet timing interval:

```sh
python3 "$CORE3_ORDINARY_SRC/test/0017.runtime/run_wasm_parked_wait_notify_performance.py" \
  --base-source-root "$CORE3_ORDINARY_SRC" --runtime-build "$CORE3_ORDINARY_BUILD" \
  --wasm-tools "$CORE3_WASMTOOLS" --expected-source-id "$CORE3_ORDINARY_ID" \
  --out "$CORE3_OUT/parked-wake-ordinary" --build-only
python3 "$CORE3_ROS_SRC/test/0017.runtime/run_wasm_parked_wait_notify_performance.py" \
  --base-source-root "$CORE3_ROS_SRC" --runtime-build "$CORE3_ROS_BUILD" \
  --wasm-tools "$CORE3_WASMTOOLS" --expected-source-id "$CORE3_ROS_ID" \
  --out "$CORE3_OUT/parked-wake-ros" --build-only
python3 "$CORE3_ORDINARY_SRC/test/0017.runtime/run_wasm_parked_wait_notify_performance.py" \
  --base-source-root "$CORE3_ORDINARY_SRC" --runtime-build "$CORE3_ORDINARY_BUILD" \
  --wasm-tools "$CORE3_WASMTOOLS" --expected-source-id "$CORE3_ORDINARY_ID" \
  --out "$CORE3_OUT/parked-wake-ordinary" --run-only --pairs 9
python3 "$CORE3_ROS_SRC/test/0017.runtime/run_wasm_parked_wait_notify_performance.py" \
  --base-source-root "$CORE3_ROS_SRC" --runtime-build "$CORE3_ROS_BUILD" \
  --wasm-tools "$CORE3_WASMTOOLS" --expected-source-id "$CORE3_ROS_ID" \
  --out "$CORE3_OUT/parked-wake-ros" --run-only --pairs 9
```

Each policy gets nine AB/BA process pairs, each process containing nine sets
of 128 qualified wakes after two warmups. This harness has not yet compiled
or executed on Linux while the Windows build owns the cgroup.

The managed-thread analog uses the same integer kernel but Java/.NET platform
threads and Node `Worker` isolates, so it belongs in a different table:

```sh
python3 "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/run_managed_threads.py" \
  --out "$CORE3_OUT/managed-threads-one-p" --ordinary "$CORE3_ORDINARY_BUILD/uwvm" \
  --ros "$CORE3_ROS_BUILD/uwvm" --ordinary-source-id "$CORE3_ORDINARY_ID" \
  --ros-source-id "$CORE3_ROS_ID" --openjdk "$CORE3_JAVA" --graalvm "$CORE3_GRAAL" \
  --java-class-dir "$CORE3_OUT/managed-threads-build/java" \
  --dotnet "$CORE3_DOTNET" --dotnet-dll "$CORE3_OUT/managed-threads-build/dotnet/ManagedThreads.dll" \
  --node "$CORE3_NODE" \
  --node-script "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/managed_threads/node/managed_threads.mjs" \
  --build-manifest "$CORE3_OUT/managed-threads-build/build.json" \
  --affinity single --cpu 0 --pairs 9 --rounds 32
python3 "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/run_managed_threads.py" \
  --out "$CORE3_OUT/managed-threads-four-p" --ordinary "$CORE3_ORDINARY_BUILD/uwvm" \
  --ros "$CORE3_ROS_BUILD/uwvm" --ordinary-source-id "$CORE3_ORDINARY_ID" \
  --ros-source-id "$CORE3_ROS_ID" --openjdk "$CORE3_JAVA" --graalvm "$CORE3_GRAAL" \
  --java-class-dir "$CORE3_OUT/managed-threads-build/java" \
  --dotnet "$CORE3_DOTNET" --dotnet-dll "$CORE3_OUT/managed-threads-build/dotnet/ManagedThreads.dll" \
  --node "$CORE3_NODE" \
  --node-script "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/managed_threads/node/managed_threads.mjs" \
  --build-manifest "$CORE3_OUT/managed-threads-build/build.json" \
  --affinity four --cpu 0 --pairs 9 --rounds 32
```

After the quiet timing phase, capture the *actual product* JIT object for
the same 1024-root GC ring. This is a code-generation audit, not another
timed sample. The disassembly and relocation log let reviewers count guest
loop calls into allocation, table-set/get and bridge helpers; compare it with
the interpreter result before attributing the ~117 ns versus ~107 ns gap to
any one helper. Use distinct paths for the two source-bound products:

```sh
python3 "$CORE3_ORDINARY_SRC/benchmark/0004.wasm3-core/inspect_core3_memory_codegen.py" \
  --out "$CORE3_OUT/gc-jit-object-ordinary" --product "$CORE3_ORDINARY_BUILD/uwvm" \
  --expected-source-id "$CORE3_ORDINARY_ID" --wasm-tools "$CORE3_WASMTOOLS" \
  --case gc-allocation-ring --count 8192 --cpu 0
python3 "$CORE3_ROS_SRC/benchmark/0004.wasm3-core/inspect_core3_memory_codegen.py" \
  --out "$CORE3_OUT/gc-jit-object-ros" --product "$CORE3_ROS_BUILD/uwvm" \
  --expected-source-id "$CORE3_ROS_ID" --wasm-tools "$CORE3_WASMTOOLS" \
  --case gc-allocation-ring --count 8192 --cpu 0 --ros
```

Every runner keeps raw rows and checksums. Publish measured medians with
actual product/competitor SHA, Cgroup OOM=0/throttling=0, temperature/frequency
range and `<100 ms` qualification flags. The prior source-bound same-Wasm GC
ring/memory64 data and GC release-gate FAIL remain in
`RESULTS_20260928_CURRENT_CANDIDATE.md`; neither managed analog nor faster
allocation is evidence of a working UWVM collector.
