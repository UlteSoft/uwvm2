This document records the ownership requirements for native `ni` and `finish`.
Linux x86-64 `ni` has a qualified current-caller near-call continuation backend;
see [NATIVE_HOST_CALL_CONTINUATION.md](NATIVE_HOST_CALL_CONTINUATION.md).
Native `finish` remains unavailable. Registered CFI and bounded owned-data rule
evaluation now feed a runtime-private live bounded multi-frame inspection query and
`bt THREAD` at a genuine native stop. A privately minted physical-return proof
now binds the authentic first caller/return target and original trap revision;
its public revalidation query returns only Wasm identities. An executable
cross-owner return event and new parent activation publication have not been
integrated. The component
scope is documented in [the registered CFI test](../../../../test/0017.runtime/README.debug-native-registered-cfi.md).

The expected behavior follows the [GDB continuing and stepping
manual](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Continuing-and-Stepping.html):
`ni` executes one machine instruction and steps over a function call; `finish`
continues until the selected frame has returned. A physical instruction step,
a source-line step and a Wasm opcode step retain separate policies and stop
identities. None may infer another layer's exact values from a previous stop.

Current evidence and missing evidence
------------------------------------

| Current input | What it proves | What it does not prove |
| --- | --- | --- |
| Runtime-private activation capture and code-copy API | Actual published code at a genuine stopped participant, with epoch/function generation and code lifetime guarded during copy | Executable code ownership after the copy transaction, current physical callers after native execution |
| `native_step_site.return_pc` | Resume PC immediately after the before-park bridge | Caller return address or caller stack frame |
| `with_owned_registers` | GPRs copied from the actual active native trap, selected thread and code range | Stack-byte read authority, vector/FP result registers or saved caller GPRs |
| `native_instruction_semantics::classify` | A complete owned-byte MC decode and descriptor flow/size properties | A branch target, indirect call target, caller return address or continuation identity |
| Native loaded line rows | Published native PC to a Wasm expression offset when exact | CFI, physical stack depth or guest custom-section authority over native memory |
| Existing debug activation ledger | Genuine debug-full entry/exit/tail/EH identities at a qualified cooperative capture | A signal-safe current ledger snapshot at an arbitrary native instruction |

The MC helper's `indirect_branch` describes branches, not indirect calls. The
helper checks MC operand/register bounds but does not export their identities.
No assembly mnemonic string is used as execution policy. LLVM exposes the
descriptor properties through [MCInstrDesc](https://llvm.org/doxygen/classllvm_1_1MCInstrDesc.html);
the exact API must also be checked against each product's actual LLVM version,
especially the ROS bundled LLVM 23 source.

Private plan creation
---------------------

The controller may request creation only for an authenticated LLVM-full debug
session, a real active native trap, the selected participant, a fresh stop and
an open all-guest pause domain. Public decimal thread/stop labels are stale
request checks; they never construct a plan, pointer or temporary breakpoint.
The runtime owns the complete plan. The command/server/DAP side receives only
an opaque revocable reference and an explicit capability/result status.

One creation transaction must pin the actual execution generation, stopped
participant, current/target native code publications and the true trap. It
copies the exact current instruction and immutable classification data. It
also captures any required stack bytes under a separate runtime-private
native-stack authority. A copied instruction by itself cannot authorize
execution after the transaction ends: the plan must retain the appropriate
execution/publication owner or abort before reset can reclaim it. Generation,
epoch and object identity are checked again before installing a stop.

The private plan binds:

- The host session/instance, actual native thread/participant, actual stop and
  runtime epoch; the selected physical frame and any Wasm continuation identity.
- The current and target immutable code owners, their exact intervals and
  function generations; the runtime-derived target instruction boundary.
- A genuine stack-registration owner, its allocation/guard extents and a bounded
  copied stack window; exact native unwind metadata owners and relocation ranges.
- The backend-owned stop resource, cancellation state and restoration receipt.

These fields are private runtime evidence, not a public aggregate callers may
populate. Creating a plan grants neither arbitrary address reads/writes nor
host expression evaluation. Invalid or incomplete evidence yields unavailable.

The runtime lock order remains execution lease, one pause-domain transaction,
publication and native transition. No caller nests another pause-domain guard
around the existing private copy API. Backend mutation and waiting must not
hold a publication/domain callback while waiting for guest execution. Every
pointer formed while copying stack or metadata is checked against its actual
owner before addition/subtraction, with the same bounds diagrams used by the
validators. No raw address is exported through plan serialization.

Native `ni`
-----------

For an ordinary or non-call control instruction, `ni` can use a qualified
one-instruction operation; it must preserve the existing rule that an unknown
or unowned destination is unavailable. Tail-call machine branches follow their
actual MC instruction semantics. A Wasm `return_call` is not reclassified as a
native call merely because its source syntax contains a call name.

For a decoded call, the candidate stop is the exact checked fallthrough
instruction in the current publication. Its address comes only from the owned
decode and real instruction boundary, with checked addition and owner bounds.
It is installed by a qualified backend, then only the selected participant is
released. Reaching the same PC recursively is insufficient: a caller-frame or
continuation witness must also match the original invocation. Stops caused by
an unrelated breakpoint, exception, signal, cancellation or owner retirement
take priority over successful completion.

A decoded call alone must not activate `ni`. The actual temporary-stop backend
and recursion/frame witness are necessary. A call into a blocking host helper
may never reach its fallthrough; deadline/peer-loss/close must still cancel the
owned stop and release the actual guest according to the backend contract.
No unbounded TF loop through arbitrary host libraries substitutes for this
missing execution capability. Backend choices are described separately in
[native_step_next_finish.md](native_step_next_finish.md).

Native `finish`
---------------

`finish` requires a genuine selected physical caller reconstructed from the
actual trap and native unwind metadata. Neither the saved cooperative trace,
the bridge resume PC, a frame-pointer guess nor SP ordering can supply it.
Inlining and source lexical frames remain distinct from physical native frames.

The first implementation should accept only the current physical frame and
explicitly decline unsupported unwind rules or a caller outside a retained
managed native owner. ELF/Mach-O CFI must come from the compiler's actual loaded
object/publication. COFF unwind metadata must come from the actual registered
function table and its owner. Guest `.debug_*` custom sections cannot supply
native unwind authority. LLVM's [RuntimeDyld](https://llvm.org/doxygen/classllvm_1_1RuntimeDyld.html)
provides loaded-object/relocation and EH-frame lifecycle interfaces. The current
Linux x86-64 backend uses the original registered manager for bounded physical
Wasm backtrace inspection; it has no executable cross-owner return-stop capability.

The true native stack extent is captured on its owning guest thread during
explicit debug entry, then retained under that thread's runtime registration.
Calling an OS current-thread stack query on the management thread would obtain
the wrong stack. Microsoft documents [GetCurrentThreadStackLimits](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadstacklimits)
and [RtlVirtualUnwind](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtlvirtualunwind);
those are platform inputs, not permission to use a caller-supplied host pointer.
The actual API/ABI wrapper and exception behavior must follow fast_io conventions.

CFI evaluation consumes an owned bounded stack copy and immutable native
metadata. Register recovery has explicit known/unavailable bits. Expressions
that need bytes outside an authorized copy, unknown registers, unsupported
pointer encodings or overflow fail closed. A computed candidate return PC must
resolve into the exact retained caller publication and instruction boundary.
The plan must distinguish recursive invocations and reused stack addresses.

For Wasm tail transfer, the selected incarnation can retire while its genuine
continuation remains active. [WebAssembly 3.0 control semantics](https://webassembly.github.io/spec/core/syntax/instructions.html#control-instructions)
require tail variants to leave the current function before invoking their
successor. `finish` therefore needs actual continuation ownership through tail
transfer, not a source-function equality check. The current sealed kernel
witness accepts a completed bounded dynamic chain or a genuine leave transition.
It declines incoherent entry/exit or host-island states and does not inspect
mutable TLS. A finish return stop still needs to publish its new parent
activation through that protocol instead of reusing the departed child cursor.

An exception that removes the selected frame is reported as an exception stop
or a frame-exited-by-exception outcome, not a normal return. Return values need
a separate compiler-produced typed result capture or fully qualified native
ABI register recovery. The present GPR snapshot cannot recover arbitrary Wasm
multi-value/SIMD results; it must report unavailable rather than guess RAX/X0.

Cancellation and publication lifetime
------------------------------------

Temporary stops are owned resources. Domain close, detach, peer loss, timeout,
guest exit, reset and a rejected publication all run the same cancellation
protocol. The exact backend stop is removed/restored before its owners retire;
the genuine handler publishes release before the fixed session is cleared.
The ready-to-arming race must preserve ownership if cancellation loses that
transition. Failure to drain cannot silently discard ownership or report success.

Hot replacement cannot install a stop in a different generation by retaining
the same numeric PC. An active target frame remains ineligible for replacement.
Every old engine needed by an in-flight native plan is pinned until the actual
stop/cancellation protocol completes. The trap and CFI owners must agree with
the actual code owner, not only the original module's engine.

All of this is explicit debug-full machinery. Ordinary compiled IR must retain
no plan pointer, per-access memory guard, opcode map, extra branch or lock. Any
new compiler event is absent from non-debug builds. `musttail` still has its
required immediate return; no plan hook or result collection is inserted in
between. Native metadata publication and plan creation run on cold paths.

Qualification before exposing commands
-------------------------------------

Each product needs a new complete all-TU/macro closure and actual LLVM-generated
code. Header/component data tests, an MC call flag, a synthetic trap state or a
prior binary cannot qualify executable `ni`/`finish`.

1. Qualify private native-stack registration, ownership/alias rejection, stack
   bounds, loaded CFI/COFF relocation extents and bounded copied-byte recovery
   using real compiler outputs. Wrong-stack, stale epoch, unreadable aliases,
   unsupported rules and owner retirement must fail before a host dereference.
2. Qualify the backend stop installation/removal and cancellation with actual
   kernel traps, including real close/arming races. A concurrent stress run alone
   does not prove an exact phase was hit; deterministic private witnesses need
   separate review and may not weaken production ownership checks.
3. Test direct/indirect native calls, recursion with identical return PCs, Wasm
   `return_call`, `return_call_ref` and `return_call_indirect`, imports/host reentry,
   optimized prologues/epilogues and both instruction/unwind diagnostic policies.
4. Test Core 3 `try_table`, `throw`/`throw_ref`, normal returns, caught and uncaught
   exits, active-frame replacement rejection and retained-generation callers.
   Source stepping and variable reads must remain unavailable at an unqualified
   native PC even when display-only provenance has an exact line.
5. Test detach, loss of the authenticated transport, timeouts, domain close,
   reset and thread exit with a pending stop. Check actual guest progress and
   release of real backend resources; component destructors are insufficient.
6. Repeat on the qualified Linux, Windows and macOS backends with actual target
   code. Cross-compilation/QEMU support and absent OS facilities remain separate
   qualification results. Unavailable is not success.
7. Compare ordinary before/after IR, assembly, memory-access instructions and
   uninstrumented performance, then inspect debug-full call/tail/EH control flow.

Until those steps close, current native one-instruction stepping, code/GPR
inspection and display provenance retain their existing precise scope.

Protected physical-return wake foundation
-----------------------------------------

The sealed return proof now has a synchronous protected host resume entry.
It uses the same private CFI/stack/owner observation as inspection, but inside
the original external-resume borrow. Exact evidence is compared before the
callback; domain and publication survive through actual one-shot wake. No
commit retains the stop, a false wake restores domain accounting, and public
API reentry is refused before locking the domain. The callback exposes only
authenticated Wasm parent IDs, never RA/CFA or native stack bytes.

This foundational entry does not install a cross-owner event or publish the
parent as a new native stop. Native `finish` stays unavailable until those
transactions and their tail/EH/cancellation/actual-worker-ACK lifetimes close.
A positive contained-instruction wake must not be counted as return completion.

The 2026-10-06 protected-resume run completed 71 selected full-product,
fixture, native runtime and regression commands in the original 64 GiB
Linux cgroup for both repositories. Actual callbacks performed 1,148
independently proved contained-instruction wakes; no-commit and failed-wake
paths each preserved 1,148 real stops. Every later trap revoked its old
return proof. Full LLVM/interpreter closures and all 13,044 SDK pins matched,
and no new memory events occurred. This qualifies the proof/wake join only;
executable cross-owner ASM finish remains unavailable.
