# SIMD audit checkpoint (2026-09-14)

This is a partial audit, not an all-platform correctness or performance certification.

## 2026-09-15 shared-rounding synchronization follow-up

A fresh source comparison found later ordinary-product SIMD rounding/NaN changes
missing from ROS. GCC 15, O3, x86-64 SSE2 without SSE4.1 reproduced 36 failures
in ROS's 792-check rounding/bit-transport test: ceil/floor/trunc could forward
signaling NaNs unchanged. The ordinary shared implementation passed. ROS now
has the same shared helpers, including target-specific integer rounding and
arithmetic NaN handling. Abs/neg remain bit-preserving; this is not permission
to canonicalize non-arithmetic transport.

Disassembly also exposed scalar lane rounding and stack temporaries in GCC's
SSE4.1 build. GCC does not define the existing general vector gate's
__LITTLE_ENDIAN__ macro. A narrow x86/SSE4.1 path now uses ROUNDPS/ROUNDPD
directly for the eight integral-rounding operations without enabling unrelated
vector paths. The builtins require float/double vector types rather than GCC's
distinct _Float32/_Float64 types; bit_cast transfers the representation without
an arithmetic conversion. Constant evaluation retains the prior fallback.
Clang and other ISAs keep their existing path.

The shared headers tested at this checkpoint have SHA-256
401a0ccb89ff3b2b9d75ebd54992d7fe0f6f404f525119f088222bcb385e1567.
All final tests ran within the aggregate 32-GiB/no-swap slice, on CPU 31:

- Two 14-configuration matrices per product passed: native GCC/Clang SSE2 and
  SSE4.1, native UBSan, and GCC or Clang cross compilation for i686 SSE2/x87,
  AArch64, ARM, PPC64 BE/LE, SystemZ, and RISC-V64 with/without vectors.
- Additional GCC i686 SSE4.1 runs passed on both products. The matrices total
  58 process runs and 45,936 lane checks; repeated native profiles overlap.
- check_simd_rounding_codegen.py passed with GCC 15 and Clang 22 on both
  products: another 6,336 runtime checks. Each SSE4.1 wrapper has one packed
  rounding instruction and no stack access or native helper call. All eight
  SSE2 wrappers are helper-free too. These are shared-evaluator tests, not
  complete interpreter/JIT executions or whole-VM throughput measurements.
- check_wasm2_shared_parity.py passes 29 deliberately shared files and four
  removed-mode path checks. It reproduced the old shared-header/module drift,
  and explicitly permits ROS's omitted tiered reentry output. This targeted
  source guard does not replace semantic tests of different implementations.

GCC cross tests disable only fast_io's unrelated floating-point formatting
tables after a constexpr-table compilation error; SIMD floating arithmetic
remains enabled. Earlier failed/partial attempts are retained, including an
unsupported PPC64 ELFv1 LLD link, a compiler-specific CPU-name mismatch, the
first ineffective vector-gated optimization, and the builtin vector-type error.
They are not counted as final passes.

The unstarted ROS module snapshot received the matching frozen ordinary shared
implementation before its first build manifest. Both module snapshots pass the
29-file parity guard. The active ordinary module snapshot was not modified.
Those Clang snapshots predate this later GCC-only optimization and remain
unfinished products; their successful shared checks are not full-build passes.

Tests: uwvm_int_simd_rounding.cc, check_simd_rounding_codegen.py, and
../0012.validator/check_wasm2_shared_parity.py. Evidence:
/tmp/uwvm-paired-audit.cm4gqc/evidence/stack32/simd-sync-results.json,
evidence/simd-sync-{before,final-v2}, and the corresponding
/var/tmp/uwvm-stack16.PsvyAm directories on Linux.

### RISC-V redundant NaN-check removal

A following disassembly review found that RISC-V ceil/floor/trunc both quieted
input NaNs and classified the result again. The native evaluator's input
quieting already handles canonical/noncanonical NaNs; finite integral rounding
cannot create a NaN. Removing only the second pass preserves that first check
and leaves abs/neg and other ISAs unchanged. Both current shared headers match
the tested trial SHA-256
6c5cb789ea8649de6ed6e918220ebf51247952acc7ed465de7915c28cf00de6a.

An independent Python math/bit oracle supplies 10,000 mixed-lane records per
run, including every NaN payload bit, exponent boundaries, signed zeros,
infinities and random representations. Both before/after versions pass on
both products with GCC 15 and Clang 22 under QEMU RV64GC, RV64GCV and RV64GC+Zfa:
24 process runs, 2,880,000 lane checks, zero failures. The count includes the
before version, not only the optimized version. The existing 792-check fixture
also passed four Clang trial configurations before the larger matrix.

Clang's static f32x4.ceil wrapper shrinks from 115 to 79 instructions on RV64GC,
86 to 46 on RV64GCV, and 75 to 41 with Zfa. The corresponding f64x2.floor
counts are 57 to 40, 45 to 29, and 35 to 21. GCC already removed the redundant
classification in these samples, so its counts are unchanged. All inspected
rounding wrappers remain free of native helper calls. These are code-size
observations, not throughput claims.

Reproduce the independent cases with make_simd_rounding_oracle.py --out CASES.
Compile uwvm_int_simd_rounding.cc with main renamed using
-Dmain=uwvm_simd_fixed_cases_main; link it with fixtures/simd_rounding_oracle.c
compiled as C11, then run the resulting target executable with CASES as its
argument. The ordinary standalone fixture still runs its 792 fixed checks
without the renamed-main define. Evidence: evidence/stack32/riscv-rounding-results.json
and evidence/riscv-rounding-oracle. The binary oracle SHA-256 is
fe940fd42b78fd5e0202cad6dceca01661ee09d4f744a1c640348400a586ad40.

The running x86-64 Clang module snapshots remain fixed; this RISC-V-only
follow-up is validated separately, not silently attributed to those builds.

## ROS full synchronization

The ROS full backend has now been synchronized with the main workspace's SIMD/vector ABI,
scalar/SIMD memory lowering, multi-value/reference/table/bulk-memory support, native target
refresh, and cache v11 ABI. Lazy/tiered/OSR remain removed. The earlier decision to leave its
LLVM backend reduced has been superseded by the user's explicit full-parity requirement.

Compatible interpreter changes and ROS-specific runtime choices are retained. ROS real full
execution, boundary/call/alias/DataCount/cache tests and cross-generated-code results are
recorded in uwvm2-ros/test/0014.llvm_jit/SIMD_DIRECT.md. Those results do not replace the
remaining shared-interpreter audit below.

## New confirmed interpreter defects and fixes

1. ARM32 Clang accepts __has_builtin(__builtin_neon_vqtbl1q_v), but cannot lower that AArch64
   instruction on ARM32. Building the actual shared evaluator reproduced three code-generation
   errors in shuffle/swizzle. Restrict qtbl to AArch64 and use two native ARM32 vtbl.8 operations
   for a 16-byte lookup; shuffle combines the two tables without scalar helper calls.

2. ARM32 FP32 comparisons/arithmetic can flush subnormals even when FPSCR.FZ is clear.
   The independent-host byte oracle found f32x4.eq (opcode 65), sample 73, returning an all-one
   fourth lane instead of zero. Disassembly contained vceq.f32 q8,q8,q9.
   Disabling explicit vector expressions alone did NOT fix it: Clang re-vectorized the scalar
   loops into the same instruction. Block-scoped clang fp exceptions(strict) preserves the
   affected floating-point expressions through inlining and optimization. Integer-only SIMD
   remains unconstrained. No per-op FP environment save/restore or bridge is added.
   Scalar fallback is also selected for the affected ARM32 arithmetic/comparison blocks.

## Tests performed

All Linux compilation/execution runs use the existing aggregate 64-GiB/no-swap cgroup and
CPU affinity 16-31 (the host's 16 E-cores). Scratch root: /tmp/uwvm-simd-audit.iYhE5k.

- Actual shared evaluator: all 236 opcodes, 385 opcode/lane variants, 256 inputs each
  (98,560 comparisons per profile).
- Native x86-64 Clang: PASS.
- ARM32 Cortex-A15 Clang 22, QEMU: PASS after the fixes.
- Thumb Cortex-A15 Clang 22, QEMU: PASS after the fixes.
- ARM32 GCC 15, QEMU: PASS using its existing scalar-fallback feature configuration.
  FAST_IO_DISABLE_FLOATING_POINT disables unrelated formatting tables in this test only:
  GCC otherwise rejects a fast_io constexpr table before SIMD compilation. This does not
  disable floating-point SIMD evaluation or its std::cmath dependencies.
- ARM/Thumb final runs include the project's no-math-errno/no-trapping-math/no-rounding-math,
  fp-contract=off release flags. No NEON FP32 arithmetic/comparison remains in their tested
  evaluator objects; native integer SIMD and byte-table instructions remain.
- ROS standalone SIMD regression: PASS.
- ROS actual interpreter memory helper, with all three fusion tiers enabled:
  134 guarded-store boundary/no-partial-write cases PASS.
- Both workspaces: git diff --check PASS.

The target evaluator does not generate its own expected bytes. The C data oracle comes from
the independently tested x86-64 simd_direct_lowering generator in the main workspace.

## Reproduction

uwvm_int_simd_cross.cc is an ordinary standalone regression test. For the full differential
suite, first run the main workspace's simd_direct_lowering with TRIPLE CPU FEATURES PREFIX
as documented in ../0014.llvm_jit/SIMD_DIRECT.md. Then:

1. Compile uwvm_int_simd_cross.cc for the target with UWVM2TEST_SIMD_CROSS_ADAPTER defined.
2. Run shared_simd_cross_oracle.py PREFIX.c and compile its stdout as C for the target.
3. Link the two objects with that target's C++ runtime and math library, then run natively
   or under QEMU. The adapter validates that all 385 wrappers were redirected.
4. Disassemble the evaluator object; the noinline reference<opcode,...> functions expose
   each operation's actual optimized shared implementation.

## Remaining audit work

- Continue compiler/ISA testing beyond the completed ROS full port and integration matrix.
- Whole-engine uwvm-int/LLVM-JIT integration, every remaining ISA/endian configuration,
  validator feature gating, ABI/calls/globals, and performance comparisons are NOT complete.
- GCC's default target macros use __BYTE_ORDER__/__ORDER_LITTLE_ENDIAN__, while this shared
  header's existing vector gates test __LITTLE_ENDIAN__. On the tested GCC ARM configuration
  that gate is false, so the vector paths are not exercised. Enabling them needs separate
  compiler/builtin validation and measurements, not merely substituting the endian macro.
- Passing this finite input corpus does not prove absence of every overflow, NaN, ABI, or
  memory-protection defect. Future wasm64 parser/runtime support is not enabled by these changes.
