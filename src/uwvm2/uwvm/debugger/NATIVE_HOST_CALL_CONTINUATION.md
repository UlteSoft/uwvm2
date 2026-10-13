# Native host-call continuation transaction

This design keeps the applied SI boundary gate: ordinary SI never executes an unproved control transfer. A completed NI must execute a call normally with TF cleared, then stop before an exact authenticated Wasm continuation instruction. It must never present the host/VM call target as a native stop. The Linux LP64 x86-64 prototype requires the explicit native-continuation product switch and the actual owner-table SDK. A successful native test qualifies its observed cases; it does not complete the execution matrix below. Software call continuation and RISC-V software finish have separate scoped execution records below.

## Authority and continuation

The runtime first canonicalizes the immutable capture control block, actual pause ticket, selected participant, current generation, exact loaded function interval and retained code engine. The initial stop must be the genuine backend trap for that native thread. Its saved register context supplies the real SP; a console number never supplies SP or a target address. All other current participants stay parked and admission stays closed throughout the existing pause episode.

A bounded owned complete function copy is decoded forward from its exact loaded entry. Actual MC must classify the current instruction as a conventional returning call and prove `call_pc + decoded_size` is another instruction boundary inside that SAME Wasm owner. Unknown, trap-state-changing, far/system or owner-end call continuations decline before any gate opens. A callee does not need native-debug ownership for NI because it runs normally with TF OFF. Cross-Wasm SI additionally needs an actual typed callee entry and runtime-issued owner-map handoff; a section address, global unwind row or numeric target cannot authorize it.

The private cursor retains the original participant, thread, actual frame incarnation, MF/generation/epoch, code owner, call PC, continuation boundary and origin SP. Only the canonical runtime registry can issue it. Public replies may carry display labels but cannot construct or reuse the cursor. Runtime reset/hot replacement retire this cursor before any engine or worker lease drain.

## Linux hardware continuation

Use a per-target-thread hardware EXECUTE breakpoint via `perf_event_open`. The descriptor belongs to `fast_io::native_file`; no JIT byte or permission is changed. Use `PERF_TYPE_BREAKPOINT`, `HW_BREAKPOINT_X`, `sample_period=1`, `PERF_SAMPLE_ADDR`, `disabled=1`, `pinned=1`, `exclude_kernel=1`, `exclude_hv=1`, `remove_on_exec=1`, `sigtrap=1`, no inheritance and `PERF_FLAG_FD_CLOEXEC`. Linux x86-64 EXECUTE breakpoint ABI requires `bp_len=sizeof(long)` (8), not a one-byte data breakpoint length; the hardware checks one instruction address and never reads eight code bytes. The Linux LP64 x86-64 reader validates the real 128-byte `siginfo_t` extent/alignment and public prefix/address offsets, then copies the UAPI perf cookie/type/flags from bounded object representation. It does not require libc to expose `si_perf_*` accessor macros or define replacements for them. Where accessors exist, compile-time assertions verify their exact offsets. This host DATA decoder supplies no native stop authority. Before opening an event, the control plane reads the actual kernel release through `uname` and requires at least Linux 5.19, which introduced the deferred-delivery ASYNC flag; older-version backports conservatively decline. This version check is never made inside a signal handler. Other platforms remain unavailable until a qualified adapter exists.

A never-reused opaque scalar cookie identifies our synchronous `TRAP_PERF`. Do not use a freed pointer or allow the debugger to supply the cookie. The signal handler verifies the active fixed session, same selected native thread, kernel signal code, decoded perf type, cookie, `si_addr` and exact PC before any state change. Only the exact Wasm continuation and its original SP may publish a native stop. A recursive/reentrant inner instance that reaches the same code address with a different SP is ignored with TF remaining OFF; Linux sets RF for its hardware execute fault, permitting normal execution to continue. Do not use PERF_EVENT_IOC_REFRESH(1), which would disable the event on the wrong recursive instance. Parent/callee logical incarnation confirmation remains a private manager check before publishing the stop.

## States

1. `trapped`: the original real Wasm trap is parked. No continuation event exists.
2. `prepared`: canonical cursor and fully decoded owned continuation exist; event is disabled. Any failure destroys the disabled FD and retains original PC/registers/stop ID.
3. `armed`: enable the event while that same trap is still parked, recheck ticket/engine/stop/cursor, then atomically choose continuation execution before opening its gate. No step flag is set.
4. `running_call`: selected thread executes call and host/callee normally. No VM PC, GPR, native memory or backtrace capability exists. Other participants remain stopped and new admission is closed. Selected debug-only polling skips this requested pause while the transaction is active; ordinary guest execution gains no IR branch.
5. `continuation_trapped`: exact hardware event at the actual continuation and original SP. TF remains OFF. Handler retains the actual context and parks. Manager disables the event while retaining its FD and owners, re-authenticates the same cursor/publication and validates incarnation before committing a fresh native stop inside Wasm. Subsequent SI still uses the existing pre-execution boundary proof.
6. `cancel_pending`: EH, explicit interrupt, timeout, close, reset or an owner mismatch first disable the event and mark cancellation. The FD, fixed session and actual worker/engine lease remain alive until the worker acknowledges retirement. Closing a perf FD does NOT prove queued kernel task work drained. Own late canceled TRAP_PERF is consumed without showing its PC/registers or publishing a native stop; other signals retain their prior disposition.
7. `cancel_acknowledged`: the selected thread reaches a real cooperative before-park or its actual entry-exit path after the event was disabled. This thread-side ACK permits the retained event FDs to close and their canonical owners to retire; the backend clears the corresponding continuation identity. A new cooperative capture uses its actual current Wasm location, never the old native frame.

Timeout must never pretend a still-running host call is parked. It returns a stopping/busy result and retains the cancellation session until actual ACK. A permanently uncooperative host call already prevents safe VM shutdown; no manager can safely free its code or force a visible VM-native stop merely to meet a timeout. No code bytes require restoration in this design.

EH notification cancels before a guest throw may unwind the origin. The exception is not converted into a fake successful NI at fallthrough. Catching at a real Wasm safe point gives a new cooperative stop. Reset/guest exit drain the event before releasing execution ownership. GC/checkpoint admission still requires their own genuine whole-cohort census; a hardware continuation cursor does not replace it.

## Finish

Finish uses this same continuation transaction only after an actual native CFI unwind restricted to registered owned JIT ranges and the selected worker's actual stack bounds identifies a genuine Wasm caller and its continuation. Do not guess `*SP`, follow arbitrary frame pointers, use DWARF file data as read authority, expose host frames, or assume a global unwind table proves an engine generation. The initial true-return path needs owner-map join, private CFI accessors and original caller CFA/SP/incarnation. Missing caller proof declines and retains the current stop. Hardware finish qualification is separate from the current Linux software-return implementation and its execution records.

## Required execution tests

Both products, instruction/unwind, CLI/DAP: six reported VM-bridge families; ordinary call, recursion/reentrancy at repeated continuation addresses, indirect Wasm call, WASIp1 host call, return/activation leave, caught and uncaught EH, hot replacement, stale/forged owner, interrupt, timeout, reset and worker exit; no VM-native stop or VM-read capability. Verify event FD retirement, queued delivery grace, TF OFF throughout the host call, exact continuation before execution and unchanged default guest IR/memory paths. Hardware denied/absent is unsupported, never PASS. Linux kernel FD/HWBPs component tests are separate from runtime authorization proof. Windows/Mach need their own existing debug-state preservation and real VM tests; BSD needs a qualified adapter or later transactional JIT software breakpoint implementation.

Primary references: [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html), [Linux perf UAPI](https://github.com/torvalds/linux/blob/master/include/uapi/linux/perf_event.h), [Linux perf core](https://github.com/torvalds/linux/blob/master/kernel/events/core.c), [Linux x86 hardware breakpoints](https://github.com/torvalds/linux/blob/master/arch/x86/kernel/hw_breakpoint.c), [LLVM MCInstrDesc](https://llvm.org/docs/doxygen/classllvm_1_1MCInstrDesc.html).

The ABI reader and its component fixtures do not qualify runtime NI or finish. The actual synchronous and blocked-ASYNC hardware-event probes must execute against the real kernel; event permission denial is SKIP, never PASS. Primary layout/flag references: [Linux 5.19 siginfo UAPI](https://github.com/torvalds/linux/blob/v5.19/include/uapi/asm-generic/siginfo.h), [Linux x86 siginfo UAPI](https://github.com/torvalds/linux/blob/v5.19/arch/x86/include/uapi/asm/siginfo.h).

For NI, an actual near CALL64 in the sealed current Wasm caller can now select an opaque-call continuation when its target lacks typed Wasm ownership. The issuer removes all target/slot claims, then repeats the complete canonical capture, physical cursor, revision, caller publication and original GPR check. It never dereferences an unknown slot, stack or host target. This permits normal VM/host helper execution with TF off and an event only at the caller's proved fallthrough and original SP. Cross-Wasm SI retains its separate typed callee/owner proof. This hardware path still requires actual kernel and full-runtime NI/cancellation qualification; software continuation evidence is tracked separately below.


## Linux software call continuations

The Linux software trap backend implements `ni` at a conventional returning
call for AArch64, ARM state, i686, PowerPC, MIPS64, RISC-V64, LoongArch64,
SPARC64 and s390x. Availability still depends on the actual LLVM product,
current native stop, instruction form and immutable Wasm owner. The qualification
record is [native_call_linux_software.qualification.json](../../../../test/0017.runtime/native_call_linux_software.qualification.json).
Backend execution, LLVM MC DATA and full Wasm runtime execution have separate
scopes in that record.

The issuer walks the complete owned instruction boundaries and seals the
caller's return point. MIPS64/SPARC64 include the exact ordinary delay slot.
ARM uses the published ELF instruction mapping to skip literal pools and
reject DATA at the call, return or escape instruction. The post-call instruction
must admit a complete contained step: an ordinary instruction or a direct
branch whose every reachable successor stays inside the same true Wasm function.
Its private escape plan seals up to two distinct successor sites, including
SPARC's actual delay-slot PC/NPC pairs. Missing evidence refuses before wake.
No requested address, callee register value or native stack read creates a plan.
SPARC V9's complete MC-decoded `MEMBARi` is a contained ordinary instruction
only for the four low memory-order bits (mask 0..15). Lookaside, MemIssue,
Sync, reserved bits and truncated forms acquire no permission. The memory-order
exception does not change the public rule hiding unmodelled/memory rows.

RISC-V64's exact `VSETIVLI`, `VSETVLI` and `VSETVL` MC forms are also
contained fallthrough instructions. They must have the complete three-operand
shape, exactly one GPR definition, exactly VL/VTYPE implicit definitions, no
implicit uses, and no memory/control/trap descriptor effects. Immediate AVL is
bounded to 0..31; immediate VTYPE admits only standard E8/E16/E32/E64 and
nonreserved LMUL/policy bits. All register operands must belong to the actual
MC GPR class. Register-sourced VTYPE stays private. These execution DATA checks
never authorize public VL/VTYPE/vector values or native memory; unmodelled
instruction rows remain hidden. CSR access, reserved immediate encodings and
truncated forms acquire no exception. The architectural configuration behavior
is specified in the [RISC-V Vector specification](https://docs.riscv.org/reference/isa/v20260120/unpriv/v-st-ext.html).

The runtime revalidates the real trap revision, publication, capture and private
snapshot inside one synchronous resume transaction. Its closed event binds the
original cursor and expires when that host callback ends. Software patches use
the existing bounded W^X backend. The callee executes normally; neither a VM
helper nor an imported host callee becomes a debugger stop or read capability.

Repeated return PCs in recursion require the actual dynamic Wasm incarnation
chain. Genuine deeper descendants pass a private return/escape pair without
publishing their snapshots, then re-arm the original return point. Completion
requires the original physical chain. Stack equality is insufficient because
TailCC can pop outgoing stack arguments; the real frame identity remains the
proof. Host suspension/reentry, pending tail transitions, retired origins and
changed runtime epochs cannot authenticate a descendant.

Every enabled Linux software backend participates in the same runtime lifecycle.
After the genuine safepoint observer runs, a still-running continuation skips
only the cooperative park; cancellation restores the ordinary real park.
Host unwinds and Wasm exceptional leaves request cancellation on the actual
worker before the dynamic ledger retires. The outer execution scope obtains
the worker ACK before poisoning the ledger or dropping its admission. These
hooks grant no code, register or memory access to the VM helper.

A successful stop clears source/local/memory replies and exposes only independently
proved Wasm numeric register bits. Native SP/FP and all unproved register bytes
remain hidden. Cancellation restores installed bytes and waits for the real
worker ACK before releasing retained owners. A blocked host helper may remain
pending until actual guest progress; no host-frame stop is fabricated.

The added fixtures cover real native calls, six-level recursive return/escape
hits, cancellation, default/expired event refusal and ordinary SI regression.
The full RISC-V Wasm console test uses a ten-parameter recursive function to
exercise TailCC stack adjustment, plus a nonreturning Wasm callee interrupted
into a genuine cooperative pause. Both repositories run instruction/unwind
policies and verify final guest results and register concealment. The newer
[SPARC/RISC-V lifecycle record](../../../../test/0017.runtime/native_call_linux_sparc_rv_lifecycle.qualification.json)
qualifies actual full LLVM Wasm NI/return/cancellation/EH sessions for those two
targets, genuine SPARC `-Rdbg` CLI numeric steps, and the current twelve-target
backend regression. Its current RISC-V vector-configuration delta also passed
16 actual full Wasm NI/modern sessions in both repositories under both policies,
with zero unavailable NI in every modern return/abandonment case. The 6560 MC
DATA cases remain separate from native execution.

The same current-source closure passed 16 RISC-V software `finish asm` sessions:
ordinary return, recursion, cancellation with a retained NI call, and a
64-parameter function with 70 recursive frames. Each deep case privately retired
69 descendant returns and 69 actual instruction successors before the original
caller completed. Public native stack bytes stayed zero, stale stops remained
retained, and unknown registers and SP/FP stayed hidden. Source/SDK/dependency,
log, process-retirement and lossless-cache closure checks passed in the original
64 GiB Linux cgroup. Resource-aborted batch 99 stays failed; its completed
consumer acquired runtime qualification only through fresh sessions in batch 106.

These results qualify the recorded source cut and current native files, with
concurrent language changes outside that closure. Other targets' complete LLVM
products and all Wasm feature combinations remain separate qualification scopes.
Earlier RISC-V software finish evidence is retained in
[native_finish_riscv64.qualification.json](../../../../test/0017.runtime/native_finish_riscv64.qualification.json);
neither record qualifies finish on every target.

Inactive Linux cross-sysroot, SPARC SDK and old MC build caches are retained in
four lossless archives. The lifecycle record contains each archive SHA256,
original root, selected paths and the verified file manifest. Restore only the
required paths, verify their file hashes and original metadata, then check the
folder budget before running another architecture. The active RISC-V SDK and
sysroot used by the completed runtime regressions remain available. Completed
consumer images and producer objects were reclaimed after qualification; rebuild
from the retained source, actual dependency files and recorded recipes for a
new execution batch.

## Linux AArch64 physical callers and software finish

[native_finish_aarch64.qualification.json](../../../../test/0017.runtime/native_finish_aarch64.qualification.json)
records the Linux little-endian LP64 AArch64 full LLVM runtime closure in both
repositories, under both instruction and unwind call-stack policies. The
52 actual Wasm sessions include returning-call NI/cancellation, modern
GC/memory64/return_call/try_table execution, 20 software finish fixtures,
eight physical caller/backtrace/hot-replacement sessions, numeric registers
and four actual `-Rdbg` CLI sessions. There are 28 real finish return hops.
Six CFI metadata runs and two MC window DATA runs are separate counts.

Debug activation emission requests LLVM asynchronous unwind rows for the
actual TailCC Wasm core and its distinct public wrapper. Return adjustment
comes from each actual registered return CFA row. No guessed ABI pop count,
logical frame or CFA-only metadata can authorize native finish. Deep
64-parameter/70-frame finish tests require 69 same-PC/wrong-SP descendant
returns and 69 real instruction successors before completing the original
caller. Independent large-frame caller tests require an explicitly bounded
32-parent prefix, current owner/generation/epoch and live stack-slot checks.
Their contained instruction wakes are not counted as finish events.

A private cursor caches only decoded return-code facts for the same real
kernel revision and exact body/owner/generation/epoch. Live registers,
CFA, sparse saved words, dynamic chain and original worker-stack bounds are
revalidated on every query. A revision change discards those facts.
A64 current-window queries use the actual fixed four-byte ISA rather than
LLVM ELF's generic one-byte assembler alignment estimate. Each returned slot
is decoded by real MC; unaligned/truncated/unknown slots refuse, and explicit
mapping masks retain their separate forward proof.

The public projection exposes only independently proved Wasm numeric bits.
SP/FP, VM/host frames, unproved register storage and native stack bytes stay
hidden. Forged or stale aliases, wrong sessions, missing external park,
failed wake and callback reentry acquire no execution or memory authority.
The QEMU ENOSYS stack-copy fallback uses FastIO for the same authenticated
eight-byte private slot and grants no public address read.

The record qualifies frozen source182 and its recorded QEMU prefix.
Real AArch64 hardware, big-endian/alternate signal ABIs, signed PAuth LR,
SVE/SME, full INT and other ISA/Core3 combinations remain separate scopes.
The 32 completed runtime command scopes from failed parent batches191/209
were independently revalidated in batch217; those parents retain their
failed labels. Batches218/219/220 ran the remaining actual sessions, and
batch221 audited the complete closure. Compiled consumers were reclaimed
after the verified source/dependency evidence archive and process retirement.
Use the retained recipes to rebuild them. A later Linux reboot does not
change the boot identity bound to these historical execution receipts.

Full INT's interpreter opfuncs are host implementation code. They never become
Wasm native ownership merely because a guest instruction uses them. The current
`-Rdbg` entry requires full LLVM compilation and native thread support; both
AArch64 full INT products reject it at startup with exit126. Independent normal
Wasm scalar/memory execution succeeds. The exact product/source/library and
startup results are recorded in
[native_interpreter_boundary_aarch64.qualification.json](../../../../test/0017.runtime/native_interpreter_boundary_aarch64.qualification.json).
This is a startup isolation check, not a live interpreter debugger or native
register/step qualification. Missing full INT debugging support must not be
filled by granting access to interpreter C++ code, registers or stack.


## Linux i386 CFI and private four-byte slot foundation

[native_i386_cfi_stack.qualification.json](../../../../test/0017.runtime/native_i386_cfi_stack.qualification.json)
records paired production CFI parsing and owned four-byte evaluation, together
with actual QEMU i386 private stack reads on normal and high-address pthread
stacks. Both O0/O3 metadata fixtures use genuine LLVM i386 TailCC functions.
The current observer retains RA column8 and four-byte target addresses; reads
use the i386 ABI width. No x86-64 eight-byte slot crosses into a neighboring word.
Overflow, unsupported RA expressions, stale registration, wrong TID and slot
bounds remain fail-closed. The new methods grant no public stack permission.

This foundation has zero live i386 Wasm caller or finish events. The 64-bit
host metadata fixture relocates target sections to bounded 32-bit labels and
executes no i386 code; QEMU reader fixtures use their own actual registered
thread stack, not a Wasm issuer. At that foundation cut, i386 public
physical-caller/finish gates remained closed. The later integration below
separately qualifies the genuine 32-bit product, live Wasm issuer, kernel
register mapping, parent-SP proof and return continuation.
The historical AArch64 52-session record remains bound to source182; the later
CFI and private-slot regressions do not requalify that entire VM source cut.


## Linux i386 physical callers and software finish

[native_finish_i386.qualification.json](../../../../test/0017.runtime/native_finish_i386.qualification.json)
records 52 actual GNU32/QEMU full LLVM Wasm sessions in both repositories,
including 20 finish fixtures and 28 real physical return hops. Private CFI
reads use four-byte CFA-4 return slots and real kernel i386 DWARF registers.
Genuine RET32/RETI32 decoding supplies each actual parent SP adjustment.
INT3 EIP normalization precedes cancellation and descendant replay.

Deep finish checks 69 same-PC/wrong-SP hits and 69 executed successors per
70-frame run. Large-frame backtrace checks all 38 distinct invocation proofs
and six explicit bounded32-parent prefixes per run, including replacement
generation, stale/alias/protected-wake/rollback/ACK and Wasm-root refusal.
A private same-revision cache retains only decoded code facts. Live stack,
registers, CFI and owners are revalidated; i386 boundaries decode forward
from the actual function entry, without AArch64 fixed-width shortcuts.

Public numeric GPR/XMM bits are joined to the same genuine stop and private
compiler locations. ESP/EBP/EFLAGS, VM/host frames, unknown/upper register
bytes and native stack storage stay hidden. The modern exception fixture
admits exact Wasm integer SSE2 carriers without exposing the private location
table. Returning NI and genuine Wasm abandonment keep distinct assertions.

Production source273 and assertion-only overlay283 are independently pinned.
The resource-stopped batch283 keeps its failed label; batch287 revalidates
those compiled consumers and repeats all 12 modern/mixed sessions. Batch288
audits the 52-session closure. Batch289 hashes all 8,060 archived production
files and both overlay sources, then reclaims only own retired compiled
consumers. All SDK archives/headers, sysroots, source, recipes, dependencies,
logs and failure records remain available. Rebuild consumers for later runs.

See [i386 usage and remaining scope](../../../../test/0017.runtime/README.debug-i386-finish-status.md).
This record does not qualify real i386 hardware, x32, alternate libc/signal
ABIs, full INT, native DAP stepOut, concurrent source changes or physical
finish on PPC/MIPS/LoongArch/SPARC/ARM/s390.
