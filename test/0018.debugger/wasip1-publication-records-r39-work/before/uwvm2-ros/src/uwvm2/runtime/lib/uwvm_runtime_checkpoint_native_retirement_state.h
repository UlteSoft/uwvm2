// Private member fragment of the coherent manager. A canonical operation owns
// the SAME retired cohort and prepared lifetime across deadline/pending returns.
#pragma once
struct native_retirement_state
{
    using execution = ::uwvm2::utils::thread::execution_domain;
    ::std::shared_ptr<llvm_jit_checkpoint_native_retirement> token{
        new llvm_jit_checkpoint_native_retirement{}};
    runtime_checkpoint_guest_worker_bridge::pending_join_owner joined{};
    execution::prepared_generation generation{};
    retirement::owner request{};
    source_owner source{};
    host_bridge::owner host{};
    ::std::chrono::steady_clock::time_point deadline{};
    llvm_jit_checkpoint_native_retirement_result result{};
    bool activated{};
    bool prepare_candidate{};
    llvm_jit_checkpoint_prepare_request preparation_request{};
    ::std::shared_ptr<runtime_checkpoint_world_transaction> candidate{};
};
inline static ::std::mutex native_retirement_mutex_{};
inline static ::std::shared_ptr<native_retirement_state> native_retirement_{};
inline static ::std::weak_ptr<llvm_jit_checkpoint_native_retirement const> completed_native_retirement_{};
inline static llvm_jit_checkpoint_native_retirement_result completed_native_retirement_result_{};

[[nodiscard]] static bool native_retirement_context_allowed() noexcept
{
    namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
    return mode::global_runtime_mode == mode::runtime_mode_t::full_compile &&
        mode::global_runtime_compiler == mode::runtime_compiler_t::llvm_jit_only &&
        get_runtime_execution_entry_depth() == 0u && get_runtime_state_publication_depth() == 0u &&
        get_runtime_compilation_metadata_callback_depth() == 0u &&
        get_llvm_jit_generated_bridge_scope_depth() == 0u && !g_debug_observer_active;
}
[[nodiscard]] static bool check_actual_candidate_source_bindings(
    ::std::shared_ptr<runtime_checkpoint_world_transaction> const&) noexcept;
static void finish_native_retirement_until(native_retirement_state& actual,
    ::uwvm2::utils::thread::execution_domain::maintenance_transition const& maintenance) noexcept
{
    using execution = ::uwvm2::utils::thread::execution_domain;
    using outcome = llvm_jit_checkpoint_native_retirement_status;
    if(!actual.activated || !actual.request || !actual.generation ||
       actual.result.runtime_epoch != current_runtime_generation() ||
       !same_owner(actual.source, ::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()) ||
       !same_owner(actual.host, g_runtime.checkpoint_host_state))
    { actual.result.status = outcome::failed_closed; return; }
    auto drained{g_runtime.execution_domain.drain_prepared_generation_until(maintenance, actual.generation, actual.deadline)};
    if(drained.status == execution::prepared_drain_status::pending_execution)
    { actual.result.status = outcome::pending_execution; return; }
    if(drained.status != execution::prepared_drain_status::drained || !drained.actual)
    { actual.result.status = outcome::failed_closed; return; }
    actual.result.execution_drained = true;
    auto physical{runtime_checkpoint_guest_worker_bridge::join_after_actual_drain_until(
        actual.joined, maintenance, actual.generation, drained.actual, actual.deadline)};
    if(!physical)
    { actual.result.status = outcome::pending_native_join; return; }
    if(!runtime_checkpoint_guest_worker_bridge::current_physical_join(actual.joined, physical, actual.request))
    { actual.result.status = outcome::failed_closed; return; }
    actual.result.native_workers_joined = true;
    if(actual.prepare_candidate && actual.candidate)
    {
        // The actual generation is drained AND the sealed native cohort has
        // physically joined, including TLS destructors. Retain source, host
        // gate, drain generation and candidate. No reset/open/publication here.
        // Recheck the complete fixed source/parser/dense binding roster and
        // unsealed candidate GC state AFTER this authentic drain and OS/TLS
        // join. A copied diagnostic cannot satisfy these ownership checks.
        if(!check_actual_candidate_source_bindings(actual.candidate))
        { actual.result.status=outcome::failed_closed;return; }
        actual.result.source_initializer_bindings_rechecked_after_physical_join=true;
        actual.result.candidate_world_retained=true;
        actual.result.status=outcome::prepared_world_ready_closed;
        return;
    }
    retirement::drain_result released{retirement::drain_result::invalid_request};
    auto const published{g_runtime.execution_domain.publish_prepared_drained_reset(
        maintenance, actual.generation, drained.actual, [&](auto const&) noexcept
        {
            // SAME retained instance: no Wasm/source/GC/code/epoch mutation.
            // The state still owns the request/cohort, so releasing its closed
            // gate cannot destroy providers/captures under admission_mutex.
            released = retirement::release_after_actual_execution_drain(actual.request);
            return released != retirement::drain_result::invalid_request;
        })};
    if(published != execution::prepared_reset_result::published_closed)
    { actual.result.status = outcome::failed_closed; return; }
    // ALL old native workers are physically reaped. Reopening the retained
    // unchanged instance needs no restored-world worker enrollment. This does
    // not open a candidate world or issue complete-instance restore permission.
    if(!g_runtime.execution_domain.open_published_startup_generation(
        maintenance, actual.generation, []() noexcept { return true; }))
    { actual.result.status = outcome::failed_closed; return; }
    actual.result.all_frames_signalled = released == retirement::drain_result::all_frames_signalled;
    actual.result.status = outcome::retired_and_joined;
}
static void release_completed_native_retirement(::std::unique_lock<::std::mutex>& lock,
    ::std::shared_ptr<native_retirement_state>& actual) noexcept
{
    if(actual->result.status == llvm_jit_checkpoint_native_retirement_status::retired_and_joined)
    {
        completed_native_retirement_ = actual->token;
        completed_native_retirement_result_ = actual->result;
        completed_native_retirement_result_.operation.reset();
        native_retirement_.reset(); // local owner keeps EVERY resource alive
    }
    lock.unlock();
    actual.reset(); // provider/source/cohort destruction outside the registry lock
}
public:
// Legacy reset/stop owns this SAME mutex for its entire cold transition.
// A pending managed operation cannot be bypassed by execution-only drain.
[[nodiscard]] static ::std::unique_lock<::std::mutex> try_legacy_transition_excluding_native_retirement() noexcept
{
    ::std::unique_lock lock{native_retirement_mutex_, ::std::try_to_lock};
    if(!lock.owns_lock() || native_retirement_) { return {}; }
    return lock;
}
[[nodiscard]] static llvm_jit_checkpoint_native_retirement_result begin_native_retirement(
    domain::pause_ticket const& ticket, ::std::span<capture::owner const> supplied,
    ::std::chrono::steady_clock::time_point deadline,
    llvm_jit_checkpoint_prepare_request const* preparing=nullptr) noexcept
{
    using outcome = llvm_jit_checkpoint_native_retirement_status;
    llvm_jit_checkpoint_native_retirement_result out{};
    if(!native_retirement_context_allowed()) { out.status = outcome::invalid_context; return out; }
    if(deadline <= ::std::chrono::steady_clock::now()) { out.status = outcome::invalid_deadline; return out; }
    if(preparing)
    {
        bool label{};for(auto b:preparing->recording_label) { label=label || b!=::std::byte{}; }
        if(!label || preparing->maximum_native_payload_bytes==0u || preparing->maximum_original_source_bytes==0u)
        { out.status=outcome::preparation_declined;out.preparation.status=llvm_jit_checkpoint_prepare_status::invalid_request;return out; }
    }
    ::std::unique_lock lock{native_retirement_mutex_, ::std::try_to_lock};
    if(!lock.owns_lock()) { out.status = outcome::busy; return out; }
    if(native_retirement_) { out = native_retirement_->result; out.status = outcome::busy; return out; }
    ::std::shared_ptr<native_retirement_state> actual{};
    try
    {
        actual = ::std::make_shared<native_retirement_state>(); actual->deadline = deadline;
        actual->joined = runtime_checkpoint_guest_worker_bridge::prepare_join_storage();
        if(!actual->joined)
        { out.status = outcome::allocation_failed; lock.unlock(); actual.reset(); return out; }
        retirement_handoff handoff{}; handoff.native = actual.get();
        complete_instance_request census{};complete_instance_result copied{};
        if(preparing)
        {
            actual->prepare_candidate=true;actual->preparation_request=*preparing;
            census.recording_id=preparing->recording_label;census.budget=preparing->graph_budget;census.include_wasip1=preparing->include_wasip1;
        }
        auto checked{inspect_current_impl(ticket,supplied,nullptr,nullptr,nullptr,nullptr,::std::addressof(handoff),
            preparing?::std::addressof(census):nullptr,preparing?::std::addressof(copied):nullptr,
            nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,false,nullptr,
            preparing?::std::addressof(actual->preparation_request):nullptr,
            preparing?::std::addressof(actual->result.preparation):nullptr)};
        if(preparing) { actual->result.preparation.data_error=copied.data_error; }
        actual->result.cohort_diagnostic = static_cast<unsigned>(checked.observation);
        if(!actual->activated)
        {
            out=actual->result;
            out.status = checked.observation == status::allocation_failed ? outcome::allocation_failed :
                preparing && actual->result.preparation.status!=llvm_jit_checkpoint_prepare_status::not_selected ? outcome::preparation_declined : outcome::rejected_current_cohort;
            out.candidate_world_retained=false;
            if(out.preparation.status==llvm_jit_checkpoint_prepare_status::prepared_and_retained)
            { out.preparation.status=llvm_jit_checkpoint_prepare_status::prepared_and_discarded; }
            out.cohort_diagnostic = actual->result.cohort_diagnostic;
            lock.unlock(); actual.reset(); return out;
        }
        if(actual->result.status == outcome::requires_llvm_jit_full)
        { actual->result.status = outcome::failed_closed; }
        actual->result.operation = actual->token;
        native_retirement_ = actual; out = actual->result;
        release_completed_native_retirement(lock, actual); return out;
    }
    catch(...)
    {
        if(actual && actual->activated)
        {
            actual->result.status = outcome::failed_closed; actual->result.operation = actual->token;
            native_retirement_ = actual; out = actual->result;
        }
        else { out.status = outcome::allocation_failed; }
        lock.unlock(); actual.reset(); return out;
    }
}
[[nodiscard]] static llvm_jit_checkpoint_native_retirement_result continue_native_retirement(
    llvm_jit_checkpoint_native_retirement_owner const& supplied,
    ::std::chrono::steady_clock::time_point deadline) noexcept
{
    using outcome = llvm_jit_checkpoint_native_retirement_status;
    llvm_jit_checkpoint_native_retirement_result out{};
    if(!native_retirement_context_allowed()) { out.status = outcome::invalid_context; return out; }
    if(deadline <= ::std::chrono::steady_clock::now()) { out.status = outcome::invalid_deadline; return out; }
    ::std::unique_lock lock{native_retirement_mutex_, ::std::try_to_lock};
    if(!lock.owns_lock()) { out.status = outcome::busy; return out; }
    if(!native_retirement_)
    {
        auto completed{completed_native_retirement_.lock()};
        if(!same_owner(completed, supplied)) { out.status = outcome::stale_owner; return out; }
        out = completed_native_retirement_result_; out.operation = completed; return out;
    }
    // Compare the native manager's OWN strong pointer+control block before any
    // supplied pointee read. Aliases, numbers and copied receipts are rejected.
    if(!same_owner(native_retirement_->token, supplied)) { out.status = outcome::stale_owner; return out; }
    auto actual{native_retirement_}; actual->deadline = deadline;
    try
    {
        auto maintenance{g_runtime.execution_domain.try_maintenance_transition()};
        if(!maintenance) { out = actual->result; out.status = outcome::busy; return out; }
        finish_native_retirement_until(*actual, maintenance); out = actual->result;
    }
    catch(...) { actual->result.status = outcome::failed_closed; out = actual->result; }
    release_completed_native_retirement(lock, actual); return out;
}
// Explicit rollback of a ready private candidate. Reacquire actual maintenance,
// drain and physical-join proof; copied statuses never reopen admission.
[[nodiscard]] static llvm_jit_checkpoint_native_retirement_result discard_prepared_instance(
    llvm_jit_checkpoint_native_retirement_owner const& supplied,
    ::std::chrono::steady_clock::time_point deadline) noexcept
{
    using outcome=llvm_jit_checkpoint_native_retirement_status;
    llvm_jit_checkpoint_native_retirement_result out{};
    if(!native_retirement_context_allowed()) { out.status=outcome::invalid_context;return out; }
    if(deadline<=::std::chrono::steady_clock::now()) { out.status=outcome::invalid_deadline;return out; }
    ::std::unique_lock lock{native_retirement_mutex_,::std::try_to_lock};
    if(!lock.owns_lock()) { out.status=outcome::busy;return out; }
    if(!native_retirement_ || !same_owner(native_retirement_->token,supplied) || !native_retirement_->prepare_candidate)
    { out.status=outcome::stale_owner;return out; }
    auto actual{native_retirement_};actual->deadline=deadline;
    try
    {
        auto maintenance{g_runtime.execution_domain.try_maintenance_transition()};
        if(!maintenance) { out=actual->result;out.status=outcome::busy;return out; }
        finish_native_retirement_until(*actual,maintenance);
        if(actual->result.status!=outcome::prepared_world_ready_closed) { return actual->result; }
        // Keep candidate alive until release_completed unlocks this registry.
        // Suppress only its closed-ready branch; actual drain/join/host/source
        // proof is independently reacquired by the ordinary finite reset path.
        actual->prepare_candidate=false;
        finish_native_retirement_until(*actual,maintenance);
        if(actual->result.status==outcome::retired_and_joined)
        { actual->result.candidate_world_retained=false;actual->result.preparation.status=llvm_jit_checkpoint_prepare_status::prepared_and_discarded; }
        out=actual->result;
    }
    catch(...) { actual->result.status=outcome::failed_closed;out=actual->result; }
    release_completed_native_retirement(lock,actual);return out;
}
private:
