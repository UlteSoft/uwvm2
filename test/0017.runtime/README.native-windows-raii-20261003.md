# Windows x64 native-step RAII qualification input

This is a new source-only qualification input. It does not replace
`native_step_windows_x86_64.cc`, its frozen source, or historical receipts.
No compile, Windows execution, QEMU run or product qualification has been
performed by the fixture author. The Linux/Windows keeper must rebuild and
run these exact sources before recording results.

The new `native_step_windows_raii_x86_64.cc` uses actual Win64 executable
code, the real two-trap VEH protocol and an assembly-only return bridge.
It checks zero guest instructions at the first gate, exactly one instruction
at the next trap, a second instruction on command, and a read-only owned GPR
snapshot. The real snapshot must contain RBX pointing at the fixture counter,
nonzero SP, the exact next RIP, and displayed flags with only the debugger's
TF bit removed. Forged identities, a first-gate register query and a retired
session query must not invoke the register callback. Its retained value copy
must remain usable without restoring any query authority.

The selected thread and the two private event handles must each have one
`fast_io::nt_file` owner. The fixture checks that their three borrowed fields
match those exact owners, that both gates are distinct, and that all handles
are valid and noninheritable. A rejected owner-end PC, controller-thread
request, real active DR7 breakpoint, or another active session must leave the
rejected session's owners and borrowed handles empty. An unpublished idle
session that already owns an event must also be refused;
the caller's event owner must remain intact until the caller retires it.
A first-gate or trapped
session must reject `clear` while retaining the live owners. The owners remain
valid after the worker reports released, until `clear` withdraws the session
and empties all three owners and borrowed fields. The fixture also covers a
first-gate release and 256 ready-cancellation/non-target-arm races; every
cancelled session is cleared before its C++ object is destroyed.

Four actual child processes must terminate with exact failure codes:

- `--invalid-gate`: deliberately close one owned event exactly once, then
  supply `INVALID_HANDLE_VALUE` as the borrowed gate; expect `0xE0000DB6`.
- `--invalid-set-event`: reject a corrupted gate signal; expect `0xE0000DB6`.
- `--invalid-first-pc`: reject a first trap outside the exact admitted return
  PC; expect `0xE0000DB7`.
- `--invalid-live-destruction`: destroy an actually published ready session
  with live gate owners; its destructor must terminate with `0xE0000DB6`
  before member destruction can close gates or leave a dangling active pointer.

The parent process creates and waits for these children; a source branch or
expected exit constant alone is not evidence that any death probe passed.
Calling a session destructor is not a cancellation operation. Controllers
must still drain and clear an admitted session before normal destruction.
The ready cancellation race does not deterministically qualify cancellation
while the target is in the short arming/executing interval; add a separate
actual scheduling/stress receipt for that controller case if it is claimed.

All formatting and command construction use fast_io print/concat. The exact
child selectors use explicit fast_io cstring views, without decimal parsing
or libc string comparisons. Existing fast_io Win32 declarations handle thread
creation, waits, sleeps, exception raising, handle inspection, process creation
and exit status. Thread entry points and exit-code storage use their exact
`std::uint_least32_t` types, avoiding SDK `DWORD` callback casts. The seven
fixture-local SDK-typed `noexcept asm` aliases cover absent APIs only; they do
not redeclare SDK C functions or change shared control headers. Newly created
thread and process handles are adopted immediately. No pseudo-handle is
owned. Ordinary NT owner destruction is nonthrowing; the deliberate invalid
event probe uses `release` followed by fast_io's error-return `nt_close` so it
can check one close without a subsequent double-close. A failed diagnostic
print is caught before a noexcept callback calls `fast_io::fast_terminate`.
Microsoft's [NtClose object list](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntclose)
includes event, process and thread handles; these owners do not treat such
handles as disk files or perform a filesystem open.

## Fresh Windows test queue

Use the sole Linux/Windows test keeper. Every cross-compilation process and
the actual x64 Windows QEMU process must belong to its same 64 GiB cgroup.
Retire completed cross-build processes before starting the Windows guest;
retain cgroup limits, command lines, compiler/provider identities, source and
PE hashes, actual exit status and complete process/VM retirement. Do not run
this fixture, a compiler, QEMU, or the product on the local macOS machine.

1. Freeze the new fixture, this README, current `native_step_windows.h`, its
   module partition, `native_registers.h`, the Win32 ABI header and the actual
   fast_io dependency closure for each repository. Record their hashes. Both
   new fixture copies must be byte-identical. Preserve the old frozen fixture
   and old test results, including failures.
2. In the cgroup, use actual Clang/GNU Windows x64 provider headers and import
   libraries. The standalone fixture directly includes the Windows backend;
   it does not by itself test the product's platform-admission switch. Compile
   it with C++26, optimization and debug information, the repository `src`
   include root and its fast_io include root. Link the real provider's kernel32
   and ntdll import libraries and required C++ runtime. Keep the exact compiler
   invocation and linker diagnostics; a missing SDK, import library or runtime
   DLL is an unavailable closure, not a passed probe.
3. Inspect the new PE's imports and the assembly bridge. Require proper Win64
   shadow space/alignment and `POPFQ; RET` with no inserted C++ epilogue. All
   resource API calls must bind the intended fast_io or typed noexcept import.
   If exception-disabled builds are claimed, build and execute that variant
   separately; the source-only conditional error reporter is not proof.
4. Run the new executable without arguments inside the actual Windows x64
   guest. Retain full stdout/stderr and process exit status. Require all four
   isolated child exit lines, the real GPR checks, exact instruction counts,
   hardware-breakpoint rejection, ownership retirement and all cancellation
   rounds. A cross-compiled PE alone cannot satisfy this step. The fixture
   intentionally reports unavailable through a compile error on other targets;
   it does not qualify AArch64, pure MSVC, or the product on any platform.
5. Build both LLVM-full debug products freshly, including runtime, main and
   host debugger translation units against the same updated session/owner
   headers. Reusing an old runtime object or previous R3 PE is invalid because
   the session now contains RAII owners and a checked destructor. Compile the
   named-module closure too if that distribution is claimed. Then test actual
   product `step asm`/`si`, `info registers`, resume, cancellation, replacement
   and stopped-target retirement under both instruction and unwind policies.
   The standalone fixture does not qualify those product commands, native PC
   provenance, caller register reconstruction or full native step-over/out.

Microsoft documents [wait-handle lifetime](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject),
[thread context suspension](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getthreadcontext),
[resume semantics](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-resumethread),
and [VEH registration](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredexceptionhandler).
Wait handles must not be closed while a wait is pending; Resume must happen
before a temporary suspended-thread owner is released. The native backend
implements that lifetime separately from regular-file I/O.
