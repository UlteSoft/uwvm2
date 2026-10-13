/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
// Included INSIDE uwvm2::runtime::lib after source-object/copy helpers. Cold
// HOST debugging only: no ordinary Wasm load/store/compiler path calls it.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Address equality only: actual management-thread TLS, never a public ID or
    // synthetic pthread handle. No callback/backend exposes this private cookie.
    static thread_local ::std::byte debug_source_memory_issuer_cookie{};
    struct llvm_jit_debug_source_memory_view::implementation
    {
        void const* issuer{}; // immutable actual TLS identity while the callback is active
        ::std::byte const* begin{};
        ::std::size_t length{}, attempted{}, copied{};
        ::std::uint_least8_t address_bytes{};
        llvm_jit_debug_source_activation_snapshot position{};
        bool active{}, exclusively_owned_bytes{}; // actual closed source/provider census only
        llvm_jit_debug_source_frame_locals frame_locals{};
        bool frame_locals_available{};
    };
    llvm_jit_debug_source_memory_view::llvm_jit_debug_source_memory_view(implementation* state) noexcept
        // [actual private state in this synchronous API frame] lifetime_end
        // [safe                                             ] no source/UI pointer;
        //  ^^ private constructor adopts only the live authenticated frame.
        : state_{state} {}
    ::std::uint_least8_t llvm_jit_debug_source_memory_view::address_bytes() const noexcept
    { return state_ != nullptr && state_->issuer == ::std::addressof(debug_source_memory_issuer_cookie) && state_->active ? state_->address_bytes : 0u; }
    bool llvm_jit_debug_source_memory_view::copy_position(llvm_jit_debug_source_activation_snapshot& out) const noexcept
    {
        out = {};
        if(state_ == nullptr || state_->issuer != ::std::addressof(debug_source_memory_issuer_cookie) || !state_->active) { return false; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            out = state_->position; return true; // detached copied metadata, never a stack/native-memory reference.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return false; }
#endif
    }
    bool llvm_jit_debug_source_memory_view::copy_guest(::std::uint_least64_t offset,
        ::std::size_t size, llvm_jit_debug_source_object_copy& out) noexcept
    {
        out = {};
        if(state_ == nullptr || state_->issuer != ::std::addressof(debug_source_memory_issuer_cookie) ||
           !state_->active || !state_->exclusively_owned_bytes || state_->attempted >= 32u) { return false; }
        ++state_->attempted; // checked32 bounded scalar count, not a memory pointer.
        if(size > 65536u || state_->copied > 2097152u || size > 2097152u - state_->copied) { return false; }
        ::std::size_t host_offset{};
        if(!validate_preload_copy_range(state_->length, offset, size, host_offset) ||
           (state_->begin == nullptr && (state_->length != 0u || size != 0u))) { return false; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            llvm_jit_debug_source_object_copy candidate{};
            candidate.position = state_->position; candidate.bytes.resize(size);
            if(size != 0u)
            {
                if(state_->begin == nullptr || candidate.bytes.size() != size) { return false; }
                // [actual retained unshared memory ... length] memory_end
                // [safe                                     ] unsafe (one-past)
                //  ^^ BEFORE begin+host_offset: offset<=length AND
                //     size<=length-offset. Destination owns exactlysize bytes.
                // Publication/memory/all-stopped guards cover every chain read.
                ::fast_io::freestanding::my_memcpy(candidate.bytes.data(), state_->begin + host_offset, size);
            }
            state_->copied += size; // checked cumulative subtraction above.
            candidate.guest_offset = offset; candidate.address_bytes = state_->address_bytes;
            out = ::std::move(candidate); return true;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return false; }
#endif
    }
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
    // Caller owns the SAME genuine full-cohort/closedhost/N/publication guard.
    // An empty counted host gate cannot revoke a previously escaped mmap alias.
    // Centralize the actual published import-closure policy for both leaf and
    // caller readers; no guest memory/compiler hot path calls this cold helper.
    [[nodiscard]] static bool debug_source_memory_exclusive_bytes_locked() noexcept
    {
        if(g_import_call_cache.size() != g_runtime.modules.size()) { return false; }
        for(::std::size_t module_id{}; module_id != g_runtime.modules.size(); ++module_id)
        {
            // [actual complete module/cache arrays ... module_id ... N] end
            // [safe] equal counts and loop bound precede BOTH indexed reads.
            auto const& member{g_runtime.modules.index_unchecked(module_id)};
            auto const* module{member.runtime_module};
            auto const& cache{g_import_call_cache.index_unchecked(module_id)};
            if(!checkpoint_effect_actual_full_owner(module_id) || module == nullptr ||
               cache.size() != module->imported_function_vec_storage.size() ||
               !member.llvm_jit_full_publication->source->actual_no_unadapted_native_memory_provider(
                   module_id,current_runtime_generation(),module)) { return false; }
            for(::std::size_t i{}; i != cache.size(); ++i)
            {
                auto const& target{cache.index_unchecked(i)};
                if(target.origin_module_id != module_id) { return false; }
                if(checkpoint_effect_actual_cached_leaf(target) == checkpoint_resolved_effect::defined_wasm) { continue; }
                // Source observation adapter only. Our canonical builtin WASIp1
                // tuple accepts scalar guest offsets, makes bounded synchronous
                // copies and retains no guest memory alias. ALL stopped/closed
                // host admission remains live. This grants no effect replay.
                ::uwvm2::uwvm::runtime::full::builtin_wasip1_function_data binding{};
                if(target.k != cached_import_target::kind::local_imported || target.reference_params || target.reference_results ||
                   !member.llvm_jit_full_publication->source->actual_builtin_wasip1_function(
                       module_id,current_runtime_generation(),module,i,binding)) { return false; }
                auto const& original{module->imported_function_vec_storage.index_unchecked(i)};
                if(original.link_kind != ::uwvm2::uwvm::runtime::storage::imported_function_link_kind::local_imported ||
                   original.target.local_imported.module_ptr != target.u.local_imported.module_ptr ||
                   original.target.local_imported.index != target.u.local_imported.index ||
                   binding.function_index != target.u.local_imported.index) { return false; }
                ::std::size_t parameters{}, results{};
                auto const size{[](auto const& values,unsigned count,::std::size_t& bytes) noexcept
                {
                    if(count > values.size()) { return false; }
                    for(unsigned n{}; n != count; ++n)
                    { if(values[n] != 0x7fu && values[n] != 0x7eu) { return false; } bytes += values[n] == 0x7fu ? 4u : 8u; }
                    return true;
                }};
                if(!size(binding.parameters,binding.parameter_count,parameters) || !size(binding.results,binding.result_count,results) ||
                   parameters != target.param_bytes || results != target.result_bytes) { return false; }
            }
        }
        return true;
    }
#endif
    extern "C++" bool llvm_jit_debug_with_source_memory_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        llvm_jit_debug_activation_capture_owner const& capture,
        llvm_jit_debug_source_binding_owner const& binding,
        void* context, llvm_jit_debug_source_memory_callback callback) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        using manager = runtime_checkpoint_coherent_manager;
        using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
        if(!ticket || supplied.empty() || supplied.size() > 256u || !capture || !binding || callback == nullptr ||
           mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const canonical{debug_activation_canonical_capture(capture)};
            // Both object AND shared ownership are registry-qualified BEFORE
            // ANY supplied activation read. This real local pin remains live.
            if(!canonical || !canonical->control_ || !canonical->ticket_ ||
               canonical->snapshot_.participant == 0u || canonical->snapshot_.frames.empty()) { return false; }
            runtime_checkpoint_thread_capture::owner captures[256u]{};
            for(::std::size_t n{}; n != supplied.size(); ++n)
            {
                // [request-owned wrapper span ... n<size<=256] end
                // [safe] count checked BEFORE index and registry lookup; no
                // opaque supplied pointee/control-block alias is dereferenced.
                auto const wrapper{llvm_jit_checkpoint_thread_capture::canonical(supplied[n])};
                if(!wrapper) { return false; }
                captures[n] = wrapper->actual_; // Registry-owned wrapper only.
            }
            struct read_context
            {
                llvm_jit_debug_activation_capture_owner const& canonical;
                llvm_jit_debug_source_binding_owner const& binding;
                void* context;
                llvm_jit_debug_source_memory_callback callback;
            } request{canonical, binding, context, callback};
            auto const read{+[](void* opaque, ::std::span<domain::stopped_participant const> actual_slots) noexcept
            {
                if(opaque == nullptr) { return false; }
                // [actual synchronous private stack recipe] lexical_end
                // [safe] only this host API supplied opaque; no public/native
                // guest address is cast to an executable or VM resource object.
                auto const& request{*static_cast<read_context const*>(opaque)};
                auto const& canonical{request.canonical};
                try
                {
                    // The sole manager has already authenticated ALL canonical
                    // typed captures, the selected activation's OWN current
                    // ticket, actual closedhost/N/publication/source/epoch, and
                    // has retained them throughout THIS synchronous callback.
                    bool found{};
                    ::uwvm2::utils::thread::cooperative_pause_location actual{};
                    for(auto const& slot : actual_slots)
                    {
                        if(slot.id != canonical->snapshot_.participant) { continue; }
                        if(found) { return false; }
                        actual = slot.location; found = true;
                    }
                    if(!found || !canonical->matches_publication_locked(actual) ||
                       !llvm_jit_debug_source_binding::canonical_locked(request.binding)) { return false; }
                    llvm_jit_debug_source_memory_view::implementation state{};
                    if(!llvm_jit_debug_source_binding::position_locked(request.binding, actual, state.position.source) ||
                       state.position.source.module != actual.code_unit || state.position.source.function != actual.function ||
                       state.position.source.runtime_epoch != actual.code_generation ||
                       state.position.source.function_generation != canonical->snapshot_.frames.back().function_generation ||
                       actual.code_unit >= g_runtime.modules.size()) { return false; }
                    // [actual current source-owned dense module table] end
                    // [safe] actual code_unit<size checked BEFORE narrowing/index.
                    auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(actual.code_unit))};
                    auto const* module{record.runtime_module};
                    // Traditional Wasm DWARF addresses have no memory index.
                    // Do not guess ambiguous/provider/shared memory ownership.
                    if(module == nullptr || !module->imported_memory_vec_storage.empty() ||
                       module->local_defined_memory_vec_storage.size() != 1u) { return false; }
                    auto const& memory{module->local_defined_memory_vec_storage.index_unchecked(0u)};
                    auto const* declaration{memory.memory_type_ptr};
                    if(declaration == nullptr) { return false; }
                    auto const shared{[](auto const& type) noexcept
                    { if constexpr(requires { type.shared; }) { return type.shared; } else { return false; } }(*declaration)};
                    auto const address64{[](auto const& type) noexcept
                    { if constexpr(requires { type.address64; }) { return type.address64; } else { return false; } }(*declaration)};
                    if(shared) { return false; }
                    state.issuer = ::std::addressof(debug_source_memory_issuer_cookie);
                    state.address_bytes = address64 ? 8u : 4u;
                    state.position.activation = canonical->snapshot_;
                    state.position.source_available = true;
                    // Closing active host admission does not revoke a native
                    // mmap descriptor previously handed to a provider. An
                    // observation profile retains ordinary host bridges; its
                    // empty counted gate is not evidence that a foreign writer
                    // never escaped. This first byte-reader qualification
                    // therefore checks the actual WHOLE published import-cache
                    // closure and admits ONLY initialized defined-Wasm leaves.
                    // Do not infer a WASIp1/foreign alias adapter from its name.
                    // Unknown/host leaves retain type-only source metadata, but
                    // cannot use copy_guest until a real adapter qualifies them.
                    state.exclusively_owned_bytes = debug_source_memory_exclusive_bytes_locked();
                    auto const observe{[&](::std::byte* begin, ::std::size_t length) noexcept
                    {
                        if(begin == nullptr && length != 0u) { return false; }
                        // [actual backend snapshot begin ... length] memory_end
                        // [safe] no offset pointer has been formed. Each bounded
                        // copy_guest request separately proves offset+size before
                        // any derivation. ONE manager's host/N/publication and
                        // backend snapshot stay live through the whole chain.
                        state.begin = begin; state.length = length; state.active = true;
                        llvm_jit_debug_source_memory_view view{::std::addressof(state)};
                        runtime_compilation_metadata_callback_scope no_reentry{};
                        auto const accepted{request.callback(request.context, view)};
                        state.active = false; state.begin = nullptr; state.length = 0u;
                        return accepted; // invalidate BEFORE backend/proof release
                    }};
                    // Type-only metadata does not need to touch a potentially
                    // foreign aliased backend at all. copy_guest rejects this
                    // nonexclusive recipe before reading begin/length.
                    if(!state.exclusively_owned_bytes) { return observe(nullptr, 0u); }
                    return with_native_preload_copy_access(memory.memory, observe);
                }
                catch(...) { return false; } // No partly copied/view/native owner escapes.
            }};
            auto const observed{manager::with_current_source_memory(ticket, {captures, supplied.size()},
                canonical->ticket_, ::std::addressof(request), read)};
            return observed.observation == manager::status::coherent_typed_data;
        }
        catch(...) { return false; }
#else
        (void)ticket; (void)supplied; (void)capture; (void)binding; (void)context; (void)callback; return false;
#endif
    }
#endif
