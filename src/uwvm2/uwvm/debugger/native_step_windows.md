# Windows x64 native-step adapter

`native_step_windows.h` provides the Windows x64 part of the proposed
`step asm THREAD` backend. It is host-only and selected by the normal
`native_step` platform dispatcher on Windows x64. The Linux backend and
Wasm/source safe-point protocols remain separate.

The host must first own an LLVM-full debug execution lease, stop all guest
participants, and authenticate the chosen participant, Windows thread ID,
exact return PC, and owning JIT function range `[owner_begin, owner_end)`.
The JIT code owner and execution lease must stay alive until the session is
released and cleared. The adapter's `request` checks the ID/range shape and
serializes one active session; it cannot prove JIT ownership by itself. The
target must be cooperatively parked in the bridge before `request`, which
briefly suspends that thread to read `CONTEXT_DEBUG_REGISTERS`, rejects every
enabled DR7 hardware breakpoint, and then resumes it. The thread handle is
retained until `clear` so the selected native identity stays pinned. This is
a cold debugger operation, not a memory-access or JIT execution-path check.

The return shim must be assembly-only. Its final `POPFQ; RET` sets x64 TF;
the first `EXCEPTION_SINGLE_STEP` must report `CONTEXT.Rip == expected_pc`
inside the retained JIT range before any guest instruction executes. The
vectored handler clears TF, publishes `at_guest_pc`, and waits on an unnamed,
noninheritable manual-reset event. After the host inspects this parked
snapshot, `continue_one` opens the first gate and the handler sets TF in the
saved fault-time `CONTEXT`. The next single-step exception records the PC after
one native instruction and parks on the second gate. Alternatively, `release`
at the first stop clears TF and resumes without a step. At the second stop,
`release` allows execution to continue. A branch or call can move that second
PC outside the retained function; another step needs a new owner check.

Only a selected OS thread with a private TLS session and the active session
pointer can consume either exception. Other exceptions return
`EXCEPTION_CONTINUE_SEARCH`, preserving the existing VEH/SEH chain. The
saved `CONTEXT.Dr6` and `EFlags` are recorded at both stops as diagnostics.
Actual Windows 11 build
26100 tests returned `Dr6=0` and `EFlags=0x202` for both real TF traps, so
neither `Dr6.BS` nor a retained TF bit can serve as that qualification. The
pre-arm DR7 inspection rejects an active same-thread hardware breakpoint;
an unrelated native debugger that changes debug registers after inspection
has process-control privileges and must not share this adapter's execution
lease. A first exception at any PC other than the authenticated JIT return
PC terminates the process with `0xE0000DB7` before guest execution resumes.
The handler never compiles, allocates, calls VM observers, or takes the host
transition lock. Events are created before publication and closed only after
`released` or cancellation from `ready`. A ready cancellation and the return
shim are serialized by the host transition lock. There is no safe cancellation
while an instruction is armed or executing: a controller must use a bounded
wait, report failure, and retain the execution lease until the target has
been released or the process is terminating. Do not call `SetThreadContext`
on a running thread; the adapter edits the fault-time `CONTEXT` supplied by
VEH instead. An invalid event wait, signal or close is an internal ownership failure; the
process exits with `0xE0000DB6` before guest execution can resume. The
`INVALID_HANDLE_VALUE` pseudo-handle is rejected before waiting, because
Windows otherwise treats `-1` as a current-process handle and hangs.

`test/0017.runtime/native_step_windows_x86_64.cc` uses executable Win64 code
with two `incq` instructions. It checks the first stop before either increment,
the second stop after exactly one, the JIT range, native-thread selection,
unrelated VEH forwarding, 256 ready-cancellation races, and isolated children
that verify failed event wait/signal and invalid first-PC operations terminate
before guest execution. It also installs a real hardware execute breakpoint
on the selected worker and verifies that `request` rejects it, then tests a
release at the first stop. The test must run
on real Windows x64, with its QEMU process in the same 64 GiB/20-CPU Linux
cgroup that cross-compiled this executable. The cross-compiled PE alone does
not establish runtime support. The standalone PE passed on Windows 11 IoT
Enterprise LTSC Evaluation build 26100 under QEMU/KVM on 2026-09-23; the
dispatcher/controller integration is present, but the LLVM-full Windows
product build and live `step asm` test have not yet passed; that platform's
end-to-end support remains unqualified.

Microsoft documents the [VEH registration and ordering](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredexceptionhandler),
the [x64 `CONTEXT.Rip` and `EXCEPTION_CONTINUE_*` callback pattern](https://learn.microsoft.com/en-us/windows/win32/debug/using-a-vectored-exception-handler),
and the [running-thread restriction on `SetThreadContext`](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadcontext).
Microsoft also states that [`EXCEPTION_SINGLE_STEP` includes other
single-instruction mechanisms](https://learn.microsoft.com/en-us/windows/win32/debug/getexceptioncode),
and [GetThreadContext requires suspension to inspect a live thread](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext).
