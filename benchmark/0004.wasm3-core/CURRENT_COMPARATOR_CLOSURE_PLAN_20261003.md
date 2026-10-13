# Current reference-VM closure and finite comparison plan

This is source preparation, dated 2026-10-03. No executable, compiler, guest,
download, profiler, or new launcher was run on the development Mac. Every new
actual operation belongs to the Linux keeper, in the existing 64-GiB/swap-zero
cgroup. Compilers use E16–31 and comparable native runs use P0. The current R3
product build and four untimed JIT-object captures retain priority. Existing
historical rows, failed samples, tools, manifests, and raw packets are immutable.

## Official candidates and what is actually available

The direct official release routes were checked rather than relying on stale
search snippets. A release's tag is source evidence, not a locally measured
binary/provider identity.

| Candidate | Official source | Existing Linux evidence | New admission needed |
| --- | --- | --- | --- |
| Wasmtime 49.0.1 | [Versioned release](https://github.com/bytecodealliance/wasmtime/releases/tag/v49.0.1), 2026-09-24 | Historical executable and successful original 16M ring; its SHA is pinned in the companion JSON | Fresh version/help, ELF/loader/DSO and official artifact/source provenance; exact same-byte cold |
| Wasmer 7.4.2 | [Versioned release](https://github.com/wasmerio/wasmer/releases/tag/v7.4.2) | Historical executable; the old GC case was rejected | Fresh provider/backend/help/closure and exact unsupported diagnostic; no rejection is a slow score |
| WAVM nightly/2026-04-05 | [Official prerelease](https://github.com/WAVM/WAVM/releases/tag/nightly/2026-04-05), commit 4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09 | Historical executable; its attempted `--version` was wrong | Correct `version` and `help run`, actual loader/libLLVM/provider closure and prerelease provenance |
| WasmEdge 0.17.2 | [Versioned release](https://github.com/WasmEdge/WasmEdge/releases/tag/0.17.2), 2026-10-02 | Only historical **0.17.1** is inventoried | A distinct new directory, official asset/source pin, backend/LLVM/provider/plugin closure, exact fresh cold |
| Temurin/OpenJDK 27+35 | [Official release](https://github.com/adoptium/temurin27-binaries/releases/tag/jdk-27%2B35) | Historical Java executable/version and class source | Fresh JDK `release`, actual `libjvm`/loaded runtime closure, actual G1 selection, complete classfiles and compilation receipt |
| GraalVM CE 25.4.4.1.1 / JDK 25.0.4.1.1 | [Official release](https://github.com/graalvm/graalvm-ce-builds/releases/tag/graal-25.4.4.1.1) | Historical Java executable/version | Fresh actual JVMCI compiler/runtime closure and G1 profile; Java JIT rather than Native Image |
| .NET runtime 10.0.12 / SDK 10.0.401 | [Official .NET 10 download](https://dotnet.microsoft.com/en-us/download/dotnet/10.0), 2026-09-08 | Historical `dotnet --version` returned **SDK** 10.0.401 | Fresh `--info`/`--list-runtimes`, actual selected CoreCLR/GC/host libraries and compiled assembly/runtimeconfig/deps closure |

The official .NET download page maps SDK 10.0.401 to runtime 10.0.12. This does
not prove which runtime an existing process selected. .NET 11 RC is a separate
candidate and is not silently substituted for the current GA profile.

The current WasmEdge release repairs stale try-table handlers, nullable reference
operations, memory64 AOT bounds, LLVM 23 size optimization handling, and runtime
temporary allocations in loops. Consequently, the old 0.17.1 JIT SIGSEGV is kept
as an old failure and does not establish the result of 0.17.2.

Wasmtime's versioned [CLI implementation](https://github.com/bytecodealliance/wasmtime/blob/v49.0.1/crates/cli-flags/src/lib.rs)
exposes compiler/collector/cache and memory64/exceptions/threads choices. Its
collector comment still calls copying unimplemented, while the same tag's
[Cargo features](https://github.com/bytecodealliance/wasmtime/blob/v49.0.1/crates/wasmtime/Cargo.toml)
include `gc-copying`, `gc-drc`, and `gc-null`. The actual installed help and exact
cold must resolve build capability; a stale doc comment is not an unsupported
result. Cranelift is the explicitly selected backend. Null is a non-reclaiming
allocation bound and is not an equivalent GC-performance competitor.

Wasmer's [versioned feature API](https://github.com/wasmerio/wasmer/blob/v7.4.2/lib/types/src/features.rs)
exposes memory64/exceptions/threads and does not expose Core3 GC. Its official
[Wasmer 7 discussion](https://wasmer.io/posts/wasmer-7) describes Cranelift
exception support and libunwind integration. That alone does not qualify modern
`try_table`/`throw_ref` on a selected binary/backend. Exact cold and diagnostics
remain necessary; memory64 and EH can be independently useful comparisons even
if GC is unavailable.

Pinned WAVM's [feature enumeration](https://github.com/WAVM/WAVM/blob/4e82bb9fecf9c1bdb4d00f96fa89063ee4382d09/Include/WAVM/IR/FeatureSpec.h)
advertises memory64 and atomics. The existing pinned-source audit found no Core3
struct/array GC or modern `try_table`/`throw_ref` implementation. Its host runtime
`GCObject` management and older exception syntax do not replace those features.
It is therefore queued for memory64/atomic-worker capability, not a fake GC/EH
benchmark produced by lowering the input to older syntax.

## Fixed finite work, with the original byte contract

1. Admit the existing Wasmtime49 tool closure; do not download/build it again if
   fresh exact artifact/source/loaded-library evidence matches the official
   candidate. Run the original **191-byte** immutable ring, SHA
   `66874f9a4a0a5977b4ace1cc702f949c6c2627311dce0bcbe87c818c55516e3e`,
   with explicit Cranelift, Copying, disabled cache, and the original feature
   vector. Its 16,000,000 loop steps, 1024 exported table roots, LCG state
   **493211925**, table.get/cast/immutable-i32-get chain are fixed. A fresh exit
   zero is a new semantic receipt; the 2026-09-28 exit-zero row is only ancestry.
   No source change, alternate collector, native SDK-free storage chain, or
   mutable lock microbenchmark is accepted as that same input.
2. Admit the same-byte existing memory64 5M/50M fixtures and the modern EH
   8M/500K-catch fixture from the immutable v2 command list. Use 5M as a short
   functional control; actual timing admission still determines whether 50M is
   sufficiently long. The EH 64M/4M-catch source is separately pinned and needs
   its actual compiled-byte closure; it is not quietly substituted for 8M.
3. Compile the new bounded `reference_modern_eh_throw_ref_20261003.wat` once
   with the keeper's genuine pinned wasm-tools. It keeps a real exception ref
   in a global across `catch_all_ref`, two `throw_ref` uses, and two typed catches
   with checked payload 47. Run identical compiled bytes on admitted backends.
   This is a syntax/semantic control, not a meaningful performance sample.
   The new WAT has **not** been parsed, validated, or executed here.
4. Once the WasmEdge 0.17.2 binary/source/backend closure is actually available,
   cold the exact ring, memory64 and modern-EH inputs before any timing. A
   decoder rejection, unsupported backend, uncaught exception, crash, timeout,
   semantic mismatch and admission failure have distinct labels. Wasmer/WAVM
   unsupported syntax is recorded without timing a different substitute.
5. Compare the existing Java/Graal/.NET immutable ring **at the explicit new
   16M count**. Its source retains descending-index 1024-root semantics and a
   final per-slot verification, with independent expected root XOR
   **1488018432** and state **493211925**. Pin all actual classfiles/assemblies
   and compile provenance. Eight 250,000-step warmups remain outside the internal
   loop clock and count as 2M additional source allocations in whole-process
   measurements. Enable existing allocation/collection telemetry outside the
   loop timer. Zero observed allocation or zero collector activity cannot be
   called a qualified reclaiming-GC comparison. Observed byte counts alone do
   not prove an exact dynamic allocation count or an equal object layout.
6. Thread creation remains a separate host-embedding task. Core atomic RMW,
   wait-mismatch and notify-empty controls **do not create any threads**. The
   [threads embedding interface](https://webassembly.github.io/threads/core/appendix/embedding.html)
   and [Wasmtime multithreaded example](https://docs.wasmtime.dev/examples-multithreaded-embedding.html)
   require explicit host/Store/lifetime design. A shared worker Wasm export,
   identical imported shared memory, create/join count, admission, stack size,
   errors, and deterministic checksum must be bound for each native host API.
   Wasmtime WASI-thread imports are not treated as uwvm's managed-thread API,
   and no WASIp2/p3 work is introduced. Existing Java/.NET one/four platform
   thread sources can provide analogous create/join controls; they cannot
   authenticate that still-missing Wasm host bridge.

The new JSON is fixed source data, not a launcher. It adds no guardian,
counter-collection implementation, scheduler, download script, or environment
workaround. Single-TID raw-counter admission is not widened to JVM/reference
worker processes. A future genuine multi-TID counter profile requires its own
approved ownership/attribution protocol. Current reference comparisons use the
existing keeper's whole-process `wait4` accounting and actual per-TID admission.

After full cold/closure, use the existing serial paired plain protocol. Three
reversed pairs are sufficient for the first exact ring group, not another broad
native-component matrix. Capture must be disabled in timed product runs. Record
wall/user/sys/maxRSS, observed raw frequency series, exact tool/source/byte
identities and CG/OOM/throttling. System-wall timing includes compile, load,
instantiation, warmups and execution; the managed internal clock covers its loop
only. Hardware counts outside a true ROI include setup/cleanup and are never
renamed per-loop counts. Sub-100ms samples stay unqualified; a longer count
requires a new explicit source/oracle/byte contract. Temperature is observation
only; no arbitrary temperature rejection is introduced.

`check_managed_immutable_ring_20261003.py` checks the existing telemetry-enabled
producer's copied JSON schema, checksum, all-root XOR and numeric bounds. Its
last-root oracle was compared with five direct pure-Python loops. Four valid
synthetic receipts and 128 corruptions were checked, including boolean numeric
carriers, missing/extra fields, runtime-label mismatch, wrong heap selection and
checksum/count changes. These are pure source/receipt checks; they do not claim
a native compiler, VM, producer, scheduler, collector or performance pass.

The already reviewed EH A1 observer component remains a finite separate queue
item. Its paired temporary-provider off/on recipe and source hashes are in
`/tmp/uwvm-native-eh-phase1-component-source-20261003-r1/keeper-plan.json`.
Parent approval is separate from that immutable plan's historical readiness
fields. Retain default-OFF, the original eager diagnostic trace, prefix-only
completeness, matched static provider/ABI1, genuine C FDE/no-LSDA frames and all
150 off/on semantic cells. It does not replace uncaught full-stack diagnostics
or claim performance improvement. The Linux keeper owns every actual provider
download, build, link and execution after the current R3 priority group.
