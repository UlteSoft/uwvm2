# Linux LoongArch64 Wasm native debugging

[The paired execution record](native_finish_loongarch64.qualification.json)
qualifies the little-endian 64-bit GNU full LLVM products in uwvm2 and
uwvm2-ros under QEMU LoongArch64. All compilation, Wasm validation, target
execution and executable evidence audits ran in the original SSH Linux 64 GiB
cgroup with swap disabled and serial compilation. The record contains 52
actual Wasm sessions: four actual `-Rdbg` CLI sessions, 20 finish fixtures,
eight physical-caller sessions and four numeric-register sessions. Twelve
CFI/layout/decoder/private-slot component executions are counted separately.
ROS gains no basic runtime mode.

Use the actual participant from `info threads`; the example assumes it is 1:

```text
step asm 1
info all-registers
ni 1
bt 1
finish asm 1
```

`step asm` executes an authenticated native Wasm instruction. `ni` runs a
proved returning Wasm call and stops again inside its original caller.
`bt` reports only authenticated Wasm parent identities, with an explicit partial
prefix at the 32-parent limit. `finish asm` returns to the nearest proved Wasm
parent. A root, host caller, expired stop, unknown instruction boundary or
missing owner cannot authorize a native return or disclose native context.

The genuine LLVM 23.1.1-uwvm-ros.11 SDK is required, with the
[LoongArch asynchronous CFI repair](../../documents/toolchain/loongarch-async-epilogue-cfi.md)
and the emitted native owner table. The recorded target SDK has 64 LoongArch64
archives containing 1,609 actual ELF64 little-endian machine258 objects.
Two genuine backend translation units are rebuilt without changing the ABI or
generated headers. MCJIT enables LLVM CFIFixup for this opt-in target so a live
framed block after an earlier return retains its correct CFI state. The O0/O3
registered-CFI fixture executes actual C ABI 64-argument functions; its explicit
legacy-layout negative case reproduces the malformed row with fixup disabled.

Private unwinding uses only the actual registered worker stack and CFI-proved
slots/registers. Return adjustment comes from the owned emitted return's real
CFA=SP row, with no TailCC or argument-pop guess. An identical PC/SP/body cannot
nominate another physical activation. Each 70-frame finish run requires 69
wrong-SP recursive returns and 69 real instruction successors; the suite proves
28 actual parent return hops. Deep caller cases authenticate 38 distinct actual
invocations each, including six bounded 32-parent prefixes, and exercise
replacement generations, aliases, stale proofs, protected wakes, rollback and
real worker release ACK.

The decoder admits only DBAR ordering hints 0, 0x10, 0x12, 0x14 and 0x700 with
an exact operand shape and a successor inside the same authenticated function.
This includes LLVM's acquire fence before a patchable Wasm call. Ordering
instructions remain hidden publicly; other hints, IBAR, system/trap forms and
out-of-owner successors are refused. Genuine recursive NI enters and leaves
five recursive descendants under both call-stack policies in each product.

Public GPR/FP/vector values require the current kernel stop and compiler-proved
Wasm numeric locations. The numeric suite covers i32/i64/f32/f64/v128 and real
SI/NI, including large i64 values. Unproved and residual bytes are cleared.
Zero, RA, TP, SP, r21, FP, status, native stack bytes, VM/host frames and arbitrary
native address reads remain unavailable. Private unwind reads provide no public
memory command. Hidden VM bridge rows expose no instruction bytes or text.

Eight modern Wasm NI return/exception sessions exercise GC, memory64,
return_call and try_table under both call-stack policies. Four mixed retained-NI
and finish-cancellation sessions also pass with actual worker ACK. This coverage
does not qualify every Core 3 feature combination or infer exception causes
from fixture names.

The frozen cut is source393. Its ordinary repository bytes equal source390
exactly; the record preserves those original successful execution commands,
logs and binary/dependency hashes. ROS runtime and host consumers are freshly
compiled from source393, using its genuine generated-only scope-depth accessor;
the scope is suspended during host callbacks. The folder-budget stops in batches394,399,401 and407 remain failed. The final
primary batch414 verifies their exact successful, fully reaped executions and
actually completes the remaining deep caller case. Batches415/416 compile and
execute the numeric and modern consumers; batch420 freshly compiles both real
CLI products. Batch421 independently audits the complete closure and its
[final retirement receipt](native_finish_loongarch64.retirement.json) proves
that the audit process itself also retired. Concurrent source changes remain outside this cut; the
[workspace comparison](native_finish_loongarch64.workspace-comparison.json)
records four subsequent compiler/cache edits in the paired runtime and emitter
files. The 32 other compared changed files still match this tested cut.

The own-folder limit remains 4 GiB. The initial successful stages enforce
a 30 GiB aggregate recovery-tree limit. Peer growth repeatedly trips that
self-selected aggregate guard; final CLI/audit stages explicitly use 36 GiB
while retaining a 32 GiB free-disk reserve and all memory/process controls.
The proof records each actual policy, its resource peaks and the adjustment. Retired products have checksum-verified recoverable archives;
inactive archives, old source cuts, historical logs and metadata are copied
to the recorded local retention directory and completely verified before
removing only their matching remote copies. Failure logs,
recipes, receipts and source cuts are preserved. No foreign process or file is
adopted for cleanup.

Actual LoongArch64 hardware, alternate libc/signal ABIs, full INT and native DAP
stepOut need separate qualification. Full physical caller/finish integration on
PPC, MIPS, SPARC, ARM and s390 remains unfinished; existing low-level instruction
and register results cannot qualify those return paths.
