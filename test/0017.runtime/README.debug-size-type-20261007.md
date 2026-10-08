# Standard Wasm sizeof/size_t rank followup — 2026-10-07

Both repositories now give finite native C/C23/C++ (including selected Objective-C/Objective-C++) `sizeof` and the existing builtin `(size_t)` cast the standard Wasm ABI's canonical `unsigned long` rank. This is a primitive result identity, not complete typedef/CV/namespace reconstruction.

Previously `sizeof(int)`, `sizeof(1+2)`, nested sizeof, `(size_t)1`, arithmetic with long and conditional long arms returned the anonymous `unsigned integer` name on Wasm32. Exact captured predecessor bytes reproduce six cases per repository; the corrected source returns `unsigned long`. The same rank now participates in the existing usual numeric/conditional conversions. On standard Wasm32, `sizeof(int)+(long long)1` is `long long`; on standard Wasm64 it is `unsigned long long`.

The implementation unifies type-only and evaluated sizeof result construction. No operand declaration DIE is inherited by a computed size. Direct native extent callbacks must supply an unsigned, non-floating, non-Boolean guest-width extent. Compound sizeof remains unevaluated; dead sizeof arms query declarations without calling the extent or operand-value resolver. The default shared_numeric dialect, unknown language and TinyGo classification retain the generic numeric contract. No controller/mode/host-ASM access is added.

The supported finite C ABI already models 32-bit int and guest-width long. Clang's standard WebAssembly target [explicitly chooses unsigned long size_t for wasm32 and wasm64](https://clang.llvm.org/doxygen/Basic_2Targets_2WebAssembly_8h_source.html). That source also documents WALI's different ABI; **WALI is outside this finite profile and not qualified here**. The [C++ sizeof contract](https://eel.is/c++draft/expr.sizeof) specifies a size_t prvalue. Independent actual target compilation and embedded DWARF evidence, rather than the Linux host pointer width, supply the qualification below.

## Fresh frozen A5 qualification

All compiler/executable/functional Python tests ran on SSH Linux inside the original 64 GiB cgroup. Each own process tree was birth/PIDFD-bound and retired/reaped. Original budgets were preserved: DATA 1 GiB aggregate owned RSS, frontend 16 GiB owned RSS, 64 MiB shared output per role, 8 MiB file/1 MiB log, and the original admission/disk floors. Cross profiles share one output cap. There were no OOM/high/max events.

| Repository | Route | Phases/components | Result |
|---|---|---:|---|
| uwvm2 | native | 41 | PASS |
| uwvm2 | index | 66 | PASS |
| uwvm2 | frontend | 2 | PASS |
| uwvm2 | QEMU-ppc64 | 7 | PASS |
| uwvm2 | QEMU-x86_64 | 7 | PASS |
| uwvm2 | QEMU-aarch64 | 7 | PASS |
| uwvm2-ros | native | 41 | PASS |
| uwvm2-ros | index | 66 | PASS |
| uwvm2-ros | frontend | 2 | PASS |
| uwvm2-ros | QEMU-ppc64 | 7 | PASS |
| uwvm2-ros | QEMU-x86_64 | 7 | PASS |
| uwvm2-ros | QEMU-aarch64 | 7 | PASS |

Current totals:

- 2,163,432 repeated size_t/sizeof rank, value, sign, extent and callback checks; this is repeated finite coverage, not millions of distinct features.
- 960 genuine Wasm32/64 embedded metadata size-type proofs, with 80 freshly compiled size-type modules across C17, C23, C++20, Objective-C and Objective-C++, DWARF 4/5, O0/O2. Thirteen independent target static type assertions per module: 1,040 total.
- 40 additional fresh Wasm32 regression producers; 11,960 previous language index checks. Total fresh Wasm modules: 120.
- 4,067,120 logical preflight regressions and 1,360,000 scalar property checks; native character escape and old character/sizeof/narrow/rank/conditional/Boolean/Zig/DAP components were also repeated.
- 42 actual Linux target ELF/QEMU components across x86_64, big-endian PPC64 and AArch64, both repositories. Outputs match pinned successful x86_64 native component logs. Target machine/width/endianness, emulator and actual dependency/link provider hashes are retained.
- 148 source-frame/WASIp1 DAP protocol regression tests and the existing 595-positive/330-negative expression grammar corpus. No new live stop or full runtime language route is inferred from these tests.
- DATA RSS peak 785,309,696 bytes; frontend 2,195,693,568 bytes. Successful job-work sum 1280.174 seconds, not wall-clock/performance measurements.

The native followup deliberately uses new output leaves and separately pinned recipes inside the **same** original role budgets. A5 corrects the predecessor include ordering and fast_io string-view fixture, and performs the baseline first. Frozen source cuts and production implementation bytes remain unchanged.

## Retained attempts

All six failed jobs and six earlier passed metadata/frontend jobs are retained. A1's extent fixture supplied a declaration key without a valid native type. A2 native and metadata tests over-constrained the valid existing internal category for wide compound numeric results. A3's existing character escape test still required the old anonymous sizeof result spelling. Both A4 native attempts exposed a raw character pointer in the fast_io comparison fixture. A5 uses a string_view. Source review also corrected predecessor include priority, and A5 performs both old/new comparisons before the full suite. All six failures remain in evidence. All production implementation bytes have remained identical since A1. These are not erased or presented as never-failed runs. The artifact collector separately retained a pre-launch incomplete pin-map error and one legacy stdout-count compatibility failure. The corrected independent collector verified all results under the original guard; no source or fixture changed for these collector corrections.

## Evidence and remaining work

Source IDs:

- uwvm2: `sha256:8029123b9b5108bc08eb69e6f79eda638af3806565df26b53e82918aef098c3c`
- uwvm2-ros: `sha256:42ec5b4c917ac96d1b6e8eddc7834278f2b67c87e82000f49e297239f4206042`

Four earlier duplicate transport spools (61,223,471 bytes) were retired only after every regular extracted member matched its compressed source. Their exact extracted content is reconstructible from the verified main archive; no copy of the retired older gzip bytes is claimed. After archive verification, the current own upload spool and eight old source snapshot roots were retired with manifest/object/inode/hash checks. All old source cuts remain reconstructible; current A5 source, logs, ELF and external providers stay live.

Linux-only main archive: `/home/macromodel/Documents/uwvm3-implementation/debugger-bounded-20261006/size-type-a5/size-type-evidence-a5.tar.xz` (35,261,728 bytes), SHA-256 `a397af40481525dedea747ae19ac987967b659c2933f0ed01ba6be72ce984839`. All member hashes and Linux fsync verified. Ten attempt/repository source cuts reconstruct from content-addressed source objects and original manifests; the exact current upload spool, reconstructible older sources, recipes, logs, argv/dependencies, native/target ELF and emulators remain in the archive. External SDK/link provider archives are separately pinned; this is not a self-contained SDK bundle. Small records are copied to both hosts; no local large archive is claimed.

This closes the canonical primitive size_t rank for the standard finite Wasm C ABI. Full scoped typedef/namespace/CV/reference/glvalue/overload grammar, WALI/other ABI profiles, user-type/VLA/function sizeof, Rust/Go/TinyGo/Zig/AssemblyScript native semantics, fresh linked/live -Rdbg qualification and remaining architecture products are still unfinished. ROS full-mode policy and the Wasm-generated-code-only ASM boundary are unchanged. Source-cut results do not automatically qualify later concurrent edits or the latest entire working tree.

## Later concurrent workspace changes

Final review observed unowned changes after the A5 cut in both repositories: `src/uwvm2/runtime/lib/uwvm_runtime_native_owner_function_claims.h`, `src/uwvm2/uwvm/debugger/NATIVE_HOST_CALL_CONTINUATION.md`. These edits are preserved and are not qualified here. All eleven owned implementation/test paths remain byte-identical to A5. Exact old/new hashes are in the final small sidecar review; the latest entire workspace remains unqualified.
