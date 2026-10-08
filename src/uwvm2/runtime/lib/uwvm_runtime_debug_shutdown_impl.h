#pragma once
// Include only after ACTUAL runtime/execution/publication/host-entry definitions.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
extern "C++" ::std::uint_least32_t runtime_debug_shutdown_terminal_cleanup_abi_host_api() noexcept
{ return 2u; } // contract only; physical retirement still requires genuine finite OS join
// A genuine std::mutex ownership token must be unlocked on its original
// native management thread. Give each real cold thread a nonreused private
// incarnation; OS thread IDs/TLS addresses can be reused and are insufficient.
::std::uint_least64_t runtime_debug_shutdown_bridge::issuer() noexcept
{
    if(actual_issuer_ != 0u) { return actual_issuer_; }
    auto previous{next_issuer_.load(::std::memory_order_relaxed)};
    for(;;)
    {
        if(previous == UINT_LEAST64_MAX) { return 0u; }
        if(next_issuer_.compare_exchange_weak(previous, previous+1u, ::std::memory_order_relaxed))
        { actual_issuer_ = previous+1u; return actual_issuer_; }
    }
}
bool runtime_debug_shutdown_bridge::management_entry() noexcept
{
    return get_runtime_execution_entry_depth() == 0u && get_runtime_state_publication_depth() == 0u &&
        get_runtime_compilation_metadata_callback_depth() == 0u && !g_debug_observer_active &&
        !g_debug_close_observer_active && get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u &&
        !details::is_llvm_wasm_fp_environment_active(get_llvm_wasm_fp_environment_active_state());
}
llvm_jit_debug_shutdown_start runtime_debug_shutdown_bridge::begin(
    ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const& supplied) noexcept
{
    namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
    if(!management_entry()) { return {result::reentrant, {}}; }
    auto const issuer_id{issuer()};
    if(issuer_id == 0u) { return {result::unavailable_cleanup, {}}; }
    if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
       mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only)
    { return {result::unsupported_mode, {}}; }
    {
        ::std::lock_guard lock{mutex_};
        // Authenticate BOTH canonical pointee and privately minted control block
        // before reading any supplied owner. Same-domain retries retain the one
        // actual maintenance transaction; they do not mint another generation.
        if(actual_request_)
        {
            if(actual_request_->issuer_ != issuer_id) { return {result::reentrant, {}}; }
            if(same_owner(supplied, actual_request_->control_)) { return {result::started, actual_request_}; }
            return {result::busy, {}};
        }
    }
    if(runtime_checkpoint_retirement_bridge::management_blocked()) { return {result::busy, {}}; }
    auto transition{g_runtime.execution_domain.try_maintenance_transition()};
    if(!transition) { return {result::busy, {}}; }
    // REAL maintenance -> execution admission -> publication. Drop our own
    // admission before requesting stop/draining; never wait for our own lease.
    auto admission{g_runtime.execution_domain.try_enter()};
    if(!admission) { return {result::busy, {}}; }
    try
    {
        auto prepared{::std::shared_ptr<llvm_jit_debug_shutdown_request>{new llvm_jit_debug_shutdown_request{::std::move(transition)}}};
        {
            runtime_state_publication_guard publication{};
            if(!same_owner(supplied, g_runtime.debug_pause_control) || !g_runtime.debug_pause_control ||
               !g_runtime.compiled_all.load(::std::memory_order_acquire)) { return {result::stale_owner, {}}; }
            prepared->control_ = g_runtime.debug_pause_control;
            prepared->observer_ = g_runtime.debug_observer;
            prepared->epoch_ = current_runtime_generation(); prepared->issuer_ = issuer_id;
            if(prepared->epoch_ == 0u || g_runtime.modules.empty()) { return {result::stale_owner, {}}; }
            prepared->modules_.reserve(g_runtime.modules.size());
            for(auto const& rec : g_runtime.modules)
            {
                auto const* code{rec.llvm_jit_full_publication.get()};
                if(code == nullptr || !code->source || code->engine == nullptr || code->context == nullptr ||
                   code->debug_shutdown_abi_revision.load(::std::memory_order_acquire) != 1u || rec.runtime_module == nullptr)
                { return {result::unavailable_cleanup, {}}; }
                llvm_jit_debug_shutdown_request::module_pin pin{};
                pin.source = code->source; pin.publication = code;
                // Source/publication ownership and parsed declarations are
                // immutable under maintenance. Body-generation and replacement
                // vectors are updated by an ALL-park callback, not this mutex:
                // no concurrent mutable vector read occurs in begin().
                pin.function_count = rec.runtime_module->local_defined_function_vec_storage.size();
                prepared->modules_.push_back(::std::move(pin));
            }
            {
                ::std::lock_guard lock{mutex_};
                if(actual_request_ || runtime_checkpoint_retirement_bridge::management_blocked()) { return {result::busy, {}}; }
                actual_request_ = prepared;
                // Publish actual ownership/negative management gate before any
                // admitted guest resumes. New replacement commit checks this
                // negative atomic gate; an already-owned ALL-park commit must
                // finish before any real guest resumes. Neither is authorized
                // by a copied caller status/epoch.
                requested_.store(true, ::std::memory_order_release);
            }
        }
        admission.reset();
        // Existing atomic.wait cancellation maps to trap_fatal INSIDE its
        // noexcept host bridge. Do NOT deliver that old stop_token callback
        // while attempting managed cleanup. Actual execution stopping is still
        // published BEFORE cooperative guests unpark, so generated polls unwind
        // safely. Blocking waits/host syscalls truthfully remain admitted and
        // pending until they return; no host exception/side effect is invented.
        if(!g_runtime.execution_domain.request_stop_without_callbacks_owned_transition(prepared->maintenance_))
        { return {result::unavailable_cleanup, ::std::move(prepared)}; }
        // close() wakes the real cooperative guest park. Do NOT call on_close:
        // native-step cancellation may need an ACK from the actual worker and
        // that operation must remain a separate bounded CLI/controller phase.
        prepared->control_->close();
        return {result::started, ::std::move(prepared)};
    }
    catch(...) { return {result::unavailable_cleanup, {}}; }
}
runtime_debug_shutdown_bridge::result runtime_debug_shutdown_bridge::poll(owner const& supplied,
    ::std::uint_least64_t wait_milliseconds) noexcept
{
    if(!management_entry()) { return result::reentrant; }
    owner actual{};
    {
        ::std::lock_guard lock{mutex_};
        if(!same_owner(supplied, actual_request_)) { return result::stale_owner; }
        if(actual_request_->issuer_ != issuer()) { return result::reentrant; }
        actual = actual_request_;
    }
    ::std::unique_lock operation{actual->operation_, ::std::try_to_lock};
    if(!operation.owns_lock()) { return result::busy; }
    {
        ::std::lock_guard lock{mutex_};
        if(!same_owner(actual, actual_request_)) { return result::stale_owner; }
    }
    auto const bounded_ms{(::std::min)(wait_milliseconds, ::std::uint_least64_t{50u})};
    auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{bounded_ms}};
    // Only ACTUAL lease retirement authorizes this status; activation ACK,
    // debugger close, thread ID or an empty displayed call stack does not.
    if(!g_runtime.execution_domain.drain_owned_transition_until(actual->maintenance_, deadline))
    { return result::pending_execution; }
    // Genuine full mode does not start lazy/tiered compiler workers. Verify
    // actual retained scheduler storage, not mode flags alone. A producer that
    // unexpectedly remains live is explicit pending; never unbounded join here.
    if(g_runtime.lazy_scheduler.running() || g_runtime.lazy_scheduler.worker_count != 0u ||
       g_runtime.lazy_scheduler.queued_count.load(::std::memory_order_acquire) != 0u ||
       g_runtime.llvm_jit_urgent_scheduler.running() || g_runtime.llvm_jit_urgent_scheduler.worker_count != 0u ||
       g_runtime.llvm_jit_urgent_scheduler.queued_count.load(::std::memory_order_acquire) != 0u)
    { return result::pending_producers; }
# if defined(UWVM_RUNTIME_UWVM_INTERPRETER_LLVM_JIT_TIERED)
    if(g_runtime.tiered_urgent_scheduler.running() || g_runtime.tiered_urgent_scheduler.worker_count != 0u ||
       g_runtime.tiered_urgent_scheduler.queued_count.load(::std::memory_order_acquire) != 0u)
    { return result::pending_producers; }
# endif
    if(!::uwvm2::runtime::llvm_jit_cache::shutdown_async_store_objects_until(deadline))
    { return result::pending_producers; }
    return result::resources_quiescent;
}
bool runtime_debug_shutdown_bridge::release(owner const& supplied) noexcept
{
    if(poll(supplied, 0u) != result::resources_quiescent) { return false; }
    // A caller may retain its opaque diagnostic handle. Consume maintenance
    // explicitly, not by relying on last shared_ptr destruction. Serialize
    // against concurrent finite polls before changing the actual lease token.
    ::std::unique_lock operation{supplied->operation_, ::std::try_to_lock};
    if(!operation.owns_lock()) { return false; }
    owner retired{};
    {
        ::std::lock_guard lock{mutex_};
        if(!same_owner(supplied, actual_request_)) { return false; }
        retired = ::std::move(actual_request_);
        requested_.store(false, ::std::memory_order_release);
    }
    // Admission remains closed and runtime registries keep code/source/CFI.
    // This releases only actual maintenance after actual work drained. Caller
    // must still have joined its genuine host worker before destroying storage.
    retired->maintenance_ = {}; // cold real lock release; no source/code reset
    retired.reset(); return true;
}
bool runtime_debug_shutdown_bridge::skip_cancelled_entry() noexcept
{
    if(!requested_.load(::std::memory_order_acquire) || !runtime_execution_stop_requested_host_api()) { return false; }
    ::std::lock_guard lock{mutex_};
    // A real entry admitted immediately before close may not acquire a debug
    // participant. Return before executing any guest code/host effect instead
    // of minting an invented activation or throwing across an unknown boundary.
    return actual_request_ && actual_request_->epoch_ == current_runtime_generation() &&
        same_owner(actual_request_->control_, g_runtime.debug_pause_control);
}
void runtime_debug_shutdown_bridge::poll_after_safe_point(::std::uint64_t incarnation,
    ::std::uintptr_t module, ::std::uintptr_t function, ::std::size_t opcode_offset) UWVM_THROWS
{
    if(!requested_.load(::std::memory_order_acquire)) { return; }
    // Nested raw host callback entry may cross a C/noexcept host frame. Never
    // unwind through it; its actual lease stays pending until it cooperates.
    if(!catchable_full_entry_ || get_runtime_execution_entry_depth() != 1u ||
       !runtime_execution_stop_requested_host_api()) { return; }
    owner actual{};
    { ::std::lock_guard lock{mutex_}; actual = actual_request_; }
    auto const participant{g_debug_pause_participant}; auto const ledger{g_debug_activation_ledger};
    if(!actual || participant == nullptr || ledger == nullptr || !ledger->complete() ||
       ledger->count() == 0u ||
       g_debug_observer_active || g_debug_activation_park_site.before_park ||
       get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u ||
       !same_owner(actual->control_, g_runtime.debug_pause_control) || actual->epoch_ != current_runtime_generation())
    { return; } // no real cleanup/catch witness: retain actual pending execution
    auto const count{ledger->count()};
    // [scope-owned real activation ledger0..count-1] end
    // [safe] positive bounded count before selecting the actual leaf; never
    // convert a debugger/caller address or saved numeric ID into frame authority.
    auto const& leaf{ledger->data()[count-1u]};
    if(leaf.incarnation != incarnation || leaf.module != module || leaf.function != function ||
       leaf.runtime_epoch != actual->epoch_) { ::fast_io::fast_terminate(); }
    {
        runtime_state_publication_guard publication{};
        if(module >= actual->modules_.size() || module >= g_runtime.modules.size()) { ::fast_io::fast_terminate(); }
        // [actual module/source owners indexed by canonical module ordinal] end
        // [safe] BOTH exact extents checked before any indexed borrow.
        auto const& rec{g_runtime.modules.index_unchecked(module)}; auto const& pin{actual->modules_[module]};
        auto const* code{rec.llvm_jit_full_publication.get()};
        if(rec.runtime_module == nullptr || code != pin.publication || code == nullptr ||
           !same_owner(code->source, pin.source)) { ::fast_io::fast_terminate(); }
        if(code->debug_shutdown_abi_revision.load(::std::memory_order_acquire) != 1u)
        { return; } // genuine unqualified current body: retain pending, never force unwind
        auto const imports{rec.runtime_module->imported_function_vec_storage.size()};
        if(function < imports || function-imports >= pin.function_count) { ::fast_io::fast_terminate(); }
        // ALL-park replacement publication cannot coexist with this genuine
        // RUNNING activation. Actual immutable slot allocation is preserved
        // for the entire maintenance/execution lifetime; no dense saved ID is
        // promoted into a code owner. Raw-only modules keep original gen1.
        auto current_generation{::std::uint_least64_t{1u}};
        if(!rec.llvm_jit_debug_full_entry_generations.empty())
        {
            if(rec.llvm_jit_debug_full_entry_generations.size() != pin.function_count) { ::fast_io::fast_terminate(); }
            current_generation = rec.llvm_jit_debug_full_entry_generations[function-imports];
        }
        if(current_generation == 0u || current_generation != leaf.function_generation) { ::fast_io::fast_terminate(); }
        static_cast<void>(opcode_offset); // emitted identity is source-bounded by fused compiler
    }
    // No mutex/cohort/publication/GC lock survives this foreign unwind. The
    // private type cannot be constructed by Wasm or caught as guest_exception.
    throw signal{::std::move(actual), participant->identifier()};
}
bool runtime_debug_shutdown_bridge::accept_at_full_entry(signal const& caught) noexcept
{
    auto const participant{g_debug_pause_participant}; auto const ledger{g_debug_activation_ledger};
    if(participant == nullptr || ledger == nullptr || !ledger->valid() || ledger->count() != 0u ||
       participant->identifier() != caught.participant_ || get_runtime_execution_entry_depth() != 1u ||
       !runtime_execution_stop_requested_host_api() || g_debug_observer_active ||
       ::uwvm2::runtime::gc::current_root_frames() != nullptr)
    { return false; }
    {
        ::std::lock_guard lock{mutex_};
        if(!same_owner(caught.request_, actual_request_) || actual_request_->epoch_ != current_runtime_generation() ||
           !same_owner(actual_request_->control_, g_runtime.debug_pause_control)) { return false; }
    }
    // Actual private signal/participant/epoch AND generated activation/GC
    // cleanup were authenticated above. The still-admitted outer entry keeps
    // release/drain pending while we drop only failed, owned recording DATA.
    // An unavailable observation must not turn a genuine terminal unwind into
    // a trap. It remains unavailable; no checkpoint capture/resume is repaired.
    if(g_checkpoint_shadow_ledger != nullptr)
    {
        if(g_checkpoint_shadow_ledger->failure() != ::uwvm2::runtime::checkpoint::status::ok &&
           !g_checkpoint_shadow_ledger->discard_failed_recording()) { return false; }
        if(g_checkpoint_shadow_ledger->size() != 0u) { return false; }
    }
    return true;
    // Outer entry/debug/FP/GC/host RAII still runs AFTER this catch. Only the
    // later actual lifetime drain and physical join prove execution retired.
}
extern "C++" llvm_jit_debug_shutdown_start runtime_begin_llvm_jit_debug_shutdown_host_api(
    ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const& control) noexcept
{ return runtime_debug_shutdown_bridge::begin(control); }
extern "C++" llvm_jit_debug_shutdown_status runtime_poll_llvm_jit_debug_shutdown_host_api(
    llvm_jit_debug_shutdown_request_owner const& owner, ::std::uint_least64_t milliseconds) noexcept
{ return runtime_debug_shutdown_bridge::poll(owner, milliseconds); }
extern "C++" bool runtime_release_llvm_jit_debug_shutdown_host_api(llvm_jit_debug_shutdown_request_owner const& owner) noexcept
{ return runtime_debug_shutdown_bridge::release(owner); }
namespace details
{
    extern "C++" void const* llvm_jit_debug_shutdown_flag_address_host_api() noexcept
    {
#if defined(__linux__)
        if constexpr(::std::atomic<bool>::is_always_lock_free && sizeof(::std::atomic<bool>) == 1u && sizeof(bool) == 1u)
        { return runtime_debug_shutdown_bridge::requested_address(); }
#endif
        return nullptr;
    }
    extern "C++" void llvm_jit_debug_shutdown_poll_abi_bridge(::std::uint64_t incarnation,
        ::std::uintptr_t module, ::std::uintptr_t function, ::std::size_t offset) UWVM_THROWS
    { runtime_debug_shutdown_bridge::poll_after_safe_point(incarnation, module, function, offset); }
    extern "C++" void llvm_jit_debug_cancelled_wait_abi_bridge() UWVM_THROWS
    {
        auto const point{g_debug_wait_point};
        auto const ledger{g_debug_activation_ledger};
        if(!point.valid || ledger == nullptr || !ledger->complete() || ledger->count() == 0u ||
           get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || g_debug_observer_active ||
           g_debug_activation_park_site.before_park) { return; }
        // [actual scope-owned ledger0..count-1] end
        // [safe] complete positive count before borrowing its real leaf.
        // The wait and its suspension callbacks have already returned, so no
        // wait-shard/domain lock or stack queue registration crosses this throw.
        auto const& leaf{ledger->data()[ledger->count() - 1u]};
        if(leaf.module != point.location.code_unit || leaf.function != point.location.function ||
           leaf.runtime_epoch != point.location.code_generation) { return; }
        runtime_debug_shutdown_bridge::poll_after_safe_point(leaf.incarnation, leaf.module,
            leaf.function, point.location.offset);
        // Returning grants no cancellation authority. The caller retains the
        // original fatal cancellation path when no genuine managed request,
        // qualified generated cleanup or catchable full-entry boundary exists.
    }
}
#elif defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
extern "C++" ::std::uint_least32_t runtime_debug_shutdown_terminal_cleanup_abi_host_api() noexcept
{ return 0u; } // unsupported builds never promise qualified cleanup
extern "C++" llvm_jit_debug_shutdown_start runtime_begin_llvm_jit_debug_shutdown_host_api(
    ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const&) noexcept
{ return {llvm_jit_debug_shutdown_status::unsupported_mode, {}}; }
extern "C++" llvm_jit_debug_shutdown_status runtime_poll_llvm_jit_debug_shutdown_host_api(
    llvm_jit_debug_shutdown_request_owner const&, ::std::uint_least64_t) noexcept
{ return llvm_jit_debug_shutdown_status::unsupported_mode; }
extern "C++" bool runtime_release_llvm_jit_debug_shutdown_host_api(llvm_jit_debug_shutdown_request_owner const&) noexcept
{ return false; }
#endif
