# Debug console keyboard source proposal

This proposal is private and not installed in either source repository. No
native test or full-product test has been run by this author on macOS. Only the
SSH Linux keeper may admit builds/tests into the original 64 GiB cgroup. New
keyboard support affects the explicit `-Rdbg` console adapter, never the ordinary
JIT instruction path. No existing WASIp1 implementation or string operation is
modified.

## Ctrl+C and signal provenance

POSIX installs a three-argument SIGINFO handler once per CLI process. A reserved
terminal's genuine kernel event or an external same-effective-UID sender can
request an ordinary pause. A self-originated signal, including existing guest
WASIp1 `proc_raise(SIGINT)`, cannot request debug pause. It is forwarded to the
predecessor's original SIG_DFL/SIG_IGN/custom handler. Default termination and
SA_RESETHAND are preserved; rejected guest signals are never silently swallowed.
No sender PID or signal number authorizes memory, stack, frame, or native access.
Those remain independently authenticated by the existing controller and actual
current runtime captures. Linux terminal provenance must be verified with real
PTY SIGINFO events; non-Linux PID-zero kernel provenance is pending its own native
qualification, never counted as a Linux pass.

Accepted callbacks touch only lock-free static atomics. They never allocate,
print, acquire a mutex, borrow the console object, or call the controller. Rejected
signals invoke only the prior signal handler or POSIX async-safe disposition
operations; errno is restored. Prior callbacks retain their original async-safety
obligations: FastIO noexcept_call adds a C++ call boundary, not signal safety. The prior handler's mask/restart/nodefer/alternate
stack are kept. Snapshots are atomically published before installation, unchanged
through process lifetime. The adapter restores only its own still-installed
handler, preserving a third party's later disposition change. A second installation
is refused to avoid stale asynchronous callbacks racing predecessor replacement.
Ready publication uses an installing-state CAS after actual terminal setup;
retired one-shot dispositions cannot be resurrected. Read loops also recheck
retirement before each poll and byte return.

Windows uses the typed nonthrowing SDK import spellings used by FastIO. Its
ConsoleCtrlHandler only requests pause through a lock-free flag; file/HANDLE
ownership and printing remain in the management thread. Existing CRT/guest
self-signal semantics are not virtualized. A real Windows console must still be
tested; direct GenerateConsoleCtrlEvent admission and DAP are not qualified here.

The Linux console owns an atomic CLOEXEC duplicate using the existing FastIO
fcntl adapter and native_file RAII. FIFO input is independently reopened through
that private /proc/self/fd reference with O_NONBLOCK; actual dev/inode/type,
read-only flags and unchanged original flags are checked. Poll AND read use the
same owned new description. Stolen readiness returns EAGAIN and revisits the
interrupt flag rather than blocking. TTY reads use VMIN=0/VTIME=1; regular files
have the ordinary EOF contract. Arbitrary devices/sockets are not admitted by
this provider. No O_NONBLOCK changes reach the launcher's shared description.
macOS/BSD FIFO independence and Windows concurrent pipe consumption remain
unqualified; /dev/fd duplicates must not be mistaken for independent flags.
The launcher's actual pre-guest sealing and closed WASIp1 fd0 are independent
input-isolation boundaries and are not substituted for this race regression.

The native input adapter checks interrupts every 100 ms using POSIX poll or a
Windows console/pipe wait. It performs actual input through FastIO. The normal
manager context cancels queued terminal input and copied partial command text,
then executes the existing authenticated `pause` command. A paused inferior keeps
its controller session and actual current stop. A running inferior must reach an
existing safe point; timeout is reported honestly by the existing controller.
A source/native command already waiting in execute may take up to its existing
bounded two-second deadline before the console observes the interrupt.

## Editing and shorthand

History holds 32 bounded 512-byte commands. Up/Down and Ctrl+P/N recall; Ctrl+A/E/B/F,
Backspace, Delete, Ctrl+U/K/W edit. UTF-8 cursor deletion stays inside owned bytes.
Oversized commands and incomplete/unsupported escape sequences are discarded as
whole lines, including at EOF. CRLF completes one command, without accidental
empty-Enter repetition. Empty Enter repeats eligible diagnostic/step commands;
resume, replacement, scripts and other mutations cannot be implicitly repeated.
Frame list/show may repeat; selecting/moving the frame cursor may not.
Cancellation clears the repeat candidate, so a leftover newline cannot replay it.

No-argument s/step, n/next and finish mean source into/over/out. They require valid
-g mapping through the existing controller, and one actual stopped Wasm thread.
si/stepi, bt/where, disas, info locals and i aliases use the same existing command
admission. Explicit s THREAD/step THREAD retain the existing Wasm meaning. Thread
numbers from a fresh inspect are public labels, not private stop permissions.
Ambiguous or missing current threads are rejected before execution. Full GDB
completion/reverse-search and native calling convention expansion are not added.

## Exit and remaining work

Ctrl+D deletes under the cursor and exits only on empty input. Returning from
run_console runs keyboard RAII and restores terminal modes/handlers. This patch
DOES NOT implement full VM retirement/guest finalizers: the current launcher still
terminates its inferior process on quit/EOF. Root must separately implement a
real retirement ACK before joining workers and destroying live JIT owners; joining
an infinite Wasm loop is not a valid cleanup solution.

Server/DAP is a separate `run` management path, not run_console. DAP pause requests
exist, but OS Ctrl+C on an attached server/VM is NOT implemented/qualified by this
packet. It requires its own host-authorized serialized management integration;
never execute the controller from a signal handler or a second concurrent manager.

## Tests to admit on SSH Linux

1. Compile editor/alias and POSIX keyboard component with current FastIO headers.
2. Run run_debug_console_keyboard.py with the exact original cgroup path. Every
   subprocess remains inside that cgroup and has a five-second hard deadline.
   Tests record actual PTY kernel origin, owner kill origin, guest/self default,
   ignore/custom/SIGINFO/RESETHAND forwarding, third-party handler preservation,
   partial input cancellation, history, Ctrl+D, and terminal attribute restoration.
   Real FIFO tests keep the writer open, consume the readiness byte through the
   original reader, demand EAGAIN from the independent description, and check the
   original flags. A second case masks SIGINT on the reader and leaves a worker
   unmasked: an actual external owner kill must wake the poll loop by the flag.
   Component PASS never claims a genuine VM pause, DWARF fix or DAP result.
3. Build a new immutable full-product cut of both repos. On instruction and unwind,
   use a genuine infinite Wasm loop and finite driver deadline: initial wait,
   partial command Ctrl+C with same stop/session, continue then terminal Ctrl+C
   producing fresh all-stopped real Wasm capture, history/repeat and explicit steps,
   Ctrl+D restores console but full retirement remains unqualified. Separately
   run genuine WASIp1 proc_raise to verify old signal disposition still terminates.
4. Test DAP attached, Windows, macOS/BSD and actual module builds separately. Those
   are pending and cannot be promoted from the Linux component result.

Primary contract references:
https://pubs.opengroup.org/onlinepubs/9799919799/functions/sigaction.html
https://sourceware.org/gdb/current/onlinedocs/gdb.html/Command-Syntax.html
https://sourceware.org/gdb/current/onlinedocs/gdb.html/Continuing-and-Stepping.html
