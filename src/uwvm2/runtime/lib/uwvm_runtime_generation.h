/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace uwvm2::runtime::lib::details
{
    /// Process-local epoch for objects whose addresses may be reused after a
    /// quiescent runtime reset. Zero is reserved for an unpublished cache entry.
    class runtime_generation_epoch
    {
    public:
        using value_type = ::std::uint_least64_t;

        [[nodiscard]] value_type current() const noexcept { return value.load(::std::memory_order_acquire); }

        /// Advance the epoch without ever wrapping back to a previously valid
        /// value. False is practically unreachable and requires the caller to
        /// terminate rather than permit an ABA cache hit.
        [[nodiscard]] bool advance() noexcept
        {
            auto observed{value.load(::std::memory_order_relaxed)};
            for(;;)
            {
                if(observed == (::std::numeric_limits<value_type>::max)()) [[unlikely]] { return false; }
                if(value.compare_exchange_weak(observed,
                                               observed + 1u,
                                               ::std::memory_order_acq_rel,
                                               ::std::memory_order_relaxed))
                {
                    return true;
                }
            }
        }

    private:
        ::std::atomic_uint_least64_t value{1u};
    };

    [[nodiscard]] inline constexpr bool call_indirect_cache_generation_matches(runtime_generation_epoch::value_type cached,
                                                                                runtime_generation_epoch::value_type current) noexcept
    {
        return cached != 0u && cached == current;
    }
}  // namespace uwvm2::runtime::lib::details
