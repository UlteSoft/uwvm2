# RV64 RuntimeDyld call-range repair

The [patch](patches/llvm-riscv-rtdyld-call-range.patch) fixes LLVM RuntimeDyld's
RV64 `R_RISCV_CALL` and `R_RISCV_CALL_PLT` relocations. The original loader
silently truncated a 64-bit displacement to the two instruction immediates.
In the existing RV64 QEMU typed-EH probe, default-model code failed on the first
ordinary host call, before any exception was thrown. Selecting the Large code
model avoided that call relocation and provided a useful control; it did not
repair the default loader.

The exact source baseline is LLVM **23.1.1**, tag commit
`6dfe1677ab8dffbc6ec13d53a1e0215d75147689`, with the recorded ROS revisions through
**23.1.1-uwvm-ros.7**. The ROS tree incorporates this repair as
**23.1.1-uwvm-ros.8**. Its source inventory and build/header guards require that
new identity. The ordinary repository provides the standalone patch rather than
modifying an installed LLVM. Apply it from the LLVM monorepo root, rebuild the
affected libraries, and use a distinct provider version/cache identity. Do not
silently mark an unpatched system LLVM as repaired.

Before this patch, the three edited source-file SHA-256 values are:

| File under `llvm/lib/ExecutionEngine/RuntimeDyld/` | SHA-256 |
| --- | --- |
| `RuntimeDyldELF.cpp` | `183873295ce08f87a9aa3654cfdb9885e2a3ab49faf9a0dbb3c05e37a6dabd24` |
| `RuntimeDyldELF.h` | `c35654e200db387f9f0a972c7e338f6fef692f90fbcd23fd1602f1a012c42c08` |
| `RuntimeDyldImpl.h` | `87cbaf5e5db8b6987a7d5d9cb4c0e3b40d5f6e1cd780ca817e08a7eb31d40662` |

## Relocation and calling semantics

The RV64 AUIPC/JALR sequence can represent a displacement from `-0x80000800`
through `0x7ffff7ff`: the rounded high part, `delta + 0x800`, must fit signed
32 bits. The check uses unsigned arithmetic before interpreting the signed
result, so computing the rounding carry does not cause signed overflow. RV32
retains its existing XLEN wrapping semantics. This follows the
[RISC-V procedure-call ABI](https://riscv-non-isa.github.io/riscv-elf-psabi-doc/#procedure-calls)
and LLVM's own rounded check in
[JITLink's RISC-V fixups](https://github.com/llvm/llvm-project/blob/main/llvm/lib/ExecutionEngine/JITLink/ELF_riscv.cpp).

External and cross-section RV64 calls reserve a 24-byte, eight-byte-aligned
stub within their calling section. Its `auipc t3; ld t3; jr t3; nop` prefix
loads an adjacent full-width destination literal. LLVM's existing JITLink PLT
also uses `t3`. The stub preserves `ra`, `sp`, and all argument registers;
`jalr rd=x0` therefore remains a tail call, and no extra stack frame appears
in exception unwinding. The symbol's addend is applied once to the literal.
Calls with the same symbol/addend reuse a stub.

Final relocation still attempts the original target first. A reachable call
keeps its original direct AUIPC/JALR pair; it executes no extra branch, load,
or guard. Only an out-of-range call uses the stub. Both direct and stub
displacements receive active Release-build checks. If stub allocation is
disabled, or the reserved stub is itself unreachable, relocation fails with a
diagnostic instead of publishing truncated instructions. The original register
fields remain intact. Existing relocation records carry the optional stub
offset; neither the record layout nor the cached native-object format changes.
Fresh loads rebind literals for the current engine and final section addresses.

This repair is limited to CALL/CALL_PLT. It does not claim that every other
RISC-V relocation or arbitrary code layout has been qualified.

## Qualification and build boundary

All compilation and execution ran on SSH Linux in the 64 GiB/no-swap cgroup,
with CPUs `0,2,4,6,16-31`. The diagnostic build recompiled only
`RuntimeDyldELF.cpp`, `RuntimeDyld.cpp`, and `Targets/RuntimeDyldELFMips.cpp`, then
replaced those objects in private copies of the existing native/RV64
`libLLVMRuntimeDyld.a`. The original shared dependency libraries were retained.

- The standalone relocation fixture passes **793 checks** covering both rounded
  boundaries, positive/negative far targets, addends, stub alignment/reuse,
  same-section direct calls, cross-section addresses, tail-call registers, and
  RV32 address wrapping. A separate process confirms an out-of-range call with
  stub allocation disabled fails with the expected diagnostic.
- The retained `llvm-rtdyld` RUN lines pass five loaded-instruction checks. The
  updated `.8` public-header guard passes all eight positive/negative controls,
  including rejection of the previous `.7` identity.
- Actual **default-code-model RV64 QEMU** execution passes the symbols probe in
  each repository: **53 checks per repository**, including engine isolation and
  relocation of a cached object under another typeinfo binding.
- The same default-model QEMU configuration passes each repository's product
  landingpad probe: **18,313 checks / 1,152 JIT executions per repository**,
  covering typed catch, foreign resume, rethrow, cleanup when a new exception
  escapes, both instruction/unwind trace policies, and cold/warm cache loads.

These diagnostic archives retain the original `.7` generated configuration
with explicitly replaced loader objects. They must not be reported as a full
`.8` packaged-library or CLI build. The `.8` manifest/guard change requires a
fresh normal dependency configuration; that complete build is a separate
qualification step. Earlier successful Large-model probes remain valid
controls and are not relabeled as default-model results.

The fixture source is retained under the bundled LLVM
`llvm/test/CodeGen/RISCV/rtdyld-call-range.*` and
`llvm/test/CodeGen/RISCV/Inputs/rtdyld-call-range.cpp`; it is also included in the
ordinary repository's patch. Build commands, input hashes, QEMU logs and the
relocation fixture's failed-no-stub log are retained with the task evidence.

The verified evidence archive is `riscv-rtdyld-call-range-r1.tar.gz` under
`build/wasm3-evidence/` in the ordinary repository, with 226 files and SHA-256
`7a5c074cf483829650bcca34205c11f2ae95cbab0f06608b0acae1d22f4bd90f`.
The patch SHA-256 is
`32f271eb86c4810d74630c2041077622a7bcf6d6ba4e31af8ba1ae219c4482b0`.
The updated source manifest hashes 12,874 retained files and has SHA-256
`b86c65d7e6290267fe11415c2d28f9c2dff7a24d8f49d51fc293b6d9777670f6`.
