#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
namespace
{
    [[nodiscard]] ::std::unique_lock<::std::mutex> try_runtime_native_legacy_transition() noexcept
    { return runtime_checkpoint_coherent_manager::try_legacy_transition_excluding_native_retirement(); }
}
#endif
// Host-cold managed retirement only. No candidate/source/ASM access or native
// address is returned; pending operation owners retain the actual old cohort.
extern "C++" llvm_jit_checkpoint_native_retirement_result llvm_jit_checkpoint_retire_saved_native_workers_host_api(
    ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
    ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
    ::std::chrono::steady_clock::time_point deadline) noexcept
{
    llvm_jit_checkpoint_native_retirement_result out{};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
    if(!ticket || supplied.empty() || supplied.size() > 256u)
    { out.status = llvm_jit_checkpoint_native_retirement_status::rejected_current_cohort; return out; }
    runtime_checkpoint_thread_capture::owner captures[256u]{};
    for(::std::size_t index{}; index < supplied.size(); ++index)
    {
        auto const actual{llvm_jit_checkpoint_thread_capture::canonical(supplied[index])};
        if(!actual) { out.status = llvm_jit_checkpoint_native_retirement_status::rejected_current_cohort; return out; }
        captures[index] = actual->actual_;
    }
    return runtime_checkpoint_coherent_manager::begin_native_retirement(ticket, {captures, supplied.size()}, deadline);
#else
    static_cast<void>(ticket); static_cast<void>(supplied); static_cast<void>(deadline); return out;
#endif
}
extern "C++" llvm_jit_checkpoint_native_retirement_result llvm_jit_checkpoint_continue_native_retirement_host_api(
    llvm_jit_checkpoint_native_retirement_owner const& supplied,
    ::std::chrono::steady_clock::time_point deadline) noexcept
{
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
    return runtime_checkpoint_coherent_manager::continue_native_retirement(supplied, deadline);
#else
    static_cast<void>(supplied); static_cast<void>(deadline); return {};
#endif
}
