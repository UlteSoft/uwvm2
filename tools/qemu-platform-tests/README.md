# Linux / QEMU shared execution ticket

`lane_ticket.py` is admission metadata for the existing PIDFD supervisors. It
does not run commands or replace their process, executable, parent, affinity,
source, provider, or resource checks. Source preparation can proceed while
another suite owns the execution lane. Native Linux, cross compilation,
Windows/FreeBSD guests, and qemu-user execution acquire the same exclusive
host-side flock. P-core profiling remains exclusive to the Linux keeper.

The authority JSON pins the current `boot_id`, outer Docker `cgroup_path`,
`init_pid`, `init_birth`, `init_exe`, `init_exe_sha256`, and the exact host-side
`init_cgroup` text. These come from a freshly verified container; a previous
boot's authority is invalid. The controller supplies the SHA-256 of this JSON
and its immutable command manifest. The private authority directory belongs
to UID 1000. Every admission and retirement requires the original init alone
in that cgroup, its live birth/executable identity, zero OOM counters, exactly
64 GiB memory, zero swap, and CPU set `0,2,4,6,16-31`.

The caller holds the `LaneTicket` object while its existing supervisor runs.
Only after the original supervisor completed and all original owned tasks
retired does it write a final receipt containing `supervisor_complete=true`,
`owned_tasks_retired=true`, `commands_sha256`, and this `ticket_id`. It supplies
that receipt and SHA-256 to `release`. `release` makes another fresh init-only
observation before unlocking. A failing test may retire safely with its real
failure preserved; retirement is not a correctness PASS.

A killed controller leaves an unretired receipt. The next owner rejects that
receipt even if the OS lock became free and the cgroup appears empty. The two
owners must reconcile the exact original PIDFD supervisor and retain the
abandoned evidence before changing admission state. No ticket grants power to
adopt, signal, or kill another owner's process. Unknown live cgroup members
always block admission; stale PIDs never authorize an action.

QEMU's user emulator adjusts target syscall ABI and endianness, but this is
not a libc, operating-system, or Wasm correctness guarantee. Each actual run
must identify target ELF/PE machine, byte order, libc and C++ ABI/provider
closure, exact frozen source, executable, emulator, runner and fixture hashes.
Compilation, loader startup, Wasm execution, and generated assembly checks
are separate results. A target without its dependency closure remains pending.
See the [QEMU user-mode manual](https://www.qemu.org/docs/master/user/main.html)
and [QEMU supported emulation](https://www.qemu.org/docs/master/about/emulation.html).
