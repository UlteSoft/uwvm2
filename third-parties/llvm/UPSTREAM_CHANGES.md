# Changes relative to official LLVM 23.1.1

Base: `llvmorg-23.1.1`, commit
`6dfe1677ab8dffbc6ec13d53a1e0215d75147689`.
Archive/signature provenance is in [README.md](README.md).

## Compiled LLVM source changes

The following fifty-five compiler inputs and one exported configuration
header template differ from the official release:

| File | Reason |
| --- | --- |
| `llvm/lib/Target/ARM/ARMFastISel.cpp` | TailCC normal calls and returns decline FastISel before ABI assignment or instruction emission, so the existing SelectionDAG TailCC implementation handles O0 typed Wasm calls. Other calling conventions retain upstream behavior; this does not change the C host entry ABI or enable ARM EHABI unwind. Real MCJIT tests cover non-tail integer and void TailCC calls and returns at O0 and optimized code generation. |
| `llvm/lib/Target/Mips/MipsBranchExpansion.cpp` | Only the actual `uwvm.native.debug.pcrel` N64 pre-R6 function attribute converts intra-function J edges to PC-relative B before the unchanged long-branch range pass. This avoids RuntimeDyld section-tail trampolines without expanding native-debug ownership, changing the static C host-call ABI, or altering unmarked/MIPS16/microMIPS/R6 functions. Actual MCJIT regression checks both marked containment and the unchanged unmarked trampoline path. Revision 13 also keeps marked long B edges inside the function with full-width relocated targets and explicit delay slots. |
| `llvm/lib/Target/Mips/MipsISelLowering.cpp` | Exact static N64 non-varargs C `musttail` bypasses the optional sibling-call switch, retaining unchanged incoming-area/byval/local-callee checks. PIC, other ABIs and differing C prototypes keep the original opt-in policy. Revision 13 separately adds qualified static N64 R2 TailCC callee-pop argument areas, signed growing/shrinking stack offsets, load/store ordering and normal-call callee-pop accounting. |
| `llvm/lib/Target/PowerPC/PPCISelLowering.cpp` | PPC32 static ELF exact-prototype, non-varargs C `musttail` reuses the unchanged incoming C argument area. PPC64 ELFv1/v2 adds a private paired TOC-tail ABI: normal calls explicitly restore the caller TOC; exact C musttail uses incoming stack slots and descriptor/global-entry indirect transfers. Its real CodeGen library exports a capability query; unmarked functions retain upstream TOC guards. Byval remains rejected. |
| `llvm/lib/Target/PowerPC/PPCAsmPrinter.cpp` | For the existing private 64-bit ELF non-varargs C TOC-tail contract, use the same synthesized TOC slot for both halves of a medium-code-model indirect load. A high half naming the function descriptor paired with a low half naming the slot produced an invalid address. Unmarked functions retain upstream slot selection. |
| `llvm/include/llvm/IR/IRBuilder.h` | Use a static no-op function for the three default intrinsic callbacks, preserving the empty callback behavior without temporary lambda callable identity across C++ module boundaries. |
| `llvm/lib/Target/PowerPC/PPCFastISel.cpp` | Exact private TOC-tail callers/callees decline FastISel normal-call lowering at O0, so SelectionDAG performs the required global-entry transfer and explicit TOC restore. Other functions retain upstream FastISel behavior. |
| `llvm/lib/Target/PowerPC/PPCISelLowering.h` | Pass the actual LLVM CallBase to the private PPC32 mandatory-tail eligibility check. |
| `llvm/lib/Target/Sparc/SparcISelLowering.cpp` | `disable-tail-calls=true` disables optional sibling calls while preserving mandatory `musttail`; the existing V9 argument-area and ABI checks remain in force. |
| `llvm/lib/Object/RelocationResolver.cpp` | Resolve V9 DISP32/DISP64 and SystemZ PC32/PC64 in the independent DWARF object reader; a valid PC-relative EH section must not invalidate the actual Wasm line/location metadata. |
| `llvm/lib/Target/Sparc/TargetInfo/SparcTargetInfo.cpp` | Revision 11 registers `sparcv9` as a JIT target alongside the completed ELF64 RuntimeDyld loader; 32-bit and little-endian SPARC remain AOT-only. |
| `llvm/include/llvm/ExecutionEngine/RuntimeDyld.h` | Revision 11 declares library-defined ELF loader and PPC64 paired TOC-tail ABI capability queries, preventing patched-header/stale-archive mixtures from enabling unsupported native targets. |
| `llvm/include/llvm/Config/llvm-config.h.cmake` | Revision 11 exports explicit PPC32/SPARC V9 loader capability macros so consumers cannot infer patched loader support from a stock version number. Revision 12 adds the exact X86 aligned-frame TailCC capability. Revision 13 packages the explicit LoongArch64 and MIPS N64EL R2 TailCC capabilities. |
| `llvm/lib/Target/X86/X86ISelLoweringCall.cpp` | Revision 12 rounds incoming guaranteed-tail call frames with MaxStackArgAlign before return-address-slot alignment, matching outgoing callee-pop accounting. Ordinary C and vararg raw stack accounting stays unchanged. |
| `llvm/lib/CodeGen/AsmPrinter/AsmPrinter.cpp` | Revision 10 emits an opt-in nonexecuting actual text begin/code-end relocation table for all emitted definitions; code end is before target-specific trailers, with actual assembler entry/local-entry names in v2. Unknown shapes stay claims; permissions remain a separate canonical runtime decision. |
| `llvm/lib/Target/RISCV/RISCVISelLowering.cpp` | Revision 9 stores C musttail arguments in incoming fixed slots after all overlapping reads; TailCC supports different argument areas, reserves growth before spills, and emits explicit tail SP deltas and callee-pop calls. |
| `llvm/lib/Target/RISCV/RISCVCallingConv.cpp` | Revision 9 gives TailCC FastCC argument registers and direct fixed-vector overflow slots, avoiding same-position indirect storage under differing prototypes. |
| `llvm/lib/Target/RISCV/RISCVInstrInfo.td` | Revision 9 carries a signed argument-area delta in TC_RETURN pseudos until frame lowering emits genuine direct/indirect tails. |
| `llvm/lib/Target/RISCV/RISCVFrameLowering.cpp` | Revision 9 combines frame deallocation and tail argument ownership, describes callee-pop CFA/register offsets, and uses dynamic call frames for normal TailCC stack calls so landing pads recover the stable caller SP. |
| `llvm/lib/Target/RISCV/RISCVMachineFunctionInfo.h` | Revision 9 records callee-popped argument bytes and normal TailCC stack calls, including MIR serialization; opaque spill frame schemes are excluded for the new ABI. |
| `llvm/lib/Target/RISCV/RISCVMachineFunctionInfo.cpp` | Revision 9 clones/serializes TailCC frame properties and excludes compressed fixed-layout push/pop frames for that ABI. |
| `llvm/lib/Target/SystemZ/SystemZElimCompare.cpp` | Revision 7 checks address-register definitions and memory effects before sinking a load into a conditional trap. It clears stale address-register kills and preserves memory operands. The previous transform could cross aliasing stores or leave an undefined base register. Adjacent safe load/trap fusion remains enabled. |
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
| `llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldELF.cpp` | GNU ARM EHABI TARGET2 and GOT_PREL allocate real typeinfo GOT slots and relocate the place-to-slot displacement with REL32; implicit and explicit addends are applied once, and local symbol offsets stay in the slot. Unsupported ARM relocations report a loader error instead of release-build undefined fall-through. The linked loader exports the exact ABI-v1 capability query. |
| `llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldELF.cpp` | Preserve all displacement bits for `LD_PREL_LO19` and `ADR_PREL_LO21`: the previous `0xffc` mask lost offsets above 4 KiB within their valid signed 1 MiB range. Read/modify/write instruction words explicitly as little-endian, also for `CONDBR19`, instead of depending on the loader host's byte order. Revision 6 also resolves defined ELF locals from their referenced symbol-table entries, not a name map: RISC-V/LoongArch `.L0 ` labels can repeat and previously redirected earlier FDEs to the last function. Global/weak resolution is unchanged. Revision 8 checks the rounded RV64 CALL/CALL_PLT displacement and uses a full-width same-section stub only when the direct call is out of range. |
| `llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldELF.h` | Revision 8 reserves 24 bytes and 8-byte alignment for RV64 call stubs and carries their optional offset into final relocation. RV32 keeps its XLEN wrapping semantics. |
| `llvm/lib/ExecutionEngine/RuntimeDyld/RuntimeDyldImpl.h` | Revision 8 documents the CALL/CALL_PLT-specific use of the existing `SymOffset` field; relocation layout and object-cache format remain unchanged. |

| `llvm/lib/Target/LoongArch/LoongArchExpandPseudoInsts.cpp` | Revision 13: Preserve the signed TailCC argument-area delta through small/large symbolic pre-register-allocation expansion; final transfers have no link. |
| `llvm/lib/Target/LoongArch/LoongArchFrameLowering.cpp` | Revision 13: Callee-pop TailCC frame restoration, dynamic normal-call frames, CFA corrections and asynchronous epilogue rows; disable shrink wrapping for the private ABI. |
| `llvm/lib/Target/LoongArch/LoongArchISelLowering.cpp` | Revision 13: Assign scalarized vectors directly, order incoming loads before outgoing stack stores, and lower different-prototype callee-pop TailCC transfers. |
| `llvm/lib/Target/LoongArch/LoongArchInstrInfo.td` | Revision 13: Define TailCC DAG nodes and transfer pseudos carrying the signed argument-area delta until frame lowering. |
| `llvm/lib/Target/LoongArch/LoongArchMachineFunctionInfo.h` | Revision 13: Record aligned incoming callee-pop bytes and ordinary TailCC stack calls. |
| `llvm/lib/Target/LoongArch/MCTargetDesc/LoongArchMCTargetDesc.cpp` | Revision 13: Emit explicit C ABI initial same-value CIE rules for RA and callee-saved GPRs; missing rows remain unknown to strict readers. |
| `llvm/lib/Target/Mips/Mips64InstrInfo.td` | Revision 13: Define static pre-R6 N64 direct/indirect TailCC transfer pseudos with delay slots and signed stack deltas. |
| `llvm/lib/Target/Mips/MipsDelaySlotFiller.cpp` | Revision 13: Treat CFI instructions as search barriers so delay-slot filling cannot move executable state changes across their unwind row. |
| `llvm/lib/Target/Mips/MipsFrameLowering.cpp` | Revision 13: Use stable normal TailCC stack-call frames, subtract callee-popped call bytes and disable shrink wrapping for the private ABI. |
| `llvm/lib/Target/Mips/MipsFrameLowering.h` | Revision 13: Declare the function-aware shrink-wrapping policy implemented in MipsFrameLowering.cpp. |
| `llvm/lib/Target/Mips/MipsISelLowering.h` | Revision 13: Use signed stack-argument offsets for growing/shrinking TailCC argument areas. |
| `llvm/lib/Target/Mips/MipsInstrInfo.td` | Revision 13: Define the MIPS TailReturn DAG node and its signed N64 stack delta. |
| `llvm/lib/Target/Mips/MipsMachineFunction.h` | Revision 13: Record aligned incoming callee-pop bytes and ordinary TailCC stack calls. |
| `llvm/lib/Target/Mips/MipsSEFrameLowering.cpp` | Revision 13: Publish actual asynchronous N64 spill/restore rows and callee-pop CFA offsets; restore the frame and tail argument-area delta together. |
| `llvm/lib/Target/Mips/MipsScheduleGeneric.td` | Revision 13: Give the three N64 TailCC transfer pseudos the existing generic jump scheduling class. |

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

Revision `-uwvm-ros.7` adds `SystemZ/load-trap-motion.ll`, bringing the inventory
to twenty-two files / fifty RUN lines. It reproduces a real register-ring
interpreter failure and an aliasing store, and preserves adjacent load/trap
fusion. The machine verifier and a separate 999-iteration QEMU execution oracle
pass with the isolated repair; a full `.7` package build is checked separately.

Revision `-uwvm-ros.8` adds `RISCV/rtdyld-call-range.s`, its `.check` file, and
`RISCV/Inputs/rtdyld-call-range.cpp`, bringing the inventory to twenty-five
files / fifty-two RUN lines. The standalone relocation fixture checks both
rounded range edges, addends, direct-call bypass, tail-call register preservation,
full-width literals, stub reuse/alignment, remapped section addresses and RV32
wrapping; disabling stubs makes an out-of-range RV64 call fail explicitly.
Actual RV64 QEMU symbols and typed landingpad probes additionally qualify
exception propagation and cached relocations with the default MCJIT code model.
This downstream loader repair is not claimed accepted upstream or included in
the original release. A `.7` archive with only the affected objects replaced
provided these diagnostic results; a complete `.8` package build is separate.
The ordinary repository supplies the same
[patch and implementation rationale](../../documents/toolchain/riscv-rtdyld-call-range.md).

## Packaging and build changes, not upstream backend fixes

- Unrelated top-level projects and bulk documentation/tests/examples were
  omitted as listed in README.md; `.git` and Git ignore/attribute metadata
  were not imported. Required libc shared math headers remain.
- Three internal mlgo utility symlinks were materialized with identical target
  bytes for Windows checkouts. Vendor line-ending conversion is disabled.
- ROS xmake builds Release static archives with suffix `-uwvm-ros.13` and
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
manifest, and the explicit compiler/config, host-build-policy and twenty-five-test-addition allowlist. It extracts no
archive paths. Update the allowlist and this document together when changing
the release or downstream patch; do not merely regenerate the manifest.

Revision `-uwvm-ros.9` adds the downstream RISC-V TailCC ABI and musttail stack
repair. The standalone, executed regression suite is retained in
`test/0014.llvm_jit/riscv_tail_abi/` at the product root, outside this pruned
upstream test inventory. These changes are not claimed to be upstream release
fixes or part of the earlier MIPS pull request. The ordinary product receives
ordered patches `llvm-riscv-musttail-stack-arguments.patch` and
`llvm-riscv-tailcc.patch`; see the paired ABI document for qualification limits.

Revision `-uwvm-ros.10` adds the downstream actual text endpoint producer.
Ordinary uwvm2 receives `llvm23-native-owner-table-v2.patch` against the exact
LLVM23 base. The opt-in table and owned record decoder are DATA, not a substitute
for authenticated loaded relocations, complete alias/unknown claims, the sealed
compiler plan or the current runtime cursor/generation. Real object emission,
full package rebuild and each platform's native boundary remain separate checks.
No earlier `.9` result is relabeled `.10`; all historical evidence is retained.

Revision `-uwvm-ros.11` completes the PPC32 call/PC-relative EH relocations and
SPARC V9 static abs64 relocation path. Full-width tail stubs use ABI scratch
registers, preserve LR/o7 and the existing native stack/register window, and
remain outside authenticated Wasm instruction bodies. Same-section calls are
resolved directly only when their displacement is provably aligned and in
range; external and cross-section calls defer their stub displacement until
final load-address mapping. Overflow/unsupported relocation checks survive
Release builds. Runtime cache identities and current build recipes change to
`.11`; no earlier product execution result qualifies the new loader.

Ordinary UWVM receives the ordered `llvm23-linux-ppc32-sparcv9-mcjit.patch`
after the existing RuntimeDyld repairs against the revision-10 source shape.
The SDK's generated capability macros gate its two added native targets.
`test/0017.runtime/native_mcjit_elf_loader.cc` is a real MCJIT call/data/ABI
execution oracle; `debug_native_linux_runtime.cc` remains the separate actual
Wasm compiler/owner/issuer/controller and numeric-projection witness. A loader
component result cannot substitute for the latter or for a full `-Rdbg` CLI.

The consumer also requires the compiled RuntimeDyld capability query symbol.
A stale `.10` or stock archive with revision-11 headers fails at link time,
before Wasm compilation, rather than relying on the new header macros alone.

The revision-11 native-debug qualification also requires exact static C `musttail`
lowering and the independent DWARF reader's V9 PC-relative EH relocation.
`test/0017.runtime/native_mcjit_musttail.cc` exercises real direct/indirect
MCJIT tails, cross-section targets, exact numeric results, and a private
bounded-stack witness over 4096 recursions. PPC32 includes C stack arguments;
V9 keeps this oracle within the qualified register-only argument shape.
This does not claim TailCC or arbitrary differing-prototype support on these
backends. `native_owner_mapping_metadata.cc` separately checks that the actual
Wasm JIT object retains valid pure-origin rows and typed numeric variables.
Executed product qualification remains separate from these component checks.

The ordered `llvm23-linux-mips64-mandatory-tail.patch` preserves N64 mandatory
C tails independently of the optional sibling-call switch. The ordered
`llvm23-linux-ppc64-toc-musttail.patch` follows the PPC32/SPARC loader patch and
adds the separate private PPC64 TOC contract. Its query is defined in
`PPCISelLowering.cpp`, so a newly installed declaration cannot qualify a stale
PowerPC CodeGen archive. ELFv1 descriptor environment inputs follow the fixed
TC_RETURN callee/delta operands; both ELF ABIs keep the original caller's TOC
save slot across a tail chain. Exact-prototype stack arguments are committed
only after incoming reads. Capability revision 2 adds the FastISel normal-call
fallback for exact opt-in callers/callees at O0. Revision 3 additionally uses
a TOC address slot for marked medium-code-model callees: the JIT loader can
resolve that slot without unsupported cross-section TOC16 arithmetic. Generic
LLVM TOC guards remain in force. Revision 4 also makes the assembler printer
use that same TOC slot for the high half of medium-model indirect loads.
ELFv1 descriptor targets otherwise paired a direct descriptor high half with
a TOC-slot low half, an inconsistent address hidden by the old loader bug.
The actual CodeGen query must report revision 4.

`llvm23-linux-systemz-dwarf-pcrel.patch` follows the V9 DWARF reader change and
adds SystemZ PC32/PC64 section-relative relocation handling. No runtime memory
read or public native capability is added by either DWARF reader fix.

All native numeric register witness targets require their actual assembler
parser in the static contract as well as runtime registration. Full MIPS static
consumers compile with `-mxgot` to avoid the signed 16-bit GOT window. These
consumer/link changes do not change the upstream source edit count. Fresh
loader, mandatory-tail, genuine Wasm trap/register, shutdown and CLI execution
are required for each exact source/SDK cut; no previous failed receipt or
backend-only result is converted into product qualification.

The ordered llvm23-linux-ppc64-medium-toc-relocations.patch follows the
PPC64 TOC-tail repair. Medium-model floating/vector constant pools can reside
in .rodata, outside .toc. The loader retains both section identities and
subtracts their final load addresses when applying TOC16 relocations; it also
recomputes the displacement if either section is remapped before final resolution. The old
same-section cancellation silently loaded unrelated bytes in release builds.
Whole TOC16 offsets and DS alignment are checked, and unresolved external TOC
references fail rather than entering the former unreachable path. A separate
compiled loader capability query is required alongside the CodeGen TOC-tail
query. Both UWVM consumers require it before native PPC64 execution.
native_mcjit_numeric_execution.cc checks actual f32/f64 constant-pool results
both directly and across a host call using the production medium code model.
This does not grant public code, stack, register, or native-memory authority.

## Linux MIPS64EL Clang 23 host scheduler build

A real N64 Linux MCJIT product compiled with Clang 23 at C++ `-O1` crashes in
`ScheduleDAGInstrs::addPhysRegDeps` while accessing a past-end `SparseMultiSet`
iterator, before executing Wasm. Compiling only `ScheduleDAGInstrs.cpp` with
`-O0` and without the optimized precompiled header passes the same actual
Wasm JIT numeric, large-i64 and real SI/NI checks at the original JIT O3 policy.
`llvm/lib/CodeGen/CMakeLists.txt` confines this host build workaround to Clang
23, Linux MIPS64 little-endian N64. It does not lower the Wasm optimization
policy, skip a target oracle, or grant debugger access to native stack/memory.
The original failing receipt remains evidence; this is a qualified build
workaround, without a claim about the compiler defect's deeper cause.


## Windows compiler-helper link probes

The separate host build policy in `llvm/cmake/config-ix.cmake` checks compiler
helper symbols with an executable link in a function-local CMake scope.
Archive-only `try_compile` can accept unresolved helper references; the probes
therefore use distinct `UWVM_LINK_*` cache keys and return only actual link
results. The caller's cross-toolchain policy is preserved, and no target
executable is run. This documents an existing concurrent change without
altering its implementation. Linux source-provenance verification covers its
retained bytes; it does not qualify Windows native execution.


## LoongArch asynchronous epilogue CFI

`llvm23-loongarch-async-epilogue-cfi.patch` updates
`llvm/lib/Target/LoongArch/LoongArchFrameLowering.cpp`. Async unwind rows
switch from FP to the current SP before saved-register restores, describe each
actual restored register, and publish CFA=SP after deallocation. The old row
retained the restored caller FP and did not describe a stopped return. C ABI
selection and synchronous tables are unchanged. The paired registered-CFI
fixture checks actual O0/O3 return rows and executes real 64-argument native
functions; live Wasm caller/finish qualification is recorded separately.
The exact inventory hash identifies this downstream source update.

The LoongArch repair also updates MCTargetDesc/LoongArchMCTargetDesc.cpp to
encode the C ABI initial same-value rules for RA and GPR r22..r31 explicitly
in each CIE. Strict readers continue to treat absent/unsupported rules as
unknown. Only actual emitted spill/restore rows can override these rules.

## Revision 12: X86 guaranteed-tail aligned frames

The caller rounds stack arguments with CCInfo.getAlignedCallFrameSize() before
GetAlignedArgumentStackSize(). The callee previously used the raw stack size
for its pop count. With 64 i128 reference parameters and a final i32 seed, the
raw size is 984 bytes while the outgoing tail frame is 1000 bytes. A same-signature
tail call consequently relocates its return slot by -16 bytes and returns with
another 16-byte accounting mismatch. Actual native Wasm tracing found the first
caller reference corrupted before any throw/catch operation; collector changes
cannot repair this calling convention error.

The patch makes only the incoming shouldGuaranteeTCO branch use the same
aligned call-frame calculation. No parameter offsets, ordinary C/vararg rules,
other target code, or collector algorithm is changed. This also covers Fast/GHC
when guaranteed TCO is enabled and SwiftTail, so independent ABI regressions
must cover those conventions before broad backend qualification is claimed.
The .12 source manifest, generated capability, cache identity and actual static
archive receipts are separate from immutable .11 evidence. A single-TU archive
overlay preserves all other base SDK members and does not claim a full SDK
rebuild or all-platform native validation.


## Revision 13: packaged LoongArch/MIPS ABI and debug CFI repairs

The audited inventory includes the existing LoongArch and MIPS TailCC patches,
LoongArch initial CIE and epilogue rows, MIPS asynchronous frame/delay-slot rows,
and the full-width debug long-branch repair. The ABI patches and component
qualification limits are described in the paired toolchain documents.
The previously unrecorded source differences were checked against patch
postimages and the pinned official archive before updating sources.sha256.
The provenance allowlist now contains exactly 55 compiler inputs, one exported
configuration template, two host build policies and 25 added regression files.

Version/header/CMake/qualification contracts use -uwvm-ros.13 together.
The revision change gives the bundled SDK and native-object cache a new identity.
No version guard, source check or system-LLVM fallback is relaxed. Historical
component receipts remain immutable and do not claim a complete revision 13 SDK
rebuild or execution on every ISA.
