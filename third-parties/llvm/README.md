# ROS pinned LLVM

ROS uses **LLVM 23.1.1**, the latest official non-prerelease at the time of
this import (2026-09-16), plus the explicitly recorded downstream backend repairs.
This is **not trunk**, a nightly, an RC, or a system LLVM installation.
The exact [downstream changes relative to the official release](UPSTREAM_CHANGES.md)
are listed separately, including seventeen compiler edits and twenty-one regression files.
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
upstream test suite. Git attributes/ignore metadata are also omitted. Twenty-one downstream
regression files remain under `llvm/test/CodeGen/{Mips,VE,AVR,MSP430,AArch64,RISCV}/`.
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
The version suffix `-uwvm-ros.6` separates the native-object cache from
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
