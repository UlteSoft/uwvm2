# Console integration after the control and pause foundations

This is the next integration contract, not an implemented CLI. The shared
`utils/control` session and Linux launch channel are implemented and independently
tested. The full-JIT safe-point runtime is a separate foundation. There is still
no claim here that `-m debug-jit`, instruction stepping, breakpoints, a server or
function replacement works end to end.

## Full compilation readiness

Add a host-only preparation API that validates the requested entry ABI and
compiles/publishes the complete LLVM-full registry without invoking any guest
function. It must share normal execution admission, publication and failure
handling, expose an owning generation/identity, and reject every other backend
or mode. Configuration happens before publication. Calling an arbitrary warm
function is not a valid preparation API: even a void function may have effects.
Guest start functions, if pending, remain guest execution and must follow the
same explicit execution policy rather than being hidden inside a readiness claim.

Preparation completion is distinct from a pause with zero participants. The
current runtime enrolls a participant after full compilation, so an empty paused
domain proves that no enrolled guest is running; it does not prove that module
compilation or initialization finished. Wait for preparation before exposing
module inspection or replacement commands.

For the initial console, create a pause ticket after preparation and start the
guest worker behind closed pause admission. Report “prepared; entry not started”
until continue opens admission. There is no actual guest PC or call stack yet.
Do not manufacture an entry frame from the selected function index. A future
stopped-at-entry implementation can explicitly enroll and park that real frame.

## Command-to-runtime mapping

Only runtime compiler `llvm_jit_only` with mode `full_compile` maps to the
control capability combination LLVM/full. Tiered execution is not LLVM/full,
even though it can compile native functions. Construct the launch authority only
after explicit host opt-in; a console uses `attach_console`, while an external
transport must prove its own launch binding before `attach_launcher`.

Use bounded textual console input to build the same typed protocol requests
used by the server. A pending `request_ticket` is an authorized request, not a
successful runtime action. Keep its work admitted through the host management
lifetime until it finishes or is safely cancelled; reset/revocation blocks new
requests but does not retroactively cancel a delivered work item.

- **status / info threads:** inspect only state and snapshots actually available.
  Complete with `inspected` after successful inspection.
- **pause:** request a domain ticket, wait with a deadline, and capture only if
  the same ticket is still paused. Only then acknowledge `paused`.
- **pause timeout:** the domain deliberately leaves the request active. Cancel
  that same ticket with `resume` before acknowledging a rejected request and
  reporting running state, or retain an explicit pending operation. Otherwise
  the guest could stop later with the console incorrectly saying it is running.
- **continue:** successfully resume the matching pause ticket before
  acknowledging `resumed`. Stale or closed tickets report failure.
- **memory read:** preallocate the bounded result outside the stopped-domain
  lock, then revalidate the generation, Wasm memory index and current bounds
  during a short stopped operation. No native address comes from the command.
- **replacement:** validation and compilation happen before a short stopped
  publication. Recheck complete canonical function ABI and expected generation.
  The current foundation only parses the request and must reject execution until
  actual replacement is implemented.

The present compiler's safe points are function entries and loop headers. They
do not establish individual-instruction stepping, arbitrary breakpoints or local
variable recovery. Those operations need their own implementation and must be
reported unavailable meanwhile. Likewise `cooperative_pause_location` is only a
top location. `_Unwind_Backtrace` on the management thread returns the management
stack; a debugger backtrace needs the guest's cold pause path to capture an owning
stack snapshot or a separately validated saved native context.

No compilation, execution wait, domain re-entry or unbounded user callback may
run inside `while_stopped`. That callback holds the domain mutex to keep resume
and close from racing a short inspection/publication. Compilation metadata and
code owners need their own normal lifetime protection as well.

## Disconnect, exit and reset

The control session preserves the last host-confirmed running/stopped state on
disconnect. An incomplete pause is not a completed stop. The console must state
its disconnect policy; losing a channel must not silently publish code or resume
an execution whose host intended it to remain stopped.

Closing a pause domain wakes parked threads; it does not forcibly terminate a
guest or an uncooperative host call. Therefore “close then join” is not a bounded
quit operation for an infinite guest loop. A standalone CLI must define explicit
inferior termination through its process-exit policy, or a separate supported
cooperative stop path. Do not label an unbounded drain as completed termination.

Reset must close pause control before draining outer execution leases, including
entries waiting for pause admission. The runtime retains code and metadata until
those leases leave. External host configuration/reset/loader changes remain
serialized administrative operations. A host callback's public raw re-entry
must reuse its outer participant, not enroll twice.

The Linux adapter currently supports a pre-fork embedding launcher with
SCM_CREDENTIALS and a retained creator pidfd. Its CLOEXEC handles deliberately do
not survive exec. A CLI exec bootstrap or a late-attach pathname server therefore
needs a separate reviewed transport setup; environment/argv/bare inherited FDs,
same UID and loopback reachability must never become authorization shortcuts.
