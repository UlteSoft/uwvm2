/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
// Runtime implementation fragment, included INSIDE uwvm2::runtime::lib after
// the private activation/source implementations and native memory copy helpers.
// It is deliberately a normal .h; no ordinary compiler or memory access path
// calls this debugger management operation.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    extern "C++" bool llvm_jit_debug_copy_source_object_host_api(
        llvm_jit_debug_activation_capture_owner const& capture,
        llvm_jit_debug_source_binding_owner const& binding,
        ::std::uint_least64_t guest_offset, ::std::size_t size,
        ::std::uint_least8_t address_bytes, llvm_jit_debug_source_object_copy& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!capture || !binding || size > 65536u || (address_bytes != 4u && address_bytes != 8u) ||
           mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const canonical{debug_activation_canonical_capture(capture)};
            // Check registry AND shared control-block identity before touching a
            // supplied pointer. A copied public PC/stop label is not authority.
            if(!canonical || !canonical->control_ || !canonical->ticket_ || canonical->snapshot_.participant == 0u) { return false; }
            // Real cold native reader excludes the original managed sweep.
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};
            if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            bool valid{};
            auto const stopped{canonical->control_->with_stopped_participant(canonical->ticket_, canonical->snapshot_.participant,
                [&](auto const actual)
            {
                runtime_state_publication_guard lock{};
                if(!canonical->matches_publication_locked(actual) ||
                   !llvm_jit_debug_source_binding::canonical_locked(binding)) { return; }
                llvm_jit_debug_source_object_copy candidate{};
                if(!llvm_jit_debug_source_binding::position_locked(binding, actual, candidate.position.source) ||
                   candidate.position.source.module != actual.code_unit || candidate.position.source.function != actual.function ||
                   candidate.position.source.runtime_epoch != actual.code_generation ||
                   candidate.position.source.function_generation != canonical->snapshot_.frames.back().function_generation ||
                   actual.code_unit >= g_runtime.modules.size()) { return; }
                auto const& rec{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(actual.code_unit))};
                auto const* module{rec.runtime_module};
                // Traditional LLVM Wasm DWARF addresses do not encode which
                // linear memory owns an object. Never guess memory zero when
                // multiple memories exist. Imported/provider memories and shared
                // memory need a separate coherent ownership/copy protocol; they
                // cannot enter this unshared locally defined producer ABI.
                if(module == nullptr || !module->imported_memory_vec_storage.empty() ||
                   module->local_defined_memory_vec_storage.size() != 1u) { return; }
                // [actual module-owned local memory table: exactly one entry] end
                // [safe                                                   ] fixed index 0;
                //  ^^ publication + execution lease retain this initialized object/declaration.
                auto const& memory{module->local_defined_memory_vec_storage.index_unchecked(0u)};
                auto const* declaration{memory.memory_type_ptr};
                if(declaration == nullptr) { return; }
                auto const shared{[](auto const& type) noexcept
                { if constexpr(requires { type.shared; }) { return type.shared; } else { return false; } }(*declaration)};
                auto const address64{[](auto const& type) noexcept
                { if constexpr(requires { type.address64; }) { return type.address64; } else { return false; } }(*declaration)};
                if(shared || address_bytes != (address64 ? 8u : 4u)) { return; }
                candidate.bytes.resize(size); // bounded owned allocation; no guest/provider callback.
                auto const copied{with_native_preload_copy_access(memory.memory,
                    [&](::std::byte* begin, ::std::size_t length) noexcept
                {
                    if(begin == nullptr && length != 0u) { return false; }
                    ::std::size_t host_offset{};
                    if(!validate_preload_copy_range(length, guest_offset, size, host_offset)) { return false; }
                    if(size != 0u)
                    {
                        if(begin == nullptr || candidate.bytes.size() != size) { return false; }
                        // [actual native single-memory bytes ... length] memory_end
                        // [safe                                      ] unsafe (one-past)
                        //  ^^ before begin + host_offset: offset<=length AND
                        //     size<=length-offset; destination owns exactly size bytes.
                        // The ONE all-stopped domain transaction and backend
                        // snapshot remain live throughout this bounded copy.
                        ::std::memcpy(candidate.bytes.data(), begin + host_offset, size);
                    }
                    return true;
                })};
                if(!copied) { return; }
                candidate.position.activation = canonical->snapshot_;
                candidate.position.source_available = true;
                candidate.guest_offset = guest_offset; candidate.address_bytes = address_bytes;
                out = ::std::move(candidate); valid = true;
            })};
            if(!stopped || !valid) { out = {}; return false; }
            return true;
        }
        catch(...) { out = {}; return false; }
#else
        (void)capture; (void)binding; (void)guest_offset; (void)size; (void)address_bytes; return false;
#endif
    }
#endif
