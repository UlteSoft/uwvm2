# Linux i386 Wasm native debugging

[The paired execution record](native_finish_i386.qualification.json) qualifies
the GNU little-endian 32-bit full LLVM products in uwvm2 and uwvm2-ros under
QEMU i386. All compilers, Wasm validation, target execution and evidence audits
ran in the original SSH Linux 64 GiB cgroup with swap disabled. This scope has
52 actual Wasm sessions, including four actual `-Rdbg` CLI sessions, 20 finish
fixtures, eight physical-caller sessions and four numeric-register sessions.
Ten CFI/return-pop/private-slot components are separate executions.

The build requires the genuine 32-bit LLVM 23.1.1-uwvm-ros.11 SDK with the
emitted native owner table. The Linux build option `--linux-native-debug=y`
now enables the continuation product for x86/i386/i686 as well. SDK archives,
target libraries, production profiles, source identities and actual compiler
dependency closures are pinned in the record. ROS gains no basic runtime mode.

At an authenticated Wasm stop, use the actual participant from `info threads`:

```text
step asm 1
info all-registers
ni 1
bt 1
finish asm 1
```

`step asm` executes a proved native instruction. `ni` can run a returning call
normally and stop again inside its original Wasm caller. `bt` shows authenticated
Wasm parent identities, with an explicit partial prefix at its 32-parent bound.
`finish asm` returns to the nearest proved Wasm parent. A Wasm root, host caller,
expired stop or missing owner cannot authorize a return. Unknown instruction
boundaries retain the stop and report unavailable.

The genuine i386 kernel register layout is translated to DWARF columns before
private unwinding. Saved return addresses are exact four-byte CFA-4 reads from
the actual registered worker stack. Parent SP adjustment comes from genuinely
decoded RET32/RETI32, including the actual immediate operand. No fixed TailCC
argument-pop guess or backward instruction-boundary guess is used. INT3 EIP is
normalized before private descendant replay or cancellation. Real 70-frame
recursion requires 69 wrong-SP returns and 69 executed instruction successors
per deep finish run; the complete suite proves 28 actual parent return hops.

Public GPR/XMM values require the current kernel stop and compiler-proved Wasm
numeric locations. Complete i64 bits may use SSE2 on i386; the numeric suite
also exercises f32, f64 and v128. Residual upper bytes and unproved values are
cleared. ESP, EBP, EFLAGS, native stack bytes, VM/host frames and arbitrary native
address reads remain unavailable. Private CFI reads never become a public
memory command. Hidden VM bridge rows expose no instruction bytes or text.

Modern GC/memory64/return_call/try_table NI return and exception sessions pass
under both call-stack policies. The exception fixture separately proves genuine
integer XMM carriers and an authentic Wasm pause after NI abandonment. That
coverage does not qualify every Core 3 feature combination or infer the exact
exception cause from a fixture name.

Production source273 and test-only assertion overlay283 have separate hashes.
Resource-stopped batch283 retains its failed label; batch287 independently
revalidates its compiled consumers and freshly repeats all 12 modern/mixed
sessions. Batch288 audits the complete closure. Batch289 independently checks
all 8,060 archived production files and both overlay files, then reclaims only
retired private compiled consumers. Recipes, SDK libraries, sysroots, source,
dependency files, logs and failures remain available. Rebuild consumers for
new execution. Concurrent changes outside the recorded source remain separate.

Real i386 hardware, alternate libc/signal ABIs, x32, full INT and i386 DAP native
stepOut require their own qualification. Full physical caller/finish integration
on PPC, MIPS, LoongArch, SPARC, ARM and s390 remains unfinished; their existing
low-level instruction/register results cannot qualify these return paths.

Later LoongArch64 full LLVM physical caller/finish is separately qualified by
[native_finish_loongarch64.qualification.json](native_finish_loongarch64.qualification.json).
The unfinished-target list above describes the historical i386 source cut;
this later result leaves its recorded i386 numbers and source/ABI scope intact.
