/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)
 * Private source-only candidate; not a product allocator.
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <memory>
# include <new>
# include <type_traits>
# include <utility>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::runtime::storage
{
    enum class compact_numeric_status : ::std::uint_least32_t
    {
        ok, invalid_store, invalid_type, invalid_reference, foreign_reference,
        out_of_bounds, size_overflow, out_of_memory, unsupported_layout,
        unsupported_exceptions, not_published, retiring, busy, invariant_error
    };
    enum class compact_numeric_kind : unsigned char { invalid, i32, f32 };

    // Token-free native storage. A buffer is NOT a Wasm object, store authority,
    // allocation lease, root or registry entry. Its native owner serializes all
    // writes/replacement/release against readers. Descriptor cells are written
    // once before release-frontier publication and never mutated while live.
    // Plain immutable cold/hot reads can coexist; no atomic_ref/plain-access mix.
    class gc_object_store;
    class compact_numeric_reader;
    class compact_numeric_descriptor;
    class compact_numeric_payload
    {
        friend class gc_object_store;
        friend class compact_numeric_reader;
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        friend class sealed_compact_entry;
#endif
        friend class compact_numeric_descriptor;
        struct cell
        {
            ::std::uint32_t bits{};
        };
        static_assert(::std::is_standard_layout_v<cell>);
        static_assert(::std::is_trivially_destructible_v<cell>);
        ::std::unique_ptr<cell[]> cells_{};
        ::std::size_t count_{};
        [[nodiscard]] ::std::uint32_t const* address_of_cell(::std::size_t index) const noexcept
        {
            if(index >= count_ || !cells_) { return nullptr; }
            // [cells_,cells_+count_) index<count_; address a real uint32 member.
            // The owning reader/store must pin this allocation through the last
            // immediate byte load. NEVER interpret it as a gc_object_value[1].
            return ::std::addressof(cells_[index].bits);
        }
    public:
        static constexpr bool has_raw_four_byte_layout{
            sizeof(cell) == 4uz};
        static constexpr ::std::size_t maximum_count{
            static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(cell)};
        compact_numeric_payload() noexcept = default;
        compact_numeric_payload(compact_numeric_payload const&) = delete;
        compact_numeric_payload& operator=(compact_numeric_payload const&) = delete;
        compact_numeric_payload(compact_numeric_payload&& other) noexcept
            // [owned cell array] moving the owner does not move any cell.
            : cells_{::std::move(other.cells_)}, count_{::std::exchange(other.count_, 0uz)} {}
        compact_numeric_payload& operator=(compact_numeric_payload&&) = delete;
        [[nodiscard]] ::std::size_t size() const noexcept { return count_; }

        // Strong failure guarantee: an unsuccessful fresh allocation preserves
        // this buffer's old cells, count and raw bits; no identity is involved.
        [[nodiscard]] compact_numeric_status replace(::std::size_t count) noexcept
        {
            if(!has_raw_four_byte_layout) { return compact_numeric_status::unsupported_layout; }
            if(count == 0uz || count > maximum_count) { return compact_numeric_status::size_overflow; }
            // [0,count) real cell objects, each initialized by the array-new.
            // No byte[] cast or fabricated C++ array lifetime is used.
            ::std::unique_ptr<cell[]> replacement{new(::std::nothrow) cell[count]{}};
            if(!replacement) { return compact_numeric_status::out_of_memory; }
            // [old owned array] retires only after the new allocation succeeded.
            cells_.swap(replacement);
            count_ = count;
            return compact_numeric_status::ok;
        }
        void release() noexcept
        {
            // [owned initialized array] only its original array-new owner frees it.
            cells_.reset();
            count_ = 0uz;
        }
        [[nodiscard]] compact_numeric_status load(::std::size_t index, ::std::uint32_t& bits) const noexcept
        {
            if(index >= count_ || !cells_) { return compact_numeric_status::out_of_bounds; }
            // [cells_,cells_+count_) index<count_; names a real immutable cell.
            // Native owner excludes writes/release; descriptor frontier acquire
            // observes the one ordinary write before its release publication.
            bits = cells_[index].bits;
            return compact_numeric_status::ok;
        }
        [[nodiscard]] compact_numeric_status store(::std::size_t index, ::std::uint32_t bits) noexcept
        {
            if(index >= count_ || !cells_) { return compact_numeric_status::out_of_bounds; }
            // [cells_,cells_+count_) index<count_; preserve NaN/sign/raw integer bits.
            // Native owner excludes all readers here. Descriptor append writes
            // this fresh slot only, before any token/live frontier is published.
            cells_[index].bits = bits;
            return compact_numeric_status::ok;
        }
    };

    // Geometry only. This cannot mint a range or confer authority over a store.
    // The actual issuer is private in gc_object_store and remains unchanged.
    struct compact_numeric_range_geometry
    {
        static constexpr ::std::uintptr_t first_object_token{0x10000u};
        static constexpr ::std::size_t reserved_width{1024uz};
        [[nodiscard]] static constexpr bool valid(::std::uintptr_t first, ::std::size_t capacity) noexcept
        {
            return capacity != 0uz && capacity <= reserved_width &&
                first >= first_object_token &&
                first <= (::std::numeric_limits<::std::uintptr_t>::max)() - reserved_width;
        }
        [[nodiscard]] static constexpr bool slot(::std::uintptr_t first, ::std::size_t capacity,
            ::std::uintptr_t token, ::std::size_t& result) noexcept
        {
            if(!valid(first, capacity) || token < first) { return false; }
            auto const difference{token - first};
            if(difference >= capacity) { return false; }
            result = static_cast<::std::size_t>(difference);
            return true;
        }
    };
}
