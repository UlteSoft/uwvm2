/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-24
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
# include <version>
# include <cstdint>
# include <cstddef>
# include <concepts>
# include <new>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::cmdline
{
    /// @brief      Shortest Path Algorithm for Strings
    /// @details    Compute the shortest path between two strings with a time complexity of O(n²) and a space complexity of O(n).
    ///             In the template, if Stack_Len is 0, heap allocation is used; if Stack_Len is greater than 0, a stack of length Stack_Len is used.
    ///             This optimization benefits command-line parsing when command lengths can be determined at compile time.
    /// @param      x_str       fist string ptr
    /// @param      x_length    fist string length
    /// @param      y_str       second string ptr
    /// @param      y_length    second string length

    template <::std::size_t Stack_Len = 0, ::std::integral char_type>
    UWVM_GNU_CONST inline constexpr ::std::size_t shortest_path(char_type const* x_str,
                                                                ::std::size_t x_length,
                                                                char_type const* y_str,
                                                                ::std::size_t y_length) noexcept
    {
        using Alloc = ::fast_io::native_typed_thread_local_allocator<::std::size_t>;

        ::std::size_t* d{};
        [[maybe_unused]] ::uwvm2::utils::container::array<::std::size_t, Stack_Len> storage;

        if constexpr(Stack_Len) { d = storage.data(); }
        else
        {
            if(y_length > ::std::numeric_limits<::std::size_t>::max() - 1uz) [[unlikely]] { ::fast_io::fast_terminate(); }

            auto const alloc_length{y_length + 1uz};

            if UWVM_IF_CONSTEVAL { d = ::new ::std::size_t[alloc_length]; }
            else
            {
                auto const alloc_ptr{Alloc::allocate(alloc_length)};

#if __cpp_lib_start_lifetime_as >= 202207L
                // No UBs, type_allocator provides built-in type alignment.
                d = ::std::start_lifetime_as_array<::std::size_t>(alloc_ptr, alloc_length);
#else
                // Historical leniency for trivial types
                // Under older conventions, many compiler implementations adopted a “lenient mode” for trivial types:
                // Simply reinterpret_cast<T*>(raw) and write to it, and lifetimes would be automatically managed for you.
                d = alloc_ptr;
#endif
            }
        }

        for(::std::size_t j{}; j <= y_length; ++j) { d[j] = j; }

        for(::std::size_t i{1}; i <= x_length; ++i)
        {
            ::std::size_t old{i - 1uz};
            d[0uz] = i;
            for(::std::size_t j{1uz}; j <= y_length; ++j)
            {
                ::std::size_t const temp{d[j]};
                if(x_str[i - 1] == y_str[j - 1uz]) { d[j] = old; }
                else
                {
                    ::std::size_t min{d[j] + 1uz};
                    if(d[j - 1uz] + 1uz < min) { min = d[j - 1uz] + 1uz; }
                    if(old + 1uz < min) { min = old + 1uz; }
                    d[j] = min;
                }
                old = temp;
            }
        }

        ::std::size_t const ret{d[y_length]};

        if constexpr(!Stack_Len)
        {
            if UWVM_IF_CONSTEVAL { ::delete[] d; }
            else
            {
                Alloc::deallocate_n(d, y_length + 1uz);
            }
        }

        return ret;
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
