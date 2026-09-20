# Direct LLVM SIMD and memory regression tests

SIMD numeric operations use LLVM SSA vectors, including locals, block/loop PHIs, globals and Wasm-to-Wasm calls. The private v128 type is `<16 x i8>` in Wasm byte order, **not i128**. Host/provider boundaries retain the existing byte-buffer ABI. Native mmap accesses and single-thread allocator accesses do not use SIMD memory bridges. Provider callbacks and moving shared allocations retain their access-lifetime/pinning bridges.

Runtime cache ABI schema v11 fingerprints both the new vector ABI and guarded-store preflight. Objects using the old integer-pair ABI or unprotected crossing stores must not be reused.

## Memory contract

| Configuration | Address proof |
| --- | --- |
| memory32 / ISA64, full mmap guard | Zero-extend the unsigned address before adding the unsigned offset. The reservation must contain the entire 33-bit effective-address domain and access width. No dynamic length load or software bounds branch. |
| memory32 / ISA32, partial guard | Check carry and escape from the reserved prefix before pointer truncation. Outside the prefix, check the entire access against the length. |
| Future memory64 / ISA64 | Check the 65th-bit carry and the entire range outside the protected prefix. This is tested at the address-helper level; it does **not** enable Wasm 3.0 or memory64 parsing/execution. |
| Small custom pages / allocator | Check `length < width || offset > length - width`, plus address overflow. Single-thread growth reloads the mutable base slot. Shared moving storage stays pinned through the actual access. |

Wasm `memarg.align` is only a hint. Loads/stores are byte-aligned in IR, use the exact opcode width, and remain volatile so that dead-code elimination cannot erase a required trap. Guard-addressable GEPs are not `inbounds`.

A protection fault does not universally make a store transactional. ARM/AArch64/PPC can write the in-bounds prefix of a crossing store before faulting; vector legalization can also split stores. The guarded store path probes the final byte **only when crossing a linear-memory page boundary**. It never reads the current length. Proven-aligned accesses eliminate this whole check after optimization. Other page-local stores still perform one memory access, but retain a small boundary predicate if alignment cannot be proved. The interpreter's scalar, SIMD and fused stores use the same rule. Enlarging mmap cannot fix partial-store semantics.

## Tests

- `simd_direct_lowering.cc`: native MCJIT differential test of all 236 supported opcodes, 385 lane variants and 256 inputs per variant. Covers every swizzle byte index, shifts, saturation, signed zero, subnormals, infinities and signaling/quiet NaNs. Arithmetic quiet-NaN payloads may differ where permitted; canonical-only input probes require canonical NaN outputs, and signaling NaNs are not silently accepted as arithmetic results. Non-arithmetic operations compare every bit. Also directly exercises the production SPARC final-legalization helper in a fresh embedded LLVM process.
- `memory_direct_lowering.cc`: 210 address/protection configurations and 454,650 boundary/random decisions per run, using independent two-limb arithmetic and an explicit trap flag. Run both normally and with `--optimize-ir`; the latter applies LLVM O3 before MCJIT. Both also assert that proven-aligned stores lose their preflight load, helper and comparison after optimization.
- `simd_guarded_store_fault.c`: link against a cross-target object emitted by the SIMD test; checks 134 real protection-fault cases and verifies that trapped stores did not modify any in-bounds byte. Includes 1/2/4/8/16/32/64-byte stores.
- `../0013.uwvm_int/uwvm_int_memory_guarded_store.cc`: the same fault regression through the production interpreter store helper; also pins unsigned memory64 carry semantics.
- `check_simd_memory_boundaries.py`: 652 process-level runs across full-O3, full-debug, lazy-balanced and lazy-debug. Run separately for mmap, single-thread-alloc and multi-thread-alloc builds. Includes exact-end loads/stores, narrow accesses, maximum offsets, 33-bit sums, growth and the vector call/global/PHI ABI fixture.

```
python3 test/0014.llvm_jit/check_simd_memory_boundaries.py /absolute/path/to/uwvm \
  --out /tmp/uwvm-simd-boundaries --jobs 4
```

The SIMD test optionally accepts `TRIPLE CPU FEATURES OUTPUT_PREFIX` and emits LLVM IR plus a portable C differential harness. Build it with `UWVM2TEST_SIMD_CROSS_TARGETS` and an LLVM library containing all desired targets to enable this mode. The ordinary test build needs only the native LLVM target.

```
simd_direct_lowering aarch64-linux-gnu generic +neon /tmp/aarch64-simd
opt -passes='default<O3>' /tmp/aarch64-simd.ll -o /tmp/aarch64-simd.bc
llc -O3 -filetype=obj -mtriple=aarch64-linux-gnu -mcpu=generic -mattr=+neon \
  /tmp/aarch64-simd.bc -o /tmp/aarch64-simd.o
aarch64-linux-gnu-gcc -O2 -static /tmp/aarch64-simd.c /tmp/aarch64-simd.o -lm -o /tmp/aarch64-simd.test
qemu-aarch64 /tmp/aarch64-simd.test
aarch64-linux-gnu-gcc -O2 -static test/0014.llvm_jit/simd_guarded_store_fault.c \
  /tmp/aarch64-simd.o -lm -o /tmp/aarch64-simd-guard.test
qemu-aarch64 /tmp/aarch64-simd-guard.test
```

Match the target CPU/features and ABI in all steps. MIPS P5600 execution needs the NaN2008 ABI. SPARC V8 additionally runs `function(scalarizer<load-store>)` **after** optimization, matching the production final legalization step.

## Fresh interpreter O3 store checks (2026-09-15)

Both fixed module-g13 source trees also passed the real guard-page store test
through the production interpreter memory.h at O3: native Linux x86_64,
QEMU i686/SSE2, and QEMU AArch64/NEON, 134 cases each, 804 total. Widths are
1/2/4/8/16/32/64 bytes; trapping writes leave every in-bounds prefix byte
unchanged. The aligned 16-byte wrapper has no preflight branch, helper call,
byte probe or hardware fence. Its instruction/operand sequence is identical
between products on each of these three target profiles.

The x86_64 wrapper is an address mask plus movups load/store and return;
AArch64 is a mask plus ldr/str q0 and return. The Pentium4-tuned i686 result
uses two 8-byte movsd loads/stores and ordinary ABI argument-stack reads.
This is target-specific code generation, not a measured throughput claim.

For this focused run, a temporary copy of the existing fault fixture includes
memory.h directly instead of the complete optable aggregate. It still calls
the actual prepare_memory_store_pointer and uses the original fault oracle.
Cross builds disable only fast_io floating-point formatting tables, which
this byte-store test does not use. These results do not certify compilation
of all fused operators or whole-engine execution. The full-aggregate x86_64
attempt passed; i686 aggregate attempts timed out under bounded resources
and remain failed attempts. No production implementation was changed.

Evidence: /var/tmp/uwvm-stack16.PsvyAm/interpreter-store-o3-v4 and the matching
directory under /tmp/uwvm-paired-audit.cm4gqc/evidence/memory-decision32.
The latter retains the exact check-interpreter-store-o3.py driver, generated
fixtures, objects, commands, results and aligned-wrapper disassembly. The
successful auxiliary job used a 4-GiB child ceiling inside the unchanged
32-GiB aggregate slice, swap disabled, CPU 31.

## Address-oracle follow-up (2026-09-15)

The old return-value-only observation used UINT64_MAX as a trap sentinel. That
value can also be the computed address: a test could miss an omitted trap even
when its numerical return matched. Both fixtures now compare an independent
trap byte as well. The reference computes the sum with two 32-bit limbs, so it
does not duplicate the production emitter's wrapped 64-bit add/carry test.
The original fixed boundary grid is retained and supplemented by 2,048
deterministic random/near-boundary inputs per configuration.

check_memory_address_decisions.py builds against the selected product source,
runs baseline and O3 IR, and optionally proves the observation's sensitivity.
Its temporary negative control suppresses the observed trap only for
memory64/software, width 1, zero static offset, and address=length=UINT64_MAX.
The independent check must fail with the expected diagnostic; removing just
the trap-byte comparison must miss the same injected fault. These mutants
change test-owned observation code, never the production emitter.

Both module-g13 source snapshots passed baseline and O3: four original runs,
1,818,600 decisions total. Both products also passed the expected negative
controls in both IR modes. Do not count the deliberately faulty sentinel-only
runs as product correctness coverage. LLVM is 22.1.8; compilation selects GCC
13 headers, while linking uses the current system libstdc++ required by LLVM's
dependencies. Both resulting original executables have SHA-256
4a78335b42e001d8ccbb3616b48dea978ace7a8c3d278278f93b9516a571f860.
At that checkpoint the fixture hash was
4fe7401c6154ac402190b41cd68917acb950ca2f626df508b1025fc18a8f835e;
the unchanged memory_emit.h hash is
6aabdb6b4a24b04b7c0d5b7cae455e56236c2d3efc6feea85809f02042865140.

These generated functions execute integer address decisions on the native
host and write only a test-owned trap byte. They do not dereference linear
memory, execute a real ISA32 process, or test the fatal runtime reporter.
The abort-only fixtures/memory_address_trap_stub.cpp is exclusively for this
standalone test and must never be linked with uwvm_runtime. Future memory64
address-helper coverage still does not enable Wasm 3.0.

Example (inside externally imposed CPU/memory limits, using a new directory):

```sh
python3 test/0014.llvm_jit/check_memory_address_decisions.py \
  --cxx clang++-22 --llvm-config llvm-config-22 \
  --compile-flag=--gcc-install-dir=/usr/lib/gcc/x86_64-linux-gnu/13 \
  --negative-sentinel-control --out /tmp/uwvm-memory-decisions-new
```

Remote commands, hashes and logs:
/var/tmp/uwvm-stack16.PsvyAm/memory-decisions-final-{full,ros}.
Selected local evidence:
/tmp/uwvm-paired-audit.cm4gqc/evidence/memory-decision32.
Earlier harness attempts missing framework macros or linking an older
libstdc++ against newer LLVM dependencies are retained as failed setup attempts.
No production protection or hot-path instruction changed in this follow-up.

### No-mmap test expectation correction and three-configuration replay

Compiling that test with UWVM_FORCE_DISABLE_MMAP reproduced a test failure
(exit 2): the fixture still assumed a 64-byte mmap guard existed and therefore
incorrectly rejected the production helper's software fallback. Both fixtures
now use the configured mmap guard width, or zero when mmap is unavailable,
and report the number of actually tested full-guard configurations. This is
a correction to the test oracle, not a production memory-access defect.

The updated fixture SHA-256 is
897ba9a0bbf6f088261b5a927cf79c8577dbd81e27daa3217dba6c88c7c723a8.
Both source trees were then compiled independently with mmap, single-thread
allocator and multi-thread allocator selections. All six builds and twelve
baseline/O3 runs passed: 5,455,800 original address decisions. Each mmap run
checks 18 full-guard configurations with no software checks; allocator runs
correctly report zero and exercise software fallback. For each selection,
the ordinary and ROS test executable hashes also match.

The two allocator selections add --compile-flag=-DUWVM_FORCE_DISABLE_MMAP;
the multi-thread selection additionally adds
--compile-flag=-DUWVM_USE_MULTITHREAD_ALLOCATOR to the example above.
As with the original oracle, this is emitted address-decision execution under
those compile-time selections. It does not allocate/grow real linear memory,
exercise a moving provider's pinning callback, or validate actual ISA32 JIT
execution. The earlier sentinel negative controls belong to the explicitly
recorded checkpoint above; the production emitter remains unchanged.

Evidence: /var/tmp/uwvm-stack16.PsvyAm/memory-decisions-backends, copied locally
under evidence/memory-decision32. The no-mmap-before failure, its original
fixture, and the exact six-build driver are retained alongside the final
commands, binary/input hashes, results and completed.json.

## Target-specific requirements

Native swizzle lowering covers SSSE3, NEON/Thumb, AltiVec, LSX, MSA, RVV and s390 vector instructions. Other configurations use portable bounds-safe vector IR. Explicit target attributes are authoritative. Initial native emission additionally mirrors full/lazy materialization's FP/ABI feature exclusions: a feature physically present on the CPU is insufficient when the runtime cannot safely enable it.

ARM32/Thumb NEON FP32 flushes subnormals; strict scalar VFP operations preserve Wasm IEEE semantics while integer SIMD and vector memory stay native. LLVM 22 workarounds are restricted to affected targets: NaN saturation on PPC/LoongArch/MIPS; scalar PPC NaN promotion; MSA conversion/sign/narrow-carrier lowering; O32 BE lane shuffles and rounding; BE MSA rounding; SPARC V8 vector legalization. These paths do not call the removed uwvm numeric bridges. A target without an applicable machine instruction may still need scalar instructions or standard math-library lowering from LLVM.

The SPARC helper explicitly initializes both Scalarizer and TargetTransformInfo in embedded LLVM: LLVM 22's legacy Scalarizer requests TTI without declaring its initialization dependency. An `opt` subprocess alone would hide this integration issue.

The unused `test6_sin_table_fill_loop_run` handler and its getters were removed after confirming that no translator references them. Its whole-loop `size_t` length multiplication could wrap on ISA32 and its monotonically advanced host pointer did not model Wasm32 pointer wrapping. This removes dead code without changing an emitted execution path.

## Validation coverage

The Linux campaign generated all 236 opcodes for 29 target/feature profiles. Twenty-two profiles also passed the complete differential execution under native Linux or QEMU: x86-64, i686, AArch64 LE/BE, ARM LE/BE, Thumb, PPC64 LE/BE, s390x, RV64, RV32, LoongArch64, MIPS64r6 LE/BE, MIPS32r5 NaN2008 LE/BE, and scalar-feature ARM/PPC64/RV64/s390x/MIPS32 variants. PPC32, SPARC32/64 and legacy MIPS ABI profiles have code-generation coverage only, not execution coverage.

The three memory-backend builds passed 652 boundary/growth/vector-ABI cases each. Eight architecture profiles passed 134 real guarded-store fault tests each. The interpreter helper also passed with extra-heavy fusion headers enabled. Full-O3, full-debug, lazy-balanced and lazy-debug each successfully stored and reused the vector-ABI fixture's native cache objects with the v11 fingerprints. The broader cache integration executable could not finish on the JIT-only test build because that build does not expose its requested tiered/interpreter CLI options; the targeted four-mode cache regression passed separately.

## Measured SIMD hot loops (2026-09-14)

Linux i9-14900HX E-core 16, LLVM 22.1.8, 100 million iterations per kernel, 1 million warmup iterations, median of seven measured rounds. Timers run inside Wasm and exclude JIT compilation/startup. Checksums match across all engines. All test work is confined to CPUs 16–31 under a shared 64-GiB memory cgroup; the benchmark itself uses CPU 16.

| Kernel | Direct SIMD | Old bridge build (`e9c4861b6`) | WAVM | Old/direct |
| --- | ---: | ---: | ---: | ---: |
| 16-byte copy | 36.743 ms | 467.056 ms | 55.900 ms | 12.71× |
| SIMD memory add | 47.236 ms | 910.219 ms | 59.484 ms | 19.27× |
| Register add/xor | 97.976 ms | 2386.952 ms | 97.957 ms | 24.36× |

These are cache-hot microbenchmarks, not a claim about every workload or architecture. Disassembly of the aligned copy loop contains direct `vmovups` loads/stores without a memory-length load, bridge call or store probe; the register loop keeps its loop-carried values in SIMD registers. The pure compute difference versus WAVM is approximately 0.02%, within measurement noise.

Passing a cross-emitted object under QEMU establishes much more than IR verification, but is not a performance measurement or a substitute for testing every physical CPU, OS ABI, LLVM version and feature combination. Full-engine ISA32 execution and future memory64 execution must not be inferred from helper/code-generation tests.

## RISC-V64 host-address follow-up (2026-09-15)

Both products now use the same register-only emitter in
`translate/host_address_emit.h`. The former volatile stack-zero/byte-shift
workaround incurred a stack frame, store and load. An O3 reproduction also
showed that its constant part could still enter a literal pool with
R_RISCV_HI20/LO12_I relocations, defeating the intended avoidance of far-address
fixups. This is code-generation evidence, not a claim that every prior input
failed at runtime.

The replacement uses an LLVM inline-assembly `li` pseudo-instruction with a
full-width immediate, no side effects and no memory access. This is not a
native bridge call: the assembler expands it into register instructions.
For the sampled address 0x00007ffff0000123, the isolated materializer changes
from 10 instructions plus a 16-byte frame to 4 instructions including return,
with no frame. Arbitrary 64-bit samples require at most 9 instructions including
return. QEMU instruction/functional checks are not physical-CPU timing results.

The checked-in `check_riscv_host_address.py` builds the exact production helper
and checks O0/O3 with static/PIC code generation for RV64G, RV64GC, and RV64GC
with Zba/Zbb/Zbs. The latter matters because the assembler may replace the base
instruction sequence with bit-manipulation instructions such as bseti/slli.uw.
Each profile verifies 270 full-width values, including the sign bit and values
above 4 GiB, plus actual high-address load, store and function-call tests.
Both products passed all 12 profiles: 6,480 value checks and 72 high-address
operations in total. This supersedes the earlier four-profile run. Every pure
address materializer has zero stack accesses, constant-pool loads and native
calls; the generated text has no address relocations. The complete translator
header also compiles with the extracted helper in both products.

The RISC-V64-only JIT ABI fingerprint gains
`llvm-riscv64-host-address=inline-li-no-data-relocation-v2`, independently of an
embedded source revision. The fingerprint test passed with RISC-V64 JIT enabled
and disabled, and confirms that this marker is absent on the non-RISC-V host.
The cross fingerprint test is header-only. Production RISC-V64 full/lazy
materialization still disables cross-process object caching because generated
code embeds process-local addresses; this optimization does not enable unsafe
cache reuse. ROS retains only its full materialization path. Whole-engine
RISC-V64 execution is not implied by these cross-object tests.

Example (run inside the caller's CPU/memory cap, with matching LLVM tools and
a Linux RISC-V64 static-link toolchain):

```
python3 test/0014.llvm_jit/check_riscv_host_address.py \
  --cxx clang++-22 --llvm-config llvm-config-22 \
  --riscv-cc riscv64-linux-gnu-gcc --out /tmp/uwvm-riscv-address-new
```

The output directory must be new. Optional `--riscv-sysroot` and `--qemu`
select private toolchains. Command manifests, IR, objects, disassembly and
runtime logs are retained with the result.

Linux evidence: /var/tmp/uwvm-stack16.PsvyAm/riscv-address/{full,ros}/feature-matrix.
Selected local evidence: /tmp/uwvm-paired-audit.cm4gqc/evidence/riscv-address.
This changes neither the Wasm address-width/guard strategy nor ROS's full-only
execution policy. Whole-product module builds including this follow-up are
tracked separately in the paired audit.
