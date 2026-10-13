/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
// Include INSIDE uwvm2::runtime::lib AFTER the current source-memory reader.
// Cold HOST observation only. A saved incarnation is a DATA selector; this API
// still requires the ONE complete current cohort/closedhost/N/publication proof.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    bool llvm_jit_debug_source_memory_view::copy_frame_locals(llvm_jit_debug_source_frame_locals& out) const noexcept
    {
        out = {};
        if(state_ == nullptr || state_->issuer != ::std::addressof(debug_source_memory_issuer_cookie) ||
           !state_->active || !state_->frame_locals_available) { return false; }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        try
        {
#endif
            out = state_->frame_locals;
            return true; // Detached native scalar DATA, never a JIT/VM stack borrow.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        }
        catch(...) { out = {}; return false; }
#endif
    }
    extern "C++" bool llvm_jit_debug_with_source_frame_memory_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        llvm_jit_debug_activation_capture_owner const& capture,
        llvm_jit_debug_source_binding_owner const& binding,
        ::std::uint_least64_t expected_incarnation,
        void* context, llvm_jit_debug_source_memory_callback callback) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        namespace checkpoint = ::uwvm2::runtime::checkpoint;
        using manager = runtime_checkpoint_coherent_manager;
        using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
        using typed_capture = runtime_checkpoint_thread_capture;
        if(!ticket || supplied.empty() || supplied.size() > 256u || !capture || !binding ||
           expected_incarnation == 0u || callback == nullptr ||
           mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const canonical{debug_activation_canonical_capture(capture)};
            // Registry object AND shared-control-block identity precede EVERY
            // supplied capture read. Neither a decimal incarnation nor an alias
            // shared_ptr can nominate another VM/native activation owner.
            if(!canonical || !canonical->control_ || !canonical->ticket_ ||
               canonical->snapshot_.participant == 0u || canonical->snapshot_.frames.empty()) { return false; }
            typed_capture::owner captures[256u]{};
            for(::std::size_t n{}; n != supplied.size(); ++n)
            {
                // [request-owned wrappers ... n<size<=256] end
                // [safe] bounds precede index and canonical registry lookup.
                auto const wrapper{llvm_jit_checkpoint_thread_capture::canonical(supplied[n])};
                if(!wrapper) { return false; }
                captures[n] = wrapper->actual_; // Privately registered wrapper, not a supplied pointee.
            }
            struct read_context
            {
                llvm_jit_debug_activation_capture_owner const& canonical;
                llvm_jit_debug_source_binding_owner const& binding;
                ::std::span<typed_capture::owner const> captures;
                ::std::uint_least64_t expected_incarnation;
                void* context;
                llvm_jit_debug_source_memory_callback callback;
            } request{canonical, binding, {captures, supplied.size()}, expected_incarnation, context, callback};
            auto const read{+[](void* opaque, ::std::span<domain::stopped_participant const> actual_slots) noexcept
            {
                if(opaque == nullptr) { return false; }
                // [THIS synchronous private management recipe] lexical_end
                // [safe] the API alone supplied opaque; no guest/native address
                // is cast into a source owner, frame, code or callback resource.
                auto const& request{*static_cast<read_context const*>(opaque)};
                auto const& canonical{request.canonical};
                try
                {
                    bool found{};
                    ::uwvm2::utils::thread::cooperative_pause_location leaf_location{};
                    for(auto const& slot : actual_slots)
                    {
                        if(slot.id != canonical->snapshot_.participant) { continue; }
                        if(found) { return false; }
                        leaf_location = slot.location; found = true;
                    }
                    if(!found || !canonical->matches_publication_locked(leaf_location)) { return false; }
                    auto const caller_binding{llvm_jit_debug_source_binding::canonical_locked(request.binding)};
                    if(!caller_binding) { return false; }
                    typed_capture const* actual_capture{};
                    for(auto const& owner : request.captures)
                    {
                        // The sole manager already registry-qualified ALL these
                        // owners and their SAME ticket/actual source/currentplan.
                        // No public owner is dereferenced before that preflight.
                        if(!owner || owner->participant_ != canonical->snapshot_.participant) { continue; }
                        if(actual_capture != nullptr) { return false; }
                        actual_capture = owner.get();
                    }
                    if(actual_capture == nullptr || actual_capture->frames_.empty() ||
                       actual_capture->frames_.size() != canonical->snapshot_.frames.size()) { return false; }
                    ::std::size_t selected{}; bool selected_found{};
                    for(::std::size_t i{}; i != actual_capture->frames_.size(); ++i)
                    {
                        // [authenticated saved/public activation chains ... i] end
                        // [safe] equal checked counts precede BOTH indexed reads.
                        auto const& actual{actual_capture->frames_[i].activation};
                        auto const& copied{canonical->snapshot_.frames[i]};
                        if(actual.incarnation != copied.incarnation || actual.parent != copied.parent ||
                           actual.continuation != copied.continuation || actual.module != copied.module ||
                           actual.function != copied.function || actual.function_generation != copied.function_generation ||
                           actual.runtime_epoch != copied.runtime_epoch) { return false; }
                        if(actual.incarnation == request.expected_incarnation)
                        {
                            if(selected_found) { return false; }
                            selected = i; selected_found = true;
                        }
                    }
                    if(!selected_found || selected >= actual_capture->frames_.size()) { return false; }
                    // [actual saved current frames ... selected ... N] end
                    // [safe] a UNIQUE authenticated incarnation produced this
                    // checked index; no UI frame ordinal is a native read grant.
                    auto const& saved{actual_capture->frames_[selected]};
                    auto const& activation{saved.activation};
                    auto const& logical{saved.logical};
                    if(typed_capture::check_owned_values(logical, typed_capture::value_use::live_observation) !=
                       checkpoint_thread_capture_status::captured || activation.module >= g_runtime.modules.size()) { return false; }
                    // [independently sealed exact sites ... site-1 ...] end
                    // [safe] check_owned_values proves 0<site<=N BEFORE subtract
                    // and index; the manager matched this actual CURRENT plan.
                    auto const& site{logical.plan->get().sites[static_cast<::std::size_t>(logical.site - 1u)]};
                    bool const leaf{selected + 1u == actual_capture->frames_.size()};
                    if(site.phase != (leaf ? checkpoint::frame_phase::before_opcode : checkpoint::frame_phase::awaiting_call_return) ||
                       site.local_count > logical.values.size() ||
                       caller_binding->module_id_ != activation.module || caller_binding->runtime_epoch_ != activation.runtime_epoch ||
                       caller_binding->publication_ != saved.publication_identity ||
                       !caller_binding->source_ || !saved.source || caller_binding->source_.get() != saved.source.get() ||
                       caller_binding->source_.owner_before(saved.source) || saved.source.owner_before(caller_binding->source_)) { return false; }
                    // Only the real authenticated saved caller's SEALED site
                    // supplies its Wasm call PC. The original leaf location and
                    // matching DWARF/name/ordinal are never substituted for it.
                    ::uwvm2::utils::thread::cooperative_pause_location selected_location{
                        activation.module, activation.function, site.opcode_offset, activation.runtime_epoch};
                    llvm_jit_debug_source_memory_view::implementation state{};
                    if(!llvm_jit_debug_source_binding::position_locked(caller_binding, selected_location, state.position.source) ||
                       state.position.source.module != activation.module || state.position.source.function != activation.function ||
                       state.position.source.runtime_epoch != activation.runtime_epoch ||
                       state.position.source.function_generation != activation.function_generation) { return false; }
                    auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(activation.module))};
                    auto const* module{record.runtime_module};
                    // [actual dense module array] preceding module<size check
                    // proves the indexed source-owned record. Native numeric
                    // local/position observation does not require a linear memory.
                    if(module == nullptr) { return false; }
                    bool byte_backend_available{module->imported_memory_vec_storage.empty() &&
                        module->local_defined_memory_vec_storage.size() == 1u};
                    if(byte_backend_available)
                    {
                        // [actual local memory array of size EXACTLY1] end
                        // [safe] count proof precedes its sole indexed borrow.
                        auto const& memory{module->local_defined_memory_vec_storage.index_unchecked(0u)};
                        auto const* declaration{memory.memory_type_ptr};
                        if(declaration == nullptr) { byte_backend_available = false; }
                        else
                        {
                            auto const shared{[](auto const& type) noexcept
                            { if constexpr(requires { type.shared; }) { return type.shared; } else { return false; } }(*declaration)};
                            auto const address64{[](auto const& type) noexcept
                            { if constexpr(requires { type.address64; }) { return type.address64; } else { return false; } }(*declaration)};
                            state.address_bytes = address64 ? 8u : 4u;
                            byte_backend_available = !shared;
                        }
                    }
                    // Ambiguous/imported/shared bytes are unavailable, but this
                    // SAME complete current proof still admits copied native
                    // scalars/metadata. No guessed memory index or backend access.
                    state.issuer = ::std::addressof(debug_source_memory_issuer_cookie);
                    state.position.activation = canonical->snapshot_;
                    // [owned identities of size N>selected] end
                    // [safe] selected<N precedes selected+1 (<=N) and shrink;
                    // caller position exposes only its actual ancestor prefix.
                    state.position.activation.frames.resize(selected + 1u);
                    state.position.activation.location = selected_location;
                    state.position.source_available = true;
                    state.frame_locals.participant = canonical->snapshot_.participant;
                    state.frame_locals.incarnation = activation.incarnation;
                    state.frame_locals.total_count = site.local_count;
                    if(site.operand_count > logical.values.size()-site.local_count) { return false; }
                    state.frame_locals.operand_count = site.operand_count;
                    auto const count{(::std::min)(site.local_count, llvm_jit_debug_max_captured_locals)};
                    auto const operand_count{(::std::min)(site.operand_count, llvm_jit_debug_max_captured_locals)};
                    state.frame_locals.values.resize(count); state.frame_locals.operands.resize(operand_count);
                    using kind = checkpoint::types::value_kind;
                    for(::std::size_t n{}; n != count+operand_count; ++n)
                    {
                        // [actual saved local prefix ... n<count<=local_count] end
                        // [safe] local_count<=values.size and owned output.count
                        // precede BOTH indices and every fixed-width byte copy.
                        auto const& value{logical.values[n < count ? n : site.local_count+(n-count)]};
                        auto& out{n < count ? state.frame_locals.values[n] : state.frame_locals.operands[n-count]};
                        ::std::size_t width{};
                        switch(value.declaration.type.kind)
                        {
                            case kind::i32: out.wasm_type = 0x7fu; width = 4u; break;
                            case kind::i64: out.wasm_type = 0x7eu; width = 8u; break;
                            case kind::f32: out.wasm_type = 0x7du; width = 4u; break;
                            case kind::f64: out.wasm_type = 0x7cu; width = 8u; break;
                            case kind::v128: out.wasm_type = 0x7bu; width = 16u; break;
                            case kind::reference: break; // REF payload NEVER becomes an integer/native pointer.
                            default: return false;
                        }
                        if(width == 0u || !value.declaration.initialized) { continue; }
                        if(width > value.bits.size() || width > out.native_bytes.size()) { return false; }
                        // [complete typed native carrier ... width<=16] end
                        // [safe] source/destination extents precede my_memcpy.
                        // Keep native ABI numeric representation/NaN raw bits;
                        // DWARF consumers already convert owned objects to LE.
                        ::fast_io::freestanding::my_memcpy(out.native_bytes.data(), value.bits.data(), width);
                        out.available = true;
                    }
                    // Local global storage belongs to the authenticated actual
                    // module publication. Imported namespaces are explicit
                    // holes until a resource-origin adapter resolves them.
                    auto const imported{module->imported_global_vec_storage.size()};
                    auto const defined{module->local_defined_global_vec_storage.size()};
                    if(imported > UINT64_MAX-defined) { return false; }
                    state.frame_locals.global_count = imported+defined;
                    auto const globals{(::std::min)(static_cast<::std::size_t>(state.frame_locals.global_count),llvm_jit_debug_max_captured_locals)};
                    state.frame_locals.globals.resize(globals);
                    if(debug_source_memory_exclusive_bytes_locked())
                    {
                        for(::std::size_t i{(::std::min)(imported,globals)}; i != globals; ++i)
                        {
                            // i>=imported and i<imported+defined precede the
                            // local namespace subtraction and fixed carrier copy.
                            auto const& global{module->local_defined_global_vec_storage.index_unchecked(i-imported)};
                            auto& out{state.frame_locals.globals[i]};
                            if(global.global_type_ptr == nullptr) { return false; }
                            auto const code{static_cast<unsigned>(global.global_type_ptr->type)};
                            using global_kind = ::uwvm2::object::global::global_type;
                            ::std::size_t width{}; void const* bytes{};
                            switch(code)
                            {
                                case 0x7fu: if(global.global.kind != global_kind::wasm_i32) { return false; } width=4u;bytes=::std::addressof(global.global.storage.i32);break;
                                case 0x7eu: if(global.global.kind != global_kind::wasm_i64) { return false; } width=8u;bytes=::std::addressof(global.global.storage.i64);break;
                                case 0x7du: if(global.global.kind != global_kind::wasm_f32) { return false; } width=4u;bytes=::std::addressof(global.global.storage.f32);break;
                                case 0x7cu: if(global.global.kind != global_kind::wasm_f64) { return false; } width=8u;bytes=::std::addressof(global.global.storage.f64);break;
                                case 0x7bu: if(global.global.kind != global_kind::wasm_v128) { return false; } width=16u;bytes=::std::addressof(global.global.storage.v128);break;
                                default: continue;
                            }
                            out.wasm_type = static_cast<::std::uint_least8_t>(code);
                            ::fast_io::freestanding::my_memcpy(out.native_bytes.data(),bytes,width); out.available = true;
                        }
                    }
                    state.frame_locals_available = true;
                    state.exclusively_owned_bytes = byte_backend_available && debug_source_memory_exclusive_bytes_locked();
                    auto const observe{[&](::std::byte* begin, ::std::size_t length) noexcept
                    {
                        if(begin == nullptr && length != 0u) { return false; }
                        // [actual retained backend snapshot ... length] end
                        // [safe] no guest-offset pointer is formed here. The SAME
                        // manager proof and memory guard cover every bounded read.
                        state.begin = begin; state.length = length; state.active = true;
                        llvm_jit_debug_source_memory_view view{::std::addressof(state)};
                        runtime_compilation_metadata_callback_scope no_reentry{};
                        auto const accepted{request.callback(request.context, view)};
                        state.active = false; state.begin = nullptr; state.length = 0u;
                        state.frame_locals_available = false;
                        return accepted; // Invalidate BEFORE backend/N/host/publication/cohort release.
                    }};
                    // Metadata/native scalar copies do not touch a potentially
                    // escaped foreign mmap alias. Unknown providers can never
                    // use copy_guest merely because all Wasm threads are parked.
                    if(!state.exclusively_owned_bytes) { return observe(nullptr, 0u); }
                    // [actual sole local memory] byte_backend_available proves
                    // count==1 above and held publication prevents mutation.
                    auto const& memory{module->local_defined_memory_vec_storage.index_unchecked(0u)};
                    return with_native_preload_copy_access(memory.memory, observe);
                }
                catch(...) { return false; }
            }};
            auto const observed{manager::with_current_source_memory(ticket, {captures, supplied.size()},
                canonical->ticket_, ::std::addressof(request), read)};
            return observed.observation == manager::status::coherent_typed_data;
        }
        catch(...) { return false; }
#else
        (void)ticket; (void)supplied; (void)capture; (void)binding;
        (void)expected_incarnation; (void)context; (void)callback; return false;
#endif
    }
#endif
