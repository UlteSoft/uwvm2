# Native cohort retirement and bounded physical join

The host-only pair llvm_jit_checkpoint_retire_saved_native_workers_host_api(ticket, captures, deadline) and llvm_jit_checkpoint_continue_native_retirement_host_api(operation, deadline) retires the **current retained instance's actual registered native guest workers**. It does not restore an instance graph, publish replacement Wasm resources, resume saved frames, or roll back external I/O. Ordinary external threads are rejected because the runtime does not own their OS thread lifetime.

Begin accepts one genuine stopped full cohort and canonical owned captures. The manager creates the next execution lifetime before stopping, authenticates the exact current source/profile/code/host gate and all registered native workers, and seals the real retirement request under the original cohort/N/publication guards. The live-root policy permits cleanup of currently rooted aggregate/reference values; it does not permit replay of their native pointers. The existing scalar execution-continuation policy stays unchanged.

The result contains diagnostic data and an opaque canonical operation owner. Its public wrapper contains no native entry, VM resource address, execution lease, source pointer, stack image, or restore capability. Pointer aliases and foreign shared control blocks are rejected before reading any supplied pointee.

    auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds{500};
    auto state = llvm_jit_checkpoint_retire_saved_native_workers_host_api(ticket, captures, until);
    if(state.operation &&
       (state.status == llvm_jit_checkpoint_native_retirement_status::pending_execution ||
        state.status == llvm_jit_checkpoint_native_retirement_status::pending_native_join))
    {
        state = llvm_jit_checkpoint_continue_native_retirement_host_api(
            state.operation, std::chrono::steady_clock::now() + std::chrono::seconds{20});
    }

- pending_execution: actual old execution leases have not drained. The same request, host closure, native owners, captures and old code remain owned.
- pending_native_join: actual execution drain succeeded, but OS join has not succeeded for every owned worker. This includes TLS destruction after a trampoline has released its execution lease.
- retired_and_joined: every owned native worker was physically joined; the same retained instance is reopened through the real prepared execution-lifetime transition. Runtime/Wasm/source epoch and resources stay unchanged. The all_frames_signalled field separately reports whether every original frame consumed its private retirement signal.
- failed_closed: an invariant/ownership failure after activation keeps the operation and admission closed. A status/count/deadline never substitutes for drain or join authority.

Both pending states keep ordinary execution and host admission closed. Legacy stop/drain/reset and source replacement own the same manager mutex for their entire cold transition. While a managed operation is active they return without closing debug control, draining, invoking the initializer, mutating the instance or reopening admission. Source replacement reports false; older void stop/reset entries are no-ops. Execution lease drain cannot bypass unfinished native TLS/OS join. A repeated begin returns busy with the current canonical operation when supplied captures are canonical. Expired deadlines do not start work. Continue authenticates the actual manager-owned token and prepared lifetime; a successful operation can be observed again while its canonical token is retained. The manager remembers only its latest completed operation.

The native test debug_checkpoint_native_cohort_retirement_runtime.cc uses real generated LLVM-full safe points and two runtime-created workers. It blocks their actual bodies after guest entry returns, then blocks actual TLS destructors, and checks the two independent pending states, refusal of new admission, physical join, unchanged original output buffers, and a subsequent ordinary entry returning 42 on the retained instance. It also checks rejection of unregistered OS threads, incomplete/duplicate/aliased captures, an additional GC reader, expired deadlines and foreign operation aliases. Run both products with instruction and unwind; compilation and runtime qualification belong in the guarded SSH Linux cgroup.

Bounded native join uses the selected FastIO native_thread::try_join provider. A target provider must actually support nonblocking join; unsupported/error is never treated as successful retirement. Linux native qualification alone does not certify every other platform.

The remaining complete-instance path still needs actual candidate root installation, a joint resource/source/code/GC/WASI publication, verified replacement-worker startup and its final admission gate. Arbitrary external I/O rollback remains outside this retirement interface.
