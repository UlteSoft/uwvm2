# RISC-V tail-call ABI repair

The paired LLVM 23 `.8` baseline has an independently reproduced miscompilation in
same-prototype C `musttail` calls with stack arguments and a local frame. Outgoing
arguments were stored relative to the current SP, inside callee-saved spill slots.
After the epilogue restored SP, the tail target still read the old incoming arguments.
A 12-integer permutation returned 710 instead of 364; a stronger host observer exposed
saved-register corruption as an invalid free. This is a backend correctness failure,
not a reason to lower mandatory tail calls to ordinary calls.

`llvm-riscv-musttail-stack-arguments.patch` changes the three outgoing stack-store
paths to incoming fixed-frame objects for tail calls. A stack-argument token factor
finishes incoming reads before overlapping writes. Ordinary calls retain their existing
SP-relative addressing. In the required SSH Linux cgroup, a separately built RV64 LLVM
provider executed the failing case through QEMU and returned 364 with normal destruction.
Six C/Fast controls and 23 positive/negative IR verifier checks passed. C12 ordinary calls,
FastCC computed-v128 ordinary calls, C2 tails, and FastCC v128 permutations retain identical
disassembly. A frameless C12 tail still passes but its load order/register allocation changes.
Full logs and immutable provider hashes are owned by the independent qualification runner;
this paragraph does not qualify the subsequent TailCC extension.

## TailCC candidate under qualification

Apply `llvm-riscv-tailcc.patch` after the stack-argument patch. The ROS vendor carries
both changes; the ordinary product receives these downstream patches. The product's RISC-V
TailCC selection and dependency version are not opened merely because the patch exists.
LLVM's [call instruction contract](https://llvm.org/docs/LangRef.html#call-instruction)
permits differing prototypes for `musttail` under `tailcc`; its
[code generator documentation](https://llvm.org/docs/CodeGenerator.html#tail-call-section)
explains the callee-popped argument area required for guaranteed tail transfers.

The candidate uses FastCC's argument registers and an aligned, callee-popped stack area.
Each tail carries `incoming_argument_bytes - outgoing_argument_bytes` to frame lowering.
Outgoing fixed objects use that signed delta, and negative fixed objects reserve growth
before callee-saved spill slots are assigned. Frame lowering consumes a dedicated
`TC_RETURN` pseudo and emits the existing true direct/indirect tail instruction. The final
frame deallocation and argument delta are combined where possible. No normal-call/return
fallback is introduced.

A TailCC frame's CFA is the SP preceding its incoming argument area. CFI descriptions adjust
both the CFA and saved-register offsets; RVV save expressions receive the same correction.
Normal calls with TailCC stack arguments use dynamic call frames and an FP in the caller.
The callee thus restores exactly the caller's stable SP on normal return or exception unwind,
including after intermediate tail calls with different argument counts. Register-only calls
need no argument-area adjustment. Existing C/Fast conventions have zero CFA correction.

TailCC currently disables shrink wrapping because moving an epilogue to a common predecessor
can transfer argument ownership before the actual return, lose a tail's individual delta,
or leave an entry path without its initial CFA. It does not use opaque save/restore libcalls
or compressed push/pop frame layouts. Fixed vectors that exhaust vector registers go directly
to stack slots; they never reuse a same-position incoming pointer under differing prototypes.
Scalable-vector overflow is explicitly unsupported by this candidate; Wasm v128 is fixed-size.

Qualification must cover growing and shrinking stack areas, permutations, indirect targets,
deep constant-stack recursion, integer/float/vector overflow, true tail assembly, preserved
registers, and a typed native exception after a 0-to-18-argument transfer. The exception test
must run a real landing pad, read caller-stack canaries, call another ordinary function, and
return successfully. A register-only throw or successful IR verifier is insufficient evidence.
The source and TableGen output form a new provider: all affected RISC-V libraries must be
rebuilt together, and the archived `.8` provider must remain unchanged.
