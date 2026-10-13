# Host control session foundation

This library provides the shared command session for the planned debugger and
function replacement service. The portable session does not open an endpoint or
authenticate an OS peer; the Linux launch adapter below supplies one concrete
transport. Neither layer stops a thread, reads guest memory, compiles a function
or publishes code.
No debugger or replacement CLI is enabled by including it.

Only trusted host launch setup constructs `launch_authority`. Both capabilities
default to disabled, and only LLVM full compilation passes `qualify`; interpreter,
lazy, lazy verification and tiered configurations return distinct errors. The
authority cannot be upgraded by a command. Its move-only `launch_permit` has
private owner identity and is never encoded, printed or read from an argument,
environment variable, signal or file descriptor. The public instance ID routes
frames; knowing it grants no authority. The launch owner can revoke all sessions,
including outstanding permits and completion tickets, concurrently.

Console and external launcher permits are separate launch origins. An external
adapter must first authenticate its exact startup-authorized connection, then
call `attach_launcher`. `authenticated_launch_peer` is a trusted-host interface
contract, not a credential verifier; accepting peer-supplied PID fields would
violate it. The expected launcher process and channel binding must both match;
the VM's own process is rejected even with a valid permit. A bare same-UID socket,
loopback connection or inherited FD has no permit. Native plugins are trusted
code and are outside the guest isolation boundary.

The Linux `linux_launch_channel` adapter creates an unnamed SOCK_SEQPACKET pair
before fork, with close-on-exec and nonblocking descriptors. The trusted launcher
retains one end, and its VM child selects the receiver. A pidfd opened against
the creator before fork pins that exact process identity. Every received packet
must carry the creator's kernel-checked SCM_CREDENTIALS (PID, UID and GID), and
the creator must still be alive. The receiver also needs the opaque launch
permit. A valid FD inherited by another same-UID process, a PID claimed in the
payload, and a sender inside the VM all fail authorization. Unexpected ancillary
data is rejected and delivered SCM_RIGHTS descriptors are closed, including
truncated ancillary messages. Each packet holds one frame or a fragment;
concatenated frames are rejected rather than silently discarded.

This is a pre-fork embedding transport, not CLI exec bootstrap or a filesystem
attach server. No method adopts an arbitrary FD. Exec closes all these handles;
a later exec bootstrap needs a separately reviewed privileged setup mechanism.
An external late-attach server must remain in the authorized launcher, or use a
private Unix socket plus an unguessable per-instance capability exchanged over
the protected launch channel. Same UID/directory permissions alone are never
enough. Guest descriptor tables/preopens must never contain control handles.
There is no environment, argument, signal or guest-FD fallback.

The Linux implementation follows the kernel credential/descriptor rules in
[unix(7)](https://man7.org/linux/man-pages/man7/unix.7.html), process-lifetime
handles in [pidfd_open(2)](https://man7.org/linux/man-pages/man2/pidfd_open.2.html),
and bounded ancillary reception in [recvmsg(2)](https://man7.org/linux/man-pages/man2/recvmsg.2.html).
Privileged native processes able to forge kernel credentials, native plugins and
compromised launchers are trusted-host threats outside the Wasm guest boundary.

The little-endian frame is 48 bytes: `UWC1`, u16 version 1, u16 operation, u32
payload length, zero u32 reserved, 16-byte instance ID, u64 generation, u64 request
ID. Payloads are bounded by 65,568 bytes. A decoder accepts fragments and consumes
at most one complete frame; the caller retains subsequent bytes. Errors are
sticky, truncated EOF is rejected, and no wire length controls allocation.
Sessions check instance/generation and exact monotonically increasing IDs across
reconnections. One request may be outstanding. Rejected host operations still
consume their ID. Invalid input closes the session without invoking runtime code.

Host framing APIs use `input_buffer` and `output_buffer`, aliases for fast_io's
contiguous buffer views over `wire_byte` (`unsigned char`). Fields are scanned
and printed sequentially with the fixed-width `le_get`/`le_put` manipulators;
the 48-byte wire layout is unchanged. No span adapter or temporary character
copy is required, including during constant evaluation. Input and output storage
must not overlap and remains owned by the caller.

`encode_frame` appends at the output view's current cursor and advances it only
after validation proves the complete frame fits. Rejected frames leave the
output bytes and cursor unchanged. `feed`, `receive`, and `send_packet` borrow
input views by value; framing returns `consumed` without advancing the caller's
view. The decoder copies each available header or payload fragment as a block,
then retains any bytes belonging to the next frame with the caller. Replacement
bodies still get an owning copy after validation so accepted tickets survive
decoder reuse.

Operations are status, pause, resume, step, bounded guest-memory read, function
replacement and detach. Step carries u64 guest thread ID. Memory read carries
u64 module ID, u32 memory index, zero u32, u64 offset, u32 length, zero u32;
length is at most 64 KiB and offset addition must not overflow. The runtime still
checks the live guest memory bounds. Replacement carries u64 module, u64 function,
u64 expected function generation, u32 body length, zero u32, then at most 64 KiB
of owning body bytes. Larger functions are explicitly rejected in this initial
protocol. The runtime must independently verify canonical parameter/result types,
reference types, module context and complete native ABI before publishing. No
operation carries a native address, register mutation or arbitrary native call.

Resume, step, memory inspection and replacement require host-confirmed stopped
state. Receiving pause only creates a ticket; it does not assert any execution
has stopped. The trusted runtime calls `complete` after the corresponding real
action. Wrong-operation, cross-session, stale and repeated completions fail.
Disconnect preserves the last host-confirmed running/stopped state; an incomplete
pause never fabricates a stop. The runtime must finish or safely cancel admitted
work before discarding its ticket. Session methods belong to one management
thread; authority revocation is synchronized. These cold locks and allocations
are absent from ordinary guest execution and memory access.

Headers and named-module exports have matching surfaces. The SSH cgroup runner
`test/0003.utils/control/run_session.py` checks unauthorized, malformed, truncated,
oversized, cross-instance, stale, replayed and wrong-completion requests, both
capability modes, owned body lifetime, concurrent revocation and named modules.
The Linux fork tests additionally exercise genuine kernel credential rejection,
forged-credential EPERM, own-process injection, inherited same-UID sender FDs,
FD-rights cleanup, oversized packets, truncated disconnects and creator death.

The console controller adds operation IDs 8–11: `breakpoint_set` (three u64
module/function/expression-byte-offset fields), `breakpoint_clear` (nonzero u64
breakpoint ID), `breakpoint_list` (empty), and `backtrace` (nonzero u64 participant
ID). All require the debug capability. Set/clear acknowledgements are
`configured`; listing/backtrace acknowledgements are `inspected`. Unknown IDs and
incorrect payload lengths remain fatal to that session. Registration alone does
not establish that a Wasm address is reachable.

`confirm_execution_state` is a trusted-host synchronization method for an
asynchronous breakpoint stop. It is not a wire command and rejects an outstanding
request. A controller may confirm stopped only after the pause domain establishes
actual quiescence; an observer requesting a stop is insufficient. These session
classes and the OS transport require native thread support. The bounded pure
protocol remains available on single-thread targets.
