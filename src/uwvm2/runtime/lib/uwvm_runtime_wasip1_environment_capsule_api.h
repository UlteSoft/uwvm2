// Actual native-only capsule API. Include inside runtime namespace AFTER
// the private canonical capture wrapper and real coherent manager definitions.
#pragma once
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
extern "C++" llvm_jit_wasip1_environment_capsule_result llvm_jit_checkpoint_capture_wasip1_environment_host_api(
    ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
    ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
    llvm_jit_wasip1_environment_capsule_request const& selected) noexcept
{
    using status=llvm_jit_wasip1_environment_capsule_status;
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
    namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
    if(mode::global_runtime_mode!=mode::runtime_mode_t::full_compile || mode::global_runtime_compiler!=mode::runtime_compiler_t::llvm_jit_only)
    { return {status::requires_llvm_jit_full,{}}; }
    if(!ticket || supplied.empty() || supplied.size()>256u) { return {status::unavailable_capture,{}}; }
    runtime_checkpoint_thread_capture::owner captures[256u]{};
    for(::std::size_t n{};n!=supplied.size();++n)
    {
        // [request-owned opaque wrappers ... n<N<=256] end
        // [safe] canonical registry compares pointer AND control block BEFORE
        // supplied pointee read. No scalar report or detached VIEW is accepted.
        auto const canonical{llvm_jit_checkpoint_thread_capture::canonical(supplied[n])};
        if(!canonical) { return {status::unavailable_capture,{}}; }
        captures[n]=canonical->actual_;
    }
    return runtime_checkpoint_coherent_manager::capture_current_wasip1_environment(ticket,{captures,supplied.size()},selected);
#else
    (void)ticket;(void)supplied;(void)selected;return {status::requires_llvm_jit_full,{}};
#endif
}
extern "C++" llvm_jit_wasip1_environment_capsule_data_result llvm_jit_checkpoint_copy_wasip1_capsule_data_host_api(
    llvm_jit_wasip1_environment_capsule_owner const& supplied) noexcept
{
    using status=llvm_jit_wasip1_environment_capsule_status;
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
    auto const actual{llvm_jit_wasip1_environment_capsule::canonical(supplied)};
    if(!actual) { return {status::invalid_capsule_owner,{}}; }
    try
    {
        // Detached copied metadata only. Native refs/duplicates, private
        // incarnation binding, directory chain names and registry never escape.
        llvm_jit_wasip1_environment_capsule_data_result output{};output.data=actual->data_;output.status=status::captured;return output;
    }
    catch(...) { return {status::allocation_failed,{}}; }
#else
    (void)supplied;return {status::requires_llvm_jit_full,{}};
#endif
}
extern "C++" llvm_jit_wasip1_environment_capsule_status llvm_jit_checkpoint_restore_wasip1_environment_host_api(
    ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
    ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
    llvm_jit_wasip1_environment_capsule_owner const& saved, llvm_jit_wasip1_environment_restore_request const& selected) noexcept
{
    using status=llvm_jit_wasip1_environment_capsule_status;
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
    namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
    if(mode::global_runtime_mode!=mode::runtime_mode_t::full_compile || mode::global_runtime_compiler!=mode::runtime_compiler_t::llvm_jit_only)
    { return status::requires_llvm_jit_full; }
    if(!ticket || supplied.empty() || supplied.size()>256u) { return status::unavailable_capture; }
    runtime_checkpoint_thread_capture::owner captures[256u]{};
    for(::std::size_t n{};n!=supplied.size();++n)
    {
        auto const canonical{llvm_jit_checkpoint_thread_capture::canonical(supplied[n])};
        if(!canonical) { return status::unavailable_capture; }
        captures[n]=canonical->actual_;
    }
    return runtime_checkpoint_coherent_manager::restore_current_wasip1_environment(ticket,{captures,supplied.size()},saved,selected);
#else
    (void)ticket;(void)supplied;(void)saved;(void)selected;return status::requires_llvm_jit_full;
#endif
}
#endif
