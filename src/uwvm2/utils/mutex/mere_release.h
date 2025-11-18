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
    /// @brief    A lock guard that merely releases the lock.
    /// @details  Locks are highly customizable via pointers, meaning you can lock at any time, but they must be unlocked at the end of their lifecycle.
    template <typename mtx_type>
    struct basic_mutex_merely_release_guard
    {
        using mutex_type = ::std::remove_cvref_t<mtx_type>;
        mutex_type* device_p{};  // Ensure device_p is initialized to nullptr. Ensure that no issues arise during destructors.

        inline constexpr basic_mutex_merely_release_guard() noexcept = default;

        UWVM_ALWAYS_INLINE inline explicit constexpr basic_mutex_merely_release_guard(mutex_type* d) : device_p(d) {}

        inline basic_mutex_merely_release_guard(basic_mutex_merely_release_guard const&) = delete;
        inline basic_mutex_merely_release_guard& operator= (basic_mutex_merely_release_guard const&) = delete;

        UWVM_ALWAYS_INLINE inline constexpr ~basic_mutex_merely_release_guard()
        {
            if(device_p) [[likely]] { device_p->unlock(); }
        }

        UWVM_ALWAYS_INLINE inline constexpr void lock() noexcept
        {
            if(device_p) [[likely]] { device_p->lock(); }
        }

        UWVM_ALWAYS_INLINE inline constexpr void unlock() noexcept
        {
            if(device_p) [[likely]] { device_p->unlock(); }
        }
    };

    using mutex_merely_release_guard_t = basic_mutex_merely_release_guard<::uwvm2::utils::mutex::mutex_t&>;
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
