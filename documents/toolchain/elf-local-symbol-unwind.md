# ELF local-symbol identity and native unwinding

## Cause and two product policies

An ELF relocation selects a **symbol-table entry**, not a globally unique name.
LLVM 22/23 RuntimeDyld looked up that entry's spelling in its global name map.
RISC-V and LoongArch object writers can give distinct local FDE-start labels
the same `.L0 ` name. Both relocations then select the last function, so the
earlier recursive function has no correct registered FDE. A successful object
load or a non-empty `.eh_frame` section does not prove correct unwinding.

The [ELF symbol-table specification](https://gabi.xinuos.com/elf/05-symtab.html)
describes symbol indices, local binding and section-relative values. The fix
uses the exact referenced entry for defined locals (including absolute and
section symbols), leaving global/weak and undefined-symbol resolution unchanged.

ROS repairs `RuntimeDyldELF.cpp` in its pinned official LLVM 23.1.1 source and
advances the downstream identity to `23.1.1-uwvm-ros.6`. This is not trunk and
not part of the earlier MIPS PR #223905. Its runtime ABI fingerprint also names
`llvm-elf-local-symbols=symbol-entry-identity-v1`.

Ordinary uwvm2 cannot assume its external LLVM is patched. On ELF RISC-V32,
RISC-V64 and LoongArch64 only, it sets `MCSaveTempLabels` before object emission,
retaining unique temporary names. Full materialization, lazy materialization
and the live unwind probe use this same policy. Its fingerprint names
`llvm-elf-local-symbols=unique-temporary-labels-v1`: a successful new probe must
not allow an older cached object with aliased FDEs. This key is independent of
git/source IDs and an external LLVM package's unchanged version string. The
runtime cache file format remains v5; product separation remains mandatory.

## Evidence from the September 17 regression run

Linux evidence root: `/tmp/uwvm-comprehensive.eUFnxH`. These directories are
independent snapshots; do not overwrite or relabel them as a later package.

| Evidence directory | Result and limit |
| --- | --- |
| `local-symbol-retained-01` | RISC-V32, RISC-V64 and LoongArch64: the old loader fails each first-FDE relocation check; the repaired loader passes each. Six RUN lines are retained as `RISCV/rtdyld-local-symbols.*`. |
| `remote-unwind-indirect-before-01` / `remote-unwind-indirect-candidate-01` | Same JIT-default recursive code, old/repaired loader. RISC-V64 changes from failed frame counts to 10/10 physical frames in normal, trap, guarded-memory fault and signaling-NaN conversion-trap modes. LoongArch64 normal recursion changes from 1/10 to 10/10; its signal cases remain failures. |
| `remote-unwind-unique-before-01` | Unique temporary names with the **old** loader recover the same RISC-V64 four cases and LoongArch64 normal recursion. No instruction-stack fallback is linked. |
| `unique-label-object-bytes-01` | All nine tested configurations retain identical bytes in every allocatable ELF section, including code/data/CFI. Symbol-table metadata can grow. This is not a throughput benchmark or proof about every module. |
| `ordinary-llvm22-native-target-06` | Exact production full/lazy selector bodies compile against external LLVM 22.1.8; 49 controls per path verify target selection, rejection and the target-specific naming option. Rejection controls are not execution support. |
| `native-i686-remote-unwind-01` | The same three IA32 generated-code bundles pass all four modes natively: 12/12. The corresponding QEMU signal cases fail. |

The fixture uses EngineBuilder's JIT defaults. An earlier forced-PIC matrix
(`remote-unwind-elf-matrix-01`) is retained as a separate diagnostic, not treated
as equivalent to native JIT code models. For example MIPS static/direct-symbol
calls and C-ABI indirect bridges differ; both controls remain recorded.

## What this does not establish

The remote tests execute generated code with the target's libgcc unwinder;
they are not complete foreign ROS/LLVM-library/bootstrap/signed-cache builds.
Their disposable signal handler uses diagnostic printing and libgcc calls;
it is not a production async-signal-safety guarantee.

`remote-unwind-linked-control-01`, `remote-unwind-gcc-control-01` and
`remote-unwind-signal-controls-01` distinguish dynamic loading from the ordinary
target linker, pure GCC code, static/dynamic unwinder linkage and alternate/
ordinary signal stacks. PPC64 and LoongArch signal failures reproduce in the
pure GCC controls too. Do not remove production stack protection or claim a
ROS LLVM fix solely from those QEMU failures. i686's native pass isolates an
emulation-path difference; it does not prove other machines pass.

Direct-symbol RISC-V controls additionally exposed unsupported `LO12_S` and
incorrect PIC GOT handling in the old loader. The indirect-bridge fixture does
not repair or erase those findings; actual production use needs separate
reproduction before widening a loader fix.

At this update, `.5` headers/LLVM, all seven header functional/assembly groups
and its 43 retained regression steps completed, but its named-module build was
still running. The `.6` private repair and source
guards do **not** replace a fresh packaged `.6` build and full regression run.
No all-platform or bug-free claim is made.

## MIPS call-range/PIC-ABI follow-up (separate from local symbol names)

The direct-symbol MIPS64 control emits `JAL` with `R_MIPS_26`. RuntimeDyld
**does create far-call stubs**; an earlier explanation incorrectly inferred
their absence from the final 26-bit relocation evaluator alone. Disassembly of
the relocated bytes confirms a full-address stub which jumps through **`at`**,
not **`t9`**. That violates a PIC host callee's `t9`/GP setup. The indirect
callback control succeeds instead. O32/N32 use the same defective register
convention, but non-PIC host functions can hide it. Consequently:

* Both products' native MIPS selectors append `+noabicalls,+long-calls`, including
  ordinary full/lazy materialization and the live probe. In MCJIT's static
  relocation model, O32/N32 ignore `long-calls` when ABICalls remains enabled;
  static N64 already implies noabicalls. A host feature list cannot override
  this required ABI policy. The final feature string is used for generated
  functions and cache context. This also avoids short-call region assumptions.
* The POSIX probe explicitly receives and forwards a C-ABI callback pointer,
  with an opaque context, instead of registering an externally named direct
  callback. This probe-only change does not add work to Wasm execution.
* Both cache ABI fingerprints include
  `llvm-mips-call-relocations=full-width-noabicalls-c-abi-v2`; v1 objects remain
  incompatible even if an embedder reuses its source ID, since v1's long-calls
  alone was ignored for O32/N32. The cache container remains format v5.

`mips-long-call-control-01` passes all four modes for both big-/little-endian
N64 (8/8), using direct named symbols again. `mips-long-call-assembly-01`
confirms the three unsafe `R_MIPS_26` call relocations are replaced with three
register calls. This has a real code-size cost: the two fixture functions grow
from 284/28 to 332/52 bytes, **24 extra bytes per call site** on these N64
profiles. The earlier nine-way byte-equality result applies to the local-label
workaround only, not this separate call-range repair. No unchanged-performance
claim is made; safe near-call optimization would need a proven code-placement
and relocation-range contract, not an assumption about typical addresses.

`ordinary-llvm22-native-target-07` passes 49 controls for each ordinary path;
`native-target-ros-10` passes 49 ROS controls, now checking the MIPS long-call
feature too. These are selector/encoder checks, not every ABI's execution test.
`posix-probe-callback-01` compiles the actual shared probe fragment with ordinary
headers and the completed LLVM `.5` libraries, then passes 20 native x86_64
lifetimes (three recursive frames plus root each). It is not a fresh ROS `.6`
integration build. At that intermediate snapshot, N32/O32/R6/compressed
variants had not yet received execution coverage for the new call policy.

The subsequent `mips-pic-call-policy-01` paired run closes the normal O32/N32/R6
PIC-callee gap: all **six** O32/N32 configurations fail normal callback capture
with long-calls alone and pass with noabicalls added. The corrected eight-profile
matrix (O32/N32/N64, both endian variants and available R6 variants) passes all
**40** modes: normal recursion, trap, guarded-access fault, sNaN conversion trap
and qNaN conversion trap. The host executable retains fixed link addresses for
remote relocation, but its function bodies are compiled `-fPIC`. N64 remains
passing in both policy variants. This is generated-code/target-libgcc execution,
not a complete foreign ROS CLI or a native-only performance benchmark.

The native-NaN fixture was corrected too: legacy MIPS reverses the quiet-bit
convention, so the original `0x7ff0000000000001` was a **qNaN**, not an sNaN
there. `mips-long-call-nan-kinds-02` separately records both bit patterns. This
test-only choice must not alter WebAssembly's fixed NaN bit encoding.

`mips-noabicalls-assembly-03` confirms removal of all short call relocations in
the eight standard-ISA profiles. Relative to the original object, entry and
recursive function sizes grow by 8/16 bytes for O32/N32 R2, 8/4 for the tested
R6 variants, and 24/48 for N64. These are function-body sizes, not total loaded
stub allocation or elapsed time. Compressed microMIPS/MIPS16 execution is still
unverified; the earlier partial assembly inventory is retained as a failure,
not silently included in the eight passing profiles. Native selection now
rejects effective microMIPS/MIPS16 modes before code emission: RuntimeDyld has
no corresponding compressed-encoding relocation implementation. It does not
silently substitute standard instructions on a possibly compressed-only CPU.
The offline inventory remains unfiltered; this is a safety boundary, **not**
implementation or certification of compressed native-JIT support.

`ordinary-llvm22-native-target-09` (full and lazy) and `native-target-ros-12`
each pass 56 selector controls, including explicit O32/N32/N64 and compressed
mode rejection. These tests compile the exact production selector bodies,
not whole current-product CLIs. Their 168 successes must not be added to an
architecture execution count.

The cache test fixture also stopped printing a binary ABI fingerprint as raw
line-oriented text: a length prefix may contain a newline and silently truncate
later fields. It now uses hex and verifies the complete key. The completed
`cache-local-symbol-policy-03` run passed 20 product/path/signature/version
controls for the local-symbol policy; the subsequently added MIPS policy must
be verified with a fresh fixture build.

Later completed snapshots: `cache-local-symbol-policy-04` passes all 20 controls
with **both** new policy fields; `release6-prebuild-03` passes all nine provenance,
configuration, bootstrap and exact-version-header drivers for frozen source
manifest `484631b7646a0b8b6cee08402d79956759f1578dd4d8d104200dd4649fdae19a`.
These still do not establish a production `.6` build. See the later module-build
interruption below; this intermediate snapshot is not a completed module test.

Evidence is retained on Linux and copied to the local audit directory:
`ros-local-symbol-regressions-20260917.tar.gz` (SHA-256
`60527ada4774b90e47296ad2783a0cb624cef4d7a3aa16b6b92c33fe5b4f3626`),
and the supplement `ros-call-policy-regressions-20260917.tar.gz` (SHA-256
`c5f8f6f55a7d5effee68ec02a9b71e4e33a365df4c21dcd1c21503bd65b68aa0`).
These bundles contain logs, manifests and small fixtures, not LLVM/SDK backups.

## Latest build/test state after the PIC-host controls

`cache-local-symbol-policy-06` passes **24** separately compiled ordinary/ROS
checks. The additional four cover signed/unsigned v1-to-v2 call-policy changes:
the cache already changes the filename, and forcing an otherwise valid signed
old blob into the new filename is independently rejected as a context mismatch.
Attempt 05 exposed a mistaken same-filename assumption in the test, not a
runtime cache-isolation failure; its logs remain retained.

Frozen `.6` runtime snapshot `release6-prebuild-04` passes all nine guards
(source SHA-256 `72299c3e9c17b576710026b06b8968fd114e4a6dd3f207a99d62fd8d17337868`).
The corrected standalone cache test was then compiled against that unchanged
runtime. Neither guard nor cache results imply a completed production build.

The `.5` named-module attempt was terminated by **systemd-oomd**, with reported
unit memory peak 6.1 GiB, while the campaign's aggregate limit stayed 32 GiB.
Its configure-only result is incomplete: no final binary/source verification
exists. Kernel cgroup OOM counters did not increase, but that does not mean no
memory-pressure termination occurred. A resumed attempt was deliberately stopped
to move the full-build campaign to the current `.6` source. Dependent `.5` module
functional tests never ran; the earlier seven passing groups are **header CLI**
results only. Fourteen superseded selector executables (1,877,411,032 bytes) were
removed from tmpfs, retaining logs, source fragments and rebuild commands.

The final3 snapshot additionally includes the corrected cache fixture;
`release6-prebuild-05` passes nine guards with source SHA-256
`4ae092ff46781c763897bef46c5fb4f87a9826f28f0e73d21012051386580afd`.
Its fresh `.6` LLVM/header CLI build is now running, with a dependent finite
codegen and seven-group header-functional batch. No `.6` module or performance
completion is claimed. The abandoned `.5` BMIs/objects (8,843,742,058 logical
bytes, 2,404 files) were removed after verifying its preserved header CLI and
stopped units; recovery is by rebuilding the retained frozen source.

The PIC-call evidence, including failed controls and the oomd journal, is
retained in `ros-mips-pic-regressions-20260917.tar.gz` (SHA-256
`0705a82b44d9dbd0cf96b9f2fdba5d7b258f88c3489e39adea3160dd378f228c`).
This is an evidence archive, not a backup of the retired compiler outputs.
