# Private LLVM-full checkpoint capture transaction, producer candidate 3

`runtime/lib/uwvm_runtime_checkpoint_thread_capture.h` is a real-runtime private
producer candidate. It reads the existing calling-thread activation and typed
shadow ledgers and checks actual canonical publications. It is not included by
the runtime TU yet, and neither a guest import nor a save/restore command is
added. Native compilation/execution and full instance acceptance are pending.

## Exact producer authority

`runtime_checkpoint_thread_capture::mint_current_before_park(ticket)` runs only
inside the genuine observer's synchronous before-park callback. It requires the
actual generated execution token, native participant, current requested private
ticket and retained configured control. The participant is not parked yet, so
the current domain must report that exact request incomplete. An old/canceled
ticket, a native-step trap, host callback execution or an external manager
calling the method outside this context cannot produce a capture.

The native activation ledger must be complete: no pending typed tail, foreign
host island or suspended native continuation. The typed shadow ledger must have
all frames materialized and no recorded unadapted host effects. A current
native operation cannot be inferred absent from an empty or displayed stack.

Each logical frame is checked against its actual activation incarnation,
parent, continuation, runtime epoch, dense module record and current function
generation. Before inspecting payload, the producer resolves the actual
published full engine/context, canonical initialized source and exact module
registry member. It then requires the sealed compiler plan to match both its
pointer and shared ownership control block, exact module/function/generation,
and the same retained immutable engine profile. Arbitrary sealed DATA is not a
substitute for the actual compiler publication.

The current original function plan has generation one. Function replacement
currently emits no new sealed checkpoint plan. Consequently an actual replaced
generation is rejected even if its original publication still retains a
checkpoint profile. Checkpoint-compatible hot replacement later needs a real
new plan/continuation dispatcher, cache identity and atomic publication; no
generation label or old plan can impersonate that code.

After provenance, the producer checks all exact typed original-index slots and
control layouts. The leaf must be materialized `before_opcode` at the actual
stop byte offset; every surviving caller must be materialized at its genuine
`awaiting_call_return` site. Entry locals copied at a later opcode are rejected.
Actual initialized values may strengthen a conservative nondefaultable local
proof; they cannot clear a proven-readable slot, fabricate an operand value or
legalize an invalid `local.get`. Unset locals have explicit uninitialized state
and cleared private native DATA, never a load of an uninitialized LLVM alloca.

## Roots, EH and the initial availability limit

The current producer deliberately declines any non-scalar native reference
before capture publication. Only null (with nullable declared type) and i31
(with the correct internal abstract heap envelope) can be carried without a
heap/external-resource root owner. Defined/imported function references, GC
struct/array objects, exception references and host/extern wrappers require
real native identity/type/store/root ownership and are currently unavailable
in this private producer. They are still represented by the complete portable
schema; schema coverage does not mean the native export path is available.

A canonical source/store shared pointer does not prevent individual objects
from being reclaimed. The future producer must register complete typed history
as independently owned roots before any collecting transaction can reclaim it,
retain these registrations through checkpoint retirement, and include all live
current native/static/EH roots. Strict TLS LIFO compiler frame roots cannot be
reused for persistent history. Actual GC/extern graph IDs and alias/cycle
relocation still need the real canonical export transaction.

Active static handler layouts and `exception_continuation` are rejected until
the real pending tag/payload/reference/native-owner producer exists. An empty
synthetic exception state is not accepted. These explicit limits prevent this
first source slice from masquerading as complete Core 3 instance capture.

## Private immutable ownership and one coherent manager transaction

The capture constructor and fields are private. It owns a detached copy of
typed logical frames, canonical source pins and sealed plan pins, its actual
ticket/control/participant/location/profile, and comparison-only publication
identities. It does not independently retain an engine or native stack. A
bounded 256-slot weak registry records the genuine owner/control block; no
live owner is evicted. A supplied shared-pointer alias is resolved against that
private registry before dereference. Allocation or validation failure publishes
no partial capture.

Only the private, nonexported `uwvm2::runtime::lib::runtime_checkpoint_coherent_manager`
is a friend. Its global-module `extern "C++"` attachment matches the actual
private host bridge; no anonymous duplicate class or public credential is used.
Its future transaction must use the following actual order:

1. Retain the genuine runtime-generation lease and resolve canonical private
   capture owners. Loader/reset replacement must not race those borrows.
2. Enter ONE pause-domain transaction that authenticates the current ticket
   episode and every live actual slot. Every slot must be cooperatively parked,
   not externally trapped and not released for native stepping. Match each
   private capture's own ticket/control, participant, location and generation
   to the actual domain slot while that same lock remains held.
3. Nonwaiting-close the independent counted native-host admission. Any active
   native provider/callback, sticky unknown operation or escaped native view
   rejects the transaction; never cancel/wait for the parked guest leases.
4. Under actual publication ownership recheck all canonical source/code/plan
   generations. Pin complete stores and actual registered root populations,
   static state, segments, tables, globals, memories and exception owners.
5. Perform the complete real guest FD/preopen/alias/ancestor/inherited/plugin
   capability census. Only that private actual issuer may obtain a protected
   asset transaction, immutable input, publication or restore staging rights.

The capture DATA, registry membership, `status::ok`, a public boolean, a numeric
epoch, `domain.capture` scratch or a successful counted host close cannot skip
any stage. The domain's general display stop accepts external native parks;
that display permission is insufficient for logical checkpoint continuation.
Callbacks must not recursively acquire the same domain or reenter guests.

## Real restore and replay obligations

Restore must validate a detached complete typed instance graph against actual
modules, reallocate/relink references and cyclic aliases to fresh canonical
tokens, and build every logical thread's locals, operands, control/EH/call/tail
continuations. Its independently published compiler resume dispatcher resumes
typed Wasm state at a sealed logical site, never a native PC/SP/GPR jump. A
single failure-atomic commit then changes actual instance generation and
invalidates all old native/source/capture handles. Code provenance cannot be
rebound from the old generation.

Unknown host imports remain non-replayable. Deterministic replay additionally
needs versioned real adapters for time/random/thread ordering/IO/resource
lifetime and side effects; recorded event DATA or a software execution trace
does not restore the external world. No complete save/restore/reverse/replay
feature is advertised by this producer candidate.

This contract follows the [Core 3 runtime configuration/store/continuation
model](https://webassembly.github.io/spec/core/exec/runtime.html),
[reference validity](https://webassembly.github.io/spec/core/exec/values.html#valid-ref)
and [function execution rules](https://webassembly.github.io/spec/core/exec/instructions.html).
[QEMU replay](https://www.qemu.org/docs/master/system/replay.html) and the
[rr replay paper](https://arxiv.org/abs/1705.05937) demonstrate the independent
external-input/event-order obligations. [CRIU's inherited-FD contract](https://criu.org/Inheriting_FDs_on_restore)
illustrates why a file-descriptor value cannot recreate external resources.
