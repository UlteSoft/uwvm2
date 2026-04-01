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
