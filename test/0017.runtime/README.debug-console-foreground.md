# CLI foreground execution (SOURCE proposal)

The native adapter chooses foreground scheduling from actual input interactivity independently of terminal cursor/display support. Interactive `c`/`continue` executes the existing controller resume once, then issues real 50ms wait operations until an actual stopped/exited/closed snapshot. A slice timeout is never completion. The same HOST observation latches Ctrl+C across slices; a separate 2s interruption deadline can return an explicit pending-pause ERROR without canceling the actual request or freeing owners. `c&`/`continue&` selects asynchronous CLI resume. Pipe/noninteractive and default embedder adapters keep asynchronous plain continue, and controller/server/DAP protocol is unchanged.

The read-byte -3 path represents an already-consumed trusted HOST keyboard event. It passes a synchronous delivered-event borrow to pause so its deadline cannot accidentally resume the VM. Idle cancellation inspects the actual unchanged stopped state rather than issuing an invalid pause. UI acknowledges only events actually observed by management. Timeout wording follows real execution state and cannot claim a retained request was canceled.

Every production helper is included/exported by impl.h/.cppm and console.h/.cppm. The policy component exercises only bounded HOST grammar. The existing product keyboard regression uses explicit `continue&` before testing the standalone `wait` command. New actual PTY foreground tests must bind the exact current product build and Core3 GC/EH fixture, prove no prompt while running, terminal and external Ctrl+C returning actual fresh-stop capture, background prompt/status, redirect-output PTY input scheduling, pipe compatibility, and guest proc_raise preserving its original default signal termination. Blocked-host cancellation must obtain a real host-call entry witness and actual later park; a marker before an import alone is insufficient proof.

No native compile/run was performed by this source proposal. Full guest/provider/compiler/cache shutdown ACK is still separate and incomplete: current CLI _exit/ExitProcess is not reported as full cleanup, and runtime_stop_and_drain has no bounded physical retirement API. No unbounded join is introduced here.

Primary GDB behavior references:
https://sourceware.org/gdb/current/onlinedocs/gdb.html/Continuing-and-Stepping.html
https://sourceware.org/gdb/current/onlinedocs/gdb.html/Background-Execution.html

SOURCE peer correction in current-rebase R2: a successful resume may already report stopping when a breakpoint observer requests pause before inspect and another genuine participant remains unparked. Foreground waits through BOTH running and stopping; only actual complete stop/exit/closed completes normally. The narrow original hunks were rebased against ROOT applied Deep46R7 console/impl/cppm, preserving wasm_path and consolidated state routes. This is still source-only and not a native product result.
