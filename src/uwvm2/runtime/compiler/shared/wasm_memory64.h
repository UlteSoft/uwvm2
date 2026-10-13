/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <concepts>
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <limits>
# include <memory>
# include <uwvm2/utils/macro/push_macros.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::runtime::compiler::shared::wasm_memory64
{
    // Bulk lengths are not scalar access widths. Keep all 64 bits until BOTH
    // ranges have passed their native allocation proofs; adding two Wasm
    // addresses first would incorrectly accept a wrapping range.
    template<::std::unsigned_integral Native>
    [[nodiscard]] inline constexpr bool range_valid(Native bound, ::std::uint_least64_t offset,
                                                     ::std::uint_least64_t length) noexcept
    {
        auto const wide_bound{static_cast<::std::uint_least64_t>(bound)};
        return offset <= wide_bound && length <= wide_bound - offset;
    }

    enum class bulk_error : unsigned { none, source, destination };

    // Caller owns/pins these snapshots for the complete operation. A stable mmap
    // base requires no operation guard. This primitive never acquires a lock.
    // Core 3 memory.copy checks both whole ranges even when length is zero.
    [[nodiscard]] UWVM_ALWAYS_INLINE inline bulk_error copy(
        ::std::byte* destination_begin, ::std::size_t destination_size,
        ::std::byte const* source_begin, ::std::size_t source_size,
        ::std::uint_least64_t destination, ::std::uint_least64_t source,
        ::std::uint_least64_t length) noexcept
    {
        if(!range_valid(source_size, source, length)) [[unlikely]] { return bulk_error::source; }
        if(!range_valid(destination_size, destination, length)) [[unlikely]] { return bulk_error::destination; }
        if(length != 0u)
        {
            // [source allocation: ... source ... source+length] | end
            // [safe                                          ]
            //                         ^^ src; full-width proof above permits narrowing.
            auto const src{source_begin + static_cast<::std::size_t>(source)};
            // [destination allocation: ... destination ... destination+length] | end
            // [safe                                                         ]
            //                              ^^ dst, formed only for a nonempty valid range.
            auto const dst{destination_begin + static_cast<::std::size_t>(destination)};
            // All checks precede mutation. memmove also handles the same memory
            // reached through different imports, in either overlap direction.
            ::std::memmove(dst, src, static_cast<::std::size_t>(length));
        }
        return bulk_error::none;
    }

    [[nodiscard]] UWVM_ALWAYS_INLINE inline bulk_error fill(
        ::std::byte* destination_begin, ::std::size_t destination_size,
        ::std::uint_least64_t destination, ::std::uint_least32_t value,
        ::std::uint_least64_t length) noexcept
    {
        if(!range_valid(destination_size, destination, length)) [[unlikely]] { return bulk_error::destination; }
        if(length != 0u)
        {
            // [destination allocation: ... destination ... destination+length] | end
            // [safe                                                         ]
            //                              ^^ dst; nonempty range was checked before narrowing.
            auto const dst{destination_begin + static_cast<::std::size_t>(destination)};
            ::std::memset(dst, static_cast<unsigned char>(value), static_cast<::std::size_t>(length));
        }
        return bulk_error::none;
    }

    // Saturate a declared page maximum to the host's byte-resource cap. Compare
    // at full Wasm width before narrowing: e.g. 2^32+1 pages must not become one
    // page on ISA32. This does not decide whether a declared type is valid.
    template<::std::unsigned_integral Native = ::std::size_t>
    [[nodiscard]] inline constexpr Native maximum_bytes_from_pages(::std::uint_least64_t pages, unsigned shift) noexcept
    {
        constexpr auto maximum{::std::numeric_limits<Native>::max()};
        if(shift >= ::std::numeric_limits<Native>::digits || pages > static_cast<::std::uint_least64_t>(maximum >> shift))
        { return maximum; }
        return static_cast<Native>(static_cast<Native>(pages) << shift);
    }

    // Core 3 execution/memory.grow: interpret delta as an unsigned i64 bit
    // pattern. A native size_t is only formed after the complete width/limit
    // proof, including on ISA32. No address or byte-count wrap is permitted.
    template<typename Memory>
    [[nodiscard]] inline constexpr ::std::uint_least64_t grow(
        Memory& memory, ::std::size_t maximum_bytes, ::std::uint_least64_t delta, bool strict) noexcept
    {
        constexpr auto failed{::std::numeric_limits<::std::uint_least64_t>::max()};
        auto const shift{memory.custom_page_size_log2};
        if(shift >= ::std::numeric_limits<::std::size_t>::digits) [[unlikely]] { return failed; }
        auto const limit_pages{maximum_bytes >> shift};
        auto old_pages{static_cast<::std::size_t>(memory.get_page_size())};
        if(old_pages > limit_pages || delta > static_cast<::std::uint_least64_t>(limit_pages - old_pages)) [[unlikely]]
        { return failed; }
        // delta <= limit_pages <= SIZE_MAX >> shift: conversion and the backend
        // multiplication cannot truncate. Grow-only memory makes this rejection
        // precheck safe; concurrent backends recheck under their growth mutex.
        auto const native_delta{static_cast<::std::size_t>(delta)};
        if(strict)
        {
            return memory.grow_strictly(native_delta, maximum_bytes, ::std::addressof(old_pages)) ?
                static_cast<::std::uint_least64_t>(old_pages) : failed;
        }
        if constexpr(Memory::support_multi_thread)
        {
            // Capture the actual previous length inside the same critical section
            // that publishes the growth. A stale precheck must never become the
            // result returned to a Wasm thread.
            return memory.try_grow_silently(native_delta, maximum_bytes, ::std::addressof(old_pages)) ?
                static_cast<::std::uint_least64_t>(old_pages) : failed;
        }
        else
        {
            // Preserve the selected fail-fast host allocation policy; Wasm/native
            // representability failures have already returned the all-ones result.
            memory.grow_silently(native_delta, maximum_bytes);
            return static_cast<::std::uint_least64_t>(old_pages);
        }
    }
}
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
