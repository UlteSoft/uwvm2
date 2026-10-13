Native instruction semantics source candidate (2026-10-03)

This is source-only until the sole Linux cgroup keeper records fresh execution.
The helper examines LLVM MCInstrDesc after a successful bounded MC decode. It
reports call, return, branch, trap, PC writes and descriptor branch properties.
It does not parse formatted assembly or convert an integer PC to a pointer.
No existing native trap/platform admission gate or normal LLVM-full hot path is
changed. The current standalone component does not by itself implement ni,
finish, return breakpoints or multi-owner native stepping.

The owned-byte fixture uses the actual native x86_64 or AArch64 LLVM target,
with direct/indirect calls, return, conditional/indirect branch, trap, ordinary
instruction, truncated input, SoftFail, missing definitions, forged opcode and
forged register. Run it against each actual product LLVM closure with fast_io
headers. x86_64 host OS is independent of the ELF display triple in this scalar
component; real Windows/macOS traps and loaded-object qualification remain
separate. Other architecture fixture exits 77 and is not a PASS.

A call flag alone is insufficient for an execution plan. Native ni must match
the actual stopped frame across nested calls/returns and owner generations;
finish needs the real caller frame and unwind lifetime. A conditional branch
flag cannot predict the taken edge. LLVM may encode tail calls as branches.

References: [LLVM MCInstrDesc](https://llvm.org/doxygen/classllvm_1_1MCInstrDesc.html),
[LLDB instruction plan](https://github.com/llvm/llvm-project/blob/main/lldb/source/Target/ThreadPlanStepInstruction.cpp),
[GDB machine-code commands](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Machine-Code.html).
