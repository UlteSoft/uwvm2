# Paired platform validation — 2026-09-15

This is a bounded evidence ledger, **not an all-platform certification**. The worktrees are shared and contain other, uncommitted changes. A passing frozen snapshot does not validate every later edit in either checkout.

## Resources and retained evidence

This campaign alone is limited to **32 GiB aggregate RAM, 28 GiB memory.high, zero swap, CPUs 16–31** on SSH host `linux`. All its build/test scopes are children of `uwvm-comprehensive.slice`; the other agent's separate 32 GiB allocation is not included. CPU affinity is also set with taskset. Compilers normally run one at a time per build.

An earlier mistaken 16 GiB setting killed one build; this is not a successful 32 GiB test. Later systemd-oomd pressure kills also interrupted module builds below the 32 GiB limit. Interrupted builds are neither passes nor source failures. The configured ceiling does not prevent the host's independent pressure policy from killing a scope.

Evidence root on Linux: `/tmp/uwvm-comprehensive.eUFnxH` (called **R** below). Extracted SDKs and module outputs were moved to `/var/tmp/uwvm-comprehensive.kH4XCt`, with symlinks retained. Exactly 215 already-extracted download archives (1,502,743,000 bytes) were deleted; `R/logs/unpacked-download-cleanup.json` records their identities. Source, extracted toolchains, checksums, successful outputs and diagnostic evidence were preserved. Deleted archives can be downloaded again.

Local evidence: `/tmp/uwvm-comprehensive-audit.i2TDbc` (**L**). Do not delete these directories until the follow-up builds have finished and evidence has been archived.

## Snapshot boundaries

Initial main HEAD: `d936f9bb6be987d24e82139b7fc23400e586e7c3`.
Initial ROS HEAD: `97cb90b7b28f9fe76a74de0a1a34389578e9de33`.
Both snapshots also contain then-existing uncommitted work; the manifests, not HEAD alone, identify the source.

Baseline full CLI binaries:

- Main SHA-256: `088b591c18d6f4d51c45543650803d9f6e97a7aff6569044ff9774c7cc8a5e4d`.
- ROS SHA-256: `b768c7f4894d928799f58b9ba7a60b923df816dc95c79090ece41ceb653ffb5f`.

These LLVM 22 binaries **precede the final native-stack cache, SIMD scalar-target, native-thread and Mach-O relocation fixes described below**. The main binary has no build-source ID and correctly refuses persistent cache use; ROS has an ID and was used for authenticated cache-hit checks.

`R/full-modules` is a different snapshot, containing the final native stack guard as of its build start, but not every subsequent SIMD/thread/cache/Mach-O edit. Its complete named-module build was still running when this ledger was written. No complete fresh paired build containing all latest edits has passed yet.

## Completed checks

| Scope / snapshot | Result | Evidence under R |
| --- | --- | --- |
| Baseline main full/lazy/tiered trap matrix | 5,184 exact stack/trap checks passed | trap-tests/full-nodump |
| Baseline ROS supported full-JIT trap matrix | 756 passed | trap-tests/ros-supported |
| Baseline deeper recursion / non-OOM traps | Main 250, ROS 50 passed | unwind-recursive-full-max; unwind-recursive-ros-pbo3 |
| Baseline feature integrations | 65 per product passed | integration-full; integration-ros |
| Baseline SIMD memory edges / grow | 652 per product passed | memory-full; memory-ros |
| Baseline start/initialization ordering | 98 passed | start-order |
| No-inline / retained-call policy | Both products passed O1/O2/O3/Os/Oz | noinline-policies |
| i686 shared-FP matrix | 168/168 passed: GCC/Clang, O0/O3, x87/SSE2/SSE4.1 | fp-i686 |
| Bootlin ABI/ISA shared-FP matrix | 390/392 passed; two LLVM backend failures remain | fp-bootlin; fp-mipsr6-o3-retry |
| x86_64 FP normal / no-SSE build matrix | 85/112 passed; 27 disabled-SSE ABI/compiler build failures remain | fp-x86 |
| uwvm-int directed combinations | 36/36 passed with host ASan/UBSan/LSan | strict-directed; strict-directed-fp-log |

The 36 directed cases are three tests (scalar FP, scalar memory variants, Wasm2 externref table) across four combine modes and three delay-local modes. They are **not** the entire strict suite or every ABI/build-flag Cartesian product. The initial heavy-mode harness wrongly disabled FP logging while requesting test diagnostics; removing that test-only flag fixed the build. The original failing log is retained separately.

The shared-FP Bootlin profiles cover RV32 glibc/musl, MIPS N32 BE/LE, MIPS32r6 LE, AArch64 BE and ARMv5 soft-float, using GCC/Clang and O0/O3. Two MIPS32r6 Clang O3 SIMD compilations still fail inside LLVM's backend; a serial retry reproduced the failures. x86_64 no-SSE failures include “SSE register return with SSE disabled”; do not silently label those configurations supported or change ABI assumptions to suppress diagnostics.

## Latest native-stack guard

The paired guard was checked with GCC 15 and Clang 22, O0/O3 and UBSan on Linux; the signal-install tests cover both optimization levels, both compilers and both products (48 scenarios). Darwin paired O0/O3/UBSan tests also pass. Cross-executed guard tests pass on i686, AArch64, ARMhf, PPC32, PPC64 BE/LE, RV64, s390x and LoongArch64.

Important repairs and reasons are commented in `uwvm_runtime_native_stack_guard.h`:

- Publish the saved host signal action before a wrapper can observe it.
- Preserve the host action's mask/flags and one-shot semantics.
- Stop repeated pthread-key destructor re-entry from allocating orphaned alternate stacks. Darwin can reset TLV storage between destructor passes, so a TLS boolean alone is insufficient; the resource-free retired-key marker matters.

Negative controls reproduced the original publication/mask/destructor failures. QEMU tests explicitly skip unsupported live RLIMIT mutation and instead exercise small pthread stacks. This does not prove every backend has equivalent native stack probing.

The final cached guard versus the earlier cached baseline, in alternating CPU-time microbenchmarks: main entry 683.14 → 682.81 ns; pthread entry 504.03 → 504.18 ns; host-provided alternate stack 347.57 → 346.89 ns; nested entry 5.2 → 5.2 ns. These measurements do not compare against the original uncached 45.8 µs design. Evidence: `stack-final-performance`, `stack-cross-latest`, `logs/stack-latest-retirement-marker.log`; local Darwin evidence under `L/mac-stack-latest`.

## Latest SIMD generation

`run_simd_cross_codegen.py` records source/tool hashes, compiler/runner commands and stage logs, and kills whole process groups on timeout. The full pipeline includes required target FP legalization, final vector legalization and the production FP entry guard before the C oracle.

All 15 configured profiles passed **236 opcode cases (lane variants and 256 inputs) plus 134 guarded-store cases per profile**:

- x86_64 SSE2; i686 x87 and SSE2.
- AArch64 NEON and no-NEON/no-SVE; ARMv7 NEON/VFP4.
- PPC32 G4/AltiVec; PPC64 BE and LE/VSX.
- RV64 GC and GCV.
- s390x z13 vector; LoongArch64 LSX and LASX.
- SPARC64 UltraSPARC, using native-NaN-aware oracle settings.

Evidence: `simd-production-harness/results.json`, per-target IR/object/assembly and logs. Some guest libc builds require a newer emulated CPU (PPC64LE power10; RV64 max); that is not proof that those libc binaries run on older processors.

The fixes are target-gated and explain the compiler/ABI failure in code:

- AArch64 without NEON lost bitmask lanes 8–15.
- i686 without SSE2 could fail f64 min/max selection or call an f32-return helper using x87 while the private integer FP ABI expected EAX.
- PPC32 without VSX could emit an invalid 64-bit GPR load for f64 min/max.
- SPARC constrained demotion could call unavailable `__truncdfsf2`; ordinary native conversion plus an input-NaN bit guard preserves the required result. Builder constrained-FP state is restored immediately.

The cache fingerprint includes `llvm-simd-scalar-lowering=scalar-target-contract-v2`, so reusing a project version/source ID cannot replay objects from the old scalar-target contract. Latest full-CLI cache replay after this fingerprint change is still pending.

For x86_64 SSE2, AArch64 NEON and PPC64LE VSX, all 236 checked IR and assembly outputs were byte-identical before/after these workarounds (`simd-fastpaths`). This is not a claim that every instruction on every architecture is call-free.

## Native unwinder provider and Mach-O repair

Default Linux CLI tests used libgcc's `_Unwind_Backtrace`, not LLVM libunwind. Deliberately preloading a mismatched registration/backtrace provider makes the live probe reject forced native mode; automatic mode safely retains exact instruction-based frames. Six automatic-provider cases passed. Evidence: `unwind-provider-probe`, `unwind-provider-auto`. Such incompatible preloads are not supported fast-path configurations.

A new regression, `llvm_jit_native_unwind_provider.cc`, uses the production section manager and call-boundary attributes. It checks five optimization pipelines, repeated engine lifetimes, two objects per engine and cached object replays. Each of 20 chains must contain four recursive activations plus the wrapper. It requires 10 object compilations and 30 cache replays.

Passed with Linux LLVM 22/libgcc and a matching LLVM 23/compiler-rt/libc++/LLVM-libunwind SDK; dynamic binding logs identify the actual unwinder (`native-provider-contract-multi`). The LLVM 23 SDK requires `-fno-rtti`; its LLVM library was built without RTTI.

On Apple arm64/LLVM 20.1.8, this test exposed incorrect registered FDE addresses. The AArch64 Mach-O loader handles explicit subtraction relocations and then the legacy FDE pass adjusts the location again. See [LLVM 20 RuntimeDyldMachO](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.8/llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldMachO.cpp) and [its AArch64 relocation implementation](https://github.com/llvm/llvm-project/blob/llvmorg-20.1.8/llvm/lib/ExecutionEngine/RuntimeDyld/Targets/RuntimeDyldMachOAArch64.h).

The paired section-manager repair records validated relocation pairs from immutable object bytes and reapplies their exact expressions before CFI registration. It is restricted to native Apple arm64 Mach-O; it neither guesses function ranges nor changes generated instructions. Reapplication is idempotent if LLVM has already relocated the field correctly. Invalid pairs make finalization fail closed. The original negative-control snapshot still loses all five JIT frames.

Both products pass the multi-object/cache regression under Apple Clang's ASan/UBSan with LLVM 20. The original single-object machine-code object is byte-identical before/after the repair. `macho_headers.h` shields colliding Darwin macros and restores them; the paired global-fragment fixture precompiles with Clang 20. The full named-module build is a separate, still-pending check.

Homebrew LLVM 20 ASan and TSan runtimes failed before main in dyld/sanitizer initialization on this Mac; those runs are not test passes or uwvm race findings. Apple sanitizer runtimes were used successfully instead. Sanitizing the C++ host does not automatically instrument JIT-generated instructions. Full macOS CLI trap validation after this repair remains pending.

Local evidence: `L/mac-simd-workarounds/*native-provider*`, `*macho-headers*`.

The isolated relocation fix and regression tests are committed as main `e6fa343a78ecfc21cc52d7fcbb7f937cc47e7785` and ROS `b98028eebb5f17541571c0c06417bf3334761a36`. Unrelated native-stack/module edits were deliberately left out of those commits.

## Paired performance and commits

A four-chain f32x4 mul/add recurrence, pinned to CPU 16, measured approximately 1.514 ns/iteration (main JIT), 1.533 (ROS JIT), 3.776 (local WAVM) and 32.194 (main interpreter). Runs used different long-loop counts for the interpreter and subtracted short-run CPU time. This is one kernel on baseline binaries, under concurrent background compilation—not an overall “2.5× faster” claim.

The actual ROS signed-cache replay was verified (`signature_verified=1`). Its loop has four vector multiplies and four adds, no calls or stack accesses. The actual WAVM object for this kernel has stack-resident vector locals. Wrapper calls outside the ROS loop remain. Evidence: `paired-vm-bench`, `ros-simd-loop-cache`, `logs/ros-simd-cache-hit.log`. WAVM binary SHA-256: `109b93af70817be9b6f7d3fa4807fe49e5a6ac18bc6f1e10eb9bab50790b2f01`.

Independent native-thread emplacement fixes were committed as main `ee54468b49864db6b3168ca263e3a866ba616905` and ROS `ef1c8a8a4dc9d63214a00a696bc6828a548cc2e3`. ROS receives only its retained pool code, not the removed lazy scheduler. Clang 20/libc++ callable-constraint recursion and the redundant temporary move are explained beside the changes. Paired functional and Apple TSan runs pass.

## Still required before claiming completion

- Fresh complete main and ROS builds containing **all** latest shared-worktree changes, followed by CLI regressions on those exact binaries.
- Complete paired named-module builds; the small Mach-O global-fragment fixture is not a substitute.
- Full CLI native/instruction trap matrices with the matching LLVM-libunwind SDK, and on macOS after the relocation repair.
- Remaining distro-ISA and ROS FP matrices, allocator configurations, all strict tests and all uwvm-int/LLVM policy combinations.
- Resolve or explicitly reject the remaining x86_64 no-SSE ABI builds and MIPS32r6 LLVM compiler failures.
- Additional OS/ISA/ABI coverage unavailable to these QEMU and SDK profiles, including SPARC32 execution and missing distro MIPS toolchains.
- Full semantic WAST coverage, stateful module registration scripts and unsupported proposals are not established by these directed suites.

Never enable an unchecked/native-only fast path merely because one microbenchmark or cross-generated object passed. Keep live probing and the instruction fallback unless the actual linked runtime and generated code contract have been established.
