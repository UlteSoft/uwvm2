# Core 3 exceptions, full-JIT debugging and function replacement

This remains the contract for unfinished debugger and replacement operations.
Numeric cross-function Core 3 exceptions now execute in the interpreter and native
LLVM paths; retained exception references and GC integration remain unfinished.
The products share bounded host control/session, a Linux launcher-channel
adapter, and cooperative LLVM-full safe points. An interactive `-m debug-jit`
console now starts before guest entry and supports exact Wasm-offset breakpoints,
cooperative pause/resume, instruction stepping, bounded waits and owning guest-thread
backtraces. Linux startup seals console input before guest file-table creation;
WASI stdin has zero read rights and final `path_open` identities are checked before
truncation. Ordinary uwvm2 passed an actual ten-process CLI run and five-process
input-isolation run at source ID `sha256:33ef5de2da8dc956a4fcda10fe2b79324261d36ae9c0e27dabf6227f3d9a3ab2`.
ROS has also passed the matching focused controller/input checks and an actual
six-process CLI run plus five-process input-isolation run with the genuine pinned
LLVM `23.1.1-uwvm-ros.9` x86-64 libraries at source ID
`sha256:8a2efbc399983bb7c94b315f3d4c41fe5905bdfd2f556ea5558f31a19f4f684a`.
A stopped-only, bounded linear-memory read is now wired through the host
controller and real CLI in both products. The console limits a read to 256 bytes;
the separately authenticated protocol caps one request at 64 KiB. A general
server, late attach, locals inspection and function replacement remain to be
implemented. No WASI API grants
management authority.

## Host authorization and activation

`-m debug-jit` selects LLVM full compilation and starts at a stopped entry point
with an interactive host console. Late activation requires a host option chosen
before guest execution. A normal full-JIT run without this option has no control
endpoint, listener, breakpoint instrumentation or replacement entry indirection.
Other runtime modes reject debugger/replacement options using the existing fatal
diagnostic, identifying the selected unsupported mode.

The preferred control transport is an inherited, host-only connected socketpair.
Its descriptor is close-on-exec and is never inserted into a guest descriptor
table, argument list or environment. The launcher retains the peer and can expose
an authenticated server. An optional filesystem Unix socket requires a private
directory, peer credentials, an instance capability and explicit startup opt-in.
The VM must reject a connection originating from its own process, including other
guest threads. Same-UID credentials alone are insufficient authorization.

Do not put an attach capability in a directory reachable through guest preopens.
An inherited capability avoids relying on filesystem permissions against a guest
running under the host's UID. A PID and a signal are identifiers/events, not
authorization. Remote control goes through an authenticated transport, such as
the launcher's SSH channel; an unauthenticated loopback TCP listener is unsuitable.
Native host plugins already execute with host authority and remain trusted.

The protocol carries an instance identifier, generation, bounded message length,
request identifier and explicit operation. It exposes validated guest entities:
modules, functions, Wasm offsets, locals, globals and bounded linear-memory ranges.
It must not offer arbitrary native memory writes, arbitrary native calls, or
unrestricted PC/register mutation. Failed authorization must not pause the guest
or initiate compilation. Disconnects release session resources and leave the VM
in a documented stopped/running state chosen by the host.

## Stopping and machine-code ownership

LLVM provides debug metadata, object emission, disassembly and unwind information;
it does not provide a complete interactive debugger. The VM owns command handling,
thread coordination, source mapping, breakpoints and object lifetime.

A management thread may request a pause. A signal handler may capture a supported
native context and park a thread only when the PC belongs to published guest code
and the runtime is outside publication, allocator and unwind-registration critical
sections. It may not allocate, format output, lock, or run LLVM. A thread currently
inside a host operation records a pending pause and stops at an eligible boundary.
Signal delivery by a guest must not create an authorized management session.

Every published PC range is associated with module identity, function index, Wasm
offset mapping, generation, debug metadata and unwind records. Breakpoint patches
require all affected guest threads stopped, instruction-boundary validation,
write/execute permission transitions and target instruction-cache synchronization.
No page may be writable and executable simultaneously. Optimized-out values must
be reported as unavailable rather than read from an obsolete stack location.

## Function replacement transaction

Replacement is eligible only for LLVM full runs launched with replacement/debug
control enabled. Such runs use stable function entry slots and prevent inlining
or direct-call shortcuts that would bypass the replaceable entry. Normal full-JIT
runs retain their existing direct-call optimization. The performance difference
must be measured and attached to the enabled control option.

1. Resolve the original module and function under a generation token.
2. Validate the replacement body in the original module's type/import/table/tag
   context. Compare canonical parameter/result types, reference nullability and
   heap types, and the complete native calling convention. Equal byte size or
   arity is insufficient. Reject additions to module state or changed imports.
3. Compile and verify code, relocations, PC maps, debug records and unwind records
   without modifying the currently published function.
4. Stop affected guest threads and recheck the generation token. Register the new
   metadata before publishing the callable entry with release semantics. Update
   all supported call routes through the same stable entry, including function
   references and indirect calls. Failure leaves the old generation callable.
5. Resume execution. Existing frames finish their old generation; subsequent
   calls enter the new one. The default operation does not rewrite active frames.

An entry epoch held across a host-to-guest invocation can protect every generation
that invocation may still execute. Retirement also waits for debugger snapshots,
exception continuations and outstanding unwind walks. Code, PC maps and FDEs are
retired together. This lifetime mechanism must not introduce instruction-style
frame pushes/pops into the native-unwind policy.

## Exception handling and diagnostic stacks

Core 3 guest exceptions are distinct from VM traps and host failures. Memory OOB,
integer traps and validation errors must not become catchable guest exceptions.
Tag matching uses instance identity and the validated payload type, including
imported tags. `try_table`, `throw` and `throw_ref` follow the current Core 3 rules;
legacy proposal `try`/`catch` syntax is not a substitute.

WAVM's local `Lib/LLVMJIT/EmitModule.cpp` and `EmitExceptions.cpp` are ABI references.
Its TLS singleton unwind record is not suitable as the lifetime model for retained
Core 3 exception references. A guest exception value owns its tag and payload;
each active native throw needs a separately owned unwind activation. Catching and
retaining an exception reference, nested throws, and repeated `throw_ref` must not
overwrite an earlier activation or free a still-referenced payload.

The typed native landingpad identifies a guest exception, then searches every
lexical Wasm handler protecting that call in inner-to-outer order. A mismatch in
an inner `try_table` must still consider outer handlers in the same Wasm function.
Only failure to find a handler anywhere in that native frame permits ABI rethrow.
If a helper throws during the catch, cleanup balances the original native catch
and resumes the new landingpad record, which may refer to a different exception.

The owning value's canonical reference pointer and root token are not the packed
`wasm_global_ref_t` operand ABI. Backend transfer must preserve its reference kind
and retain the corresponding root; copying only pointer-sized bytes into an old
reference carrier is insufficient. Retained exception/tag identities must remain
valid after the producing activation and parser/module storage are retired.

Native exception dispatch and native stack inspection may share registered unwind
metadata. They have different control flow: inspection walks without consuming
an exception; exception dispatch runs matching handlers and cleanup actions.
Instruction stack mode needs cleanup on exceptional exits as well as normal
returns. Unwind stack mode continues to omit logical frame-maintenance calls.
Old code generations remain identifiable during both operations.

True Wasm tail calls remove the current activation and its handlers before
entering the callee. Interpreter opcode `musttail` dispatch by itself does not
implement `return_call`. LLVM typed C calls with incompatible parameter lists also
cannot simply be marked `musttail`. Backend ABI support, imported-call adapters,
argument lifetime, frame retirement and bounded stack growth require explicit
qualification on each target.

## Required acceptance cases

- Refuse control in every non-full mode, without a startup capability, from the
  guest's own process, with stale generations, malformed lengths, replayed
  requests and unauthorized peer credentials. Test actual guest attempts.
- Pause/resume while looping, during host calls, in recursive calls, during memory
  growth and concurrent compilation, and with all guest threads active. Include
  disconnect, timeout and shutdown behavior.
- Replace direct/indirect/imported-reference call targets. Reject ABI/type/import
  mismatches, invalid bodies and failed compilation. Exercise racing publication,
  active old frames, recursion, retained debugger snapshots and unwind walks.
- Throw through recursive frames and host boundaries; catch tagged and catch-all
  variants; retain exception references across nested throws and replacement;
  rethrow repeatedly; verify that fatal Wasm traps remain uncatchable.
- Inspect emitted code for instruction versus unwind policies and for disabled
  versus enabled control. Exercise fresh code and authenticated cache reloads,
  signed/unsigned arithmetic, memory access and all requested target ABIs.

Normative and implementation references:

- https://webassembly.github.io/spec/core/appendix/changes.html
- https://webassembly.github.io/spec/core/exec/instructions.html
- https://llvm.org/docs/ExceptionHandling.html
- https://llvm.org/docs/LangRef.html#call-instruction
