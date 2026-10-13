# Actual managed full-JIT shutdown

Requires the paired42 actual shutdown runtime/compiler candidate, current full
source closure, and qualified native exception cleanup. Assemble the modern
`try_table` fixture with the official Wasm assembler, then run the driver as:

`debug_actual_managed_shutdown_runtime fixture.wasm instruction|unwind none|observe|resumable parked-loop|host-block`

This is real owned parse/initializer and the actual one-pass full LLVM compiler,
native guest parent/child execution, genuine cooperative park or a deliberately
blocked real observer callback. It checks canonical request ownership, forged
control-block refusal, same-domain retry, real ≤50ms pending execution, retained
owners, foreign cancellation skipping Wasm catch_all, actual entry RAII/work
drain, unchanged host result, true test-worker join, request consumption/reset.
The blocked callback test is an actual native debugger host callback inside a
Wasm safe-point bridge; it does not claim arbitrary WASIp1 socket/file callback
or C-import coverage. Production physical thread join is a separate CLI owner
contract. Backend cache workers use qualified actual FastIO native physical join;
unsupported/pending retains the real native owner. The process-owned idle service
object remains retained after its worker retired. This does not prove external
caller thread/service destruction, all finalizers, checkpoint restore or replay.

Only the sole Linux keeper may compile/run these fixtures in the original
64GiB cgroup/CPU lane. No local native execution has been performed. Requested
matrix: two products × both frame strategies × three profile purposes × both
scenarios, ordinary/null IR zero-change comparison, real generated cleanup
Invoke inspection, and actual CLI quit/CtrlD/pipe EOF with the finite native
worker API after ROOT narrow-composes both candidates. Cross-platform native
cleanup qualification remains to be tested by the platform keeper.

LLVM normative references:
https://llvm.org/docs/ExceptionHandling.html
https://llvm.org/docs/LangRef.html#invoke-instruction
Wasm Core 3 catch syntax/semantics:
https://webassembly.github.io/spec/core/text/instructions.html
https://webassembly.github.io/spec/core/exec/instructions.html

Managed stop uses the REAL execution stopping flag and closes admission, but
intentionally does not deliver legacy stop_token callbacks: atomic.wait
cancellation currently traps inside its noexcept bridge. Existing blocking waits
and native syscalls therefore remain genuine pending until they return; the CLI
may explicitly force process termination. This is a safety limitation, not a
claim of complete cooperative wait/thread termination. Ordinary request_stop
behavior is preserved outside an actual managed request. R1/R2/R3 are HOLD;
R4 is the first candidate with this actual atomic-wait safety closure.

R5 additionally requires the exact independent physical-native-thread R2
candidate. Cache terminal phase closes acceptance/wakes its genuine worker and
only returns success after actual provider physical join, or if its worker
truly never started/already joined. Earlier R4 resources_quiescent was WORK-only;
R5 is strict backend-native retirement while caller guest thread still requires
its separate actual join. No fallback unbounded join is used for terminal status.

`debug_actual_cache_terminal_shutdown` is a separate real-cache-service
component: it starts the genuine async store worker, proves idle WORK0 still
owns a joinable native thread, then stops/wakes it and consumes the actual
FastIO owner only after qualified OS-native join. No fake host-loop/early-exit
ACK is used. It requires the exact physicaljoin R2 provider packet and runs only
on a provider qualified by actual native testing. Native0 remains the current
source-candidate fact; unsupported providers retain owners and are not passed.

R6 rebases the actual current coherent control/EH and exact resume ABI2 without
changing the raw continuation ABI or granting saved DATA new authority. A new
HOST `runtime_debug_shutdown_terminal_cleanup_abi_host_api()` symbol returns 2
only for the new terminal contract. The CLI queries it before begin; older
runtime objects lack the symbol and fail a mixed link. Revision alone proves
no execution/native ACK and cannot bypass any retained-owner or OS-join check.

`debug_actual_resumed_shutdown_runtime fixture.wasm instruction|unwind` adds
the real resumed-entry path: Core3 nondefaultable i31 local and a waiting
parent are captured before a flat child nop, original native frames actually
retire/drain, canonical saved frames execute through engine-resolved resume
entries, the new native parent/child parks again, and managed cancellation
really unwinds the resumed entry. Original and resumed host result owners must
remain unchanged. Its private catch lies inside the actual NOEXCEPT host entry
boundary and only accepts the privately issued foreign cleanup signal after
real native activation/shadow cleanup. Native testing is still pending. This
is not complete fresh-world, FILE checkpoint, arbitrary host or replay support.
