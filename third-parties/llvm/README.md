# ROS pinned LLVM

ROS uses **LLVM 23.1.1**, the latest official non-prerelease at the time of
this import (2026-09-16), plus the explicitly recorded downstream backend repairs.
This is **not trunk**, a nightly, an RC, or a system LLVM installation.
The exact [downstream changes relative to the official release](UPSTREAM_CHANGES.md)
are listed separately, including fifty-five compiler edits, one exported configuration header template,
two host build policies (Linux MIPS64EL Clang 23 scheduling and Windows helper linking), and twenty-five regression files.
See the [maintenance rationale and upgrade checklist](../../documents/toolchain/ros-llvm-maintenance.md)
before relaxing a version/build guard, changing cache identity or removing a workaround.

* Release: https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.1
* Release date: 2026-09-08.
* Tag commit: `6dfe1677ab8dffbc6ec13d53a1e0215d75147689`.
* Official source: `llvm-project-23.1.1.src.tar.xz` from that release's assets.
* SHA-256: `ebe9be46fe8756d58c5b198ffad0fa2a766257add81a4dc52179bfacc7888ee6`.
* The detached signature was verified using the keys published at
  https://releases.llvm.org/release-keys.asc. Signing fingerprint:
  `D574BD5D1D0E98895E3BF90044F2485E45D59042` (Tobias Hieta).
  The original signature, attestation bundle and published keys are retained
  in `provenance/`. The locally imported key has no personal web-of-trust
  certification; verification establishes agreement with the published key.

## Retained sources and repair

Retained: `llvm/` core headers/libraries/tools/build utilities, shared `cmake/`,
`third-party/` build dependencies and their licenses, and the root license.
LLVM 23 also requires the libc shared math/support headers for APFloat; the
`libc/{shared,src/__support,hdr,include}` subset and license are retained without
building or replacing the operating system's libc.
Three internal `llvm/utils/mlgo-utils` script symlinks are materialized with
identical target bytes so Windows checkouts do not depend on symlink privileges.
The repository `.gitattributes` disables newline conversion for this vendor tree.
Omitted: all other top-level projects (Clang, LLDB, LLD, MLIR, Flang, runtimes,
etc.), LLVM documentation/examples/benchmarks/bindings/unit tests and the large
upstream test suite. Git attributes/ignore metadata are also omitted. Twenty-five downstream
regression files remain under `llvm/test/CodeGen/{Mips,VE,AVR,MSP430,AArch64,RISCV,SystemZ}/`.
No `.git` repository or submodule is embedded.
Do not remove `.td`, `.inc`, `.def`, Python or CMake files by extension: these
are required source/build inputs, not disposable generated artifacts.

Applied repair: https://github.com/llvm/llvm-project/pull/223905,
commit `a830ec0e085d364e72a5126a9f866bae5b0fb09c`.
`MipsSEInstrInfo.cpp` handles non-microMIPS R6 GPR32/FGR64 condition copies
without unreliable nearby-instruction searches. `MipsRegisterInfo.td` gives
`FGR64CC` 64-bit spill slots, preserving full doubles and enabling the correct
spill/reload instructions. The patch does not change pre-R6/microMIPS paths.
The matching patch and detailed rationale also live in
`documents/toolchain/patches/llvm-mips-r6-condition-registers.patch` and
`documents/toolchain/mips-r6-llvm-backend.md` (paths from repository root).

`sources.sha256` identifies every retained source after this repair. Regenerate
it only as part of an audited source/patch update, not to bypass a build error.
The upstream archive is intentionally not duplicated in the repository.

## Automatic xmake build

When LLVM execution is enabled, the first `xmake f` builds the bundled LLVM
dependency with CMake >= 3.20 and Ninja. Subsequent invocations reuse Ninja's
incremental build. `--execution-jit=none` does not need or build LLVM.
`LLVM_CONFIG`, PATH's llvm-config, and installed LLVM packages are never used
as dependency fallbacks. No llvm-config executable is built or queried, even
from the bundled tree: CMake's file API provides include paths, definitions and
the complete ordered static-library dependency list. The metadata-only CMake
target is never compiled or executed. An installed C/C++ compiler is still needed to
bootstrap this source; vendoring LLVM is not vendoring the entire host SDK.
In particular, the MIPS patch repairs the LLVM dependency that generates Wasm
code; it does not patch an installed Clang compiling ROS/uwvm-int or LLVM itself.
A native MIPS R6 bootstrap must independently use a compiler with the repair
or another validated compiler. Do not infer bootstrap correctness from the
cross-generated MIPS regression results.
Named-module builds also probe the bootstrap Clang's actual BMI-to-IR output:
some development snapshots drop non-inline internal-variable initialization
([upstream fix #218304](https://github.com/llvm/llvm-project/pull/218304)).
Seeing an exported variable initialize, or selecting a newer major version,
does not detect that failure. ROS rejects a failing compiler before its module
objects can be used; select a passing bootstrap or `--use-cxx-module=n`.
This probe does not execute a target program and is not a guarantee against
other compiler bugs. It neither downgrades the bundled LLVM dependency nor
applies a Clang patch to this pruned LLVM-library source tree.
CI installs CMake/Ninja explicitly and no longer creates llvm-config links or
queries it to locate a bootstrap SDK. The backend job deliberately supplies an
invalid `LLVM_CONFIG` and runs the source/metadata guard tests.

Build outputs are under `<builddir>/bundled-llvm/<configuration hash>/`.
The hash distinguishes source/patch content, compiler binaries, ABI-affecting
flags, target backends and cross configuration. LLVM archives are linked by
absolute path even when the application's system libraries remain dynamic;
runtime loader search paths cannot replace libLLVM with another installation.
The version suffix `-uwvm-ros.13` separates the native-object cache from
unpatched upstream LLVM and earlier downstream revisions. Revision 2 added
VE/AVR/MSP430 repairs; revision 3 fixes FP-predicate inversion and MIPS R6
strict comparison selection/exception preservation. Otherwise an old cache
could replay a wrong SIMD mask without running the corrected compiler.
Revision 4 adds compressed-MIPS strict FP, call-frame and encoding repairs,
including MC mappings used after instruction selection. The repair does not
complete MIPS16's missing direct-object/pseudo-expansion implementation, and a
successful assembly listing must not be treated as a native MCJIT-host claim.
Revision 5 fixes AArch64 RuntimeDyld stub byte order and full-width PC-relative
relocations. Loader repairs need a new identity too: previously cached objects
are not evidence that their old relocated code was correct. This LLVM package
revision is independent of the runtime cache's v5 file format.
Revision 6 resolves ELF local symbols by their actual symbol-table entries,
not a potentially duplicated name. RISC-V/LoongArch writers can emit multiple
local `.L0 ` labels; the old loader pointed earlier functions' unwind records
at the last function. This repair changes load-time symbol resolution, not the
generated hot-path instructions, and has before/after relocation and recursive
execution controls. The full `.6` package/CLI build remains a separate required
validation; `.5` build results must not be relabeled as `.6` results.
Revision 7 prevents SystemZ load-and-trap fusion from moving a load across a
store, call, ordering barrier or address-register definition; extends address
register liveness and retains memory operands. The real multi-memory interpreter
exposed a machine-verifier failure. A reduced regression also executes 999 alias
and cursor cases under QEMU, while adjacent loads still fuse to `lgat`. A fresh
`.7` package build remains required independently of the isolated patched tool.
Revision 8 fixes RV64 RuntimeDyld CALL/CALL_PLT overflow. Reachable calls remain
direct; distant calls use an aligned, full-width target stub in the same section.
Rounded AUIPC/JALR range checks remain active in Release builds and reject
unrepresentable calls when no stub is available. Tail-call return addresses and
RV32 XLEN wrapping are preserved. Native relocation tests and real RV64 QEMU
typed-EH/cached-object probes use the default MCJIT code model; the earlier Large
workaround qualification remains recorded. These tests rebuilt only affected
RuntimeDyld objects in a `.7` library overlay, so they do not establish that a
complete `.8` package/CLI has already been built. See
[the repair and evidence](../../documents/toolchain/riscv-rtdyld-call-range.md).
Revision 10 adds an opt-in AsmPrinter DATA section carrying actual text-start
and pre-trailer code-end relocations for all emitted MachineFunction definitions,
plus the exact assembler entry/local-entry names (v2; no guessed mangling).
The actual object symbols/aliases remain an independent conflict source. Neither
the section, an IR name nor a descriptive role grants native debug permission.
An absent module flag preserves the existing instruction/section emission. The
owned wire parser/object probe lives at the product root; a complete `.10`
package/CLI and loaded-owner/native-boundary qualification are still required.
These tests are not part of the pruned upstream test inventory.
Revision 12 makes X86 TailCC callee-pop use the same MaxStackArgAlign-rounded
call frame as the caller before return-address-slot alignment. The native
64-i128-reference plus scalar-seed regression exposed a 32-byte caller stack
drift with the old incoming/outgoing size formulas. The generated header exports
LLVM_UWVM_X86_TAILCC_ALIGNED_FRAME_FIXED; every runtime cache context records its
fixed/unfixed state. Provider qualification must include actual rebuilt archive
hashes, caller/callee stack evidence and native Wasm execution. A version label,
a successful source compile or earlier .11 evidence cannot replace these checks.
These revisions do not replace the official release base with trunk.
A source/version mismatch fails closed.
Source verification/Ninja checks run once per xmake process, not once per
registered test target; this in-memory shortcut is never persisted across runs.
A copied CMake build directory pointing at another source checkout is rejected:
checking this checkout's manifest cannot authenticate the other directory.
Use a fresh output directory (`xmake f -o <directory>`) instead of copying caches.
Unsafe host-compiler FP options such as `-ffast-math`, `-Ofast` or `/fp:fast`
are rejected, including resolved CMake flags. LLVM's APFloat uses shared libc
math helpers; compiling LLVM itself with assumptions that discard NaNs or signed
zero could corrupt folding before the runtime's floating-point guards execute.

LLVM is compiled in Release mode without LTO/debug data, with one concurrent
link job and two compile jobs by default. `--llvm-build-jobs=1..16` changes
compile parallelism, **not** a hard memory limit: use an external cgroup/job
limit on constrained builders. `--llvm-build-targets=Native` builds the native
backend; use `all` or e.g. `X86;Mips;AArch64` for code-generation audits.

Cross builds require `--llvm-cmake-toolchain=<file>` specifying the same target,
sysroot, compiler/ABI and linker as ROS, including an explicit `LLVM_HOST_TRIPLE`
for the machine on which ROS will run. For example, an AArch64 Linux toolchain
sets `LLVM_HOST_TRIPLE` to `aarch64-unknown-linux-gnu`; its compiler/sysroot/ABI
settings must agree. LLVM's config.guess otherwise identifies the build machine,
even when the selected compiler really produces AArch64 objects. xmake clears
old inferred host/default triple cache entries before reading the toolchain.
The chosen backend list must include that host's native backend for ROS JIT.
No target runner is needed to discover
dependencies; the former llvm-config runner option has been removed.
LLVM builds native TableGen tools from the bundled sources. A host LLVM must
never stand in for the cross-target dependency. No cross target is silently
treated as native.

Cross-generated object tests are not validation of this entire cross-library
build workflow. Complete cross-target LLVM library/CLI builds, Windows and
Darwin integration still require target-specific validation; see the recorded
coverage in [the ROS vendor audit](../../documents/toolchain/ros-bundled-llvm23.md).

An official stable release and these regressions reduce known risks; they do
not prove that LLVM or ROS is bug-free on every platform and configuration.

Revision 9 repairs RISC-V C musttail outgoing stack addresses and adds the
callee-popped TailCC ABI, including different parameter counts, genuine indirect
tail transfers, stack argument ownership, and DWARF CFA/exception restoration.
The independent RV64 QEMU candidate passed 23 execution cases and 28 verifier
cases, including large argument-register overflow and typed exception landing
pads. Genuine revision-9 package/VM qualification is tracked separately; these
results must not relabel an old revision-8 build. See
[the ABI repair and qualification scope](../../documents/toolchain/llvm-riscv-tail-abi.md).

Revision 11 adds static ELF32 PowerPC and ELF64 SPARC V9 MCJIT relocation
dispatch, full-address external/cross-section call stubs, and Release range
checks. SPARC V9 alone registers native JIT support; SPARC32/LE, Thumb,
compressed MIPS and unimplemented relocation/ABI combinations remain refused.
The generated `llvm-config.h` exports `LLVM_UWVM_ROS_ELF_PPC32_MCJIT` and
`LLVM_UWVM_ROS_ELF_SPARCV9_MCJIT`; ordinary UWVM accepts these two loaders only
from such a patched SDK. Earlier backend-only QEMU evidence is not full Wasm
JIT evidence. Fresh qualification of revision 11 is required before making
product/platform coverage claims.

The two generated capability macros additionally require the library-defined
`RuntimeDyld::getUWVMELFLoaderCapabilities()` ABI. Mixing patched headers with
unpatched archives fails to link; capability is never inferred solely from
headers or an LLVM version string.

The revision-11 native-debug qualification also requires exact static C `musttail`
lowering and the independent DWARF reader's V9/SystemZ PC-relative EH relocations.
The PPC64 TOC-tail capability query returns revision 3 only when the actual
CodeGen archive contains SelectionDAG lowering, the O0 FastISel normal-call
fallback, and medium-code-model TOC address slots for marked callees. These
slots avoid unsupported cross-section TOC16 address arithmetic in RuntimeDyld;
revision 1 and 2 archives cannot qualify the current contract.
`llvm23-linux-systemz-dwarf-pcrel.patch` follows the V9 reader update and keeps
valid SystemZ EH-frame PC-relative relocations from invalidating Wasm metadata.
`test/0017.runtime/native_mcjit_musttail.cc` exercises real direct/indirect
MCJIT tails, cross-section targets, exact numeric results, and a private
bounded-stack witness over 4096 recursions. PPC32 includes C stack arguments;
V9 keeps this oracle within the qualified register-only argument shape.
This does not claim TailCC or arbitrary differing-prototype support on these
backends. `native_owner_mapping_metadata.cc` separately checks that the actual
Wasm JIT object retains valid pure-origin rows and typed numeric variables.
Executed product qualification remains separate from these component checks.

Native debug also marks its actual MIPS functions with
`uwvm.native.debug.pcrel`. On static N64 pre-R6, the MIPS branch-expansion
pass changes local `J` edges to PC-relative `B` before the unchanged range
pass. RuntimeDyld otherwise sends even local jump edges through section-tail
trampolines outside the exact Wasm function. The debugger continues to refuse
those stubs; no section-wide code, register or stack permission is added.
Unmarked functions, calls, the static C ABI, MIPS16, microMIPS and R6 retain
their existing behavior. Ordinary uwvm2 must rebuild its external LLVM library
with the matching repair; headers or compiler flags cannot replace it. See
[build and actual-product checks](../../documents/toolchain/mips-native-debug-branches.md).

Revision 13 packages the documented LoongArch64 and static MIPS N64EL R2
callee-pop TailCC implementations, their asynchronous CFI prerequisites, and
MIPS debug long-branch containment repair. See the paired
[LoongArch ABI](../../documents/toolchain/llvm-loongarch-tail-abi.md) and
[MIPS ABI](../../documents/toolchain/llvm-mips-n64-tailcc.md) documents.
The source allowlist covers all 56 compiler/configuration inputs and two host
build policies against the exact official archive. Old SDKs and native objects
retain their original revision; a matching rebuilt dependency is required.
Native-only builds qualify their configured backend, not every retained ISA.
