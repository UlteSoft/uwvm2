# Thread Utilities

This directory implements UWVM2's lightweight batch scheduler for CPU-bound parallel work. The design combines native threads with C++20 coroutine handles, but it is intentionally much smaller than a general-purpose async runtime:

- Native threads provide parallel execution for coarse-grained tasks.
- Coroutines provide a cheap, movable representation of deferred work.
- Scheduling is batch-oriented: the caller prepares a fixed set of tasks, then runs the whole batch once.

In the current codebase this mechanism is primarily used by the UWVM interpreter translation pipeline to compile groups of local functions in parallel. See `../../runtime/compiler/uwvm_int/compile_all_from_uwvm/translate/single_func.h`.

## Core Building Blocks

### `native_global_typed_allocator_buffer<T>`

`native_global_typed_allocator_buffer<T>` is a minimal RAII buffer wrapper around `fast_io::native_typed_global_allocator<T>`.

- It allocates raw storage for a fixed number of objects.
- It is move-only, which makes ownership transfer explicit.
- It is used for both coroutine-handle storage and worker-thread storage.

The type avoids depending on heavier containers for a very small scheduling substrate and keeps allocation behavior explicit.

### `scheduled_task`

`scheduled_task` is the coroutine-facing task wrapper.

- `initial_suspend()` returns `std::suspend_always`, so creating a task does not start executing it.
- `final_suspend()` also returns `std::suspend_always`, so the scheduler regains control at completion and can destroy the frame explicitly.
- `release()` transfers the raw coroutine handle into a batch.

This means a coroutine is used here as a deferred execution frame, not as a long-lived async workflow. A task is created, queued, resumed once by the scheduler, and then destroyed.

### `scheduled_task_batch`

`scheduled_task_batch` owns a fixed-size array of `std::coroutine_handle<>`.

- `handle_count` tracks how many entries are live.
- `resume_and_destroy(i)` resumes one coroutine and destroys its frame immediately after it reaches `final_suspend`.
- `resume_and_destroy(i)` also enforces the one-shot contract: a task must reach `final_suspend` on its first resume.
- `resume_all_serial()` executes the whole batch on the current thread.
- `clear()` destroys any still-owned coroutine frames, which makes the batch exception-safe and leak-safe.

The batch is immutable once scheduling starts: workers only read handles and claim indices atomically.

### `native_thread_pool`

`native_thread_pool` is the execution engine. Despite the name, it is not a classic always-on pool with a shared queue and background workers.

- `run(task_batch, extra_worker_count)` executes one prepared batch.
- `extra_worker_count` means "additional worker threads besides the caller thread".
- The caller thread also participates in execution, so the maximum useful extra worker count is `task_count - 1`.
- `join_all()` is used both for normal cleanup and for fallback paths.

This makes the implementation simple and predictable for coarse compile tasks.

## Scheduling Flow

The end-to-end execution model is:

1. Higher-level code creates one coroutine per task group.
2. Each coroutine returns a `scheduled_task`, which is still suspended because of `initial_suspend()`.
3. The producer moves each handle into a `scheduled_task_batch`.
4. `native_thread_pool::run()` clamps the requested worker count to the useful range.
5. If no extra worker is useful or available, the batch runs serially on the current thread.
6. Otherwise, the pool spawns `extra_worker_count` native threads.
7. Those threads and the caller thread all execute the same worker loop.
8. The worker loop claims the next task index with an atomic `fetch_add`.
9. For each claimed index, the scheduler resumes the coroutine and then destroys its frame.
10. When no indices remain, all workers exit and the pool joins them.

The important consequence is that the scheduler is lock-light:

- There is no mutex-protected task queue.
- There is no condition-variable wakeup path.
- The only shared scheduling state is `next_task_index`.

This is a good fit for a fixed batch of medium or large CPU-bound tasks where task creation happens up front.

## Why Coroutines Fit Here

The coroutine layer is being used as a structured task frame:

- A task body can use normal local variables and structured control flow.
- The scheduler receives a uniform `std::coroutine_handle<>` regardless of the task's concrete function.
- Ownership and destruction are explicit, which matters when tasks are moved across threads.

At the same time, this is not a cooperative multitasking runtime:

- Tasks do not currently `co_await` intermediate scheduler events.
- There is no reschedule/yield queue.
- A scheduled task normally runs from its first resume to `co_return` in one shot.

So the "coroutine scheduling" in this directory is best understood as batched coroutine-frame dispatch, not a full async executor.

## Failure Handling and Fallback Behavior

The implementation is deliberately conservative.

- If `fast_io::native_thread` is unavailable on the target platform, the code falls back to serial execution.
- If the requested parallelism is not useful, it also falls back to serial execution.
- If worker-thread construction throws, the current thread drains work, joins already-created workers, and then finishes any remaining tasks serially.
- `scheduled_task::promise_type::unhandled_exception()` terminates the process, so task bodies that need recoverable error handling must catch locally and publish failure through shared state.

The current compile pipeline follows that model: each task catches expected failures internally and reports them through atomics plus a shared error slot before the caller rethrows at the batch boundary.

## Current Integration Pattern

The main production use is in the UWVM interpreter full-translation path:

- The compiler splits local functions into task groups.
- Each group becomes one `scheduled_task`.
- The task batch is executed by `native_thread_pool::run()`.
- The caller thread participates as a worker, which reduces idle time when the batch is small.

This model gives UWVM2 parallel compilation without introducing a heavyweight runtime dependency or a persistent scheduler subsystem.

## Design Trade-offs

Advantages:

- Very small implementation surface.
- Explicit ownership and destruction.
- Good fit for batch-parallel compilation.
- Low scheduler synchronization overhead.
- Clear serial fallback path.

Trade-offs:

- Not a general task system.
- No persistent worker threads across runs.
- No dynamic submission while a batch is running.
- No work stealing, priorities, cancellation, or suspension points.
- Correctness for shared writable task state is the responsibility of the caller.

That trade-off is intentional: UWVM2 only needs a dependable, low-overhead way to fan out coarse-grained compile work, and this design stays close to that requirement.

## Owner-scoped execution and blocking waits

`execution_lifetime` is a separate, bounded admission/lifetime primitive for work
executing on host-owned threads. Its move-only lease holds one admission until
release. Stop closes admission and exposes a cooperative stop flag; drain waits
for all admitted work to leave. It neither detaches nor forcibly terminates an OS
thread, and it does not create Wasm threads. The owner must stop new callers before
destruction and must never drain while retaining its own lease.

`keyed_wait_set` is a generic blocking registry keyed by `(resource identity,
position)`. It has 64 mutex shards and stack-owned waiter nodes. The value
comparison and queue insertion use the same mutex as notification. Notifications
select at most the requested number of matching, not-yet-selected waiters; host
spurious wakeups do not count as notifications. Each node has its own event, so
unrelated addresses are not woken by a notification. Timeouts use a monotonic clock
and saturate at its maximum time point. Negative timeouts are unbounded.

An owning runtime first requests execution stop, then closes its wait set to wake
blocked executions, drains execution leases, and joins owned OS threads before
freeing their resources. Closing/draining the wait registry does not itself join
host threads or reclaim their resource identities. Identity reuse is forbidden
while a waiter can still reference the old resource. Imported aliases must use
the same owner's identity and position. These utilities contain no Wasm types and
are not used by ordinary load/store handlers.

Both utilities are available on targets with native thread support; unsupported
targets expose `has_keyed_wait_set == false`. Their current focused Linux
ASan/UBSan test is `test/0017.runtime/keyed_wait_set.cc`. Integration with Wasm
shared-memory declarations and wait/notify instructions remains separate work.

For resources shared across execution owners, pass the lease's cancellation token
to `keyed_wait_set::wait`. Stopping one owner then cancels only its waits; the shared
registry remains open for other owners. Stop callbacks take the same shard mutex
as comparison/enqueue to avoid lost cancellation wakeups. Callback unregistration
runs after releasing that mutex, and drain counts entire wait calls through callback
destruction, preventing either a callback/destructor deadlock or premature shard
reclamation. `matches()` must not request cancellation on its own waiting thread.
`tools/ci/run_wasm_threads_primitives.sh` runs the focused sanitizer test in the
required Linux cgroup, including cancellation of one of two sharing owners.


`execution_domain` owns reusable execution generations. `try_enter()` returns a
lease from the current generation; `request_stop()` closes admission and requests
cooperative cancellation. `reset(cleanup)` closes admission, cancels and drains
existing leases, runs cleanup, then publishes a fresh generation. Cleanup failure
leaves admission closed. Administration is serialized separately from admission;
no admission lock is held during callbacks, drain or cleanup. A reset/drain caller
must not hold a lease, and cleanup/cancellation callbacks must not recursively
reset or drain their own domain. Work that borrows resources must retain its lease
through callback unregistration and final cleanup. Stop flags are not forced
termination or synchronization for otherwise racy user data.

`stop_and_drain(callback)` keeps maintenance ownership through a dependent
producer-shutdown callback. A concurrent reset cannot reopen execution between
lease drain and producer shutdown. The callback may query admission or request
stop but must not reset/drain this domain; a throwing callback leaves it closed.
The focused sanitizer test exercises shutdown/reset serialization and recovery.

Both runtimes use this domain at the outermost full/lazy/public-raw host execution
boundary (ROS has full/public-raw only). Supported nested raw callback entry reuses
the outer lease. Reset waits until thread-state cleanup and native-stack restoration
finish before destroying code and module registries. The host still owns and joins
its OS threads and synchronizes external loader/configuration mutations. The
runtime's stop API can be called by a host callback without waiting for itself;
reset from an active callback remains fatal. No lease operation is added to guest
function dispatch or ordinary memory loads/stores.

Both runtimes additionally expose `runtime_stop_and_drain_host_api()` for host
administration. It cooperatively closes and drains outer executions, joins the
ordinary product's compiler schedulers, and flushes accepted cache writes. Code
and registries remain alive; admission stays closed until explicit reset. It
rejects synchronous invocation from execution or compilation/provider callbacks,
which would otherwise wait for themselves. The process-owned cache service stays
idle and reusable, avoiding synchronous disk writes after a later reset. External
module storage still requires reset before destruction, and the host still joins
its own native threads. This is not debugger suspension or forced interruption.

The focused `execution_domain.cc` test exercises cancellation, draining before
cleanup, closed admission during replacement, distinct cancellation generations
and exception recovery under ASan/UBSan. `wasm_execution_domain.cc` runs real
interpreter and LLVM entry points, preserves values across `atomic.fence`, resets
while an import callback is live, reenters generated code from that callback, and
verifies that replacement generations execute. Its two-thread barrier verifies
concurrent full execution in both instruction and native-unwind modes. Ordinary
lazy entry tests additionally run independent cold compilations with two configured
workers. Full JIT publishes immutable unwind maps before execution; ordinary lazy
unwind uses the snapshot facility below. The Wasm-specific shared-memory checks,
wait32/wait64/notify semantics and host-entry binding now live in
`runtime/wasm_threads`; the queue and generation utilities here remain independent
of Wasm. Host entry installs one wait domain/cancellation scope for the admitted
activation. Import aliases use stable native memory identity and offset, so memory
growth cannot orphan waiters. Allocator pins cover only comparisons, not sleep.
Only standard results 0/1/2 return to guest code. Host cancellation currently uses
the runtime's fatal trap path after releasing wait locks; recoverable activation
cancellation is not implemented. The OS thread remains owned by its embedder.


### Immutable publication and VM-owned compiler workers

`immutable_snapshot<Payload>` is a Wasm-independent metadata publication utility.
A nonmovable reader registers at an outer owner boundary, before entering any
signal/trap path. `acquire()` and `release()` use lock-free pointer atomics without
allocation or a reader mutex. The returned immutable borrow lasts until the same
reader acquires again, releases, or is destroyed. It is not reentrant. Writers
serialize copy/edit or replacement, retire old versions, and reclaim only versions
absent from all registered hazards. Failed construction/edit preserves publication.
The owner must outlive its readers; payload callbacks/destructors must not reenter
this store. `collect()` can reclaim retired versions after readers release.

The ordinary runtime keeps compact mutable unwind builders behind a writer mutex
and publishes entries and executable ranges as a single snapshot. Lazy host entry
registers one reader; callback reentry reuses it. Trap resolution uses OS TLS even with optional VM TLS caches disabled, and returns
an entry by value before releasing the borrow. Reset first drains execution, stops compiler
writers, then clears metadata. Full-mode lookup still uses its frozen vectors.
No per-function logical stack recording is added to native-unwind execution.

Lazy schedulers belong to the VM generation, not an individual host call. Returning
one call cannot destroy queues still borrowed by another. Reset stops workers after
host execution drain, and a guard registered at first entry drains/stops workers
before namespace-static caches and thread maps are destroyed at process teardown.
Explicit proc-exit shutdown remains a terminal operation, not concurrent reusable
administration. Generic scheduler `start()`/`stop()` require owner serialization
and drained external producers/helpers; `running()` publishes completed startup
atomically for concurrent deferred-start polling. It is not an ownership lease.

`tools/ci/run_immutable_snapshot.sh` runs ASan/UBSan and TSan tests in the Linux
cgroup. `native_thread_tsan.cc` also exercises concurrent readiness polling.

The Linux alternate-signal-stack cache retires once at thread exit, then uses its
trivial TLS marker to force uncached late host-destructor reentry. Only Darwin
rearms the resource-free TSD marker needed across TLV re-creation. This avoids
entering signal-stack interceptors after another runtime has retired thread state.
The runtime mmap trap callback is also published once before guest execution,
not rewritten by each concurrent host entry. These are lifecycle operations;
normal mmap load/store paths retain hardware protection without extra guards.
