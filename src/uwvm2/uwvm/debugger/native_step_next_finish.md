# Native next-instruction and finish backend proposal

Status: design only. No console command, platform capability, kernel operation,
generated-code change, or ABI is enabled by this document. The controller-level
contract is in `native_execution_plan.md`; this document covers native backend
evidence and choices. Existing platform qualification switches remain intact.

## Existing evidence and missing authority

`native_instruction_semantics::classify` validates a real decoded MC instruction
and returns `kind`, `size`, `conditional_branch`, `indirect_branch`, `barrier`,
and `may_change_pc`. Its indirect flag classifies branches, not indirect calls.
It supplies neither a branch target nor a return continuation. LLVM MC metadata
alone cannot implement native `ni` or `finish`.

`native_step::with_owned_registers` copies the authentic current trap's GPR
snapshot while holding the backend transition ownership. Its callback receives
the target native thread, actual PC, original owner extent and a local snapshot.
The snapshot has `pc()`, `sp()` and `fp()`. It contains no native stack bytes,
stack bounds, frame incarnation, or unwind rules. Neither raw `sp()` nor `fp()`
permits dereferencing a native address.

`llvm_jit_debug_native_step_site::return_pc` is the emitted debug bridge's
continuation in its actual Wasm function. It is not the physical caller return
PC. `resolve_llvm_jit_unwind_entry` resolves executable ownership and logical
module/function identity; it does not return CFA rules. The before-park logical
trace and DI line rows cannot substitute for an actual native caller frame.

GDB defines instruction-next as executing a machine instruction while running
through a call, and finish as returning from the selected frame. LLVM LLDB's
instruction and step-out plans retain stack identities, detect stale plans and
validate a return breakpoint. Those behaviors guide this proposal rather than
any claim of GDB/LLDB parity. See [GDB stepping](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Continuing-and-Stepping.html),
[LLDB instruction plan](https://github.com/llvm/llvm-project/blob/main/lldb/source/Target/ThreadPlanStepInstruction.cpp),
and [LLDB step-out plan](https://github.com/llvm/llvm-project/blob/main/lldb/source/Target/ThreadPlanStepOut.cpp).

## Proposed continuation proof

The runtime must derive a continuation from the actual stopped native thread,
an execution-pinned code publication, and its exact owned native stack. No
requested native address, file DWARF row, guessed frame pointer, guest value or
wire-supplied CFA can authorize it. A future private cold bridge needs distinct
stack ownership and CFI ownership; current line metadata remains display data.

For `ni`, first copy and decode the current instruction from the authenticated
code owner. Before adding its size, prove `size <= owner_end - current_pc` and
that the resulting call fallthrough is strictly inside a verified executable
owner. An ordinary instruction can use a qualified single-step backend. A call
requires an owned continuation plan: return to that exact fallthrough in the
same physical activation, not merely the same function number or PC. A hit in
a recursive activation must not complete the outer plan. A tail branch is not
classified as a returning call.

For `finish`, recover the selected physical frame's saved return PC and caller
CFA from exact unwind rules and bounded native stack data. Authenticate the
caller code owner independently. The selected frame disappearing through
exception unwind, stack switching, checkpoint restore or process exit is a
different result from normal return. Physical tail-call behavior must be
documented separately from logical Wasm frame behavior. V1 should refuse an
unprovable caller instead of guessing a native frame chain.

Every armed plan retains participant/native-thread identity, originating
capture/stop incarnation, code-owner generations, runtime epoch, actual stack
incarnation and a backend-owned continuation witness. CFA equality by itself
does not protect against stack-address reuse. A trap PC match by itself does
not protect against recursion or another thread. Replacement, restore, reset
and observer retirement must invalidate or drain the plan before any owner is
released. Native addresses in replies remain display origins, not subsequent
read or breakpoint capabilities.

Typed returned values are a separate ABI problem. A physical return and valid
return register do not identify the source-language type, multi-value Wasm
layout, aggregate result or reference-root lifetime. Keep the returned value
explicitly unavailable until its actual ABI/type/root contract is implemented.

## Actual CFI capture

LLVM23's real APIs include `DWARFDebugFrame::parse(DWARFDataExtractor)` and
`llvm::dwarf::createUnwindTable(FDE const*)`, returning an `Expected<UnwindTable>`.
`UnwindRow` supplies CFA and register locations. This is a parser, not an
authorized native-memory evaluator. A first implementation can admit validated
register/offset and CFA/offset rules, copying any stack word only after checked
range arithmetic. Unsupported expressions, register rules, stack switches,
signal frames and return-address authentication must produce unavailable, not
an unchecked read. Actual unwind support may extend this set after tests.

The loaded `.eh_frame` bytes and actual loaded section base must belong to the
same exact code owner. PC-relative FDE decoding needs that real EH section base.
The checked LLVM23 `DWARFObjInMemory` constructor assigns a frame section's
address from the original object even when LoadedObjectInfo supplies relocated
contents. Therefore the current line-only DWARFContext cannot automatically
prove loaded CFI. Before consuming its EH frame accessor, test its actual
loaded-byte/base relationship or parse an independently owned frame image with
the explicit actual base. This observation is not an asserted running-runtime
failure: the current native line collector does not consume EH frame rules.

Windows COFF uses a different path. The checked RuntimeDyldCOFFX86_64 source
registers `.pdata` with its memory manager; its `.xdata` references use the real
image-base relationship. Neither line tables nor a DWARF-only FDE parser prove
Windows native caller recovery. The exact Windows unwind table owner and ABI
need an independent qualification path. Compact unwind and authenticated return
addresses on macOS likewise need actual owner/format handling.

## Backend choices

Prefer one thread-specific temporary execution breakpoint to instruction-by-
instruction tracing through a called function. It avoids modifying shared JIT
text and avoids a trap for every callee instruction. It is a new capability,
not a relaxation of the existing one-instruction owner gate.

| Backend | Candidate mechanism | Required proof before enabling |
| --- | --- | --- |
| Linux | Thread-targeted `PERF_TYPE_BREAKPOINT`, execute breakpoint and synchronous perf SIGTRAP | Actual kernel support/permissions/slot admission; actual event cookie, thread and signal provenance; disable/drain before descriptor retirement |
| Windows x64 | Exact retained thread and private debug-register slot | Actual VEH saved-context versus suspended-thread behavior, kernel restoration of debug registers, hardware-trap identification, original-state restoration |
| macOS x64/AArch64 | Exact retained Mach thread and debug-state slot | Actual debug-state flavor/count, allowed control bits, entitlement/kernel behavior, authentic exception route, original-state restoration |
| Other OS/architecture | Unavailable initially | Fresh native backend and continuation qualification, not only object decoding or user-mode QEMU |

Linux's UAPI exposes synchronous perf-event SIGTRAP and event-identifying
`sig_data`. The event must target the exact native thread, with inheritance
disabled; it must never monitor arbitrary host memory. The handler must validate
the actual perf signal kind and event identity before reading a session. An
unrelated SIGTRAP must follow the existing forwarding contract. Kernel/API
availability and resource exhaustion are explicit unsupported outcomes. Use a
`fast_io::native_file` for the returned descriptor, and syscall result handling
through a documented noexcept ABI. These UAPI fields do not prove that a kernel
accepts the requested execute breakpoint. See the [Linux perf manual](https://man7.org/linux/man-pages/man2/perf_event_open.2.html)
and [kernel UAPI](https://github.com/torvalds/linux/blob/master/include/uapi/linux/perf_event.h).

Windows currently refuses preexisting enabled DR7 slots. Preserve that policy
until shared debug-resource ownership is explicit. A thread blocked inside this
VM's VEH wait is not equivalent to a kernel debugger stop: manager-side
GetThreadContext can describe the handler, not the saved guest trap. The future
mechanism must distinguish those contexts, and cannot write general registers
through the immutable snapshot API. Microsoft requires suspension when using
SetThreadContext on another running thread; its old vectored-handler sample
also explicitly excludes 64-bit qualification. A real Windows VM component must
establish which private context operation restores a hardware breakpoint.
Retain actual thread/event owners through release, observed handler retirement
and clear. Use fast_io NT ownership and typed noexcept ABI declarations. See
[SetThreadContext](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setthreadcontext)
and [Microsoft VEH example limitations](https://learn.microsoft.com/en-us/windows/win32/debug/using-a-vectored-exception-handler).

The checked Apple SDK defines `x86_debug_state64_t` and `arm_debug_state64_t`
with their exact flavor counts. The current backend already checks debug-slot
availability and retains the original debug state. A new execution breakpoint
must use those actual typed counts, preserve unowned fields and restore every
owned bit before releasing a Mach thread owner. General-state writes or debug
bits in exception replies cannot be generalized across x64 and AArch64. Existing
entitlement restrictions and historical failed qualification remain recorded.
See Apple's [x64 thread state](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/i386/thread_status.h)
and [AArch64 thread state](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/mach/arm/thread_status.h);
the local installed Apple SDK declarations were also read for this proposal.

Software text patching is a separate possible fallback, requiring a reviewed
memory-manager patch transaction, writable-alias/W^X policy, architecture-specific
instruction cache synchronization, actual all-execution drain and restoration
of exact original bytes. Thread filtering alone does not protect shared code:
other threads can execute a patched instruction. Neither MC classification nor
cooperative parking by itself authorizes changing JIT text. Do not implement a
raw `INT3`/`BRK` write or reinterpret hot replacement as a text-patching API.

## Lifetime and cancellation

A cold plan transitions from authenticated stop to armed backend, running and
matching stop. Completion validates the physical frame witness again. An
unrelated breakpoint/exception remains visible as its own stop reason. A
proved deeper recursive hit may be rearmed only through the same authenticated
backend transaction; unknown frame identity cannot be auto-continued silently.

Cancellation first disables the owned breakpoint, drains any actual callback
or trapped borrower, then clears the backend and releases its resources and
code/stack owners. Closing a pause domain or socket cannot replace that drain.
The current release-to-released-to-clear discipline remains mandatory, including
ready-to-arming races. Failure to prove callback retirement must retain ownership
or terminate using the existing fatal policy; it cannot destroy a borrowed
session. No handler can allocate, parse CFI, perform file IO, throw, recursively
enter a control API or close an owned resource.

Ordinary LLVM-full execution emits no new calls or polls for this feature.
Metadata collection and execution control are private debug-full cold paths.
Debug NI call overhead should be measured as well as correctness; a hardware
continuation should not accidentally retain instruction-by-instruction TF
tracing after a call starts.

## Required real tests

The keeper must use fresh paired runtime/main/host TUs and exact source/SDK
identities. A model test for a plan enum or MC call flag is not execution proof.
Begin with a native assembly component that performs a real call and return,
then require the actual LLVM-full controller and generated Wasm bridge.

Cover direct/indirect calls, recursion hitting the same continuation address,
two threads sharing code, leaf and framed functions, optimized prologue/epilogue
locations, tail calls, throwing/catching and escaping exceptions, nonreturning
calls, peer loss, close during arming, reset/replacement/restore invalidation,
foreign signal or enabled debug slots, resource denial, one-past/out-of-owner
continuations, and byte-identical or debug-state-identical cleanup. Values must
remain unavailable unless their independent ABI contract is qualified.

Linux native kernels, the Windows VM and future macOS tests are separate results.
COFF/MachO object reading and QEMU user-mode execution do not qualify another
kernel's breakpoint/CFI/handler lifecycle. No author-host compile or native test
was performed for this proposal. A production backend requires root approval
and the actual keeper evidence before any availability gate or CLI command is
enabled.
