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
        namespace checkpoint = ::uwvm2::runtime::checkpoint;
        auto const workspace{g_debug_operand_workspaces->enter(compiler_bytes,checkpoint::observer_site_bytes)};
        auto const shadow{g_checkpoint_shadow_ledger};
        if(!workspace.empty() && shadow != nullptr && shadow->failure() == checkpoint::status::ok)
        {
            auto const ledger{g_debug_activation_ledger};
            auto const incarnation{ledger != nullptr && ledger->valid() && ledger->count() != 0u ?
                ledger->data()[ledger->count()-1u].incarnation : 0u};
            checkpoint::activation_identity identity{};
            if(checkpoint_borrow_actual_frame_plan(incarnation,identity) == nullptr)
            { shadow->poison(checkpoint::status::invalid_activation); }
            else { static_cast<void>(shadow->bind_observer_storage(identity,g_debug_operand_workspaces->current_storage())); }
        }
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
// One synchronous native entry reuses its authentic admission result only
// until the actual LIFO workspace bind. Cold inspection/GC/exit retain their
// independent joins; ordinary/resumable/other-target entry ABIs are unchanged.
namespace
{
template<bool SmallCapacity>
UWVM_ALWAYS_INLINE details::llvm_jit_checkpoint_observer_entry_result checkpoint_observer_activation_enter_impl(
    ::std::size_t compiler_bytes, ::std::uintptr_t module_id, ::std::uintptr_t function_index,
    ::std::uint64_t compiled_generation) noexcept
{
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || g_debug_observer_active || g_debug_operand_workspaces == nullptr)
    [[unlikely]] { ::fast_io::fast_terminate(); }
    // One real transition authenticates the entry and reuses the SAME pinned
    // record for warm plan admission. No client-supplied tuple grants a bind;
    // cache misses retain the full canonical generation/source lookup.
    ::uwvm2::runtime::checkpoint::activation_identity admitted{};
    auto const incarnation{checkpoint_enter_registered_observer_activation(
        module_id,function_index,compiled_generation,admitted)};
    auto const allocate{[&]() -> ::std::uintptr_t
    {
        namespace cp=::uwvm2::runtime::checkpoint;
        static_assert(cp::observer_site_bytes == sizeof(::std::uint64_t));
        auto const workspace{[&] UWVM_ALWAYS_INLINE
        {
            if constexpr(SmallCapacity) { return g_debug_operand_workspaces->enter_small(compiler_bytes); }
            else { return g_debug_operand_workspaces->enter(compiler_bytes,cp::observer_site_bytes); }
        }()};
        auto const shadow{g_checkpoint_shadow_ledger};
        if(!workspace.empty() && shadow != nullptr && shadow->failure() == cp::status::ok)
        {
            // Neither allocation nor this bind invokes a guest callback, parks
            // the worker or changes the publication pinned by its execution
            // lease. bind_observer_storage independently matches the live top
            // identity, purpose, unbound LIFO allocation, extent and zero site.
            // Re-running the activation/plan join here cannot add a new proof.
            auto const ledger{g_debug_activation_ledger};
            if(admitted.incarnation == 0u || ledger == nullptr || !ledger->valid() || ledger->count() != shadow->size())
            { shadow->poison(cp::status::invalid_activation); }
            else { static_cast<void>(shadow->bind_observer_storage(admitted,g_debug_operand_workspaces->current_storage())); }
        }
        return workspace.empty() ? 0u : reinterpret_cast<::std::uintptr_t>(workspace.data());
    }};
    ::std::uintptr_t workspace{};
# ifdef UWVM_CPP_EXCEPTIONS
    try { workspace=allocate(); } catch(...) {}
# else
    workspace=allocate();
# endif
    if(workspace != 0u) { return {incarnation,workspace}; }
    ::fast_io::io::perrln(::uwvm2::uwvm::io::u8runtime_log_output,u8"uwvm: debug value workspace allocation failed");
    ::fast_io::fast_terminate();
# else
    (void)compiler_bytes;(void)module_id;(void)function_index;(void)compiled_generation;::fast_io::fast_terminate();
# endif
}
}
extern "C++" details::llvm_jit_checkpoint_observer_entry_result details::llvm_jit_checkpoint_observer_activation_enter_abi_bridge(
    ::std::size_t bytes, ::std::uintptr_t module, ::std::uintptr_t function, ::std::uint64_t generation) noexcept
{ return checkpoint_observer_activation_enter_impl<false>(bytes,module,function,generation); }
extern "C++" details::llvm_jit_checkpoint_observer_entry_result details::llvm_jit_checkpoint_observer_small_enter_abi_bridge(
    ::std::size_t bytes, ::std::uintptr_t module, ::std::uintptr_t function, ::std::uint64_t generation) noexcept
{ return checkpoint_observer_activation_enter_impl<true>(bytes,module,function,generation); }
extern "C++" UWVM_ALWAYS_INLINE void details::llvm_jit_checkpoint_observer_workspace_leave_abi_bridge(::std::uintptr_t compiler_buffer) noexcept
{
# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(get_llvm_jit_generated_wasm_bridge_entry_depth() == 0u || g_debug_observer_active || g_debug_operand_workspaces == nullptr)
    [[unlikely]] { ::fast_io::fast_terminate(); }
    auto const storage{g_debug_operand_workspaces->current_storage()};
    if(storage.size() < ::uwvm2::runtime::checkpoint::observer_site_bytes ||
       compiler_buffer != reinterpret_cast<::std::uintptr_t>(storage.data()+::uwvm2::runtime::checkpoint::observer_site_bytes))
    [[unlikely]] { ::fast_io::fast_terminate(); }
    if(auto const shadow{g_checkpoint_shadow_ledger}; shadow != nullptr)
    { shadow->unbind_observer_storage(storage.data()); }
    if(!g_debug_operand_workspaces->leave(compiler_buffer)) [[unlikely]] { ::fast_io::fast_terminate(); }
# else
    (void)compiler_buffer; ::fast_io::fast_terminate();
# endif
}

// One generated ABI call retires the actual buffer and then the actual frame.
// Preserve the previous ordering for reference cleanup, EH and typed tails.
extern "C++" UWVM_ALWAYS_INLINE void details::llvm_jit_checkpoint_observer_activation_leave_abi_bridge(
    ::std::uint64_t incarnation, ::std::uintptr_t exit, ::std::uintptr_t compiler_buffer) noexcept
{
    details::llvm_jit_checkpoint_observer_workspace_leave_abi_bridge(compiler_buffer);
    details::llvm_jit_checkpoint_activation_leave_abi_bridge(incarnation,exit);
}
// The compiler proved this real exit is an ordinary return. Expose the same
// complete transition with a constant kind to native optimization; all actual
// ownership, publication, LIFO and active native-session checks remain.
extern "C++" void details::llvm_jit_checkpoint_observer_return_abi_bridge(
    ::std::uint64_t incarnation, ::std::uintptr_t compiler_buffer) noexcept
{ details::llvm_jit_checkpoint_observer_activation_leave_abi_bridge(incarnation,0u,compiler_buffer); }


# if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
namespace
{
    // Both callers synchronously borrowed this SAME native leaf's plan. Nothing
    // calls, parks or executes guest code between that authenticated borrow and
    // this copy. Reuse its exact identity instead of repeating the entire join.
    void checkpoint_materialize_actual_packet(
        ::uwvm2::runtime::checkpoint::activation_identity identity,
        ::uwvm2::runtime::checkpoint::sealed_function_plan::owner const& plan,
        ::std::uint64_t site_id, ::std::uintptr_t native_slots, ::std::size_t bytes,
        ::std::uintptr_t native_flags, ::std::size_t flag_count) noexcept
    {
        namespace checkpoint = ::uwvm2::runtime::checkpoint;
        auto const shadow{g_checkpoint_shadow_ledger};
        if(shadow == nullptr || shadow->failure() != checkpoint::status::ok) { return; }
        if(site_id == 0u || site_id > plan->get().sites.size())
        { shadow->poison(checkpoint::status::invalid_activation); return; }
        // [canonical actual plan sites ... site_id-1 ... N] sites_end
        // [safe] full ID bound precedes selection, with actual entry/activation,
        // entry-authenticated publication and live activation/generation checks.
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
        if(shadow->compiler_observer_packet(identity,site_id,{payload,bytes},{flags,flag_count}))
        { return; } // Exact live typed slots; expand only for a cold reader.
        auto const commit{[&]
        {
            // Validate and copy into activation-owned storage at EVERY opcode.
            // Bound observer packets keep actual typed slots until a cold
            // inspection or GC census. Unbound references and resumable
            // packets retain synchronous full decoding.
            static_cast<void>(shadow->materialize_compiler_native_data(identity, plan, site_id,
                ::std::span<::std::byte const>{payload, bytes}, ::std::span<::std::uint8_t const>{flags, flag_count}, true));
        }};
    # ifdef UWVM_CPP_EXCEPTIONS
        try { commit(); }
        catch(...) { shadow->poison(checkpoint::status::quota_exceeded); }
    # else
        commit();
    # endif
    }
}
# endif

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
    checkpoint::activation_identity identity{};
    auto const borrowed_plan{checkpoint_borrow_actual_frame_plan(incarnation, identity)};
    if(borrowed_plan == nullptr)
    { shadow->poison(checkpoint::status::invalid_activation); return; }
    auto const& plan{*borrowed_plan};
    checkpoint_materialize_actual_packet(identity,plan,site_id,native_slots,bytes,native_flags,flag_count);
# else
    (void)incarnation; (void)site_id; (void)native_slots; (void)bytes; (void)native_flags; (void)flag_count;
# endif
}
