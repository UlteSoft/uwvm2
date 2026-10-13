/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
// PRIVATE member fragment of the real runtime_execution_entry_scope. Keep the
// original ordinary constructor and its ABI/body untouched. Only the private
// restored dispatcher can choose this nonfatal REAL-admission constructor.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
friend class ::uwvm2::runtime::lib::runtime_checkpoint_continuation_dispatcher;
struct restored_try_entry {};
void abort_unexecuted_restored_entry() noexcept
{
    // No Wasm instruction/participant/provider has been invoked in this path.
    // Clear actual borrows/depth BEFORE the real members release their leases.
    if(checkpoint_gc_entry_==this) { checkpoint_gc_entry_=nullptr; }
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
    if(source_leaf_entry==this) { source_leaf_entry=nullptr; }
#endif
    if(entered)
    {
        auto& depth{get_runtime_execution_entry_depth()};
        if(::uwvm2::runtime::lib::details::runtime_execution_entry_leave(depth)!=
            ::uwvm2::runtime::lib::details::runtime_execution_entry_leave_result::outermost) { ::fast_io::fast_terminate(); }
        entered=false;
    }
    // [this real still-live execution member][calling-thread TLS/map borrow]
    // [safe] compare actual member BEFORE dropping TLS; never read erased nodes.
    if(get_runtime_execution_lease()==::std::addressof(execution_lease))
    { get_runtime_execution_lease()=nullptr;erase_current_thread_runtime_state(); }
    gc_admission.reset();
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1 && \
    defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
    if(exception_retirement) { exception_retirement->finish_after_native_resume(); }
#endif
    // The failed scope still owns its REAL execution/host/native stack members;
    // their normal destructor order releases them. No lease count/OS ACK is
    // forged, and admitted host callbacks cannot be forcibly unwound here.
}
inline explicit runtime_execution_entry_scope(restored_try_entry,::uwvm2::utils::thread::execution_domain::lease&& admitted) noexcept
    :execution_lease{::std::move(admitted)}
{
    namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
    if(get_runtime_execution_entry_depth()!=0u || get_runtime_state_publication_depth()!=0u ||
       mode::global_runtime_mode!=mode::runtime_mode_t::full_compile ||
       mode::global_runtime_compiler!=mode::runtime_compiler_t::llvm_jit_only) { return; }
    try
    {
#if (defined(__linux__) || defined(__APPLE__)) && !defined(_WIN32)
        if(!native_stack_scope.ready()) { return; }
#endif
        ensure_runtime_process_lifetime();
        gc_admission=::uwvm2::runtime::gc::runtime_gc_entry_admission.enter();
        if(!gc_admission) { return; }
        // The private canonical world obtained this REAL lease through its
        // already-published native startup token while ordinary admission is
        // still CLOSED. Never call ordinary try_enter(), accept a fake ACK, or
        // replace a missing/stopped token with a fresh unrelated lifetime.
        if(!execution_lease || execution_lease.stop_requested()) { gc_admission.reset();return; }
        // Actual execution admission now pins immutable selected configuration.
        // An ordinary/foreign execution cannot be relabelled as a restored one.
        if(!g_runtime.checkpoint_profile || !g_runtime.debug_pause_control ||
           !g_runtime.checkpoint_host_enabled.load(::std::memory_order_acquire)) { return; }
        get_runtime_execution_lease()=::std::addressof(execution_lease);
        thread_wait_scope.emplace(g_runtime.thread_wait_domain,execution_lease.cancellation_token());
        auto& depth{get_runtime_execution_entry_depth()};
        if(!::uwvm2::runtime::lib::details::runtime_execution_entry_enter(depth,runtime_execution_entry_reentry::reject))
        { abort_unexecuted_restored_entry();return; }
        entered=true;checkpoint_gc_entry_=this;
        if(runtime_checkpoint_host_bridge::begin_actual_entry(checkpoint_host_entry)!=runtime_checkpoint_host_bridge::status::ok)
        { abort_unexecuted_restored_entry();return; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) && UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS == 1 && \
    defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
        exception_retirement.emplace();
#endif
#if defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
        bind_actual_source_exception_publication();
#endif
    }
    catch(...) { abort_unexecuted_restored_entry(); }
}
[[nodiscard]] bool restored_entry_admitted() const noexcept
{
    return entered && execution_lease && gc_admission && checkpoint_gc_entry_==this &&
        get_runtime_execution_entry_depth()==1u && get_runtime_execution_lease()==::std::addressof(execution_lease);
}
#endif
