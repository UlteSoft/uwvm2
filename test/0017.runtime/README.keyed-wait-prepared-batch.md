# Cold prepared keyed-wait nodes

Mandatory SOURCE prerequisite: `/tmp/uwvm-managed-wait-infrastructure-source-proposal-20261004-r2` manifest SHA256 `2b3f752755406e2e97b6fa27294f373bc88c7db43508f20e4c9c7c96c3dea9ab`. This successor uses that genuine suspended stack-wait API and preserves its winner arbitration. It is utility infrastructure only. No VM, source, GC, provider, or restoration authority is issued by a node index, scalar queue ordinal, batch object, or copied resource label.

`prepare_ordered_waits(span<prepared_wait_spec const>, quota<=4096)` first allocates all nonmoving heap nodes. It then holds the real 64 shard mutexes in fixed order, verifies all same-key pending ordinals are dense 0..N-1, checks all keys are new to the real registry and all active-call counts fit, and publishes nodes in source order. No memory comparison occurs. Ready-notified and ready-timed-out records are owned completion state and never enter the pending queue. Actual keys must be derived by the VM's private new-memory/source adapter and strong resource owner, not by interpreting a checkpoint's native bits.

`prepared_wait_batch::start_deadlines() noexcept` is one-shot and has no allocation or recoverable failure. A real startup publisher calls it only after all fallible source/seed/enrollment/session checks, and before releasing guest execution. It starts every finite remaining budget from one actual monotonic clock origin. An invalid caller order (twice, closed registry, aborted/consumed/active node) terminates as a HOST invariant failure; this method does not assert that a VM startup is closed. Pending records with zero remaining budget remain real queue nodes until actual notification/expiry arbitration: zero clock DATA does not forge an already-completed ready2. Consumption still checks notification before expiry.

`consume(index, actual_expected_key, suspension_policy, new_cancellation)` verifies the entire index/key and armed one-shot node before claiming it. Native consumer arrival order cannot alter the queue order. It consumes ready0/2 directly or waits using the genuine existing node; it never re-compares memory or removes/reinserts a pending node. Original notified/cancelled/closed/deadline winner order is checked before and after a suspension. Callback/listener cleanup finishes before active-call count release. A completed detached node conveys DATA only and cannot be consumed again.

`abort_unconsumed()` removes real unclaimed nodes and releases their actual active-call counts. Claimed nodes receive a real wake/abort request, then their own caller finishes queue and callback cleanup. This is not an OS acknowledgement. The actual owner must release/close the corresponding cooperative domain, cancel workers as required, and join consuming native threads before destroying the batch. The batch destructor removes unclaimed work and terminates if a live consumer remains, rather than waiting forever or freeing a live CV. The registry and every resource must outlive the batch. In particular, destroy or abort/join the batch before calling registry destruction or `close_and_drain`; prepared nodes correctly prevent a false drain result.

The FIFO requirements come from the official threads runtime and `notify` queue-head semantics:

- https://webassembly.github.io/threads/core/exec/runtime.html
- https://webassembly.github.io/threads/core/exec/instructions.html
- https://webassembly.github.io/threads/core/exec/relaxed.html

Future actual VM integration still needs genuine sealed wait-site/operand/root ownership, final-ALL-stop fresh queue inspection, controlled state schema/identity, and a true current-world publisher. These utility methods do not replace those guards and do not qualify a complete VM restore.

## Finite keeper recipe

After ROOT applies the prerequisite and this exact successor into an admitted product cut, compile `test/0017.runtime/keyed_wait_prepared_batch.cc` using the actual same toolchain/include flags as `keyed_wait_suspension.cc`; record full cut/source/compiler/MD/product pins. Run with a finite 30-second external deadline in the original SSH Linux cgroup. Both products require fresh compilation. Example:

```
clang++ -std=c++23 -O2 -pthread -DUWVM=2 -fno-rtti -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -MD -MF keyed-wait-prepared.d test/0017.runtime/keyed_wait_prepared_batch.cc -o keyed-wait-prepared
```

On success stdout starts `PASS prepared wait batch:`. The actual fixture covers reversed native arrival with source-head notification before its consumer starts, a real pause census confirming the first native arrival still waits, real queue borrowing, ready0/2 without insertion, wrong owner/index/unarmed/consumed refusal, true claimed/unclaimed startup abort, quota and malformed order, genuine close-and-drain counts, zero-budget pending notification/expiry arbitration, and a finite remaining budget that starts after preallocation. Re-run the existing ordinary keyed-wait regression and the prerequisite real suspension fixture once. SOURCE owner has run no compiler, native test, SSH command, or VM restore.
