# Changes relative to official LLVM 23.1.1

Base: `llvmorg-23.1.1`, commit
`6dfe1677ab8dffbc6ec13d53a1e0215d75147689`.
Archive/signature provenance is in [README.md](README.md).

## Compiled LLVM source changes

Only these seventeen retained compiler files differ from the official release:

| File | Reason |
| --- | --- |
| `llvm/lib/Target/Mips/MipsSEInstrInfo.cpp` | R6 condition-register copies must use the physical GPR32/FGR64 copy instructions. Looking for a nearby CMP/SEL is unreliable after scheduling/register allocation and can fail on valid shared conditions. Revision 4 extends the same low-word rule to microMIPS, paired with its MC encoding repair below. |
| `llvm/lib/Target/Mips/MipsRegisterInfo.td` | FGR64CC stores full f64 values; a 32-bit spill slot/instruction can lose half the value or make legal spills fail. Give the class 64-bit spill size. |
| `llvm/lib/Target/VE/VEInstrInfo.cpp` | Stack-growth expansion splits a post-register-allocation block. Recompute continuation/syscall live-ins in successor-first order; otherwise live argument registers become undefined to the machine verifier. No additional runtime instructions. Based on upstream [PR #221553](https://github.com/llvm/llvm-project/pull/221553), commit `7dbdc02e2f5bd0c2aefff489f737a53342277d06`; that PR was still open when inspected. |
| `llvm/lib/Target/AVR/AVRShiftExpand.cpp` | Scalarize fixed-vector shifts lane by lane before scalar loop expansion. The original pass produced illegal vector-to-scalar trunc IR; simply skipping vectors would miss wide variable shifts that need AVR's loop instead of unavailable libcalls. Nonsplat counts retain per-lane semantics. |
| `llvm/lib/Target/MSP430/MSP430ISelLowering.cpp` | Select/variable-shift expansion can split an active call-frame setup. Both new blocks inherit the call-frame size at the split, instead of incorrectly starting at zero. This preserves stack reasoning rather than disabling verification. |
| `llvm/lib/CodeGen/SelectionDAG/LegalizeDAG.cpp` | Inverting an expanded FP comparison must use the boolean representation of its original FP operand type, not its integer result type. MIPS R6 has integer true = 1 but FP true = -1; XOR with 1 corrupts sign-extended SIMD masks (equal inputs incorrectly produce -2). Applies to ordinary/strict comparisons and VP inversion without adding a runtime helper. |
| `llvm/lib/Target/Mips/MipsSEISelLowering.cpp` | R6 hard-float strict comparisons must bypass the inherited pre-R6 FCC0/CMOV custom lowerer: those instructions do not exist on R6. Keep strict nodes legal for chain-aware instruction selection. Pre-R6/soft-float lowering is unchanged. |
| `llvm/lib/Target/Mips/Mips32r6InstrInfo.td` | Select quiet/signaling strict predicates using the matching CMP instruction. Explicitly retain the strict dependency chain and FP-exception property: adding Pat patterns alone inherited chainless ordinary-pattern inference, which deleted unused strict comparisons. Ordinary comparisons remain chainless and freely schedulable. |
| `llvm/lib/Target/Mips/MicroMips32r6InstrInfo.td` | Instantiate strict quiet/signaling comparisons, match strict arithmetic and legal min/max/canonicalization, and select R6 constant materialization and register-tailcall encodings. Pattern predicates do not automatically inherit restrictions from their output instructions. |
| `llvm/lib/Target/Mips/MicroMipsInstrFPU.td` | Match strict as well as ordinary arithmetic via `any_f*`, as the lowering already marks it legal. Preserve the strict dependency chain even when the arithmetic result is unused. |
| `llvm/lib/Target/Mips/MicroMipsInstrInfo.td` | Restrict the old arbitrary constant-materialization patterns to pre-R6. Otherwise they select the removed microMIPS LUI encoding, despite the result instruction's own ISA predicate. |
| `llvm/lib/Target/Mips/MipsInstrInfo.cpp` | Compact-return/branch selection must distinguish microMIPS revisions. An old JRC halfword decodes as MOVEP on R6, so successful printing and machine verification do not establish a valid return. |
| `llvm/lib/Target/Mips/MCTargetDesc/MipsMCCodeEmitter.cpp` | Map private FGR64 condition copies, physical FP copies, PIC-prologue LUI and late PseudoCVT expansions to actual microMIPS R6 encodings. These operations can be selected after DAG matching and lack the regular mapping-table entries; emitting the standard-MIPS word executes unrelated instructions. No runtime helper or additional guest instruction is introduced. |
| `llvm/lib/Target/Mips/Mips16HardFloat.cpp` | Intrinsics do not have a native call ABI. In particular, synthesizing an FP-call stub for constrained FP metadata creates invalid IR. Leave intrinsic/libcall lowering to SelectionDAG. This does not complete the separate MIPS16 MC/pseudo support. |
| `llvm/lib/Target/Mips/Mips16ISelLowering.cpp` | Each new select-diamond block inherits the active call-frame size at the split. The split can occur during libcall argument preparation after call-frame setup; resetting it to zero breaks stack verification. |
| `llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyld.cpp` | AArch64 instructions are always little-endian, including in big-endian ELF. Write every far-call stub instruction explicitly as little-endian, not in the target's data byte order. Otherwise object loading can succeed with invalid executable bytes. No instruction is added to the stub. |
| `llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldELF.cpp` | Preserve all displacement bits for `LD_PREL_LO19` and `ADR_PREL_LO21`: the previous `0xffc` mask lost offsets above 4 KiB within their valid signed 1 MiB range. Read/modify/write instruction words explicitly as little-endian, also for `CONDBR19`, instead of depending on the loader host's byte order. Revision 6 also resolves defined ELF locals from their referenced symbol-table entries, not a name map: RISC-V/LoongArch `.L0 ` labels can repeat and previously redirected earlier FDEs to the last function. Global/weak resolution is unchanged. |

Patch: [MIPS R6 condition registers](../../documents/toolchain/patches/llvm-mips-r6-condition-registers.patch).
Upstream submission: [llvm/llvm-project#223905](https://github.com/llvm/llvm-project/pull/223905),
commit `a830ec0e085d364e72a5126a9f866bae5b0fb09c`.
This is a **downstream patch on a stable release**, not a claim that the original
release already contained it. The patch leaves pre-R6/microMIPS handling alone.

Three added regression files under `llvm/test/CodeGen/Mips/` are
`r6-condition-copy.mir`, `r6-fgr64cc-spill.mir` and
`r6-shared-select-condition.ll`. Their ten RUN lines test physical copies,
full-width spills and shared comparison/select conditions.

Revision `-uwvm-ros.2` adds `VE/extend-stack-liveins.ll` (two RUN lines),
`AVR/vector-variable-shifts.ll` and `MSP430/select-call-frame.ll` under the same
`llvm/test/CodeGen/` root. The retained inventory now has six files / fourteen
RUN lines, all with machine verification. The AVR/MSP430 fixes are downstream
changes, not claims of inclusion or acceptance upstream. VE's separate short-
vector fallback lives in **UWVM's TargetMachine feature selection**, not LLVM: no
unverified general-purpose VL-register backend rewrite is included here.

Revision `-uwvm-ros.3` additionally retains
`Mips/r6-fp-predicate-inversion.ll`. Its six RUN lines cover both endiannesses,
O32/N64, and microMIPS R6, including discarded-result strict comparisons.
That revision's inventory was seven files / twenty RUN lines. These new predicate
repairs are downstream changes, **not part of PR #223905** above. That earlier
PR repairs physical copies/spills and cannot repair generic boolean inversion.

For strict comparisons LLVM distinguishes quiet `constrained.fcmp` (Invalid
for sNaN) from signaling `constrained.fcmps` (Invalid for any NaN); see
[LLVM's intrinsic contract](https://llvm.org/docs/LangRef.html#llvm-experimental-constrained-fcmp-and-llvm-experimental-constrained-fcmps-intrinsics).
Checking only a returned i1, finite inputs, or successful machine verification
misses these bugs. The diagnostic R6 execution oracle checks all fourteen
predicates, f32/f64, retained/discarded results and exception flags separately.
See the target-repair audit for exact tested snapshots; a diagnostic relink is
not proof that a subsequently packaged library/CLI was rebuilt correctly.

Revision `-uwvm-ros.4` adds six MIPS regressions: `mips16-callframe.ll`,
`mips16-constrained-intrinsic.ll`, `micromips-strict-arithmetic.ll`,
`micromips-r6-minmax.ll`, `micromips-r6-compact-control.ll`, and
`micromips-r6-physical-copies.ll`. That revision's inventory was thirteen files
/ thirty-five RUN lines. The last test checks the actual MC encoding and
relocation kind, not just the identical printed mnemonic. Strict arithmetic
tests retain discarded results, and compact control tests explicitly enable
LLVM's opt-in MIPS tail calls. These are additional downstream repairs, not
part of the earlier upstream PR #223905.

The diagnostic microMIPS R6 compiler passed the full 236-opcode SIMD oracle,
guarded-store execution and 28,672 strict comparison/result/Invalid checks.
MIPS16 assembly regressions pass but its separate direct-object and external
PIC assembly failures remain recorded; do not claim MIPS16 MCJIT support or
silently substitute an external assembler. See the target-repair audit for
the exact diagnostic build and subsequent packaged-build status.

Revision `-uwvm-ros.5` adds AArch64 `rtdyld-stub-byte-order.ll` and
`rtdyld-pcrel-full-width.ll`, with separate little-/big-endian `.check` files.
At revision 5 the retained inventory is nineteen files / forty-three RUN lines. Checks inspect
all five relocated stub instructions and positive/negative LDR/ADR displacements
beyond 4 KiB, including nonzero ADR low bits. Expected encoding follows
[AAELF64](https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst).
The old loader fails the big-endian stub and both PC-relative controls; an
isolated relink passes them. These are relocated-byte regressions, **not** native
AArch64_BE VM/unwind execution or a complete rebuild of the packaged revision.
The source comments distinguish data endianness from instruction endianness so
that a future simplification cannot restore a superficially successful load
with wrong executable bytes. These repairs are **not part of PR #223905**.

Revision `-uwvm-ros.6` adds `RISCV/rtdyld-local-symbols.ll` and its `.check`
file, bringing the inventory to twenty-one files / forty-nine RUN lines. The
RISC-V32, RISC-V64 and LoongArch64 controls check the first FDE's relocated
address with two distinct, same-named local symbol-table entries. The old
loader fails all three; the isolated repaired loader passes all three. ELF
relocations select an entry by symbol index, while local names need not be
globally unique ([ELF symbol table specification](https://gabi.xinuos.com/elf/05-symtab.html)).
The repair preserves global/weak resolution and undefined-symbol handling.
It adds no generated instructions or per-Wasm-call work. Separate QEMU controls
recover all ten recursive/entry frames on RISC-V64 and LoongArch64; RISC-V64
also passes the trap, guarded-access and signaling-NaN conversion-trap cases.
Other signal-unwinder failures remain recorded, including pure GCC/system-linker
controls; they are not claimed repaired by this local-symbol patch. This repair
is also **not part of PR #223905**. A fresh packaged `.6` build and its tests are
required independently of the completed `.5` build and private candidate tests.

## Packaging and build changes, not upstream backend fixes

- Unrelated top-level projects and bulk documentation/tests/examples were
  omitted as listed in README.md; `.git` and Git ignore/attribute metadata
  were not imported. Required libc shared math headers remain.
- Three internal mlgo utility symlinks were materialized with identical target
  bytes for Windows checkouts. Vendor line-ending conversion is disabled.
- ROS xmake builds Release static archives with suffix `-uwvm-ros.6` and
  verifies `sources.sha256`. Its CMake metadata target exports include paths,
  definitions and the complete ordered static-library closure.
- Separately from compiler-source edits, ROS native MIPS target selection uses
  `+noabicalls,+long-calls`: RuntimeDyld's far stubs jump through `at`, not the
  PIC host callee's required `t9`. O32/N32 ignore long-calls with ABICalls still
  enabled; static N64 already disables ABICalls. The ordinary runtime uses the
  same policy and both reject older v1 call-policy cache contexts. See
  [the call-range evidence and real code-size cost](../../documents/toolchain/elf-local-symbol-unwind.md).
- No `llvm-config` executable is built or queried. `LLVM_TOOL_LLVM_CONFIG_BUILD`
  is explicitly OFF. LLVM's public generated **header** `llvm/Config/llvm-config.h`
  remains required for version/platform definitions; it is not the executable.
- The metadata-only CMake target is never compiled or executed, so dependency
  discovery does not require running target code during cross compilation.
- ROS's own CMake hook rejects non-Release configurations, an omitted native
  JIT backend, or a cross toolchain lacking explicit LLVM_HOST_TRIPLE. xmake
  clears previously inferred triple cache entries before reconfiguration.
  These are application build-policy changes, not edits to upstream LLVM's
  host-triple inference or backend implementations.
- The integration also rejects unsafe floating-point flags from CMake's
  resolved consumer command before building archives. Directory compile options
  can bypass CMAKE_CXX_FLAGS; checking that cache variable alone is insufficient.
  This check changes no upstream LLVM source or generated guest instructions.
- Windows CRT selection uses CMAKE_MSVC_RUNTIME_LIBRARY with checked library
  target properties. LLVM 23 no longer consumes LLVM_USE_CRT_RELEASE; retaining
  that old option could silently mix release/debug or static/dynamic CRT ABIs.
  This is a ROS recipe correction, not an upstream LLVM source patch.
- ROS resolves the build-cache directory's physical path before locking and
  configuring it. Symlink aliases must not change CMake's exported include paths
  and repeatedly invalidate Ninja's command hashes. The small path resolver is
  application build integration, not a modification of LLVM's source tree.
- CI removes llvm-config setup/prefix queries and explicitly installs CMake and
  Ninja. Installed Clang/SDK packages are bootstrap compilers, not ROS's LLVM
  dependency. This CI integration is outside the vendored upstream sources.
- The named-module build checks the bootstrap Clang for lost internal-variable
  initialization using a real full-BMI-to-IR probe. A failing compiler is
  rejected, with a link to upstream fix #218304. This is a ROS build guard, not
  an LLVM source patch: Clang is not included in this library-only vendor
  tree, and a linked LLVM library cannot repair its bootstrap compiler.

There is no downstream X86 conversion patch: the official 23.1.1 release
already has the correct mixed-signedness combine. ROS's workaround removal is
an application-code change, verified separately, not an edit to vendored X86.

To verify that retained bytes have no additional unrecorded upstream changes:

```sh
python3 third-parties/llvm/provenance/verify-upstream.py /path/to/llvm-project-23.1.1.src.tar.xz
```

This checks the exact archive SHA-256, every retained source against its
manifest, and the explicit seventeen-edit/twenty-one-addition allowlist. It extracts no
archive paths. Update the allowlist and this document together when changing
the release or downstream patch; do not merely regenerate the manifest.
