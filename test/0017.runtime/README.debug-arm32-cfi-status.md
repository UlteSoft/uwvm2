ARM32 native debugger foundation (2026-10-08)
===========================================

The paired [qualification record](native_arm32_cfi_stack.qualification.json)
separates ARM32 owned-DATA recovery, runtime-private registered thread-stack
reads, existing CFI evaluator regressions and actual host MCJIT registration.
It does not qualify an ARM Wasm physical caller, native `ni` or `finish`.

`native_debug_cfi_arm.h` implements bounded and sparse ARM-state AAPCS32
recovery from owned integer row/slot data. Slots are exactly four bytes. The
interrupted SP may be word-aligned, while recovered caller CFA is eight-aligned.
All address arithmetic is checked against UINT32_MAX, including when tested
on a 64-bit host. It recovers only explicit r4-r8/r10/r11/LR rules and derives
SP from CFA. Unknown registers stay zero/unknown. Platform-specific r9, volatile
registers, PC, CPSR and FP/vector state are not inferred. Zero-sized leaves need
an actually known LR and read no stack slots. Zero, misaligned and Thumb-tagged
return values refuse without rounding or clearing state bits.

The bounds checks precede every reader callback. Missing, duplicate, overlapping,
misaligned or out-of-frame slots, oversized slot collections, unknown bases,
32-bit overflow and a PC rule substituted for LR are covered by the fixture.
The standalone owned row types and ARM evaluator contain no LLVM API, native
read or I/O dependency. The registered LLVM observer continues to include the
same row definitions. No provider ABI check is disabled by these new files.

The Linux ARM EABI little-endian private stack reader obtains bounds and TID
from its actual registered worker thread. It accepts only an exact four-byte
slot in [SP,CFA). QEMU's genuine process_vm_readv ENOSYS alone admits a FastIO
native_file opening of /proc/self/mem. ARM pread64 puts padding in r3 and its
64-bit offset in r4/r5; it does not reuse i386's argument layout. Tests execute
on actual ARM32 ELF images in QEMU, including a real pthread stack above
INT32_MAX. They reject wrong widths, wrong threads, retired registrations,
insufficient frame spans and reads crossing the supplied frame. Adjacent output
bytes stay untouched. Big-endian ARM/OABI private reads remain unavailable.

The source of the preserved-register and stack alignment rules is
[Arm AAPCS32](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst).
Public ASM commands still require separate Wasm domain, publication, generation,
worker-trap, code-owner and numeric-value provenance. A test-thread registration
or an owned row/slot label grants none of those permissions. No stack/SP/LR,
host pointer, VM memory or VM-code display is added by this foundation.

ARM debug-frame integration (2026-10-08)
----------------------------------------

The private production memory manager now observes real ARM ELF32 little-endian
.debug_frame through its owning MCJIT listener. FullDebug provenance makes LLVM
emit these debug-only rows while the actual ARM exception ABI remains EHABI.
It neither enables process-all-sections nor reads a loaded debug address. The
original bounded bytes are copied, and only R_ARM_ABS32 from the same object's
SHT_REL section is accepted. Every FDE needs one CIE-field reference to an
actual CIE boundary and one initial-location reference to loaded same-object
text. Undefined/external symbols, unsupported/duplicate/out-of-section fields,
DWARF64 and ranges beyond the exact original text section retire the private
observer. Metadata failure does not set VM finalization failure or change
EHABI registration. No borrowed LLVM object or row is retained.

Address arithmetic stays in the ELF32 range, including on the 64-bit test host.
Normalized rows retain only explicit compiler rules. Entry CFA=SP with no LR
rule stays CFA-only; no presumed same-LR rule is invented. An LR expression
cannot become a caller rule. Exact FDE extents are mandatory and manager
retirement clears queries. The existing sparse evaluator still recovers only
its allowed four-byte owned DATA and leaves platform r9 and PC unknown.

Actual O3 ARM TailCC exposed stale epilogue CFI: after pop {r11,lr}, LLVM retains
CFA=R11+8 through add sp,sp,#240 and bx lr. That r11 already belongs to the
caller. The observer therefore scans only bounded same-object ARM words and
refuses the lexical suffix after an LDM restore of r11. The exclusion applies
to both full and CFA-only queries. A conditional restore or later laid-out
block is conservatively unavailable. This is a known restored-frame guard,
not proof that all ARM instruction states or return patterns are qualified.
Metadata and code scans are separately bounded.

The paired actual compiler-object fixture covers AAPCS O0/O3 and optimized
SelectionDAG TailCC, seven mutations of real objects, missing/expressive LR,
exact extents, owned DATA recovery, r9/PC exclusion, restored-frame refusal and
retirement. Distinct 32-bit mapped section labels are metadata DATA; no ARM
machine code or Wasm program executes on the x86-64 host. The exact provider's
unpatched O0 ARM FastISel aborted on TailCC and remains recorded as failed.
The existing toolchain repair in documents/toolchain/
arm-native-debug-calling-convention.md must be present in the linked real SDK;
a host-target compiler metadata test cannot establish that qualification.

ARM A32 frame-boundary qualification (2026-10-08)
-----------------------------------------------

The [current frame-guard record](native_arm32_frame_guards.qualification.json)
qualifies the extended private guard, separate from full ARM VM integration.
Original ELF mapping symbols must be local STT_NOTYPE, zero-sized, in the same
text section. An exact $a region must cover the beginning of each function.
Thumb, missing mappings or an initial data region refuse. An interior $d/$t
marker ends the usable lexical prefix before that region, including subsequent
$a code. Literal bytes never enter the A32 word classifier. Only SP/r11 CFA
bases are retained for ARM.

The conservative A32 integer veto now includes single loads, MOV/arithmetic,
conditional writes, memory base writeback, multiply/DSP/media destination
fields, double-load implicit r11 and LDM lists. Only the first unconditional
MOV/ADD r11,sp in a straight prologue may install the frame. A second identical
installation also refuses the suffix. This overapproximation may make additional
instructions unavailable; it is not a complete ISA decoder or live-state proof.
Fifteen mutation encodings are independently assembled from symbolic instructions
and compared byte-for-byte with the object fixture's inputs inside the cgroup.

An earlier negative test reused another ObjectFile's LoadedObjectInfo. Because
LLVM binds this map to object identity, those assertions did not isolate CFI
parser refusal. Historical negative counts remain archived but are superseded
by this record. Each current mutation receives a fresh real RuntimeDyld loader
and its own section map. A pristine clone must first produce the exact r11 CFA;
each r11 mutation must still provide the pre-write row and refuse all subsequent
rows. Ten malformed/mapping mutations, fifteen r11 mutations, genuine assembler
$d boundaries, real TailCC restored-frame suffixes and retirement are checked.

Both repositories pass six final ARM compiler-object/owned-DATA runs: O0/O3
AAPCS and O3 SelectionDAG TailCC per repository. Those runs verify six fresh-loader
controls, 60 malformed/mapping objects, 90 stale-r11 objects, 20 literal/data
instruction positions and 12 restored-frame positions. Four existing x86-64/i386
registered-CFI regressions also pass against the same paired final header.

The original emitted objects are separately linked into real ARMhf Linux ELF
executables and run with QEMU cortex-a15 in the original 64 GiB cgroup. A genuine
C entry calls the 64-argument AAPCS/TailCC caller, which calls the leaf; TailCC
callees pop their actual 240-byte stack-argument area. All six executables each
complete 1,024 chains with result 64 and unchanged SP/r11, for 6,144 chains. The
literal test function is never executed. FastIO handles the fixture's file,
print and argument parsing I/O. This proves actual generated ARM ordinary-call
execution, not an ARM-native LLVM SDK, live Wasm frame, kernel stop, ni or finish.
No module compilation claim is made. Full evidence, failed/transitional runs,
source/tool/dependency hashes and PIDFD retirement are retained separately.

Real ARM native LLVM component qualification (2026-10-08)
--------------------------------------------------------

The [real ARM MCJIT record](native_arm32_real_mcjit.qualification.json)
supersedes the earlier lack of an ARM-host component SDK. The rebuilt Linux
ARMv7 A32 EABI hard-float LLVM 23.1.1 library executes on QEMU cortex-a15;
its actual ARM FastISel TailCC fallback and RuntimeDyld TARGET2 repair are
linked, not inferred from headers. O0 and O3 TailCC both pass. This plain
23.1.1 component SDK does not satisfy the current ROS .13 full-product
version/capability contract; that guard remains unchanged.

A genuine ARM EHABI/registered-CFI compilation exposed glibc link.h ELF macro
collisions with later LLVM ELF declarations. The paired registry header now
loads LLVM's actual ELFObjectFile declarations before link.h when LLVM JIT is
selected, including the default-selection global-fragment path. Platform ELF
macros remain intact; standalone registry users without a JIT selection do not
acquire an LLVM dependency. A header-first fixture compiles and runs in both
repositories with default LLVM selection. This checks the include ordering,
not compilation of a complete C++ module or a new ROS runtime mode.

Twenty final ARM executable runs pass across the two repositories:

* Real ARM-host MCJIT normal TailCC integer/void calls at O0 and optimized
  code generation use 64 distinct arguments and exact weighted numeric
  results. Sixteen C-entry call chains check unchanged private SP/r11 across
  the actual 240-byte callee-popped argument area. Six additional direct,
  indirect and cross-engine musttail chains each execute 4,096 steps.
* Real recursive EHABI unwinding reports three recursive JIT frames and one
  JIT root, excludes host PCs and refuses queries after engine drain/retirement.
  Two binaries with the lookup hook genuinely absent from dynamic exports
  refuse JIT execution. Merely omitting --export-dynamic was insufficient
  because lld can auto-export a symbol referenced by a shared library; the
  negative fixture localizes that hook with an actual linker version map.
* Actual TARGET2/GOT_PREL DATA relocations and the linked loader ABI query
  pass. Eight ARM-native compiler-object/owned-DATA CFI runs cover O0/O3
  AAPCS and O0/O3 TailCC. Eight fresh object-bound loader controls pass;
  80 malformed/mapping objects and 120 stale-r11 objects refuse.
* Two header-first/default-selection executables retain glibc ELF macros.

The native unwind fixture is compiled with asynchronous unwind tables, as
already required by the product build. Its private stack/frame witnesses and
owned-DATA observer create no public register, pointer or stack display.
All compilation, QEMU execution, ELF/dependency audit and retirement ran in
the original 64 GiB cgroup; final test peak group memory was 28,142,264,320
bytes and all memory.events counters stayed zero. Current local consumed
source files were separately matched against the frozen tested inputs.

The [retention record](native_arm32_real_mcjit.retention.json) binds the
80,794,376-byte cold archive to its exact hash and local acknowledgment.
Original LLVM source, the actual ARM library/generated inputs, runtime
library dependencies, failed/transitional runs and final evidence are
retained. Recomputable SDK .a/.o/.pch caches remain online; they are not
represented as cold-retained raw bytes. The [audit receipt](native_arm32_real_mcjit.audit_receipt.json)
records independent ELF/source/dependency checks and PIDFD retirement.

Remaining ARM integration
-------------------------

The exact current ROS .13 full-product ARM SDK, every instruction's live
unwind state, physical Wasm callers, return-stop and native ni/finish remain
unqualified. The ARM-native component SDK and calls above do not qualify
those complete Wasm runtime paths.
Default .ARM.exidx cannot stand in for instruction-precise state. Thumb/
interworking, alternate ARM DWARF EH products, big-endian private reads,
VFP/NEON caller reconstruction and complete ARM Wasm debugger coverage remain
unqualified. Public interfaces retain their existing ARM caller/finish gate.
No host stack, raw SP/FP/LR, VM register/memory/code or new public inspection
interface is added by the private metadata path.
