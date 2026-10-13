// Native management-only API. Include inside runtime::lib after the actual
// opaque capture wrapper, GC borrow/reader and coherent manager definitions.
// Every owner is authenticated before pointee access; no raw guest/VM address
// or copied stack/register value is accepted as an authority argument.
#pragma once
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    extern "C++" ::uwvm2::uwvm::debugger::wasm_state::view llvm_jit_debug_query_wasm_state_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        ::uwvm2::uwvm::debugger::wasm_state::request const& selection) noexcept
    {
        namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
        ws::view out{}; out.requested = ws::unavailable_request(selection);
        if(!ws::valid(selection)) { out.result = ws::status::invalid_selection; return out; }
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        if(!ticket) { out.result = ws::status::requires_current_cooperative_stop; return out; }
        if(supplied.empty() || supplied.size() > 256u)
        { out.result = ws::status::incomplete_cohort; return out; }
        runtime_checkpoint_thread_capture::owner captures[256u]{};
        for(::std::size_t index{}; index != supplied.size(); ++index)
        {
            // [request-owned opaque capture array, size<=256] end
            // [safe                                        ] index < size BEFORE owner read.
            auto canonical{llvm_jit_checkpoint_thread_capture::canonical(supplied[index])};
            if(!canonical) { out.result = ws::status::unavailable_activation; return out; }
            // [private registry owner == supplied get + control block] live
            // [safe] No field of a caller's pointee was read to establish this
            // identity; the actual private producer remains strongly pinned.
            captures[index] = canonical->actual_;
        }
        // The actual manager performs every current-ticket/participant/module,
        // profile/epoch/code-owner check inside its ONE domain transaction. Do
        // not wrap this call in a second domain/GC guard or use count0/public
        // labels as root/world-stop permission.
        return runtime_checkpoint_coherent_manager::query_current_wasm_state(
            ticket, {captures, supplied.size()}, selection);
# else
        out.result = ws::status::requires_llvm_jit_full; return out;
# endif
    }
#endif
