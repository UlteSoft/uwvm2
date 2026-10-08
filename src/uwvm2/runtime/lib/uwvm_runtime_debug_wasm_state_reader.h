// Private, synchronous consumer. Include inside runtime::lib after the actual
// GC state borrow definition and before the coherent manager definition.
// This header opens no guest endpoint and exports no native pointer/lease.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    extern "C++" class runtime_checkpoint_debug_state_reader final
    {
        runtime_checkpoint_debug_state_reader() = delete;
    public:
        [[nodiscard]] static ::uwvm2::uwvm::debugger::wasm_state::view copy(
            runtime_checkpoint_gc_state_borrow const& actual,
            ::uwvm2::uwvm::debugger::wasm_state::request const& selection) noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            ws::view unavailable{}; unavailable.requested = ws::unavailable_request(selection);
            if(!ws::valid(selection))
            { unavailable.result = ws::status::invalid_selection; return unavailable; }
            // This constructor-private, immovable context exists only inside
            // the real manager's ONE current cohort -> closed host gate -> N
            // actual GC leases and root census -> publication lexical scope.
            // Only its private checked producer resolves actual module/frame
            // values and GC/exn tokens. The reader receives detached DATA, not
            // a span/pointer that could survive collection or resume.
            auto copied{actual.copy_selected(selection)};
            if(!ws::valid(copied))
            { unavailable.result = ws::status::invalid_data; return unavailable; }
            return copied;
        }
    };
#endif
