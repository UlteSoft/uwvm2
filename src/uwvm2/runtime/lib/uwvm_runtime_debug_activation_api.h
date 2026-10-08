// Exact-event metadata queries. The joint query validates source position and
// the event chain in ONE stopped transaction. Neither query grants locals,
// address, memory, expression or executable-code ownership to its metadata.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
    namespace
    {
        // Called under the originating actual before-park lease (which is not
        // parked and therefore excludes replacement), or the query publication
        // guard. A replacement's retained engine is its real code owner; the
        // original full publisher alone must not impersonate that generation.
        template<typename Record>
        [[nodiscard]] void const* debug_activation_current_code_owner(Record const& rec,
            ::std::uint64_t module_id, ::std::uint64_t function, ::std::size_t local, ::std::uint64_t generation) noexcept
        {
            if(!rec.llvm_jit_full_publication || !rec.llvm_jit_full_publication->engine ||
               !rec.llvm_jit_full_publication->context || rec.llvm_jit_full_publication->plan || generation == 0u) { return nullptr; }
            if(generation == 1u) { return rec.llvm_jit_full_publication.get(); }
            if(local >= rec.llvm_jit_debug_full_typed_entry_targets.size()) { return nullptr; }
            for(auto const& owner : rec.llvm_jit_debug_full_retained_generations)
            {
                if(owner && owner->committed && owner->metadata_ready && owner->engine && owner->context &&
                   owner->module_id == module_id && owner->function_index == function && owner->local_index == local &&
                   owner->expected_generation == generation - 1u && owner->typed_entry != 0u &&
                   owner->typed_entry == rec.llvm_jit_debug_full_typed_entry_targets.index_unchecked(local))
                {
                    // [actual committed retained engine owner] lease/guard
                    // [safe                                  ] keeps it live;
                    //  ^^ comparison-only pointer, never an executable/native-stack read.
                    return owner.get();
                }
            }
            return nullptr;
        }
    }
#endif
    extern "C++" llvm_jit_debug_activation_capture_owner llvm_jit_debug_capture_activation_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket) noexcept
    {
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        if(!ticket || !g_debug_observer_active || !g_debug_activation_park_site.before_park ||
           g_debug_pause_participant == nullptr || g_debug_activation_ledger == nullptr ||
           !g_debug_activation_ledger->complete() || !g_runtime.debug_pause_control) { return {}; }
        try
        {
            auto const control{g_runtime.debug_pause_control};
            // The actual participant has not parked yet. A current request must
            // therefore be incomplete; stale/closed/cancelled tickets fail. No
            // publication/domain nesting occurs in this before-park operation.
            if(control->capture(ticket).result != ::uwvm2::utils::thread::cooperative_pause_result::timeout) { return {}; }
            auto const location{g_debug_activation_park_site.location};
            auto const count{g_debug_activation_ledger->count()};
            // [real TLS scope-owned fixed ledger] frame_end
            // [safe                             ] bounded count and complete same-island chain;
            //  ^^ borrow only during the synchronous callback before the guest parks.
            auto const frames{g_debug_activation_ledger->data()};
            if(count == 0u ||
               frames[count - 1u].module != location.code_unit || frames[count - 1u].function != location.function ||
               frames[count - 1u].runtime_epoch != location.code_generation) { return {}; }
            ::std::shared_ptr<llvm_jit_debug_activation_capture> candidate{new llvm_jit_debug_activation_capture};
            candidate->control_ = control; candidate->ticket_ = ticket;
            // Retain the ACTUAL dynamically owned debug ledger before-park.
            // Captured identities do not keep a physical scope alive: its real
            // worker poisons this same ledger after native ACK at scope exit.
            // Copy control-block ownership only here, never in a signal handler.
            if(g_debug_activation_ledger_owner == nullptr ||
               g_debug_activation_ledger_owner->get() != g_debug_activation_ledger) { return {}; }
            candidate->native_ledger_owner_ = *g_debug_activation_ledger_owner;
            if(g_debug_native_stack_scope != nullptr) { candidate->native_stack_owner_ = g_debug_native_stack_scope->pin(); }
            // Code origin and machine-step permission are distinct contracts.
            // The noinline debug bridge supplies an authentic cooperative code
            // site without manufacturing an OS native-thread/trap identity.
            candidate->native_code_site_ = llvm_jit_capture_debug_native_code_site_host_api();
            candidate->native_site_ = llvm_jit_capture_debug_native_step_site_host_api();
            candidate->snapshot_.participant = g_debug_pause_participant->identifier();
            candidate->snapshot_.location = location;
            candidate->snapshot_.frames.reserve(count); candidate->code_owners_.reserve(count);
            for(::std::size_t i{}; i != count; ++i)
            {
                auto const& frame{frames[i]};
                if(frame.module >= g_runtime.modules.size() || frame.runtime_epoch != location.code_generation) { return {}; }
                auto const& rec{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(frame.module))};
                auto const* module{rec.runtime_module};
                if(module == nullptr || !rec.llvm_jit_ready || !rec.llvm_jit_full_publication ||
                   !rec.llvm_jit_full_publication->engine || !rec.llvm_jit_full_publication->context ||
                   rec.llvm_jit_full_publication->plan) { return {}; }
                auto const imports{module->imported_function_vec_storage.size()};
                if(frame.function < imports || frame.function - imports >= rec.llvm_jit_debug_full_entry_generations.size() ||
                   rec.llvm_jit_debug_full_entry_generations[static_cast<::std::size_t>(frame.function - imports)] != frame.function_generation)
                { return {}; }
                candidate->snapshot_.frames.push_back({frame.incarnation, frame.parent, frame.continuation,
                    frame.module, frame.function, frame.function_generation, frame.runtime_epoch});
                // [actual full/retained native owner] originating outer lease
                // [safe                            ] owns its lifetime. Save
                //  ^^ only the checked comparison token; query reacquires a genuine lease.
                auto const code_owner{debug_activation_current_code_owner(rec, frame.module, frame.function,
                    static_cast<::std::size_t>(frame.function - imports), frame.function_generation)};
                if(code_owner == nullptr) { return {}; }
                candidate->code_owners_.push_back(code_owner);
            }
            // Publish only the complete privately minted owner. Old live labels
            // can exhaust the bounded cold registry; that returns unavailable,
            // never evicts a live owner or accepts an alias as its replacement.
            ::std::lock_guard lock{g_debug_activation_capture_mutex};
            for(auto& slot : g_debug_activation_captures)
            { if(slot.expired()) { slot = candidate; return candidate; } }
            return {};
        }
        catch(...) { return {}; }
#else
        (void)ticket; return {};
#endif
    }
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
    namespace
    {
        [[nodiscard]] llvm_jit_debug_activation_capture_owner debug_activation_canonical_capture(
            llvm_jit_debug_activation_capture_owner const& supplied) noexcept
        {
            if(!supplied) { return {}; }
            llvm_jit_debug_activation_capture_owner canonical{};
            {
                ::std::lock_guard lock{g_debug_activation_capture_mutex};
                for(auto const& slot : g_debug_activation_captures)
                {
                    auto const owner{slot.lock()};
                    if(owner && owner.get() == supplied.get() && !owner.owner_before(supplied) && !supplied.owner_before(owner))
                    { canonical = owner; break; }
                }
            }
            return canonical;
        }
    }
    bool llvm_jit_debug_activation_capture::matches_publication_locked(
        ::uwvm2::utils::thread::cooperative_pause_location actual, bool native) const noexcept
    {
        if(native_return_anchor_)
        {
            // Native-only rebasing never changes the cooperative domain slot.
            // Default/source callers cannot reinterpret that old slot as parent
            // locals or memory. The original exact ticket/publication stays live.
            if(!native || native_return_anchor_->native_return_anchor_ ||
               !debug_activation_canonical_capture(native_return_anchor_) ||
               !native_return_anchor_->matches_publication_locked(actual)) { return false; }
            actual = snapshot_.location;
        }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        // The genuine stopped query already owns the publication guard. A
        // private no-trace publication cannot impersonate debug-full code.
        if(runtime_native_eh_private_leaf_management_blocked()) { return false; }
#endif
        if(!control_ || !ticket_ || snapshot_.participant == 0u || snapshot_.frames.empty() ||
           code_owners_.size() != snapshot_.frames.size()) { return false; }
        if(g_runtime.debug_pause_control.get() != this->control_.get() ||
           g_runtime.debug_pause_control.owner_before(this->control_) ||
           this->control_.owner_before(g_runtime.debug_pause_control) ||
           !g_runtime.compiled_all.load(::std::memory_order_acquire) || actual.code_generation != current_runtime_generation() ||
           actual.code_unit != snapshot_.location.code_unit || actual.function != snapshot_.location.function ||
           actual.offset != snapshot_.location.offset || actual.code_generation != snapshot_.location.code_generation) { return false; }
        for(::std::size_t i{}; i != snapshot_.frames.size(); ++i)
        {
            auto const& frame{snapshot_.frames[i]};
            if(frame.module >= g_runtime.modules.size() || frame.runtime_epoch != actual.code_generation ||
               frame.incarnation == 0u || frame.continuation == 0u ||
               frame.parent != (i == 0u ? 0u : snapshot_.frames[i - 1u].incarnation)) { return false; }
            auto const& rec{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(frame.module))};
            auto const* module{rec.runtime_module};
            auto const& publication{rec.llvm_jit_full_publication};
            if(module == nullptr || !rec.llvm_jit_ready || !publication || !publication->engine || !publication->context ||
               publication->plan) { return false; }
            auto const imports{module->imported_function_vec_storage.size()};
            if(frame.function < imports || frame.function - imports >= rec.llvm_jit_debug_full_entry_generations.size() ||
               rec.llvm_jit_debug_full_entry_generations[static_cast<::std::size_t>(frame.function - imports)] != frame.function_generation)
            { return false; }
            if(debug_activation_current_code_owner(rec, frame.module, frame.function,
               static_cast<::std::size_t>(frame.function - imports), frame.function_generation) != this->code_owners_[i]) { return false; }
        }
        auto const& current{snapshot_.frames.back()};
        if(current.module != actual.code_unit || current.function != actual.function) { return false; }
        return true;
    }
#endif
    extern "C++" bool llvm_jit_debug_query_activation_host_api(
        llvm_jit_debug_activation_capture_owner const& capture,
        llvm_jit_debug_activation_snapshot& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!capture || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const canonical{debug_activation_canonical_capture(capture)};
            // Never dereference a supplied alias/foreign control block. The
            // private registry supplied this real, fully owned immutable copy.
            if(!canonical) { return false; }
            auto const& saved{canonical->snapshot_};
            if(!canonical->control_ || !canonical->ticket_ || saved.participant == 0u || saved.frames.empty() ||
               canonical->code_owners_.size() != saved.frames.size()) { return false; }
            // Real cold native reader excludes the original managed sweep.
            auto gc_reader{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_enter()};
            if(!gc_reader) { return false; }
            auto lease{g_runtime.execution_domain.try_enter()}; if(!lease) { return false; }
            bool valid{};
            auto const stopped{canonical->control_->with_stopped_participant(canonical->ticket_, saved.participant,
                [&](auto const actual)
            {
                runtime_state_publication_guard lock{};
                if(!canonical->matches_publication_locked(actual)) { return; }
                // Transactional owned output only. Allocation failure unwinds
                // both guards and clears every partial copied identity below.
                out = saved; valid = true;
            })};
            if(!stopped || !valid) { out = {}; return false; }
            return true;
        }
        catch(...) { out = {}; return false; }
#else
        (void)capture; return false;
#endif
    }
    extern "C++" bool llvm_jit_debug_query_source_activation_host_api(
        llvm_jit_debug_activation_capture_owner const& capture,
        llvm_jit_debug_source_binding_owner const& binding,
        llvm_jit_debug_source_activation_snapshot& out) noexcept
    {
        out = {};
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!capture || mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
           !details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
           !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
           !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth())) { return false; }
        try
        {
            auto const canonical{debug_activation_canonical_capture(capture)};
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
                if(!canonical->matches_publication_locked(actual)) { return; }
                if(binding && !llvm_jit_debug_source_binding::canonical_locked(binding)) { return; }
                llvm_jit_debug_source_activation_snapshot candidate{};
                // This SAME locked actual position supplies Code-relative PC.
                // A missing/stale/foreign source binding produces unavailable
                // source, never an unchecked substituted decimal/native PC.
                candidate.source_available = llvm_jit_debug_source_binding::position_locked(binding, actual, candidate.source);
                if(candidate.source_available &&
                   (candidate.source.module != actual.code_unit || candidate.source.function != actual.function ||
                    candidate.source.runtime_epoch != actual.code_generation ||
                    candidate.source.function_generation != canonical->snapshot_.frames.back().function_generation)) { return; }
                auto const context{llvm_jit_debug_source_binding::canonical_locked(binding)};
                if(context && context->module_id_ == actual.code_unit && context->runtime_epoch_ == actual.code_generation &&
                   actual.code_unit < g_runtime.modules.size())
                {
                    auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(actual.code_unit))};
                    auto const& publication{record.llvm_jit_full_publication};
                    auto const& frame{canonical->snapshot_.frames.back()};
                    auto const* module{record.runtime_module};
                    if(module != nullptr && publication && publication.get() == context->publication_ &&
                       publication->debug_source_runtime_epoch == actual.code_generation &&
                       record.llvm_jit_debug_source_fused_epoch == actual.code_generation &&
                       context->source_ && publication->source.get() == context->source_.get() &&
                       !publication->source.owner_before(context->source_) && !context->source_.owner_before(publication->source) &&
                       context->source_->actual_validated_file(static_cast<::std::size_t>(actual.code_unit),actual.code_generation,module) != nullptr &&
                       actual.function >= module->imported_function_vec_storage.size())
                    {
                        auto const local{actual.function-module->imported_function_vec_storage.size()};
                        if(local < context->image_.functions.size() && local < record.llvm_jit_debug_full_entry_generations.size())
                        {
                            auto const& original{context->image_.functions[static_cast<::std::size_t>(local)]};
                            candidate.source_context_available = original.function == actual.function && original.expression_size != 0u &&
                                actual.offset < original.expression_size && frame.module == actual.code_unit && frame.function == actual.function &&
                                frame.runtime_epoch == actual.code_generation && frame.function_generation == original.function_generation &&
                                frame.function_generation == record.llvm_jit_debug_full_entry_generations[static_cast<::std::size_t>(local)];
                        }
                    }
                }
                candidate.activation = canonical->snapshot_;
                out = ::std::move(candidate); valid = true;
            })};
            if(!stopped || !valid) { out = {}; return false; }
            return true;
        }
        catch(...) { out = {}; return false; }
#else
        (void)capture; (void)binding; return false;
#endif
    }
#endif
