// Native-only checkpoint bridge implementation. Included in the runtime TU
// after actual publication/activation declarations. No serialized label,
// source metadata, native PC, caller boolean or public ledger mints authority.
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
namespace
{
    [[nodiscard]] bool checkpoint_actual_frame(::std::uint64_t incarnation,
        ::uwvm2::runtime::checkpoint::activation_identity& identity,
        ::uwvm2::runtime::checkpoint::sealed_function_plan::owner& plan) noexcept
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
        if(publication == nullptr || module == nullptr || !record.llvm_jit_ready ||
           !publication->engine || !publication->context || publication->plan || !publication->checkpoint_profile ||
           publication->checkpoint_profile != g_runtime.checkpoint_profile ||
           publication->checkpoint_profile.owner_before(g_runtime.checkpoint_profile) ||
           g_runtime.checkpoint_profile.owner_before(publication->checkpoint_profile)) { return false; }
        auto const& source{publication->source};
        if(!::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source) ||
           !source->initialized_from_actual_state()) { return false; }
        auto const member{source->registry().find(record.module_name)};
        if(member == source->registry().end() || ::std::addressof(member->second) != module) { return false; }
        auto const imports{module->imported_function_vec_storage.size()};
        if(actual.function < imports || actual.function - imports >= record.llvm_jit_compiled.local_funcs.size()) { return false; }
        auto const metadata{checkpoint_current_generation_plan(record,
            static_cast<::std::size_t>(actual.function - imports), actual.function_generation)};
        if(!metadata || metadata->get().profile != publication->checkpoint_profile ||
           metadata->get().profile.owner_before(publication->checkpoint_profile) ||
           publication->checkpoint_profile.owner_before(metadata->get().profile) ||
           metadata->get().module != actual.module || metadata->get().function != actual.function ||
           metadata->get().function_generation != actual.function_generation) { return false; }
        identity = {actual.incarnation, actual.parent, actual.continuation, actual.runtime_epoch};
        plan = metadata; return true;
    }
    void checkpoint_enter_actual_activation(::std::uint64_t incarnation) noexcept
    {
        auto const shadow{g_checkpoint_shadow_ledger}; if(shadow == nullptr) { return; }
        ::uwvm2::runtime::checkpoint::activation_identity identity{};
        ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
        if(!checkpoint_actual_frame(incarnation, identity, plan))
        { shadow->poison(::uwvm2::runtime::checkpoint::status::invalid_activation); return; }
        if(plan->get().producer_availability != ::uwvm2::runtime::checkpoint::status::ok)
        { shadow->poison(plan->get().producer_availability); return; }
#ifdef UWVM_CPP_EXCEPTIONS
        try { static_cast<void>(shadow->enter(identity, ::std::move(plan))); }
        catch(...) { shadow->poison(::uwvm2::runtime::checkpoint::status::quota_exceeded); }
#else
        static_cast<void>(shadow->enter(identity, ::std::move(plan)));
#endif
    }
    void checkpoint_leave_actual_activation(::std::uint64_t incarnation, ::std::uintptr_t exit) noexcept
    {
        auto const shadow{g_checkpoint_shadow_ledger}; if(shadow == nullptr) { return; }
        ::uwvm2::runtime::checkpoint::activation_identity identity{};
        ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
        if(exit > 3u || !checkpoint_actual_frame(incarnation, identity, plan))
        { shadow->poison(::uwvm2::runtime::checkpoint::status::invalid_activation); return; }
        static_cast<void>(shadow->leave(identity, exit == 1u)); // before genuine musttail / exception cleanup
    }
}
# endif
extern "C++" void details::llvm_jit_checkpoint_materialize_abi_bridge(
    ::std::uint64_t incarnation, ::std::uint64_t site_id, ::std::uintptr_t native_slots, ::std::size_t bytes) noexcept
{
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || g_debug_observer_active) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    auto const shadow{g_checkpoint_shadow_ledger}; if(shadow == nullptr) { return; }
    if(shadow->failure() != ::uwvm2::runtime::checkpoint::status::ok) { return; }
    ::uwvm2::runtime::checkpoint::activation_identity identity{};
    ::uwvm2::runtime::checkpoint::sealed_function_plan::owner plan{};
    if(!checkpoint_actual_frame(incarnation, identity, plan) || site_id == 0u || site_id > plan->get().sites.size())
    { shadow->poison(::uwvm2::runtime::checkpoint::status::invalid_activation); return; }
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
    try { static_cast<void>(shadow->materialize(identity, site_id, payload)); }
    catch(...) { shadow->poison(::uwvm2::runtime::checkpoint::status::quota_exceeded); }
#else
    static_cast<void>(shadow->materialize(identity, site_id, payload));
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
       get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u ||
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
extern "C++" void details::llvm_jit_checkpoint_activation_leave_abi_bridge(
    ::std::uint64_t incarnation, ::std::uintptr_t exit) noexcept
{
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || g_debug_observer_active) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    checkpoint_leave_actual_activation(incarnation, exit);
# endif
    details::llvm_jit_debug_activation_leave_abi_bridge(incarnation, exit);
}
