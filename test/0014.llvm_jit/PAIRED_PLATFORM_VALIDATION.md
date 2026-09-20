# Paired platform validation — 2026-09-15

This is a bounded evidence ledger, **not an all-platform certification**. The worktrees are shared and contain other, uncommitted changes. A passing frozen snapshot does not validate every later edit in either checkout.

## Resources and retained evidence

This campaign alone is limited to **32 GiB aggregate RAM, 28 GiB memory.high, zero swap, CPUs 16–31** on SSH host `linux`. All its build/test scopes are children of `uwvm-comprehensive.slice`; the other agent's separate 32 GiB allocation is not included. CPU affinity is also set with taskset. Compilers normally run one at a time per build.

An earlier mistaken 16 GiB setting killed one build; this is not a successful 32 GiB test. Later systemd-oomd pressure kills also interrupted module builds below the 32 GiB limit. Interrupted builds are neither passes nor source failures. The configured ceiling does not prevent the host's independent pressure policy from killing a scope.

Evidence root on Linux: `/tmp/uwvm-comprehensive.eUFnxH` (called **R** below). Extracted SDKs and module outputs were moved to `/var/tmp/uwvm-comprehensive.kH4XCt`, with symlinks retained. Exactly 215 already-extracted download archives (1,502,743,000 bytes) were deleted; `R/logs/unpacked-download-cleanup.json` records their identities. Source, extracted toolchains, checksums, test-result logs and diagnostic evidence were preserved. Deleted archives can be downloaded again.

After the directed sanitizer matrix completed, its 36 successful, inactive ELF executables (4,902,710,312 bytes) were also removed from the two exact `strict-directed*` directories. No recursive directory deletion was used. `R/logs/completed-strict-cleanup.json` retains each executable's SHA-256, size and rebuild/run commands; sources and toolchains remain available for regeneration, but the original executable bytes are not backed up. Failed cases, assembly evidence, active module outputs and the other agent's directories were not touched. These deletions freed about 5.97 GiB in total (archives on disk, sanitizer executables on tmpfs).

Local evidence: `/tmp/uwvm-comprehensive-audit.i2TDbc` (**L**). Do not delete these directories until the follow-up builds have finished and evidence has been archived.

The completed fresh CLI evidence is backed up locally as `L/latest-cli-evidence-20260915.tar.gz`. The FP/no-inline/MIPS follow-up has a separate 602-file, content-verified backup, `L/validation-followup-evidence-20260915.tar.gz` (SHA-256 `20944702fb94ea1f3eb1d8568c87d8fc75610245c254c715a3b4a6ee776e62b2`). It contains commands, results, original failures, IR/diagnostics and source-parity records, not SDKs or full compiler binaries. The matching remote copies remain under R.

After diagnosing the terminated LLVM 22 module build, 1,210 inactive runtime-target BMI intermediates (8,038,458,168 bytes, about 7.49 GiB) were removed from the exact `full-modules/build-y/.gens/uwvm_runtime` directory. The main-target BMIs needed by the location-space reproducer, source, dependency commands and logs remain. `R/logs/obsolete-runtime-bmis-cleanup.json` records every deleted file's hash/size/inode. These obsolete BMIs are incompatible with the fresh LLVM 23 build; their original bytes are not backed up, but they can be regenerated.

## Snapshot boundaries

Initial main HEAD: `d936f9bb6be987d24e82139b7fc23400e586e7c3`.
Initial ROS HEAD: `97cb90b7b28f9fe76a74de0a1a34389578e9de33`.
Both snapshots also contain then-existing uncommitted work; the manifests, not HEAD alone, identify the source.

Baseline full CLI binaries:

- Main SHA-256: `088b591c18d6f4d51c45543650803d9f6e97a7aff6569044ff9774c7cc8a5e4d`.
- ROS SHA-256: `b768c7f4894d928799f58b9ba7a60b923df816dc95c79090ece41ceb653ffb5f`.

These LLVM 22 binaries **precede the final native-stack cache, SIMD scalar-target, native-thread and Mach-O relocation fixes described below**. The main binary has no build-source ID and correctly refuses persistent cache use; ROS has an ID and was used for authenticated cache-hit checks.

`R/full-modules` is a different snapshot, containing the final native stack guard as of its build start, but not every subsequent SIMD/thread/cache/Mach-O edit. Its LLVM 22/libstdc++ 16 named-module build terminated at 40% with `ran out of source locations`, **not OOM**. The isolated replay reports 2,140,802,865 bytes of loaded AST source-location space, about 99% of the compiler's address space. `bits/version.h` alone was entered 9,129 times, accounting for 874,147,395 file-location bytes plus macro locations. Raising physical RAM cannot fix that counter limit. Evidence: `module-location-probe`, `logs/full-modules-resume.log`. The failed `xmake --files` attempt separately failed dependency scanning and is not the compiler reproducer. Complete paired named-module builds remain pending; no default build policy or standard-library header was changed to hide the failure.

## Fresh paired LLVM 23 CLI closure

The 20:55 CST frozen source snapshots are under `/var/tmp/uwvm-comprehensive.kH4XCt/latest-72hOuQ` (local copies under `L/latest.72hOuQ`). All 3,457 main and 3,362 ROS manifest entries were rehashed after transport. Manifest SHA-256 values, also used as the build-source IDs:

- Main: `e8ba3a9077d87f7c35243d63e3772347d8eef833d9d77a81558c0754b753b870`.
- ROS: `067b5a85b8eb351e551e3f123f3adc940de687280c4548ca134f1fbacb2b6036`.

Both complete **header-mode** release builds passed with the matching LLVM 23/compiler-rt/libc++/LLVM-libunwind SDK, O3, native ISA, TLS, default memory mapping and each product's default combine configuration (main heavy, ROS soft). They contain the final cached native guard and the then-current shared-worktree repairs. They do not certify edits made after this freeze. Binary SHA-256:

- Main: `f55166e243fba2e7b0c1c05869ee71271627a2798d424c274cd08817307386ae`.
- ROS: `1235b91470c160b0684ef6963cbc6bc0c5e353202a938a4d7df8ab423b39bdbc`.

The 22:14 CST source-drift check covers all currently tracked/untracked `src/` files: 1,662 main and 1,609 ROS, no additions or removals since the freeze. In each product only `section_memory_manager.cppm` differs, by the later Apple/aarch64 global-fragment include documented separately in `MODULE_VALIDATION.md`. This is not a complete new module build. The current shared-source guard also passes 49 files plus four removed-mode paths; frozen/earlier guards covered 29/33 files, respectively (`logs/current-source-drift.json`, `logs/shared-parity-final.json`).

Dynamic binding logs identify `_Unwind_Backtrace` in the SDK's `libunwind.so.1` for both actual CLIs. `libgcc_s` is also loaded transitively; merely reading `ldd` would not establish which provider executed. The required native probe checks omitted logical frames and the exact native caller chain.

| Fresh binary checks | Main | ROS | Evidence under R |
| --- | ---: | ---: | --- |
| Full/lazy/tiered or ROS-supported exact trap/policy matrix | 5,184 | 756 | trap-tests/*-latest-llvm23 |
| Deeper recursive/non-OOM trap chains | 250 | 50 | latest-cli-full/recursive-corrected; latest-cli-ros/recursive |
| Feature/import-alias/DataCount integration | 65 | 65 | latest-cli-*/features |
| SIMD boundary/width/offset/grow checks | 652 | 652 | latest-cli-*/memory |
| Persistent signed-cache integration | passed | passed | latest-cli-*/signed-cache-integration.log |
| New authenticated-cache recursive trap regression | 108 runs / 72 trap replays | 108 runs / 72 trap replays | unwind-cache-latest-* |
| Native stack exhaustion | 285 runs across both products / 19 profiles, all passed | | latest-native-exhaustion |
| Start/initialization ordering | 98 runs across both products, all passed | | latest-start-order |

The first main recursive-driver invocation duplicated `-Rllvm-cache-path` in the campaign wrapper; the CLI rejected it before Wasm execution. That failure remains in `latest-cli-full/results.json`. Removing the duplicate wrapper option produced the separate successful `logs/latest-full-recursive-corrected.log`; no runtime source was changed for this harness mistake.

`check_native_unwind_cache.py` is paired and committed as main `a6e1ad077f2ef9e40011d3b1f7c8890a02377220` / ROS `b79ab20d95c95be06482459850afe457f0e65653`. It requires a verified-signature cache hit on each replay, eight repeated recursive activations plus their wrapper, three trap kinds and six CLI optimization policies. Instruction and checked-native policies are tested independently. The same entry is warmed with a nontrapping argument so normal shutdown can drain the asynchronous cache writer; a fatal cold run is not a reliable cache seed. A no-provenance baseline is correctly rejected by this test for failing its required cache-hit condition.

The first small cache smoke process was killed by systemd-oomd (9.8 MiB scope, host pressure policy) while two large compilations overlapped. No new parent hard-limit OOM-kill event occurred. The auxiliary FP matrix was explicitly interrupted, and the successful paired full builds plus cache regressions completed before resuming heavy FP compilation serially. Initial failure/interruption evidence remains in `logs/latest-cache-smoke-pressure.log` and `logs/latest-fp-interruption.json`.

Fresh shared rounding checks also pass with Clang 22: 1,584 per product, 16 helper-free functions per product, including eight single-instruction stack-free SSE4.1 packed roundings. These are shared-evaluator checks, not a substitute for complete CLI execution. Evidence: `rounding-latest-{full,ros}-clang22`.

The paired no-inline unit also passes with the matching LLVM 23 SDK, checking actual retained calls after O1/O2/O3/Os/Oz optimization as well as mandatory unoptimized function attributes (`latest-noinline-policies-v4`). The expanded regression is committed as main `1ac3b2225` / ROS `90bebb9c`. The earlier v1/v2/v3 attempts failed at test-program linking, before any assertions: unavailable zstd development linkage, an accidentally shadowing non-PIC distro libunwind archive, and a dangling extracted zstd symlink. The successful build selects the SDK's matching unwinder and the host's real zstd runtime explicitly; no runtime protection or optimization policy was changed to bypass a test.

Actual latest interpreter binaries: 54 selected scalar/SIMD add specializations across both products have no helper calls or native-stack accesses and retain tail dispatch (`latest-int-assembly-v2`). SIMD operand-stack loads/stores are still present and are not native stack spills. Clang uses packed additions plus unused-lane clearing for some scalar register-cache variants; requiring only ADDSS/ADDSD would be a false failure.

The actual signed-cache JIT benchmark objects are byte-identical between products. Their nonempty executable `.ltext` section is 227 bytes; the loop is four `vmulps` / four `vaddps`, decrement and branch, with no calls or native-stack accesses. Wrapper calls remain outside the loop. Evidence: `latest-jit-assembly-v3`. Earlier inspection attempts lacked the SDK loader path or compared empty `.text` sections; those attempts are not executable-byte validation. The corrected check discovers disassembled sections and rejects empty bytes.

Latest rotating CPU-time kernel measurements on E-core 16: main JIT 1.530 ns/iteration, ROS JIT 1.523, WAVM 3.779, main interpreter 29.787 and ROS interpreter 33.094 (`paired-latest-vm-bench`). Other background builds were present. This is one kernel, not an overall speed ranking or a causal comparison against LLVM 22: the toolchain and snapshots both changed.

The fresh ROS i686 matrix now passes **168/168**: six GCC/Clang x87/SSE2/SSE4.1 profiles, 14 scalar/boundary/tail/SIMD test sources, O0 and O3. All 116 resumed cases completed, and the merge with the earlier 52 passes requires exactly 168 unique `(target, source, optimization)` tuples, successful compilation and execution, and the frozen ROS source identity. Evidence: `fp-latest-ros-i686-combined.json`, `fp-latest-ros-i686-resume`, `logs/fp-latest-ros-i686-resume.log`. The original interruption is retained; this is shared FP/opfunc coverage, not a full i686 JIT CLI build.

## MIPS32r6 compiler failure diagnosis and test-oracle repair

The two earlier Clang 22 O3 SIMD failures also reproduce with the available LLVM 23 snapshot `4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89` on the fresh main source (`fp-mips23-latest`, 0/2). The MIPS SDK, O32 ABI, NaN2008 encoding, CPU and optimization level were retained; the compiler host uses its own matching libc++ loader paths while the MIPS guest retains libstdc++/libgcc. These failures happen during compilation, not Wasm execution or QEMU.

Retained optimized IR plus `llc -verify-machineinstrs` identifies invalid machine instructions instead of only the final `MCInst 0` diagnostic:

- The SIMD NaN test's independent rounding oracle is inlined into `check<unsigned>()`; condition-register (`fgr64cc`) spills become illegal PHI stores/loads after greedy allocation. Isolating only `fp_rounding_oracle::expected()` fixes this test. The paired, Clang/MIPS-r6-only `noinline` attribute leaves production SIMD evaluators, O3 optimization, integer-bit inputs/results and expected values unchanged. All four main/ROS × Clang 22/23 builds and QEMU runs pass. Commits: main `8267a5b3e`, ROS `11192c66`. Evidence: `mipsr6-machine-probe`, `mipsr6-oracle-guarded-*`; original failures are retained as negative controls.
- The separate SIMD bit test has invalid `$d2_64 = PHI killed $at` copies after pseudo expansion in the multi-operation f64 test driver. Its independent, target-gated invocation adapter now passes all four main/ROS × Clang 22/23 O3 builds and QEMU runs (`mipsr6-bits-guarded-*`). It preserves the real optimized opfunc, explicit three-argument template specialization, byref/tail modes and every original bit assertion. Commits: main `6f968ac8c`, ROS `4b24fdf4`; the latter also tracks the previously untracked ROS regression. The first prototype omitted that template argument pack and failed before execution; its harness error remains in `mipsr6-bits-isolated`. Original backend failures and IR remain in `mipsr6-machine-probe-uwvm_int_simd_fp_bits`.

The four native-x86_64 preprocessed-token comparisons (two test components × both products) are identical before/after these guards: approximately 2.53 million tokens for each bit test and 144,868 for each oracle. Evidence: `nonmips-test-token-equivalence`. The exact completed token-output intermediates (1,554,296,222 bytes, about 1.45 GiB) were removed after retaining their semantic digests and rebuild commands. These tests do not constitute a rerun of the entire cross matrix or the newly guarded MIPS O0 cases.

Crucially, `fixtures/mipsr6_fp_select_backend.cpp` reduces the copy failure to an independent scalar-condition vector select followed by pseudo-minimum, with no UWVM headers or library calls. Both unpatched MIPS backends fail; native x86_64 control compilations pass (`mipsr6-independent-backend-repro`). **2026-09-16 update:** the independent copy defect and the separate condition-class spill defect are now repaired in a tested [downstream LLVM backend patch](../../documents/toolchain/mips-r6-llvm-backend.md), with paired production-emitter execution and failing stock negative controls. This does not automatically patch an installed Clang/libLLVM or establish complete MIPS JIT/CLI coverage. The earlier eight directed O3 passes remain test-driver-isolation evidence, not backend-fix evidence.

These test-driver findings do **not** fix LLVM's backend itself or establish complete MIPS JIT/CLI coverage. The [upstream MIPS register-copy/spill implementation](https://github.com/llvm/llvm-project/blob/llvmorg-22.1.8/llvm/lib/Target/Mips/MipsSEInstrInfo.cpp) is relevant to the emitted opcode-zero diagnostics; the [similar upstream report](https://github.com/llvm/llvm-project/issues/181442) is background, not evidence that our reproducer is identical to that issue. No global optimization disable, reduced precision or runtime safety-check removal was introduced.

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

The cache fingerprint includes `llvm-simd-scalar-lowering=scalar-target-contract-v2`, so reusing a project version/source ID cannot replay objects from the old scalar-target contract. Fresh paired full-CLI signed replay now passes on the LLVM 23 snapshots above; cross-ISA persistent-cache replay is still separate work.

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

- Revalidate any shared-worktree changes after the 20:55 freeze; fresh paired header-mode builds and CLI regressions for that exact freeze are complete.
- Complete paired named-module builds; the small Mach-O global-fragment fixture is not a substitute.
- Full macOS CLI native/instruction traps after the relocation repair; the matching Linux LLVM-libunwind CLI matrices now pass.
- Remaining distro-ISA and ROS FP matrices, allocator configurations, all strict tests and all uwvm-int/LLVM policy combinations.
- Resolve or explicitly reject the remaining x86_64 no-SSE ABI builds and MIPS32r6 LLVM compiler failures.
- Do not mistake the MIPS test-oracle/driver isolation for a fix of the independent LLVM select/SIMD backend defect; validate complete MIPS JIT combinations and rerun affected guarded-test configurations separately.
- Additional OS/ISA/ABI coverage unavailable to these QEMU and SDK profiles, including SPARC32 execution and missing distro MIPS toolchains.
- Full semantic WAST coverage, stateful module registration scripts and unsupported proposals are not established by these directed suites.

Never enable an unchecked/native-only fast path merely because one microbenchmark or cross-generated object passed. Keep live probing and the instruction fallback unless the actual linked runtime and generated code contract have been established.
