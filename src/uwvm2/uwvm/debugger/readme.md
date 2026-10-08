# Host debugger controller

## Default JIT policy for enabled debugging

An enabled full LLVM JIT debug session defaults to the `debug` compilation
policy, so large instrumented functions can reach their first pause without
running the O3 pipeline. An explicit `-Rllvm-full-policy` or `-Rllvm-policy`
overrides this default; `auto` and `default` still select the session default.
Normal execution without an enabled debug session keeps its existing JIT
policy. The selection happens during compilation, not in the guest execution
path.

This folder contains the LLVM-full host console and shared bounded control
operations. The command grammar names three stepping levels. Wasm instruction
stepping, DWARF source-line `into`/`over`/`out`, and native instruction stepping
have separate execution engines. Source `over`/`out` requires a complete
caller-frame trace. Native stepping is enabled only where its host backend
can authenticate the current JIT code range and thread; unsupported targets
fail closed before resuming the guest.
`controller::create` requires an explicit, qualified host launch
grant; it opens no endpoint. The runtime must be configured with its domain and
owning observer before publication, using instruction safe-point granularity.
`-m debug-jit` calls `arm_initial_pause` after `prepare_debug_host_api` and before
starting its guest worker. This prepared state has no fabricated PC or stack.

Linux `-m run --debug-jit-control-fd N` is a separate, explicit opt-in for
commands after the guest starts. The full product also needs `-Rcc jit -Rcm full`;
ROS selects its LLVM-full default when `-Rint` is absent. The host launcher creates
an anonymous `AF_UNIX SOCK_SEQPACKET` socketpair before exec, keeps one endpoint,
and passes only the other as inherited FD `N`. It may wait for an application
marker before sending a packet containing one UTF-8 console command, such as
`status`, `pause`, or `bt 1`; each packet receives one reply. The launcher must
remain the sending process. The VM checks its peer credentials and live PID on
adoption and on every packet, rejects descriptors and oversized packets, and
seals the endpoint against guest WASI path opens including `/proc/self/fd`.
The VM endpoint has no pathname, listener, token, signal hook, or
arbitrary-process attach.
`quit` closes the control channel while the guest continues. Invalid packets
also close the channel. The test `test/0017.runtime/run_llvm_debug_control_fd.py`
is a runnable launcher example and exercises these cases.

macOS ARM64 supports the same explicit `-m run --debug-jit-control-fd N` grant
in LLVM-full mode. Darwin does not create an `AF_UNIX SOCK_SEQPACKET`
socketpair, so the host's **direct child** inherits one end of an unnamed
`AF_UNIX SOCK_STREAM` pair. Each command and reply has a four-byte,
network-order length prefix; commands are bounded to 512 bytes and replies to
65536 bytes. The VM checks `LOCAL_PEERPID`, `LOCAL_PEERCRED`, and
`LOCAL_PEERTOKEN` at adoption and again for each I/O, requires the original
peer to remain its direct live parent, rejects ancillary descriptors and
incomplete/oversized frames, and shuts down the channel when the parent exits.
The Mac-only socket seal conservatively rejects all guest-visible sockets
while control is active; it also covers `/dev/fd/N` reopening through WASI.
There is no listener or guest-visible capability path. The native test
`test/0017.runtime/run_llvm_debug_control_fd_macos.py` checks a running guest,
pause, Wasm/native step, function replacement, rights injection, parent exit,
guest FD/path isolation, and invalid modes under the 4 GiB watchdog. The
[Apple socketpair](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/socketpair.2.html)
and [getsockopt](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/getsockopt.2.html)
manuals describe the underlying host interfaces.

For a late client connection on macOS, use `tools/debug/secure_server_macos.py`:
`serve --uwvm BINARY -- -m run -Rcc jit -Rcm full --run MODULE.wasm` (omit
`-Rcc/-Rcm` for ROS). The broker creates the private socketpair and stays the
VM's direct parent. Its separate, owner-only local listener requires a fresh
256-bit capability file and a same-user peer that is not the guest; no listener
FD, filesystem socket path, or capability is inherited by the VM. The broker
rejects an explicit WASI preopen containing its private directory. A host
client uses `connect --socket-dir DIRECTORY --command pause` after the guest
has started, then may send `step`, `replace`, or `quit`. The native
`test/0017.runtime/run_llvm_debug_server_macos.py` proves this sequence for
both products; it also verifies that `quit` detaches while the Wasm guest
continues. The broker is optional: a trusted launcher may instead send the
framed commands directly on the inherited endpoint.

Late commands require JIT instruction safe points compiled from process start;
the opt-in therefore carries debug instrumentation even before the first command.
Without this flag the ordinary JIT does not create the controller or safe points.
The runtime cannot turn on those safe points in already-published code. A host
that authorized the control FD at launch may attach a client after guest entry;
an unprepared VM cannot acquire this capability after launch.

`tools/debug/secure_server.py` is an optional host-owned Unix server and
client for that FD. `serve --uwvm BINARY -- VM_ARGS...` starts an LLVM-full VM,
prints its private socket directory, and keeps the original authorized peer
process alive. `connect --socket-dir DIRECTORY` opens the command prompt;
`--command status` sends one command. The supervisor requires an owner-only
directory and a fresh 256-bit capability file, checks Linux peer credentials
and pidfd liveness, rejects packets from the VM's PID, passes no client FD to
the guest, and rejects SCM_RIGHTS. It also rejects an explicit WASI directory
preopen that would expose the capability file. The capability never appears
in guest argv or environment. `disconnect` closes only the client; `quit` detaches the
VM control FD and leaves the guest running. Tests in
`test/0017.runtime/run_llvm_debug_server.py` use a running Core 3 exception
and atomic guest. This is local Unix access, not a TCP listener.

The sole management thread calls `execute`, `inspect`, or `run_console`. The
runtime calls `on_safe_point` on each actual guest thread for breakpoint/step
policy. It calls `on_before_park` only when that thread is about to poll a pending
pause, permitting a cold owning stack capture. Neither callback waits for another
execution or invokes guest code. Controller/domain locks have one order:
controller then domain; waiting for quiescence releases the controller mutex.
`wait_until_paused`/`capture`, never the observer callback, establishes a stopped
state. Native unwind traces remain separate from instruction trace bookkeeping.

Wasm debug commands are limited to 512 bytes, with separate token and page
bounds for each grammar. GC member/path commands accept their full bounded
selector lists; there is no common five-token limit. Oversized input is drained
and discarded in full. Numeric IDs are unsigned decimal without signs or
expressions. Breakpoints are bounded to 256 entries and participant storage to the
launch-selected maximum (at most 4096). Console commands use the same UWC1 framed,
capability checked, monotonic request protocol as the host control session.
The inherited Linux packet channel translates the same bounded console commands.
The host supervisor may expose its own listener; the VM itself does not.

Commands:

- `break MODULE FUNCTION BYTE_OFFSET` accepts only an expression-relative
  offset for which the current LLVM-full function generation retained an
  instruction safe point after LLVM optimization. Immediate bytes, constant-dead
  branches, structural non-executing opcodes,
  imported functions and out-of-range offsets are rejected at registration.
  `break-source MODULE FILE:LINE` resolves the exact embedded DWARF path and
  one-based line to the first emitted safe point in function/offset order.
  Source files are never opened; paths are compared byte-for-byte. The console
  remains limited to 512 command bytes. `delete ID` removes the breakpoint;
  `info breakpoints` lists its concrete Wasm address. Successful function
  replacement clears that function's old breakpoints; its original DWARF line
  mapping remains unavailable while numeric breakpoints use the new body.
- `continue` resumes and immediately returns to the prompt. Use `status` or
  `info threads` to observe an asynchronous stop. These commands distinguish a
  pending pause from all executions being parked.
- `pause` waits up to two seconds for cooperative quiescence. A timeout cancels
  its pending request; an uncooperative host callback is never forcibly stopped.
- `step wasm THREAD` resumes all participants and requests a new global pause at the
  chosen participant's next emitted Wasm instruction safe point. Other participants
  may advance before parking. Other breakpoints do not preempt this selected step.
  It is not a machine-instruction step or a guarantee of stopping on structural
  opcodes that emit no reachable execution. `step THREAD` and `s THREAD` remain
  aliases for this same Wasm-level operation.
- `step source THREAD [into|over|out]` uses `into` by default. With a valid
  DWARF4/5 line table from a `-g` C, C++, or Rust Wasm module, `into` resumes
  until the selected participant reaches an emitted Wasm instruction in another
  function, source file, or source line. The debugger converts the instruction's
  expression offset to the Code-section-content offset used by Wasm DWARF.
  Missing, duplicate, invalid, or unmapped line data is reported explicitly;
  it never silently becomes a Wasm step. `over` stops at a different source
  location in the same or shallower caller depth, and `out` stops after the
  selected frame returns. Both require a complete captured caller-frame trace
  at the origin and each candidate stop; an unavailable or truncated trace
  reports an error rather than guessing a caller depth.
  DWARF paths are displayed only; the debugger does not open source files.
- `step asm THREAD` uses an authenticated JIT code/PC mapping and a host
  single-step backend. Linux x86-64 and macOS ARM64 have end-to-end LLVM-full
  product tests for consecutive native steps. A Windows x64 adapter and
  dispatcher exist; full Windows product qualification is still in progress.
  A host without a qualified backend reports assembly-step-unavailable.
- `bt THREAD` prints the owning stack captured on that actual guest thread before
  parking, innermost first. Missing traces, truncation, and unavailable source lines
  or caller byte offsets are explicit. Guest names have terminal controls escaped.
- `locals THREAD` prints the first 256 typed Wasm parameters and locals from the
  selected stopped frame. The generated code snapshots current values at each
  debug safe point; the owning thread copies them before parking. `i32`/`i64`
  appear as signed integers, `f32`/`f64` as exact bit patterns, `v128` as bytes,
  and reference values as null or non-null without exposing host handles.
  Larger local sets show an explicit truncation count.
- `frames wasm THREAD STOP_ID FIRST COUNT` pages the complete authenticated
  Wasm activation chain, innermost first, with at most 128 rows per request.
  `STOP_ID` must identify the current cooperative stop. The activation ledger
  grows in stable blocks of 64 frames; 64 is not a maximum call depth. Allocation
  budgets and an incomplete capture can still make a query unavailable.
- `locals wasm THREAD FRAME FIRST COUNT` and
  `operands THREAD FRAME FIRST COUNT` return typed pages of at most 64 values
  from the selected authenticated Wasm frame. These commands also cover locals
  beyond the legacy `locals THREAD` prefix. `FRAME` is an innermost-first ordinal
  in the complete chain; it does not grant native memory access. A resume or
  replacement invalidates the old capture. Numeric values preserve their types
  and bits after JIT; references use copied Wasm identities rather than host
  pointers. Operand previews identify their origin explicitly:
  `Note: Last Wasm safepoint snapshot; may differ from current native state.`
  An arbitrary ASM pause does not establish a fresh typed Wasm stack.
- `memory MODULE MEMORY OFFSET LENGTH` copies at most 256 bytes from a stopped
  Wasm linear memory after checking its current bounds. It does not accept a
  native address.
- `help` prints usage. `quit`/EOF returns `terminate_process` to the launcher, which
  must enact an explicit process exit policy. The console does not close a domain
  and wait indefinitely for an infinite guest to drain.

`console_io` accepts host-owned byte input and output callbacks. Its context must
outlive the synchronous call. The single-argument stdio adapter requires stdin to
be reserved exclusively for management. Production must remove every guest path
that can read that input, including readable inherited stdout/stderr handles.
The CLI launcher owns this isolation and the process-exit decision. No guest
argument, environment entry, native pointer, FD, UID, signal or loopback address
constructs a control grant.

A TCP listener and arbitrary code patching are unsupported. Function-level
replacement accepts an owner-controlled Wasm body file only while all guest
threads are stopped. The runtime compiles the new body privately, checks the
exact existing function ABI and generation, rejects an active target frame,
then publishes the new entry atomically. The command reports the resulting
generation. Late attachment requires the explicit launch-time FD capability
described above.

`impl.h` and `impl.cppm` expose all component headers/partitions. Unsupported
native-thread targets retain the pure command/protocol grammar without including
controller mutexes, condition variables, runtime headers or console execution.
