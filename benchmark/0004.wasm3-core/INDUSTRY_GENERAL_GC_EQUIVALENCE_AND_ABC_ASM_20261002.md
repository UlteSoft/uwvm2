# Industry general GC equivalence and ABC codegen audit, 2026-10-02

This is a source/read-only audit beside the preserved ports, runners and version
metadata. No local native build/VM/SSH/download ran. The companion JSON pins the
reviewed files and source-only scalar expectations. Existing545 ABC and132 EH
Python checks are not VM or performance passes.

## Equal logical workload, separate physical representation

| Family | Logical payload/roots | Managed operation equivalence |
| --- | --- | --- |
| mutable-struct | mutable i32;1024 boxes | publish fresh box, set state+i, read back |
| reference-cycle | two i32+reference nodes A↔B;1024 groups | real classes; write self-edge, read, restore B; assert B.next=A |
| numeric-array | eight mutable i32 cells;1024 arrays | Java int[8]/C# uint[8]; exact selected/following update/read |
| reference-array | cyclic A/B plus eight refs;1024 arrays+1024 A roots | real ref arrays and separate A-root ring preserve3072 reachable objects |

All loops use seed123456789, LCG1664525/1013904223 modulo2^32, salt0xa5c31f27.
Allocate creates n groups, reads displaced groups after1024 steps and sums all
final roots. Mutate creates exactly1024 groups then performs n modifications.
Java int wrap and C# unchecked uint are the same bits. Width/cycle identities
and independent step/root/state/return checksums reject incorrect readback.
Logical programs match current generate_general_gc.py/WAT; object headers,
alignment, compressed refs, root containers and GC policy do not have equal
physical size or collection frequency.

Static ROOTS/NODE_ROOTS publication makes escape explicit, but source alone does
not prove that all allocation remains. [HotSpot escape analysis](https://raw.githubusercontent.com/openjdk/jdk/jdk-27-ga/src/hotspot/share/opto/escape.cpp),
[Graal partial escape analysis](https://www.graalvm.org/jdk25/reference-manual/java/compiler/)
and [.NET object allocation analysis](https://raw.githubusercontent.com/dotnet/runtime/v10.0.12/src/coreclr/jit/objectalloc.cpp)
can transform eligible allocations. Keep primary optimizations enabled. Obtain
untimed allocation-path code plus actual positive allocated-byte deltas and
1M/2M scaling; incidental managed allocations mean those bytes are not exact
source object counts. A disabled-EA diagnostic is a separate configuration.

Existing managed/node/managed_ring.mjs is the legacy immutable Box ring:
remaining-count indexing, final LCG result and root XOR outside the timed loop.
It has no new mutable/arrays/cycle loops or displaced-root step sums. Exclude it
from the new four-family comparison. An equivalent general-family JS port is
still missing; do not rename the legacy result as general GC.

Use identical official-valid Wasm bytes on uwvm/ROS/Wasmtime. Pinned
[v49.0.1 DRC source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/wasmtime/src/runtime/vm/gc/enabled/drc.rs)
has no tracing cycle collector: it cannot qualify cyclic reclamation. Its acyclic
diagnostic is separate. Pinned [copying source](https://raw.githubusercontent.com/bytecodealliance/wasmtime/v49.0.1/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs)
uses Cheney semi-spaces, bump allocation and updated roots. Record actual
collector selection, collections/reclamation evidence available from installed
instrumentation and bounded RSS; exit0 alone exposes no reclaimed count. Do not
invent one or use contradictory floating50dev docs for the installed49.0.1.

## Candidate versions and immutable source pins

Official [Wasmtime49.0.1](https://github.com/bytecodealliance/wasmtime/releases/tag/v49.0.1),
[Temurin27+35](https://github.com/adoptium/temurin27-binaries/releases/tag/jdk-27%2B35),
[GraalVM CE25.4.4.1.1](https://www.graalvm.org/release-notes/25.4/) and
[.NET10.0.12/SDK10.0.401](https://builds.dotnet.microsoft.com/dotnet/release-metadata/10.0/releases.json)
were reread on this date. Asset digests remain in the preserved
industry_general_gc_candidates_20261002.json, SHA
a0e18bf43fa853b369a05ae8e6f06407da84417c9f83142ca601f5f4488da522.
These are official candidates, not an assertion that any is strongest.
Actual executable hashes/version/SDK/DSO receipts remain keeper inputs.

Compile Java once with actual javac --release21 and use the identical65.0 class
files for Temurin and Graal. Pin every nested class. Pin all C# dll/deps/config/PDB
outputs and actual compiler receipts. Capture actual JDK heap/G1/oop/alignment
settings and .NET workstation-GC/heap-limit/tiering/runtimeconfig separately,
without diagnostic flags in timed argv. Retain the existing inherited-config
sanitation and stopped-child actual environment witness.

## Minimal existing-protocol keeper commands

All commands below are inner commands, not authority to launch directly.
Only the keeper admits E-core builds and compiler-retired P0 samples inside
the current64GiB/swap0 scope, fresh original-PIDFD ownership and each managed
TID's UID/P0/cgroup. Temperature is observed only.

First use already approved24-cell cold with actual artifact/SDK bindings:

    python3 -B "$GC_SOURCE/prepare_managed_general_plan.py" --bindings "$GC_OUT/actual-artifacts.json" --out "$GC_OUT/cold65536-plan.json" --iterations 65536 --warmup-iterations 250000 --warmup-rounds 0 --gc-telemetry
    python3 -B "$GC_SOURCE/run_managed_general_measurement.py" --plan "$GC_OUT/cold65536-plan.json" --admission "$GC_FRESH_ADMISSION" --sdk-closure "$GC_OUT/actual-sdk-closure.json" --out "$GC_FRESH_EVIDENCE" --execute

Unbound variables and absent actual pins are not runnable. The frozen runner
only admits65536/rounds0 and always keeps collector qualification false.
After actual24-cell closure, use the existing producer for a representative
2M warm plan, then review a narrow binding to the same multi-TID plain protocol:

    python3 -B "$GC_SOURCE/prepare_managed_general_plan.py" --bindings "$GC_OUT/actual-artifacts.json" --out "$GC_OUT/warm2M-plan.json" --iterations 2000000 --warmup-iterations 250000 --warmup-rounds 8 --gc-telemetry

The producer emits24 rows; initially select only reference-array/allocate and
mutable-struct/mutate × three engines. The frozen cold runner does not admit
this plan. Do not remove its checks or pretend warmed performance is ready.
Exact application args (same after actual java/classpath or dotnet/dll prefix):

    reference-array allocate 2000000 250000 8 3016573056 236603904 750095765 1224754256 2668464640 972744485 --gc-telemetry
    mutable-struct mutate 2000000 250000 8 39747584 3034316800 3673977237 3834106880 3853910016 1615514405 --gc-telemetry

Java prefix: actual java -Xms256m -Xmx1g -XX:+UseG1GC -cp actual-class-dir GeneralGc.
.NET: actual dotnet actual-GeneralGc.dll; actual environment gcServer0,
GCHeapHardLimit0x40000000, TieredPGO1 as frozen producer specifies.
A distinct unprofiled argv group omits --gc-telemetry; no silent replacement.
Warmup8×250000 verifies every checksum but does not prove tier stability.
No forced GC follows warmup; preserve and record carried heap/compiler state.

Internal execution_ns covers setup, loop and final roots, excludes warmup/output.
Parent wait4 wall/user/sys/RSS covers all managed TIDs/startup/JIT/warmup/output.
Collector counters never turn Run time into collector-only latency. Mutation is
field/lookup work. Product single-TID grouped HW cannot represent whole JVM/.NET
while omitting compiler/GC workers; its contract remains unchanged.

Wasmtime same-byte existing command uses -C cache=n,collector=copying and -W
all-proposals=n,bulk-memory=y,multi-value=y,reference-types=y,simd=y,function-references=y,gc=y,
then exact fixture. Keep actual installed-help/ELF/DSO/version binding.
_start selfchecks and untimed run-return oracle remain. Compare equal cold
wholeprocess boundaries first, never short missing-identity/frequency rows.

## WAVM same-platform support and build boundary

Read-only local /Users/liyinan/Documents/MacroModel/src/WAVM has HEAD
6f871e61c6e8fc54fa5317b5d61d128d681846f3. README identifies WAVM-MEMTAG,
a research fork; Lib/LLVMJIT/LLVMJIT.cpp has an uncommitted25-add/3-delete delta.
Its exact source/readback files are separately pinned in the companion JSON.
This is not the official WAVM release, and local macOS artifacts cannot serve
as Linux comparison binaries.

Local FeatureSpec includes memory64/legacy EH but no GC. OperatorTable has
legacy try/catch/rethrow and no struct.new/array.new/try_table/throw_ref.
The official [nightly2026-04-05](https://github.com/WAVM/WAVM/releases/tag/nightly%2F2026-04-05)
is commit4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09. Its
[pinned operator table](https://raw.githubusercontent.com/WAVM/WAVM/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Include/WAVM/IR/OperatorTable.h)
also lacks the required new GC/EH syntax and uses an older exception encoding.
Thus current four-family GC and try_table EH must be unsupported preflight
results, never slow/PASS. memory64 remains a candidate needing exact-byte cold.

First inventory existing remote WAVM ELF/source/commit/version/DSOs without
assuming that local builds were deployed. If a new official build is necessary,
its [pinned manual](https://raw.githubusercontent.com/WAVM/WAVM/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Doc/Building.md)
permits the minimal CMake/Ninja path with actual LLVM20+:

    taskset -c 16-31 "$WAVM_CMAKE" -S "$WAVM_SOURCE" -B "$WAVM_FRESH_BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DLLVM_DIR=$WAVM_LLVM_CMAKE" "-DCMAKE_C_COMPILER=$WAVM_CLANG" "-DCMAKE_CXX_COMPILER=$WAVM_CLANGXX"
    taskset -c 16-31 "$WAVM_CMAKE" --build "$WAVM_FRESH_BUILD" --target wavm --parallel 16

Use exact source/dirty/submodule, compiler/LLVM21-or-supported-LLVM closure,
CMakeCache/compile/link/RSP/MD and generated ELF/DSO before/after pins; all tasks
inside64GiB cgroup. No installer/sudo/global changes or automatic dev.py downloads.
Fresh built Linux same-platform memory64 candidate CLI, subject to actual help:

    "$WAVM_ELF" run --nocache --abi=none --function=_start --enable memory64 "$EXACT_MEMORY64_WASM"

GC/new EH preflight uses identical bytes, not legacy-syntax rewrites. Retain
rejection/unsupported status. Memory64 _start/run/checksum/steps and ROI must
match uwvm/Wasmtime; all P0 samples follow builds retiring, no launch here.

## ABC untimed assembly and GC slow paths

A omits experimental flags; B only COLLECTION_LOCAL_MEMBERSHIP1; C only
PRECISE_GC_TRACE_METADATA1. Same source per product/all3TU nativeTLS/macro vector,
complete actual fresh build and cold closure precede codegen inspection.
No actual ABC codegen/performance pass is asserted.

Initially capture untimed reference-array allocate2M and mutable-struct mutate2M
for each profile/repo; numeric-array/reference-cycle allocate are controls.
Existing llvm_object_cache.h capture takes an exact filename (not directory),
requires compile-time support and uses O_EXCL/O_NOFOLLOW/0600. Use a fresh private
path per single-module emission, full/cache-disabled/Rct0; don't overwrite or
capture parallel objects into one path. Save actual checksum/GC metrics, exact
source/profile/argv, object SHA/target/sections/symbols/relocations and actual
llvm-nm/readobj/objdump version/pins. Process-local helper addresses are not
portable callable code. Bind loaded ranges and contemporaneous maps before
mapping actual runtime PCs; anonymous ld.so.cache remains unknown.

| Region | Required actual assembly evidence |
| --- | --- |
| LCG/root ring | preserved loop/allocation/readback/publication, precise live-root spills/polls and helper count |
| get/set/refcast | exact ABI/trap statuses, successful output read dominated by success, no unrelated memory check/carrier/poll regression |
| allocation/publish | actual allocator/managed boundary, roots/retirement/exception edges; token remains key |
| B host collector | closed exclusive eligibility, acquire local chains and exact canonical owner/cohort; ineligible old global path preserved |
| C host collector | private ref-field/ref-array plan used, numeric classification saved; all real refs/exn edges/full preflight retained |
| sweep/failure | full graph before reclamation; cross-store leases/native exception roots, late-invalid zero reclaim, epochs/OOM intact |

B/C principally change host collector code: inspect actual runtime.o/finalELF
GC helpers or actual inlined enclosing function in addition to JIT objects.
Identical JIT bytes are not proof the flag is inactive. Use actual nm/address
witness, never lambda ordinal guesses. Test-only hit probes must not be timed.

All formal plain/HW/VTune samples clear capture in actual child environment and
retain actual stopped/loaded witness. Object capture remains untimed.
Internal Wasm, parent wholeprocess, typed-PMU whole single-TID guest with exact
ENA/RUN/FD/ACK, and VTune sampled-self/MUX/stack/maps are separate families.
Unknown SMT/loader/map limitations remain unknown. No promotion or ranking.
