Native registered CFI foundation
===============================

`debug_native_registered_cfi.cc` compiles and loads real LLVM MCJIT functions
through the production `runtime_llvm_jit_section_memory_manager`. The manager
observes the actual object ABI and parses the actual relocated EH registration
after the original unwinder registration. It retains integer rules, not borrowed
LLVM expressions or native metadata pointers. Deregistration invalidates queries.

The evaluators support SysV x86-64 and GNU RV64 register-plus-offset CFA and ordinary
same/register/CFA value or memory rules. It reads exclusively from an immutable
owned byte copy, with checked offsets and arithmetic. Missing rules remain
unknown. Expressions, alternate address spaces, unsupported ABIs and ambiguous
or mismatched FDE extents fail closed. Returned parse errors and caught exceptions leave the original unwind
registration intact. No IO occurs in observation or
evaluation; test diagnostics use FastIO.

Observation is gated by genuine LLVM 23 or later and the existing exact-1 native
owner-table build gate on non-Windows targets. This is currently qualified on
Linux x86-64, little-endian RV64/AArch64 LP64 and i386 through their separate
scoped records. Parsing uses at most a 1 MiB registration span, 16 MiB aggregate
input, 4096 FDEs and 65536 retained rows per manager. Temporary LLVM parse storage
is also subject to the native test cgroup supervisor.

The test accepts code-generation optimization levels 0 and 3, parsed with
FastIO `parse_by_scan`. It checks real symbol extents and CFI row transitions across generated
prologues, bodies and epilogues, executes the genuine generated caller, and
checks extent and retired-metadata rejection. A real emitted valid RA DWARF
expression remains usable for ordinary generated execution while the metadata
query refuses the affected instruction intervals. Evaluator cases use compiler-derived
rules with an owned test allocation. They do not authenticate a live native stack
or physical caller.

The separate [physical caller inspection integration](README.debug-native-physical-caller.md)
joins actual worker stack metadata and the real trap for a read-only single
Wasm caller query. It does not issue a return-stop or resume capability.

Before native `finish` can use this foundation, a runtime-private capability must
join the selected actual trap, immutable CFI owner, current Wasm publication and
generation, bounded stack copy from its registered guest thread, caller code
ownership and exact instruction boundary. Raw addresses, logical Wasm frames,
host custom DWARF and these metadata rows alone must never grant native reads.
These metadata-only queries never grant native `finish`; executable integration
must separately qualify its return-stop lifetime and worker acknowledgement.
This foundation adds no public stack, SP/FP, host register, VM code or memory display.

RV64 and CFA-only metadata on 2026-10-07
----------------------------------------

`debug_native_registered_cfi_riscv64.cc` loads genuine RV64 TailCC functions
with 64 i64 parameters through the production manager. Both repositories pass
O0 and O3 with the actual qualified target SDK. The emitted incoming argument
area is 416 bytes. Body rows may use SP or s0 as the CFA register. At the return
instruction the actual CFA is SP with offset zero: the argument area was
already counted in CFA and has now been popped. Adding 416 again would invent
the wrong parent SP. The tests reject wrong extents, end-PC queries and retired
metadata. They observe generated metadata without executing a public ASM step.

`copy_debug_native_cfa_row` returns owned scalar CFA metadata only, including
intervals where restored or expression-based RA has no supported retained rule.
The normal full-row/caller query still rejects those intervals. No same-RA rule,
known register or live-caller authority is fabricated to make a test pass.
The x86-64 registered-CFI test checks this separation at O0/O3 as well.

The separate RV64 owned-DATA evaluator tests validate sparse preserved-register
recovery, unavailable and high-numbered registers, missing/duplicate/out-of-range
slots, and checked CFA bounds. DATA tests read no live native memory. See
[RV64 physical finish](README.debug-riscv-finish-status.md) for the independent
actual worker/trap/stack/owner/return-event integration and executable tests.


i386 CFI and ABI-sized private stack slots on 2026-10-07
-------------------------------------------------------

[The paired i386 foundation record](native_i386_cfi_stack.qualification.json)
separates registered metadata, owned DATA and private live-stack reader tests.
The production observer accepts genuine little-endian i386 objects with four-byte
addresses and DWARF RA column8. CFA registers are limited to columns0..7. It
rejects addresses outside the 32-bit target range even when a 64-bit host emits
and relocates the object. Unsupported expressions still provide no RA/read
permission; CFA-only metadata remains a separate scalar query.

The independent i386 evaluator reads exact four-byte slots, recovers only
EBX/EBP/ESI/EDI and EIP, and derives ESP from CFA. Its ordinary return rule must
be `*(CFA-4)`. Unknown values, missing or overlapping slots, truncated bounds,
32-bit arithmetic overflow and values above UINT32_MAX refuse or remain unknown.
Compiler-derived rows across the actual TailCC prologue/body/epilogue are tested
at O0/O3 with 64 i32 parameters. A saved-register rule outside the supplied owned
frame does not justify reading that slot. The 64-bit host uses distinct 32-bit
RuntimeDyld target labels for the metadata test and executes no i386 code there.

The runtime-private stack registration also supports an exact i386 four-byte
reader. Wrong ABI widths refuse before copying. The existing eight-byte reader
continues to serve x86-64/RV64/AArch64. Only QEMU's ENOSYS permits the fixed
FastIO self-mem fallback. i386 pread64 passes the low and high offset arguments
separately; an actual pthread stack above INT32_MAX exercises this path under
QEMU. Wrong thread, retired scope, insufficient frame space and adjacent output
bytes are checked independently. These registered test-thread stack checks are
not Wasm-frame authentication and issue no public memory or resume capability.

Both repositories pass the recorded Linux cgroup foundation tests. At that
cut, i386 physical-caller/finish gates remained closed. Existing x86-64
generated-CFI execution and AArch64 CFI/private-slot regressions retain their
separate counts.

The later [full i386 integration](native_finish_i386.qualification.json) uses
the genuine ELF32 vendor SDK, actual kernel register mapping, authenticated
Wasm owners and exact four-byte live stack slots. It qualifies 52 actual Wasm
sessions and 28 physical finish return hops in both repositories. Actual
RET32/RETI32 operands supply the parent SP adjustment; a row or DATA allocation
alone still cannot authorize a live read or return. Public native stack bytes,
ESP/EBP/EFLAGS and VM/host frames remain unavailable. See
[usage and scope](README.debug-i386-finish-status.md).


ARM32 owned recovery and private stack slots on 2026-10-08
---------------------------------------------------------

The [ARM32 scoped status](README.debug-arm32-cfi-status.md) documents the new
four-byte owned-DATA evaluator and Linux ARM EABI little-endian private reader.
Shared row types now live in native_debug_cfi_types.h without LLVM ABI coupling.
The LLVM observer uses those same definitions. The subsequent ARM integration
accepts bounded same-object ELF32 .debug_frame metadata and rejects the lexical
suffix after an actual r11 restore, where LLVM TailCC rows otherwise retain
the caller's frame pointer. It preserves EHABI registration and grants no live
read. Exact native SDK, Wasm ownership and instruction-state qualification are
still required before physical ARM Wasm callers, native ni or finish can be
enabled. See the scoped status for the actual compiler-object test limits.
