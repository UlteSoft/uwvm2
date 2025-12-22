
/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-06-30
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstdint>
# include <cstddef>
# include <cstring>
# include <climits>
# include <concepts>
# include <memory>
# include <utility>
# include <type_traits>
# include <bit>
# include <algorithm>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include "wrapper.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::mutex
{
    template <typename T>
    concept basic_lockable = requires(T& t) {
        t.lock();
        t.unlock();
    };

    namespace detail
    {
        template <typename M>
        inline constexpr void lock_via_void_ptr(void* p) noexcept
        { static_cast<M*>(p)->lock(); }
    }  // namespace detail

    template <typename... Mutexes>
        requires (sizeof...(Mutexes) > 0) && ((basic_lockable<Mutexes>) && ...)
    inline constexpr void lock_all(Mutexes & ... mutexes) noexcept
    {
        if constexpr(sizeof...(Mutexes) == 1)
        {
            (mutexes.lock(), ...);
            return;
        }

        struct lock_entry
        {
            ::std::uintptr_t address{};
            void* pointer{};
            void (*lock_fn)(void*) noexcept {};
        };

        lock_entry entries[sizeof...(Mutexes)]{
            lock_entry{::std::bit_cast<::std::uintptr_t>(::std::addressof(mutexes)),
                       const_cast<void*>(static_cast<void const*>(::std::addressof(mutexes))),
                       ::std::addressof(detail::lock_via_void_ptr<::std::remove_reference_t<Mutexes>>)}
            ...
        };

        ::std::sort(entries, entries + (sizeof...(Mutexes)), [](lock_entry const& a, lock_entry const& b) constexpr noexcept { return a.address < b.address; });

        ::std::uintptr_t previous_address{};
        bool has_previous{};
        for(lock_entry const& e: entries)
        {
            if(!has_previous || e.address != previous_address)
            {
                e.lock_fn(e.pointer);
                previous_address = e.address;
                has_previous = true;
            }
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
