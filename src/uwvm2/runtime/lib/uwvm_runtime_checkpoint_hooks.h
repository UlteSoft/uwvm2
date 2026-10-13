// Native-only checkpoint bridge implementation. Included in the runtime TU
// after actual publication/activation declarations. No serialized label,
// source metadata, native PC, caller boolean or public ledger mints authority.
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
namespace
{
    [[nodiscard]] UWVM_ALWAYS_INLINE inline ::uwvm2::runtime::checkpoint::sealed_function_plan::owner const*
    checkpoint_borrow_actual_frame_plan(::std::uint64_t incarnation,
        ::uwvm2::runtime::checkpoint::activation_identity& identity) noexcept
    {
        auto const activations{g_debug_activation_ledger};
        auto const shadow{g_checkpoint_shadow_ledger};
        if(shadow == nullptr || activations == nullptr || !activations->valid() ||
           activations->count() == 0u || activations->count() != shadow->size() ||
           g_debug_pause_participant == nullptr || incarnation == 0u) { return nullptr; }
        auto const& actual{activations->data()[activations->count() - 1u]};
        if(actual.incarnation != incarnation || actual.runtime_epoch != current_runtime_generation()) { return nullptr; }
        identity = {actual.incarnation, actual.parent, actual.continuation, actual.runtime_epoch};
        auto const plan{shadow->compiler_top_plan(identity)};
        if(plan == nullptr || !*plan) { return nullptr; }
        auto const& metadata{(*plan)->get()};
        if(metadata.module != actual.module || metadata.function != actual.function ||
           metadata.function_generation != actual.function_generation) { return nullptr; }
        // checkpoint_enter_actual_activation did the canonical source registry,
        // publication, profile-control-block and generation checks. That hook's
        // retained immutable plan is bound to the SAME live TLS activation here.
        // The outer execution lease pins its publication; replacement requires
        // all workers parked and rejects every active target frame. No callback
        // or park occurs between this borrow and the complete packet copy.
        return plan;
    }
    // Warm entry DATA only. Repeat the same actual ledger/module/publication,
    // epoch, generation, native entry and shared-control-block comparisons as
    // the original entry cache hit. The execution lease pins these immutable
    // tables; a miss goes through the unchanged full canonical admission.
    // No callback or park may span this synchronous borrow and shadow admission.
    [[nodiscard]] UWVM_ALWAYS_INLINE inline ::uwvm2::runtime::checkpoint::sealed_function_plan::owner const*
    checkpoint_borrow_warm_entry_plan(::std::uint64_t incarnation,
        ::uwvm2::runtime::checkpoint::activation_identity& identity) noexcept
    {
        auto const cache{g_checkpoint_entry_plans};
        auto const activations{g_debug_activation_ledger};
        if(cache == nullptr || g_checkpoint_shadow_ledger == nullptr || activations == nullptr || !activations->valid() ||
           activations->count() == 0u || g_debug_pause_participant == nullptr || incarnation == 0u) { return nullptr; }
        auto const& actual{activations->data()[activations->count()-1u]};
        if(actual.incarnation != incarnation || actual.module >= g_runtime.modules.size() ||
           actual.runtime_epoch != current_runtime_generation()) { return nullptr; }
        auto const& record{g_runtime.modules.index_unchecked(actual.module)};
        auto const publication{record.llvm_jit_full_publication.get()};
        auto const module{record.runtime_module};
        if(publication == nullptr || module == nullptr || !record.llvm_jit_ready) { return nullptr; }
        auto const imports{module->imported_function_vec_storage.size()};
        if(actual.function < imports || actual.function-imports >= record.llvm_jit_compiled.local_funcs.size()) { return nullptr; }
        auto const local{static_cast<::std::size_t>(actual.function-imports)};
        if(local >= record.llvm_jit_debug_full_entry_generations.size() ||
           record.llvm_jit_debug_full_entry_generations[local] != actual.function_generation ||
           local >= record.llvm_jit_local_raw_entry_addresses.size() || local >= record.llvm_jit_local_entry_addresses.size())
        { return nullptr; }
        details::checkpoint_entry_plan_cache::key const binding{::std::addressof(record),publication,
            actual.runtime_epoch,actual.module,actual.function,actual.function_generation,
            record.llvm_jit_local_raw_entry_addresses.index_unchecked(local),
            record.llvm_jit_local_entry_addresses.index_unchecked(local)};
        auto const retained{cache->find(binding,record.llvm_jit_compiled.local_funcs.index_unchecked(local).checkpoint_plan)};
        if(retained == nullptr) { return nullptr; }
        identity={actual.incarnation,actual.parent,actual.continuation,actual.runtime_epoch};
        return retained;
    }
    [[nodiscard]] bool checkpoint_actual_frame(::std::uint64_t incarnation,
        ::uwvm2::runtime::checkpoint::activation_identity& identity,
        ::uwvm2::runtime::checkpoint::sealed_function_plan::owner& plan, bool entry_lookup = false,
        ::uwvm2::runtime::checkpoint::sealed_function_plan::owner const** borrowed_entry = nullptr) noexcept
    {
        auto const activations{g_debug_activation_ledger};
        if(g_checkpoint_shadow_ledger == nullptr || activations == nullptr || !activations->valid() ||
           activations->count() == 0u || g_debug_pause_participant == nullptr || incarnation == 0u)
        { return false; }
        // [actual TLS-owned fixed activation array ... count] end
        // [safe                                             ] count>0 before
        // last-slot indexing; this is native identity, not a saved guest stack.
        auto const& actual{activations->data()[activations->count() - 1u]};
        if(actual.incarnation != incarnation || actual.module >= g_runtime.modules.size() ||
           actual.runtime_epoch != current_runtime_generation()) { return false; }
        auto const& record{g_runtime.modules.index_unchecked(actual.module)};
        auto const publication{record.llvm_jit_full_publication.get()};
        auto const module{record.runtime_module};
        if(publication == nullptr || module == nullptr || !record.llvm_jit_ready) { return false; }
        // The generation-plan lookup below authenticates the initializer-sealed
        // source binding through the current root or committed replacement.
        // Repeating a display-name hash or preload scan here adds no ownership
        // proof. No guest callback or publication intervenes in this lookup.
        auto const imports{module->imported_function_vec_storage.size()};
        if(actual.function < imports || actual.function - imports >= record.llvm_jit_compiled.local_funcs.size()) { return false; }
        auto const local{static_cast<::std::size_t>(actual.function - imports)};
        auto const& live_plan{record.llvm_jit_compiled.local_funcs.index_unchecked(local).checkpoint_plan};
        details::checkpoint_entry_plan_cache::key binding{};
        auto const cache{entry_lookup ? g_checkpoint_entry_plans : nullptr};
        if(cache != nullptr && local < record.llvm_jit_debug_full_entry_generations.size() &&
           record.llvm_jit_debug_full_entry_generations[local] == actual.function_generation &&
           local < record.llvm_jit_local_raw_entry_addresses.size() && local < record.llvm_jit_local_entry_addresses.size())
        {
            binding = {::std::addressof(record),publication,actual.runtime_epoch,actual.module,actual.function,
                actual.function_generation,record.llvm_jit_local_raw_entry_addresses.index_unchecked(local),
                record.llvm_jit_local_entry_addresses.index_unchecked(local)};
            if(auto const retained{cache->find(binding,live_plan)}; retained != nullptr)
            {
                // Same immutable plan/control block, publication, native entries
                // and epoch under this worker's actual execution lease. An
                // all-park replacement changes generation/plan/entries; reset
                // destroys this scope. Cold capture always rechecks the source.
                identity = {actual.incarnation,actual.parent,actual.continuation,actual.runtime_epoch};
                if(borrowed_entry != nullptr) { *borrowed_entry=retained; }
                else { plan = *retained; }
                return true;
            }
        }
        // A cache row was inserted only after these checks and the canonical
        // source/generation lookup below. The actual execution lease pins this
        // immutable publication and its configuration profile until drain. A
        // warm entry already matched that publication, BOTH native entries,
        // epoch/generation and the live plan's exact shared control block above.
        // Configuration cannot replace the profile once code is published;
        // all-park function replacement changes the entry/plan/generation key.
        // Keep the full proof for misses, cold observations and restore capture.
        if(!publication->engine || !publication->context || publication->plan || !publication->checkpoint_profile ||
           publication->checkpoint_profile != g_runtime.checkpoint_profile ||
           publication->checkpoint_profile.owner_before(g_runtime.checkpoint_profile) ||
           g_runtime.checkpoint_profile.owner_before(publication->checkpoint_profile)) { return false; }
        auto metadata{checkpoint_current_generation_plan(record,local,actual.function_generation)};
        if(!metadata || metadata->get().profile != publication->checkpoint_profile ||
           metadata->get().profile.owner_before(publication->checkpoint_profile) ||
           publication->checkpoint_profile.owner_before(metadata->get().profile) ||
           metadata->get().module != actual.module || metadata->get().function != actual.function ||
           metadata->get().function_generation != actual.function_generation) { return false; }
        identity = {actual.incarnation, actual.parent, actual.continuation, actual.runtime_epoch};
        if(cache != nullptr) { cache->remember_validated(binding,metadata); }
        plan = ::std::move(metadata); return true;
    }
    UWVM_NOINLINE void checkpoint_enter_actual_activation_uncached(::std::uint64_t incarnation,
        ::uwvm2::runtime::checkpoint::activation_identity* admitted = nullptr) noexcept
    {
        auto const shadow{g_checkpoint_shadow_ledger}; if(shadow == nullptr) { return; }
        ::uwvm2::runtime::checkpoint::activation_identity identity{};
        ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
        ::uwvm2::runtime::checkpoint::sealed_function_plan::owner const* borrowed{};
        if(!checkpoint_actual_frame(incarnation, identity, plan, true,::std::addressof(borrowed)))
        { shadow->poison(::uwvm2::runtime::checkpoint::status::invalid_activation); return; }
        auto const& actual_plan{borrowed != nullptr ? *borrowed : plan};
        if(actual_plan->get().producer_availability != ::uwvm2::runtime::checkpoint::status::ok)
        { shadow->poison(actual_plan->get().producer_availability); return; }
        auto const admit{[&]
        {
            if(actual_plan->get().profile->purpose() == ::uwvm2::runtime::checkpoint::compilation_purpose::observe_values)
            { static_cast<void>(shadow->compiler_enter_observer(identity,actual_plan)); }
            else if(borrowed != nullptr) { static_cast<void>(shadow->enter(identity,*borrowed)); }
            else { static_cast<void>(shadow->enter(identity,::std::move(plan))); }
        }};
#ifdef UWVM_CPP_EXCEPTIONS
        try { admit(); if(admitted != nullptr && shadow->failure() == ::uwvm2::runtime::checkpoint::status::ok) { *admitted=identity; } }
        catch(...) { shadow->poison(::uwvm2::runtime::checkpoint::status::quota_exceeded); }
#else
        admit();
        if(admitted != nullptr && shadow->failure() == ::uwvm2::runtime::checkpoint::status::ok) { *admitted=identity; }
#endif
    }
    UWVM_ALWAYS_INLINE void checkpoint_enter_actual_activation(::std::uint64_t incarnation,
        ::uwvm2::runtime::checkpoint::activation_identity* admitted = nullptr) noexcept
    {
        auto const shadow{g_checkpoint_shadow_ledger}; if(shadow == nullptr) { return; }
        ::uwvm2::runtime::checkpoint::activation_identity identity{};
        auto const borrowed{checkpoint_borrow_warm_entry_plan(incarnation,identity)};
        if(borrowed == nullptr) { checkpoint_enter_actual_activation_uncached(incarnation,admitted); return; }
        // Exact original warm-hit admission, without constructing a cold owned
        // plan or crossing the full canonical lookup's native ABI/prologue.
        // This cache contains immutable metadata DATA, never values or roots.
        auto const& plan{*borrowed};
        if(plan->get().producer_availability != ::uwvm2::runtime::checkpoint::status::ok)
        { shadow->poison(plan->get().producer_availability); return; }
        // Keep this synchronous admission inside its caller: outlining this
        // lambda prevented LLVM from joining the already-authenticated entry
        // and workspace bind. Every original ledger check and catch remains.
        auto const admit{[&] UWVM_ALWAYS_INLINE
        {
            if(plan->get().profile->purpose() == ::uwvm2::runtime::checkpoint::compilation_purpose::observe_values)
            { static_cast<void>(shadow->compiler_enter_observer(identity,plan)); }
            else { static_cast<void>(shadow->enter(identity,plan)); }
        }};
#ifdef UWVM_CPP_EXCEPTIONS
        try { admit(); if(admitted != nullptr && shadow->failure() == ::uwvm2::runtime::checkpoint::status::ok) { *admitted=identity; } }
        catch(...) { shadow->poison(::uwvm2::runtime::checkpoint::status::quota_exceeded); }
#else
        admit();
        if(admitted != nullptr && shadow->failure() == ::uwvm2::runtime::checkpoint::status::ok) { *admitted=identity; }
#endif
    }
    // Called only by the genuine fused generated entry. Authenticate its live
    // publication once, then use that SAME record for the metadata cache join.
    // The native ledger still mints a fresh identity; the shadow ledger still
    // validates the actual awaiting parent, quota, tail and shared plan owner.
    // Allocation may fail, but no guest callback/park/publication occurs here.
    [[nodiscard]] UWVM_ALWAYS_INLINE ::std::uint64_t checkpoint_enter_registered_observer_activation(
        ::std::uintptr_t module_id, ::std::uintptr_t function_index, ::std::uint64_t compiled_generation,
        ::uwvm2::runtime::checkpoint::activation_identity& admitted) noexcept
    {
        namespace cp=::uwvm2::runtime::checkpoint;
        auto const ledger{g_debug_activation_ledger};
        auto const shadow{g_checkpoint_shadow_ledger};
        auto const invalid{[&]() -> ::std::uint64_t
        {
            if(ledger != nullptr) { ledger->poison(); }
            checkpoint_enter_actual_activation_uncached(0u,::std::addressof(admitted));
            return 0u;
        }};
        if(ledger == nullptr) { checkpoint_enter_actual_activation_uncached(0u,::std::addressof(admitted)); return 0u; }
        if(g_debug_pause_participant == nullptr || module_id >= g_runtime.modules.size()) { return invalid(); }
        auto const& record{g_runtime.modules.index_unchecked(module_id)};
        auto const module{record.runtime_module};
        auto const publication{record.llvm_jit_full_publication.get()};
        if(module == nullptr || !record.llvm_jit_ready || publication == nullptr ||
           !publication->engine || !publication->context || publication->plan) { return invalid(); }
        auto const imports{module->imported_function_vec_storage.size()};
        if(function_index < imports || function_index-imports >= record.llvm_jit_debug_full_entry_generations.size() ||
           compiled_generation == 0u || record.llvm_jit_debug_full_entry_generations[function_index-imports] != compiled_generation)
        { return invalid(); }
        auto const epoch{current_runtime_generation()};
        auto const incarnation{ledger->enter(module_id,function_index,compiled_generation,epoch)};
        if(shadow == nullptr) { return incarnation; }
        auto const cache{g_checkpoint_entry_plans};
        auto const local{static_cast<::std::size_t>(function_index-imports)};
        if(incarnation == 0u || !ledger->valid() || ledger->count() == 0u || cache == nullptr ||
           local >= record.llvm_jit_compiled.local_funcs.size() || local >= record.llvm_jit_local_raw_entry_addresses.size() ||
           local >= record.llvm_jit_local_entry_addresses.size())
        { checkpoint_enter_actual_activation_uncached(incarnation,::std::addressof(admitted)); return incarnation; }
        details::checkpoint_entry_plan_cache::key const binding{::std::addressof(record),publication,
            epoch,module_id,function_index,compiled_generation,record.llvm_jit_local_raw_entry_addresses.index_unchecked(local),
            record.llvm_jit_local_entry_addresses.index_unchecked(local)};
        auto const borrowed{cache->find(binding,record.llvm_jit_compiled.local_funcs.index_unchecked(local).checkpoint_plan)};
        if(borrowed == nullptr)
        { checkpoint_enter_actual_activation_uncached(incarnation,::std::addressof(admitted)); return incarnation; }
        // enter() wrote this top cell itself; no external tuple or cached
        // identity is used. Original warm admission and every failure remain.
        auto const& actual{ledger->data()[ledger->count()-1u]};
        cp::activation_identity const identity{actual.incarnation,actual.parent,actual.continuation,actual.runtime_epoch};
        auto const& plan{*borrowed};
        if(plan->get().producer_availability != cp::status::ok)
        { shadow->poison(plan->get().producer_availability); return incarnation; }
        auto const admit{[&] UWVM_ALWAYS_INLINE
        {
            if(plan->get().profile->purpose() == cp::compilation_purpose::observe_values)
            { static_cast<void>(shadow->compiler_enter_observer(identity,plan)); }
            else { static_cast<void>(shadow->enter(identity,plan)); }
        }};
#ifdef UWVM_CPP_EXCEPTIONS
        try { admit(); if(shadow->failure() == cp::status::ok) { admitted=identity; } }
        catch(...) { shadow->poison(cp::status::quota_exceeded); }
#else
        admit(); if(shadow->failure() == cp::status::ok) { admitted=identity; }
#endif
        return incarnation;
    }
    UWVM_ALWAYS_INLINE void checkpoint_leave_actual_activation(::std::uint64_t incarnation, ::std::uintptr_t exit) noexcept
    {
        auto const shadow{g_checkpoint_shadow_ledger}; if(shadow == nullptr) { return; }
        ::uwvm2::runtime::checkpoint::activation_identity identity{};
        // Entry retained the canonical plan under the actual publication lease.
        // Borrow that SAME live leaf, including current epoch/count/generation,
        // before retiring it; no callback or publication can intervene here.
        auto const plan{checkpoint_borrow_actual_frame_plan(incarnation, identity)};
        if(exit > 3u || plan == nullptr)
        { shadow->poison(::uwvm2::runtime::checkpoint::status::invalid_activation); return; }
        static_cast<void>(shadow->leave(identity, exit == 1u)); // before genuine musttail / exception cleanup
    }
}
# endif
extern "C++" void details::llvm_jit_checkpoint_materialize_abi_bridge(
    ::std::uint64_t incarnation, ::std::uint64_t site_id, ::std::uintptr_t native_slots, ::std::size_t bytes) noexcept
{
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(get_llvm_jit_generated_bridge_scope_depth() == 0u || g_debug_observer_active) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    auto const shadow{g_checkpoint_shadow_ledger}; if(shadow == nullptr) { return; }
    if(shadow->failure() != ::uwvm2::runtime::checkpoint::status::ok) { return; }
    ::uwvm2::runtime::checkpoint::activation_identity identity{};
    auto const actual_plan{checkpoint_borrow_actual_frame_plan(incarnation, identity)};
    if(actual_plan == nullptr || site_id == 0u || site_id > (*actual_plan)->get().sites.size())
    { shadow->poison(::uwvm2::runtime::checkpoint::status::invalid_activation); return; }
    auto const& plan{*actual_plan};
    auto const& site{plan->get().sites[static_cast<::std::size_t>(site_id - 1u)]};
    if(site.slots.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
           ::uwvm2::runtime::checkpoint::native_slot_bytes ||
       bytes != site.slots.size() * ::uwvm2::runtime::checkpoint::native_slot_bytes ||
       (bytes != 0u && native_slots == 0u) || native_slots > UINTPTR_MAX - bytes)
    { shadow->poison(::uwvm2::runtime::checkpoint::status::invalid_layout); return; }
    // [actual generated frame allocation native_slots ... +bytes] end
    // [safe actual native producer provenance + exact immutable site extent]
    // Canonical activation/publication/plan and arithmetic were checked BEFORE
    // forming this borrowed range. Integer bounds alone would grant no pointer
    // permission; neither pointer nor range escapes this synchronous bridge.
    auto const data{reinterpret_cast<::std::byte const*>(native_slots)};
    ::std::span<::std::byte const> const payload{data, bytes};
#ifdef UWVM_CPP_EXCEPTIONS
    try { static_cast<void>(shadow->materialize(identity, site_id, payload, true)); }
    catch(...) { shadow->poison(::uwvm2::runtime::checkpoint::status::quota_exceeded); }
#else
    static_cast<void>(shadow->materialize(identity, site_id, payload, true));
#endif
# else
    (void)incarnation; (void)site_id; (void)native_slots; (void)bytes;
# endif
}

# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
extern "C++" llvm_jit_checkpoint_recording_observation llvm_jit_observe_checkpoint_recording_host_api() noexcept
{
    llvm_jit_checkpoint_recording_observation out{};
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    auto const shadow{g_checkpoint_shadow_ledger};
    auto const activations{g_debug_activation_ledger};
    if(!g_debug_observer_active || !g_debug_activation_park_site.before_park ||
       get_llvm_jit_generated_bridge_scope_depth() == 0u ||
       g_debug_pause_participant == nullptr || shadow == nullptr || activations == nullptr ||
       !activations->valid() || activations->count() == 0u) { return out; }
    // [actual native activation array ... nonzero count] end
    // [safe                                           ] nonzero count was
    // checked before last-slot indexing. Scalar observation grants no roots.
    auto const& actual{activations->data()[activations->count() - 1u]};
    ::uwvm2::runtime::checkpoint::activation_identity identity{};
    ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
    if(!checkpoint_actual_frame(actual.incarnation, identity, plan) ||
       g_debug_activation_park_site.location.code_unit != actual.module ||
       g_debug_activation_park_site.location.function != actual.function ||
       g_debug_activation_park_site.location.code_generation != actual.runtime_epoch) { return out; }
    out.instrumented = true; out.status = plan->get().producer_availability == ::uwvm2::runtime::checkpoint::status::ok ?
        shadow->failure() : plan->get().producer_availability;
    out.incarnation = actual.incarnation; out.runtime_epoch = actual.runtime_epoch;
    out.function_generation = actual.function_generation; out.native_frames = shadow->size();
    auto const frames{shadow->frames_while_actually_stopped()};
    // Cold numeric expansion may fail allocation and poison recording. Report
    // the CURRENT failure rather than the status sampled before that expansion.
    if(shadow->failure() != ::uwvm2::runtime::checkpoint::status::ok)
    { out.status = shadow->failure(); return out; }
    if(!frames.empty() && frames.back().identity == identity && frames.back().materialized)
    {
        out.site = frames.back().site; out.typed_slots = frames.back().values.size();
        if(out.site != 0u && out.site <= plan->get().sites.size())
        { out.at_current_opcode = plan->get().sites[static_cast<::std::size_t>(out.site - 1u)].opcode_offset ==
            g_debug_activation_park_site.location.offset; }
    }
    // Entry snapshots alone have no all-opcode executable restore capability.
    out.executable_restore_available = !g_debug_activation_park_site.suspended_wait &&
        shadow->executable_restore_capability() == ::uwvm2::runtime::checkpoint::status::ok;
# endif
    return out;
}
# endif

// Selected at compile time only by the immutable checkpoint profile. Ordinary
// observational debug bridges keep their original code/TLS cost unchanged.
extern "C++" ::std::uint64_t details::llvm_jit_checkpoint_activation_enter_abi_bridge(
    ::std::uintptr_t module_id, ::std::uintptr_t function_index, ::std::uint64_t compiled_generation) noexcept
{
    auto const incarnation{details::llvm_jit_debug_activation_enter_abi_bridge(module_id, function_index, compiled_generation)};
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    checkpoint_enter_actual_activation(incarnation);
# endif
    return incarnation;
}
extern "C++" UWVM_ALWAYS_INLINE void details::llvm_jit_checkpoint_activation_leave_abi_bridge(
    ::std::uint64_t incarnation, ::std::uintptr_t exit) noexcept
{
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(get_llvm_jit_generated_bridge_scope_depth() == 0u || g_debug_observer_active) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    checkpoint_leave_actual_activation(incarnation, exit);
# endif
    details::llvm_jit_debug_activation_leave_abi_bridge(incarnation, exit);
}
