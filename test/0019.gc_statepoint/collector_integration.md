# Exact GC roots and collection integration

This is the implementation checklist for replacing the current monotonic
`gc_object_store`. It is not a claim that the collector exists. The executable
RSS gate in `benchmark/0004.wasm3-core/release_check_gc.py` must continue to fail
until objects are actually reclaimed in both products.

## Object identity and collection boundary

`gc_reference` is a 16-byte tagged integer carrier, not an LLVM managed
pointer. The payload of a struct/array reference is a never-reused VM token.
The collector must resolve a token through a proven membership index before
dereferencing an object. Keep the token nonrecycling after sweep so stale or
forged references cannot turn into references to later allocations.

The present local membership buckets are lock-free and their object links are
never removed during execution. A nonmoving first collector can preserve this
fast read path by sweeping only after **all** enrolled guest and host bridge
readers reach a coordinated safe point. Sweep must unlink an unreachable object
from the owning list, local membership bucket, and process-wide foreign bucket
before its header or fields are freed. Foreign bucket stripes must be held
while unlinking; a reader that acquired an owner lease before the pause must
also be included in the quiescence proof. A per-object mark bit is insufficient
without that reader-lifetime protocol.

All stores participating in a cross-module object graph must be traced as one
reachability domain. An object's `value_leases` and a bridge's `inner_owner`
currently create strong ownership cycles, and replacing a reference field
does not release its old owner lease. Collection must remove those ownership
edges or replace them with roots derived from the live graph. Simply sweeping
one store at a time cannot reclaim a cross-store cycle safely.

## Exact roots at a safe point

| Root source | Current representation | Required collector view |
| --- | --- | --- |
| Interpreter activation | `execute_compiled_defined_once` owns packed locals and operand stack, with a live `stack_top` | Validated ref-typed local offsets and live operand offsets for the current bytecode PC; no scanning numeric bytes as references |
| LLVM activation | Ref locals are LLVM allocas; operand values are 16-byte integer SSA values | Explicitly materialize every live ref at allocation and Wasm/host call sites, including operands below call arguments and cold control-flow edges |
| Tiered transfer | Interpreter frame and JIT OSR local snapshot | Keep both sets rooted while ownership changes; retire the old frame only after new roots are published |
| Module storage | Defined/imported tables and globals, active/passive element payloads | Read the complete tagged reference under the same publication protocol as execution |
| Exceptions | In-flight exception payload, exnref token registry, caught/rethrown value | Trace aggregate refs inside the payload throughout cross-frame unwinding and host reentry |
| Host boundary | Imported/exported callbacks and returned references | Explicit scoped root/handle ownership, including a host callback parked while another thread collects |
| Threads | Start arguments, live frames and `atomic.wait` sleepers | All registered guest threads must publish roots and acknowledge the same collection epoch; a blocked waiter must remain rooted without needing to execute an instruction |
| Debugger/hot replacement | Paused frame and old JIT generation | Keep the frame and its code-generation root map together until the final executing frame retires |

The present LLVM experiment in this folder proves that an ordinary integer
carrier is *not* discovered by `rewrite-statepoints-for-gc`; a separate
`llvm.experimental.stackmap` experiment records the two integer halves, but
does not bind a runtime call's return PC to a map or recover caller-saved
registers. A production strategy must either use frame-owned root slots that
are visible throughout an allocator call, or prove complete stack-map register
recovery on every target ABI. A stack map immediately before a call does not by
itself make the registers available inside the callee. Ordinary numeric and
linear-memory code must not pay a GC-root or pause cost when GC is disabled.

The existing debugger `cooperative_pause_domain` demonstrates enrollment and
admission closure, but its debug-only polling is not a GC safepoint. A collector
triggered by an enrolled allocating thread must exclude or explicitly account
for that initiator when waiting for all other participants. Loop backedges,
calls, allocation slow paths, thread waits, and host callbacks need a bounded
handshake. A host call that never returns may delay collection; the allocator
must fail safely rather than freeing objects under it.

## Graph traversal and evidence

Use validated struct field and array element types to visit only reference
slots. Packed integers, float/vector bits, and i31 values cannot become
object edges. Preserve foreign object identity while checking its issuer and
kind. A first implementation may be nonmoving, but must show bounded RSS and
measured pause/throughput before choosing a more advanced layout or barrier.
The [Wasmtime 49.0.1 copying implementation](https://github.com/bytecodealliance/wasmtime/blob/v49.0.1/crates/wasmtime/src/runtime/vm/gc/enabled/copying.rs)
uses Cheney semispaces, bump allocation and no read/write barriers. It is a
useful performance reference; its speed cannot substitute for this VM's root
and lifetime proofs. Bind comparisons to that source tag and the tested CLI;
the generic documentation may describe a different development version.

Every root-survival fixture (local, operand below call, table, global,
exception, many locals, host callback, and sleeping worker) must force and
count at least one collection while the tested reference is live. Also require
cyclic and overwritten cross-store graph reclamation, stale-token rejection,
concurrent readers, sanitizer runs, and the 1M-to-10M bounded-RSS gate in
interpreter full/lazy and JIT full/lazy/tiered as applicable. Compare generated
machine code for GC and non-GC memory access; keep hardware mmap protection
pages and avoid an extra per-access software guard.

Relevant primary references: [LLVM statepoints](https://llvm.org/docs/Statepoints.html),
[LLVM stack maps](https://llvm.org/docs/StackMaps.html), and the pinned Wasmtime
implementation above.
