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

Remaining ARM integration
-------------------------

Actual ARM target SDK integration, every instruction's live unwind state,
physical Wasm callers, return-stop and native ni/finish remain unqualified.
Default .ARM.exidx cannot stand in for instruction-precise state. Thumb/
interworking, alternate ARM DWARF EH products, big-endian private reads,
VFP/NEON caller reconstruction and complete ARM Wasm debugger coverage remain
unqualified. Public interfaces retain their existing ARM caller/finish gate.
No host stack, raw SP/FP/LR, VM register/memory/code or new public inspection
interface is added by the private metadata path.
