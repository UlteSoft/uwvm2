# UWVM2 / UWVM2-ROS Wasm 2.0 paired audit

Date: 2026-09-15. This is an evidence ledger, not a claim of complete conformance or a proof of memory safety.

Subsequent native-stack entry caching is documented in
[NATIVE_STACK_GUARD.md](../0017.runtime/NATIVE_STACK_GUARD.md). The revision5
results below belong to the pre-cache source snapshot. Revision6 product/QEMU
validation was interrupted by the Linux host going offline and is not counted
as a pass for the newer implementation. Post-reboot work has resumed in a
disk-backed workspace under the user's updated 32 GiB aggregate cap, with swap
disabled. The fixed cached-entry snapshots now pass six Linux native unit runs,
seven QEMU guard architectures plus native i686, and complete non-module builds
of both products. They also pass 285 exhaustion, 108 directed and 98 start-order
replays. QEMU explicitly skips live RLIMIT mutation, which native Linux covers.
Both products also pass 652 SIMD boundary/growth checks and 65 integration
checks each; their production-cache SIMD loops match with no bridge call or
stack access. These results belong to the manifests in NATIVE_STACK_GUARD.md,
not to later local signal-handler edits or the older revision5 results below.
The later signal header has separate native/QEMU regression coverage.

A subsequent RISC-V64 host-address optimization removes a volatile stack
round-trip and O3-reintroduced literal-pool relocations. Both products pass the
exact production helper's O0/O3 static/PIC tests, including full-width pointer
values and actual high-address load/store/call. Its architecture-specific cache
revision is tested separately. See SIMD_DIRECT.md for scope and evidence.
That named-module attempt later failed on Clang's source-location address-space
limit. The new fixed libstdc++13 attempt and a confirmed Darwin module-dependency
repair are recorded in [MODULE_VALIDATION.md](../0014.llvm_jit/MODULE_VALIDATION.md).
Neither unfinished whole-product build is counted as a pass.

The next test-only follow-up executes 1,818,600 paired address decisions before
and after LLVM O3 and proves that an independent trap flag catches a fault the
old UINT64_MAX sentinel-only comparison missed. Real linear-memory accesses
are not performed by that oracle. See SIMD_DIRECT.md. The synchronization
guard now covers 49 files, including interpreter memory and the complete
linear-memory allocation tree; both copies pass nine checker-semantic tests.
ROS's removed execution modes remain removed. The ordinary existing SIMD BMI
also matches header-mode rounding assembly in eight wrappers; this is not a
whole-product module pass or ROS BMI coverage. See MODULE_VALIDATION.md.

That oracle subsequently exposed a test-only no-mmap expectation error. After
correction, both products pass six independent mmap/single-allocator/multi-
allocator builds and twelve baseline/O3 runs (5,455,800 address decisions).
Actual interpreter store-fault checks also pass on native x86_64 and QEMU
i686/AArch64: 804 cases, unchanged valid prefixes after traps, and identical
paired aligned-wrapper assembly without preflight branches or hardware fences.
These are focused helper tests, not a whole-engine completion claim. Their
exact coverage, earlier compiler timeouts and source hashes are in SIMD_DIRECT.md.

A later native Apple arm64 specialization uses 16-KiB JIT stack probes only
after confirming a 16-KiB physical page on macOS. Both products' actual
generated-code suites pass with LLVM 20 and the installed LLVM 23 development
toolchain, while Linux-host IR remains unchanged in 12 paired comparisons.
The fixed Linux module builds predate that Apple-only follow-up. Detailed
coverage, toolchain failures and limits are in NATIVE_STACK_GUARD.md.

A fresh paired-source check then reproduced 36 ROS-only SSE2 SIMD rounding
failures out of 792 checks. The missing shared rounding/NaN updates are now
synchronized. A GCC-only x86 SSE4.1 specialization also removes scalar lane
rounding and stack temporaries. Both products pass the final native/cross
matrices and packed-instruction checks described in SIMD_AUDIT.md.
At that checkpoint check_wasm2_shared_parity.py verified 29 shared files (the
later module-dependency follow-up expands this to 33) while
preserving ROS's removed execution modes. Before its first build manifest, the
unstarted ROS module snapshot received the frozen ordinary shared implementation;
the active ordinary snapshot was not changed. Both snapshots now pass this
source-parity guard, but neither unfinished module build is counted as a pass.
A subsequent RISC-V-only follow-up removes duplicate result NaN classification
after already-quieted integral rounding. Both products' before/after versions
pass 2,880,000 independent-oracle lane checks across GCC/Clang RV64GC, RV64GCV
and Zfa configurations. Clang wrapper instruction counts decrease; GCC's
counts stay unchanged. SIMD_AUDIT.md records the separate snapshot and limits.

## Scope and correspondence

ROS retains eager/full execution only. This audit does not restore lazy compilation, tiered execution, or OSR. The four common execution paths are ordinary UWVM2 full interpreter/full LLVM and ROS interpreter/full LLVM.

| Area | Ordinary UWVM2 | UWVM2-ROS | Checked behavior |
| --- | --- | --- | --- |
| Parser | wasm1/wasm1p1 plus wasm2 policy | same binary grammar, layered policy | types, sections, const expressions, feature gates, malformed/invalid binaries |
| Validation | separate legacy and wasm2 implementations | wasm2 adapter over eager wasm1p1 validator | bottom-stack typing, br_table labels, reference declarations, split table policy |
| Initialization | runtime initializer | corresponding full-only initializer | globals, imports, DataCount, active/passive/declarative segments, implicit drops |
| Globals | object/global and raw ABI storage | same object/global representation | numeric/v128/reference globals; imported storage and byte bridges |
| Interpreter | common eager translator plus other execution modes | eager translator only | control reachability, stack ownership, SIMD semantics, memory bounds |
| LLVM | full module generation plus other execution modes | full module generation only | direct typed calls, multi-value, vector ABI, native SIMD lowering, guarded/checked memory |
| Entry / imports | full module graph | full module graph | preloaded start sections, start order, reference identity, table mutations |

The object/global trees were byte-identical at inspection. Parser differences are largely policy/tag naming and comments, not missing binary-section parsers. Full LLVM opcode emitters, SIMD lowering and memory lowering correspond; remaining generator differences primarily remove lazy/OSR state. File-name equality is not used as a conformance test.

## Confirmed defects and repairs

- Const expressions incorrectly accepted local globals in data/element initializers; only permitted immutable imported globals are now accepted.
- br_table validation/code generation did not correctly account for bottom/unknown stack arguments.
- Interpreter operand pops could consume values belonging to an outer validation frame.
- Interpreter code-generation reachability was conflated with validation polymorphism, losing nested branch-table state.
- ROS interpreter validation omitted function-export declarations needed by ref.func.
- Full-mode entry dispatch omitted preloaded module start sections; both products now run actual starts in preload order. Exported names alone do not make a preloaded function a start section.
- ROS canonical MVP/wasm1p1 CLI spellings differed; ordinary spellings are accepted while ROS aliases remain compatible.
- LLVM identity optimizations could forward signaling NaNs through scalar/SIMD arithmetic. Native constrained arithmetic preserves quieting; cache environment fingerprints invalidate old arithmetic code.
- PPC SIMD promotion preserved signaling NaNs; the VSX path uses native vector conversion. Non-VSX and scalar promotion also quiet NaNs.
- MIPS big-endian MSA constrained arithmetic caused LLVM instruction selection to loop. Native MSA arithmetic intrinsics avoid this.
- LoongArch generic constrained arithmetic emitted soft-float calls despite native FP hardware. LSX intrinsics and scalar native instruction lowering remove those calls.
- ARM32 interpreter scalar helpers could be re-vectorized into NEON arithmetic that flushes subnormals. Strict exceptions are now specified inside the helper itself.
- RISC-V ceil/floor/trunc could preserve signaling NaNs. Integer classification quiets NaN inputs without floating-point comparisons.
- Interpreter frame sizing checked max - (padding + stack_bytes), but the parenthesized sum could already wrap. A tested checked-add helper now validates each addition before allocation, for both 32-bit and 64-bit sizes.
- Reverse comparison found ordinary UWVM2's legacy validator still coupled table instructions to reference-types and omitted split-policy checks. The legacy API regression failed before the fix, while ROS passed; both now pass.
- A concurrently edited ordinary interpreter f32x4.splat used nonexistent enum member names; the names were corrected without reverting its bit-preserving implementation.
- A final i686/x87 opfunc test reproduced 24 ROS failures in scalar pseudo-min/max selection, with the first mismatch at pmin for each affected input. Both products now pass with tail and by-reference dispatch after synchronizing integer-bit selection. Floating-lane wrappers were also aligned with ordinary UWVM2's integer carriers, but the isolated old ROS legacy-lane test already passed on this compiler; that synchronization is not presented as a separately reproduced failure.
- Module translation units received the checked-size/strict-FP dependencies corresponding to the header implementation; ROS shared SIMD's module entry now includes strict_float.h too. The closure continuation below now also builds complete named-module products.

Regression sources include check_wasm2_regressions.py, check_wasm2_starts.py, wasm1p1_table_feature_split.cc, llvm_jit_nan_arithmetic.cc, uwvm_int_rounding_nan.cc and checked_frame_size.cc.

## Evidence currently completed

Official corpus: WebAssembly/spec tag wg-2.0, commit fffc6e12fa454e475455a7b58d3b5dc343980c10.
Source: [WebAssembly specification tests, wg-2.0](https://github.com/WebAssembly/spec/tree/wg-2.0).

| Test | Result |
| --- | --- |
| Actual parser/validator binary assertions | 4,581 per product, zero failures |
| Parser concept/binfmt unit union | 64 product/test runs passed |
| Diagnostic formatters | 30 direct-sink runs passed: 123 parser, 28 name and 52 validation codes, all five character types in both products; decoded outputs agree after validated pointer-address normalization |
| Initializer/validator unit union | 34 product/test runs passed on the final revision8 matrix, including both legacy-policy regressions |
| Backend-specific native units | Closure revision4: 62 product/test runs passed (32 ordinary, 30 ROS), zero failures and zero skips after recorded repairs/retries |
| Official instantiation, three feature modes | Closure revision5: 17,756 runs per mode; 53,268 total, zero failures |
| Official execution, three feature modes | Closure revision5: 4,592 runs / 166,304 scheduled assertions per mode, zero failures; earlier exhaustion failures repaired |
| Directed defect regressions | 108 runs passed on closure revision5 |
| Start-section integration | 98 runs passed on closure revision5; ordinary extra modes included, ROS full-only |
| Full-feature/import-alias/DataCount integration | Closure revision5: 65 checks per product passed, across five backend/optimization policies |
| SIMD memory bounds / memory.grow | 652 checks per product passed on closure revision5 |
| Cross-module funcref and non-scalar globals | Closure revision5: six scenarios in each of four common backend paths passed |
| Interpreter SIMD differential | 236 opcodes, 385 lane variants, 98,560 cases per profile; 13 profiles per product passed |
| LLVM SIMD differential | All 236 opcodes generated for 30 profiles per product; 23 profiles per product executed successfully |
| LLVM scalar arithmetic | 44 optimized native checks per product; eight cross profiles per product plus LoongArch scalar passed |
| Interpreter scalar rounding | 72 checks per product/profile; native x86_64, i686, ARM32, PPC64 BE and RISC-V64 passed |
| Checked frame extents | Native UBSan and i686 QEMU passed for both products; uint32_t/uint64_t boundary cases |
| Actual SIMD opfunc bit transport on i686/x87 | Ordinary revision7 and fixed ROS revision8: zero failures; pre-fix ROS: 24 failures |
| Isolated legacy floating-lane moves | Old ROS i686/x87 and repaired ROS native tests passed; no independent pre-fix failure reproduced |

Three feature modes mean --wasm-feature-wasm2, --wasm-feature-wasm1p1 and the default policy. They are not three independent specifications.

The historical revision8 matrix used the ordinary revision7 executable and the repaired ROS revision8 executable; it is superseded by closure revision5 for the whole-product rows above. The harness's assertion_executions summary field sums scheduled assertions; it is not an instrumented count of assertions actually reached in failed processes.

Cross-generated LLVM profiles: x86_64, i686, aarch64, aarch64_be, arm, thumb, armeb, arm_scalar, ppc64le, ppc64, ppc64_scalar, ppc32, s390x, s390x_scalar, riscv64, riscv64_scalar, riscv32, loongarch64 with/without LSX, MIPS64 r5/r6 both endians, MIPS32 MSA legacy/NaN2008 profiles, MIPS32 scalar profiles, sparcv9 and sparc32.

Executed LLVM profiles: x86_64, i686, aarch64, aarch64_be, arm, thumb, armeb, arm_scalar, ppc64le, ppc64, ppc64_scalar, s390x, s390x_scalar, riscv64, riscv64_scalar, riscv32, loongarch64 with/without LSX, MIPS64 r6 both endians, MIPS32 NaN2008 both endians and MIPS32 NaN2008 scalar.

Cross tests compile the production emitters/evaluators, then execute generated objects under QEMU. This is not the same as bootstrapping the entire UWVM CLI and LLVM MCJIT under every guest OS. Host-generated reference vectors are also not independent formal verification; native official-spec tests complement them.

## Performance sample (x86_64)

One CPU (31), five measured samples after one warm-up; 300 million loop iterations, each updating four f32x4 accumulators with separate multiply/add instructions. The final vector is stored to exported memory. UWVM uses full LLVM pb-o3, zero background compilation workers (-Rct 0) and persistent object cache disabled. The benchmark did not explicitly override the call-stack tracking policy. Timings subtract the median of a 1,000-iteration process to estimate loop cost; they are not a general benchmark suite.

| Existing executable | Long-process median | Estimated loop time |
| --- | --- | --- |
| UWVM2 revision7 | 0.4879 s | 0.4683 s |
| UWVM2-ROS revision6 | 0.4879 s | 0.4684 s |
| Existing Linux WAVM build | 1.1447 s | 1.1298 s |
| Wasmtime 46 | 0.5203 s | 0.5157 s |

The UWVM object extracted through its production cache decoder has four register-resident accumulator chains, vmulps/vaddps, and no call or memory spill in the loop. WAVM's assembly uses the same native vector arithmetic but reloads/stores four vector locals on the native stack each iteration. This explains the observed difference in this sample; it does not establish a universal speed ranking, zero overhead, or equivalent performance on QEMU targets. Raw entry wrappers outside the loop remain normal ABI calls.

Evidence: simd-fp-bench/{long.wat,results.json,uwvm2.s,wavm.s} and simd-fp-bench-memory.log. Two earlier WAVM runs failed due to the environment's libc++ lookup and an unsupported v128 global initializer; those timings are excluded. The successful comparison uses the matching LLVM-toolchain runtime libraries and exported memory instead.

## Closure continuation (in progress)

The continuation uses /tmp/uwvm-paired-closure.W9Xx4o. Its revision4 native binaries passed all six whole-product matrices: 53,268 instantiation runs and 13,776 execution runs, with zero failures. This is a fixed snapshot, not a blanket claim about later edits.

Revision4 source-manifest SHA-256: ordinary 8619581c822a404e8b5e9defce6c384f42cfe2420269788ef89773df12a04bf9; ROS 457491fbe7f4078e9a594f0950d468d6802e582cf364d13f17bd8073fc41349e. Both native builds completed. Revision4 also passed 108 directed regressions, 65 feature integration checks per product, 652 SIMD memory checks per product, and six cross-module scenarios across the four common paths.

Revision5 incorporates the later native-NaN/rounding changes and wide diagnostic repair. Both native products built successfully (ordinary 330.863 s, ROS 319.832 s). Source-manifest SHA-256: ordinary 42e6dfd56466d02e7e7ff67f2154fb84c224d51ae1523044c83e6f5452f30a25; ROS 0d8e060201fbcc87dadfdeb4f1434fa5e9ea70dd86a4a2e2a04a31e4fa4b6bb0. All six full corpus matrices passed: 53,268 instantiation runs and 13,776 execution runs, zero failures. Its directed suite, 98 startup tests and 285 exhaustion tests also passed. Its production-cache SIMD loops again have identical four-multiply/four-add disassembly, without calls or stack accesses. Later documentation-only FP commits and test-only pointer/feature-gate corrections are outside those build manifests.

The revision4 native unit inventory includes every top-level LLVM unit plus checked_frame_size and native_stack_guard. All 62 now compile and run without skips. First failures and skips remain in native-unit-matrix/results.json; retry-results.json and final-results.json record closure. A private extracted WASI sysroot/compiler runtime enabled actual C++→Wasm integration. ROS's stale expectation that the default feature policy must reject Wasm 1.1 was replaced by explicit sign-extension disablement, and its explicit feature subset and supported unwind-uncheck policy now match ordinary full-mode coverage. ROS's removed lazy/tiered modes remain removed. An unused compile-time-only FP fixture member was marked maybe_unused, and out-of-source trap tests received source-discovery links in the scratch build layout.

- Both products now establish a thread-local, lock-free native stack fault context and an alternate signal stack on Linux/Darwin. Stack overflow emits a bounded diagnostic and exits 127 without allocating or unwinding the exhausted stack. Existing signal stacks are restored. Other platform stubs do not claim equivalent handling.
- Native C++ and supported LLVM targets use stack probes. AArch64 GCC's default 64 KiB interval was insufficient for 4 KiB guards; the build explicitly requests smaller probes.
- A new adversarial RISC-V test reproduced a guard skip with LLVM's 4096-byte probes: the separate CSR-saving adjustment can leave an extra unprobed gap. RISC-V now requests 2048-byte probes. All 256 16-byte-aligned entry offsets passed after this correction. GCC's existing 4 KiB implementation passed the same boundary test without that change.
- All 13 official exhaustion actions plus two no-memory modules passed again on revision4 in 19 execution profiles (285 runs). This includes ordinary lazy/tiered modes and only full execution in ROS.
- Atomic native-stack tests passed under QEMU for i686, AArch64, ARM, PPC64 BE/LE, RISC-V64 and s390x. The QEMU pthread cases use explicitly mapped guard pages: default guest pthread guards failed in this environment and are not counted as passes. Native i686 and macOS pthread tests passed.
- LLVM large-frame objects executed successfully on x86_64, i686, AArch64, RISC-V64 and s390x Linux; an arm64 Mach-O object also executed successfully on macOS. AArch64 BE, RISC-V32 and x86_64 Darwin additionally have code-generation evidence, not equivalent native execution coverage. Small-frame disassembly has no added probe call.
- New module-order regressions found both products applied later active data/element segments before earlier start sections. Six scenarios across the four common paths failed (24 failures), including growth in an earlier start making a later data segment valid. The CLI now applies one module's segments and invokes its start before moving to the next. Ordinary lazy/tiered dispatch shares this sequence; ROS remains full-only. Revision4 passed all 98 start tests (70 ordinary, 28 ROS).
- Complete named-module builds exposed missing direct dependencies: the WASI trace enum, runtime-mode defaults used by the version callback, and strict_float helpers used by the interpreter's heavy operator partitions. Corresponding module dependencies were added to both products without restoring ROS lazy/tiered execution. Both complete builds remain in progress; successful individual repaired partitions are not a full build pass. Module outputs were byte-verified and relocated from RAM-backed /tmp to /var/tmp/uwvm-paired-modules.1GKfGf, retaining the original paths as symlinks.
- The validation diagnostic fixture previously stopped at 46 codes and omitted six Wasm 1.1/2.0 diagnostics; it now supplies valid payloads for all 52. Isolating just br_table_target_type_mismatch reproduced a wide-character compilation OOM at an 8 GiB job ceiling. Splitting optional fields into bounded wide-character print records repairs the template expansion; char/UTF-8 batching and execution hot paths are unchanged. All ten complete, unsplit validation formatter runs compile at O2 and produce 52 lines, using about 0.8 GiB for narrow types and 1.8 GiB for wide types. The complete 30-run direct-sink matrix passed after correcting parser test pointer payloads; only the intentional absolute-address diagnostic is ASLR-normalized after validating its one-byte pointer difference. Earlier OOM attempts, including buffered configurations, remain failed attempts, not passes.
- Revision4 production-cache objects for the short SIMD arithmetic sample have identical function disassembly in both products: four vmulps and four vaddps instructions, no calls or stack loads/stores in the loop. Cache-decoded objects and disassembly are revision4/simd-{full,ros}.{o,s}. This refreshes machine-code evidence, not the earlier uncontended timing measurements.

Key continuation logs: final-matrix-results.json, exhaustion-final3.log, directed-final3.log, native-stack-cross-atomic.log, llvm-stack-riscv-boundary-before.log, llvm-stack-riscv-boundary-v2.log, native-riscv-boundary.log, llvm-stack-darwin.log, starts-order-before.log, revision4/starts-full.log, ros-module-build-scanner.log, ros-module-build-v2.log, full-module-build.log and print-matrix.log.

ROS's third module attempt then exposed one additional missing dependency: initializer/init.cppm lacked cstring for the bit-preserving std::memcpy global initialization path. The ordinary module already included it. ROS now includes it too; the memcpy implementation is unchanged. A paired scan found no other direct memcpy-family or bit_cast users with a corresponding module entry missing cstring/bit. The fourth ROS attempt is running. xmake's two-phase module builder requires both BMI and object outputs when deciding reuse, so a failure before object generation causes broad BMI rebuilding on retry.

Current module source-manifest hashes: ordinary 897315f134025c3c178335dbf30390ddbaeae57fa6eb427437042b54c2456c1a; ROS 4c7de6b128d10b515b47e614db748ce9e5243f86e84df96197dd4baf3dd7aa4b. The preceding ROS attempt used c8ad939389536e81429a512c980e7e3e56645dedbed8ab93d54d973b363468e6. These module snapshots differ from the revision5 native snapshots; neither unfinished module build is counted as a pass.

## Limitations and unfinished checks

1. The original x86_64 exhaustion failures are repaired and passed the closure revision5 matrix. This does not establish universal native-stack safety: LLVM probing is not implemented here for every supported JIT backend, custom guardless/fiber stacks are not covered, and non-Linux/Darwin native signal handling remains platform-specific.
2. Registered multi-module WAST scripts, foreign host references/imports and stateful actions after a first trap are not fully replayed by the CLI harness. Execution records contain 4,673 exclusion records per feature configuration; instantiation excludes 142 foreign-module-graph records. The binary parser corpus also excludes 1,091 text-only assertions. Directed integration covers only a subset of the graph exclusions.
3. The complete 30-case direct-sink diagnostic matrix passed. Earlier buffered compilation attempts exhausted their resource limits and are not counted as passes; this does not claim every buffered stream instantiation fits the same compiler-memory budget. Legacy Windows console text-attribute paths were not executed on Linux.
4. SPARC still has compiler-generated conversion libcalls (including __truncdfsf2); no all-platform zero-helper performance claim is made.
5. wasm64 is future work, not implemented Wasm 2.0 conformance. Cross-host-width arithmetic tests do not establish a complete wasm64 execution implementation.
6. Page-protected accesses depend on the actual reservation/guard model and stable memory provider. Explicit checks/bridges remain appropriate for providers that cannot supply those guarantees. Tests do not justify removing all such fallbacks.
7. The SIMD timing sample used ordinary revision7 and ROS revision6; timings were not rerun during the contended closure builds. Closure revision5 refreshes actual native loop disassembly, not the historical timing comparison.
8. Another task has been editing FP-related files concurrently. Fixed remote snapshots, rather than the moving local working tree, define each test's provenance.

## Test environment and artifacts

Remote ssh linux, scratch root /tmp/uwvm-paired-audit.WusY2i.
Clang/LLVM 22.1.8; GCC cross compilers 15; WABT; user-mode QEMU.
Own tests use CPUs 16-31 and a shared systemd slice with MemoryMax=64 GiB and MemorySwapMax=0. The memory ceiling was not raised after OOMs. No other task's processes were stopped.

Build source manifests are revisionN/full.manifest and revisionN/ros.manifest.
Revision7 ordinary source equals revision6 plus the legacy validator fix/test; ROS revision7 source differs only in tests and reuses the revision6 ROS binary.

Final whole-product build provenance:

| Product | Tested source / executable | SHA-256 of source manifest |
| --- | --- | --- |
| Ordinary UWVM2 | revision7/full, reused by revision8/full | ce96c778498a3b0127ce95da78cd5dbf21c399f550f21b0653e1e8f26851f564 |
| UWVM2-ROS | revision8/ros | b2ddb93459fbc7bba5bf095ed4c5acf265c20315f213b94999c2fe59df2fd813 |

These identify source manifests, not executable hashes. Later module-only dependency additions were not covered by the non-module executable builds. Concurrent changes after the fixed snapshots are not implicitly validated by these results.

Key evidence:

- revision8/directed.log and its directed/integration/boundaries subdirectories
- revision8/header-semantic/results.json; revision4/header-tests.log
- revision8/final-execution-{wasm2,wasm1p1,default}/results.json and excluded.json
- revision8/final-instantiation-{wasm2,wasm1p1,default}/results.json
- int-cross-final.log; int-cross/*.{log,s}
- nan-cross-final3.log (initial no-LSX test-harness rejection retained), nan-final-bare.log (corrected native-instruction whitelist and successful no-LSX execution)
- nan-probe/{full,ros}/cross/*.{ll,s,undefined} and scalar-loongarch64.*
- int-rounding-cross.log; revision6/checked-frame-size.log
- legacy-policy-before.log and legacy-policy-final.log
- simd-fp-bits-before.log and simd-fp-bits-final.log

Selected logs, final manifests and the benchmark's actual object/assembly are also retained locally under /tmp/uwvm-paired-audit.cm4gqc/evidence/{final,simd-fp-bench}.

The no-helper test permits only LLVM intrinsics and an exact whitelist of single LoongArch FP instructions in inline assembly. An LLVM CallInst containing such an instruction is not a host bridge.

No commits were created by this audit. Concurrent task changes were preserved. A final whole-worktree diff check reported trailing whitespace in concurrently edited conbine_heavy.h and delay_local.h; those unrelated edits were not rewritten by this audit.
