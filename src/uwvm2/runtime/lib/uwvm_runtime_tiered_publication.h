/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#include <atomic>
#include <cstdint>

namespace uwvm2::runtime::lib::details
{
    // Serialize ONLY entry publication, never generated execution or guest memory
    // access. A movable module record owns the scalar; records cannot move while
    // execution/compilation is admitted. T1 must recheck T2 readiness under this
    // scope: checking before locking permits a delayed T1 writer to replace T2.
    class tiered_entry_publication_scope
    {
        ::std::uint_least8_t& state;

    public:
        template<typename Yield>
        explicit tiered_entry_publication_scope(::std::uint_least8_t& live_state, Yield&& yield) noexcept : state{live_state}
        {
            // [live module record] owns state until this non-escaping scope ends.
            ::std::atomic_ref<::std::uint_least8_t> flag{state};
            while(flag.exchange(1u, ::std::memory_order_acquire) != 0u)
            {
                while(flag.load(::std::memory_order_relaxed) != 0u) { yield(); }
            }
        }
        tiered_entry_publication_scope(tiered_entry_publication_scope const&) = delete;
        tiered_entry_publication_scope& operator=(tiered_entry_publication_scope const&) = delete;
        ~tiered_entry_publication_scope()
        {
            ::std::atomic_ref<::std::uint_least8_t>{state}.store(0u, ::std::memory_order_release);
        }
    };
}
