// Private implementation attachment. Include INSIDE the actual runtime
// namespace AFTER coherent manager and actual canonical public capture wrapper.
#pragma once
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    extern "C++" ::uwvm2::uwvm::debugger::wasip1_state::view llvm_jit_debug_query_wasip1_state_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        ::uwvm2::uwvm::debugger::wasip1_state::request const& selected) noexcept
    {
        namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
        ws::view out{}; out.module = selected.module; out.operation = selected.operation;
        if(!ws::valid(selected)) { out.result = ws::status::invalid_request; return out; }
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        if(!ticket || supplied.empty() || supplied.size() > 256u)
        { out.result = ws::status::incomplete_cohort; return out; }
        runtime_checkpoint_thread_capture::owner captures[256u]{};
        for(::std::size_t i{}; i != supplied.size(); ++i)
        {
            // [request-owned wrapper cells ... i<N<=256] end
            // [safe] canonical registry compares pointer AND control block
            // before ANY supplied payload read; a forged alias is declined.
            auto const canonical{llvm_jit_checkpoint_thread_capture::canonical(supplied[i])};
            if(!canonical) { out.result = ws::status::incomplete_cohort; return out; }
            captures[i] = canonical->actual_;
        }
        return runtime_checkpoint_coherent_manager::query_current_wasip1_state(ticket, {captures, supplied.size()}, selected);
#else
        (void)ticket; (void)supplied; out.result = ws::status::requires_llvm_jit_full; return out;
#endif
    }
#endif
