// Runtime-private, debug-full-only uncaught observation. Include after the
// generated exception bridges and their actual module/tag owner resolver.
#pragma once

#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
    namespace
    {
        void debug_observe_uncaught_before_unwind(
            ::uwvm2::runtime::exception::guest_exception const& caught) noexcept
        {
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            auto const point{g_debug_wait_point};
            auto const participant{g_debug_pause_participant};
            auto const ledger{g_debug_activation_ledger};
            auto const shadow{g_checkpoint_shadow_ledger};
            auto const& observer{g_runtime.debug_observer};
            if(!point.valid || !participant || !ledger || !ledger->complete() || !shadow ||
               shadow->failure() != checkpoint::status::ok || !g_runtime.debug_pause_control ||
               g_runtime.debug_pause_control->is_closed() || !observer.on_uncaught ||
               g_debug_observer_active || g_debug_native_safe_point_site.return_pc != 0u ||
               get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u) { return; }
            auto const frames{shadow->frames_while_actually_stopped()};
            auto const activations{ledger->data()};
            if(frames.empty() || frames.size() != ledger->count() ||
               point.location.code_generation != current_runtime_generation()) { return; }
            auto const& leaf{activations[ledger->count() - 1u]};
            if(point.incarnation == 0u || point.incarnation != leaf.incarnation ||
               leaf.module != point.location.code_unit || leaf.function != point.location.function ||
               leaf.runtime_epoch != point.location.code_generation) { return; }

            // The actual thrown guest_exception owns its immutable tag/payload.
            // Inspect ALL current sealed lexical clauses before calling it
            // uncaught; equal signatures never replace real tag identity.
            auto const tag_identity{caught.instance()->tag_identity()};
            if(tag_identity == nullptr) { return; }
            for(::std::size_t ordinal{}; ordinal != frames.size(); ++ordinal)
            {
                auto const& frame{frames[ordinal]};
                auto const& active{activations[ordinal]};
                if(!frame.materialized || !frame.plan || active.module >= g_runtime.modules.size() ||
                   active.runtime_epoch != point.location.code_generation ||
                   frame.identity != checkpoint::activation_identity{active.incarnation, active.parent,
                       active.continuation, active.runtime_epoch}) { return; }
                auto const& record{g_runtime.modules.index_unchecked(active.module)};
                auto const module{record.runtime_module};
                if(module == nullptr || active.function < module->imported_function_vec_storage.size()) { return; }
                auto const plan{checkpoint_current_generation_plan(record,
                    active.function - module->imported_function_vec_storage.size(), active.function_generation)};
                if(!plan || plan.get() != frame.plan.get() || plan.owner_before(frame.plan) ||
                   frame.plan.owner_before(plan) || frame.site == 0u || frame.site > plan->get().sites.size()) { return; }
                // [actual sealed site array ... site-1 ...] end
                // [safe] nonzero/bounded site BEFORE indexing; entry lease and
                // exact current publication retain every module/tag/plan owner.
                auto const& site{plan->get().sites[static_cast<::std::size_t>(frame.site - 1u)]};
                if(checkpoint::validate_site(site, plan->get()) != checkpoint::status::ok ||
                   frame.values.size() != site.slots.size()) { return; }
                if(ordinal + 1u == frames.size())
                {
                    if(site.phase != checkpoint::frame_phase::before_opcode ||
                       site.opcode_offset != point.location.offset) { return; }
                }
                else if(site.phase != checkpoint::frame_phase::awaiting_call_return) { return; }
                for(auto const& handler : site.handlers)
                {
                    if(handler.catch_all) { return; }
                    auto const imports{module->imported_tag_vec_storage.size()};
                    using tag_type = ::uwvm2::uwvm::runtime::storage::local_defined_tag_storage_t;
                    tag_type const* tag{};
                    if(handler.tag_index < imports)
                    { tag = module->imported_tag_vec_storage.index_unchecked(handler.tag_index).resolved_tag; }
                    else
                    {
                        auto const local{handler.tag_index - imports};
                        if(local >= module->local_defined_tag_vec_storage.size()) { return; }
                        tag = ::std::addressof(module->local_defined_tag_vec_storage.index_unchecked(local));
                    }
                    if(tag == nullptr || !tag->exception_identity) { return; }
                    if(tag->exception_identity.get() == tag_identity) { return; }
                }
            }
            // No generated Wasm frame has unwound: locals, operands, reference
            // roots and caller identities are still real and remain alive for
            // the entire park. The wrapper's C++ catch is NEVER an ASM endpoint.
            g_debug_observer_active = true;
            auto const stop{observer.on_uncaught(observer.context.get(), participant->identifier(), point.location)};
            g_debug_observer_active = false;
            if(!stop) { return; }
            participant->poll(point.location, [&]() noexcept
            {
                debug_capture_actual_park(participant, point.location, point.local_snapshot,
                    point.captured_local_count, true);
            });
        }
    }
#endif

    extern "C++" [[noreturn]] void details::llvm_jit_debug_throw_numeric_abi_bridge(
        ::std::uintptr_t module, ::std::uintptr_t tag, ::std::uintptr_t buffer, ::std::size_t bytes) UWVM_THROWS
    {
#ifdef UWVM_CPP_EXCEPTIONS
        try { details::llvm_jit_throw_numeric_abi_bridge(module, tag, buffer, bytes); }
        catch(::uwvm2::runtime::exception::guest_exception const& caught)
        {
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
            debug_observe_uncaught_before_unwind(caught);
# endif
            throw;
        }
#else
        details::llvm_jit_throw_numeric_abi_bridge(module, tag, buffer, bytes);
#endif
    }
    extern "C++" [[noreturn]] void details::llvm_jit_debug_throw_tuple_abi_bridge(
        ::std::uintptr_t module, ::std::uintptr_t tag, ::std::uintptr_t buffer, ::std::size_t bytes) UWVM_THROWS
    {
#ifdef UWVM_CPP_EXCEPTIONS
        try { details::llvm_jit_throw_tuple_abi_bridge(module, tag, buffer, bytes); }
        catch(::uwvm2::runtime::exception::guest_exception const& caught)
        {
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
            debug_observe_uncaught_before_unwind(caught);
# endif
            throw;
        }
#else
        details::llvm_jit_throw_tuple_abi_bridge(module, tag, buffer, bytes);
#endif
    }
    extern "C++" [[noreturn]] void details::llvm_jit_debug_throw_ref_abi_bridge(
        ::std::uintptr_t module, ::std::uintptr_t reference) UWVM_THROWS
    {
#ifdef UWVM_CPP_EXCEPTIONS
        try { details::llvm_jit_throw_ref_abi_bridge(module, reference); }
        catch(::uwvm2::runtime::exception::guest_exception const& caught)
        {
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
            debug_observe_uncaught_before_unwind(caught);
# endif
            throw;
        }
#else
        details::llvm_jit_throw_ref_abi_bridge(module, reference);
#endif
    }
