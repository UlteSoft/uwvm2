# Private checkpoint host-admission foundation

This source-only component is not connected to runtime host entry, controller,
checkpoint capture, protected assets, publication, or restore. Ordinary memory
access and ordinary interpreter/JIT IR are unchanged. Merely importing the new
module does not allocate a gate or execute a probe. The future actual runtime
bridge is the only constructor/entry/closure friend; the standalone unit defines
a separate component-only bridge and must never link it with the runtime.

The gate tracks known synchronous host operations. It has no runtime execution
lease, pause ticket, source/code owner, file descriptor, guest census, or arbitrary
address. Its private closure token proves only that this exact gate rejects new
operations and has no counted operation. It cannot authorize checkpoint capture,
a protected registry transaction, publishing a file, or executable restore.

## Required actual producer and lock order

1. Keep the actual runtime generation lease and selected canonical owning source
   alive. Reject management callback/reentry and unknown native admission.
2. Request the real cooperative pause, then wait for actual enrolled guests. New
   participant admission is already blocked by that domain's real ticket.
3. Inside ONE `while_stopped` domain callback, try-close this independent host
   gate. This is nonwaiting. Any actual external entry setup, synchronous memory
   operation, foreign borrow, or suspended host continuation still owning an
   operation returns `busy`. Never wait for the whole guest execution lease.
4. With the short gate mutex released and the private closure still owned, take
   the actual publication guard. Verify the complete module cohort, canonical
   source/control blocks, runtime epoch and every function generation, actual
   non-external safepoint/complete materialization, and complete FD/import census.
   Only this future private runtime issuer may then mint an assets transaction.
5. Copy bounded data while that same real guard protects it. No metadata result,
   clean public report, numeric stop label, or gate token substitutes for step 4.

No host-gate lock is held across a domain callback, native callback, condition
wait, guest execution, compilation, registry allocation, or file write. The
brief acquisition order is domain -> gate mutex (release) -> publication. Any
ordinary external entry acquires/releases the gate mutex before enrolling in the
domain or taking publication, so it must not hold the gate mutex while blocked
in `cooperative_pause_domain::enter`.

Closing the host gate *before* requesting the guest pause is unsafe: a guest can
reach a host-call opcode after its last poll and block at the closed gate without
ever parking. The first slice instead pauses guests first and fails promptly
on an already admitted host continuation. A future replay adapter must prove a
real suspended continuation separately before changing that rule.

## Current real entry surfaces to integrate, not inferred coverage

`runtime_execution_entry_scope` owns the whole guest-entry lease;
`llvm_jit_call_raw_host_api` permits nested callback entry with that same lease.
`llvm_jit_debug_host_scope` gives a genuine debug-full host boundary and retains
existing C++ unwind cleanup, but currently supplies no host-admission operation.
`preload_memory_descriptor_at_host_api` can return `mmap_view_begin` and
`dynamic_length_atomic_object`; a synchronous call count cannot revoke these
escaped pointers. `preload_memory_read/write_host_api`, debug memory reads,
prepare/commit replacement, prepare/debug configure, reset/source replacement
and source ownership publication must be included or explicitly excluded by the
actual manager admission/census. Current external serialization comments are
not a runtime credential.

First runtime slice must decline native preload/plugin access, escaped memory
views, foreign/imported memory, opaque imports, unknown host setup/borrow,
active host continuation/reentry, externally parked native traps, and unknown
FD/preopen/dir-stack identities. Recording unknown host access is sticky for
this gate. There is no clearing API: only a genuine drained-owner rebuild can
create a different gate. This does not pretend to retract a raw pointer that
previous native code cached.

## Keeper tests and limits

The new standalone `checkpoint_host_admission.cc` tests real gate mutex/count
semantics, move ownership, busy active host setup, new-entry rejection while
closed, foreign guard rejection, sticky unknown access, last-serial exhaustion,
and safe gate-state retirement. It does not test runtime world stop. Build it
with the current paired `__1` headers and ordered libc++/libc++abi/libunwind
archives under the existing 64 GiB keeper cgroup; no old `__2` runtime object is
needed. Native execution and named-module compilation remain pending.

After integration, actual runtime tests must prove: all guests parked while a
foreign entry is still in setup => busy; nested raw callback parked inside an
old host import => busy; post-pause entry refused before any mutation; stale
ticket/resume/reset/replacement invalidates the private full transaction; raw
preload escape keeps checkpoint unavailable; actual full cohort/FD census is
required before file registration. Existing pause-only tests and this standalone
component cannot count as those runtime tests.
