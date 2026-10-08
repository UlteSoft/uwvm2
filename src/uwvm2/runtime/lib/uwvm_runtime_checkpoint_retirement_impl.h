// Include INSIDE runtime::lib AFTER actual canonical thread capture and all
// publication/TLS/entry definitions. This is execution retirement only. Full
// instance resources, file checkpoint loading and new epoch need a separate
// complete staged graph/publication transaction; this class cannot issue it.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
extern "C++"
{
    runtime_checkpoint_retirement_bridge::owner runtime_checkpoint_retirement_bridge::arm_current_while_cohort_owned(
        runtime_checkpoint_host_bridge::owner const& configuration,
        ::std::span<capture_owner const> captured,
        ::std::span<domain::stopped_participant const> actual_slots,
        gate::closed_admission& closed, value_policy policy) noexcept
    {
        namespace cp = ::uwvm2::runtime::checkpoint;
        // Compare the actual immutable slot before supplied pointee access.
        // The manager owns an actual lease+ONE complete ticket+N+publication;
        // no caller-created control block, roster, numeric ID or DATA enters.
        auto const installed{g_runtime.checkpoint_host_state};
        if(!same_owner(installed, configuration) || !g_runtime.checkpoint_host_enabled.load(::std::memory_order_acquire) ||
           captured.empty() || captured.size() > 256u || captured.size() != actual_slots.size() ||
           get_runtime_state_publication_depth() == 0u || get_llvm_jit_generated_wasm_bridge_entry_depth() != 0u ||
           g_debug_observer_active || !runtime_checkpoint_host_bridge::current_coherent_state(installed, closed) ||
           installed->epoch_ != current_runtime_generation() ||
           !same_owner(installed->control_, g_runtime.debug_pause_control) ||
           !same_owner(installed->profile_, g_runtime.checkpoint_profile) ||
           !installed->profile_ || installed->profile_->purpose() != cp::compilation_purpose::resumable)
        { return {}; }
        try
        {
            owner candidate{new request{installed, installed->control_, installed->profile_, installed->epoch_}};
            candidate->members.reserve(captured.size());
            for(::std::size_t i{}; i != captured.size(); ++i)
            {
                // [complete manager-owned canonical capture span0..N] end
                // [safe] i<N before registry resolution. An alias is compared
                // inside the real registry before ANY supplied capture read.
                auto actual{runtime_checkpoint_thread_capture::canonical(captured[i])};
                if(!actual || !same_owner(actual->control_, candidate->control) ||
                   !same_owner(actual->profile_, candidate->profile) || actual->frames_.empty() ||
                   actual->location_.code_generation != candidate->epoch || actual->participant_ == 0u)
                { return {}; }
                bool found{};
                for(auto const& slot : actual_slots)
                {
                    if(slot.id == actual->participant_ && slot.location == actual->location_) { found = true; break; }
                }
                if(!found) { return {}; }
                for(auto const& earlier : candidate->members)
                { if(earlier.captured->participant_ == actual->participant_) { return {}; } }
                // Same real source/plan/native code under publication and N.
                // Scalar continuation remains exact. Native-only retirement
                // may clean genuinely rooted references; it never replays them.
                full_code_publication const* actual_leaf_code{};
                for(auto const& saved : actual->frames_)
                {
                    full_code_publication const* code{};
                    ::uwvm2::uwvm::runtime::full::full_source_instance::owner source{};
                    if(runtime_checkpoint_thread_capture::check_actual_frame(saved.activation, saved.logical, code, source) !=
                        checkpoint_thread_capture_status::captured || code != saved.publication_identity ||
                       !same_owner(source, saved.source) ||
                       runtime_checkpoint_thread_capture::check_owned_values(saved.logical,
                           policy == value_policy::live_native_roots ? runtime_checkpoint_thread_capture::value_use::live_observation :
                           runtime_checkpoint_thread_capture::value_use::executable_scalar) != checkpoint_thread_capture_status::captured)
                    { return {}; }
                    // Borrow only the freshly resolved current publication;
                    // its actual generation lease/publication guard remains.
                    actual_leaf_code = code;
                }
                auto const& leaf_frame{actual->frames_.back()};
                auto const& leaf{leaf_frame.logical};
                if(!leaf.plan || leaf.plan->get().resume_abi_revision != 2u || leaf.plan->get().retirement_abi_revision != 1u ||
                   leaf.site == 0u || leaf.site > leaf.plan->get().sites.size())
                { return {}; }
                // Resolve the actual native continuation entry independently
                // of the immutable DATA site's ABI label. No saved address is
                // read/dereferenced, and this request exports no executable PC.
                auto const module_id{leaf_frame.activation.module};
                if(module_id >= g_runtime.modules.size() || actual_leaf_code == nullptr) { return {}; }
                // [actual generation-owned module registry0..N] end
                // [safe] module_id<N before actual record/pointer selection.
                auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(module_id))};
                auto const module{record.runtime_module};
                if(module == nullptr || record.llvm_jit_full_publication.get() != actual_leaf_code) { return {}; }
                auto const imports{module->imported_function_vec_storage.size()};
                auto const function{leaf_frame.activation.function};
                if(function < imports || function-imports >= actual_leaf_code->checkpoint_resume_entries.size()) { return {}; }
                auto const local{static_cast<::std::size_t>(function-imports)};
                // [ALL-symbol-resolved current entry vector0 ... local ... L] end
                // [safe] local<L before exact plan/real native address comparison.
                auto const& resolved{actual_leaf_code->checkpoint_resume_entries[local]};
                if(resolved.address == 0u || !same_owner(resolved.plan, leaf.plan)) { return {}; }
                bool landing{};
                for(auto site : leaf.plan->get().resume_sites) { landing |= site == leaf.site; }
                bool actual_poll{};
                for(auto site : leaf.plan->get().retirement_sites) { actual_poll |= site == leaf.site; }
                if(!landing || !actual_poll) { return {}; }
                candidate->members.push_back({::std::move(actual), member_phase::parked});
            }
            // Transfer the genuine already closed guard only AFTER every
            // allocation and owner check. It remains closed across cleanup.
            ::std::lock_guard lock{mutex_};
            if(actual_request_ || requested_.load(::std::memory_order_relaxed)) { return {}; }
            candidate->closed = ::std::move(closed);
            actual_request_ = candidate;
            management_blocked_.store(true, ::std::memory_order_release);
            return candidate;
        }
        catch(...) { return {}; }
    }

    bool runtime_checkpoint_retirement_bridge::activate_after_cohort_unlock(owner const& supplied) noexcept
    {
        if(get_runtime_state_publication_depth() != 0u || get_runtime_execution_entry_depth() != 0u ||
           get_llvm_jit_generated_wasm_bridge_entry_depth() != 0u || g_debug_observer_active) { return false; }
        {
            ::std::lock_guard lock{mutex_};
            if(!same_owner(actual_request_, supplied) || requested_.load(::std::memory_order_relaxed) ||
               actual_request_->phase != request_phase::prepared) { return false; }
            actual_request_->phase = request_phase::activating;
        }
        // Actual execution API acquires admission and invokes cancellation
        // callbacks after dropping it. No request/cohort/pub/N mutex is held.
        // Guests remain genuinely parked and cannot run a new Wasm opcode yet.
        g_runtime.execution_domain.request_stop();
        {
            ::std::lock_guard lock{mutex_};
            if(!same_owner(actual_request_, supplied) || actual_request_->phase != request_phase::activating) { return false; }
            actual_request_->phase = request_phase::active;
            requested_.store(true, ::std::memory_order_release);
        }
        return true;
    }

    bool runtime_checkpoint_retirement_bridge::cancel_unactivated_after_cohort_unlock(owner const& supplied) noexcept
    {
        if(get_runtime_state_publication_depth() != 0u || get_runtime_execution_entry_depth() != 0u ||
           get_llvm_jit_generated_wasm_bridge_entry_depth() != 0u || g_debug_observer_active) { return false; }
        owner retired{};
        {
            ::std::lock_guard lock{mutex_};
            if(!same_owner(actual_request_, supplied) || requested_.load(::std::memory_order_relaxed) ||
               actual_request_->phase != request_phase::prepared) { return false; }
            retired = ::std::move(actual_request_);
            management_blocked_.store(false, ::std::memory_order_release);
        }
        retired->closed = gate::closed_admission{};
        return true;
    }

    void runtime_checkpoint_retirement_bridge::poll_current_after_park(::std::uint64_t incarnation,
        ::std::uintptr_t module, ::std::uintptr_t function, ::std::size_t opcode_offset) UWVM_THROWS
    {
        if(!requested_.load(::std::memory_order_acquire)) { return; }
        auto const participant{g_debug_pause_participant};
        auto const activations{g_debug_activation_ledger}; auto const shadow{g_checkpoint_shadow_ledger};
        if(participant == nullptr || activations == nullptr || shadow == nullptr || incarnation == 0u ||
           !activations->complete() || !shadow->has_complete_materialized_frames() || g_debug_observer_active ||
           g_debug_activation_park_site.before_park || get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u)
        { ::fast_io::fast_terminate(); }
        owner actual{}; ::std::size_t selected{};
        {
            ::std::lock_guard lock{mutex_}; actual = actual_request_;
            if(!actual || actual->phase != request_phase::active) { ::fast_io::fast_terminate(); }
            bool found{};
            for(::std::size_t i{}; i != actual->members.size(); ++i)
            {
                // [actual request-owned members0 ... i ... N] end
                // [safe] i<N before native participant comparison; no guest
                // value supplies an owner, stop ticket or incarnation issuer.
                if(actual->members[i].captured->participant_ == participant->identifier())
                { selected = i; found = true; break; }
            }
            if(!found || actual->members[selected].phase != member_phase::parked) { ::fast_io::fast_terminate(); }
        }
        auto const captured{actual->members[selected].captured};
        auto const logical{shadow->frames_while_actually_stopped()}; auto const count{activations->count()};
        if(!same_owner(actual->configuration, g_runtime.checkpoint_host_state) ||
           !same_owner(actual->control, g_runtime.debug_pause_control) || !same_owner(actual->profile, g_runtime.checkpoint_profile) ||
           actual->epoch != current_runtime_generation() || count == 0u || count != logical.size() || count != captured->frames_.size() ||
           captured->location_.code_unit != module ||
           captured->location_.function != function || captured->location_.offset != opcode_offset ||
           captured->location_.code_generation != actual->epoch)
        { ::fast_io::fast_terminate(); }
        auto const native{activations->data()};
        // [actual bounded activation ledger0 ... count-1] end
        // [safe] positive count<=fixed capacity above BEFORE leaf selection.
        if(native[count-1u].incarnation != incarnation) { ::fast_io::fast_terminate(); }
        for(::std::size_t i{}; i != count; ++i)
        {
            // [scope-owned native/shadow arrays and request-owned copied frames]
            // [safe] equal complete counts above precede every indexed borrow.
            auto const& saved{captured->frames_[i]}; auto const& live{native[i]};
            if(live.incarnation != saved.activation.incarnation || live.parent != saved.activation.parent ||
               live.continuation != saved.activation.continuation || live.runtime_epoch != saved.activation.runtime_epoch ||
               live.module != saved.activation.module || live.function != saved.activation.function ||
               live.function_generation != saved.activation.function_generation || live.island != saved.activation.island ||
               logical[i].identity != saved.logical.identity || logical[i].site != saved.logical.site ||
               !same_owner(logical[i].plan, saved.logical.plan)) { ::fast_io::fast_terminate(); }
        }
        {
            // Re-resolve code/source under actual publication; raw saved
            // publication addresses are comparison tokens only, never owners.
            runtime_state_publication_guard publication{};
            for(auto const& saved : captured->frames_)
            {
                full_code_publication const* code{};
                ::uwvm2::uwvm::runtime::full::full_source_instance::owner source{};
                if(runtime_checkpoint_thread_capture::check_actual_frame(saved.activation, saved.logical, code, source) !=
                    checkpoint_thread_capture_status::captured || code != saved.publication_identity || !same_owner(source, saved.source))
                { ::fast_io::fast_terminate(); }
            }
        }
        {
            ::std::lock_guard lock{mutex_};
            if(!same_owner(actual_request_, actual) || !requested_.load(::std::memory_order_relaxed) ||
               selected >= actual->members.size() || actual->members[selected].phase != member_phase::parked)
            { ::fast_io::fast_terminate(); }
            actual->members[selected].phase = member_phase::signalled;
        }
        // No request/publication/cohort/GC mutex is held during unwind. This
        // private foreign C++ type does not match the Wasm guest_exception type;
        // catch_all Wasm clauses must propagate it through cleanup-only edges.
        throw signal{::std::move(actual), selected};
    }

    bool runtime_checkpoint_retirement_bridge::accept_at_full_entry(signal const& caught) noexcept
    {
        auto const participant{g_debug_pause_participant}; auto const activations{g_debug_activation_ledger};
        auto const shadow{g_checkpoint_shadow_ledger};
        if(participant == nullptr || activations == nullptr || shadow == nullptr || g_debug_observer_active ||
           get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || !activations->valid() || activations->count() != 0u ||
           shadow->failure() != ::uwvm2::runtime::checkpoint::status::ok || shadow->size() != 0u)
        { return false; }
        ::std::lock_guard lock{mutex_};
        if(!same_owner(actual_request_, caught.request_) || actual_request_->phase != request_phase::active ||
           !requested_.load(::std::memory_order_relaxed) ||
           caught.member_ >= actual_request_->members.size()) { return false; }
        auto& member{actual_request_->members[caught.member_]};
        if(member.phase != member_phase::signalled || member.captured->participant_ != participant->identifier() ||
           !same_owner(actual_request_->configuration, g_runtime.checkpoint_host_state) ||
           actual_request_->epoch != current_runtime_generation()) { return false; }
        member.phase = member_phase::native_frames_cleaned;
        // This ACK is diagnostic/lifecycle only; actual domain drain must still
        // wait outer debug-participant, GC, FP, host and execution lease RAII.
        return true;
    }

    runtime_checkpoint_retirement_bridge::drain_result runtime_checkpoint_retirement_bridge::release_after_actual_execution_drain(owner const& supplied) noexcept
    {
        if(get_runtime_execution_entry_depth() != 0u || get_runtime_state_publication_depth() != 0u ||
           get_llvm_jit_generated_wasm_bridge_entry_depth() != 0u || g_debug_observer_active) { return drain_result::invalid_request; }
        owner retired{}; bool all_signalled{true};
        {
            ::std::lock_guard lock{mutex_};
            if(!same_owner(actual_request_, supplied) || actual_request_->phase != request_phase::active ||
               !requested_.load(::std::memory_order_relaxed)) { return drain_result::invalid_request; }
            for(auto const& member : actual_request_->members)
            { all_signalled = all_signalled && member.phase == member_phase::native_frames_cleaned; }
            // A concurrently closed real pause domain can let an entry leave
            // normally before observing our private signal. Actual execution
            // drain still proves every old native/root/host lease is gone.
            // Preserve the distinction in the result; an ACK is never drain.
            retired = ::std::move(actual_request_); requested_.store(false, ::std::memory_order_release);
            management_blocked_.store(false, ::std::memory_order_release);
        }
        // Actual trusted drain callback owns the lifecycle proof; no copied ACK
        // or participant ID may call this sole-manager private method. Release
        // the retained old host gate outside every request/runtime lock, only
        // after all old shared GC/root/host/execution owners really retired.
        retired->closed = gate::closed_admission{}; // public move assignment releases the old private guard
        return all_signalled ? drain_result::all_frames_signalled : drain_result::actual_drain_without_all_signals;
    }
}
namespace details
{
    extern "C++" void llvm_jit_checkpoint_retirement_poll_abi_bridge(::std::uint64_t incarnation,
        ::std::uintptr_t module, ::std::uintptr_t function, ::std::size_t opcode_offset) UWVM_THROWS
    { runtime_checkpoint_retirement_bridge::poll_current_after_park(incarnation, module, function, opcode_offset); }
}
#endif
