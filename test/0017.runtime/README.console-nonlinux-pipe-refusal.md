# NonLinux synchronous pipe input: SOURCE-only refusal

F_DUPFD_CLOEXEC duplicates the same POSIX open-file description. A competing
reader can steal readiness between poll and synchronous read, and a CtrlC
signal delivered to another worker need not interrupt the manager's blocked
read. Terminal VMIN/VTIME does not apply to pipes. No safe independently owned
nonblocking/cancellable macOS/FreeBSD pipe reader is qualified yet.

This narrow SOURCE change rejects nonregular nonterminal input on macOS and
FreeBSD before SIGINT installation or input/terminal modification. Regular-file
policy and actual terminal VMIN/VTIME behavior remain existing. Linux's actual
independent FIFO O_NONBLOCK provider is byte-identical, as are Windows reader
ownership/cancellation and existing WASIp1 code. Generic POSIX targets whose
read_byte loop has no transfer provider now fail before ready publication.
This is honest refusal, NOT support qualification for redirected pipes or all
platform debugger input. Embedders can supply their own proven isolated bounded
console_io transport; native provider work remains necessary for pipe support.

No new syscall/raw IO/string manipulation exists in production. The finite
actual-target test must be launched by the platform keeper with real pipe stdin;
it checks no read, preserved real descriptor/dev/inode/flags, previous SIGINT
handler/flags/mask while rejected session lives and after native_file RAII cleanup.
It uses FastIO status/getfl/print plus existing or test-local typed noexcept ASM
POSIX aliases. Linux returns77; do not count that as target pass. No macOS build
or native run has happened. All native tests use ROOT's guardian/cgroup; local
Mac tests require prior Linux memory qualification and4GiB limit.

Meaningful follow-up: Linux existing15 keyboard component regressions + actual
PTY CtrlC/EOF/restore remain necessary; actual FreeBSD/macOS terminal and pipe
refusal must be independently qualified. This source packet grants no exception to
safe physical ownership/ACK requirements for a future cancellable reader.

Revision2 retains all production bytes. Its target fixture checks the complete
actual FreeBSD SIGRTMAX-inclusive range, rather than old NSIG32; macOS checks
1..NSIG-1. This corrects the independent SDK SOURCE peer's test coverage gap;
no target compile/native pass is inferred from these constants.
