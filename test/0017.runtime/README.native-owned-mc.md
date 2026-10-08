This component exercises the real LLVM MC decoder using bounded owned bytes.
It executes no machine instruction and proves no OS trap adapter, Wasm runtime
authorization, NI command, call continuation, caller CFA, or native stack range.
Run it only through the existing Linux cgroup keeper or standing Windows guest
owner, against each product's actual LLVM headers and archive closure. Keep
EH/no-EH builds distinct; record exact source/header/archive pins and results.

The new decoder keeps the existing native disassembly platform gates. Its
target is the host's fixed triple; the caller cannot supply a triple, features,
symbolizer, live address, stack pointer or frame. `display_pc` is an integer
decode origin, never a dereference. A 15-byte x64 or 4-byte AArch64 local copy
feeds LLVM; failure and soft failure never yield usable decoded data. Exact
opcode/operand/register bounds are checked by the existing descriptor helper.

`decoded_instruction` has no public constructor from a classification. Its
read-only result distinguishes this conservative decoder from a bare MC table
classification. It is not an authentication capability: runtime-private stop,
participant, live owner, generation and epoch must still authorize execution.
Only ordinary results without descriptor control/terminator/unmodelled effects
or any target-specific MCInst prefix flags can report safe scalar selection.
The default object is unavailable. A caller must never promote an unavailable
result, guessed text, console numbers, or LLVM's `SoftFail` to a native step.

The fixture checks actual ordinary/call/indirect-call/return/conditional and
indirect branch/trap bytes, truncated/invalid/empty input, display-PC overflow,
and independent RAII context teardown. x64 additionally requires real REP,
INT3, SYSCALL, HLT, STI, MOV SS, POPFQ and IRETQ decodes to be refused for the ordinary NI
policy; a descriptor ambiguity is a failure, not a skip or synthetic pass.
AArch64 additionally rejects a real constrained indexed load with writeback to
its destination, for which the actual LLVM source returns `SoftFail`, and
rejects unaligned PCs. Actual x64 LLVM's decoder currently uses Fail/Success;
the existing `native_instruction_semantics.cc` separately checks generic
SoftFail rejection without claiming an actual x64 SoftFail instruction.

Keeper acceptance requires actual compilation/linking and component execution,
plus separate header/named-module compilation of the new partition. Future
controller integration needs a fresh complete runtime/main/host ABI build and
real selected-thread stop-to-step tests. Objects or QEMU user-mode components
cannot count as native Windows/Mac kernel qualification. Unsupported platforms
return 77 and stay unqualified. No local compile or execution was performed.

Primary references: [MCDisassembler contract](https://llvm.org/doxygen/classllvm_1_1MCDisassembler.html),
[MCInstrDesc semantics](https://llvm.org/doxygen/classllvm_1_1MCInstrDesc.html),
[LLVM's owning MC decoder construction](https://llvm.org/doxygen/Disassembler_8cpp_source.html).
These rolling docs may be newer than the product SDK; actual bundled LLVM 23
`MCContext.h`, `TargetRegistry.h`, `MCDisassembler.h`, `MCInstrDesc.cpp`,
`X86Disassembler.cpp` and `AArch64Disassembler.cpp` were also read before editing.
