# MIPS r6 condition-register backend repair

Validated on 2026-09-16 against LLVM 22.1.8 and LLVM 23 commit
`4c4c1db7c69a6fda6cfa6bc6066bb09a433edc89`. This is a **downstream LLVM backend
patch**, not a change to Wasm semantics or a test-only `noinline` workaround.

Upstream submission: [llvm/llvm-project#223905](https://github.com/llvm/llvm-project/pull/223905),
targeting `main` (LLVM trunk). The fork branch is
[`MacroModel:fix-mips32-bug`](https://github.com/MacroModel/llvm-project/tree/fix-mips32-bug),
commit `a830ec0e085d364e72a5126a9f866bae5b0fb09c`, based on trunk
`182ca95eed3feb7c7ba83ed2b35a0c5f29517b9b`. Submission is not acceptance or a
released-toolchain fix; continue applying the downstream patch until the actual
dependency includes it. The PR adds three LLVM IR/MIR regression files: all ten
RUN lines pass on each patched LLVM 22/23 backend and fail on the unpatched
LLVM 22 negative control. Current-trunk whole-suite validation is not claimed.

## Why two changes are necessary

1. After allocation, a scalar condition can be copied from a GPR32 into an
   FGR64 carrier, copied again into another FPR, and finally consumed by `sel.d`.
   `MipsSEInstrInfo::copyPhysReg()` previously guessed legality by scanning for
   nearby comparisons/selects. An intervening copy, block boundary or spill
   breaks that guess. The opcode remains zero: an unintended machine `PHI`,
   reported as `Unsupported instruction: <MCInst 0 ...>` in release builds.
   On non-microMIPS r6, explicitly support low-word GPR32/FGR64 transport with
   `mtc1`/`mfc1`. These are bit moves, not numerical conversions. As in LLVM's
   existing r6 select lowering, the upper word of an imported condition is
   undefined and `sel.d` only consumes bit zero.
2. `FGR64CC` lists `[i32, f32, f64]`. Its `64` template argument specifies
   alignment, not saved-value size; the default size follows the first `i32`.
   This produces four-byte spill slots aligned to eight bytes. The class also
   carries the **full f64 result** of the tied `sel.d` instruction, so narrowing
   its storage is invalid. Explicit `Size = 64` also restores its FGR64 subclass
   relationship, allowing spill/reload lowering to select `SDC164`/`LDC164`
   instead of opcode-zero PHIs. Fixing only the physical copy misses this bug.

The patch retains optimization, native floating instructions, normal register
allocation, bounds checks and trap tracking. It does not introduce runtime
bridges, soft-float fallback, global `optnone`, or additional Wasm call frames.
An actual spill now saves the complete value; an eight-byte slot is required
for correctness, not an optional performance regression to optimize away.

## Apply to the dependency, not only UWVM

The patch is [llvm-mips-r6-condition-registers.patch](patches/llvm-mips-r6-condition-registers.patch).
From an LLVM checkout, check and apply it, then rebuild the toolchain normally:

```sh
git apply --check /path/to/uwvm2/documents/toolchain/patches/llvm-mips-r6-condition-registers.patch
git apply /path/to/uwvm2/documents/toolchain/patches/llvm-mips-r6-condition-registers.patch
cmake --build /path/to/llvm-build --target clang llc LLVM --parallel 4
```

Use an aggregate memory/CPU limit around the build; `--parallel` is not a memory
limit. All validation here ran in this campaign's **32 GiB** parent cgroup,
28 GiB memory.high, zero swap, CPUs **16–31**. The other agent's budget was not
used. No global SDK/library, shell profile or system configuration was replaced.

Both dependencies matter: Clang's backend compiles uwvm-int C++, while the
runtime's linked `libLLVM` compiles LLVM-JIT functions. Updating just the UWVM
repository, just Clang, or just the JIT library does not repair the other path.
Static consumers require relinking. Verify the actually loaded shared library.
Use a new JIT cache directory/source provenance when deploying a patched LLVM
with an unchanged version string; do not replay old MIPS objects as validation.

The independent source remains unchanged in behavior. Existing target-specific
test-driver isolation stays useful for **unpatched** system compilers; it is
neither proof of this fix nor a substitute for applying it.

## Regression tests and observed results

Run [check_mipsr6_backend.py](../../test/0014.llvm_jit/check_mipsr6_backend.py)
with `--clang`, `--llc` (patched), `--stock-llc` (unpatched), `--ld` (MIPS GNU
linker), `--qemu-dir`, and a fresh `--output` directory. The script records
commands, input hashes, logs and return codes, and kills complete process groups
on timeout. A stock timeout/unrelated failure cannot count as the expected bug.

- For **each** LLVM version, O32 big/little endian × O0/O1/O2/O3/Os/Oz:
  12 successful QEMU executions, 2,409,600 exact-bit vector selections and
  4,819,200 f64 spill/restore checks. Inputs include signed zeros, subnormals,
  infinities, signaling/quiet NaNs with payloads, and deterministic random bits.
- Both byte orders also reproduce the original copy failure with the unpatched
  backend. A separate forced-all-FPR-clobber MIR regression reproduces its
  missing spill/reload handling. Patched allocation must show eight-byte slots
  and real `SDC164`/`LDC164`, with `-verify-machineinstrs` enabled.
- N32/N64, big/little endian: four verified object-generation checks per LLVM
  version. These are **not** N32/N64 guest execution results.
- LLVM 23 production SIMD emitters compiled against both frozen main and ROS
  snapshots each pass the same 12-execution matrix, with an additional 2,409,600
  exact-bit Wasm-byte-order selections per product. Build
  `fixtures/mipsr6_wasm_select_emit.cpp` with the normal LLVM-JIT test defines
  and include/library paths, then supply `--jit-emitter` and matching `--opt`.
  This tests emitted machine code under QEMU, not a complete MIPS CLI.
- The original **unisolated** uwvm-int SIMD-bit and SIMD-NaN LLVM 22 IR snapshots
  now pass machine verification, linking and QEMU execution. Neither original
  test IR was rewritten to hide the failing copy or the spill pressure.
- A private patched LLVM 23 shared library also fixes the original direct
  Clang `-c` command in both byte orders. The installed Clang executable and
  frontend library were unchanged. Unpatched negative controls still fail.
- Both frozen native x86_64 full-JIT CLIs loaded that private library and each
  passed 108 runs, including 72 signed-cache recursive trap replays: six
  optimization policies, unreachable/OOB/float-to-int traps, independent native
  libunwind and instruction tracking, exact recursive frames. Fresh cache
  directories were used. This checks library integration, not a MIPS-native CLI.
- Native x86_64, AArch64 and MIPS32r2 BE/LE control objects are byte-identical
  before/after loading that private library. The standalone r6 function retains
  native `mtc1`, `cmp.lt.d`, `sel.d`, no helper calls and no stack-frame allocation
  (O32 stack-passed arguments still require loads). Do not extend this observation
  to all JIT functions: the byte-buffer production fixture has register saves
  and byte-addressed loads, unlike the standalone vector-ABI function. Its
  main/ROS × BE/LE assembly is byte-identical to stock LLVM in all four controls;
  those existing costs were not introduced by the repair.

Evidence on SSH `linux` is under
`/tmp/uwvm-comprehensive.eUFnxH/mipsr6-backend-fix/`: `matrix22-final`,
`matrix23-final`, `matrix23-jit-{full,ros}`, `original-tests-v2`,
`shared23-checks`, `shared23-{full,ros}-cache`, source snapshots, build commands,
generated IR/MIR/assembly.
The final checked-in fixture/script revisions were rerun in
`matrix22-committed-source` and `matrix23-committed-source` (the latter also
includes the production emitter). Evidence is backed up on the Mac in
`/tmp/uwvm-comprehensive-audit.i2TDbc/mipsr6-backend-fix-evidence.tar.gz`
(SHA-256 `d3d235b919571602fca5c514e4e1dd8de054788fcd36695891994a3cea4c9ffd`).

The private tested library also has a hash-verified, reboot-persistent copy at
`/var/tmp/uwvm-comprehensive.kH4XCt/mipsr6-llvm23/libLLVM.so.23.0git`
(SHA-256 `b2ac5d4f1f777c82d8a870ebe9b5d256758bf31f07f9cfb994612022a8a94205`).
Opt in per process by prepending that directory to the matching SDK's
`LD_LIBRARY_PATH`. **Do not mix it into a different LLVM version or make a
global preload.** The ordinary installed SDK remains unmodified.
This binary is for the matching **x86_64 Linux** SDK. A native MIPS JIT build
must rebuild its own architecture's `libLLVM` from the patched sources.

This repair/coverage does not certify every MIPS instruction, microMIPS, every
operating system, all LLVM releases or a complete cross-platform UWVM CLI.
The new physical-copy rule explicitly excludes microMIPS, whose encodings need
separate treatment. No claim is made that the patch has been accepted upstream.
