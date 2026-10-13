# Current industry GC comparison candidates and managed family ports

Source audit on 2026-10-02; no runtime, compiler, VM, download or performance execution occurred on the development host. The accompanying candidate JSON records official release/asset digests and leaves actual executable hashes null. These are candidates to measure, not an assertion that any VM is the strongest.

The first comparison remains the exact current Wasm bytes on fresh, aligned native-TLS uwvm2/ROS and Wasmtime copying. The previous S6e measurement used the non-native-TLS compatibility profile; it cannot stand for a current production-default baseline. The v2 membership native component used six switches explicitly set to zero, not six omitted. The keeper separately reported v3's genuinely omitted-switch native component86checks/3success/8reject/9reclaimed; its raw packet is still pending independent review. Neither component is VM or performance qualification.

## Official version audit

| Candidate | Official version observed | Scope |
| --- | --- | --- |
| [Wasmtime](https://github.com/bytecodealliance/wasmtime/releases/tag/v49.0.1) | 49.0.1, September 24 | Same Wasm bytes; copying first. Exact tag commit 46c23a87dac1465986a8ad53ba6a7ae49372857b. |
| [Eclipse Temurin HotSpot](https://github.com/adoptium/temurin27-binaries/releases/tag/jdk-27%2B35) | JDK 27+35, September 21 | Java ports; start with explicit G1. The direct jdk.java.net/27 page returned 403; the official Temurin release is the distribution source used here. |
| [GraalVM CE](https://www.graalvm.org/release-notes/25.4/) | 25.4.4.1.1, September 22 | Same Java class files as Temurin; HotSpot G1 with Graal compiler. Release notes identify base OpenJDK25.0.4.1; the actual asset label is25.0.4.1.1. Not Native Image or GraalWasm. |
| [.NET](https://builds.dotnet.microsoft.com/dotnet/release-metadata/10.0/releases.json) | Runtime10.0.12 / SDK10.0.401, September 8 | C# ports; first workstation GC with TieredPGO. The11RC is not this stable baseline. |
| [Wasmer](https://github.com/wasmerio/wasmer/releases/tag/v7.5.0) | 7.5.0, October 1 | Fresh exact-module preflight; previous7.2.0 rejection results cannot establish7.5 support. |
| [WasmEdge](https://github.com/WasmEdge/WasmEdge/releases/tag/0.17.1) | 0.17.1, changelog July3 / published July6 | Exact-module semantic/RSS/collection controls. A short successful syntax probe does not replace a long workload. |
| [WAVM](https://github.com/WAVM/WAVM/releases/tag/nightly%2F2026-04-05) | Official listed nightly2026-04-05 | Prerelease explicitly labelled; exact required Core3 features may be unsupported. |

Wasmer7.5's [feature structure](https://raw.githubusercontent.com/wasmerio/wasmer/v7.5.0/lib/types/src/features.rs) has no GC field; this is a source warning, not an invented runtime result. WasmEdge0.17.1's [module allocator](https://raw.githubusercontent.com/WasmEdge/WasmEdge/0.17.1/include/runtime/instance/module.h) appends new arrays/structs to module-owned unique-pointer vectors. Reclamation evidence must therefore be obtained separately. Retain every exact module rejection, crash, timeout and unsupported flag; do not emulate missing instructions and call the bytes equivalent. Wasmtime null is only a no-collection lower bound. DRC's cycle limitation makes it a distinct diagnostic, not the copying replacement for cyclic families.

## Source-equivalent managed ports

New Java/Graal `managed_general/java/GeneralGc.java` and C# `managed_general/dotnet/Program.cs` contain eight separate loops, one per family/phase. The original immutable `ManagedRing` and original Wasm generator remain unchanged. Ports follow current `generate_general_gc.py` create/load/retained/cycle-check order and unsigned32 modular arithmetic. Java int overflow and C# explicit unchecked uint produce the same bit patterns.

| Family | Declared logical payload | Objects/group | Observable final roots |
| --- | --- | --- | --- |
| mutable-struct | One mutable32-bit numeric field | 1 | 1024 escaped boxes |
| reference-cycle | A and B each numeric32-bit value + mutable node reference; A↔B | 2 | 1024 A roots, 2048 reachable nodes |
| numeric-array | Eight mutable32-bit cells | 1 | 1024 escaped arrays |
| reference-array | A↔B plus eight mutable node references | 3 | 1024 arrays plus a second1024 A-root ring; 3072 reachable objects |

No extra wrapper surrounds either array. The second node-root container is allocated only for reference-array. Native language headers, alignment, compressed references, JVM/.NET array headers and uwvm token/lease metadata differ; the table is logical payload, not an equal physical allocation size claim.

Allocate performs n new groups, reads the displaced group's values before replacement after the first1024 steps, then mutates and reads the new group. Mutate initializes exactly1024 groups and performs n modifications with no main-loop new objects. Reference-cycle temporarily writes A.next=A, reads that self edge, restores B, reads B, and checks B.next=A. Reference-array writes B and A into selected/following cells and reads both values, retaining independent A roots. Every final ring slot is visited; width8 and cycle identity are checked, and step/root/state/return checksums must all match the independently prepared scalar values. Static global publication and final retained readback preserve escaping roots. Actual allocation telemetry and generated code still must confirm that an optimizer did not eliminate the intended allocation work.

The independent Python graph checker reconstructs actual A/B identities rather than the generator's scalar cell tags. Its160 value checks and two broken-restore controls passed locally. This is mathematical/source evidence only; Java/C# grammar, bytecode, JIT and native correctness are pending. Expected CLI values come from the scalar oracle before process launch, so O(n) oracle evaluation is not smuggled into measured guest execution.

## Measurement boundaries and collection proof

`execution_ns` covers Run, including mutation setup when applicable, LCG arithmetic, old-root readback, new allocation, field/array work and final complete root traversal. It excludes warmup and output/telemetry reads. It is not collector-only latency. Parent wait4 wall/user/sys/RSS cover the whole launched process, including managed startup, warmup, JIT compilation and output. Use separate cold (rounds0) and warmed (rounds8×250000) groups; never put a warmed managed internal timer and a cold Wasm process wall into one ranking. Fixed warmup is a recorded protocol, not proof that no tier transition remains during the ROI.

Java optional telemetry uses cached GC MXBeans and main-thread allocated-byte deltas; MXBean counts may sum different collector events, and collection-time sums are not unique pauses. .NET reports generation-specific collection deltas, total pause duration, main-thread allocated bytes and approximate process allocation. The marker writes occur before the baseline counters, and end markers after the ending counters. Diagnostics with telemetry/logging remain a separate group from unprofiled runs. Planned syntax allocation counts are not measured runtime allocation counts.

Allocate collector qualification requires actual positive reclamation/collection evidence and bounded RSS within the real cgroup. Mutation may legally report no collections and is only a field/lookup workload, never collector-qualified. The old eight-byte-per-step floor is not a proof of exactly one object allocation: obtain actual counter deltas, layout/runtime configuration, and allocation-path code evidence. Do not force an identical collection frequency across different autonomous heap policies.

A1GiB managed heap cap is an explicit configuration, not equal to uwvm's4096-object threshold or a1GiB RSS promise. Preserve the64GiB outer limit. Initial profiles use Java G1 `-Xms256m -Xmx1g` and .NET `DOTNET_gcServer=0`, `DOTNET_GCHeapHardLimit=0x40000000`, `DOTNET_TieredPGO=1`. Microsoft's [configuration reference](https://learn.microsoft.com/dotnet/core/runtime-config/garbage-collector) defines environment numeric heap limits as hexadecimal. Record .NET actual configured limit, heap budget and GC mode. [Oracle's collector guidance](https://docs.oracle.com/en/java/javase/27/gctuning/available-collectors.html) makes G1/Serial/Parallel/ZGC workload-dependent choices; no single collector is presumed strongest. After the first group, add separate actual supported collector cells on reference-array allocate and mutable mutate. ZGC latency and parallel throughput serve different goals. Four-P-core/server profiles are separate from P0 single-core data.

## Minimal keeper-only cold and timing queue

All commands below are inner commands, executed by the sole Linux keeper inside the current reviewed64GiB/swap-free scope. No direct host shell launch gives measurement qualification. Runtime/compiler/source artifacts and DSO closures must be frozen before/after; temperatures are recorded only. Builds on E16 and all native/VM/profiling tasks must be retired before P0 samples. JVM and .NET may use multiple actual TIDs: every TID must remain sameUID/exactcgroup/P0. The existing product pure-HW single-TID protocol must not be silently widened for them.

Set these task-specific variables from actual keeper inventory; unbound variables deliberately fail. Reuse already verified remote distributions where bytes match. New downloads, if needed, go to a private remote temporary tools directory; verify the accompanying official asset digest and extracted binary bytes, never pipe an installer into a shell.

```sh
: "${GC_SOURCE:?}" "${GC_OUT:?}" "${GC_JAVAC:?}" "${GC_DOTNET:?}"
taskset -c 16 "$GC_JAVAC" --release 21 -d "$GC_OUT/java" "$GC_SOURCE/managed_general/java/GeneralGc.java"
taskset -c 16 "$GC_DOTNET" build "$GC_SOURCE/managed_general/dotnet/GeneralGc.csproj" -c Release -o "$GC_OUT/dotnet" -maxcpucount:1 -p:UseSharedCompilation=false "-p:BaseIntermediateOutputPath=$GC_OUT/obj/" "-p:MSBuildProjectExtensionsPath=$GC_OUT/obj/"
```

Save raw compile commands/exit status, tool versions and all GeneralGc class files / .NET dll+deps+runtimeconfig+PDBs. Select a private DOTNET_CLI_HOME/NUGET_PACKAGES and offline-safe actual SDK closure; never repurpose HOME.

The small plan producer requires genuine existing tool/artifact pins and compile/version receipts:

```sh
python3 "$GC_SOURCE/prepare_managed_general_plan.py" --bindings "$GC_OUT/actual-artifacts.json" --out "$GC_OUT/cold65536-plan.json" --iterations 65536 --warmup-rounds 0
```

The plan schema is `uwvm-managed-general-artifacts-v1`: exact keys schema/openjdk/graalvm/dotnet/java_class_dir/java_artifacts/dotnet_artifacts/compile_receipts/version_receipts. Each tool or artifact is a path/SHA256 pair; artifact lists are nonempty. Classfile65.0 confirms Java release21 compatibility with both actual JDKs. The producer does not run, qualify or grant resource permission. Actual runtime library and compiler provenance review remains necessary.

Start24 managed cold commands (eight cells×three runtimes), rounds0/count65536, with every scalar field asserted. Bad expected checksum, invalid family/phase, invalid counts and broken-cycle/source controls remain failures. Then target reference-array allocate plus mutable mutate with n1M/2M, rounds8; initial ABBA pairs on P0 compare frequency distributions and preserve SMT noise/unknowns. Expand all eight long cells only after this small group has actual semantic/counter data. Use exact Wasm n1M/2M fixtures for uwvm/ROS/Wasmtime and record all required proposal flags and cache disable independently. Current general generator maximum2M stays unchanged; short same-byte WT results remain unqualified, and a later larger fixture needs its own reviewed source/oracle/cold binding.

Three evidence families remain separate: unprofiled whole-process wait4 plus internal execution; product single-TID pure grouped cpu_core raw cycles/instructions with exact enabled/running; and independent VTune HW samples with actual P0/PID/TID filtering, MUX/stack/mapping limitations. For managed runtimes, a main-TID count misses collector/compiler workers, so it cannot claim whole-VM hardware cost. A later multi-TID HW protocol needs independent ownership/ROI review. No software counter fallback replaces available hardware.
