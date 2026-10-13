#pragma once
// Private native context protocol; no guest/Wasm ABI or authority constructor.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
#include <atomic>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <type_traits>
namespace uwvm2::uwvm::runtime::storage { class sealed_compact_entry; }
namespace uwvm2::runtime::gc
{
    class managed_collection_state;
    enum class sealed_cursor_status : ::std::uintptr_t
    { ready, original_route, out_of_memory, size_overflow, interrupted, invariant_error, original_route_already_charged };
    // NOT a guest carrier. All pointers name actual native members/array bases.
    // The actual outer-entry scope owns this object until generated code exits.
    struct sealed_compact_cursor_view
    {
        ::std::atomic<::std::uintptr_t> armed_nonce{}, interrupts{};
        ::std::atomic_bool const* pause_requested{};
        ::std::atomic_bool const* policy_disabled{};
        ::std::uint64_t const* epoch_address{};
        void* cells{};
        ::std::atomic_size_t* frontier{};
        ::std::atomic<::std::uint64_t>* live{};
        ::std::uintptr_t token_begin{};
        ::std::size_t capacity{}, budget{}, first_credit{}, unaccounted{};
        ::std::uint64_t epoch{};
        ::std::uint32_t type_index{}, raw_kind{};
#if defined(UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE) && UWVM_EXPERIMENTAL_SEALED_LOCAL_TABLE == 1
        // Native actual-entry owned table capture; not a guest handle. These
        // fields are cleared before any possible reallocation/collection and
        // initialized after original poll, before armed_nonce release.
        void* table_elements{};
        ::std::size_t table_extent{};
        ::std::uint32_t table_ready{}, table_address64{};
#endif
    };
    static_assert(::std::is_standard_layout_v<sealed_compact_cursor_view>);
    static_assert(::std::atomic_size_t::is_always_lock_free);
    static_assert(::std::atomic<::std::uint64_t>::is_always_lock_free);
    static_assert(::std::atomic<::std::uintptr_t>::is_always_lock_free);
    static_assert(sizeof(::std::atomic_bool) == 1uz);
    // Cold callback first resolves the native runtime's OWN current scope and
    // compares ctx with its view address. It NEVER dereferences supplied ctx.
    using sealed_capture_callback = ::std::uintptr_t (*)(::std::uintptr_t) noexcept;
    using sealed_refill_callback = ::std::uintptr_t (*)(::std::uintptr_t, ::std::uintptr_t, ::std::uint32_t) noexcept;
    using sealed_boundary_callback = void (*)(::std::uintptr_t) noexcept;
    // Native-only cold result; this is NOT an LLVM/guest aggregate ABI. charged
    // distinguishes decline before the original allocation_poll from decline
    // after that original function pre-recorded the current allocation attempt.
    struct sealed_attempt_account { ::std::size_t budget{}; bool charged{}, eligible{}; };
    // prepare_next=false only settles committed hot attempts; no new poll attempt.
    using sealed_account_callback = sealed_attempt_account (*)(::std::uintptr_t, ::std::uintptr_t, ::std::size_t, bool) noexcept;
    inline ::std::atomic<sealed_capture_callback> sealed_capture{};
    inline ::std::atomic<sealed_refill_callback> sealed_refill{};
    inline ::std::atomic<sealed_boundary_callback> sealed_boundary{};
    inline ::std::atomic<sealed_account_callback> sealed_account{};
    using sealed_flush_callback = void (*)(bool) noexcept;
    using sealed_borrow_callback = ::uwvm2::uwvm::runtime::storage::sealed_compact_entry* (*)(::std::uintptr_t) noexcept;
    inline ::std::atomic<sealed_flush_callback> sealed_flush{};
    inline ::std::atomic<sealed_borrow_callback> sealed_borrow{};
    inline void flush_actual_sealed_entry(bool revoke) noexcept
    { if(auto f{sealed_flush.load(::std::memory_order_acquire)}) { f(revoke); } }
    [[nodiscard]] inline auto borrow_actual_sealed_entry(::std::uintptr_t module) noexcept
        -> ::uwvm2::uwvm::runtime::storage::sealed_compact_entry*
    { auto f{sealed_borrow.load(::std::memory_order_acquire)}; return f ? f(module) : nullptr; }
    [[nodiscard]] inline ::std::uintptr_t sealed_compact_capture_leaf(::std::uintptr_t module) noexcept
    { auto f{sealed_capture.load(::std::memory_order_acquire)}; return f ? f(module) : 0u; }
    [[nodiscard]] inline ::std::uintptr_t sealed_compact_refill_leaf(
        ::std::uintptr_t ctx, ::std::uintptr_t module, ::std::uint32_t index) noexcept
    {
        auto f{sealed_refill.load(::std::memory_order_acquire)};
        return f ? f(ctx, module, index) : static_cast<::std::uintptr_t>(sealed_cursor_status::original_route);
    }
    inline void sealed_compact_boundary_leaf(::std::uintptr_t ctx) noexcept
    { if(auto f{sealed_boundary.load(::std::memory_order_acquire)}) { f(ctx); } }
    static_assert(::std::is_same_v<decltype(&sealed_compact_capture_leaf), sealed_capture_callback>);
    static_assert(::std::is_same_v<decltype(&sealed_compact_refill_leaf), sealed_refill_callback>);
    static_assert(::std::is_same_v<decltype(&sealed_compact_boundary_leaf), sealed_boundary_callback>);
}
#endif
