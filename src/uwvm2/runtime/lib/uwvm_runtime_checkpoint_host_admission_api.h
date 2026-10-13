// Private member definitions, included INSIDE uwvm2::runtime::lib AFTER all
// actual runtime entries, publication guards and TLS accessors are defined.
// Not exported as a management API. No closed token/census/restore issuer exists here.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
extern "C++"
{
    runtime_checkpoint_host_bridge::status runtime_checkpoint_host_bridge::begin_registered_worker_startup(
        owner const& supplied, gate_type::host_operation& startup) noexcept
    {
        auto installed{g_runtime.checkpoint_host_state};
        if(!installed || !supplied || installed.get() != supplied.get() ||
           installed.owner_before(supplied) || supplied.owner_before(installed) ||
           !g_runtime.checkpoint_host_enabled.load(::std::memory_order_acquire)) { return status::invalid_context; }
        auto const& actual{*installed};
        if(actual.epoch_ != current_runtime_generation() || !actual.profile_ || !actual.control_ ||
           actual.profile_.get() != g_runtime.checkpoint_profile.get() ||
           actual.profile_.owner_before(g_runtime.checkpoint_profile) || g_runtime.checkpoint_profile.owner_before(actual.profile_) ||
           actual.control_.get() != g_runtime.debug_pause_control.get() ||
           actual.control_.owner_before(g_runtime.debug_pause_control) || g_runtime.debug_pause_control.owner_before(actual.control_))
        { return status::invalid_context; }
        // Only private initial factory invokes this under its REAL whole-worker
        // execution lease. Future closed-startup workers use their world token.
        return actual.gate_->try_enter(startup);
    }
    runtime_checkpoint_host_bridge::entry_scope::~entry_scope()
    {
        if(!linked_) { return; }
        // [this actual scope + its enclosing actual scope or null]
        // [safe] LIFO validation precedes restoring the borrowing TLS pointer.
        // The real execution scope still retains its lease during member teardown;
        // no fallback thread-state/map access occurs after runtime cleanup erased it.
        if(runtime_checkpoint_host_bridge::actual_entry_ != this) [[unlikely]]
        { ::fast_io::fast_terminate(); }
        runtime_checkpoint_host_bridge::actual_entry_ = previous_;
        previous_ = nullptr; linked_ = false;
        // setup_ and state_ then die in this order; neither pins VM resources.
    }
    [[nodiscard]] bool runtime_checkpoint_host_bridge::matches_actual_state(entry_scope const& entry) noexcept
    {
        // [privately installed scope] only compared/borrowed on its own native thread.
        // [safe] actual_entry_ is installed after ownership and cleared before this
        // scope dies; caller addresses never enter this path. The real lease pins
        // this generation even when the managed-page producer moves its lease.
        auto const lease{get_runtime_execution_lease()};
        if(!entry.linked_ || actual_entry_ != ::std::addressof(entry) || !entry.state_ ||
           lease == nullptr || !*lease || entry.depth_ != get_runtime_execution_entry_depth() ||
           entry.state_->epoch_ != current_runtime_generation() ||
           !g_runtime.checkpoint_profile || !g_runtime.debug_pause_control) { return false; }
        auto const& state{*entry.state_};
        // Immutable after quiescent installation until real generation drain.
        // Compare both control blocks before reading any caller-like pointee.
        return state.profile_.get() == g_runtime.checkpoint_profile.get() &&
            !state.profile_.owner_before(g_runtime.checkpoint_profile) &&
            !g_runtime.checkpoint_profile.owner_before(state.profile_) &&
            state.control_.get() == g_runtime.debug_pause_control.get() &&
            !state.control_.owner_before(g_runtime.debug_pause_control) &&
            !g_runtime.debug_pause_control.owner_before(state.control_);
    }
    runtime_checkpoint_host_bridge::owner runtime_checkpoint_host_bridge::create_for_actual_configuration(
        ::uwvm2::runtime::checkpoint::compilation_profile::owner const& profile) noexcept
    {
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(!profile || get_runtime_execution_entry_depth() != 0u || get_runtime_state_publication_depth() != 1u ||
           get_runtime_compilation_metadata_callback_depth() != 0u || !g_runtime.debug_pause_control ||
           mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only) { return {}; }
#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            // Private gate construction, only after the actual setter has acquired
            // execution-domain maintenance+admission quiescence and publication.
            auto gate{::std::unique_ptr<gate_type>{new gate_type{}}};
            return owner{new retained_state{profile, g_runtime.debug_pause_control,
                current_runtime_generation(), ::std::move(gate)}};
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(...) { return {}; } // Nothing installed; complete source stays unchanged.
#endif
    }
    runtime_checkpoint_host_bridge::status runtime_checkpoint_host_bridge::begin_actual_entry(entry_scope& entry) noexcept
    {
        // The only extra ordinary-mode operation: one cold acquire-load. A real
        // quiescent configure holds execution admission while publishing true,
        // so an already-admitted ordinary entry cannot miss a newly enabled gate.
        if(!g_runtime.checkpoint_host_enabled.load(::std::memory_order_acquire)) { return status::ok; }
        if(entry.linked_ || entry.state_) { return status::invalid_context; }
        auto const lease{get_runtime_execution_lease()};
        auto const depth{get_runtime_execution_entry_depth()};
        if(lease == nullptr || !*lease || depth == 0u || g_debug_observer_active) { return status::invalid_context; }
        // [actual admitted generation lease] -> [local strong retained-state pin]
        // [safe] configuration cannot change the owner while ANY actual lease is
        // live; reset/destruction drain those leases before retiring this slot.
        auto state{g_runtime.checkpoint_host_state};
        if(!state || state->epoch_ != current_runtime_generation()) { return status::invalid_context; }
        auto const admitted{state->gate_->try_enter(entry.setup_)};
        if(admitted != status::ok) { return admitted; } // Nonwaiting; do not silently call any target.
        entry.state_ = ::std::move(state); entry.depth_ = depth;
        // [live current scope] -> [new scope owns gate+operation]
        // [safe] publish the new borrowing pointer only AFTER complete ownership.
        entry.previous_ = actual_entry_; entry.linked_ = true; actual_entry_ = ::std::addressof(entry);
        if(!matches_actual_state(entry))
        {
            entry.state_->gate_->record_untracked_host();
            return status::invalid_context;
        }
        if(depth != 1u)
        {
            // Genuine host callback reentry keeps guest semantics and its original
            // outer host operation. We cannot serialize that native continuation.
            entry.state_->gate_->record_untracked_host();
        }
        return status::ok;
    }
    void runtime_checkpoint_host_bridge::actual_participant_admitted(entry_scope& entry) noexcept
    {
        if(!entry.state_) { return; }
        if(!matches_actual_state(entry) || g_debug_pause_participant == nullptr ||
           !*g_debug_pause_participant || g_debug_pause_participant->identifier() == 0u ||
           g_debug_activation_ledger == nullptr || g_checkpoint_shadow_ledger == nullptr)
        {
            entry.state_->gate_->record_untracked_host();
            return; // No proven guest participant: retain setup count until real exit.
        }
        // The runtime_debug_execution_scope producer installed this exact live
        // participant from the immutable same control. It, not a caller bool,
        // accounts for the guest after this nonwaiting setup operation is released.
        entry.setup_ = {};
    }
    runtime_checkpoint_host_bridge::status runtime_checkpoint_host_bridge::enter_actual_foreign_operation(
        foreign_operation_scope& output) noexcept
    {
        auto const entry{actual_entry_};
        if(entry == nullptr || !matches_actual_state(*entry) ||
           get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || g_debug_observer_active)
        { return status::invalid_context; }
        // [actual privately linked entry] real generation lease remains held.
        // [safe] copy only its own retained owner, never an address supplied by a
        // native target resolver. Typed tail may already have retired its frame;
        // gate authority must not infer an activation from offset/CFA/top frame.
        auto state{entry->state_};
        auto const admitted{state->gate_->try_enter(output.operation_)};
        if(admitted == status::ok) { output.state_ = ::std::move(state); }
        return admitted;
    }
    runtime_checkpoint_host_bridge::foreign_operation_scope::foreign_operation_scope() noexcept
        : status_{runtime_checkpoint_host_bridge::enter_actual_foreign_operation(*this)} {}
}
#endif
