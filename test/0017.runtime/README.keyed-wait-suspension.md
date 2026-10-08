# Opt-in managed keyed-wait suspension

This utility SOURCE adds a real pause wake subscription and a suspension borrow for an already-compared, already-linked stack wait node. It does not issue VM checkpoint authority, interpret native addresses from a file, restore a VM, or claim an `atomic.wait` runtime integration. The original `keyed_wait_set::wait` and the enabled ordinary `cooperative_pause_domain::poll` bodies remain byte-identical.

`participant::register_pause_waker(context, wake)` is an intrusive stack-lifetime subscription bounded by the domain's fixed slot capacity. The subscriber registers before locking the external shard and unregisters after unlocking it. `request_pause` and `close` wake subscriptions while owning the domain mutex; wake callbacks only lock the external shard and notify its CV. They must not call the domain, a guest function, a provider, or request stop recursively. The subscriber pins domain drain even if its participant is reset first.

`keyed_wait_set::wait_suspendable` compares memory once, enqueues once, and keeps the real node linked while `Suspension::suspend` runs outside the shard lock. The policy's `requested()` is only a wake hint. The real policy must perform participant/source/queue/capture authentication before any VM observation. Returning false requests HOST cleanup; `{cancelled,true}` is returned only after the real node, shard lock, stop callback, pause listener, and active-call registration are released. This is never a fourth Wasm wait result and cannot be thrown through a `noexcept` Wasm bridge. Notification, original cancellation, close, and timeout are checked before a new suspension and immediately after reacquiring the shard, in the existing order. A still-pending HOST abort cannot override a winner selected during the unlocked suspension.

The noncopyable `suspension_borrow::inspect(actual_resource, visitor)` synchronously verifies the real linked node and actual key under its shard mutex. The noexcept visitor receives only copied position, pending same-key predecessor rank, remaining monotonic nanoseconds, and selected/cancelled/closed flags. No resource or native-node pointer escapes in the DATA snapshot. Rank is actual pending FIFO order, not OS arrival order, and a ready-notified or timed-out wait must not be restarted by repeating the comparison. This is consistent with the official threads runtime/`wait'`/`notify` semantics:

- https://webassembly.github.io/threads/core/exec/runtime.html
- https://webassembly.github.io/threads/core/exec/instructions.html
- https://webassembly.github.io/threads/core/exec/relaxed.html

Future actual VM integration still needs the sealed actual wait-site descriptor, strongly owned operand/root packet, same-episode queue census, genuine private abort invocation after cleanup, and new-world pre-registration in saved FIFO order. Those capabilities are not provided by a scalar snapshot or this utility-only fixture.

## Finite keeper recipe

Compile and run `test/0017.runtime/keyed_wait_suspension.cc` for each exact ROOT-admitted product cut in the existing SSH Linux cgroup. Use the cut's complete source fingerprint and real compiler/MD/artifact pins; do not overlay into an old binary. Example argv skeleton (keeper supplies actual toolchain and paths):

```
clang++ -std=c++23 -O2 -pthread -DUWVM=2 -fno-rtti -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -MD -MF keyed-wait-suspension.d test/0017.runtime/keyed_wait_suspension.cc -o keyed-wait-suspension
```

Run with a finite 30-second external deadline. On success stdout starts `PASS managed wait suspension:`. The real utility fixture covers two actual domain participants, two real FIFO queue nodes, notification while paused and exact notification counts, once-only matching, resource mismatch, finite deadline DATA, original cancellation and registry close, HOST abort cleanup and real notify/stop/close/deadline-versus-abort windows, zero-timeout/capacity/immediate results, empty participant refusal, and actual listener drain lifetime. It deliberately makes no full-VM restoration claim.

The existing `keyed_wait_set.cc` and `cooperative_pause_domain.cc` are appropriate finite regressions. If a sanitizer profile is already supported by the admitted toolchain, the same actual fixtures may run once under ASan/UBSan. Module qualification must use the real existing two partitions and their primary module, not a fabricated imported implementation. No Mac compiler or native test has been run by the source owner.
