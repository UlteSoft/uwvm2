// Native-only current-opcode producer; proposed include in the real runtime
// TU AFTER checkpoint_actual_frame and actual bridge/activation definitions.
// No public tuple, native address, integer extent or ledger grants permission.
extern "C++" ::std::uintptr_t details::llvm_jit_checkpoint_observer_workspace_abi_bridge(
    ::std::size_t compiler_bytes) noexcept
{
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || g_debug_observer_active || g_debug_operand_workspaces == nullptr) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    // Storage ownership is independent of recording availability. A poisoned
    // diagnostic ledger grants no preview, but must not invalidate real SSA
    // addresses or reject a guest whose native function is still executing.
    auto const allocate{[&]() -> ::std::uintptr_t
    {
        auto const workspace{g_debug_operand_workspaces->enter(compiler_bytes)};
        return workspace.empty() ? 0u : reinterpret_cast<::std::uintptr_t>(workspace.data());
    }};
    ::std::uintptr_t address{};
# ifdef UWVM_CPP_EXCEPTIONS
    try { address = allocate(); } catch(...) {}
# else
    address = allocate();
# endif
    if(address != 0u) { return address; }
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8runtime_log_output, u8"uwvm: debug value workspace allocation failed");
    ::fast_io::fast_terminate();
# else
    (void)compiler_bytes; ::fast_io::fast_terminate();
# endif
}
extern "C++" void details::llvm_jit_checkpoint_observer_workspace_leave_abi_bridge(::std::uintptr_t compiler_buffer) noexcept
{
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || g_debug_observer_active || g_debug_operand_workspaces == nullptr ||
       !g_debug_operand_workspaces->leave(compiler_buffer)) [[unlikely]] { ::fast_io::fast_terminate(); }
# else
    (void)compiler_buffer; ::fast_io::fast_terminate();
# endif
}

extern "C++" void details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge(
    ::std::uint64_t incarnation, ::std::uint64_t site_id, ::std::uintptr_t native_slots,
    ::std::size_t bytes, ::std::uintptr_t native_flags, ::std::size_t flag_count) noexcept
{
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || g_debug_observer_active) [[unlikely]]
    { ::fast_io::fast_terminate(); }
    auto const shadow{g_checkpoint_shadow_ledger}; if(shadow == nullptr) { return; }
    if(shadow->failure() != ::uwvm2::runtime::checkpoint::status::ok) { return; }
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    checkpoint::activation_identity identity{}; checkpoint::sealed_function_plan::owner plan{};
    if(!checkpoint_actual_frame(incarnation, identity, plan) || site_id == 0u || site_id > plan->get().sites.size())
    { shadow->poison(checkpoint::status::invalid_activation); return; }
    // [canonical actual plan sites ... site_id-1 ... N] sites_end
    // [safe] full ID bound precedes selection, with actual entry/activation,
    // full publication, generation and same-control-block profile checks.
    auto const& site{plan->get().sites[static_cast<::std::size_t>(site_id - 1u)]};
    if(site.slots.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
           checkpoint::native_slot_bytes || bytes != site.slots.size() * checkpoint::native_slot_bytes ||
       flag_count != site.local_count || flag_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) ||
       (bytes != 0u && native_slots == 0u) || (flag_count != 0u && native_flags == 0u) ||
       native_slots > UINTPTR_MAX - bytes || native_flags > UINTPTR_MAX - flag_count)
    { shadow->poison(checkpoint::status::invalid_layout); return; }
    // [genuine compiler-owned native packet ... +bytes] packet_end
    // [safe] current generated activation and exact typed plan prove actual
    // producer provenance, then extent/null/overflow checks precede pointer
    // construction. Arithmetic alone never grants arbitrary native memory access.
    auto const payload{reinterpret_cast<::std::byte const*>(native_slots)};
    // [genuine compiler-owned original-index flags ... +flag_count] flags_end
    // [safe] same current native producer, count==actual local_count, nullable
    // empty range and checked uintptr addition BEFORE forming this second span.
    auto const flags{reinterpret_cast<::std::uint8_t const*>(native_flags)};
    auto const commit{[&]
    {
        checkpoint::dynamic_native_packet::owner owned{};
        auto const copied{checkpoint::dynamic_native_packet::copy_compiler_packet(plan, site_id,
            ::std::span<::std::byte const>{payload, bytes}, ::std::span<::std::uint8_t const>{flags, flag_count}, owned)};
        if(copied != checkpoint::status::ok || !owned)
        { shadow->poison(copied == checkpoint::status::ok ? checkpoint::status::invalid_layout : copied); return; }
        // The actual packet factory checks ALL flags before ANY value byte read;
        // stronger executed initialization does not amend validator legality.
        // Copy detached typed DATA; no borrowed native address survives this call.
        static_cast<void>(shadow->materialize_validated_native_data(identity, owned->plan(), owned->site(), owned->values()));
    }};
# ifdef UWVM_CPP_EXCEPTIONS
    try { commit(); }
    catch(...) { shadow->poison(checkpoint::status::quota_exceeded); }
# else
    commit();
# endif
# else
    (void)incarnation; (void)site_id; (void)native_slots; (void)bytes; (void)native_flags; (void)flag_count;
# endif
}
