# Native normal-return event qualification

`debug_native_return_event_runtime.cc` uses the real full interpreter plus full
LLVM product, genuine debug-full entry/leave hooks, registered engine CFI and
Linux x86_64 synchronous perf execute traps. It never substitutes a constructed
native stack, a callback invocation, or an accepted wake for a completed return.

The runtime issues `llvm_jit_debug_native_return_event` only while freshly
revalidating the opaque physical-return proof in the original domain/publication
transaction. Its backend transition is private, bound to the exact origin
kernel PC/SP/revision and the issuing host callback. A retained uncommitted
plan cannot be executed later or from another host thread. The parent provider
retains the child cursor and authenticates its genuine normal leave, exact
parent ledger prefix, private CFA and owned kernel PC before copying registers.
Host leave reports, typed/host tail exits and exception exits cannot substitute
for that normal-return witness.

The event query exposes only the authenticated Wasm parent IDs. It requires the
original ticket, unchanged publication, precise next trap revision and real
parent witness. The old child capture refuses native code at the parent stop;
its copied source locals are never promoted into a new parent snapshot. The
provider, strong sealed plan and owned event FD remain live through disable,
actual worker release ACK and session clear.

This stage qualifies a host-private kernel return event. ASM console `finish`
remains unavailable until fresh parent capture/stop publication, subsequent
steps and cancellation/EH/tail recovery have their own complete qualification.
This implementation is gated to Linux x86_64 LP64; no other OS/architecture
qualification follows from this fixture.

Normal Wasm returns use actual LLVM tailcc, which may emit RETI64 to pop an
outgoing argument-area reservation. The issuer decodes the complete bounded
owned child body with its actual target MC provider, requires one consistent
supported near-return adjustment, and seals CFA plus that adjustment as the
exact kernel return SP. Unsupported/inconsistent/incomplete return decoding
refuses issuance. No guessed ABI constant or relaxed stack match is accepted.

The exported native session remains attached to its backend module. The closed
backend entry takes an opaque identity and compares the actual active session
before dereferencing it. Issuer/registers/backend module component probes check
this interface; they do not qualify the full runtime named-module build.

A completed return can now mint an independent native-only parent capture and
register the sealed parent cursor. Its exact kernel revision, normal leave,
current ledger prefix and owned publication are rechecked under one transaction.
Both bounded registries publish together or leave the original event unchanged.
A repeated mint returns the same canonical owners. The private original domain
anchor does not become a cooperative parent/source/locals snapshot.

Code windows, whole-function images, native provenance and bounded physical
backtraces use the new parent's own identity and actual trap. Subsequent SI keeps
the same witnessed cursor; stale event revisions cannot remint. The fixture
requires positive actual parent SI, recursion and hot replacement, plus wrong
session, alias, unexecuted-event and real worker ACK refusals. The console stop
publication/finish command and cancellation/EH/tail matrix remain separate.

Chained normal returns must distinguish an event's originating native-only
capture from the new capture published for that event. An original capture's
domain anchor alone is not a publication marker: it may already be anchored
after an earlier return. Publication compares the sealed originating capture
with the event provider's current capture, then verifies both canonical
registries; repeated publication returns the same owners.

The fixture requires multiple genuine consecutive parent returns in one
native session, with unchanged original domain ticket, exact kernel revisions,
registered CFI, normal leave and fresh ancestor code/provenance/backtrace.
Every departed parent, previous event, source/locals promotion and post-ACK
capture is refused. The actual Wasm root refuses host backtrace and host return
issuance. All event descriptors and sealed owners survive disable through the
real worker ACK before close. This does not enable ASM console finish or
qualify the remaining cancellation/EH/tail or other-platform matrix.
