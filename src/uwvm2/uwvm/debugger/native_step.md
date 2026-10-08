# Native machine stepping

`native_step.h` is a host-only backend for `step asm THREAD`. It is separate
from Wasm safe-point and source stepping. Linux selects the x86_64 trace-flag backend or a target-specific software
breakpoint backend after LLVM-full debug setup and an explicit host request.
Ordinary JIT executions do not install the native signal handler.

The active guest's safe-point bridge is an assembly-only wrapper. Its C++
observer first captures an owning JIT function range and the exact return PC;
the wrapper sets EFLAGS.TF only in its final `POPFQ; RET` sequence. The first
`TRAP_TRACE` therefore stops at that JIT PC before another guest instruction.
The target thread waits on a private futex. The manager removes its external
park before opening the first gate; the second `TRAP_TRACE` follows exactly
one native instruction and closes the gate again. The manager must authenticate
the copied participant, OS thread ID and `[owner_begin, owner_end)` range,
retain the execution lease/code owner, and reject a second step if the new PC
has left the authenticated function. A selected instruction may itself branch
or call, so the second PC is a result to inspect, not an automatically trusted
owner. Only one native-step session can be active in a process.

The SIGTRAP handler reads/writes lock-free fixed-size state, the saved
`ucontext_t`, and raw futex syscalls. It never calls a runtime observer, the
cooperative pause domain, a compiler, an allocator, or an ordinary lock.
Unrelated SIGTRAPs are delivered to the pre-existing disposition. The host
must serialize handler installation/removal with its own process signal
administration and retain the session until `phase::released` and `clear`.
A cold host-side transition guard keeps ready-session cancellation from racing
the return shim's pointer borrow; the signal handler never takes that guard.

The cooperative pause domain has a host-only selective transfer: first wait
until every guest is parked, then `release_one_for_native_step`; after the
signal handler reports `at_guest_pc`, call `external_park` to restore a fully
stopped snapshot. Before `continue_one`, call `external_unpark`, so code
replacement and memory inspection cannot treat a running instruction as
parked. Repeat the external park at `trapped`, then unpark and release the
signal gate before resuming the global pause ticket. Other participants remain
in their original cooperative waits throughout the selected instruction.

macOS ARM64 selects a separate thread-scoped protected Mach exception backend.
The AAPCS64 bridge carries the guest return PC in `x30`, parks in the host
observer, arms a hardware breakpoint for only that authenticated PC, then
returns to the JIT. The protected exception reply removes that breakpoint and
single-steps exactly one instruction; no `thread_set_state` call occurs inside
the exception callback. LLVM MC decodes only four aligned bytes copied from a
live JIT owner range. The backend, bridge ABI, and guard-page disassembler
have passed actual macOS ARM64 standalone and LLVM-full CLI tests under the
4 GiB limit in both repositories.

Windows x64 selects its VEH adapter through the shared dispatcher and uses the
controller's persistent two-trap session protocol. Its full Windows LLVM-full
product test is still pending. It must never call
`SetThreadContext` on a running thread: Microsoft requires suspending the
target first. Neither platform uses the Linux SIGTRAP implementation.

Focused Linux cgroup tests are `test/0017.runtime/native_step_linux_x86_64.cc`
and `test/0017.runtime/cooperative_pause_domain.cc`; actual LLVM-full
qualification is `test/0017.runtime/native_step_llvm_full.cc` and
`test/0017.runtime/run_native_step_debug_cli.py`. The standalone test proves
zero guest instructions at the first trap, exactly one at each of two further
traps, owner/TID rejection, first-trap cancellation, and unrelated-thread
signal forwarding. Both repositories have passed O3 LLVM-full CLI tests under
`instruction` and `unwind`, including two consecutive disassembled native
steps, hot-replacement regression, and secure-server detach from a native
trap. These results do not extend to untested operating systems or modes.

The Linux software backend implements actual target instruction execution and
kernel signal-context capture for AArch64, i686, PPC32/PPC64, ARM32, MIPS64,
RISC-V64, LoongArch64, SPARC64 and S390x. It patches only authenticated live
Wasm JIT instruction boundaries and sealed successor sites, checks original
bytes, changes pages from RX to RW and back to RX, and flushes the instruction
cache. Other Wasm participants stay parked. Cancellation, Wasm exceptions and
engine retirement restore owned patches before releasing the code lease.
This backend uses software breakpoints rather than an architectural trace flag.
An unsupported instruction or unproved successor is refused before execution.

The private kernel context is not a public register dump. Display requires a
live numeric provenance record for an exact typed Wasm value and the complete
register class that holds it. SP/FP/RA/TLS, unproved registers, host or VM code,
raw native stack and process memory grant no asm debugger capability. Capturing
private machine state to resume execution does not make that state inspectable.
Wasm linear-memory inspection continues through its bounded Wasm interface.

Actual Linux QEMU product qualification covers x86_64, AArch64, i686, both
PPC64 byte orders, both MIPS64 byte orders, RISC-V64, LoongArch64 and SPARC64
under both instruction and unwind policies in both repositories. S390x has
also passed those product checks. These are target-binary execution results
under emulation, not physical-machine qualification. ARM32 additionally needs
the linked TailCC and EHABI loader repairs described in
`documents/toolchain/arm-native-debug-calling-convention.md`; its complete
numeric-carrier product qualification is tracked separately from relocation
and IR component tests. This list does not assert a full project/module build.


SPARC64 static integer and floating branch conditions now use their exact MC condition operand:
always (8) and never (0) have one proved PC/NPC successor. An annulled always
branch skips its delay slot and stops at the owned target with NPC=target+4;
an annulled never branch stops at PC+8 with NPC=PC+12. Non-annulled forms
still stop before the delay instruction, with the corresponding owned NPC.
Both SI and NI require a complete owned boundary proof for the resulting PC
and NPC. Nested control in an already pending delay slot, an NPC at/outside
the owner end, foreign targets and stale trap revisions remain refused.
The public annotation marks an exact never condition as target-never-taken=1;
a supplied annotation cannot claim this for a different actual MC condition.
The native_sparc_delay_step component checks actual kernel PC/NPC/registers,
including an executable trap in the skipped delay slot; its isolated page
issuer is a kernel component fixture, not a Wasm/public source-authority proof.

The sealed SPARC breakpoint plan also retains whether each exact successor
is a pending delay slot. A sequential NPC=PC+4 does not erase that fact:
a nested branch/call/return is refused before execution even on this path.
The controller and runtime issuer use the same current owned kernel snapshot
and closed plan; an optional display annotation supplies no such authority.

The backend copies that delay fact into the accepted trap before publishing
its parked state. Staging a future plan or a failed continue cannot overwrite
the current trap fact; snapshot readers acquire it under the same transition
guard as PC/NPC and trap revision. The next accepted ordinary trap clears it.
