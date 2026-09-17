# ROS full JIT synchronization (2026-09-14)

ROS retains only full LLVM compilation. The previous reduced LLVM capability preflight
has been removed after porting the corresponding real implementations; valid SIMD,
reference, table/bulk-memory and multi-value functions are no longer deliberately rejected.
Lazy compilation, tiered execution, lazy target slots, background publication and OSR
have NOT been restored.

## Implementation

- All 236 Release-2 SIMD opcodes use direct LLVM vector lowering; v128 SSA values,
  parameters, results, locals and PHIs use <16 x i8>. Multiple results use typed tuples.
- Scalar/SIMD memory lowering is shared with the main workspace: zero-extend wasm32
  dynamic addresses before adding offsets, exact-width unaligned accesses, full-guard
  omission of software bounds checks, and overflow-safe partial-guard fallback.
- Cross-custom-page stores preflight the last byte to avoid partial writes before trapping.
  A proven aligned in-page access loses this preflight during optimization.
- Host-provided/moving multi-thread memory retains the required ownership/pinning bridge.
  Numerical SIMD and ordinary full-guard loads/stores do not use that bridge.
- Full-mode table.set/init/copy/fill/grow refresh only affected native targets, including
  every imported alias. Table growth republishes relocated target storage.
- Runtime storage and both translators preserve actual DataCount presence/count.
- Persistent cache schema v11 includes vector SSA ABI, typed tuple-return ABI,
  guarded-store and generated bridge ABI tags. ROS signature/provenance guards remain.
- Shared SIMD C++ module registration is included in xmake.
- Both the ROS standard validator and uwvm-int multiple-table diagnostics now record the real opcode,
  matching the LLVM validator, rather than incorrectly recording the table index.

## Evidence

Linux scratch directory: /tmp/uwvm-ros-full.pYpCVX.
All compilation and execution used the aggregate 64-GiB/no-swap cgroup
uwvmsimd-zHWxwM.slice and CPU affinity 16-31 (16 E-cores).

- Release build with uwvm-int + LLVM full: PASS.
- LLVM-only runtime translation unit, with the production backend/OpenSSL defines: syntax check PASS.
- simd_direct_lowering: 236 opcodes, 385 lane variants, 98,560 differential cases: PASS.
- memory_direct_lowering: 210 configurations, 24,570 boundary decisions: PASS.
  Includes checking that full-guard IR has no software bounds checks.
- llvm_jit_global_storage: scalar/vector/reference storage IR and alignment: PASS.
- wasm2_feature_validator_parity: all eight feature groups, standard/uwvm-int/LLVM
  first-error parity, real CLI rejection, and MVP/Wasm2 call_indirect encodings: PASS.
- llvm_jit_verify_compile: native execution of previously rejected feature fixtures,
  vector direct/indirect calls, tuple results, table mutation, validation failures,
  WASI alignment traps and native unwind: PASS.
- check_simd_memory_boundaries.py: 163 fixtures x four full policies = 652 real
  guard-page/grow/vector-ABI process checks: PASS.
- check_full_feature_integration.py: 65 checks across four full policies and uwvm-int,
  including cross-module vector/tuple calls, mutable globals, imported memory,
  table alias mutation/growth, missing/zero DataCount, and bounds traps: PASS.
- Cache integration: unsigned/unprovenanced fail-closed behavior, path modes,
  generated-IR invalidation, signatures, corrupt/truncated/fuzzed cache recovery,
  and vector/tuple/table-mutation cache hits: PASS.
- ROS direct-lowering cross suite: 29 code-generation profiles PASS; 22 profiles
  execute all 236 opcodes natively or under QEMU (98,560 cases per profile).
  Coverage includes x86-64/i686, AArch64 LE/BE, ARM/Thumb/ARM BE, PPC64 LE/BE,
  s390x, RV64/RV32, LoongArch64, MIPS64r6 LE/BE and MIPS32 NaN2008 LE/BE,
  plus tested no-vector fallbacks. The other profiles are code-generation-only.
- bench-native.s is disassembly of an actual ROS full-mode cached object, not synthetic
  standalone IR: each aligned copy iteration has one vmovups load and one vmovups store;
  add/xor stays in vector registers. Loop bodies contain no SIMD bridge calls.
  Function-entry/exit and host ABI bridges outside those loops are expected.

## Measured performance

100,000,000 iterations, in-Wasm monotonic timing (startup/compilation excluded),
1,000,000-iteration warmup, median of seven measured runs with alternating engine order.
All engines produced matching checksums. Times are milliseconds.

| Kernel | ROS direct | Main direct | Old bridge | WAVM |
|---|---:|---:|---:|---:|
| scalar copy16 | 41.222466 | 41.204837 | 41.177738 | 53.658201 |
| SIMD copy16 | 36.768925 | 36.757452 | 466.362209 | 55.912456 |
| SIMD memory add | 47.162659 | 47.174653 | 906.963919 | 59.384098 |
| SIMD register add/xor | 97.966149 | 97.995297 | 2387.003292 | 97.986793 |

ROS versus main differs by less than 0.05% in these samples. The SIMD kernels are
12.68x, 19.23x and 24.37x faster than the former bridge implementation.
This is evidence for these workloads/host, not a universal performance guarantee.

## Reproduce focused integration

Build the ROS uwvm executable with LLVM full enabled. Test artifacts stay outside the source tree.

```sh
python3 test/0014.llvm_jit/check_simd_memory_boundaries.py /absolute/path/to/uwvm --jobs 4
python3 test/0014.llvm_jit/check_full_feature_integration.py /absolute/path/to/uwvm
```

The ordinary xmake test targets include llvm_jit_verify_compile, llvm_jit_cache_integration,
simd_direct_lowering, memory_direct_lowering, llvm_jit_global_storage and
wasm2_feature_validator_parity. Persistent cache hit tests require a trustworthy build identity;
unknown/dirty builds deliberately test refusal instead.

## Limits

Full-feature support is synchronized; this is not a claim of exhaustive all-platform safety.
Cross ISA execution above tests generated opcode functions, not a complete 32-bit JIT runtime.
PPC32/SPARC runtime restrictions remain. Future wasm64 support is not enabled.
The full C++ module build, all alternative memory backends on ROS, every embedding callback,
and remaining shared-interpreter compiler/ISA configurations still require their own runs.
The earlier interpreter audit and pending GCC endian-gate investigation are recorded in
../0013.uwvm_int/SIMD_AUDIT.md.

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
