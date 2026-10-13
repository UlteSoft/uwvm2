This is a **source-only actual-runtime fixture candidate**. It has not been
compiled or executed locally and has no native or platform qualification yet.

The fixture uses the actual owning CLI initializer, full fused compiler and
publisher, a host-created console authority, the real controller observer,
private runtime activation/code-copy factory and platform single-step backend.
It sets a real emitted instruction breakpoint in a Core 3 `return_call` module,
observes a genuine native instruction trap and authenticated copied GPRs, closes
the same pause domain, requires register reads to fail, detaches twice, joins
the actual guest and checks result 13. Runtime reset then retires its observer;
the weak controller owner must expire only after the native borrower drained.

Optional bounded close delays run a real concurrent close against the first
native-step request/arming/trap sequence. A close may win before request, while
the guest arms, or after a completed trap. Rejection is valid; lost native
ownership, a blocked gate, stale code read, wrong result or failure to retire
the observer is not. No timing or output is interpreted as proof that one
particular arming instruction was hit. Exact phase coverage would need a
separately reviewed private test-hook witness; no such hook is added here.

New `reset` and `stop` optional cases invoke the actual host maintenance API
while the genuine native trap is still active, **without pre-detach or release**.
The copied strong observer context must close the actual domain and retire its
backend borrower outside publication locks before execution-lease drain. A
domain-close-only implementation deadlocks in this case. The bounded external
runner treats that timeout as failure, never success. Direct reset must retire
the actual observer; stop retains it until the later reset. These are new ABI
tests and require all TUs to consume the appended `on_close` observer callback.

`recursive-reset` and `recursive-stop` are separate fatal controls: a genuine
native trap is proved before invoking a deliberately invalid trusted host close
callback. That callback must be entered exactly once, then abort before any
second drain or publication/fallback-map access. The remote runner accepts only
the expected fatal signals with both proof markers once; timeout, stack-overflow
SIGSEGV, recursion, callback return and a normal-success marker all fail. These
death cases are not normal successful reset/stop observations. Their guard uses
independent constinit trivial TLS, including final global destruction, and never
a map-backed thread-state scope that may already have been destroyed.

The keeper must build this host fixture against the same **fresh** production
compiler/runtime/CLI layout macros and complete dependencies, including the
availability packet, native session owners and code-copy APIs. An old runtime
object or identical binary name cannot qualify the candidate. The optional
Linux runner executes an already qualified binary, checks the existing 64GiB
cgroup, parses and validates the WAT with official wasm-tools, records artifacts
and runs both instruction/unwind call-stack policies. Its binary digest is
identification; the required external build record supplies build provenance.

Other-platform owners may port the runner while retaining all actual backend
qualification gates. Exit 77 is unavailable/SKIP, never a passing platform.
This tests detach/close lifecycle, not complete native `ni/finish`, arbitrary
register writes, executable checkpoint restore or GC reference capabilities.
No production hot-path guard, lock, opcode map or instruction is introduced.

The tail-call fixture follows the current [WebAssembly 3.0 control instruction
semantics](https://webassembly.github.io/spec/core/syntax/instructions.html#control-instructions).
The distinction between one native instruction and native next/finish follows
the [GDB continuing and stepping manual](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Continuing-and-Stepping.html);
this lifecycle test does not confer those additional stepping capabilities.
